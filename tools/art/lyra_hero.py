import unreal, os, sys, traceback

# Run in the Lyra project (C:\Hamid\code\Lyra\LyraStarterGame), with its
# editor closed and the Lyra lock taken (mkdir C:\Hamid\code\cs-wt\lyra.lock):
#   UnrealEditor-Cmd.exe <Lyra>/LyraStarterGame.uproject -ExecCmds="py <repo>/tools/art/lyra_hero.py <Hero> [retarget|weapons]" -unattended -nullrhi
# (paths absolute). <Hero> is a Paragon hero the Epic Games Launcher added to
# the Lyra project: Murdock is /Game/ParagonMurdock/Characters/Heroes/Murdock
# with its mesh Meshes/Murdock. It writes to Saved/lyra_hero_<Hero>.txt:
#
# - what the hero is made of: its mesh's bones (those the reference pose
#   scales, which the retargeted animations must leave alone, and those its
#   own weapons are skinned to), material slots and physics asset;
# - with "retarget": an IK rig (the UE4 mannequin's, whose bones Paragon's
#   heroes have) and a retargeter from Lyra's UE5 mannequin, with the hero's
#   pose aligned to it, and Lyra's rifle set retargeted to the hero in
#   <hero>/Animations/Rifle, as Lt. Belica's was, with the other weapons'
#   sets (WEAPON_SETS) beside it;
# - with "weapons": only the other weapons' sets, through the retargeter
#   "retarget" made, leaving the rig and the rifle set as they are; with
#   kinds of WEAPON_SETS after it ("weapons Grenade"), only those;
# - the packages the game needs (the mesh, its animations, and everything
#   they depend on), one a line after "# packages", for
#   tools/art/copy_from_lyra.py to copy into the game's Content.
HEROES = {
    "Belica": ("/Game/ParagonLtBelica/Characters/Heroes/Belica", "Belica"),
    "Murdock": ("/Game/ParagonMurdock/Characters/Heroes/Murdock", "Murdock"),
    "Wraith": ("/Game/ParagonWraith/Characters/Heroes/Wraith", "Wraith"),
    "Twinblast": ("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast", "TwinBlast"),
    "Revenant": ("/Game/ParagonRevenant/Characters/Heroes/Revenant", "Revenant"),
    "Phase": ("/Game/ParagonPhase/Characters/Heroes/Phase", "Phase_GDC"),
}
M = "/Game/Characters/Heroes/Mannequin"
R = M + "/Animations/Locomotion/Rifle/"
A = M + "/Animations/Actions/"
AO = M + "/Animations/AimOffsets/"
RIFLE_SET = [
    R + "MM_Rifle_Idle_Hipfire", R + "MM_Rifle_Idle_ADS",
    R + "MM_Rifle_Walk_Fwd", R + "MM_Rifle_Walk_Bwd", R + "MM_Rifle_Walk_Left", R + "MM_Rifle_Walk_Right",
    R + "MM_Rifle_Jog_Fwd", R + "MM_Rifle_Jog_Bwd", R + "MM_Rifle_Jog_Left", R + "MM_Rifle_Jog_Right",
    R + "MM_Rifle_Crouch_Idle", R + "BS_MM_Rifle_Crouch_Walk", R + "MM_Rifle_Jump_Fall_Loop",
    AO + "AO_MM_Rifle_Idle_Hipfire", AO + "AO_MM_Rifle_Idle_ADS", AO + "AO_MM_Rifle_Crouch_Idle",
    A + "MM_Rifle_Fire", A + "MM_Rifle_Reload",
]
P = M + "/Animations/Locomotion/Pistol/"
U = M + "/Animations/Locomotion/Unarmed/"
# The rest of what the weapons are held by (UCheeseStrikeAnimInstance::FindSets), by the folder beside the rifle set's
# each goes in: drawing the rifle; the pistol's set, which the knife is held by too, with its blow and the unarmed
# guard; the shotgun's firing and reloading.
WEAPON_SETS = {
    "Rifle": [A + "MM_Rifle_Equip"],
    "Pistol": [
        P + "MM_Pistol_Idle_Hipfire", P + "MM_Pistol_Idle_ADS",
        P + "MM_Pistol_Walk_Fwd", P + "MM_Pistol_Walk_Bwd", P + "MM_Pistol_Walk_Left", P + "MM_Pistol_Walk_Right",
        P + "MM_Pistol_Jog_Fwd", P + "MM_Pistol_Jog_Bwd", P + "MM_Pistol_Jog_Left", P + "MM_Pistol_Jog_Right",
        P + "MM_Pistol_Crouch_Idle",
        P + "MM_Pistol_Crouch_Walk_Fwd", P + "MM_Pistol_Crouch_Walk_Bwd", P + "MM_Pistol_Crouch_Walk_Left", P + "MM_Pistol_Crouch_Walk_Right",
        P + "MM_Pistol_Jump_Fall_Loop",
        AO + "AO_MM_Pistol_Idle_ADS",
        A + "MM_Pistol_Fire", A + "MM_Pistol_Reload", A + "MM_Pistol_Equip", A + "MM_Pistol_Melee",
        U + "MM_Unarmed_Idle_Ready",
    ],
    "Shotgun": [A + "MM_Shotgun_Fire", A + "MM_Shotgun_Reload"],
    # Throwing a grenade, over whatever weapon is in hand.
    "Grenade": [A + "MM_Rifle_GrenadeToss"],
}

args = [a for a in sys.argv[1:] if a]
hero = args[0] if args else "Murdock"
retarget = "retarget" in args[1:]
weapons = retarget or "weapons" in args[1:]
# "weapons Grenade" does only the sets named after it.
only = [a for a in args[1:] if a in WEAPON_SETS]
OUT = unreal.Paths.project_saved_dir() + f"lyra_hero_{hero}.txt"
lines = []
def say(*parts):
    lines.append(" ".join(str(p) for p in parts))

lib = unreal.EditorAssetLibrary
try:
    folder, mesh_name = HEROES[hero]
    mesh = unreal.load_asset(f"{folder}/Meshes/{mesh_name}")
    say("mesh", mesh.get_path_name(), "skeleton", mesh.skeleton.get_path_name())
    say("physics asset", mesh.physics_asset.get_path_name() if mesh.physics_asset else None)
    bounds = mesh.get_bounds()
    say("bounds", bounds.origin, bounds.box_extent)
    for slot in mesh.materials:
        say("slot", slot.material_slot_name, slot.material_interface.get_path_name() if slot.material_interface else None)
    pose = mesh.skeleton.get_reference_pose()
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
    say("bones", len(names), ", ".join(names))
    scaled = []
    for bone in names:
        s = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL).scale3d
        if abs(s.x - 1) > 1e-3 or abs(s.y - 1) > 1e-3 or abs(s.z - 1) > 1e-3:
            scaled.append(f"{bone} {s.x:.2f}")
    say("scaled in the reference pose:", ", ".join(scaled))
    for bone in ["root", "pelvis", "head", "hand_r", "hand_l", "weapon_r", "weapon", "weapon_l", "pistol", "eyeball_l", "L_eye"]:
        if bone in names:
            say("bone", bone, unreal.AnimPoseExtensions.get_ref_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD))

    rig_folder = folder + "/Rig"
    animations = folder + "/Animations"
    anims = animations + "/Rifle"
    manny = unreal.load_asset(M + "/Meshes/SKM_Manny")
    if retarget:
        rig_path = f"{rig_folder}/IK_{hero}"
        if lib.does_asset_exist(rig_path):
            lib.delete_asset(rig_path)
        rig = lib.duplicate_asset("/Game/Characters/Heroes/Mannequin_UE4/Meshes/IK_UE4_Mannequin", rig_path)
        rig_ctrl = unreal.IKRigController.get_controller(rig)
        say("rig compatible", rig_ctrl.is_skeletal_mesh_compatible(mesh))
        say("rig mesh set", rig_ctrl.set_skeletal_mesh(mesh))
        rtg_path = f"{rig_folder}/RTG_Manny_{hero}"
        if lib.does_asset_exist(rtg_path):
            lib.delete_asset(rtg_path)
        rtg = lib.duplicate_asset("/Game/Characters/Heroes/Mannequin_UE4/Meshes/RTG_UE5Manny_UE4Manny", rtg_path)
        rc = unreal.IKRetargeterController.get_controller(rtg)
        target = unreal.RetargetSourceOrTarget.TARGET
        rc.set_ik_rig(target, rig)
        rc.set_preview_mesh(target, mesh)
        try:
            rc.assign_ik_rig_to_all_ops(target, rig)
        except Exception as e:
            say("assign_ik_rig_to_all_ops", e)
        pose_name = rc.create_retarget_pose(f"{hero} Aligned", target)
        rc.set_current_retarget_pose(pose_name, target)
        rc.auto_align_all_bones(target, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
        try:
            rc.snap_bone_to_ground("ball_l", target)
        except Exception as e:
            say("snap failed", e)
        lib.save_loaded_asset(rig)
        lib.save_loaded_asset(rtg)
        sources = [lib.find_asset_data(p) for p in RIFLE_SET]
        if lib.does_directory_exist(anims):
            lib.delete_directory(anims)
        lib.make_directory(anims)
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(assets_to_retarget=sources, source_mesh=manny, target_mesh=mesh, ik_retarget_asset=rtg,
                                                                     search="", replace="", prefix="", suffix="", target_path=anims, include_referenced_assets=True)
        say("retargeted", len(made))
        lib.save_directory(anims)
    if weapons:
        rtg = unreal.load_asset(f"{rig_folder}/RTG_Manny_{hero}")
        for kind, paths in WEAPON_SETS.items():
            if only and kind not in only:
                continue
            into = f"{animations}/{kind}"
            if kind == "Rifle":
                # Beside the rifle set, which stays: only what this adds goes first.
                for path in paths:
                    old = f"{into}/{path.split('/')[-1]}"
                    if lib.does_asset_exist(old):
                        lib.delete_asset(old)
            elif lib.does_directory_exist(into):
                lib.delete_directory(into)
            lib.make_directory(into)
            sources = [lib.find_asset_data(p) for p in paths]
            missing = [p for p, d in zip(paths, sources) if not d.is_valid()]
            if missing:
                say("missing in Lyra", ", ".join(missing))
            made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(assets_to_retarget=[d for d in sources if d.is_valid()], source_mesh=manny, target_mesh=mesh,
                                                                         ik_retarget_asset=rtg, search="", replace="", prefix="", suffix="", target_path=into,
                                                                         include_referenced_assets=True)
            say("retargeted", kind, len(made))
            lib.save_directory(into)

    # What the game needs: the mesh and its animations, with what they depend on.
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game"], True)
    roots = [mesh.get_outermost().get_name()]
    # (only the sets made here: the hero's own Paragon animations beside them stay behind)
    for kind in ["Rifle"] + [k for k in WEAPON_SETS if k != "Rifle"]:
        roots += [str(a.package_name) for a in registry.get_assets_by_path(f"{animations}/{kind}", True)]
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                                     include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
    seen = set()
    order = []
    stack = list(roots)
    while stack:
        package = stack.pop()
        if package in seen or not package.startswith("/Game/"):
            continue
        seen.add(package)
        assets = registry.get_assets_by_package_name(package)
        if not assets:
            say("missing", package)
            continue
        order.append((package, ",".join(sorted({str(a.asset_class_path.asset_name) for a in assets}))))
        stack.extend(str(d) for d in (registry.get_dependencies(package, options) or []))
    say("# packages")
    for package, kinds in sorted(order):
        say(f"{package}\t{kinds}")
except Exception:
    say(traceback.format_exc())
finally:
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
