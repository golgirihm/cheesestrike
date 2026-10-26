import math, os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, cylinder, fill, place, painted

# Run with the editor closed, after polyhaven.py, import_polyhaven.py and kitchen_assets.py (and what The Creamery
# needs: import_fab.py, build_materials.py, cheese_material.py and wax_material.py):
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_kitchen.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder). It writes what it built to Saved/Lvl_Kitchen.txt.
#
# Builds Crumb Kitchen (/Game/CheeseStrike/Maps/Lvl_Kitchen) from nothing, with the blocks in map_kit.py: a giant's
# kitchen, five times life size, so that the players are the size of a mouse in a real one; about 70 by 46 m.
# docs/unreal/maps/kitchen.md says how it plays and what the mice change. The counter runs along the north wall, its
# worktop a plateau 4.5 m up (the north lane), its cupboards a covered way under it; the floor is the middle lane, with
# a wedge of cheese the size of a house leaning against the counter, its slope a ramp up to the worktop and a hollow
# through it; the table and its chairs stand over the south lane. Gouda come in at the west end by the fridge, Mozza at
# the east by the larder. The west half is built and mirrored for the east (x to -x), with its own things on it; what
# straddles the middle is built once. Units are centimetres, the floor is at 0, x runs west to east, y south to north.
# Its surfaces and things are Poly Haven's (CC0), its books and appliances blocks.

LEVEL = "/Game/CheeseStrike/Maps/Lvl_Kitchen"

HALF_X = 3500          # the room's west and east walls
Y_S, Y_N = -2100, 2500  # its south and north walls
CEILING = 1250         # a 2.5 m ceiling, five times
FRONT = 1900           # the counter's front
TOP = 450              # the worktop: 90 cm, five times
UNDER = 410            # the worktop's underside: the cupboards' ceiling
SHELF = 18             # the cupboards' floor: a step up from the room's
CUPBOARDS = 2000       # the cupboards run from -2000 to 2000; drawers and appliances beyond
PANEL = 700            # the panels between the cupboards: Under the Sink is from -700 to 700
SINK_X, SINK_Y = 600, (2080, 2420)
# The wedge: from y 300 (its tip, on the floor) to the counter's front (its top, at the worktop), 14 m wide.
WEDGE_X = 730         # its outer sides
WEDGE_Y = (300, FRONT)
STRIPS = 8             # 2 m each along the slope
COLUMNS = 9            # across it, inside its rind
SLOTS = (1, 7)         # the columns open over the hollow
RIND = 130             # its sides, red wax
HOLLOW = (1500, FRONT)  # the way through it, against the counter
HOLLOW_TOP = 241.25    # under the planks that carry the slope over it (the slope is 281.25 at the hollow's south wall)
# The cookbooks up to the worktop: 25 books of 18 cm, 30 cm apart, from y 1150 to the counter.
BOOKS_X = (2050, 2330)
BOOK = 18.0
TREAD = 30.0
# The table, five times a farmhouse table: its top at 3.75 m.
TABLE_X = 1300
TABLE_Y = (-1500, -700)
TABLE_TOP = 375
TABLE_BOOKS = (-1240, -960)  # the books up to it at either end, across y
SPAWN_X = 2800         # each side's end of the room: from here to the wall
DRESSER_X = (2650, 2875)  # the dresser that screens each side's starts (x for the east)
DRESSER_Y = (-650, 750)
SPAWN_WING = 3250         # the wings at the dresser's ends reach back to here, 2.5 m short of the end wall


def team_of(s):
    return "Gouda" if s < 0 else "Mozza"


def m(name):
    return kit.material("MI_Kitchen_" + name)


def ph(name):
    """A Poly Haven model (import_polyhaven.py)."""
    return unreal.load_asset(f"/Game/PolyHaven/{name}/SM_{name}")


RAMP = unreal.load_asset("/Game/LevelPrototyping/Meshes/SM_Ramp")      # 1 m, its top falling from 1 at y 0 to 0 at y 1
CHUNK = unreal.load_asset("/Game/LevelPrototyping/Meshes/SM_ChamferCube")
BOOK_MESH = unreal.load_asset("/Game/CheeseStrike/Meshes/SM_KitchenBook")
COVERS = ("Red", "Green", "Blue", "Brown", "Ochre", "Black", "Cream")


def span(s, a, b):
    """x from a to b (given for the east, as distances from the middle) on side s, low to high."""
    return (min(s * a, s * b), max(s * a, s * b))


def xbox(label, folder, s, x0, x1, y0, y1, z0, z1, material):
    """A block from x0 to x1 (east, mirrored by s) and y0 to y1, z0 to z1."""
    a, b = span(s, x0, x1)
    return box(label, folder, (a, y0, z0), (b, y1, z1), material)


def xfill(label, folder, model, s, x0, x1, y0, y1, z0, z1, yaw=0, materials=None):
    """A model filling the block from x0 to x1 (east, mirrored by s); `yaw` for the east, mirrored for the west."""
    a, b = span(s, x0, x1)
    turn = yaw if s > 0 else (180 - yaw) % 360
    if turn % 90:
        turn = yaw
    return fill(label, folder, model, (a, y0, z0), (b, y1, z1), turn, materials)


def no_collision(actor):
    actor.static_mesh_component.set_collision_profile_name("NoCollision")
    return actor


def book(label, folder, x, y, z, length, width, cover, yaw=0):
    """A book lying flat, its bottom middle at (x, y, z): `length` along x (its spine at -x, or turned by yaw), `width`
    along y, BOOK thick."""
    return kit.solid(label, folder, (x, y, z), (length, width, BOOK), m("Cover_" + cover), BOOK_MESH, (0, yaw, 0))


def point_lamp(label, folder, x, y, z, intensity, radius, temperature=3400.0, shadows=True, source=20.0):
    light = place(unreal.PointLight, label, folder, (x, y, z))
    c = light.get_component_by_class(unreal.PointLightComponent)
    c.set_editor_property("intensity", intensity)
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("use_temperature", True)
    c.set_editor_property("temperature", temperature)
    c.set_editor_property("source_radius", source)
    c.set_editor_property("cast_shadows", shadows)
    kit.count("lamps")
    return light


def strip_lamp(label, folder, x, y, z, width, intensity, radius, temperature=3200.0):
    """A strip of light under a cupboard, shining down."""
    light = place(unreal.RectLight, label, folder, (x, y, z), (-90, 0, 0))
    c = light.get_component_by_class(unreal.RectLightComponent)
    c.set_editor_property("intensity", intensity)
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("source_width", float(width))
    c.set_editor_property("source_height", 20.0)
    c.set_editor_property("use_temperature", True)
    c.set_editor_property("temperature", temperature)
    c.set_editor_property("barn_door_angle", 70.0)
    kit.count("lamps")
    return light


# ---------------------------------------------------------------- the room

def room():
    f = "Ground"
    kit.ground("Floor", (-HALF_X, Y_S), (HALF_X, Y_N), m("Floor"))
    b = "Ground/Boundary"
    T = 100
    wall = m("Wall")
    # The north wall, with the window over the sink; the south wall, with a window onto the garden.
    north = ((-700, 700), (560, 1100))
    south = ((-1000, 1000), (500, 1050))
    for name, y0, y1, ((wx0, wx1), (wz0, wz1)) in (("North", Y_N, Y_N + T, north), ("South", Y_S - T, Y_S, south)):
        box(f"Wall_{name}_West", b, (-HALF_X - T, y0, -400), (wx0, y1, CEILING + 50), wall)
        box(f"Wall_{name}_East", b, (wx1, y0, -400), (HALF_X + T, y1, CEILING + 50), wall)
        box(f"Wall_{name}_Under", b, (wx0, y0, -400), (wx1, y1, wz0), wall)
        box(f"Wall_{name}_Over", b, (wx0, y0, wz1), (wx1, y1, CEILING + 50), wall)
        window(name, (wx0, wx1), (wz0, wz1), y0 if name == "North" else y1, 1 if name == "North" else -1)
    box("Wall_West", b, (-HALF_X - T, Y_S, -400), (-HALF_X, Y_N, CEILING + 50), wall)
    box("Wall_East", b, (HALF_X, Y_S, -400), (HALF_X + T, Y_N, CEILING + 50), wall)
    # The ceiling, which nobody reaches: no collision, so the minimap leaves it out.
    no_collision(box("Ceiling", "Room", (-HALF_X - T, Y_S - T, CEILING), (HALF_X + T, Y_N + T, CEILING + 50), m("Ceiling")))
    # Skirting along the walls the floor meets (the counter and the fridge stand against the north one).
    white = m("Paint_White")
    box("Skirting_South", "Room", (-HALF_X, Y_S, 0), (HALF_X, Y_S + 5, 50), white)
    for s, side in ((-1, "West"), (1, "East")):
        xbox(f"Skirting_{side}", "Room", s, HALF_X - 5, HALF_X, Y_S, 1850, 0, 50, white)
    # A door in each end wall: the hall's (west) and the garden's (east), shut.
    for s, side in ((-1, "West"), (1, "East")):
        d = f"Room/Door_{side}"
        xbox(f"Door_{side}_Frame", d, s, HALF_X - 12, HALF_X, -1500, -100, 0, 1080, white)
        xbox(f"Door_{side}", d, s, HALF_X - 16, HALF_X - 8, -1440, -160, 0, 1040, m("Oak") if s < 0 else m("Cupboard"))
        for i, (z0, z1) in enumerate(((120, 500), (580, 960))):
            xbox(f"Door_{side}_Panel_{i}", d, s, HALF_X - 22, HALF_X - 16, -1360, -240, z0, z1, m("Oak") if s < 0 else m("Cupboard"))
        xbox(f"Door_{side}_Knob", d, s, HALF_X - 60, HALF_X - 16, -380, -320, 480, 540, painted("Yellow"))
    # The garden beyond the windows, for what they look out on.
    g = "Beyond"
    box("Garden_North", g, (-9000, Y_N + T, -60), (9000, 9000, -20), m("Grass"))
    box("Garden_South", g, (-9000, -9000, -60), (9000, Y_S - T, -20), m("Grass"))
    box("GardenWall_North", g, (-9000, 6500, -20), (9000, 6700, 1200), m("Brick"))
    box("GardenWall_South", g, (-9000, -6700, -20), (9000, -6500, 1200), m("Brick"))
    for x in (-3000, 1800):
        box(f"Hedge_North_{x}", g, (x, 5200, -20), (x + 2200, 6200, 700), m("Grass"))
    box("Hedge_South", g, (-2600, -6200, -20), (900, -5200, 900), m("Grass"))


def window(name, xs, zs, wall_y, inward):
    """A window's frame in the wall: white painted wood, a mullion and a transom, a sill inside. No glass: the giant has
    it open."""
    f = f"Room/Window_{name}"
    white = m("Paint_White")
    inner = wall_y if inward > 0 else wall_y
    y0, y1 = (inner, inner + 100) if inward > 0 else (inner - 100, inner)
    (x0, x1), (z0, z1) = xs, zs
    box(f"Window_{name}_Left", f, (x0, y0, z0), (x0 + 20, y1, z1), white)
    box(f"Window_{name}_Right", f, (x1 - 20, y0, z0), (x1, y1, z1), white)
    box(f"Window_{name}_Top", f, (x0, y0, z1 - 20), (x1, y1, z1), white)
    box(f"Window_{name}_Bottom", f, (x0, y0, z0), (x1, y1, z0 + 20), white)
    mid = (x0 + x1) / 2.0
    box(f"Window_{name}_Mullion", f, (mid - 10, y0 + 40, z0), (mid + 10, y0 + 60, z1), white)
    box(f"Window_{name}_Transom", f, (x0, y0 + 40, (z0 + z1) / 2.0 + 80), (x1, y0 + 60, (z0 + z1) / 2.0 + 100), white)
    # The sill, inside.
    if inward > 0:
        box(f"Window_{name}_Sill", f, (x0 - 60, y0 - 70, z0 - 20), (x1 + 60, y0, z0), white)
    else:
        box(f"Window_{name}_Sill", f, (x0 - 60, y1, z0 - 20), (x1 + 60, y1 + 70, z0), white)


# ---------------------------------------------------------------- the counter: worktop, cupboards, appliances

def counter():
    f = "Counter"
    worktop, cupboard, white, steel = m("Worktop"), m("Cupboard"), m("Paint_White"), m("Stainless")
    # The worktop: oak, from end to end, with a hole for the sink and one for the cooker's hob.
    kit.slab("Worktop", f, (-SPAWN_X, FRONT, UNDER), (SPAWN_X, Y_N, TOP), worktop,
             holes=[((-SINK_X, SINK_Y[0]), (SINK_X, SINK_Y[1])), ((-2700, FRONT), (-2440, 2240))])
    # The splashback: tiles between the worktop and the wall cupboards, under the window too.
    tiles = m("Splashback")
    box("Splashback_West", f, (-SPAWN_X, Y_N - 8, TOP), (-700, Y_N, 790), tiles)
    box("Splashback_East", f, (700, Y_N - 8, TOP), (SPAWN_X, Y_N, 790), tiles)
    box("Splashback_Window", f, (-700, Y_N - 8, TOP), (700, Y_N, 540), tiles)
    # The cupboards' carcass: their floor, back, the panels between them (each with a hole at the back for the
    # pipes, stopped with cheese until the mice eat it: CupboardSeal), and the front with its doors.
    c = "Counter/Cupboards"
    box("Cupboards_Floor", c, (-CUPBOARDS, FRONT, 0), (CUPBOARDS, Y_N, SHELF), white)
    box("Cupboards_Back", c, (-CUPBOARDS, Y_N - 40, SHELF), (CUPBOARDS, Y_N, UNDER), white)
    hole = (2180, 2440, SHELF + 240)
    for s, side in ((-1, "West"), (1, "East")):
        xbox(f"Panel_{side}_Front", c, s, PANEL - 20, PANEL + 20, FRONT + 40, hole[0], SHELF, UNDER, white)
        xbox(f"Panel_{side}_OverHole", c, s, PANEL - 20, PANEL + 20, hole[0], hole[1], hole[2], UNDER, white)
        xbox(f"Panel_{side}_Back", c, s, PANEL - 20, PANEL + 20, hole[1], Y_N - 40, SHELF, UNDER, white)
        for i, (y0, y1) in enumerate(((hole[0], (hole[0] + hole[1]) / 2.0), ((hole[0] + hole[1]) / 2.0, hole[1]))):
            for k, (z0, z1) in enumerate(((SHELF, SHELF + 120), (SHELF + 120, hole[2]))):
                a, b = span(s, PANEL - 60, PANEL + 60)
                kit.cheese_box(f"Cheese_Seal_{side}_{i}_{k}", "CupboardSeal", (a, y0, z0), (b, y1, z1))
        # The pipe through the hole, under the cheese's top.
        a, b = span(s, PANEL - 300, PANEL + 300)
        kit.pipe_x(f"Pipe_{side}", c, a, b, 2420, 300, 30, m("Copper"))
    # The front: panels between the doors; the doors slide open as anyone comes up to them.
    doors = [(-1350, 240, 300), (1350, 240, 300), (-350, 240, 200), (350, 240, 200)]
    edges = [-CUPBOARDS]
    for x, w, h in sorted(doors):
        edges += [x - w / 2.0, x + w / 2.0]
    edges.append(CUPBOARDS)
    for i in range(0, len(edges), 2):
        box(f"Front_{i}", c, (edges[i], FRONT, SHELF), (edges[i + 1], FRONT + 40, UNDER), cupboard)
    for x, w, h in doors:
        box(f"Front_OverDoor_{x}", c, (x - w / 2.0, FRONT, SHELF + h), (x + w / 2.0, FRONT + 40, UNDER), cupboard)
        kit.door(f"CupboardDoor_{x}", c, (x, FRONT + 20, SHELF), 90, w, h, double=False, leaves=cupboard, jambs=False)
    # What makes the front read as cupboard doors: seams and handles.
    seams = [-1700, -1000, -700, 700, 1000, 1700]
    for x in seams:
        box(f"Front_Seam_{x}", c, (x - 3, FRONT - 2, SHELF + 10), (x + 3, FRONT, UNDER - 10), painted("Dark"))
    for x in (-1780, -920, -560, -130, 130, 560, 920, 1780):
        box(f"Front_Handle_{x}", c, (x - 8, FRONT - 14, 280), (x + 8, FRONT, 380), steel)
    # The drawers and the appliances beyond the cupboards, each side.
    for s, side in ((-1, "West"), (1, "East")):
        d = f"Counter/Drawers_{side}"
        for x0, x1 in ((CUPBOARDS, 2440), (2700, SPAWN_X)):
            xbox(f"Drawers_{side}_{x0}", d, s, x0, x1, FRONT, Y_N, 0, UNDER, cupboard)
            for z in (140, 280):
                xbox(f"Drawers_{side}_{x0}_Gap_{z}", d, s, x0 + 10, x1 - 10, FRONT - 2, FRONT, z - 3, z + 3, painted("Dark"))
            if x1 - x0 > 200:
                for z in (90, 230, 360):
                    xbox(f"Drawers_{side}_{x0}_Handle_{z}", d, s, (x0 + x1) / 2.0 - 70, (x0 + x1) / 2.0 + 70, FRONT - 14, FRONT, z - 8, z + 8, steel)
        # Behind the cooker and the washing machine.
        xbox(f"Drawers_{side}_Behind", d, s, 2440, 2700, 2240, Y_N, 0, UNDER, cupboard)
    # The cooker (west): its hob is the worktop there.
    xfill("Cooker", "Counter/Cooker", ph("electric_stove"), -1, 2440, 2700, FRONT, 2240, 0, TOP, 0)
    # The washing machine (east): white enamel, a porthole of dark glass in a steel ring, a dial.
    w = "Counter/WashingMachine"
    box("Washer", w, (2440, FRONT + 4, 0), (2700, 2240, UNDER - 4), m("Enamel_White"))
    kit.pipe_y("Washer_Ring", w, 2570, FRONT - 6, FRONT + 4, 190, 200, steel)
    kit.pipe_y("Washer_Glass", w, 2570, FRONT - 10, FRONT - 4, 190, 160, m("BlackGlass"))
    box("Washer_Panel", w, (2440, FRONT - 4, 330), (2700, FRONT + 4, 400), m("Enamel_White"))
    kit.pipe_y("Washer_Dial", w, 2650, FRONT - 14, FRONT - 4, 365, 40, steel)
    box("Washer_Drawer", w, (2460, FRONT - 8, 345), (2560, FRONT - 4, 385), m("Enamel_White"))
    # The tall things at either end: the fridge (west) and the larder cupboard (east).
    r = "Counter/Fridge"
    xbox("Fridge", r, -1, SPAWN_X, HALF_X, 1850, Y_N, 0, 950, steel)
    xbox("Fridge_Seam", r, -1, 3148, 3152, 1846, 1850, 20, 940, painted("Dark"))
    for x in (3110, 3190):
        xbox(f"Fridge_Handle_{x}", r, -1, x - 8, x + 8, 1820, 1850, 380, 760, steel)
        xbox(f"Fridge_Handle_{x}_Top", r, -1, x - 8, x + 8, 1820, 1850, 750, 770, steel)
    xbox("Fridge_Dispenser", r, -1, 2930, 3080, 1844, 1850, 520, 700, m("BlackGlass"))
    l = "Counter/Larder"
    xbox("Larder", l, 1, SPAWN_X, HALF_X, 1850, Y_N, 0, CEILING, cupboard)
    for z in (520, 1080):
        xbox(f"Larder_Gap_{z}", l, 1, SPAWN_X + 10, HALF_X - 10, 1846, 1850, z - 3, z + 3, painted("Dark"))
    xbox("Larder_Seam", l, 1, 3148, 3152, 1846, 1850, 20, 1240, painted("Dark"))
    for x in (3110, 3190):
        xbox(f"Larder_Handle_{x}", l, 1, x - 8, x + 8, 1830, 1850, 560, 680, steel)
    # The wall cupboards over the worktop (not over the window), lit underneath.
    for s, side in ((-1, "West"), (1, "East")):
        u = f"Counter/WallCupboards_{side}"
        xbox(f"WallCupboards_{side}", u, s, 900, SPAWN_X, 2170, Y_N, 790, 1150, cupboard)
        for x in (1530, 2160):
            xbox(f"WallCupboards_{side}_Seam_{x}", u, s, x - 3, x + 3, 2166, 2170, 800, 1140, painted("Dark"))
        for x in (1480, 2110, 2750):
            xbox(f"WallCupboards_{side}_Handle_{x}", u, s, x - 8, x + 8, 2155, 2170, 810, 900, steel)
        for x in (1350, 2350):
            strip_lamp(f"UnderCupboard_{side}_{x}", u, s * x, 2300, 785, 500, 18.0, 1100.0)
    # The sink: a steel basin let into the worktop, with ramps down its ends (the drainers), a plug hole and a tap.
    k = "Counter/Sink"
    box("Sink_Basin", k, (-SINK_X, SINK_Y[0], 370), (SINK_X, SINK_Y[1], 390), steel)
    for name, a, b in (("South", (-SINK_X, SINK_Y[0], 390), (SINK_X, SINK_Y[0] + 8, TOP)), ("North", (-SINK_X, SINK_Y[1] - 8, 390), (SINK_X, SINK_Y[1], TOP))):
        box(f"Sink_{name}", k, a, b, steel)
    for s, side in ((-1, "West"), (1, "East")):
        kit.ramp(f"Sink_Drainer_{side}", k, (s * SINK_X, (SINK_Y[0] + SINK_Y[1]) / 2.0, TOP), (s * 420, (SINK_Y[0] + SINK_Y[1]) / 2.0, 390), SINK_Y[1] - SINK_Y[0], steel, 20)
    cylinder("Sink_Plug", k, 0, 2250, 390, 70, 1, m("BlackGlass"))
    cylinder("Tap_Riser", k, 0, 2460, TOP, 50, 380, steel)
    kit.pipe_y("Tap_Arm", k, 0, 2200, 2480, 830, 44, steel)
    cylinder("Tap_Spout", k, 0, 2200, 740, 40, 90, steel)
    for x in (-130, 130):
        cylinder(f"Tap_Handle_{x}", k, x, 2460, TOP, 40, 60, steel)
        cylinder(f"Tap_Handle_{x}_Top", k, x, 2460, TOP + 60, 90, 16, steel)
    # Under the sink: its waste pipe and trap, and the water pipes up to the tap.
    for name, x, y, z0, z1, d in (("Waste", 0, 2250, 250, 370, 50), ("Trap", 0, 2330, 200, 250, 60)):
        cylinder(f"Pipe_{name}", k, x, y, z0, d, z1 - z0, m("Copper") if name == "Trap" else m("Enamel_White"))
    kit.pipe_y("Pipe_TrapBack", k, 0, 2250, Y_N - 40, 230, 50, m("Enamel_White"))
    for x in (-130, 130):
        cylinder(f"Pipe_Water_{x}", k, x, 2440, SHELF, 24, UNDER - SHELF, m("Copper"))
    # The ledger on the counter's front that carries the planks over the hollow in the wedge.
    box("Wedge_Ledger", f, (-WEDGE_X - 10, FRONT - 30, HOLLOW_TOP - 20), (WEDGE_X + 10, FRONT, HOLLOW_TOP), m("Oak"))


# ---------------------------------------------------------------- the wedge

def wedge():
    """The giant's wedge of cheese, lying on a cut face against the counter: its slope a ramp from the floor at its tip
    to the worktop at its top (eight strips, each a column of blocks under a sloped cap), and a hollow through it along
    the counter, roofed by planks from the slope's columns to a ledger on the counter. Two slots open from the top of
    the slope down into the hollow. Its sides are its rind, red wax 1.3 m thick, which the mice leave: once they have
    eaten the rest, two narrow ridges still run from the floor up to the worktop, over the hollow's ends."""
    core = WEDGE_X - RIND
    width = 2.0 * core / COLUMNS
    run = (WEDGE_Y[1] - WEDGE_Y[0]) / STRIPS
    rise = TOP / STRIPS
    columns = [(-core + i * width, -core + (i + 1) * width, "Wedge", False, i in SLOTS) for i in range(COLUMNS)]
    columns += [(-WEDGE_X, -core, "Rind", True, False), (core, WEDGE_X, "Rind", True, False)]
    for x0, x1, kind, kept, slot in columns:
        structure = "WedgeRind" if kept else "Wedge"
        name = f"{kind}_{int(x0)}"
        for k in range(STRIPS):
            y0, y1 = WEDGE_Y[0] + k * run, WEDGE_Y[0] + (k + 1) * run
            low = k * rise
            over_hollow = y0 >= HOLLOW[0]
            south_wall = y1 == HOLLOW[0] and not slot
            if over_hollow and slot:
                continue  # the slot down into the hollow
            # The column under the strip's cap: in courses of about a metre, stopping under the planks over the hollow.
            bottom = HOLLOW_TOP + 40 if over_hollow else 0
            top = HOLLOW_TOP if south_wall else low
            courses = max(1, int(round((top - bottom) / 115.0))) if top - bottom > 1 else 0
            for c in range(courses):
                z0 = bottom + (top - bottom) * c / courses
                z1 = bottom + (top - bottom) * (c + 1) / courses
                kit.cheese_box(f"Cheese_{name}_{k}_{c}", structure, (x0, y0, z0), (x1, y1, z1), not kept)
            if south_wall:
                # The plank over the hollow, from this strip's column to the ledger on the counter.
                kit.cheese_box(f"Cheese_{name}_Plank", structure, (x0, y0, HOLLOW_TOP), (x1, WEDGE_Y[1], HOLLOW_TOP + 40), not kept)
            # The cap: its top the slope.
            kit.cheese(f"Cheese_{name}_{k}_Cap", structure, (x1, y1, low), (x1 - x0, y1 - y0, rise), RAMP, edible=not kept, rotation=(0, 180, 0))
    # A pillar of cheese in the middle of the hollow, under the planks: the way through squeezes past it either side,
    # and no line runs the hollow's length until the mice eat it.
    for c, (z0, z1) in enumerate(((0, 120), (120, HOLLOW_TOP))):
        kit.cheese_box(f"Cheese_HollowPillar_{c}", "HollowPillar", (-110, 1610, z0), (110, 1790, z1))
    # Light in the hollow.
    for x in (-400, 400):
        point_lamp(f"Hollow_Lamp_{x}", "Wedge", x, 1700, 200, 15.0, 700.0, 3000.0, shadows=False)


# ---------------------------------------------------------------- the cookbooks: the stairs up to the worktop at each end

def book_stair(name, folder, height, top_at, d, across, axis, tint=0):
    """Two piles of books side by side, each book TREAD on from the one under it, leaning on what they climb to: a stair
    from the floor up `height`, its top at `top_at` along `axis` ("x" or "y"), rising toward +`axis` if `d` is 1 and
    -`axis` if -1, its books side by side from across[0] to across[1] across it. The top books reach in under what they
    lean on. Never eaten."""
    steps = int(round(height / BOOK))
    rise = height / steps
    middle = (across[0] + across[1]) / 2.0
    for i in range(steps):
        near = top_at - d * (steps - i) * TREAD
        for j, (a, b) in enumerate(((across[0], middle), (middle, across[1]))):
            # Each a little different: a few centimetres narrower, longer or shorter, and its own colour.
            narrow = (i * 7 + j * 3) % 4 * 3
            length = 110 + (i * 5 + j * 11) % 3 * 10
            a, b = (a + narrow, b) if j == 0 else (a, b - narrow)
            along = near + d * length / 2.0
            cover = COVERS[(i * 3 + j * 5 + tint) % len(COVERS)]
            spine = 0 if (i + j) % 2 == 0 else 180
            if axis == "y":
                kit.solid(f"{name}_{i}_{j}", folder, ((a + b) / 2.0, along, i * rise), (b - a, length, rise), m("Cover_" + cover), BOOK_MESH, (0, spine, 0))
            else:
                kit.solid(f"{name}_{i}_{j}", folder, (along, (a + b) / 2.0, i * rise), (length, b - a, rise), m("Cover_" + cover), BOOK_MESH, (0, spine, 0))


def cookbooks(s, side):
    """The cookbooks up to the worktop at each end (2.8 m wide, by the cooker or the washing machine), and up to the
    table at its ends."""
    x0, x1 = span(s, BOOKS_X[0], BOOKS_X[1])
    book_stair(f"Cookbooks_{side}", f"Cookbooks_{side}", TOP, FRONT, 1, (x0, x1), "y", 0 if s < 0 else 2)
    book_stair(f"TableBooks_{side}", f"TableBooks_{side}", TABLE_TOP, s * TABLE_X, -s, TABLE_BOOKS, "x", 4 if s < 0 else 1)


# ---------------------------------------------------------------- the worktop's things, each side

def worktop(s, side):
    team = team_of(s)
    f = f"Worktop_{side}"
    steel = m("Stainless")
    if s < 0:
        # The hob's pans: a brass stock pot and an enamel pot.
        xfill("Hob_StockPot", f, ph("brass_pot_01"), s, 2510, 2690, 1960, 2140, TOP, TOP + 175)
        xfill("Hob_Pot", f, ph("pot_enamel_01"), s, 2440, 2640, 2150, 2290, TOP, TOP + 135, 0)
        # A jug of spoons in the corner, the microwave behind the stair's top.
        xfill("Corner_Jug", f, ph("jug_01"), s, 2560, 2780, 2320, 2470, TOP, TOP + 170, 90)
        xfill("Microwave", f, ph("vintage_microwave"), s, 1950, 2430, 2150, 2480, TOP, TOP + 245, 180)
        # The toaster: chrome, two slots, a lever, on a black foot.
        t = f"{f}/Toaster"
        xbox("Toaster_Foot", t, s, 1600, 1900, 2150, 2400, TOP, TOP + 12, painted("Dark"))
        xbox("Toaster", t, s, 1610, 1890, 2160, 2390, TOP + 12, TOP + 160, steel)
        for y in (2215, 2315):
            xbox(f"Toaster_Slot_{y}", t, s, 1640, 1860, y - 14, y + 14, TOP + 159, TOP + 161, m("BlackGlass"))
        xbox("Toaster_Lever", t, s, 1680, 1730, 2140, 2160, TOP + 80, TOP + 110, painted("Dark"))
        # The grater, and the cheese grated by it.
        g = f"{f}/Grater"
        xbox("Grater", g, s, 680, 830, 2260, 2410, TOP, TOP + 230, steel)
        xbox("Grater_Teeth", g, s, 690, 820, 2256, 2260, TOP + 30, TOP + 200, m("BlackGlass"))
        xbox("Grater_Handle", g, s, 740, 770, 2310, 2360, TOP + 230, TOP + 260, painted("Dark"))
        heap(f"Grated_{team}", f"Grated{team}", s, 760, 2160, 6)
    else:
        # The kettle on the washing machine's corner, the bread bin behind the stair's top.
        xfill("Kettle", f, ph("vintage_electric_kettle"), s, 2510, 2690, 1960, 2140, TOP, TOP + 175, 270)
        xfill("Corner_Jug", f, ph("jug_01"), s, 2560, 2780, 2320, 2470, TOP, TOP + 170, 270)
        xfill("Pot", f, ph("pot_enamel_01"), s, 2440, 2640, 2150, 2290, TOP, TOP + 135, 180)
        b = f"{f}/BreadBin"
        xbox("BreadBin", b, s, 1950, 2430, 2150, 2480, TOP, TOP + 210, m("Enamel_Cream"))
        xbox("BreadBin_Lid", b, s, 1960, 2420, 2140, 2160, TOP + 40, TOP + 200, m("Oak"))
        xbox("BreadBin_Knob", b, s, 2160, 2220, 2125, 2140, TOP + 150, TOP + 175, m("Oak"))
        xfill("BreadBin_Buns", b, ph("hamburger_buns"), s, 2000, 2380, 2200, 2300, TOP + 210, TOP + 240)
        # The knife block.
        k = f"{f}/KnifeBlock"
        xbox("KnifeBlock", k, s, 1630, 1880, 2160, 2390, TOP, TOP + 190, m("Table"))
        for i, x in enumerate((1680, 1750, 1820)):
            xbox(f"KnifeBlock_Handle_{i}", k, s, x - 15, x + 15, 2220 + i * 40, 2280 + i * 40, TOP + 190, TOP + 290, painted("Dark"))
        # A pepper mill, and crumbs by it.
        cylinder("PepperMill", f, s * 755, 2340, TOP, 110, 260, m("Table"))
        cylinder("PepperMill_Top", f, s * 755, 2340, TOP + 260, 70, 40, steel)
        heap(f"Crumbs_{team}", f"Grated{team}", s, 760, 2160, 6)
    # The chopping board, with a block of cheese on it.
    xfill("ChoppingBoard", f, ph("wooden_cutting_board"), s, 850, 1550, 1980, 2380, TOP, TOP + 30, 0)
    a, b = span(s, 950, 1430)
    kit.cheese_stack(f"Board_{team}", f"Board{team}", (a, 2040, TOP + 30), (b, 2320, TOP + 210), 3, 2, 2)
    # A shelf on the wall with things on it, out of reach.
    sh = f"{f}/Shelf"
    xbox("Shelf", sh, s, 1300, 1950, 2400, Y_N, 640, 660, m("Oak"))
    if s < 0:
        for i, x in enumerate(range(1350, 1900, 75)):
            cylinder(f"Spice_{i}", sh, s * x, 2450, 660, 55, 100, m("Enamel_" + ("Red" if i % 3 == 0 else "Cream" if i % 3 == 1 else "White")))
            cylinder(f"Spice_{i}_Lid", sh, s * x, 2450, 760, 58, 12, m("Copper"))
    else:
        for i, x in enumerate(range(1330, 1900, 32)):
            # Standing on end: turned up on its side, its covers upright.
            height = 130 + (i * 17) % 40
            kit.solid(f"Shelf_Book_{i}", sh, (s * x - BOOK / 2.0, 2450, 660 + height / 2.0), (height, 90, BOOK), m("Cover_" + COVERS[i % len(COVERS)]), BOOK_MESH, (90, 0, 0))


def heap(name, structure, s, x, y, n):
    """A little heap of grated cheese or crumbs: a few small blocks on the worktop and one on top."""
    pieces = [(-60, -40, 70), (40, -50, 60), (-20, 50, 65), (70, 40, 55), (-80, 60, 50)][:n - 1]
    for i, (dx, dy, size) in enumerate(pieces):
        kit.cheese(f"Cheese_{name}_{i}", structure, (s * (x + dx), y + dy, TOP + size / 2.0), (size, size, size), CHUNK, rotation=(0, i * 23, 0))
    kit.cheese(f"Cheese_{name}_Top", structure, (s * (x - 60), y - 40, TOP + 70 + 25), (50, 50, 50), CHUNK, rotation=(0, 40, 0))


# ---------------------------------------------------------------- the cupboards' insides

def cupboards(s, side):
    team = team_of(s)
    f = f"Cupboards_{side}"
    # The giant's cheese, kept in the cupboard: wheels along the back, in stacks of two.
    for i, x in enumerate((850, 1050, 1250, 1450)):
        kit.wheel_stack(f"CupboardWheels_{team}_{i}", f"Cupboard{team}", s * x, 2330, SHELF, 180, 90, 2)
    # And a block by the door, crouching cover inside.
    a, b = span(s, 1030, 1230)
    kit.cheese_stack(f"CupboardBlock_{team}", f"Cupboard{team}", (a, 2000, SHELF), (b, 2120, SHELF + 120), 2, 1, 1)
    point_lamp(f"Cupboard_Lamp_{side}", f, s * 1350, 2150, 340, 14.0, 1100.0, 3200.0, shadows=False)
    # Pots and pans stored.
    xfill("Stored_Pot", f, ph("brass_pot_01"), s, 1700, 1900, 2250, 2450, SHELF, SHELF + 195)
    xfill("Stored_Pan", f, ph("brass_pan_01"), s, 1600, 1840, 2050, 2180, SHELF, SHELF + 30, 90)
    # Under the sink, this side: bottles, a bucket.
    u = "UnderTheSink"
    if s < 0:
        xfill("Bleach", u, ph("bleach_bottle"), s, 480, 570, 2300, 2440, SHELF, SHELF + 150)
        xfill("Cleaner", u, ph("multi_cleaner_bottle"), s, 380, 470, 2340, 2440, SHELF, SHELF + 115)
    else:
        xfill("AllPurpose", u, ph("all_purpose_cleaner"), s, 430, 590, 2300, 2420, SHELF, SHELF + 160)
    # A block of cheese under the sink, against the panel.
    a, b = span(s, 440, 620)
    kit.cheese_stack(f"SinkBlock_{team}", "UnderTheSink", (a, 1990, SHELF), (b, 2170, SHELF + 200), 1, 1, 2)


def under_the_sink():
    u = "UnderTheSink"
    xfill("Bucket", u, ph("wooden_bucket_01"), 1, -120, 120, 2050, 2290, SHELF, SHELF + 300)
    point_lamp("UnderTheSink_Lamp", u, 0, 2050, 330, 12.0, 900.0, 3200.0, shadows=False)


# ---------------------------------------------------------------- the table and its chairs

def table():
    f = "Table"
    wood, oak = m("Table"), m("Oak")
    y0, y1 = TABLE_Y
    box("Table_Top", f, (-TABLE_X, y0, TABLE_TOP - 35), (TABLE_X, y1, TABLE_TOP), wood)
    for y in (y0, y1 - 40):
        box(f"Table_Apron_{y}", f, (-TABLE_X + 40, y, 300), (TABLE_X - 40, y + 40, TABLE_TOP - 35), wood)
    for x in (-TABLE_X, TABLE_X - 40):
        box(f"Table_Apron_{x}", f, (x, y0 + 40, 300), (x + 40, y1 - 40, TABLE_TOP - 35), wood)
    for x in (-TABLE_X + 20, 0, TABLE_X - 20):
        for y in (y0 + 20, y1 - 20):
            box(f"Table_Leg_{x}_{y}", f, (x - 22, y - 22, 0), (x + 22, y + 22, 300), wood)
    # The chairs, pushed in: four along each side.
    for i, x in enumerate((-975, -325, 325, 975)):
        for side, sy, back in (("North", y1 - 60, 1), ("South", y0 + 60, -1)):
            c = f"{f}/Chair_{side}_{i}"
            sy0, sy1 = (y1 - 60, y1 + 165) if back > 0 else (y0 - 165, y0 + 60)
            box(f"Chair_{side}_{i}_Seat", c, (x - 112, sy0, 200), (x + 112, sy1, 225), oak)
            for lx in (x - 103, x + 103):
                for ly in (sy0 + 9, sy1 - 9):
                    box(f"Chair_{side}_{i}_Leg_{lx}_{ly}", c, (lx - 9, ly - 9, 0), (lx + 9, ly + 9, 200), oak)
            by0, by1 = (sy1 - 18, sy1) if back > 0 else (sy0, sy0 + 18)
            for lx in (x - 103, x + 103):
                box(f"Chair_{side}_{i}_Post_{lx}", c, (lx - 9, by0, 225), (lx + 9, by1, 470), oak)
            box(f"Chair_{side}_{i}_Rail", c, (x - 112, by0, 420), (x + 112, by1, 470), oak)
            box(f"Chair_{side}_{i}_Slat", c, (x - 112, by0 + 4, 320), (x + 112, by1 - 4, 345), oak)
    # On the table, up the books at either end: a tea set and a jug (west), a cake and a bowl of fruit (east), and a
    # board of cheese in the middle, crouching cover along its middle with room to walk either side.
    t = f"{f}/OnIt"
    fill("Table_TeaSet", t, ph("tea_set_01"), (-850, -1330, TABLE_TOP), (-350, -1060, TABLE_TOP + 90))
    fill("Table_Jug", t, ph("jug_01"), (-760, -1000, TABLE_TOP), (-520, -860, TABLE_TOP + 180), 90)
    fill("Table_Cake", t, ph("carrot_cake"), (380, -1330, TABLE_TOP), (680, -1030, TABLE_TOP + 130))
    fill("Table_Bowl", t, ph("wooden_bowl_01"), (500, -980, TABLE_TOP), (800, -800, TABLE_TOP + 70))
    fill("Table_Bowl_Apple", t, ph("food_apple_01"), (560, -950, TABLE_TOP + 20), (660, -850, TABLE_TOP + 110))
    fill("Table_Bowl_Lemon", t, ph("lemon"), (670, -940, TABLE_TOP + 20), (750, -860, TABLE_TOP + 125))
    fill("Table_Croissant", t, ph("croissant"), (-150, -820, TABLE_TOP), (150, -730, TABLE_TOP + 60))
    fill("Table_Board", t, ph("wooden_cutting_board"), (-260, -1300, TABLE_TOP), (260, -1000, TABLE_TOP + 25))
    kit.cheese_stack("TableCheese", "TableCheese", (-200, -1260, TABLE_TOP + 25), (200, -1040, TABLE_TOP + 165), 2, 2, 2)
    # Its lamps.
    for x in (-850, 0, 850):
        lamp = fill(f"Table_Lamp_{x}", f"{f}/Lamps", ph("modern_ceiling_lamp_01"), (x - 110, -1210, CEILING - 480), (x + 110, -990, CEILING))
        point_lamp(f"Table_Light_{x}", f"{f}/Lamps", x, -1100, CEILING - 520, 60.0, 2400.0, 3000.0, shadows=True, source=60.0)


# ---------------------------------------------------------------- the floor's things, each side

def crumbs(s, side):
    """Crumbs of cheese on the floor: crouching cover across the middle and in front of the counter, the size of a
    crumb to a giant. Some lie on others."""
    team = team_of(s)
    spots = [  # x (east), y, size, yaw, and whether another lies on it
        (1050, 1350, 120, 10, True), (1700, 1050, 100, 35, False), (2350, 700, 110, 60, False), (1050, 820, 90, 20, False),
        (350, -150, 110, 15, True), (950, 120, 100, 50, False), (1500, -300, 120, 5, True), (2100, -150, 95, 40, False),
        (1700, -1800, 110, 25, True), (2300, -1250, 100, 70, False), (850, -1850, 105, 15, False), (2000, 350, 90, 80, False),
    ]
    for i, (x, y, size, yaw, more) in enumerate(spots):
        kit.cheese(f"Cheese_Crumb_{team}_{i}", f"Crumbs{team}", (s * x, y, size / 2.0), (size, size * 0.9, size), CHUNK, rotation=(0, yaw * -s, 0))
        if more:
            top = size * 0.6
            kit.cheese(f"Cheese_Crumb_{team}_{i}_On", f"Crumbs{team}", (s * x, y, size + top / 2.0), (top, top, top), CHUNK, rotation=(0, yaw * 2, 0))
    # A block of cheddar dropped on the floor in front of the drawers: full cover, the size of a garden shed to them.
    a, b = span(s, 1250, 1650)
    kit.cheese_stack(f"Cheddar_{team}", f"Cheddar{team}", (a, 380, 0), (b, 640, 210), 2, 2, 2)
    # Under the table: a lump of cheese fallen from it, and crumbs.
    a, b = span(s, 150, 350)
    kit.cheese_stack(f"UnderTable_{team}", f"UnderTable{team}", (a, -1250, 0), (b, -950, 200), 2, 2, 2)
    for i, (x, y, size) in enumerate(((800, -1150, 90), (500, -800, 80), (1150, -1350, 85))):
        kit.cheese(f"Cheese_UnderTable_{team}_{i}", f"UnderTable{team}", (s * x, y, size / 2.0), (size, size, size), CHUNK, rotation=(0, 30 * i, 0))


def floor(s, side):
    """Each end of the room: the dresser that screens a side's starts from the room, with a wing at either end of it
    back toward the end wall, so that the ways out run along the wall and no line from the room reaches the starts;
    and the floor's hard cover beyond."""
    f = f"Floor_{side}"
    team = team_of(s)
    dresser(s, side)
    # The wings: delivery boxes stacked to the north, the bin (west) or the groceries (east) to the south.
    a, b = span(s, DRESSER_X[0], SPAWN_WING)
    kit.stack("Delivery", f, "CardboardBoxes", (a, DRESSER_Y[1] + 10, 0), (b, DRESSER_Y[1] + 210, 400), 2, 1, 2)
    if s < 0:
        fill("Bin", f, ph("metal_trash_can"), (a, DRESSER_Y[0] - 200, 0), (b, DRESSER_Y[0] - 10, 300))
    else:
        fill("Groceries", f, ph("long_life_food"), (a, DRESSER_Y[0] - 150, 0), (b, DRESSER_Y[0] - 10, 300))
    xfill("Bucket", f, ph("wooden_bucket_01"), s, 2360, 2580, -1120, -900, 0, 280, 0 if s < 0 else 90)
    if s < 0:
        xfill("Onion", f, ph("yellow_onion"), s, 2300, 2410, 1020, 1130, 0, 140)
        xfill("Onion_2", f, ph("yellow_onion"), s, 2420, 2540, 1100, 1220, 0, 150, 90)
        for i in range(5):
            book(f"Pile_{i}", f, -2440 + (i % 2) * 8, -300 + (i % 3) * 6, i * BOOK, 150 - i * 4, 115 - (i % 2) * 6, COVERS[(i * 2) % len(COVERS)], 0 if i % 2 else 180)
    else:
        kit.stack("Crate", f, "PlasticCrate", (2300, -420, 0), (2580, -200, 100), 1, 1, 1)
        xfill("Crate_Onion", f, ph("yellow_onion"), s, 2380, 2500, -370, -250, 100, 220)
        xfill("Lemons", f, ph("lemon"), s, 2300, 2410, 1020, 1130, 0, 140)
        xfill("Lemons_2", f, ph("lemon"), s, 2420, 2540, 1100, 1220, 0, 150, 90)
    # Bananas dropped on the way to the table: crouching cover.
    xfill("Bananas", f, ph("bananas"), s, 2000, 2300, -1560, -1440, 0, 90, 0)
    # A block of cheese dropped on the south side of the floor, closing the line along the south wall.
    a, b = span(s, 1550, 1800)
    kit.cheese_stack(f"Dropped_{team}", f"Dropped{team}", (a, Y_S + 60, 0), (b, Y_S + 360, 220), 1, 2, 2)


def dresser(s, side):
    """A dresser standing out from the end wall's middle, its back to the starts: a cupboard as high as the worktop
    with a plate rack on it, oak at the west end and painted at the east. Hard cover the starts are behind."""
    f = f"Floor_{side}/Dresser"
    wood = m("Oak") if s < 0 else m("Paint_White")
    x0, x1 = DRESSER_X
    y0, y1 = DRESSER_Y
    xbox("Dresser", f, s, x0, x1, y0, y1, 0, TOP - 20, wood)
    xbox("Dresser_Top", f, s, x0 - 15, x1, y0 - 15, y1 + 15, TOP - 20, TOP, m("Worktop"))
    # Its doors, facing the room.
    for y in (y0 + (y1 - y0) / 3.0, y0 + 2 * (y1 - y0) / 3.0):
        xbox(f"Dresser_Seam_{int(y)}", f, s, x0 - 2, x0, y - 3, y + 3, 20, TOP - 40, painted("Dark"))
    for y in (y0 + (y1 - y0) / 3.0 - 60, y0 + (y1 - y0) / 3.0 + 60, y0 + 2 * (y1 - y0) / 3.0 - 60, y0 + 2 * (y1 - y0) / 3.0 + 60):
        xbox(f"Dresser_Knob_{int(y)}", f, s, x0 - 12, x0, y - 10, y + 10, 300, 320, m("Copper"))
    # The plate rack: a back, two shelves, plates standing on them.
    xbox("Rack_Back", f, s, x1 - 30, x1, y0, y1, TOP, 950, wood)
    for z in (650, 950):
        xbox(f"Rack_Shelf_{z}", f, s, x1 - 110, x1, y0, y1, z - 20, z, wood)
    for side_y in (y0, y1 - 25):
        xbox(f"Rack_Side_{side_y}", f, s, x1 - 110, x1, side_y, side_y + 25, TOP, 950, wood)
    for z in (TOP, 650):
        for i, y in enumerate(range(int(y0) + 150, int(y1) - 100, 190)):
            colour = "Enamel_White" if (i + z // 100) % 3 else "Enamel_Cream"
            kit.solid(f"Rack_Plate_{z}_{i}", f, (s * (x1 - 70), y, z + 80), (160, 160, 8), m(colour), kit.CYLINDER, (80 if s > 0 else -80, 0, 0))


# ---------------------------------------------------------------- what a match needs

def starts():
    for s, side in ((-1, "West"), (1, "East")):
        team = team_of(s)
        points = [(s * x, y) for x in (3320, 3050) for y in (-450, -130, 190, 510)]
        kit.team_starts(team, points, 0 if s < 0 else 180)
    points = []
    for s, side in ((-1, "West"), (1, "East")):
        yaw = 0 if s < 0 else 180
        for name, x, y, z in (("Worktop", 1750, 1990, TOP), ("Cupboard", 1350, 2150, SHELF), ("UnderTable", 1000, -1100, 0),
                              ("Front", 1450, 1050, 0), ("South", 2150, -1750, 0), ("Middle", 1700, -650, 0)):
            points.append((s * x, y, yaw, z, f"{side}_{name}"))
    kit.free_starts(points)


def mouse_entries():
    """The mice come out of holes: in the skirting of the south wall, in the splashback on the worktop and at the back
    of each cupboard. Each hole is drawn, dark, where its mouse comes out: square at the bottom, rounded at the top (an arch)."""
    f = "Mice"
    dark = m("BlackGlass")

    def hole(label, x, y0, y1, z0):
        for i, ((u0, w0), (u1, w1)) in enumerate(kit.arch_pieces(x - 100, x + 100, z0, z0 + 160)):
            box(f"{label}_{i}", f, (u0, y0, w0), (u1, y1, w1), dark)
    for x in (-2000, -700, 700, 2000):
        kit.mouse_entry(x, Y_S + 5, 0, 90)
        hole(f"MouseHole_South_{x}", x, Y_S, Y_S + 7, 0)
    for x in (-1520, 1520):
        kit.mouse_entry(x, Y_N - 10, TOP, -90)
        hole(f"MouseHole_Worktop_{x}", x, Y_N - 10, Y_N - 7, TOP)
        kit.mouse_entry(x * 0.85, Y_N - 45, SHELF, -90)
        hole(f"MouseHole_Cupboard_{x}", x * 0.85, Y_N - 43, Y_N - 40, SHELF)


def places():
    """What players call each part of the kitchen (for the HUD and the kill feed); the smaller wins where they meet."""
    for s, side in ((-1, "West"), (1, "East")):
        team = team_of(s)

        def area(name, x0, x1, y0, y1, z0=-50, z1=CEILING):
            a, b = span(s, x0, x1)
            kit.place_name(name, (a, y0, z0), (b, y1, z1))
        area("The Fridge" if s < 0 else "The Larder", SPAWN_X, HALF_X, Y_S, Y_N)
        area(f"{team} Cookbooks", BOOKS_X[0] - 100, BOOKS_X[1] + 100, 1150, FRONT, -50, TOP + 20)
        area("The Hob" if s < 0 else "The Kettle", 1950, SPAWN_X, FRONT, Y_N, TOP - 20, CEILING)
        area("The Toaster" if s < 0 else "The Knife Block", 1550, 1950, FRONT, Y_N, TOP - 20, CEILING)
        area(f"{team} Board", 600, 1550, FRONT, Y_N, TOP - 20, CEILING)
        area(f"{team} Cupboard", PANEL, CUPBOARDS, FRONT, Y_N, -10, UNDER)
        area(f"{team} Drawers", WEDGE_X, SPAWN_X, 700, FRONT)
        area("The Bin" if s < 0 else "The Groceries", 1500, SPAWN_X, Y_S, TABLE_Y[0] - 200)
        area(f"{team} Crumbs", WEDGE_X, SPAWN_X, TABLE_Y[1], 700)
    kit.place_name("The Worktop", (-SPAWN_X, FRONT, TOP - 20), (SPAWN_X, Y_N, CEILING))
    kit.place_name("The Sink", (-600, FRONT, TOP - 80), (600, Y_N, CEILING))
    kit.place_name("Under the Sink", (-PANEL, FRONT, -10), (PANEL, Y_N, UNDER))
    kit.place_name("The Wedge", (-WEDGE_X, WEDGE_Y[0], -50), (WEDGE_X, WEDGE_Y[1], TOP + 300))
    kit.place_name("The Hollow", (-WEDGE_X, HOLLOW[0], -50), (WEDGE_X, HOLLOW[1], HOLLOW_TOP))
    kit.place_name("The Crumbs", (-WEDGE_X, TABLE_Y[1], -50), (WEDGE_X, WEDGE_Y[0], CEILING))
    kit.place_name("Under the Table", (-TABLE_X - 100, TABLE_Y[0] - 200, -50), (TABLE_X + 100, TABLE_Y[1] + 200, TABLE_TOP))
    kit.place_name("The Skirting", (-1500, Y_S, -50), (1500, TABLE_Y[0] - 200, CEILING))


def hills():
    """King of the Hill's spots, on the middle line or in twins: the crumbs between the wedge and the table, Gouda's
    drawers, the table's top, and Mozza's drawers. The room's floor and the table's top, never eaten."""
    kit.hill_spots([("Crumbs", 0, -200, 0, 400), ("DrawersGouda", -1500, 1300, 0, 380), ("Table", 0, -1100, TABLE_TOP, 380),
                    ("DrawersMozza", 1500, 1300, 0, 380)])


def pickups():
    """The weapons' stands (kit.pickup), clear of the hills (the crumbs, the drawers, the table's top): the Roquefort in
    the sink, in the middle of the worktop; a Munster in each half of the hollow through the wedge; a Cheddar Blaster
    on the worktop at the top of each side's cookbooks and a Swiss Shotgun in each side's cupboard; the Dual Burratas
    on the floor under the table, between its legs."""
    kit.pickup("roquefort", 0, (SINK_Y[0] + SINK_Y[1]) / 2.0, 390, "Sink")
    kit.pickup("burratas", 0, -1700, 0, "UnderTheTable")
    for s, side in ((-1, "West"), (1, "East")):
        kit.pickup("munster", s * 400, (HOLLOW[0] + HOLLOW[1]) / 2.0, 0, f"Hollow{side}")
        kit.pickup("blaster", s * 2190, 2025, TOP, f"Worktop{side}")
        kit.pickup("shotgun", s * 900, 2080, SHELF, f"Cupboard{side}")


def build():
    room()
    counter()
    wedge()
    under_the_sink()
    table()
    for s, side in ((-1, "West"), (1, "East")):
        cookbooks(s, side)
        worktop(s, side)
        cupboards(s, side)
        crumbs(s, side)
        floor(s, side)
    starts()
    mouse_entries()
    places()
    hills()
    pickups()
    kit.nav_bounds((-HALF_X - 100, Y_S - 100, -60), (HALF_X + 100, Y_N + 100, 1000))
    # Day through the north window: the sun comes in over the sink, across the lanes, and falls on the wedge.
    kit.light_rig("day", sun=(-38, -90))


kit.build_level(LEVEL, build)
