import os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, cylinder, fill, mesh, prop, ramp, decal, cheese, wheel, place, painted, sheet

# Run with the editor closed, after import_fab.py, build_materials.py, cheese_material.py and wax_material.py:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_creamery.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it built to Saved/Lvl_Creamery.txt.
#
# Builds The Creamery (/Game/CheeseStrike/Maps/Lvl_Creamery) from scratch, with the blocks in map_kit.py:
# a cheese works about 130 by 100 m, a dock at each end for each side, and three lanes between them;
# docs/unreal/maps/creamery.md says how it plays and what the mice change. Most of what stands in it is
# cheese, each block an ACheeseStrikeCheese with the structure it belongs to: the houses on the plazas, the
# tunnels along the hall, the hall's racks, catwalks, edges and seals, the Wheel Stack, the walls in front of
# the docks. The west half is built and mirrored for the east (x to -x); what straddles the middle is built
# once. Units are centimetres, the floor is at 0, x runs west to east and y south to north. Its surfaces are
# Megascans textures and its props Megascans models and a shipping container from Fab (import_fab.py); a prop
# that is cover is scaled to fill the block it stands for, so the map plays as its notes say.

LEVEL = "/Game/CheeseStrike/Maps/Lvl_Creamery"

# Bounds of the playable ground; the canal runs west to east south of the hall.
HALF_X = 6600
HALF_Y = 5000
WALL_TOP = 1000
CANAL_Y = (-3600, -2800)
CANAL_DEPTH = 300
CANAL_FLAT_X = 4200   # the canal's floor is flat between -4200 and 4200
CANAL_RAMP_X = 5400   # and ramps up to the ground at either end by 5400
HALL_X = 2400
HALL_Y = 1800
HALL_HEIGHT = 900
DOCK_DOORS = (-1200, 600, 2400)  # the roller doors behind each dock, by y
# The Curing Houses on the plazas (east side; x to -x for the west), two storeys of cheese.
HOUSE_X = (3400, 4600)
HOUSE_Y = (-2000, -600)
# The Rind Tunnels along the hall's south wall, from the walkway's middle to the plaza.
TUNNEL_X = (1000, 3000)
TUNNEL_Y = -2075      # its middle: inside from -2275 to -1875, against the hall's wall
TUNNEL_WIDTH = 400
# The decks along the hall's south and north walls, and the posts under their edges (clear of the racks and catwalks).
DECK_POSTS = (-1940, -1450, -300, 300, 1450, 1940)
CATWALK_X = 900


def team_of(s):
    return "Gouda" if s < 0 else "Mozza"


# ---------------------------------------------------------------- the ground

def ground():
    f = "Ground"
    # North of the canal and south of it, deep enough that their faces are the canal's walls.
    # The yard is asphalt, the rest concrete.
    box("Ground_North", f, (-HALF_X, CANAL_Y[1], -400), (HALF_X, 2600, 0), kit.floor())
    box("Ground_Yard", f, (-HALF_X, 2600, -400), (HALF_X, HALF_Y, 0), kit.asphalt())
    box("Ground_South", f, (-HALF_X, -HALF_Y, -400), (HALF_X, CANAL_Y[0], 0), kit.floor())
    # The canal: a floor 3 m down, ramps up at either end, ground beyond them.
    box("Canal_Floor", f, (-CANAL_RAMP_X, CANAL_Y[0], -CANAL_DEPTH - 100), (CANAL_RAMP_X, CANAL_Y[1], -CANAL_DEPTH), kit.floor())
    canal_middle = (CANAL_Y[0] + CANAL_Y[1]) / 2.0
    canal_width = CANAL_Y[1] - CANAL_Y[0]
    for s, side in ((-1, "West"), (1, "East")):
        ramp(f"Canal_Ramp_{side}", f, (s * CANAL_FLAT_X, canal_middle, -CANAL_DEPTH), (s * CANAL_RAMP_X, canal_middle, 0), canal_width, kit.floor())
        box(f"Canal_End_{side}", f, (min(s * CANAL_RAMP_X, s * HALF_X), CANAL_Y[0], -400), (max(s * CANAL_RAMP_X, s * HALF_X), CANAL_Y[1], 0), kit.floor())
    kit.boundary(-HALF_X, -HALF_Y, HALF_X, HALF_Y, WALL_TOP)


# ---------------------------------------------------------------- one side's dock, plaza, house and tunnel (s = -1 the west, 1 the east)

def dock(s, side):
    f = f"Dock_{side}"
    X = lambda x: s * x  # x given for the east side, as distances from the middle
    team = team_of(s)
    # The loading dock: a platform along the outer wall, 1.2 m up, with ramps down towards the middle.
    box(f"{f}_Platform", f, (min(X(5900), X(HALF_X)), -2400, 0), (max(X(5900), X(HALF_X)), 4400, 120), kit.floor())
    box(f"{f}_PlatformEdge", f, (min(X(5880), X(5900)), -2400, 120), (max(X(5880), X(5900)), 4400, 135), painted("Yellow"))
    for y in (-1600, 3600):
        ramp(f"{f}_Ramp_{y}", f, (X(5900), y, 120), (X(5300), y, 0), 400, kit.floor())
    # Roller doors in the outer wall behind the platform, for the place it is.
    for y in DOCK_DOORS:
        box(f"{f}_Door_{y}", f, (min(X(6595), X(6600)), y - 250, 120), (max(X(6595), X(6600)), y + 250, 620), kit.material("MI_GarageDoor"))
    # Crates on pallets on the platform and the ground, for cover leaving the dock.
    for x, y, h in ((6200, 900, 140), (6300, 2900, 140), (5600, 4500, 120), (5650, -4300, 140), (5700, 2100, 120)):
        z0 = 120 if x > 5900 else 0
        fill(f"{f}_Pallet_{x}_{y}", f, mesh("Pallet"), (X(x) - 70, y - 70, z0), (X(x) + 70, y + 70, z0 + 14))
        fill(f"{f}_Crate_{x}_{y}", f, mesh("TarpedCrate"), (X(x) - 70, y - 70, z0 + 14), (X(x) + 70, y + 70, z0 + h), 90 if x % 200 else 0)
    # The dock's cheese: low walls in front of it, two blocks high, that keep a side's way out covered until eaten.
    for y0, y1 in ((-2200, -1000), (1000, 2200)):
        for y in range(y0, y1, 100):
            for z in (0, 100):
                cheese(f"Cheese_DockWall_{team}_{y}_{z}", f"DockWall{team}", (X(5000), y + 50, z + 50), (100, 100, 100))
    # Starts: four on the platform, four on the ground; all face the middle.
    yaw = 0 if s < 0 else 180
    kit.team_starts(team, [(X(6250), y) for y in (-1800, 0, 1800, 3600)], yaw, z=120, name="Dock_")
    kit.team_starts(team, [(X(5450), y) for y in (-4300, -500, 1300, 4500)], yaw, name="Yard_")

    # The plaza between the dock and the lanes: open, with pallets of wheels to dash between.
    p = f"Plaza_{side}"
    for x, y in ((4300, 3600), (3800, 1200), (4500, -150), (3800, -2450), (4000, 4500)):
        fill(f"{p}_Pallet_{x}_{y}", p, mesh("Pallet"), (X(x) - 75, y - 75, 0), (X(x) + 75, y + 75, 15))
        for layer in (0, 1):
            for dx in (-37, 37):
                for dy in (-37, 37):
                    wheel(f"Cheese_Pallet_{team}_{x}_{y}_{layer}_{dx}_{dy}", f"Pallets{team}", X(x) + dx, y + dy, 15 + layer * 50, 72, 50)
    # A load waiting by the hall's corner: a big tarped crate on a pallet.
    fill(f"{p}_Load_Pallet", p, mesh("Pallet"), (X(3000) - 120, 2050, 0), (X(3000) + 120, 2250, 15))
    fill(f"{p}_Load", p, mesh("TarpedCrate"), (X(3000) - 120, 2050, 15), (X(3000) + 120, 2250, 220))


def curing_house(s, side):
    """The Curing House on each plaza: two storeys of cheese on a steel frame, the high ground over the plaza. A door at
    either end (on the line of the tunnel's mouth and of the dock's south ramp) and one to the plaza; windows onto the
    plaza below, and upstairs onto the plaza, the hall's main door, the dock and the canal. The mice eat its walls from
    the ground, and each column comes down a block at a time; once they are gone its upper floor is an open deck on
    posts, with its stairs, and nothing to hide behind."""
    X = lambda x: s * x
    team = team_of(s)
    name = f"CuringHouse{team}"
    lo = (min(X(HOUSE_X[0]), X(HOUSE_X[1])), HOUSE_Y[0])
    hi = (max(X(HOUSE_X[0]), X(HOUSE_X[1])), HOUSE_Y[1])
    # Its ends: towards the middle and towards the dock. Doors and windows are on its columns' middles (240 cm apart
    # along its north and south walls, 260 along its ends) so that each hole is a column wide.
    middle, dockside = ("E", "W") if s < 0 else ("W", "E")
    kit.building(name, name, lo, hi, storeys=2,
                 doors=[(middle, -1300), (dockside, -1300), ("N", X(4000))],
                 windows=[("N", X(3760), 0), ("N", X(4240), 0),
                          ("N", X(3760), 1), ("N", X(4240), 1), (middle, -1560, 1), (middle, -1040, 1), (dockside, -1300, 1),
                          ("S", X(4240), 1)],
                 stairway=("S", X(3600), X(4250)))
    f = f"Buildings/{name}"
    # Inside: a workbench and a cabinet on the ground floor, blocks of cheese upstairs to crouch behind.
    prop(f"{name}_Bench", f, "Workbench", X(4380), -1000, 0, 90)
    prop(f"{name}_Cabinet", f, "MetalCabinet", X(3700), -720, 0, 180)
    kit.cheese_stack(f"{name}_Upstairs", name, (min(X(3700), X(3900)), -1100, 350), (max(X(3700), X(3900)), -900, 450), 2, 2, 1)


def rind_tunnel(s, side):
    """The Rind Tunnel: a covered way of cheese along the hall's south wall, from the walkway's middle to the plaza, with
    a door into the hall and a gap onto the canal's steps. Its roof's planks lie on its wall of cheese and on a ledger
    on the hall's wall; as the mice eat the wall, the planks drop and the walkway opens again."""
    X = lambda x: s * x
    team = team_of(s)
    # Walking from the middle out to the plaza, its south wall is on the left in the west and on the right in the east.
    wall = "L" if s < 0 else "R"
    kit.tunnel(f"RindTunnel{team}", f"RindTunnel{team}", (X(TUNNEL_X[0]), TUNNEL_Y), (X(TUNNEL_X[1]), TUNNEL_Y), width=TUNNEL_WIDTH,
               height=300, sides=(wall,), openings=[(wall, 1300, 1700)])
    # Its door into the hall: the hall's south door on this side, opening as anyone comes up to it.
    kit.door(f"Hall_Door_South_{side}", "Hall/Doors", (X(1400), -HALL_Y, 0), 90, 400, 300, jambs=False)


# ---------------------------------------------------------------- the north lane: the loading yard

def yard_side(s, side):
    f = f"Yard_{side}"
    X = lambda x: s * x
    team = team_of(s)
    # Shipping containers: alone, and two stacked.
    def container(name, x0, y0, x1, y1, h, colour):
        kit.shipping_container(f"{f}_{name}", f, (min(X(x0), X(x1)), y0, 0), (max(X(x0), X(x1)), y1, h), colour)
    container("Container_A", 2300, 3000, 2900, 3250, 260, "Blue")
    container("Container_B", 1550, 3700, 1800, 4300, 260, "Red")
    container("Container_Stack", 2400, 4300, 3000, 4550, 520, "Teal")
    container("Container_C", 850, 3150, 1450, 3400, 260, "Yellow")
    # A trailer along the yard's north edge.
    box(f"{f}_Trailer", f, (min(X(600), X(1400)), 4550, 40), (max(X(600), X(1400)), 4850, 340), sheet("Cream"))
    for wx in (700, 1300):
        box(f"{f}_Trailer_Wheels_{wx}", f, (X(wx) - 60, 4580, 0), (X(wx) + 60, 4820, 40), painted("Dark"))
    # Cheese waiting between the containers: wheels stacked, and cubes of blocks. Cover the mice take away, so that the
    # yard's fights move.
    kit.wheel_stack(f"YardWheels_{team}_A", f"YardCheese{team}", X(2600), 3950, 0, 150, 70, 3)
    kit.wheel_stack(f"YardWheels_{team}_B", f"YardCheese{team}", X(2750), 3800, 0, 150, 70, 2)
    kit.wheel_stack(f"YardWheels_{team}_C", f"YardCheese{team}", X(3900), 2950, 0, 150, 70, 2)
    kit.cheese_stack(f"YardBlock_{team}", f"YardCheese{team}", (min(X(500), X(700)), 2850, 0), (max(X(500), X(700)), 3250, 200), 1, 2, 2)
    kit.cheese_stack(f"YardDrop_{team}", f"YardCheese{team}", (min(X(2100), X(2300)), 4550, 0), (max(X(2100), X(2300)), 4950, 200), 1, 2, 2)


def yard_middle():
    f = "Yard_Middle"
    # The Wheel Stack: a wall of cheese wheels across the yard, four high, two deep, from the walkway's edge to the
    # north wall, the rows side by side and half a wheel apart. It closes the yard's long line from dock to dock: the
    # north lane is fought through the Squeeze until the mice eat it, and as they eat its foot the wheels come down.
    diameter, height = 180, 90
    for row, x in enumerate((-90, 90)):
        offset = 0 if row == 0 else diameter / 2.0
        y = 2600 + diameter / 2.0 + offset
        while y + diameter / 2.0 <= HALF_Y:
            for layer in range(4):
                wheel(f"Cheese_WheelStack_{row}_{int(y)}_{layer}", "WheelStack", x, y, layer * height, diameter, height)
            y += diameter
    # The Squeeze: two crate stacks on the walkway by the hall that overlap, so the way past the stack zig-zags and no
    # line runs along the hall's north wall from end to end.
    kit.stack("Squeeze_A", f, "TarpedCrate", (-900, HALL_Y, 0), (-500, 2300, 300), 3, 4, 2)
    kit.stack("Squeeze_B", f, "TarpedCrate", (500, 2100, 0), (900, 2600, 300), 3, 4, 2)


# ---------------------------------------------------------------- the middle lane: the aging hall

def hall():
    f = "Hall"
    T = 50  # wall thickness
    metal = sheet("Grey")

    def wall_x(name, x, y0, y1, z0, z1):
        box(f"Hall_{name}", f + "/Walls", (x - T / 2.0, y0, z0), (x + T / 2.0, y1, z1), metal)

    def wall_y(name, y, x0, x1, z0, z1):
        box(f"Hall_{name}", f + "/Walls", (x0, y - T / 2.0, z0), (x1, y + T / 2.0, z1), metal)

    # West and east walls, each with a main door 8 m wide and 5 m high on the middle line.
    for s, side in ((-1, "West"), (1, "East")):
        x = s * HALL_X
        wall_x(f"{side}_South", x, -HALL_Y, -400, 0, HALL_HEIGHT)
        wall_x(f"{side}_North", x, 400, HALL_Y, 0, HALL_HEIGHT)
        wall_x(f"{side}_OverDoor", x, -400, 400, 500, HALL_HEIGHT)
    # North and south walls, each with two doors 4 m wide and 3 m high at x = -1400 and 1400.
    for y, side in ((HALL_Y, "North"), (-HALL_Y, "South")):
        edges = [-HALL_X - T / 2.0, -1600, -1200, 1200, 1600, HALL_X + T / 2.0]
        for i in range(0, len(edges) - 1, 2):
            wall_y(f"{side}_{i}", y, edges[i], edges[i + 1], 0, HALL_HEIGHT)
        for x0, x1 in ((-1600, -1200), (1200, 1600)):
            wall_y(f"{side}_OverDoor_{x0}", y, x0, x1, 300, HALL_HEIGHT)
    # The Seals: the two north doors, to the yard, are stopped with cheese. Once eaten,
    # the hall opens onto the yard and the yard onto the hall.
    for x0 in (-1600, 1200):
        for x in range(x0, x0 + 400, 100):
            for z in range(0, 300, 100):
                cheese(f"Cheese_HallSeal_{x}_{z}", "HallSeal", (x + 50, HALL_Y, z + 50), (100, 100, 100))
    # The roof: panels across the hall with strips of skylight between them.
    x = -HALL_X - T / 2.0
    while x < HALL_X:
        box(f"Hall_Roof_{int(x)}", f + "/Roof", (x, -HALL_Y - T, HALL_HEIGHT), (min(x + 600, HALL_X + T / 2.0), HALL_Y + T, HALL_HEIGHT + 40), kit.roof())
        x += 800
    # The press in the middle, on the line between the main doors: hard cover where the
    # hall's long line would otherwise run from door to door.
    cylinder("Hall_Press_Base", f + "/Press", 0, 0, 0, 520, 120, painted("Dark"))
    cylinder("Hall_Press_Drum", f + "/Press", 0, 0, 120, 420, 260, painted("Red"))
    cylinder("Hall_Press_Head", f + "/Press", 0, 0, 380, 200, 220, kit.steel())
    # The racks: steel shelving in rows across the hall, three shelves of wheels each.
    # North and south of the middle aisle, at three places either side of the press,
    # leaving aisles between them and the middle aisle clear. Eaten, they're bare frames
    # and the hall is one open floor.
    for s in (-1, 1):
        for rx in (600, 1200, 1800):
            for y0 in (300, -1500):
                rack(s * rx, y0, y0 + 1000)
    # The mezzanine along the south wall and the gallery along the north, 4 m up, each with ramps at either end and a
    # rail, broken where the catwalks meet it; and cheese along its edge to crouch behind, which the mice eat away.
    for name, deck_y, edge_y, sign in (("Mezzanine", (-HALL_Y + T / 2.0, -1300), -1300, -1), ("Gallery", (1300, HALL_Y - T / 2.0), 1300, 1)):
        m = f"{f}/{name}"
        box(f"Hall_{name}", m, (-1950, deck_y[0], 370), (1950, deck_y[1], 400), kit.floor())
        for i, (rx0, rx1) in enumerate(((-1950, -CATWALK_X - 110), (-CATWALK_X + 110, CATWALK_X - 110), (CATWALK_X + 110, 1950))):
            kit.rail(f"Hall_{name}_Rail_{i}", m, (rx0, edge_y + sign * 5), (rx1, edge_y + sign * 5), 400)
        for px in DECK_POSTS:
            box(f"Hall_{name}_Post_{px}", m, (px - 10, min(edge_y, edge_y + sign * 20), 0), (px + 10, max(edge_y, edge_y + sign * 20), 370), painted("Yellow"))
        for s, side in ((-1, "West"), (1, "East")):
            ramp(f"Hall_{name}_Ramp_{side}", m, (s * 2150, sign * 300, 0), (s * 2150, edge_y, 400), 300, kit.steel(), thickness=30)
            box(f"Hall_{name}_Landing_{side}", m, (min(s * 1950, s * 2300), deck_y[0], 370), (max(s * 1950, s * 2300), deck_y[1], 400), kit.floor())
        for i, px in enumerate(range(-1800, 1801, 300)):
            if abs(px) == CATWALK_X:
                continue  # where a catwalk meets it
            y0 = edge_y + sign * 20
            kit.cheese_box(f"Cheese_{name}Edge_{i}", "HallEdges", (px - 100, min(y0, y0 + sign * 60), 400), (px + 100, max(y0, y0 + sign * 60), 500))
    # The catwalks: two bridges across the hall at 4 m, over the racks, from the mezzanine to the gallery. Their decks
    # are cheese on beams of cheese in wax, which the mice never eat: eaten, a catwalk is two beams to balance along.
    for s, side in ((-1, "West"), (1, "East")):
        kit.bridge(f"Catwalk_{side}", "Catwalks", (s * CATWALK_X, -1300, 400), (s * CATWALK_X, 1300, 400), width=200, beams=2, beam=40,
                   supports=(850, 1850), wax_beams=True)
    # Lamps over the aisles.
    for lx in (-1500, 0, 1500):
        for ly in (-900, 900):
            light = place(unreal.RectLight, f"Hall_Lamp_{lx}_{ly}", f + "/Lamps", (lx, ly, HALL_HEIGHT - 40), (-90, 0, 0))
            component = light.get_component_by_class(unreal.RectLightComponent)
            component.set_editor_property("intensity", 40.0)
            component.set_editor_property("attenuation_radius", 1500.0)
            component.set_editor_property("source_width", 300.0)
            component.set_editor_property("source_height", 60.0)
            component.set_editor_property("use_temperature", True)
            component.set_editor_property("temperature", 4200.0)
            for dy in (-60, 60):
                prop(f"Hall_Lamp_{lx}_{ly}_Tube_{dy}", f + "/Lamps", "FluorescentLamp", lx, ly + dy, HALL_HEIGHT - 30, 90, 3.0)


def rack(x, y0, y1):
    f = "Hall/Racks"
    depth = 120
    for py in (y0, (y0 + y1) / 2.0, y1):
        for px in (x - depth / 2.0, x + depth / 2.0):
            box(f"Rack_Post_{int(x)}_{int(py)}_{int(px)}", f, (px - 5, py - 5, 0), (px + 5, py + 5, 310), painted("Blue"))
    for z in (10, 110, 210):
        box(f"Rack_Shelf_{int(x)}_{int(y0)}_{z}", f, (x - depth / 2.0, y0, z), (x + depth / 2.0, y1, z + 10), painted("Orange"))
        y = y0 + 50
        while y < y1:
            wheel(f"Cheese_Rack_{int(x)}_{int(y)}_{z}", "Racks", x, y, z + 10, 98, 80)
            y += 100
    box(f"Rack_Top_{int(x)}_{int(y0)}", f, (x - depth / 2.0, y0, 300), (x + depth / 2.0, y1, 310), painted("Orange"))


# ---------------------------------------------------------------- the south lane: the canal and the tanks

def south():
    f = "South"
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        team = team_of(s)
        # A concrete bridge across the canal at either side, with low rails.
        b = f"{f}/Bridge_{side}"
        box(f"Bridge_{side}", b, (X(2800) - 200, CANAL_Y[0], -50), (X(2800) + 200, CANAL_Y[1], 0), kit.floor())
        for rx in (-190, 190):
            box(f"Bridge_{side}_Rail_{rx}", b, (X(2800) + rx - 8, CANAL_Y[0], 0), (X(2800) + rx + 8, CANAL_Y[1], 100), painted("Yellow"))
        # Steps out of the canal, along its north wall.
        ramp(f"Canal_Steps_{side}", f, (X(1300), -2950, -CANAL_DEPTH), (X(2500), -2950, 0), 300, kit.floor())
        # Tank Row, south of the canal: silos and barriers, and cheese waiting by the silos.
        for tx in (1200, 3200):
            cylinder(f"Silo_{side}_{tx}", f + "/Tanks", X(tx), -4300, 0, 400, 900, kit.steel())
            cylinder(f"Silo_{side}_{tx}_Cap", f + "/Tanks", X(tx), -4300, 900, 300, 80, kit.steel())
        for bx, by in ((2200, -3900), (400, -4550), (4000, -4400)):
            prop(f"Barrier_{side}_{bx}", f + "/Barriers", "ConcreteBarrier", X(bx), by)
        kit.cheese_stack(f"TankCheese_{team}", f"TankCheese{team}", (min(X(500), X(700)), -4350, 0), (max(X(500), X(700)), -3950, 200), 1, 2, 2)
        kit.wheel_stack(f"TankWheels_{team}", f"TankCheese{team}", X(4500), -4700, 0, 150, 70, 3)
        # The walkway between the hall and the canal: a barrier past the tunnel's mouth.
        prop(f"Planter_{side}_600", f + "/Walkway", "ConcreteBarrier", X(600), -2550)
    # The Cheese Bridge: the middle crossing over the canal, a deck of cheese on steel girders. The mice take the deck
    # away, and the canal is then crossed at the concrete bridges, or along a girder by whoever keeps their balance.
    for x in range(-200, 200, 100):
        for y in range(CANAL_Y[0], CANAL_Y[1], 100):
            cheese(f"Cheese_Bridge_{x}_{y}", "CheeseBridge", (x + 50, y + 50, -50), (100, 100, 100))
    for gx in (-190, -100, 0, 100, 190):
        box(f"CheeseBridge_Girder_{gx}", f + "/CheeseBridge", (gx - 10, CANAL_Y[0] - 50, -130), (gx + 10, CANAL_Y[1] + 50, -100), painted("Dark"))


# ---------------------------------------------------------------- what makes it a place: pipes, lamps and lines, and the works beyond the wall

def details():
    f = "Details"
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        # The yard: barriers and lamps.
        for bx, by, along in ((1200, 2850, True), (2000, 4050, False), (3100, 3700, True)):
            prop(f"Yard_{side}_Barrier_{bx}", f"Yard_{side}", "ConcreteBarrier", X(bx), by, 0, 0 if along else 90)
        for lx, ly in ((2000, 2700), (3300, 4200), (1000, 4300)):
            kit.lamp_post(f"Lamp_{side}_{lx}_{ly}", f + "/Lamps", X(lx), ly)
        # The hall's main doors: a canopy over each, on posts.
        c = f"Hall/Canopy_{side}"
        box(f"Canopy_{side}", c, (min(X(2400), X(3100)), -650, 560), (max(X(2400), X(3100)), 650, 590), painted("Dark"))
        for py in (-620, 620):
            box(f"Canopy_{side}_Post_{py}", c, (X(3070) - 10, py - 10, 0), (X(3070) + 10, py + 10, 560), kit.steel())
        # A pipe bridge from the tanks over the canal and the tunnel to the hall's south wall.
        kit.pipe_y(f"PipeBridge_{side}_A", f + "/Pipes", X(1900), -4100, -1800, 560, 45, kit.steel())
        kit.pipe_y(f"PipeBridge_{side}_B", f + "/Pipes", X(2000), -4100, -1800, 560, 30, painted("Red"))
        for py in (-3700, -2700):
            bottom = -CANAL_DEPTH if CANAL_Y[0] < py < CANAL_Y[1] else 0
            box(f"PipeBridge_{side}_Support_{py}", f + "/Pipes", (X(1950) - 15, py - 15, bottom), (X(1950) + 15, py + 15, 530), kit.steel())
            box(f"PipeBridge_{side}_Cross_{py}", f + "/Pipes", (min(X(1850), X(2050)), py - 15, 530), (max(X(1850), X(2050)), py + 15, 540), kit.steel())
        # Lamps along the canal.
        for lx in (1000, 3600):
            kit.lamp_post(f"Lamp_{side}_Canal_{lx}", f + "/Lamps", X(lx), -2730)
    # The pipe rack along Tank Row, between the canal and the silos.
    for y, z, d, m in ((-3820, 420, 45, kit.steel()), (-3880, 420, 30, painted("Red")), (-3850, 480, 35, painted("Teal"))):
        kit.pipe_x(f"PipeRack_{y}_{z}", f + "/Pipes", -4500, 4500, y, z, d, m)
    for px in range(-4500, 4501, 1000):
        box(f"PipeRack_Post_{px}", f + "/Pipes", (px - 12, -3862, 0), (px + 12, -3838, 400), kit.steel())
        box(f"PipeRack_Arm_{px}", f + "/Pipes", (px - 12, -3920, 395), (px + 12, -3780, 405), kit.steel())
    # Painted lines on the ground: the walkways along the hall.
    for y in (HALL_Y + 600, -HALL_Y - 600):
        box(f"Line_{y}", f + "/Lines", (-HALL_X - 400, y - 6, 0), (HALL_X + 400, y + 6, 0.6), painted("Yellow"))
    # The boundary wall: pilasters along its inside, so a long wall isn't a flat sheet.
    for x in range(-HALF_X + 800, HALF_X, 800):
        for y, d in ((HALF_Y, -1), (-HALF_Y, 1)):
            box(f"Pilaster_{x}_{y}", "Ground/Boundary", (x - 40, min(y, y + d * 60), 0), (x + 40, max(y, y + d * 60), 1000), kit.concrete())
    for y in range(-HALF_Y + 800, HALF_Y, 800):
        if any(abs(y - door) < 300 for door in DOCK_DOORS):
            continue  # not in front of a dock's doors
        for x, d in ((HALF_X, -1), (-HALF_X, 1)):
            bottom = 120 if -2400 < y < 4400 else 0  # on the docks' platforms
            box(f"Pilaster_{x}_{y}", "Ground/Boundary", (min(x, x + d * 60), y - 40, bottom), (max(x, x + d * 60), y + 40, 1000), kit.concrete())
    # The works beyond the wall, for a skyline: cold stores, a chimney, a battery of tall silos.
    b = "Beyond"
    box("Beyond_ColdStore_West", b, (-5200, 5700, 0), (-800, 9200, 1800), sheet("Teal"))
    box("Beyond_ColdStore_East", b, (1400, 5900, 0), (5600, 8200, 1300), sheet("Grey"))
    cylinder("Beyond_Chimney", b, 2600, 8800, 0, 320, 3600, kit.concrete())
    for x in range(-5000, 5001, 1500):
        cylinder(f"Beyond_Silo_{x}", b, x, -6600, 0, 600, 2400, kit.steel())
        cylinder(f"Beyond_Silo_{x}_Cap", b, x, -6600, 2400, 420, 150, kit.steel())
    box("Beyond_Office_West", b, (-9800, -3000, 0), (-7400, 2500, 1500), kit.material("MI_ConcreteFacade"))
    box("Beyond_Office_East", b, (7400, -1000, 0), (9600, 4000, 1100), sheet("Grey"))
    # Ground for them, outside the wall only (under the yard it would roof the canal).
    box("Beyond_Ground_North", b, (-12000, HALF_Y + 100, -60), (12000, 12000, -40), kit.asphalt())
    box("Beyond_Ground_South", b, (-12000, -10000, -60), (12000, -HALF_Y - 100, -40), kit.asphalt())
    box("Beyond_Ground_West", b, (-12000, -HALF_Y - 100, -60), (-HALF_X - 100, HALF_Y + 100, -40), kit.asphalt())
    box("Beyond_Ground_East", b, (HALF_X + 100, -HALF_Y - 100, -60), (12000, HALF_Y + 100, -40), kit.asphalt())


# ---------------------------------------------------------------- dressing: what a working place has lying about, kept off the lanes, the ramps and the lines the tests check

def dressing():
    f = "Dressing"
    down = (-90, 0, 0)
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        d = f"{f}/{side}"
        # The dock: crates, barrels, a hand truck and boxes on a pallet along the back wall, oil on the platform, and each door's number.
        for i in range(3):
            prop(f"{d}_DockCrates_{i}", d, "PlasticCrate", X(6450), -2250, 120 + i * 26)
        for i, (x, y) in enumerate(((6480, 1250), (6490, 1330), (6420, 1295))):
            prop(f"{d}_DockBarrel_{i}", d, "Barrel", X(x), y, 120, i * 40)
        prop(f"{d}_HandTruck", d, "HandTruck", X(6500), -300, 120, 90 if s < 0 else -90)
        prop(f"{d}_DockPallet", d, "Pallet", X(6450), 4250, 120)
        prop(f"{d}_DockBoxes", d, "CardboardBoxes", X(6450), 4250, 134, 0, 2.5)
        decal(f"{d}_DockOil_A", d, "OilStain", (X(6150), -900, 120), (40, 160, 160), down)
        decal(f"{d}_DockOil_B", d, "OilStain", (X(6250), 2250, 120), (40, 120, 120), down)
        for number, y in zip(("One", "Two", "Three"), DOCK_DOORS):
            decal(f"{d}_DockNumber_{number}", d, "Painted" + number, (X(6597), y, 520), (30, 110, 110), (0, 0 if s > 0 else 180, 90))
        # The yard: drums and gas bottles by the north wall, cracks in the asphalt, oil under the trailer.
        for i, (x, y) in enumerate(((1850, 4880), (1920, 4900), (1890, 4820))):
            prop(f"{d}_YardDrum_{i}", d, "PlasticDrum", X(x), y, 0, i * 50)
        for i, x in enumerate((1520, 1580)):
            prop(f"{d}_YardGas_{i}", d, "GasTank", X(x), 4935)
        for i, (x, y) in enumerate(((1900, 2900), (3600, 4200), (600, 4000), (4800, 3300))):
            decal(f"{d}_YardCrack_{i}", d, "AsphaltCrack", (X(x), y, 0), (40, 300, 300), (-90, i * 70, 0))
        decal(f"{d}_YardOil", d, "OilStain", (X(1000), 4700, 0), (40, 220, 220), down)
        # The plaza: a bin by the house, barrels by the canal, oil where loads stand.
        prop(f"{d}_HouseBin", d, "TrashCan", X(3300), -450)
        for i, (x, y) in enumerate(((4900, -2450), (4970, -2480), (4930, -2390))):
            prop(f"{d}_PlazaBarrel_{i}", d, "Barrel", X(x), y, 0, i * 60)
        decal(f"{d}_PlazaOil_A", d, "OilStain", (X(3000), 1900, 0), (40, 200, 200), down)
        decal(f"{d}_PlazaOil_B", d, "OilStain", (X(4000), 300, 0), (40, 150, 150), down)
        # The hall: barrels in the north corners, a workbench and a cabinet under the mezzanine.
        for i, (x, y) in enumerate(((2300, 1650), (2230, 1690), (2290, 1580))):
            prop(f"{d}_HallBarrel_{i}", d, "Barrel", X(x), y, 0, i * 30)
        prop(f"{d}_HallBench", d, "Workbench", X(1000), -1700)
        prop(f"{d}_HallCabinet", d, "MetalCabinet", X(600), -1720)
        decal(f"{d}_HallOil", d, "OilStain", (X(1200), -900, 0), (40, 180, 180), down)
        # Tank Row: barrels and a gas bottle between the silos; rust leaking down the south wall.
        for i, (x, y) in enumerate(((2200, -4700), (2140, -4740))):
            prop(f"{d}_TankBarrel_{i}", d, "Barrel", X(x), y, 0, i * 45)
        prop(f"{d}_TankGas", d, "GasTank", X(2280), -4720)
        for i, x in enumerate((1800, 4600)):
            decal(f"{d}_WallLeak_{i}", d, "Leakage", (X(x), -4995, 600), (30, 500, 220), (0, -90, 90))


# ---------------------------------------------------------------- what a match needs: starts, the mice's ways in, places

def free_starts():
    """In free-for-all everyone is an enemy, and comes back at the start furthest from them all: so as not to begin and
    come back in a crowd on a dock, there are starts in every lane too, each facing the middle."""
    points = []
    for s, side in ((-1, "West"), (1, "East")):
        for name, x, y in (("Plaza", 4100, 200), ("Yard", 3600, 3300), ("YardNorth", 1200, 4300), ("HallNorth", 1500, 1500),
                           ("HallSouth", 1500, -1000), ("Walkway", 700, -2450), ("Tanks", 2600, -4600)):
            points.append((s * x, y, 0 if s < 0 else 180, 0, f"{side}_{name}"))
    kit.free_starts(points)


def mouse_entries():
    """Where the mice come over the edge (ACheeseStrikeGameMode::SpawnMice): points on top of the outer wall, each
    facing in, mirrored like the map. Four along the north wall (the yard; not where the Wheel Stack meets it) and four
    along the south (Tank Row), and one at each end south of the docks: eight mice come, at eight of the ten."""
    kit.mouse_entries(-HALF_X, -HALF_Y, HALF_X, HALF_Y, WALL_TOP, xs=(-4000, -1500, 1500, 4000))
    kit.mouse_entry(-HALF_X - 50, -3500, WALL_TOP, 0)
    kit.mouse_entry(HALF_X + 50, -3500, WALL_TOP, 180)


def places():
    """What players call each part of the map (for the HUD and the kill feed)."""
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        team = team_of(s)

        def area(name, x0, y0, x1, y1, z0=-400, z1=1200):
            kit.place_name(name, (min(X(x0), X(x1)), y0, z0), (max(X(x0), X(x1)), y1, z1))
        area(f"{team} Dock", 5300, -HALF_Y, HALF_X, HALF_Y)
        area(f"{team} Plaza", HALL_X, -HALF_Y, 5300, HALF_Y)
        area(f"{team} Curing House", HOUSE_X[0], HOUSE_Y[0], HOUSE_X[1], HOUSE_Y[1])
        area(f"{team} Tunnel", TUNNEL_X[0], TUNNEL_Y - TUNNEL_WIDTH / 2.0 - 50, TUNNEL_X[1], -HALL_Y, 0, 330)
        area(f"{team} Yard", 0, 2600, HALL_X, HALF_Y)
        area(f"{team} Catwalk", CATWALK_X - 100, -1300, CATWALK_X + 100, 1300, 340, 600)
    kit.place_name("The Wheel Stack", (-180, 2600, -10), (180, HALF_Y, 400))
    kit.place_name("The Squeeze", (-1000, HALL_Y, -10), (1000, 2600, 600))
    kit.place_name("The Hall", (-HALL_X, -HALL_Y, -10), (HALL_X, HALL_Y, HALL_HEIGHT))
    kit.place_name("The Press", (-500, -500, -10), (500, 500, 700))
    kit.place_name("The Mezzanine", (-2300, -HALL_Y, 340), (2300, -1300, 700))
    kit.place_name("The Gallery", (-2300, 1300, 340), (2300, HALL_Y, 700))
    kit.place_name("The Walkway", (-HALL_X, CANAL_Y[1], -10), (HALL_X, -HALL_Y, 600))
    kit.place_name("The Canal", (-CANAL_RAMP_X, CANAL_Y[0], -CANAL_DEPTH - 10), (CANAL_RAMP_X, CANAL_Y[1], 0))
    kit.place_name("The Cheese Bridge", (-200, CANAL_Y[0], -100), (200, CANAL_Y[1], 300))
    kit.place_name("Tank Row", (-5300, -HALF_Y, -10), (5300, CANAL_Y[0], 1000))


def hills():
    """King of the Hill's spots, on the middle line or in twins (the yards'): the press in the middle of the hall, the
    west yard among the containers, Tank Row between the silos, the east yard, and the walkway between the tunnels.
    Concrete and asphalt, never eaten."""
    kit.hill_spots([("Press", 0, 0, 0, 520), ("YardGouda", -2150, 3800, 0, 330), ("TankRow", 0, -4200, 0, 350),
                    ("YardMozza", 2150, 3800, 0, 330), ("Walkway", 0, -2350, 0, 350)])


def pickups():
    """The weapons' stands (kit.pickup), the better at the riskier spots: the Roquefort on the mezzanine's middle, over
    the whole hall; the Munster in the Squeeze, the north lane's choke; a Cheddar Blaster in each half of the yard (off its hill), a
    Swiss Shotgun in each Rind Tunnel and a Parmesan Grater in each Curing House, nearer a side's own dock."""
    kit.pickup("roquefort", 0, -1580, 400, "Mezzanine")
    kit.pickup("munster", 0, 2200, 0, "Squeeze")
    for s, side in ((-1, "West"), (1, "East")):
        kit.pickup("blaster", s * 1950, 3350, 0, f"Yard{side}")
        kit.pickup("shotgun", s * 2300, TUNNEL_Y, 0, f"Tunnel{side}")
        kit.pickup("grater", s * 4000, -1250, 0, f"House{side}")


def build():
    ground()
    for s, side in ((-1, "West"), (1, "East")):
        dock(s, side)
        curing_house(s, side)
        rind_tunnel(s, side)
        yard_side(s, side)
    yard_middle()
    hall()
    south()
    details()
    dressing()
    free_starts()
    mouse_entries()
    places()
    hills()
    pickups()
    # The navigation mesh's bounds: the playable ground, from the canal's floor to above the houses.
    kit.nav_bounds((-HALF_X - 200, -HALF_Y - 200, -CANAL_DEPTH - 110), (HALF_X + 200, HALF_Y + 200, 1490))
    kit.light_rig("day")


kit.build_level(LEVEL, build)
