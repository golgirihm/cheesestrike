"""The Labyrinth's surfaces: stone, brick, flagstones and old wood for a cellar, from Poly Haven (CC0,
https://polyhaven.com/license), drawn from world space on the map's blocks by M_Surface (build_materials.py).

Run it twice. With ordinary Python it downloads each texture's maps (base colour, OpenGL normal, roughness, ambient
occlusion, 2K JPEGs) into C:\\Hamid\\code\\FabDownloads\\polyhaven\\<asset>\\<asset>_<map>.jpg (set CHEESESTRIKE_FAB
to put them elsewhere); what is there already is kept:

    python tools/art/cellar_materials.py

Then in the editor, with the editor closed, it imports them into /Game/PolyHaven/<asset>/T_<asset>_<map> (cut to
the size in TEXTURES) and makes the materials, MI_<surface> in /Game/CheeseStrike/Materials, as new assets only: it
changes no asset that was there before it (M_Surface is only their parent).

    UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/cellar_materials.py" -unattended -nullrhi

(both paths absolute). It writes what it did to Saved/cellar_materials.txt."""

import os
import traceback

try:
    import unreal
except ImportError:
    unreal = None

FAB = os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads")
SOURCE = os.path.join(FAB, "polyhaven")
# Poly Haven's name for each texture, by Rob Tuytel and Amal Kumar: the largest size kept.
TEXTURES = {
    "medieval_blocks_03": 2048,       # dressed stone: the pillars, the vault, the cellar's walls
    "medieval_red_brick": 2048,       # the ceiling's vaults
    "monastery_stone_floor": 2048,    # the floor's flagstones
    "old_planks_02": 1024,            # the gallery's deck, shelves, crates
    "medieval_wood": 1024,            # the doors: planks and iron studs
}
# Surfaces: name -> (texture, TileU, TileV: the centimetres it covers, as Poly Haven measured it; M_Surface's settings).
SURFACES = {
    "StoneBlocks": ("medieval_blocks_03", 200, 200, {"Macro": 0.3, "Grime": 0.3, "Tint": (0.92, 0.88, 0.82)}),
    "OldBrick": ("medieval_red_brick", 200, 200, {"Macro": 0.35, "Grime": 0.2, "Tint": (0.85, 0.8, 0.76)}),
    "Flagstones": ("monastery_stone_floor", 180, 180, {"Macro": 0.35, "Tint": (0.95, 0.92, 0.88)}),
    "OldPlanks": ("old_planks_02", 200, 200, {"Macro": 0.25, "Tint": (0.8, 0.72, 0.62)}),
    "StuddedPlanks": ("medieval_wood", 200, 200, {"Macro": 0.15}),
}
# Poly Haven's names for the maps, and ours.
MAPS = {"diff": "BaseColor", "nor_gl": "Normal", "rough": "Roughness", "ao": "AO"}


def download():
    import urllib.request
    for asset in TEXTURES:
        os.makedirs(os.path.join(SOURCE, asset), exist_ok=True)
        for theirs, ours in MAPS.items():
            path = os.path.join(SOURCE, asset, f"{asset}_{ours}.jpg")
            if os.path.exists(path) and os.path.getsize(path) > 0:
                continue
            url = f"https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/{asset}/{asset}_{theirs}_2k.jpg"
            request = urllib.request.Request(url, headers={"User-Agent": "CheeseStrike map tools"})
            try:
                with urllib.request.urlopen(request, timeout=120) as response, open(path + ".part", "wb") as f:
                    f.write(response.read())
                os.replace(path + ".part", path)
                print("  " + os.path.basename(path))
            except Exception as error:
                print(f"  {asset} has no {theirs}: {error}")


def import_all():
    lines = []
    lib = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    root, ours = "/Game/PolyHaven", "/Game/CheeseStrike/Materials"

    def texture(asset, kind, limit):
        destination, name = f"{root}/{asset}", f"T_{asset}_{kind}"
        if lib.does_asset_exist(f"{destination}/{name}"):
            return unreal.load_asset(f"{destination}/{name}")
        path = os.path.join(SOURCE, asset, f"{asset}_{kind}.jpg")
        if not os.path.exists(path):
            return None
        task = unreal.AssetImportTask()
        for k, v in (("filename", path), ("destination_path", destination), ("destination_name", name), ("automated", True),
                     ("replace_existing", True), ("save", False)):
            task.set_editor_property(k, v)
        tools.import_asset_tasks([task])
        found = [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]
        t = next((a for a in found if isinstance(a, unreal.Texture2D)), None)
        if t is None:
            lines.append(f"  {name}: nothing imported from {path}")
            return None
        if kind == "BaseColor":
            t.set_editor_property("srgb", True)
        elif kind == "Normal":
            # Poly Haven's OpenGL normals, green up; Unreal's are DirectX's.
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False)
            t.set_editor_property("flip_green_channel", True)
        else:
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            t.set_editor_property("srgb", False)
        width = t.blueprint_get_size_x()
        if width > limit:
            unreal.TextureAssetActionsBlueprintFns.resize_texture_source(t, limit)
        lib.save_asset(t.get_path_name(), False)
        lines.append(f"  {name}: {width} -> {t.blueprint_get_size_x()}")
        return t

    try:
        parent = unreal.load_asset(ours + "/M_Surface")
        maps = {}
        for asset, limit in TEXTURES.items():
            maps[asset] = {kind: t for kind in MAPS.values() for t in [texture(asset, kind, limit)] if t is not None}
            lines.append(f"{asset}: {sorted(maps[asset])}")
        for name, (asset, tile_u, tile_v, settings) in SURFACES.items():
            path = f"{ours}/MI_{name}"
            if lib.does_asset_exist(path):
                mi = unreal.load_asset(path)
            else:
                mi = tools.create_asset("MI_" + name, ours, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(mi, parent)
            mel.clear_all_material_instance_parameters(mi)
            for kind, t in maps[asset].items():
                mel.set_material_instance_texture_parameter_value(mi, kind, t)
            settings = dict(settings, TileU=tile_u, TileV=tile_v)
            tint = settings.pop("Tint", None)
            for k, v in settings.items():
                mel.set_material_instance_scalar_parameter_value(mi, k, float(v))
            if tint:
                mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
            lines.append(f"MI_{name} saved {lib.save_asset(path, False)}")
    except Exception:
        lines.append(traceback.format_exc())
    finally:
        open(unreal.Paths.project_saved_dir() + "cellar_materials.txt", "w").write("\n".join(lines))
        unreal.SystemLibrary.quit_editor()


if unreal is None:
    download()
else:
    import_all()
