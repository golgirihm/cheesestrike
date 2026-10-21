import unreal, sys, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/ragdolls.py [<Hero>]" -unattended -nullrhi
# (both paths absolute), for one hero (HEROES) or, with none named, all of
# them. It writes what it did to Saved/ragdolls.txt.
#
# Gives each Paragon hero a ragdoll that falls as a body does, and makes it
# the hero's mesh's physics asset. Paragon's meshes come with a few rough
# bodies for their bounds (Belica_Extents and the like), which as a ragdoll
# sag and float. The editor's Create Physics Asset made one for Belica with
# bodies wherever her bones were long enough, which was a muddle: none for
# her left upper arm or her neck, bodies for twist bones, a collar piece and
# her own (hidden) guns, and 45 degrees of soft give every way at every
# joint, so that a falling body twisted its spine round on itself, folded its
# neck and bent its knees and elbows sideways. So this makes the bodies a
# ragdoll needs (BODIES), each a capsule fitted to the vertices on its bone
# and the bones under it, none under the head (with bodies of their own, the
# face and jaw swing loose as the body falls, and the face melts) and none
# fitted to the hero's own weapons; limits every joint as a person's is (the
# table below), on hard limits; damps every body, and has every joint resist
# being turned fast, so that the body settles rather than wobbles; and lets
# the legs collide with each other, so that they can't pass through each
# other.
OUT = unreal.Paths.project_saved_dir() + "ragdolls.txt"
# Each hero: its mesh, and the bones its own weapons are skinned to, which no
# body is fitted to (they are hidden in the game). The ragdoll is saved beside
# the mesh as PA_<Hero>_Ragdoll.
HEROES = {
    "Belica": ("/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica", ["pistol", "weapon"]),
    "Murdock": ("/Game/ParagonMurdock/Characters/Heroes/Murdock/Meshes/Murdock", ["gun", "shotgun_base", "sniper_can_arm_01", "trap_handle"]),
    "Wraith": ("/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith", ["weapon_r", "weapon_l"]),
    "Twinblast": ("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast",
                  ["weapon_r", "weapon_l", "q_weapon_r", "q_weapon_l", "ult_weapon_r", "ult_weapon_l", "ult_root", "grenade"]),
}

# Directions in the mesh's space as its reference pose stands: it faces +Y,
# with +Z up.
FORWARD = unreal.Vector(0, 1, 0)
BACK = unreal.Vector(0, -1, 0)
UP = unreal.Vector(0, 0, 1)

# The bodies, the same bones on every hero.
BODIES = ["pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head",
          "upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r",
          "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r"]

# Each joint, by the bone of the body below it ("_l" ones are mirrored to
# "_r"): which way it bends, how far (degrees, the other way negative, from
# the reference pose or, for a hinge, from straight), how far it leans to the
# side of that either way, how far it twists about the bone either way, and
# whether its bend is measured from straight.
JOINTS = [
    # The spine, a little at each of its joints: in all, some 50 degrees
    # forward, 30 back, 30 to each side and 25 of twist.
    ("spine_01", FORWARD, -10, 18, 10, 8, False),
    ("spine_02", FORWARD, -10, 18, 10, 8, False),
    ("spine_03", FORWARD, -10, 18, 10, 8, False),
    # The neck and head: 45 degrees down, 40 up, 30 to the side, 45 of turn.
    ("neck_01", FORWARD, -20, 20, 15, 20, False),
    ("head", FORWARD, -20, 25, 15, 25, False),
    # The shoulder swings forward far, back less, out and in, and turns.
    ("upperarm_l", FORWARD, -40, 95, 50, 40, False),
    # The elbow is a hinge: straight to fully bent, the hand coming forward.
    ("lowerarm_l", FORWARD, -5, 140, 4, 30, True),
    ("hand_l", FORWARD, -45, 45, 25, 10, False),
    # The hip swings forward far, back a little, out and in, and turns.
    ("thigh_l", FORWARD, -20, 100, 30, 25, False),
    # The knee is a hinge: straight to fully bent, the foot going back.
    ("calf_l", BACK, -2, 140, 3, 5, True),
    # The ankle: toes down 35, up 20; it rocks a little to the side.
    ("foot_l", UP, -35, 20, 15, 5, False),
]
LEFT_LEG = ["thigh_l", "calf_l", "foot_l"]
RIGHT_LEG = ["thigh_r", "calf_r", "foot_r"]

lines = []
try:
    tools = unreal.CheeseStrikeArtTools
    table = {}
    for bone, toward, bend_from, bend_to, side, twist, straight in JOINTS:
        table[bone] = (toward, bend_from, bend_to, side, twist, straight)
        if bone.endswith("_l"):
            table[bone[:-2] + "_r"] = (toward, bend_from, bend_to, side, twist, straight)
    named = [a for a in sys.argv[1:] if a]
    for hero in (named or list(HEROES)):
        mesh_path, leave = HEROES[hero]
        ragdoll_path = mesh_path.rsplit("/", 1)[0] + f"/PA_{hero}_Ragdoll"
        lines.append(f"== {hero}")
        mesh = unreal.load_asset(mesh_path)
        had = mesh.get_editor_property("physics_asset")
        had = had.get_path_name() if had else None
        if unreal.EditorAssetLibrary.does_asset_exist(ragdoll_path):
            unreal.EditorAssetLibrary.delete_asset(ragdoll_path)
        ragdoll = tools.make_ragdoll(mesh, ragdoll_path, BODIES, leave)
        bodies = [str(b) for b in tools.ragdoll_bodies(ragdoll)]
        lines.append("bodies: " + ", ".join(bodies))
        for bone in bodies[1:]:
            if bone not in table:
                raise RuntimeError(f"no limits for the joint above {bone}")
            if not tools.limit_ragdoll_joint(ragdoll, mesh, bone, *table[bone]):
                raise RuntimeError(f"no joint above {bone}")
        tools.settle_ragdoll(ragdoll, 0.1, 1.0, 5.0, 16, 4)
        for pair in tools.let_ragdoll_bodies_collide(ragdoll, mesh, LEFT_LEG, RIGHT_LEG, 2.0):
            lines.append("collide " + pair)
        lines.extend(str(line) for line in tools.describe_ragdoll(ragdoll, mesh))
        lines.append("saved ragdoll " + str(unreal.EditorAssetLibrary.save_asset(ragdoll_path, False)))
        # The mesh names its physics asset by path: saved again (many MB more
        # for Git LFS) only if that changed.
        if had != ragdoll.get_path_name():
            lines.append("saved mesh " + str(unreal.EditorAssetLibrary.save_asset(mesh_path, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
