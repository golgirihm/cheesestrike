"""Writes the bounds and material slots of the weapon models wave 14's placeholders borrow (Saved/measure_weapon_models.txt):
where a model's muzzle, grip and fore-end are, for CheeseStrikeWeaponLooks. Run headless in the editor:

    UnrealEditor-Cmd.exe CheeseStrike.uproject -ExecCmds="py tools/art/measure_weapon_models.py" -unattended -nullrhi
"""

import os
import unreal

MODELS = [
    "/Game/Weapons/GrenadeLauncher/Meshes/SM_GrenadeLauncher",
    "/Game/Fab/Max3d/PPSh41/SM_PPSh41",
    "/Game/Fab/Max3d/DP28/SM_DP28",
    "/Engine/BasicShapes/Cube",
    "/Engine/BasicShapes/Cylinder",
]

lines = []
for path in MODELS:
    mesh = unreal.load_asset(path)
    if mesh is None:
        lines.append(f"{path}: missing")
        continue
    box = mesh.get_bounding_box()
    lines.append(f"{path}: min ({box.min.x:.1f}, {box.min.y:.1f}, {box.min.z:.1f}) max ({box.max.x:.1f}, {box.max.y:.1f}, {box.max.z:.1f})")
    for index, material in enumerate(mesh.static_materials):
        lines.append(f"  slot {index}: {material.material_slot_name} {material.material_interface.get_path_name() if material.material_interface else None}")
    # Where the vertices are along its length, in slices, for a rough side view.
    try:
        verts = unreal.EditorStaticMeshLibrary.get_number_verts(mesh, 0)
        lines.append(f"  vertices {verts}")
    except Exception as error:
        lines.append(f"  {error}")

out = os.path.join(unreal.Paths.project_saved_dir(), "measure_weapon_models.txt")
with open(out, "w") as f:
    f.write("\n".join(lines) + "\n")
unreal.SystemLibrary.quit_editor()
