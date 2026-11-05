import os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, painted

# Run with the editor closed, after what The Creamery needs (build_materials.py, cheese_material.py, wax_material.py):
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_proving.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder). It writes what it built to Saved/Lvl_ProvingGround.txt.
#
# Builds the Proving Ground (/Game/CheeseStrike/Maps/Lvl_ProvingGround) from nothing: a basic, symmetric arena, 60 by
# 40 m inside its walls, for learning the game and trying it (docs/unreal/maps/proving.md). A flat concrete floor and an
# outer wall; each side's eight starts behind a screen of waxed cheese at its end (never eaten, so that the open arena
# has no line into a spawn); between them, rounded cheese to fight round, all of it eaten: two walls either side of the
# middle, four stacks and a pair of low walls at the middle, crouching blocks along the sides; two low platforms of
# waxed cheese north and south of the middle, stepped up from the west and the east; four weapon stands (the power
# weapons on the middle line, a Munster nearer each side); three hill spots on the middle line (the middle, the two
# platforms); the mice over the north and south walls. x runs west (the Gouda Guys) to east (the Mozza Radicals),
# y south to north; the map is mirrored across x = 0.

LEVEL = "/Game/CheeseStrike/Maps/Lvl_ProvingGround"

HALF_X, HALF_Y = 3000, 2000     # inside the walls
WALL = 500                      # the walls' height
START_X = 2700                  # the starts, at the ends
SCREEN_X = 2200                 # the waxed screen in front of each side's starts
PLATFORM_Y = 1100               # the platforms' middles, north and south of the middle
PLATFORM = (700, 700, 100)      # a platform's top: x, y and height
STEPS = 3                       # its steps, west and east: 25 cm risers, which the bots' navigation mesh steps (35 at most)
COURSE = 100                    # the cover's blocks are a metre high


def arena():
    kit.ground("Ground", (-HALF_X, -HALF_Y), (HALF_X, HALF_Y))
    kit.boundary(-HALF_X, -HALF_Y, HALF_X, HALF_Y, WALL)
    # A yellow line down the middle, for the eye: nothing to stand on.
    box("MiddleLine", "Arena", (-5, -HALF_Y, 0), (5, HALF_Y, 1), painted("Yellow"))


def screens(s, side):
    """Waxed cheese across the end in front of the starts, 2 m high, a gap in its middle and open ends, so that every
    way out of a spawn turns a corner. Never eaten: the arena after the mice has no line into a spawn either."""
    team = "Gouda" if s < 0 else "Mozza"
    x = s * SCREEN_X
    for name, (y0, y1) in (("South", (-1600, -100)), ("North", (100, 1600))):
        kit.cheese_wall(f"Screen{team}{name}", f"Screen{team}", (x, y0), (x, y1), 0, (COURSE, COURSE), thickness=50, column=250, edible=False)


def cover(s, side):
    """Rounded cheese to fight round, all of it eaten: a wall of two courses north and south of each side's lane, a
    2 m stack at each corner of the middle, two low walls at the middle and crouching blocks along the side walls."""
    team = "Gouda" if s < 0 else "Mozza"
    structure = f"Cover{team}"
    x = s * 1200
    kit.cheese_wall(f"Wall{team}South", structure, (x, -1500), (x, -500), 0, (COURSE, COURSE), thickness=50, column=250)
    kit.cheese_wall(f"Wall{team}North", structure, (x, 500), (x, 1500), 0, (COURSE, COURSE), thickness=50, column=250)
    for name, y in (("South", -500), ("North", 500)):
        x0, x1 = sorted((s * 500, s * 700))
        kit.cheese_stack(f"Stack{team}{name}", structure, (x0, y - 100, 0), (x1, y + 100, 2 * COURSE), 2, 2, 2)
    # Crouching cover along the side walls, a block from each wall.
    for name, y in (("South", -HALF_Y + 300), ("North", HALF_Y - 300)):
        x0, x1 = sorted((s * 1900, s * 2100))
        kit.cheese_stack(f"Crouch{team}{name}", structure, (x0, y - 50, 0), (x1, y + 50, COURSE), 2, 1, 1)
    # At the middle, a low wall of cheese either side of the middle line, 3 m long and two blocks high.
    x0, x1 = sorted((s * 425, s * 475))
    kit.cheese_stack(f"Middle{team}", "Middle", (x0, -300, 0), (x1, 300, 2 * COURSE), 1, 3, 2)


def platforms():
    """Two low platforms of waxed cheese, a metre up, north and south of the middle, stepped up from the west and the
    east by three steps a metre deep (risers of 25 cm, which the bots walk): high ground to hold, never eaten."""
    for name, y in (("South", -PLATFORM_Y), ("North", PLATFORM_Y)):
        structure = f"Platform{name}"
        w, d, h = PLATFORM
        kit.cheese_stack(f"{structure}Top", structure, (-w / 2.0, y - d / 2.0, 0), (w / 2.0, y + d / 2.0, h), 2, 2, 1, edible=False)
        for s in (-1, 1):
            for k in range(STEPS):
                x0, x1 = sorted((s * (w / 2.0 + 100 * k), s * (w / 2.0 + 100 * (k + 1))))
                rise = h * (STEPS - k) / (STEPS + 1)
                kit.cheese_box(f"Cheese_{structure}Step_{'West' if s < 0 else 'East'}_{k}", structure, (x0, y - d / 2.0, 0), (x1, y + d / 2.0, rise), edible=False)


def stands():
    """Four stands: the power weapons on the middle line, north and south of the platforms, and a Munster on each
    side's lane; the middle is the hill's."""
    kit.pickup("babybomb", 0, HALF_Y - 250, 0, "North")
    kit.pickup("stick", 0, -HALF_Y + 250, 0, "South")
    for s, side in ((-1, "West"), (1, "East")):
        kit.pickup("munster", s * 1600, 0, 0, side)


def starts():
    ys = [-1400 + 400 * k for k in range(8)]
    kit.team_starts("Gouda", [(-START_X, y) for y in ys], yaw=0)
    kit.team_starts("Mozza", [(START_X, y) for y in ys], yaw=180)
    # Free-for-all's: through the lanes and the middle, facing in.
    kit.free_starts([(-1700, -1800, 90), (1700, -1800, 90), (-1700, 1800, -90), (1700, 1800, -90), (-900, 0, 0), (900, 0, 180)])


def places():
    kit.place_name("Gouda End", (-HALF_X, -HALF_Y, -50), (-SCREEN_X, HALF_Y, WALL))
    kit.place_name("Mozza End", (SCREEN_X, -HALF_Y, -50), (HALF_X, HALF_Y, WALL))
    kit.place_name("Gouda Lane", (-SCREEN_X, -HALF_Y, -50), (-800, HALF_Y, WALL))
    kit.place_name("Mozza Lane", (800, -HALF_Y, -50), (SCREEN_X, HALF_Y, WALL))
    kit.place_name("The Middle", (-800, -HALF_Y, -50), (800, HALF_Y, WALL))
    w, d, h = PLATFORM
    kit.place_name("North Platform", (-w / 2.0 - 100, PLATFORM_Y - d / 2.0, -50), (w / 2.0 + 100, PLATFORM_Y + d / 2.0, WALL))
    kit.place_name("South Platform", (-w / 2.0 - 100, -PLATFORM_Y - d / 2.0, -50), (w / 2.0 + 100, -PLATFORM_Y + d / 2.0, WALL))


def build():
    arena()
    kit.mirrored(screens)
    kit.mirrored(cover)
    platforms()
    stands()
    starts()
    places()
    # On the middle line, as far from both sides: the middle first, then the platforms.
    kit.hill_spots([("Middle", 0, 0), ("NorthPlatform", 0, PLATFORM_Y, PLATFORM[2]), ("SouthPlatform", 0, -PLATFORM_Y, PLATFORM[2])])
    # The mice come over the north and south walls, never over a spawn.
    kit.mouse_entries(-HALF_X, -HALF_Y, HALF_X, HALF_Y, WALL, xs=(-1500, -500, 500, 1500))
    kit.nav_bounds((-HALF_X - 100, -HALF_Y - 100, -100), (HALF_X + 100, HALF_Y + 100, 600))
    # The sun from the south, across the lanes, in neither side's eyes.
    kit.light_rig("noon", sun=(-55, -90))


kit.build_level(LEVEL, build)
