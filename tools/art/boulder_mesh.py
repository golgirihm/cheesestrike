"""A closed boulder for the maps' floors (wave 15): Gruyère Grotto's boulders were Megascans quarry cliffs, which are
facades with open backs, and on the floor, walked round, they showed a flat slab hanging in the air from behind.
SM_Boulder (/Game/CheeseStrike/Meshes) is a lump that is closed from every side: a sphere a metre across, its vertices
pushed in and out along their normals by two layers of Perlin noise, in the maps' mine-rock surface drawn from world
space (so it shades as the cave's rock whatever its size), with one convex hull for collision (shots too). A map scales
it to any boulder (map_kit.fill); a different `seed` gives a different lump.

    boulder_mesh.boulder(seed=0)   the mesh, made and saved the first time it is asked for

Run on its own, with the editor closed, it makes them again:

    UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/boulder_mesh.py" -unattended -nullrhi
"""

import traceback
import unreal

FOLDER = "/Game/CheeseStrike/Meshes"
MATERIAL = "/Game/CheeseStrike/Materials/MI_MineRock"
SEEDS = (0, 1, 2)
_cache = {}


def name_for(seed):
    return "SM_Boulder" if seed == 0 else f"SM_Boulder_{seed}"


def boulder(seed=0, lines=None):
    """The boulder mesh for `seed`: loaded from the folder, or made and saved there."""
    name = name_for(seed)
    if name in _cache:
        return _cache[name]
    path = FOLDER + "/" + name
    mesh = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mesh is None:
        mesh = make(seed, lines)
    _cache[name] = mesh
    return mesh


def make(seed, lines=None):
    name = name_for(seed)
    path = FOLDER + "/" + name
    lump = unreal.DynamicMesh()
    options = unreal.GeometryScriptPrimitiveOptions()
    unreal.GeometryScript_Primitives.append_sphere_box(lump, options, unreal.Transform(), 50.0, 10, 10, 10, unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    # Two layers of noise: big lobes, then a rough skin over them.
    for magnitude, frequency, shift in ((9.0, 0.018, 0), (2.5, 0.07, 100)):
        layer = unreal.GeometryScriptPerlinNoiseLayerOptions()
        layer.set_editor_property("magnitude", magnitude)
        layer.set_editor_property("frequency", frequency)
        layer.set_editor_property("frequency_shift", unreal.Vector(shift + 37.0 * seed, 11.0 * seed, 5.0 * seed))
        layer.set_editor_property("random_seed", seed)
        noise = unreal.GeometryScriptPerlinNoiseOptions()
        noise.set_editor_property("base_layer", layer)
        noise.set_editor_property("apply_along_normal", True)
        unreal.GeometryScript_MeshDeformers.apply_perlin_noise_to_mesh2(lump, unreal.GeometryScriptMeshSelection(), noise)
    normals = unreal.GeometryScriptCalculateNormalsOptions()
    unreal.GeometryScript_Normals.set_per_vertex_normals(lump)
    unreal.GeometryScript_Normals.recompute_normals(lump, normals)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    asset_options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    asset_options.set_editor_property("enable_nanite", False)
    made, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(lump, path, asset_options)
    if made is None:
        raise RuntimeError(f"could not make {path}: {outcome}")
    collision = unreal.GeometryScriptCollisionFromMeshOptions()
    collision.set_editor_property("method", unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS)
    collision.set_editor_property("max_convex_hulls_per_mesh", 1)
    unreal.GeometryScript_Collision.set_static_mesh_collision_from_mesh(lump, made, collision)
    body = made.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    made.set_material(0, unreal.load_asset(MATERIAL))
    unreal.EditorAssetLibrary.save_asset(path, False)
    if lines is not None:
        b = made.get_bounding_box()
        lines.append(f"made {name}: {made.get_num_triangles(0)} triangles, bounds {b.min.x:.0f} {b.min.y:.0f} {b.min.z:.0f} to {b.max.x:.0f} {b.max.y:.0f} {b.max.z:.0f}")
    return made


if __name__ == "__main__":
    out = unreal.Paths.project_saved_dir() + "boulder_mesh.txt"
    lines = []
    try:
        for seed in SEEDS:
            make(seed, lines)
    except Exception:
        lines.append(traceback.format_exc())
    finally:
        open(out, "w").write("\n".join(lines))
        unreal.SystemLibrary.quit_editor()
