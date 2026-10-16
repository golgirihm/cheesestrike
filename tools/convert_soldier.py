"""Turns the Tactical Soldier 02 download into the model the game loads.

The download (see game/licensed/LICENSES.md) is a Character Creator character:
an FBX with only its colour and normal textures wired up, a zip of 4K texture
sets, and parts the game has no use for. This keeps what the game shows, at a
size it can carry, and writes it as one glTF file:

- The tongue, the tear lines and the shadows round the eyes go. Each needs a
  shader of its own and none shows at the distance players see each other.
- Of the face's 148 shapes, only those the game's faces use are kept (FACE).
- Every material gets its colour, normal, roughness and metallic maps from the
  zip, scaled down to TEXTURE_SIZE (SMALL_TEXTURE_SIZE for the small parts).
- The skeleton and the skinning stay as they are, for the animations to drive.

Run it with Blender, which writes game/licensed/soldier/soldier.glb:

    blender --background --factory-startup --python tools/convert_soldier.py -- <the .fbx> <Maps.zip>
"""

import pathlib
import sys
import tempfile
import zipfile

import bpy

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "game" / "licensed" / "soldier" / "soldier.glb"

DROP = ["CC_Base_Tongue", "CC_Base_TearLine", "CC_Base_EyeOcclusion"]

# The face shapes the game's faces are made of: blinking, wincing (screwed-up
# eyes, dropped brows, gritted teeth), cheering (a wide open grin), firing
# (set brows, pressed lips) and glancing.
FACE = {
    "Eye_Blink_L", "Eye_Blink_R", "Eye_Squint_L", "Eye_Squint_R", "Eye_Wide_L", "Eye_Wide_R",
    "Brow_Drop_L", "Brow_Drop_R", "Brow_Raise_Inner_L", "Brow_Raise_Inner_R",
    "Brow_Compress_L", "Brow_Compress_R",
    "Cheek_Raise_L", "Cheek_Raise_R",
    "Mouth_Smile_L", "Mouth_Smile_R", "Mouth_Frown_L", "Mouth_Frown_R",
    "Mouth_Stretch_L", "Mouth_Stretch_R", "Mouth_Press_L", "Mouth_Press_R",
    "Mouth_Shrug_Upper", "Mouth_Shrug_Lower", "Jaw_Open",
    "Eye_L_Look_L", "Eye_R_Look_L", "Eye_L_Look_R", "Eye_R_Look_R",
    "Eye_L_Look_Up", "Eye_R_Look_Up", "Eye_L_Look_Down", "Eye_R_Look_Down",
}

TEXTURE_SIZE = 2048
SMALL_TEXTURE_SIZE = 512
# Materials whose parts are small enough on screen for SMALL_TEXTURE_SIZE.
SMALL = ("Std_Eye", "Std_Cornea", "Std_Nails", "Std_Eyelash", "Std_Upper_Teeth", "Std_Lower_Teeth", "Glass")


def main(fbx: pathlib.Path, maps_zip: pathlib.Path) -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(fbx))

    for name in DROP:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)

    for obj in bpy.data.objects:
        if obj.type == "MESH" and obj.data.shape_keys:
            keep_face_shapes(obj)

    with tempfile.TemporaryDirectory() as tmp:
        maps = extract_maps(maps_zip, pathlib.Path(tmp))
        for material in bpy.data.materials:
            if material.users:
                wire(material, maps)
        OUT.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.export_scene.gltf(
            filepath=str(OUT),
            export_format="GLB",
            export_image_format="WEBP",
            export_image_quality=90,
            export_yup=True,
            export_skins=True,
            export_morph=True,
            export_morph_normal=False,
            export_animations=False,
            export_extras=False,
        )
    print(f"Wrote {OUT} ({OUT.stat().st_size / 1e6:.1f} MB)")


def keep_face_shapes(obj) -> None:
    keys = obj.data.shape_keys.key_blocks
    # The teeth's own two shapes follow the jaw, and stay.
    if obj.name == "CC_Base_Teeth":
        return
    for key in list(keys)[1:]:
        if key.name not in FACE:
            obj.shape_key_remove(key)
    if len(keys) == 1:
        obj.shape_key_remove(keys[0])


def extract_maps(maps_zip: pathlib.Path, into: pathlib.Path) -> dict:
    """The zip's top-level maps, by name without the extension."""
    maps = {}
    with zipfile.ZipFile(maps_zip) as archive:
        for info in archive.infolist():
            name = info.filename
            if not name.startswith("Maps/") or name.count("/") != 1 or name.endswith((".tx", "/")):
                continue
            stem = pathlib.Path(name).stem
            maps[stem] = into / pathlib.Path(name).name
            maps[stem].write_bytes(archive.read(info))
    return maps


def find(maps: dict, material: str, kind: str):
    # The zip names some maps after a different level of detail than the
    # material (Hat_LP3 against the hat's Hat_LP2), and a few in other case.
    for stem, path in maps.items():
        if stem.lower() == f"{material}_{kind}".lower():
            return path
    return None


def image(path: pathlib.Path, size: int, colour: bool):
    img = bpy.data.images.load(str(path))
    if max(img.size) > size:
        img.scale(size, size)
    img.colorspace_settings.name = "sRGB" if colour else "Non-Color"
    return img


def wire(material, maps: dict) -> None:
    size = SMALL_TEXTURE_SIZE if material.name.startswith(SMALL) else TEXTURE_SIZE
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    old = {n.image.name: n for n in nodes if n.type == "TEX_IMAGE" and n.image}
    alpha = next((n for n in old.values() if any(l.to_socket.name == "Alpha" for l in n.outputs[0].links)), None)
    alpha_path = maps.get(pathlib.Path(alpha.image.name).stem) if alpha else None
    nodes.clear()

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    output = nodes.new("ShaderNodeOutputMaterial")
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    def texture(kind: str, colour: bool):
        path = find(maps, material.name, kind)
        if path is None:
            return None
        node = nodes.new("ShaderNodeTexImage")
        node.image = image(path, size, colour)
        return node

    diffuse = texture("Diffuse", True)
    if diffuse:
        links.new(diffuse.outputs["Color"], bsdf.inputs["Base Color"])
    normal = texture("Normal", False)
    if normal:
        normal_map = nodes.new("ShaderNodeNormalMap")
        links.new(normal.outputs["Color"], normal_map.inputs["Color"])
        links.new(normal_map.outputs["Normal"], bsdf.inputs["Normal"])
    roughness = texture("roughness", False)
    if roughness:
        links.new(roughness.outputs["Color"], bsdf.inputs["Roughness"])
    metallic = texture("metallic", False)
    if metallic:
        links.new(metallic.outputs["Color"], bsdf.inputs["Metallic"])
    else:
        bsdf.inputs["Metallic"].default_value = 0.0

    if alpha_path:
        node = nodes.new("ShaderNodeTexImage")
        node.image = image(alpha_path, size, False)
        links.new(node.outputs["Color"], bsdf.inputs["Alpha"])
        material.surface_render_method = "BLENDED" if material.name.startswith("Std_Cornea") else "DITHERED"


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:]
    main(pathlib.Path(args[0]), pathlib.Path(args[1]))
