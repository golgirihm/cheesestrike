"""Rounded blocks of cheese for the maps: every box of cheese a map's script places (map_kit.cheese, and cheese_box,
cheese_stack, cheese_wall, cheese_floor, pillar_deck, the buildings', tunnels' and bridges' cheese, edible and waxed
alike) is a static mesh with its edges rounded off, as a real block of cheese is cut and as SM_CheeseWheel's are
(wheel_mesh.py), rather than the engine's sharp cube. Scaling one rounded mesh would stretch its rounding with the
block, so there is one mesh for each distinct size the maps use, made in the editor the first time a map asks for it
and kept in /Game/CheeseStrike/Blocks as SM_CheeseBlock_<x>x<y>x<z> (centimetres): a map rebuilt finds its meshes
there; a size a map no longer uses leaves its mesh behind, harmlessly.

Each mesh is a box ROUNDING cm rounded along every edge (less on a block too thin for it), standing OVERLAP cm proud
of its size on every face, so that blocks laid edge to edge (a wall's courses, a floor's tiles) run into each other a
little and show a shallow seam rather than a slit; its collision is a plain box of the size asked for, used for shots
too, so that nothing of the game changes (the minimap's outlines, the mice's holds, where a shot lands). Its bounds are
the mesh's own, all of what is drawn: the game measures a block by its collision (ACheeseStrikeCheese::GetBlockBox),
never by its bounds. (Until wave 16 the bounds were pulled in to the box, 2 cm inside what is drawn; the renderer's
occlusion test then found a block hidden behind its own faces, and blocks vanished for a frame now and then, which
flickered.) The cheese material carves its holes in world space from the mesh's custom primitive data, so it reads on
these as on the cube.

Run on its own, with the editor closed, it makes every mesh again (after a change to the rounding), from the names in
the folder:

    UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/block_mesh.py" -unattended -nullrhi

and writes what it did to Saved/block_mesh.txt. With `bounds` as its argument it only gives every mesh in the folder its
own bounds (no extension), in place, keeping the maps' references to them.
"""

import os
import re
import sys
import traceback
import unreal

FOLDER = "/Game/CheeseStrike/Blocks"
ROUNDING = 7.0      # cm: the radius of the rounding along every edge (6 to 8 reads as cut cheese at life size)
OVERLAP = 2.0       # cm: how far each face stands proud of the collision box, so that neighbours run together
SUBDIVISIONS = 3    # edge loops across the rounding: 3 reads round from a metre away without doubling the triangles
MATERIAL = "/Game/CheeseStrike/Materials/M_Cheese"

_cache = {}


def name_for(size):
    """SM_CheeseBlock_<x>x<y>x<z>, the size rounded to whole centimetres: sizes within half a centimetre share a mesh."""
    return "SM_CheeseBlock_%dx%dx%d" % tuple(int(round(float(s))) for s in size)


def size_from(name):
    """The size a mesh's name says, or None."""
    found = re.fullmatch(r"SM_CheeseBlock_(\d+)x(\d+)x(\d+)", name)
    return tuple(float(n) for n in found.groups()) if found else None


def rounded_block(size, lines=None):
    """The rounded block mesh for a block `size` (x, y, z in cm): loaded from the folder, or made and saved there."""
    name = name_for(size)
    if name in _cache:
        return _cache[name]
    path = FOLDER + "/" + name
    mesh = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mesh is None:
        mesh = make(size, lines)
    _cache[name] = mesh
    return mesh


def make(size, lines=None):
    """Makes and saves the mesh for `size`, replacing any there; returns it."""
    name = name_for(size)
    path = FOLDER + "/" + name
    x, y, z = (float(s) for s in size)
    # The rounding must leave a flat middle on the thinnest side, or the bevels meet and the geometry folds.
    rounding = min(ROUNDING, min(x, y, z) / 3.0)
    shown = unreal.DynamicMesh()
    options = unreal.GeometryScriptPrimitiveOptions()
    options.set_editor_property("polygroup_mode", unreal.GeometryScriptPrimitivePolygroupMode.PER_FACE)
    unreal.GeometryScript_Primitives.append_box(shown, options, unreal.Transform(), x + 2 * OVERLAP, y + 2 * OVERLAP, z + 2 * OVERLAP,
                                                0, 0, 0, unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    bevel = unreal.GeometryScriptMeshBevelOptions()
    bevel.set_editor_property("bevel_distance", rounding)
    bevel.set_editor_property("subdivisions", SUBDIVISIONS)
    bevel.set_editor_property("round_weight", 1.0)
    bevel.set_editor_property("infer_material_id", True)
    unreal.GeometryScript_MeshModeling.apply_mesh_polygroup_bevel(shown, bevel)
    # Smooth over the rounding and the faces alike: no edge is sharp on a rounded block.
    normals = unreal.GeometryScriptCalculateNormalsOptions()
    normals.set_editor_property("angle_weighted", True)
    normals.set_editor_property("area_weighted", True)
    unreal.GeometryScript_Normals.set_per_vertex_normals(shown)
    unreal.GeometryScript_Normals.recompute_normals(shown, normals)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    asset_options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    asset_options.set_editor_property("enable_nanite", False)
    made, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(shown, path, asset_options)
    if made is None:
        raise RuntimeError(f"could not make {path}: {outcome}")
    # Its collision: the plain box of the size asked for (not the rounded, overlapping shape), for shots too.
    box = unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_box(box, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), x, y, z, 0, 0, 0,
                                                unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    collision = unreal.GeometryScriptCollisionFromMeshOptions()
    collision.set_editor_property("method", unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)
    unreal.GeometryScript_Collision.set_static_mesh_collision_from_mesh(box, made, collision)
    body = made.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    # Its bounds the mesh's own, all that is drawn (never pulled in: the renderer culls by them).
    made.set_editor_property("positive_bounds_extension", unreal.Vector(0, 0, 0))
    made.set_editor_property("negative_bounds_extension", unreal.Vector(0, 0, 0))
    made.set_material(0, unreal.load_asset(MATERIAL))
    unreal.EditorAssetLibrary.save_asset(path, False)
    if lines is not None:
        b = made.get_bounding_box()
        boxes = len(body.get_editor_property("agg_geom").get_editor_property("box_elems"))
        lines.append(f"made {name}: {made.get_num_triangles(0)} triangles, rounded {rounding:.1f}, bounds {b.max.x - b.min.x:.0f} x {b.max.y - b.min.y:.0f} x {b.max.z - b.min.z:.0f}, {boxes} collision box")
    return made


def true_bounds(lines):
    """Every mesh in the folder given its own bounds, in place: no extension either way."""
    for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False, include_folder=False):
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh) or size_from(mesh.get_name()) is None:
            continue
        mesh.set_editor_property("positive_bounds_extension", unreal.Vector(0, 0, 0))
        mesh.set_editor_property("negative_bounds_extension", unreal.Vector(0, 0, 0))
        saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        b = mesh.get_bounds()
        lines.append(f"{mesh.get_name()}: bounds {2 * b.box_extent.x:.0f} x {2 * b.box_extent.y:.0f} x {2 * b.box_extent.z:.0f}, saved {saved}")


def remake_all(lines):
    """Every mesh in the folder made again, by the size its name says."""
    for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False, include_folder=False):
        name = path.split("/")[-1].split(".")[0]
        size = size_from(name)
        if size is None:
            lines.append(f"left alone: {name}")
            continue
        make(size, lines)


if __name__ == "__main__":
    out = unreal.Paths.project_saved_dir() + "block_mesh.txt"
    lines = []
    try:
        sizes = [tuple(float(s) for s in arg.split("x")) for arg in sys.argv[1:] if arg != "bounds"]
        if "bounds" in sys.argv[1:]:
            true_bounds(lines)
        elif sizes:
            for size in sizes:
                make(size, lines)
        else:
            remake_all(lines)
    except Exception:
        lines.append(traceback.format_exc())
    finally:
        open(out, "w").write("\n".join(lines))
        unreal.SystemLibrary.quit_editor()
