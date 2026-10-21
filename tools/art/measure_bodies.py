import unreal, traceback

# Run in the game's project, with its editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/measure_bodies.py" -unattended -nullrhi
# Writes to Saved/measure_bodies.txt, for each hero standing in its rifle
# idle (the first frame of its retargeted MM_Rifle_Idle_Hipfire), where its
# right hand and the middle of its eyes are in the mesh's space, and the hand
# from the eyes: what the first-person view frames. And the grip for the
# table in CheeseStrikeBodies: where the rifle sits in the hero's hand_r, as
# the mannequin holds it in its weapon_r (turned a quarter turn, as
# ACheeseStrikeWeapon::PutInHand has it) in the same frame of its own idle:
# turned as the mannequin's hand turns it, and as far from the hand as the
# mannequin's weapon_r is from its own (a couple of centimetres; the two
# hands are in different places, the hero's arms being longer or shorter).
HEROES = {
    "Belica": "/Game/ParagonLtBelica/Characters/Heroes/Belica",
    "Murdock": "/Game/ParagonMurdock/Characters/Heroes/Murdock",
    "Wraith": "/Game/ParagonWraith/Characters/Heroes/Wraith",
    "Twinblast": "/Game/ParagonTwinblast/Characters/Heroes/TwinBlast",
}
OUT = unreal.Paths.project_saved_dir() + "measure_bodies.txt"
lines = []
try:
    space = unreal.AnimPoseSpaces.WORLD
    manny = unreal.load_asset("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_Hipfire")
    manny_pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(manny, 0.0, unreal.AnimPoseEvaluationOptions())
    weapon = unreal.AnimPoseExtensions.get_bone_pose(manny_pose, "weapon_r", space)
    rifle = unreal.MathLibrary.compose_transforms(unreal.Transform(rotation=unreal.Rotator(0.0, 0.0, -90.0)), weapon)
    manny_hand = unreal.AnimPoseExtensions.get_bone_pose(manny_pose, "hand_r", space)
    rifle_in_hand = unreal.MathLibrary.make_relative_transform(rifle, manny_hand)
    for hero, folder in HEROES.items():
        anim = unreal.load_asset(folder + "/Animations/Rifle/MM_Rifle_Idle_Hipfire")
        options = unreal.AnimPoseEvaluationOptions()
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, 0.0, options)
        names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        left, right = ("eyeball_l", "eyeball_r") if "eyeball_l" in names else ("L_eye", "R_eye")
        def at(bone):
            return unreal.AnimPoseExtensions.get_bone_pose(pose, bone, space).translation
        eye_l, eye_r = at(left), at(right)
        eyes = unreal.Vector((eye_l.x + eye_r.x) / 2, (eye_l.y + eye_r.y) / 2, (eye_l.z + eye_r.z) / 2)
        hand = at("hand_r")
        hand_l = at("hand_l")
        head = at("head")
        hand_turn = unreal.MathLibrary.make_relative_transform(manny_hand, unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_r", space))
        hand_turn = unreal.Transform(rotation=hand_turn.rotation.rotator())
        grip = unreal.MathLibrary.compose_transforms(rifle_in_hand, hand_turn)
        turn = grip.rotation.rotator()
        lines.append(f"{hero} grip: FTransform(FRotator({turn.pitch:.1f}f, {turn.yaw:.1f}f, {turn.roll:.1f}f), FVector({grip.translation.x:.1f}f, {grip.translation.y:.1f}f, {grip.translation.z:.1f}f))")
        lines.append(f"{hero}: eyes ({eyes.x:.1f}, {eyes.y:.1f}, {eyes.z:.1f}) head ({head.x:.1f}, {head.y:.1f}, {head.z:.1f}) hand_r ({hand.x:.1f}, {hand.y:.1f}, {hand.z:.1f}) hand_l ({hand_l.x:.1f}, {hand_l.y:.1f}, {hand_l.z:.1f})"
                     f" hand_r from eyes ({hand.x - eyes.x:.1f}, {hand.y - eyes.y:.1f}, {hand.z - eyes.z:.1f}) hand_l from eyes ({hand_l.x - eyes.x:.1f}, {hand_l.y - eyes.y:.1f}, {hand_l.z - eyes.z:.1f})")
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
