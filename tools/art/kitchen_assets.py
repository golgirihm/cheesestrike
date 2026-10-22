"""Crumb Kitchen's own art, for build_kitchen.py: its stainless steel and enamel, its books' pages, and its book.

Run in the editor, with the editor closed, after import_polyhaven.py (which brings in the kitchen's Poly Haven
surfaces and models, and makes the book covers this gives the book):

    UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/kitchen_assets.py" -unattended -nullrhi

(both paths absolute). It makes, as new assets only (it changes no asset that was there before it; M_Surface is only
their parent, and what is there already is left as it is unless CHEESESTRIKE_FAB_REIMPORT=1):
  MI_Kitchen_<metal>: stainless steel, enamel, dark glass and copper, from the maps' painted metal (import_fab.py)
    recoloured;
  T_KitchenPages and MI_Kitchen_Pages: a book's page edges, made here;
  SM_KitchenBook (/Game/CheeseStrike/Meshes): a book a metre each way, its covers and spine one material and its pages
    another, so that a pile of them is a staircase whose steps show their pages.
It writes what it did to Saved/kitchen_assets.txt."""

import os
import struct
import traceback
import zlib

import unreal

REIMPORT = os.environ.get("CHEESESTRIKE_FAB_REIMPORT") == "1"
OURS = "/Game/CheeseStrike/Materials"
BOOK = "/Game/CheeseStrike/Meshes/SM_KitchenBook"
PAINTED = "/Game/Fab/Megascans/Surfaces/PaintedMetal/T_PaintedMetal"
# Metal and enamel from the maps' painted metal: name -> M_Surface's settings.
METALS = {
    "Stainless": {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.2, "Tint": (0.62, 0.62, 0.63), "Metallic": 1.0, "RoughnessScale": 0.45},
    "Enamel_White": {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.12, "Tint": (0.82, 0.81, 0.78), "Metallic": 0.0, "RoughnessScale": 0.35},
    "Enamel_Cream": {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.15, "Tint": (0.78, 0.7, 0.52), "Metallic": 0.0, "RoughnessScale": 0.35},
    "Enamel_Red": {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.2, "Tint": (0.45, 0.06, 0.04), "Metallic": 0.0, "RoughnessScale": 0.4},
    "BlackGlass": {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.05, "Tint": (0.02, 0.02, 0.022), "Metallic": 0.0, "RoughnessScale": 0.15},
    "Copper": {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.3, "Tint": (0.6, 0.3, 0.17), "Metallic": 1.0, "RoughnessScale": 0.5},
}
lines = []
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def instance(name, parent, maps, scalars, tint):
    """An instance of `parent`, left as it is if it is there already."""
    path = f"{OURS}/{name}"
    if not REIMPORT and lib.does_asset_exist(path):
        return unreal.load_asset(path)
    mi = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, OURS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    for kind, t in maps.items():
        mel.set_material_instance_texture_parameter_value(mi, kind, t)
    for k, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, float(value))
    if tint:
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    lib.save_asset(path, False)
    lines.append(f"{name} made")
    return mi


def pages():
    """T_KitchenPages: a book's page edges, cream, with fine lines along them where the leaves meet, a little uneven."""
    path = f"{OURS}/T_KitchenPages"
    if not REIMPORT and lib.does_asset_exist(path):
        return unreal.load_asset(path)
    width, height = 64, 256
    rows = []
    for y in range(height):
        line = 0.86 - (0.1 if y % 4 == 0 else 0.0) - (0.04 if (y * 37) % 11 == 0 else 0.0)
        row = b"\x00"
        for x in range(width):
            v = line - 0.02 * (((x * 13 + y * 7) % 5) / 4.0)
            row += bytes((int(255 * v), int(255 * v * 0.96), int(255 * v * 0.86)))
        rows.append(row)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))
    file = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "T_KitchenPages.png")
    with open(file, "wb") as f:
        f.write(png)
    task = unreal.AssetImportTask()
    for k, value in (("filename", file), ("destination_path", OURS), ("destination_name", "T_KitchenPages"), ("automated", True),
                     ("replace_existing", True), ("save", False)):
        task.set_editor_property(k, value)
    tools.import_asset_tasks([task])
    t = unreal.load_asset(path)
    t.set_editor_property("srgb", True)
    lib.save_asset(path, False)
    lines.append("T_KitchenPages made")
    return t


def book(cover, leaves):
    """SM_KitchenBook: a book a metre each way about the middle of its bottom, lying flat, its spine at -X: covers at
    top and bottom and the spine (material 0), the pages between them set in a little (material 1). Its collision is
    its box, so a pile of them is a stair."""
    if not REIMPORT and lib.does_asset_exist(BOOK):
        return
    if lib.does_asset_exist(BOOK):
        lib.delete_asset(BOOK)
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptPrimitiveOptions()

    def block(x0, y0, z0, x1, y1, z1, material_id):
        options.set_editor_property("material_id", material_id)
        transform = unreal.Transform(location=[(x0 + x1) / 2.0, (y0 + y1) / 2.0, z0])
        unreal.GeometryScript_Primitives.append_box(mesh, options, transform, x1 - x0, y1 - y0, z1 - z0, 0, 0, 0, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    block(-50, -50, 0, 50, 50, 9, 0)        # the back cover
    block(-50, -50, 91, 50, 50, 100, 0)     # the front cover
    block(-50, -50, 9, -43, 50, 91, 0)      # the spine
    block(-43, -47, 9, 48, 47, 91, 1)       # the pages
    made, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, BOOK, unreal.GeometryScriptCreateNewStaticMeshAssetOptions())
    made.set_material(0, cover)
    made.set_material(1, leaves)
    meshes.remove_collisions(made)
    meshes.add_simple_collisions(made, unreal.ScriptCollisionShapeType.BOX)
    made.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    lib.save_asset(BOOK, False)
    lines.append(f"{BOOK}: {outcome}")


try:
    surface = unreal.load_asset(OURS + "/M_Surface")
    painted = {kind: unreal.load_asset(f"{PAINTED}_{kind}") for kind in ("BaseColor", "Normal", "Roughness", "AO", "Metalness")}
    for name, settings in METALS.items():
        settings = dict(settings, TileU=400, TileV=200)
        tint = settings.pop("Tint")
        instance("MI_Kitchen_" + name, surface, {k: t for k, t in painted.items() if t is not None}, settings, tint)
    page = instance("MI_Kitchen_Pages", surface, {"BaseColor": pages()}, {"TileU": 40, "TileV": 40, "Macro": 0.1}, None)
    book(unreal.load_asset(OURS + "/MI_Kitchen_Cover_Red"), page)
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(unreal.Paths.project_saved_dir() + "kitchen_assets.txt", "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
