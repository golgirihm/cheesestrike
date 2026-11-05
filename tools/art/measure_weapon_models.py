"""Writes the bounds and material slots of the weapons' models (Saved/measure_weapon_models.txt): where each lies in
the grip frame, for checking CheeseStrikeWeaponLooks' sights, muzzles and left hands against the models
tools/art/cheese_weapons.py made. Run headless in the editor:

    UnrealEditor-Cmd.exe CheeseStrike.uproject -ExecCmds="py tools/art/measure_weapon_models.py" -unattended -nullrhi
"""

import os
import unreal

MODELS = [
    "/Game/CheeseStrike/Weapons/Blaster/SM_CheddarBlaster",
    "/Game/CheeseStrike/Weapons/Carbine/SM_ColbyCarbine",
    "/Game/CheeseStrike/Weapons/Grater/SM_CheeseGrater",
    "/Game/CheeseStrike/Weapons/Grater/SM_GraterShard",
    "/Game/CheeseStrike/Weapons/Munster/SM_MunsterSMG",
    "/Game/CheeseStrike/Weapons/Shotgun/SM_SwissShotgun",
    "/Game/CheeseStrike/Weapons/Burratas/SM_Burrata",
    "/Game/CheeseStrike/Weapons/Roquefort/SM_RoquefortRifle",
    "/Game/CheeseStrike/Weapons/BabyBomb/SM_BabyBomb",
    "/Game/CheeseStrike/Weapons/BabyBomb/SM_BabyBombRound",
    "/Game/CheeseStrike/Weapons/Stick/SM_CheeseStick",
    "/Game/CheeseStrike/Weapons/Pistol/SM_CurdPistol",
    "/Game/CheeseStrike/Weapons/Knife/SM_CheeseKnife",
    "/Game/CheeseStrike/Weapons/Gatling/SM_GoudaGatling",
    "/Game/CheeseStrike/Weapons/Gatling/SM_GoudaGatlingBarrels",
    "/Game/CheeseStrike/Weapons/Grenade/SM_FragGrenade",
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
    try:
        lines.append(f"  vertices {unreal.EditorStaticMeshLibrary.get_number_verts(mesh, 0)}")
    except Exception as error:
        lines.append(f"  {error}")

out = os.path.join(unreal.Paths.project_saved_dir(), "measure_weapon_models.txt")
with open(out, "w") as f:
    f.write("\n".join(lines) + "\n")
unreal.SystemLibrary.quit_editor()
