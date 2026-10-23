import os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, cylinder, prop, ramp, place, painted, sheet

# Run with the editor closed, after build_materials.py, cheese_material.py, wax_material.py and fondue_material.py:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_fondue.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it built to Saved/Lvl_FondueFloor.txt.
#
# Builds The Fondue Floor (/Game/CheeseStrike/Maps/Lvl_FondueFloor) from scratch, with the blocks in map_kit.py: a
# vat hall 72 by 40 m whose whole floor is cheese over a vat of molten fondue. docs/unreal/maps/fondue.md says how it
# plays and what the mice change. The floor is a grid of 2 m tiles, each a pillar of cheese standing on the vat's
# floor 6 m down, its top at the floor and the fondue's surface 2.5 m below that: nothing else holds a tile up, so a
# tile eaten is a hole straight down into the fondue, and nothing is left to walk on there. The tiles the mice never
# eat (red wax) make the paths that join the spawn decks at each end to each other and to the islands. Falling in
# kills (the level's kill height). The west half is laid out in GRID and mirrored for the east (x to -x); x runs west
# to east and y south to north, in centimetres, with the floor at 0.

LEVEL = "/Game/CheeseStrike/Maps/Lvl_FondueFloor"

TILE = 200
COLS, ROWS = 36, 20
X0, Y0 = -3600, -2000
X1, Y1 = 3600, 2000
FONDUE = -250     # the fondue's surface
KILL = -160       # the kill height: a player whose middle is there has their feet in the fondue
VAT = -600        # the vat's floor, which the tiles stand on, out of sight under the fondue
DECK = 120        # the spawn decks' top
GANTRY = 320      # the gantry's deck, over the middle
HALL_TOP = 1200   # the hall's walls; its roof is on them
VENT_Z = 220      # the vents the mice come in by, low in the walls

# The west half of the floor, a 2 m cell to a character, the north row first and the west wall on the left; the east
# half is its mirror. Cell (c, r) has its south-west corner at (X0 + 200c, Y0 + 200r).
#   .  a tile of cheese the mice eat          w  a tile of cheese in wax: a path, never eaten
#   D  a spawn deck (concrete, 1.2 m up)      R  a ramp down from a deck
#   P  a pump island (concrete)               B  a boiler island         S  the Stirrer's island
#   -  a plank of waxed cheese, 1.4 m wide, over the moat                  (space) open fondue: the moat
#   G  the gantry's stairs, steel over the fondue
GRID = [
    "..................",  # 19, along the north wall
    "..............wwww",  # 18  the Skim: the north path, to the gantry's stairs and on to the other side
    ".............ww..G",  # 17
    "..........PPPw...G",  # 16
    "........wwPPP....G",  # 15
    "DDDDRRwww.PPP.....",  # 14  the north ramp, and the path to the pump
    "DDDDRR............",  # 13
    "DDDD...........   ",  # 12  the moat round the Stirrer
    "DDDD....wwww... SS",  # 11
    "DDDDRRwww..w... SS",  # 10  the middle ramp, and the path that zig-zags to the Stirrer
    "DDDDRR.....wwww-SS",  # 9
    "DDDD........... SS",  # 8
    "DDDD...........   ",  # 7
    "DDDDRR............",  # 6   the south ramp
    "DDDDRRww..........",  # 5
    ".......wBBB......G",  # 4
    "........BBBwwwww.G",  # 3   the Rind Run: a tunnel of cheese over the path east of the boiler
    "........BBB....w.G",  # 2
    "...............www",  # 1   the Drain: the south path, to the gantry's stairs and on
    "..................",  # 0, along the south wall
]


def kind(c, r):
    row = GRID[ROWS - 1 - r]
    return row[c] if c < COLS // 2 else row[COLS - 1 - c]


def cell(c, r):
    return X0 + TILE * c, Y0 + TILE * r, X0 + TILE * (c + 1), Y0 + TILE * (r + 1)


def centre(c, r):
    return X0 + TILE * c + TILE / 2.0, Y0 + TILE * r + TILE / 2.0


def team_of(s):
    return "Gouda" if s < 0 else "Mozza"


def col(c, s):
    """Column `c` counted from the west wall on the west side (s = -1), mirrored on the east (s = 1)."""
    return c if s < 0 else COLS - 1 - c


def cells_of(letter):
    return [(c, r) for c in range(COLS) for r in range(ROWS) if kind(c, r) == letter]


def bounds_of(cells):
    x0 = min(cell(c, r)[0] for c, r in cells)
    y0 = min(cell(c, r)[1] for c, r in cells)
    x1 = max(cell(c, r)[2] for c, r in cells)
    y1 = max(cell(c, r)[3] for c, r in cells)
    return x0, y0, x1, y1


def islands(letter):
    """The cells of each `letter` island, west and east."""
    west = [(c, r) for c, r in cells_of(letter) if c < COLS // 2]
    east = [(c, r) for c, r in cells_of(letter) if c >= COLS // 2]
    return [(-1, bounds_of(west)), (1, bounds_of(east))] if east else [(0, bounds_of(west))]


# ---------------------------------------------------------------- the vat, the fondue and the floor

def vat():
    # The vat's floor, out of sight under the fondue: the tiles stand on it, and what falls in comes to rest on it.
    box("Vat_Floor", "Ground", (X0, Y0, VAT - 100), (X1, Y1, VAT), kit.concrete())
    # The fondue: molten cheese that glows and bubbles, which blocks nothing, so whoever goes in sinks.
    kit.liquid("Fondue", "Vat", (X0, Y0, FONDUE - 40), (X1, Y1, FONDUE), kit.material("M_Fondue"))
    kit.kill_below(KILL)
    # The hall's walls, down into the vat; the mice come in by vents in the long ones.
    kit.boundary(X0, Y0, X1, Y1, HALL_TOP, material=sheet("Grey"), bottom=VAT - 100)


SEAM = 2  # each tile is this much short of its cell all round: the floor shows its tiles


def floor():
    """Every tile of the floor: a pillar of cheese from the vat's floor to the floor, edible or in wax."""
    for c in range(COLS):
        for r in range(ROWS):
            k = kind(c, r)
            x0, y0, x1, y1 = cell(c, r)
            x0, y0, x1, y1 = x0 + SEAM, y0 + SEAM, x1 - SEAM, y1 - SEAM
            team = "Gouda" if c < COLS // 2 else "Mozza"
            if k == ".":
                kit.cheese_box(f"Cheese_Floor_{c}_{r}", f"Floor{team}", (x0, y0, VAT), (x1, y1, 0))
            elif k == "w":
                kit.cheese_box(f"Cheese_Path_{c}_{r}", "WaxPaths", (x0, y0, VAT), (x1, y1, 0), edible=False)
            elif k == "-":
                # A plank over the moat, the way onto the Stirrer: a beam of waxed cheese 1.4 m wide (the bots and mice
                # need a metre and more).
                ym = (y0 + y1) / 2.0
                kit.cheese_box(f"Cheese_Plank_{c}_{r}", "WaxPaths", (x0, ym - 70, VAT), (x1, ym + 70, 0), edible=False)


def decks():
    """A spawn deck at each end: concrete 1.2 m up, walled on three sides, with three ramps down to the floor, and hard
    cover set back from the top of each, so that no line runs into a spawn even once the floor is gone."""
    for s, (x0, y0, x1, y1) in islands("D"):
        team = team_of(s)
        f = f"Decks/{team}"
        box(f"Deck_{team}", f, (x0, y0, VAT), (x1, y1, DECK), kit.concrete())
        box(f"Deck_{team}_Top", f, (x0, y0, DECK - 2), (x1, y1, DECK), kit.floor())
        front = x1 if s < 0 else x0
        inward = 1 if s < 0 else -1  # toward the middle
        box(f"Deck_{team}_Edge", f, (min(front, front - inward * 15), y0, DECK), (max(front, front - inward * 15), y1, DECK + 3), painted("Yellow"))
        # The ramps, each 4 m wide, from the deck's front edge down to the floor.
        ramps = sorted({r for c, r in cells_of("R") if (c < COLS // 2) == (s < 0)})
        spans = []
        for r in ramps:
            if spans and spans[-1][1] == r - 1:
                spans[-1][1] = r
            else:
                spans.append([r, r])
        for r0, r1 in spans:
            ym = (Y0 + TILE * r0 + Y0 + TILE * (r1 + 1)) / 2.0
            width = TILE * (r1 - r0 + 1)
            foot = front + inward * 2 * TILE
            ramp(f"Deck_{team}_Ramp_{r0}", f, (front, ym, DECK), (foot, ym, 0), width, kit.floor(), thickness=150)
            # Under it, down to the vat.
            box(f"Deck_{team}_RampPier_{r0}", f, (min(front, foot), ym - width / 2.0, VAT), (max(front, foot), ym + width / 2.0, -60), kit.concrete())
            # A door across the ramp's top, which opens as anyone comes up to it; and hard cover set back from it:
            # what comes up the ramp goes round it, and no line from the floor runs past it into the deck.
            kit.door(f"Deck_{team}_Door_{r0}", f, (front, ym, DECK), 0, width, 220, jambs=False)
            cx = front - inward * 260
            kit.stack(f"Deck_{team}_Cover_{r0}", f, "TarpedCrate", (cx - 70, ym - 200, DECK), (cx + 70, ym + 200, DECK + 230), 1, 3, 2)
        # Walls between the ramps and along its sides, 2.2 m above the deck: the spawn is out of sight.
        edges = [y0] + [v for r0, r1 in spans for v in (Y0 + TILE * r0, Y0 + TILE * (r1 + 1))] + [y1]
        for i in range(0, len(edges) - 1, 2):
            if edges[i + 1] > edges[i]:
                box(f"Deck_{team}_Wall_{i}", f, (min(front, front - inward * 30), edges[i], DECK), (max(front, front - inward * 30), edges[i + 1], DECK + 220), sheet("Teal" if s < 0 else "Orange"))
        back = x0 if s < 0 else x1
        for y, name in ((y0, "South"), (y1, "North")):
            box(f"Deck_{team}_Side_{name}", f, (min(back, front), min(y, y + (30 if name == "South" else -30)), DECK), (max(back, front), max(y, y + (30 if name == "South" else -30)), DECK + 220),
                sheet("Teal" if s < 0 else "Orange"))
        # A lamp over it, and the side's colour on the wall behind.
        kit.interior_lamp(f"Deck_{team}_Lamp", f, (x0 + x1) / 2.0, 0, 900, intensity=30.0, radius=1800.0, size=(300, 60))
        box(f"Deck_{team}_Banner", f, (min(back, back + inward * 6), -500, 500), (max(back, back + inward * 6), 500, 900), painted("Teal" if s < 0 else "Orange"))
        # What a deck has lying about, in its back corners: barrels, gas bottles.
        bx = back + inward * 60
        for i, y in enumerate((-900, -830, -880)):
            prop(f"Deck_{team}_Barrel_{i}", f, "Barrel", bx + inward * (i % 2) * 70, y, DECK, i * 40)
        prop(f"Deck_{team}_Gas", f, "GasTank", bx, 900, DECK)


def islands_and_machines():
    """The islands: concrete standing in the vat, their tops at the floor, never eaten. The pumps on the north, the
    boilers on the south, each with hard cover on it; the Stirrer in the middle, in its moat, under the gantry."""
    for s, (x0, y0, x1, y1) in islands("P"):
        team = team_of(s)
        f = f"Islands/Pump_{team}"
        box(f"Pump_{team}_Pylon", f, (x0, y0, VAT), (x1, y1, 0), kit.concrete())
        box(f"Pump_{team}_Top", f, (x0, y0, -2), (x1, y1, 0), kit.floor())
        cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        # The pump: a housing, its motor, and the pipe that rises from it to the roof and runs along under it.
        box(f"Pump_{team}_Housing", f, (cx - 120, cy - 130, 0), (cx + 120, cy + 130, 220), painted("Blue"))
        box(f"Pump_{team}_Motor", f, (cx - 60, cy - 90, 220), (cx + 60, cy + 90, 330), painted("Dark"))
        cylinder(f"Pump_{team}_Pipe", f, cx, cy + 60, 330, 50, HALL_TOP - 330 - 120, kit.steel())
        kit.pipe_y(f"Pump_{team}_PipeRun", f, cx, cy + 60, Y1, HALL_TOP - 120, 50, kit.steel())
    for s, (x0, y0, x1, y1) in islands("B"):
        team = team_of(s)
        f = f"Islands/Boiler_{team}"
        box(f"Boiler_{team}_Pylon", f, (x0, y0, VAT), (x1, y1, 0), kit.concrete())
        box(f"Boiler_{team}_Top", f, (x0, y0, -2), (x1, y1, 0), kit.floor())
        cx = (x0 + x1) / 2.0
        # The boiler: a tank on its side along the island's south edge, on saddles, with a flue up to the roof.
        ty = y0 + 100
        for sx in (cx - 160, cx + 160):
            box(f"Boiler_{team}_Saddle_{int(sx)}", f, (sx - 20, ty - 70, 0), (sx + 20, ty + 70, 40), painted("Dark"))
        kit.pipe_x(f"Boiler_{team}_Tank", f, cx - 260, cx + 260, ty, 40 + 85, 170, painted("Red"))
        cylinder(f"Boiler_{team}_Flue", f, cx + (180 if s < 0 else -180), ty, 200, 40, HALL_TOP - 200, kit.steel())
        box(f"Boiler_{team}_Gauge", f, (cx - 20, ty + 80, 120), (cx + 20, ty + 95, 160), painted("Cream"))
    (x0, y0, x1, y1) = bounds_of(cells_of("S"))
    f = "Islands/Stirrer"
    box("Stirrer_Pylon", f, (x0, y0, VAT), (x1, y1, 0), kit.concrete())
    box("Stirrer_Top", f, (x0, y0, -2), (x1, y1, 0), kit.floor())
    # The Stirrer: its gearbox on the island, hard cover across the middle line, and the drive shaft up to its motor on
    # the gantry; its paddles stand in the fondue in the moat round it.
    box("Stirrer_Gearbox", f, (-180, -180, 0), (180, 180, 240), painted("Yellow"))
    box("Stirrer_GearboxTop", f, (-140, -140, 240), (140, 140, 270), painted("Dark"))
    cylinder("Stirrer_Shaft", f, 0, 0, 270, 70, GANTRY - 30 - 270, kit.steel())
    for i, (px, py, sx, sy) in enumerate(((-500, 250, 20, 160), (500, -250, 20, 160), (250, 500, 160, 20), (-250, -500, 160, 20))):
        paddle = box(f"Stirrer_Paddle_{i}", f, (px - sx / 2.0, py - sy / 2.0, FONDUE - 30), (px + sx / 2.0, py + sy / 2.0, FONDUE + 40), painted("Dark"))
        paddle.static_mesh_component.set_collision_profile_name("NoCollision")


def gantry():
    """The gantry: a steel bridge over the middle from the north path to the south, 3.2 m up, with the Stirrer's
    motor on a platform over its island: the high ground, open to the whole hall, never eaten, with cheese at the
    platform's edges to crouch behind until the mice climb up for it."""
    f = "Gantry"
    (sx0, sy0, sx1, sy1) = bounds_of(cells_of("S"))
    stairs_north = [r for c, r in cells_of("G") if r > ROWS // 2]
    stairs_south = [r for c, r in cells_of("G") if r < ROWS // 2]
    north_foot = Y0 + TILE * (max(stairs_north) + 1)
    south_foot = Y0 + TILE * min(stairs_south)
    run = 18 * 30  # 18 steps of 17.8 cm, each 30 deep
    north_top, south_top = north_foot - run, south_foot + run
    px = 300  # the platform's half width
    box("Gantry_Platform", f, (-px, -px, GANTRY - 30), (px, px, GANTRY), kit.floor())
    box("Gantry_Span_North", f, (-100, px, GANTRY - 30), (100, north_top, GANTRY), kit.floor())
    box("Gantry_Span_South", f, (-100, south_top, GANTRY - 30), (100, -px, GANTRY), kit.floor())
    kit.stairs("Gantry_Stairs_North", f, (0, north_foot, 0), (0, north_top, GANTRY), 200, kit.steel(), solid_under=False)
    kit.stairs("Gantry_Stairs_South", f, (0, south_foot, 0), (0, south_top, GANTRY), 200, kit.steel(), solid_under=False)
    # Girders under the spans and the stairs, on posts down to the vat (in the moat and the stairs' cells: never through
    # a tile).
    for x in (-95, 95):
        box(f"Gantry_Girder_North_{x}", f, (x - 8, px, GANTRY - 60), (x + 8, north_top, GANTRY - 30), painted("Dark"))
        box(f"Gantry_Girder_South_{x}", f, (x - 8, south_top, GANTRY - 60), (x + 8, -px, GANTRY - 30), painted("Dark"))
        for y, top in ((north_top - 15, GANTRY - 60), (north_foot - 40, 0), ((north_top + north_foot) / 2.0, GANTRY / 2.0 - 20),
                       (south_top + 15, GANTRY - 60), (south_foot + 40, 0), ((south_top + south_foot) / 2.0, GANTRY / 2.0 - 20),
                       (sy1 + TILE / 2.0, GANTRY - 60), (sy0 - TILE / 2.0, GANTRY - 60)):
            kit.frame_post(f"Gantry_Post_{x}_{int(y)}", f, x, y, VAT, top, size=20)
        # The stairs' stringers.
        box(f"Gantry_Stringer_North_{x}", f, (x - 6, north_top, -20), (x + 6, north_foot, 0), painted("Dark"))
        box(f"Gantry_Stringer_South_{x}", f, (x - 6, south_foot, -20), (x + 6, south_top, 0), painted("Dark"))
    for (cx, cy) in ((-px + 20, -px + 20), (px - 20, -px + 20), (-px + 20, px - 20), (px - 20, px - 20)):
        kit.frame_post(f"Gantry_PlatformPost_{cx}_{cy}", f, cx, cy, 0, GANTRY - 30, size=24)
    # Rails along the spans and round the platform, open where the spans meet it.
    for x in (-96, 96):
        kit.rail(f"Gantry_Rail_North_{x}", f, (x, px), (x, north_top), GANTRY)
        kit.rail(f"Gantry_Rail_South_{x}", f, (x, south_top), (x, -px), GANTRY)
    for y in (-px + 4, px - 4):
        kit.rail(f"Gantry_PlatformRail_{y}_W", f, (-px, y), (-100, y), GANTRY)
        kit.rail(f"Gantry_PlatformRail_{y}_E", f, (100, y), (px, y), GANTRY)
    for x in (-px + 4, px - 4):
        kit.rail(f"Gantry_PlatformRail_{x}", f, (x, -px), (x, px), GANTRY)
    # The Stirrer's motor in the platform's middle: hard cover up there.
    box("Gantry_Motor", f, (-90, -110, GANTRY), (90, 110, GANTRY + 170), painted("Yellow"))
    box("Gantry_MotorCap", f, (-60, -80, GANTRY + 170), (60, 80, GANTRY + 210), painted("Dark"))
    # Cheese at the platform's west and east edges, to crouch behind.
    for s in (-1, 1):
        for i, y in enumerate((-180, 0, 180)):
            kit.cheese_box(f"Cheese_Gantry_{s}_{i}", "GantryCheese", (s * (px - 70) - 30, y - 70, GANTRY), (s * (px - 70) + 30, y + 70, GANTRY + 100))
    kit.interior_lamp("Gantry_Lamp", f, 0, 0, 1000, intensity=25.0, radius=1600.0, size=(200, 60))


# ---------------------------------------------------------------- cover on the floor

def crate(name, structure, c, r, high=2, size=160, dx=0, dy=0):
    """A crate of cheese on one tile, `high` blocks of a metre: one is crouching cover, two hide a player. It goes down
    with its tile, into the fondue."""
    x, y = centre(c, r)
    kit.cheese_stack(name, structure, (x + dx - size / 2.0, y + dy - size / 2.0, 0), (x + dx + size / 2.0, y + dy + size / 2.0, 100 * high), 1, 1, high)


def wheels(name, structure, c, r, layers=3):
    x, y = centre(c, r)
    kit.wheel_stack(name, structure, x, y, 0, 160, 60, layers)


def low_wall(name, structure, c, r, along_x):
    """A block of cheese a metre high across the middle of a tile: crouching cover."""
    x, y = centre(c, r)
    half = (90, 25) if along_x else (25, 90)
    kit.cheese_box(name, structure, (x - half[0], y - half[1], 0), (x + half[0], y + half[1], 100))


def cover(s, side):
    team = team_of(s)
    C = lambda c: col(c, s)
    north, middle, south = f"CoverNorth{team}", f"CoverMiddle{team}", f"CoverSouth{team}"
    # The north: crates and wheels along the Skim, between the deck, the pump and the gantry's stairs.
    crate(f"N_DeckCorner_{team}", north, C(5), 17)
    crate(f"N_DeckCorner2_{team}", north, C(6), 17, high=1)
    wheels(f"N_PumpWest_{team}", north, C(9), 18)
    crate(f"N_PumpSouth_{team}", north, C(11), 13)
    crate(f"N_PumpSouth2_{team}", north, C(12), 13, high=1)
    wheels(f"N_Run_{team}", north, C(15), 16)
    crate(f"N_Stairs_{team}", north, C(15), 15)
    low_wall(f"N_Wall_{team}", north, C(16), 19, True)
    crate(f"N_Field_{team}", north, C(8), 13, high=1)
    crate(f"N_Field2_{team}", north, C(14), 14)
    # The middle: low walls along the zig-zag path, crates either side of it and by the moat.
    low_wall(f"M_Path_{team}", middle, C(7), 11, True)
    crate(f"M_Bend_{team}", middle, C(9), 10, high=1)
    crate(f"M_Bend2_{team}", middle, C(10), 10)
    crate(f"M_North_{team}", middle, C(10), 12)
    crate(f"M_South_{team}", middle, C(9), 7)
    wheels(f"M_South2_{team}", middle, C(12), 7)
    crate(f"M_Moat_{team}", middle, C(14), 11, high=1)
    crate(f"M_Moat2_{team}", middle, C(14), 7)
    low_wall(f"M_Plank_{team}", middle, C(14), 8, False)
    crate(f"M_Deck_{team}", middle, C(6), 8)
    # The south: wheels and crates round the boiler and along the Drain.
    crate(f"S_DeckCorner_{team}", south, C(5), 2)
    wheels(f"S_DeckCorner2_{team}", south, C(4), 1)
    crate(f"S_Field_{team}", south, C(9), 6)
    crate(f"S_Field2_{team}", south, C(12), 5, high=1)
    wheels(f"S_Tunnel_{team}", south, C(13), 5)
    crate(f"S_Drain_{team}", south, C(14), 1)
    crate(f"S_Drain2_{team}", south, C(16), 3, high=1)
    wheels(f"S_Field3_{team}", south, C(16), 6)
    # The Rind Run: a covered way of cheese over the path east of the boiler, its walls on the tiles either side and
    # its ends open. The mice take its walls (or the tiles under them) and its roof comes down.
    xs = [cell(C(c), 3) for c in (11, 14)]
    west, east = min(xs[0][0], xs[1][0]), max(xs[0][2], xs[1][2])
    ym = (cell(0, 3)[1] + cell(0, 3)[3]) / 2.0
    start, end = (west, ym), (east, ym)
    if s > 0:
        start, end = end, start
    kit.tunnel(f"RindRun{team}", f"RindRun{team}", start, end, width=200, height=250, wall=50, lamps=400)


# ---------------------------------------------------------------- the hall

def hall():
    f = "Hall"
    # The walls inside: concrete to 3 m, sheets above (the boundary), steel columns every 6 m, the vents the mice
    # come in by.
    for x in range(X0, X1 + 1, 600):
        for y, d in ((Y1, -1), (Y0, 1)):
            box(f"Hall_Column_{x}_{y}", f + "/Columns", (x - 20, min(y, y + d * 40), VAT), (x + 20, max(y, y + d * 40), HALL_TOP), painted("Dark"))
    for y, d, name in ((Y1, -1, "North"), (Y0, 1, "South")):
        box(f"Hall_Dado_{name}", f, (X0, min(y, y + d * 10), VAT), (X1, max(y, y + d * 10), 300), kit.concrete())
    for x, d, name in ((X0, 1, "West"), (X1, -1, "East")):
        box(f"Hall_Dado_{name}", f, (min(x, x + d * 10), Y0, VAT), (max(x, x + d * 10), Y1, 300), kit.concrete())
    # The roof: panels across the hall on trusses, with strips of skylight between.
    x = X0
    while x < X1:
        box(f"Hall_Roof_{x}", f + "/Roof", (x, Y0 - 100, HALL_TOP), (min(x + 700, X1), Y1 + 100, HALL_TOP + 40), kit.roof())
        box(f"Hall_Truss_{x}", f + "/Roof", (x + 740, Y0, HALL_TOP - 80), (x + 760, Y1, HALL_TOP), painted("Dark"))
        x += 800
    # Lamps over the floor, in two rows, and the light of the fondue from below where the floor is open.
    for lx in (-2400, -1200, 1200, 2400):
        for ly in (-1100, 1100):
            kit.interior_lamp(f"Hall_Lamp_{lx}_{ly}", f + "/Lamps", lx, ly, HALL_TOP - 120, intensity=60.0, radius=2600.0, temperature=4300.0, size=(260, 60))
            box(f"Hall_Lamp_{lx}_{ly}_Chain", f + "/Lamps", (lx - 2, ly - 2, HALL_TOP - 114), (lx + 2, ly + 2, HALL_TOP), painted("Dark"))
    # A pipe run along each long wall, and steam rising from the vat by the walls and in the moat.
    for y, d in ((Y1 - 70, 1), (Y0 + 70, -1)):
        kit.pipe_x(f"Hall_Pipe_{y}", f + "/Pipes", X0, X1, y, 420, 40, painted("Red"))
        kit.pipe_x(f"Hall_Pipe_{y}_B", f + "/Pipes", X0, X1, y - d * 10, 480, 28, kit.steel())
    for i, (x, y) in enumerate(((-500, 500), (500, 500), (-500, -500), (500, -500))):
        kit.steam_column(f"Steam_Moat_{i}", "Vat/Steam", x, y, FONDUE, 160, 700)


def vents_and_mice():
    """The mice come in by vents in the long walls (mouse entries, facing in), four on each, near the middle where
    the Skim and the Drain run close to the walls, and one at each end's corner, north of the decks."""
    f = "Hall/Vents"
    for x in (-1100, -300, 300, 1100):
        for y, yaw, d in ((Y1 + 50, -90, -1), (Y0 - 50, 90, 1)):
            kit.mouse_entry(x, y, VENT_Z, yaw)
            wall = Y1 if d < 0 else Y0
            box(f"Vent_{x}_{wall}", f, (x - 150, min(wall, wall + d * 8), VENT_Z - 20), (x + 150, max(wall, wall + d * 8), VENT_Z + 180), painted("Dark"))
            box(f"Vent_{x}_{wall}_Hood", f, (x - 170, min(wall, wall + d * 40), VENT_Z + 180), (x + 170, max(wall, wall + d * 40), VENT_Z + 200), kit.steel())
    for x, yaw in ((X0 - 50, 0), (X1 + 50, 180)):
        kit.mouse_entry(x, 1600, VENT_Z, yaw)
        d = 1 if x < 0 else -1
        wall = X0 if x < 0 else X1
        box(f"Vent_{wall}_1600", f, (min(wall, wall + d * 8), 1450, VENT_Z - 20), (max(wall, wall + d * 8), 1750, VENT_Z + 180), painted("Dark"))


# ---------------------------------------------------------------- what a match needs: starts, places

def starts():
    for s in (-1, 1):
        team = team_of(s)
        xs = (-3450, -3250) if s < 0 else (3450, 3250)
        kit.team_starts(team, [(x, y) for x in xs for y in (-700, -230, 230, 700)], 0 if s < 0 else 180, z=DECK, name="Deck_")
    # Free-for-all adds starts on every island and path, each facing the middle.
    points = []
    for s, side in ((-1, "West"), (1, "East")):
        yaw = 0 if s < 0 else 180
        for name, c, r in (("Pump", 10, 14), ("Boiler", 9, 3), ("Middle", 9, 11), ("Skim", 14, 18), ("Drain", 16, 1),
                           ("Stirrer", 16, 9), ("North", 6, 14)):
            x, y = centre(col(c, s), r)
            points.append((x, y, yaw, 0, f"{side}_{name}"))
    kit.free_starts(points)


def places():
    for s, side in ((-1, "West"), (1, "East")):
        team = team_of(s)
        for letter, name in (("D", "Deck"), ("P", "Pump"), ("B", "Boiler")):
            (x0, y0, x1, y1) = [b for t, b in islands(letter) if t == s][0]
            kit.place_name(f"{team} {name}", (x0 - (0 if letter == "D" else 200), y0 - 200, -100), (x1 + (0 if letter == "D" else 200), y1 + 200, 900))
        xs = [cell(col(c, s), 3) for c in (11, 14)]
        kit.place_name(f"{team} Rind Run", (min(a[0] for a in xs), -1500, -100), (max(a[2] for a in xs), -1100, 400))
    kit.place_name("The Skim", (X0 + 1000, 1000, -100), (X1 - 1000, Y1, 900))
    kit.place_name("The Drain", (X0 + 1000, Y0, -100), (X1 - 1000, -1000, 900))
    kit.place_name("The Moat", (-1400, -1000, -100), (1400, 1000, 900))
    kit.place_name("The Stirrer", (-600, -600, -100), (600, 600, 300))
    kit.place_name("The Gantry", (-320, -1800, GANTRY - 40), (320, 1800, 900))
    # The vat itself, under the floor: where the falls go ("fell into the fondue").
    kit.place_name("The Fondue", (X0, Y0, VAT - 200), (X1, Y1, -100))


def hills():
    """King of the Hill's spots: the Stirrer's island round its gearbox, then the pumps' islands round their housings,
    Gouda's and Mozza's, twins. Concrete, never eaten."""
    kit.hill_spots([("Stirrer", 0, 0, 0, 380), ("PumpGouda", -1300, 1100, 0, 300), ("PumpMozza", 1300, 1100, 0, 300)])


def pickups():
    """The weapons' stands (kit.pickup), each on concrete, steel or wax the mice never eat, and clear of the hills (on
    the Stirrer's island and the pumps'): the Roquefort on the gantry's platform, the high ground over everything; a
    Munster on each side's middle path by the moat's plank; a Cheddar Blaster on each north path short of its pump; and a
    Swiss Shotgun on each boiler's island, by the Rind Run."""
    kit.pickup("roquefort", 0, 205, GANTRY, "Gantry")
    for s, side in ((-1, "West"), (1, "East")):
        x, y = centre(col(13, s), 9)
        kit.pickup("munster", x, y, 0, f"Middle{side}")
        x, y = centre(col(8, s), 14)
        kit.pickup("blaster", x, y, 0, f"North{side}")
        kit.pickup("shotgun", s * 1700, -1130, 0, f"Boiler{side}")


def build():
    vat()
    floor()
    decks()
    islands_and_machines()
    gantry()
    kit.mirrored(cover)
    hall()
    vents_and_mice()
    starts()
    places()
    hills()
    pickups()
    # The navigation mesh: the floor and above it, never down in the vat.
    kit.nav_bounds((X0 - 100, Y0 - 100, -100), (X1 + 100, Y1 + 100, 800))
    kit.light_rig("overcast", sun=(-60, 70))


kit.build_level(LEVEL, build)
