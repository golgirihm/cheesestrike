import io, os, re, struct, traceback, zipfile, zlib
import unreal

# Run with the editor closed, once the Fab downloads below are in FAB:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/import_fab.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder). Then run
# build_materials.py, which makes the materials that use what this imports.
# It writes what it did to Saved/import_fab.txt.
#
# Imports the free Fab assets the maps are dressed with into /Game/Fab/<publisher>/<asset>/:
# each one's textures (base colour, normal, roughness, ambient occlusion, opacity, metalness,
# whichever it has) and, for a model, its mesh, with Nanite on and box collision (for shots too). The
# downloads are the files Fab's web page gives (Download, then the zip named below), kept
# outside the repository in FAB (set CHEESESTRIKE_FAB to put them elsewhere); Content/LICENSES.md
# lists each listing. Textures are cut to MAX pixels (small props to SMALL) to keep the
# assets repository small, as Belica's were.
FAB = os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads")
# What is imported already is left as it is (a re-import saves every asset anew, and Git LFS keeps both copies);
# set CHEESESTRIKE_FAB_REIMPORT=1 to import everything again.
REIMPORT = os.environ.get("CHEESESTRIKE_FAB_REIMPORT") == "1"
WORK = os.path.join(FAB, "x")
OUT = unreal.Paths.project_saved_dir() + "import_fab.txt"
ROOT = "/Game/Fab"
OURS = "/Game/CheeseStrike/Materials"
MAX = 2048
SMALL = 1024
WAREHOUSE = "warehouse_mid.zip"  # Quixel Megascans' Warehouse pack, the files under each asset's id
MINE = "grotto/mine_mid.zip"  # Quixel Megascans' Old Mine pack, the same

# Surfaces, drawn from world space on the maps' blocks: (asset, zip, folder in the zip).
SURFACES = [
    ("ConcreteFloor", "industrial_concrete_floor_sl2qedtp_2k.zip", ""),
    ("ConcreteWall", "shuttered_concrete_wall_2k.zip", ""),
    ("ConcreteFacade", WAREHOUSE, "ufouccbo/"),
    ("Asphalt", "asphalt_2k.zip", ""),
    ("CorrugatedSheets", "painted_corrugated_sheets_2k.zip", ""),
    ("PaintedMetal", "scratched_painted_metal_sheet_2k.zip", ""),
    ("GarageDoor", "garage_door_2k.zip", ""),
    ("PaintedRoof", WAREHOUSE, "vd3kcjs/"),
    ("MineRockWall", MINE, "uebmddyn/"),
    ("MineRockDark", MINE, "ueknfaclw/"),
    ("CoalMineFloor", MINE, "ud0nfezew/"),
]
# Models: (asset, zip, folder in the zip, texture limit).
MODELS = [
    ("Pallet", WAREHOUSE, "vh1jeck/", MAX),
    ("TarpedCrate", WAREHOUSE, "vh3lbfy/", MAX),
    ("Barrel", WAREHOUSE, "ui1maewga/", MAX),
    ("PlasticDrum", WAREHOUSE, "vgyiciqaw/", SMALL),
    ("PlasticCrate", WAREHOUSE, "vh2jfg1s/", SMALL),
    ("CardboardBoxes", WAREHOUSE, "tlbjdbova/", SMALL),
    ("FluorescentLamp", WAREHOUSE, "vidrear/", SMALL),
    ("MetalCabinet", WAREHOUSE, "vjhledeaw/", SMALL),
    ("Workbench", WAREHOUSE, "wbuidixga/", SMALL),
    ("HandTruck", WAREHOUSE, "virqcfw/", SMALL),
    ("GasTank", WAREHOUSE, "vizqehw/", SMALL),
    ("TrashCan", WAREHOUSE, "vhskccxaw/", SMALL),
    ("ConcreteBarrier", "concrete_barrier_3m_mid.zip", "", MAX),
    ("QuarryCliff", "grotto/quarry_cliff_uchwaffda_mid.zip", "", MAX),
    ("QuarryCliffLarge", "grotto/quarry_cliff_ubhubhyda_mid.zip", "", MAX),
    ("MineCart", MINE, "ufmodhpfa/", SMALL),
    ("BulkheadLight", MINE, "vgyidfpaw/", SMALL),
    ("WoodenBeam", MINE, "tgnhde0fa/", SMALL),
    ("RailTrack", MINE, "ufekaeedw/", SMALL),
]
# Decals, projected on the ground and walls: (asset, zip, folder in the zip).
DECALS = [
    ("OilStain", "oil_stain_2k.zip", ""),
    ("Leakage", WAREHOUSE, "tdcafitc/"),
    ("AsphaltCrack", WAREHOUSE, "ugcmfivcw/"),
    ("PaintedOne", WAREHOUSE, "ukpocglfw/"),
    ("PaintedTwo", WAREHOUSE, "ukpodcofw/"),
    ("PaintedThree", WAREHOUSE, "ukpodeifw/"),
]
# Nestaeric's Black Rat, for the mice: a rigged mesh with its idle, walk and run animations, and its textures.
RAT = "blackrat_free_fbx.zip"
# The mice's sounds, from OpenGameArt (downloaded into FAB/oga, the archives unpacked beside them):
# (asset, file under FAB/oga, whether it loops).
SOUNDS = [
    ("RatSqueak", "qubodupSqueakyRat/qubodupSqueakyRat/qubodupSqueakyRatAttack.flac", False),
    ("RatSqueakPain", "qubodupSqueakyRat/qubodupSqueakyRat/qubodupSqueakyRatPain.flac", False),
    ("RatScream", "qubodupRat/qubodupRat/qubodupRatPain.flac", False),
    ("Gnawing", "RabbitEating.wav", True),
    ("Crunch1", "crunch_-_tito/crunch.1.ogg", False),
    ("Crunch2", "crunch_-_tito/crunch.2.ogg", False),
    ("Crunch3", "crunch_-_tito/crunch.3.ogg", False),
    ("Crunch4", "crunch_-_tito/crunch.4.ogg", False),
]
# Sousinho Games' shipping container: one mesh, three materials (sides, doors, frame).
CONTAINER = "container_orange.zip"
CONTAINER_MESH = "source/shipping cointainer Orange.zip"
CONTAINER_PARTS = {"Sides": "corrugated sides", "Doors": "front doors", "Frame": "frame"}

MAPS = {"basecolor": "BaseColor", "color": "BaseColor", "albedo": "BaseColor", "normal": "Normal", "roughness": "Roughness", "rough": "Roughness", "ao": "AO",
        "opacity": "Opacity", "metalness": "Metalness", "metallic": "Metalness", "metal": "Metalness"}
lines = []
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
zips = {}


def archive(name):
    if name not in zips:
        zips[name] = zipfile.ZipFile(os.path.join(FAB, name))
    return zips[name]


def extract(z, member, folder):
    """Writes one file from a zip into WORK/folder and returns its path."""
    os.makedirs(os.path.join(WORK, folder), exist_ok=True)
    path = os.path.join(WORK, folder, os.path.basename(member))
    with open(path, "wb") as f:
        f.write(z.read(member))
    return path


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


def texture(path, destination, name, kind, limit):
    """Imports one texture map and sets it up for what it is."""
    if not REIMPORT and lib.does_asset_exist(f"{destination}/{name}"):
        return unreal.load_asset(f"{destination}/{name}")
    imported = [a for a in import_file(path, destination, name) if isinstance(a, unreal.Texture2D)]
    if not imported:
        lines.append(f"  {name}: nothing imported from {path}")
        return None
    t = imported[0]
    if kind == "BaseColor":
        t.set_editor_property("srgb", True)
    elif kind == "Normal":
        # Megascans' (and Substance's) normals are OpenGL's way up; Unreal's are DirectX's.
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


def texture_set(asset, zip_name, folder, destination, limit):
    """Imports the maps of one Megascans asset found under `folder` in the zip."""
    z = archive(zip_name)
    found = {}
    for member in z.namelist():
        if not member.startswith(folder) or "/" in member[len(folder):]:
            continue
        m = re.search(r"_([A-Za-z]+)\.(jpe?g|png)$", member, re.IGNORECASE)
        if m and m.group(1).lower() in MAPS:
            found[MAPS[m.group(1).lower()]] = member
    for kind, member in sorted(found.items()):
        texture(extract(z, member, asset), destination, f"T_{asset}_{kind}", kind, limit)
    return sorted(found)


def model(asset, fbx_path, destination):
    """Imports a mesh, its parts combined into one: Nanite, box collision, no materials (build_materials.py gives it ours)."""
    if not REIMPORT and lib.does_asset_exist(f"{destination}/SM_{asset}"):
        return
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("build_nanite", True)
    imported = import_file(fbx_path, destination, "SM_" + asset, options)
    mesh = None
    for a in imported:
        if isinstance(a, unreal.StaticMesh) and mesh is None:
            mesh = a
        elif not isinstance(a, unreal.StaticMesh):
            lib.delete_asset(a.get_path_name())
    if mesh is None:
        lines.append(f"  SM_{asset}: no mesh in {fbx_path} ({[a.get_name() for a in imported]})")
        return
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", nanite)
    meshes.remove_collisions(mesh)
    meshes.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    # Shots and sight lines meet the box too, not the model's own shape: stacked crates leave no slits
    # between their rounded tarps to shoot through, and cover is exactly as big as it looks from afar.
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    lib.save_asset(mesh.get_path_name(), False)
    box = mesh.get_bounding_box()
    size = box.max - box.min
    lines.append(f"  SM_{asset}: {size.x:.0f} x {size.y:.0f} x {size.z:.0f} cm, {len(mesh.get_editor_property('static_materials'))} materials")


def skeletal(asset, fbx_path, destination):
    """Imports a rigged model and its animations: no materials (build_materials.py gives it ours), no physics asset."""
    if not REIMPORT and lib.does_asset_exist(f"{destination}/SK_{asset}"):
        return
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("create_physics_asset", False)
    import_file(fbx_path, destination, "SK_" + asset, options)
    lib.save_directory(destination, False, True)
    for path in lib.list_assets(destination, True, False):
        lines.append(f"  {path.split('.')[-1]}: {lib.find_asset_data(path).asset_class_path.asset_name}")


def sound(asset, path, looping):
    """Imports a sound wave, looping or not, into /Game/OpenGameArt/Mice."""
    destination = "/Game/OpenGameArt/Mice"
    if not REIMPORT and lib.does_asset_exist(f"{destination}/SW_{asset}"):
        return
    imported = [a for a in import_file(path, destination, "SW_" + asset) if isinstance(a, unreal.SoundWave)]
    if not imported:
        lines.append(f"  SW_{asset}: nothing imported from {path}")
        return
    imported[0].set_editor_property("looping", looping)
    lib.save_asset(imported[0].get_path_name(), False)
    lines.append(f"  SW_{asset}: {imported[0].get_editor_property('duration'):.1f} s")


def white():
    """A small linear white texture: what a material uses for a map an asset hasn't got."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    rows = b"".join(b"\x00" + b"\xff" * 12 for _ in range(4))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    os.makedirs(WORK, exist_ok=True)
    path = os.path.join(WORK, "T_White.png")
    with open(path, "wb") as f:
        f.write(png)
    texture(path, OURS, "T_White", "Mask", MAX)


try:
    # Unreal's own FBX importer, which combines a model's parts into one mesh; Interchange makes a mesh of each.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
    white()
    for asset, zip_name, folder in SURFACES:
        lines.append(f"{asset}: {texture_set(asset, zip_name, folder, f'{ROOT}/Megascans/Surfaces/{asset}', MAX)}")
    for asset, zip_name, folder in DECALS:
        lines.append(f"{asset}: {texture_set(asset, zip_name, folder, f'{ROOT}/Megascans/Decals/{asset}', SMALL)}")
    for asset, zip_name, folder, limit in MODELS:
        destination = f"{ROOT}/Megascans/Props/{asset}"
        lines.append(f"{asset}: {texture_set(asset, zip_name, folder, destination, limit)}")
        z = archive(zip_name)
        fbx = [m for m in z.namelist() if m.startswith(folder) and m.lower().endswith(".fbx")]
        model(asset, extract(z, fbx[0], asset), destination)
    # The mice's sounds.
    for asset, path, looping in SOUNDS:
        sound(asset, os.path.join(FAB, "oga", path), looping)
    # The rat, for the mice.
    destination = f"{ROOT}/Nestaeric/BlackRat"
    lines.append(f"BlackRat: {texture_set('BlackRat', RAT, '', destination, MAX)}")
    z = archive(RAT)
    skeletal("BlackRat", extract(z, "blackrat.fbx", "BlackRat"), destination)
    # The container: its mesh is in a zip in the zip; its textures by part.
    destination = f"{ROOT}/SousinhoGames/ShippingContainer"
    z = archive(CONTAINER)
    inner = zipfile.ZipFile(io.BytesIO(z.read(CONTAINER_MESH)))
    for part, label in CONTAINER_PARTS.items():
        for member in z.namelist():
            m = re.search(r"_(BaseColor|Normal|Roughness|Metallic)\.1001\.png$", member)
            if member.startswith("textures/") and label in member.replace("_", " ") and m:
                kind = MAPS[m.group(1).lower()]
                texture(extract(z, member, "Container"), destination, f"T_Container_{part}_{kind}", kind, MAX)
    model("Container", extract(inner, [m for m in inner.namelist() if m.lower().endswith(".fbx")][0], "Container"), destination)
    lines.append("container done")
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
