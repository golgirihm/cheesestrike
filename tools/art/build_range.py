import os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, painted

# Run with the editor closed, after what The Creamery needs (build_materials.py):
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_range.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder). It writes what it built to Saved/Lvl_Range.txt.
#
# Builds the Practice Range (/Game/CheeseStrike/Maps/Lvl_Range) from nothing: a flat walled yard, about 74 by 36 m,
# for trying the weapons alone (the front end's PRACTICE; docs/unreal/maps/range.md). x runs from the yard (west) to
# the far end of the range (east), y south to north. The firing line is at x = 0; behind it, a row of a stand for every
# weapon that lies on one (instant: taken, it is back at once), and behind them the starts. Ahead of the line: markers
# at 5, 10, 20 and 40 m, a wall of cheese targets that shots break and that grow back, and four dummies (bots that
# never move, aim or fire) standing at the dummy spots, coming back there when killed. There is no cheese for the
# mice: no mice come. The stands and starts are fair to both sides (a side's starts either side of the middle line),
# so that the range also plays as a map in the lobby, small and open.

LEVEL = "/Game/CheeseStrike/Maps/Lvl_Range"

WEST, EAST = -2200, 5200    # the yard's back wall and the range's far end
SOUTH, NORTH = -1800, 1800
WALL = 400                  # the walls round it
LINE = 0                    # the firing line
ROW = -400                  # the stands' row, behind it
# The stands, south to north: the close-range weapons at the ends, the power weapons in the middle (the most contested).
STANDS = ("grater", "blaster", "roquefort", "babybomb", "stick", "munster", "burratas", "shotgun")
MARKERS = (5, 10, 20, 40)   # metres from the line
# The dummies: at each marker's distance, out of each other's way and out of the way of the shots at the target wall.
DUMMIES = (("5m", 500, -650), ("10m", 1000, 250), ("20m", 2000, -500), ("40m", 4000, 400))
# The target wall: at 15 m, on the north side.
TARGETS_X = 1500
TARGETS_Y = (950, 1110, 1270, 1430)
TARGETS_Z = (70, 150, 230)


def yard():
    kit.ground("Ground", (WEST, SOUTH), (EAST, NORTH))
    kit.boundary(WEST, SOUTH, EAST, NORTH, WALL)
    # A tall backstop at the far end, where the shots go.
    box("Backstop", "Range", (EAST - 200, SOUTH, 0), (EAST, NORTH, 900), kit.sheet("Dark"))


def firing_line():
    f = "FiringLine"
    box("FiringLine", f, (LINE - 10, SOUTH + 200, 0), (LINE + 10, NORTH - 200, 1), painted("Yellow"))
    for s in (-1, 1):
        kit.frame_post(f"FiringLine_Post_{s}", f, LINE, s * (NORTH - 150), 0, 140, size=20, colour="Yellow")
    kit.text_sign("FiringLine_Sign", f, "FIRING LINE", (LINE - 12, NORTH - 150, 175), 180, size=40, colour=(255, 214, 80))


def markers():
    f = "Markers"
    for metres in MARKERS:
        x = LINE + metres * 100
        box(f"Marker_{metres}", f, (x - 5, SOUTH + 200, 0), (x + 5, NORTH - 200, 1), painted("Cream"))
        for s in (-1, 1):
            y = s * (NORTH - 150)
            kit.frame_post(f"Marker_{metres}_Post_{s}", f, x, y, 0, 160, size=16)
            box(f"Marker_{metres}_Board_{s}", f, (x - 4, y - 70, 160), (x + 4, y + 70, 260), painted("Dark"))
            # Read from the line: the board's west face, toward it.
            kit.text_sign(f"Marker_{metres}_Sign_{s}", f, f"{metres} m", (x - 6, y, 210), 180, size=70, colour=(255, 214, 80))


def armoury():
    for i, weapon in enumerate(STANDS):
        y = (i - (len(STANDS) - 1) / 2.0) * 300
        kit.pickup(weapon, ROW, y, 0, "Row", instant=True)


def target_wall():
    f = "TargetWall"
    y0, y1 = TARGETS_Y[0] - 110, TARGETS_Y[-1] + 110
    for y in (y0, y1):
        kit.frame_post(f"TargetWall_Post_{int(y)}", f, TARGETS_X + 30, y, 0, 300, size=20)
    box("TargetWall_Top", f, (TARGETS_X + 20, y0, 290), (TARGETS_X + 40, y1, 305), painted("Dark"))
    # A dark board behind them, where the shots that miss leave their marks.
    box("TargetWall_Board", f, (TARGETS_X + 45, y0, 20), (TARGETS_X + 60, y1, 290), painted("Dark"))
    for i, y in enumerate(TARGETS_Y):
        for j, z in enumerate(TARGETS_Z):
            kit.target(f"Target_{i}_{j}", f, TARGETS_X, y, z, yaw=180)


def dummies():
    for name, x, y in DUMMIES:
        kit.dummy_spot(name, x, y, 0, yaw=180)


def starts():
    # Behind the stands, a side's either side of the middle line: every stand as near to one side as to the other.
    xs = [-900 - 150 * k for k in range(8)]
    kit.team_starts("Gouda", [(x, -100) for x in xs], yaw=0)
    kit.team_starts("Mozza", [(x, 100) for x in xs], yaw=0)


def places():
    kit.place_name("Yard", (WEST, SOUTH, -50), (-650, NORTH, 600))
    kit.place_name("Armoury", (-650, SOUTH, -50), (-150, NORTH, 600))
    kit.place_name("Firing Line", (-150, SOUTH, -50), (150, NORTH, 600))
    kit.place_name("Range", (150, SOUTH, -50), (EAST, NORTH, 900))
    kit.place_name("Target Wall", (TARGETS_X - 300, TARGETS_Y[0] - 200, -50), (TARGETS_X + 200, NORTH, 600))


def build():
    yard()
    firing_line()
    markers()
    armoury()
    target_wall()
    dummies()
    starts()
    places()
    # On the middle line, where both sides' starts are as near: in front of the line, in the field and at the far end.
    kit.hill_spots([("Near", 700, 0), ("Field", 2800, 0), ("Far", 4400, 0)])
    kit.nav_bounds((WEST - 100, SOUTH - 100, -100), (EAST + 100, NORTH + 100, 600))
    # The sun behind the line, so that nobody shoots into it.
    kit.light_rig("day", sun=(-40, 20))


kit.build_level(LEVEL, build)
