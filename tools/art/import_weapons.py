import os, traceback
import unreal

# Run with the editor closed, after tools/art/weapon_textures.py, tools/art/weapon_models.py and
# tools/art/cheese_weapons.py (Blender) have written into FAB/x/weapons and tools/art/weapon_icons.py has drawn the
# icons:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/import_weapons.py [Name ...]" -unattended -nullrhi
# (both paths absolute). Then run tools/art/weapon_materials.py, which gives them the game's cheese materials.
# It writes what it did to Saved/import_weapons.txt. With names (of OURS, "Icons" for the icons), only those are
# imported, afresh.
#
# Imports the weapons' models: the Fab models of the primaries that aren't Lyra's (Content/LICENSES.md lists
# each listing) into /Game/Fab/<publisher>/<model>/, each a static mesh (no Nanite, which the first-person view
# doesn't draw; no collision, as a weapon in hand needs none) with its textures; and the Cheese Knife, the muzzle
# flash and the cheese grenade, made by weapon_models.py, into /Game/CheeseStrike/Weapons/. Lyra's shotgun, copied in with its 4096-pixel maps,
# has them cut to MAX. Also writes each mesh's vertices to Saved/weapon_shapes/ for looking at their shapes, and imports
# the HUD's weapon icons that tools/art/weapon_icons.py drew into FAB/x/weapons/icons, into /Game/CheeseStrike/Weapons/Icons/.
FAB = os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads")
WORK = os.path.join(FAB, "x", "weapons")
OUT = unreal.Paths.project_saved_dir() + "import_weapons.txt"
SHAPES = unreal.Paths.project_saved_dir() + "weapon_shapes/"
MAX = 2048
REIMPORT = os.environ.get("CHEESESTRIKE_FAB_REIMPORT") == "1"

# (asset, destination, model file under FAB, its texture sets (name in WORK, the maps it has))
MODELS = [
    ("PPSh41", "/Game/Fab/Max3d/PPSh41", "Max3d_PPSh41/source/PPSH 41/PPSH 41.fbx", [("PPSh41", True)]),
    ("DP28", "/Game/Fab/Max3d/DP28", "Max3d_DP28/source/Dp28/dp28.FBX", [("DP28", True)]),
    ("SV98", "/Game/Fab/SpatialNeglect/SV98", "SpatialNeglect_SV98/source/sv 98.fbx", [("SV98", True), ("SV98Scope", False)]),
    ("ClassicM4", "/Game/Fab/SpatialNeglect/ClassicM4", "SpatialNeglect_ClassicM4/source/Classic M4.fbx", [("ClassicM4", False), ("ClassicM4Scope", False)]),
    # The Dual Burratas' pistols: no maps of its own; weapon_materials.py makes it burrata and steel. Made along X,
    # muzzle toward -X: turned as it comes in to lie along Y, muzzle forward, as the game's weapons do.
    ("M1911", "/Game/Fab/Xeradev/M1911", "Xeradev_M1911/fbx/FBX/NonRigged/NonRigged_M1911.fbx", [], -90.0),
]
OURS = [("CheeseKnife", "/Game/CheeseStrike/Weapons/Knife", os.path.join(WORK, "cheese_knife.obj")),
        ("MuzzleFlash", "/Game/CheeseStrike/Weapons/Flash", os.path.join(WORK, "muzzle_flash.obj")),
        ("FragGrenade", "/Game/CheeseStrike/Weapons/Grenade", os.path.join(WORK, "frag_grenade.obj"))]
# The cheese weapons, modelled in Blender by tools/art/cheese_weapons.py.
OURS += [(asset, "/Game/CheeseStrike/Weapons/" + folder, os.path.join(WORK, f"cheese_{source}.obj")) for asset, folder, source in [
    ("CheddarBlaster", "Blaster", "blaster"), ("ColbyCarbine", "Carbine", "carbine"), ("RoquefortRifle", "Roquefort", "roquefort"),
    ("CurdPistol", "Pistol", "pistol"), ("SwissShotgun", "Shotgun", "shotgun"), ("MunsterSMG", "Munster", "munster"),
    ("CheeseGrater", "Grater", "grater"), ("GraterShard", "Grater", "grater_shard"), ("BabyBomb", "BabyBomb", "babybomb"),
    ("BabyBombRound", "BabyBomb", "babybomb_round"), ("CheeseStick", "Stick", "stick"), ("GoudaGatling", "Gatling", "gatling")]]
ONLY = [a for a in __import__("sys").argv[1:] if a]
SHOTGUN_TEXTURES = ["/Game/Weapons/Shotgun/Texture/T_Shotgun_AORM", "/Game/Weapons/Shotgun/Texture/T_Shotgun_D", "/Game/Weapons/Shotgun/Texture/T_Shotgun_N", "/Game/Weapons/Shotgun/Texture/T_Shotgun_Mask"]

lines = []
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def import_file(path, destination, name, options=None):
    task = unreal.AssetImportTask()
    if options is not None:
        task.set_editor_property("options", options)
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]


def texture(path, destination, name, kind):
    if not REIMPORT and lib.does_asset_exist(f"{destination}/{name}"):
        return unreal.load_asset(f"{destination}/{name}")
    imported = [a for a in import_file(path, destination, name) if isinstance(a, unreal.Texture2D)]
    if not imported:
        lines.append(f"  {name}: nothing imported from {path}")
        return None
    t = imported[0]
    if kind == "Albedo":
        t.set_editor_property("srgb", True)
    elif kind == "Normal":
        # Substance's and Max3d's normals are OpenGL's way up; Unreal's are DirectX's.
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
        t.set_editor_property("flip_green_channel", True)
    else:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        t.set_editor_property("srgb", False)
    lib.save_asset(t.get_path_name(), False)
    lines.append(f"  {name}: {t.blueprint_get_size_x()} pixels")
    return t


def model(asset, path, destination, yaw=0.0):
    """A static mesh, its parts combined, with its material slots; no Nanite, no collision."""
    if not REIMPORT and asset not in ONLY and lib.does_asset_exist(f"{destination}/SM_{asset}"):
        return unreal.load_asset(f"{destination}/SM_{asset}")
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    # A model with bones (the DP28 has some) is still taken as a plain mesh.
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("build_nanite", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    # The models' own normals: smooth cheese, sharp where they are cut.
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    if yaw:
        data.set_editor_property("import_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    imported = import_file(path, destination, "SM_" + asset, options)
    mesh = None
    names = [a.get_name() for a in imported if a is not None]
    for a in imported:
        if isinstance(a, unreal.StaticMesh) and mesh is None:
            mesh = a
        elif a is not None and not isinstance(a, unreal.StaticMesh):
            lib.delete_asset(a.get_path_name())
    if mesh is None:
        lines.append(f"  SM_{asset}: no mesh in {path} ({names})")
        return None
    meshes.remove_collisions(mesh)
    lib.save_asset(mesh.get_path_name(), False)
    return mesh


def describe(asset, mesh):
    box = mesh.get_bounding_box()
    size = box.max - box.min
    slots = [str(m.material_slot_name) for m in mesh.get_editor_property("static_materials")]
    lines.append(f"  SM_{asset}: from ({box.min.x:.1f}, {box.min.y:.1f}, {box.min.z:.1f}) to ({box.max.x:.1f}, {box.max.y:.1f}, {box.max.z:.1f}), {size.x:.1f} x {size.y:.1f} x {size.z:.1f}; slots {slots}")
    # Its vertices, a few thousand at most, for drawing its shape outside the editor.
    os.makedirs(SHAPES, exist_ok=True)
    description = mesh.get_static_mesh_description(0)
    count = description.get_vertex_count()
    step = max(1, count // 6000)
    with open(SHAPES + asset + ".txt", "w") as f:
        for i in range(0, count, step):
            p = description.get_vertex_position(unreal.VertexID(i))
            f.write(f"{p.x:.3f} {p.y:.3f} {p.z:.3f}\n")


def icon(path, name):
    """A HUD icon: white on clear, drawn as it is, no mips."""
    destination = "/Game/CheeseStrike/Weapons/Icons"
    if not REIMPORT and "Icons" not in ONLY and lib.does_asset_exist(f"{destination}/{name}"):
        return
    imported = [a for a in import_file(path, destination, name) if isinstance(a, unreal.Texture2D)]
    if not imported:
        lines.append(f"  {name}: nothing imported from {path}")
        return
    t = imported[0]
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    lib.save_asset(t.get_path_name(), False)
    lines.append(f"  {name}: {t.blueprint_get_size_x()} x {t.blueprint_get_size_y()}")


try:
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ False")
    for asset, destination, path, sets, *turn in (MODELS if not ONLY else []):
        lines.append(asset)
        for name, has_cheese in sets:
            for kind in ["Albedo", "Normal", "AORM"] + (["Cheese"] if has_cheese else []):
                texture(os.path.join(WORK, f"T_{name}_{kind}.png"), destination, f"T_{name}_{kind}", kind)
        mesh = model(asset, os.path.join(FAB, path), destination, turn[0] if turn else 0.0)
        if mesh is not None:
            describe(asset, mesh)
    for asset, destination, path in OURS:
        if ONLY and asset not in ONLY:
            continue
        lines.append(asset)
        mesh = model(asset, path, destination)
        if mesh is not None:
            describe(asset, mesh)
    icons = os.path.join(WORK, "icons")
    if os.path.isdir(icons) and (not ONLY or "Icons" in ONLY):
        for file in sorted(os.listdir(icons)):
            if file.endswith(".png"):
                icon(os.path.join(icons, file), file[:-4])
    for path in ([] if ONLY else ["/Game/Weapons/Rifle/Mesh/SK_Rifle", "/Game/Weapons/Pistol/Mesh/SK_Pistol", "/Game/Weapons/Shotgun/Mesh/SKM_Shotgun"]):
        skeletal = unreal.load_asset(path)
        box = skeletal.get_bounds()
        lines.append(f"{path}: middle {box.origin}, half {box.box_extent}")
    for path in ([] if ONLY else SHOTGUN_TEXTURES):
        t = unreal.load_asset(path)
        width = t.blueprint_get_size_x()
        if width > MAX:
            unreal.TextureAssetActionsBlueprintFns.resize_texture_source(t, MAX)
            lib.save_asset(path, False)
        lines.append(f"{path}: {width} -> {t.blueprint_get_size_x()}")
    # Lyra's weapon textures, written out to look at.
    for path in [] if ONLY else ["/Game/Weapons/Rifle/Textures/T_Rifle_Masks", "/Game/Weapons/Rifle/Textures/T_Rifle_D", "/Game/Weapons/Pistol/Textures/T_Pistol_Masks", "/Game/Weapons/Pistol/Textures/T_Pistol_D", "/Game/Weapons/Shotgun/Texture/T_Shotgun_Mask", "/Game/Weapons/Shotgun/Texture/T_Shotgun_D"]:
        task = unreal.AssetExportTask()
        task.set_editor_property("object", unreal.load_asset(path))
        task.set_editor_property("filename", SHAPES + path.split("/")[-1] + ".png")
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_identical", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("exporter", unreal.TextureExporterPNG())
        lines.append(f"exported {path}: {unreal.Exporter.run_asset_export_task(task)}")
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
