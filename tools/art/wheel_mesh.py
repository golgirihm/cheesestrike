import math, traceback
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wheel_mesh.py" -unattended -nullrhi
# (both paths absolute). It writes what it did to Saved/wheel_mesh.txt.
#
# Makes SM_CheeseWheel (/Game/CheeseStrike/Meshes): a wheel of cheese as it is made, its sides a little rounded out
# and its edges rounded off, a metre across at its widest and a metre high, about its middle, as the engine's
# cylinder is, so that a map scales it to any wheel (map_kit.py, wheel(mesh=...)). Stacked, the rounded edges make a
# groove where one wheel sits on the next, so a stack reads as wheels, not as a post. It is shaped for the wheels the
# maps use, about 2.4 times as wide as high: the rounding is about 8 cm across and 10 cm up on a wheel 240 by 100.
# Its collision is one convex hull of it (for shots too), so a stack has no slits where the edges meet.
OUT = unreal.Paths.project_saved_dir() + "wheel_mesh.txt"
PATH = "/Game/CheeseStrike/Meshes/SM_CheeseWheel"
SIDES = 48
lines = []


def profile():
    """The wheel's side from its bottom edge to its top edge: (radius, height) in a 100 cm wheel."""
    bulge = 1.0         # how far its middle stands out past where the rounding starts
    across, up = 3.4, 10.0
    knee = 50.0 - up
    points = []
    # The bottom edge's rounding, then the side, then the top's.
    for i in range(7):
        t = math.radians(90.0 - 90.0 * i / 6.0)
        points.append((50.0 - bulge - across + across * math.cos(t), -knee - up * math.sin(t)))
    for i in range(1, 12):
        z = -knee + 2.0 * knee * i / 12.0
        points.append((50.0 - bulge * (z / knee) ** 2, z))
    for i in range(7):
        t = math.radians(90.0 * i / 6.0)
        points.append((50.0 - bulge - across + across * math.cos(t), knee + up * math.sin(t)))
    return points


try:
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptPrimitiveOptions()
    revolve = unreal.GeometryScriptRevolveOptions()
    revolve.set_editor_property("revolve_degrees", 360.0)
    revolve.set_editor_property("hard_normals", False)
    path = [unreal.Vector2D(r, z) for r, z in profile()]
    unreal.GeometryScript_Primitives.append_revolve_path(mesh, options, unreal.Transform(), path, revolve, SIDES, True)
    unreal.GeometryScript_Normals.recompute_normals(mesh, unreal.GeometryScriptCalculateNormalsOptions())
    if unreal.EditorAssetLibrary.does_asset_exist(PATH):
        unreal.EditorAssetLibrary.delete_asset(PATH)
    made, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, PATH, unreal.GeometryScriptCreateNewStaticMeshAssetOptions())
    lines.append(f"made {PATH}: {outcome}")
    # Its collision: one convex hull, used for shots and sight too.
    collision = unreal.GeometryScriptCollisionFromMeshOptions()
    collision.set_editor_property("method", unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS)
    collision.set_editor_property("max_convex_hulls_per_mesh", 1)
    unreal.GeometryScript_Collision.set_static_mesh_collision_from_mesh(mesh, made, collision)
    body = made.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    made.set_material(0, unreal.load_asset("/Game/CheeseStrike/Materials/M_Cheese"))
    b = made.get_bounding_box()
    lines.append(f"bounds {b.min.x:.1f} {b.min.y:.1f} {b.min.z:.1f} to {b.max.x:.1f} {b.max.y:.1f} {b.max.z:.1f}")
    lines.append(f"collision hulls {len(body.get_editor_property('agg_geom').get_editor_property('convex_elems'))}")
    lines.append("saved " + str(unreal.EditorAssetLibrary.save_asset(PATH, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
