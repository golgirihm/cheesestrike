import math
import unreal, traceback

# Run with the editor closed, after import_fab.py, build_materials.py and cheese_material.py:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_creamery.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it built to Saved/build_creamery.txt.
#
# Builds The Creamery (/Game/CheeseStrike/Maps/Lvl_Creamery) from scratch:
# a cheese works about 130 by 100 m, a dock at each end for each side, and
# three lanes between them; docs/unreal/maps/creamery.md says how it plays
# and what the mice change. Every block of cheese is an ACheeseStrikeCheese
# with the structure it belongs to. The west half is built and mirrored for
# the east (x to -x); what straddles the middle is built once. Units are
# centimetres, the floor is at 0, x runs west to east and y south to north.
# Its surfaces are Megascans textures and its props Megascans models and a
# shipping container from Fab (import_fab.py); a prop that is cover is scaled
# to fill the block it stands for, so the map plays as its notes say.

OUT = unreal.Paths.project_saved_dir() + "build_creamery.txt"
LEVEL = "/Game/CheeseStrike/Maps/Lvl_Creamery"
MATS = "/Game/CheeseStrike/Materials/"
lines = []
counts = {}
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
COLOURS = ("Cream", "Teal", "Red", "Blue", "Yellow", "Orange", "Dark", "Grey")
FLOOR = unreal.load_asset(MATS + "MI_ConcreteFloor")
WALL = unreal.load_asset(MATS + "MI_ConcreteWall")
FACADE = unreal.load_asset(MATS + "MI_ConcreteFacade")
ASPHALT = unreal.load_asset(MATS + "MI_Asphalt")
DOOR = unreal.load_asset(MATS + "MI_GarageDoor")
ROOF = unreal.load_asset(MATS + "MI_Roof")
STEEL = unreal.load_asset(MATS + "MI_Steel")
SHEET = {name: unreal.load_asset(MATS + "MI_Corrugated_" + name) for name in COLOURS}
PAINT = {name: unreal.load_asset(MATS + "MI_Painted_" + name) for name in COLOURS}
PROPS = "/Game/Fab/Megascans/Props/"
CONTAINER = unreal.load_asset("/Game/Fab/SousinhoGames/ShippingContainer/SM_Container")

# Bounds of the playable ground; the canal runs west to east south of the hall.
HALF_X = 6600
HALF_Y = 5000
CANAL_Y = (-3600, -2800)
CANAL_DEPTH = 300
CANAL_FLAT_X = 4200   # the canal's floor is flat between -4200 and 4200
CANAL_RAMP_X = 5400   # and ramps up to the ground at either end by 5400
HALL_X = 2400
HALL_Y = 1800
HALL_HEIGHT = 900
DOCK_DOORS = (-1200, 600, 2400)  # the roller doors behind each dock, by y


def v(x, y, z):
    return unreal.Vector(float(x), float(y), float(z))


def place(cls, label, folder, centre, rotation=(0, 0, 0)):
    actor = eas.spawn_actor_from_class(cls, v(*centre), unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def solid(label, folder, centre, size, material, mesh=CUBE, rotation=(0, 0, 0)):
    """A static block of `size` (cm, along its own axes) centred on `centre`."""
    actor = place(unreal.StaticMeshActor, label, folder, centre, rotation)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    actor.set_actor_scale3d(v(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    counts[folder.split("/")[0]] = counts.get(folder.split("/")[0], 0) + 1
    return actor


def box(label, folder, low, high, material):
    """A block from corner `low` to corner `high`."""
    centre = [(a + b) / 2.0 for a, b in zip(low, high)]
    size = [abs(b - a) for a, b in zip(low, high)]
    return solid(label, folder, centre, size, material)


def cylinder(label, folder, centre_x, centre_y, bottom, diameter, height, material):
    return solid(label, folder, (centre_x, centre_y, bottom + height / 2.0), (diameter, diameter, height), material, CYLINDER)


def mesh(name):
    return unreal.load_asset(f"{PROPS}{name}/SM_{name}")


def fill(label, folder, model, low, high, yaw=0, materials=None):
    """A model scaled to fill the block from corner `low` to corner `high`, turned by `yaw` (a multiple of 90): cover keeps its block's size."""
    b = model.get_bounding_box()
    dx, dy, dz = abs(high[0] - low[0]), abs(high[1] - low[1]), abs(high[2] - low[2])
    across, along = (dy, dx) if yaw % 180 else (dx, dy)
    scale = (across / (b.max.x - b.min.x), along / (b.max.y - b.min.y), dz / (b.max.z - b.min.z))
    middle = ((b.min.x + b.max.x) / 2.0 * scale[0], (b.min.y + b.max.y) / 2.0 * scale[1])
    turn = math.radians(yaw)
    offset = (middle[0] * math.cos(turn) - middle[1] * math.sin(turn), middle[0] * math.sin(turn) + middle[1] * math.cos(turn))
    centre = ((low[0] + high[0]) / 2.0 - offset[0], (low[1] + high[1]) / 2.0 - offset[1], min(low[2], high[2]) - b.min.z * scale[2])
    actor = place(unreal.StaticMeshActor, label, folder, centre, (0, yaw, 0))
    component = actor.static_mesh_component
    component.set_static_mesh(model)
    for index, material in enumerate(materials or []):
        component.set_material(index, material)
    actor.set_actor_scale3d(v(*scale))
    counts[folder.split("/")[0]] = counts.get(folder.split("/")[0], 0) + 1
    return actor


def prop(label, folder, name, x, y, z=0, yaw=0, scale=1.0):
    """A model at its own size (times `scale`), standing on the floor at (x, y, z)."""
    model = mesh(name)
    b = model.get_bounding_box()
    half = ((b.max.x - b.min.x) * scale / 2.0, (b.max.y - b.min.y) * scale / 2.0)
    if yaw % 180:
        half = (half[1], half[0])
    return fill(label, folder, model, (x - half[0], y - half[1], z), (x + half[0], y + half[1], z + (b.max.z - b.min.z) * scale), yaw)


def shipping_container(label, folder, low, high, colour):
    """A shipping container (two stacked, for a block 5.2 m high) filling the block, long side along its longer side, painted `colour`."""
    slots = [str(s.get_editor_property("material_slot_name")).lower() for s in CONTAINER.get_editor_property("static_materials")]
    paint = [unreal.load_asset(MATS + "MI_Container_" + ("Frame" if "frame" in s else "Doors" if "door" in s else "Sides") + "_" + colour) for s in slots]
    yaw = 0 if abs(high[0] - low[0]) >= abs(high[1] - low[1]) else 90
    levels = max(1, round(abs(high[2] - low[2]) / 260.0))
    step = abs(high[2] - low[2]) / levels
    for level in range(levels):
        z0 = min(low[2], high[2]) + level * step
        fill(f"{label}_{level}", folder, CONTAINER, (low[0], low[1], z0), (high[0], high[1], z0 + step), yaw + 180 * level, paint)


def stack(label, folder, name, low, high, nx, ny, nz):
    """A block filled with nx by ny by nz of a model, side by side and on top of each other."""
    model = mesh(name)
    sx, sy, sz = (high[0] - low[0]) / nx, (high[1] - low[1]) / ny, (high[2] - low[2]) / nz
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                a = (low[0] + i * sx, low[1] + j * sy, low[2] + k * sz)
                fill(f"{label}_{i}_{j}_{k}", folder, model, a, (a[0] + sx, a[1] + sy, a[2] + sz), 90 * ((i + j + k) % 2))


def decal(label, folder, name, at, size, rotation):
    """A decal of `size` (depth, width, height) at `at`, projecting along its rotation's forward axis."""
    actor = place(unreal.DecalActor, label, folder, at, rotation)
    component = actor.get_component_by_class(unreal.DecalComponent)
    component.set_decal_material(unreal.load_asset(MATS + "MI_Decal_" + name))
    component.set_editor_property("decal_size", v(*size))
    counts["decals"] = counts.get("decals", 0) + 1
    return actor


def ramp(label, folder, start, end, width, material, thickness=40):
    """A walkable slab whose top runs from `start` to `end` (points on its top surface, at the middle of its width)."""
    dx, dy, dz = end[0] - start[0], end[1] - start[1], end[2] - start[2]
    run = math.hypot(dx, dy)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, run))
    p, w = math.radians(pitch), math.radians(yaw)
    up = (-math.sin(p) * math.cos(w), -math.sin(p) * math.sin(w), math.cos(p))
    middle = [(a + b) / 2.0 for a, b in zip(start, end)]
    centre = [middle[i] - up[i] * thickness / 2.0 for i in range(3)]
    return solid(label, folder, centre, (math.hypot(run, dz) + 20, width, thickness), material, rotation=(pitch, yaw, 0))


def cheese(label, structure, centre, size, mesh=CUBE, material=None):
    """A block of cheese: its own actor, part of `structure`."""
    actor = place(unreal.CheeseStrikeCheese, label, "Cheese/" + structure, centre)
    actor.set_editor_property("structure", structure)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(mesh)
    if material is not None:
        component.set_material(0, material)
    actor.set_actor_scale3d(v(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    counts["cheese:" + structure] = counts.get("cheese:" + structure, 0) + 1
    return actor


def wheel(label, structure, x, y, bottom, diameter, height, material=None):
    return cheese(label, structure, (x, y, bottom + height / 2.0), (diameter, diameter, height), CYLINDER, material)


def start(label, x, y, z, yaw, tag):
    """A start, tagged with the side it is for ("Gouda", "Mozza"), or "Free" for one that free-for-all adds to them."""
    actor = place(unreal.PlayerStart, label, "Starts", (x, y, z + 100), (0, yaw, 0))
    actor.set_editor_property("player_start_tag", tag)
    counts["starts"] = counts.get("starts", 0) + 1


# ---------------------------------------------------------------- the ground

def ground():
    f = "Ground"
    # North of the canal and south of it, deep enough that their faces are the canal's walls.
    # The yard is asphalt, the rest concrete.
    box("Ground_North", f, (-HALF_X, CANAL_Y[1], -400), (HALF_X, 2600, 0), FLOOR)
    box("Ground_Yard", f, (-HALF_X, 2600, -400), (HALF_X, HALF_Y, 0), ASPHALT)
    box("Ground_South", f, (-HALF_X, -HALF_Y, -400), (HALF_X, CANAL_Y[0], 0), FLOOR)
    # The canal: a floor 3 m down, ramps up at either end, ground beyond them.
    box("Canal_Floor", f, (-CANAL_RAMP_X, CANAL_Y[0], -CANAL_DEPTH - 100), (CANAL_RAMP_X, CANAL_Y[1], -CANAL_DEPTH), FLOOR)
    canal_middle = (CANAL_Y[0] + CANAL_Y[1]) / 2.0
    canal_width = CANAL_Y[1] - CANAL_Y[0]
    for s, side in ((-1, "West"), (1, "East")):
        ramp(f"Canal_Ramp_{side}", f, (s * CANAL_FLAT_X, canal_middle, -CANAL_DEPTH), (s * CANAL_RAMP_X, canal_middle, 0), canal_width, FLOOR)
        box(f"Canal_End_{side}", f, (min(s * CANAL_RAMP_X, s * HALF_X), CANAL_Y[0], -400), (max(s * CANAL_RAMP_X, s * HALF_X), CANAL_Y[1], 0), FLOOR)
    # The yard's outer wall, all round.
    wall = "Ground/Boundary"
    box("Boundary_North", wall, (-HALF_X - 100, HALF_Y, -400), (HALF_X + 100, HALF_Y + 100, 1000), WALL)
    box("Boundary_South", wall, (-HALF_X - 100, -HALF_Y - 100, -400), (HALF_X + 100, -HALF_Y, 1000), WALL)
    box("Boundary_West", wall, (-HALF_X - 100, -HALF_Y, -400), (-HALF_X, HALF_Y, 1000), WALL)
    box("Boundary_East", wall, (HALF_X, -HALF_Y, -400), (HALF_X + 100, HALF_Y, 1000), WALL)


# ---------------------------------------------------------------- one side's dock and plaza (west; s = -1 builds it, s = 1 mirrors it east)

def dock(s, side):
    f = f"Dock_{side}"
    X = lambda x: s * x  # x given for the east side, as distances from the middle
    team = "Gouda" if s < 0 else "Mozza"
    # The loading dock: a platform along the outer wall, 1.2 m up, with ramps down towards the middle.
    box(f"{f}_Platform", f, (min(X(5900), X(HALF_X)), -2400, 0), (max(X(5900), X(HALF_X)), 4400, 120), FLOOR)
    box(f"{f}_PlatformEdge", f, (min(X(5880), X(5900)), -2400, 120), (max(X(5880), X(5900)), 4400, 135), PAINT["Yellow"])
    for y in (-1600, 3600):
        ramp(f"{f}_Ramp_{y}", f, (X(5900), y, 120), (X(5300), y, 0), 400, FLOOR)
    # Roller doors in the outer wall behind the platform, for the place it is.
    for y in DOCK_DOORS:
        box(f"{f}_Door_{y}", f, (min(X(6595), X(6600)), y - 250, 120), (max(X(6595), X(6600)), y + 250, 620), DOOR)
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
    for i, y in enumerate((-1800, 0, 1800, 3600)):
        start(f"Start_{team}_Dock_{i}", X(6250), y, 120, yaw, team)
    for i, y in enumerate((-4300, -500, 1300, 4500)):
        start(f"Start_{team}_Yard_{i}", X(5450), y, 0, yaw, team)

    # The plaza between the dock and the lanes: open, with pallets of wheels to dash between.
    p = f"Plaza_{side}"
    for x, y in ((4300, 3600), (3800, 1200), (4300, -900), (3800, -2300), (4000, 4500)):
        fill(f"{p}_Pallet_{x}_{y}", p, mesh("Pallet"), (X(x) - 75, y - 75, 0), (X(x) + 75, y + 75, 15))
        for layer in (0, 1):
            for dx in (-37, 37):
                for dy in (-37, 37):
                    wheel(f"Cheese_Pallet_{team}_{x}_{y}_{layer}_{dx}_{dy}", f"Pallets{team}", X(x) + dx, y + dy, 15 + layer * 50, 72, 50)
    # A load waiting by the hall's corner: a big tarped crate on a pallet.
    fill(f"{p}_Load_Pallet", p, mesh("Pallet"), (X(3000) - 120, 2050, 0), (X(3000) + 120, 2250, 15))
    fill(f"{p}_Load", p, mesh("TarpedCrate"), (X(3000) - 120, 2050, 15), (X(3000) + 120, 2250, 220))


# ---------------------------------------------------------------- the north lane: the loading yard

def yard_side(s, side):
    f = f"Yard_{side}"
    X = lambda x: s * x
    # Shipping containers: alone, and two stacked.
    def container(name, x0, y0, x1, y1, h, colour):
        shipping_container(f"{f}_{name}", f, (min(X(x0), X(x1)), y0, 0), (max(X(x0), X(x1)), y1, h), colour)
    container("Container_A", 2300, 3000, 2900, 3250, 260, "Blue")
    container("Container_B", 1550, 3700, 1800, 4300, 260, "Red")
    container("Container_Stack", 2400, 4300, 3000, 4550, 520, "Teal")
    # A trailer along the yard's north edge.
    box(f"{f}_Trailer", f, (min(X(600), X(1400)), 4550, 40), (max(X(600), X(1400)), 4850, 340), SHEET["Cream"])
    for wx in (700, 1300):
        box(f"{f}_Trailer_Wheels_{wx}", f, (X(wx) - 60, 4580, 0), (X(wx) + 60, 4820, 40), PAINT["Dark"])


def yard_middle():
    f = "Yard_Middle"
    team = "Middle"
    # The Wheel Stack: a wall of cheese wheels across the yard, four high, two deep,
    # from the walkway's edge to the north wall. It closes the yard's long line from
    # dock to dock: the north lane is fought through the Squeeze until the mice eat it.
    diameter, height = 180, 90
    for row, x in enumerate((-50, 50)):
        offset = 0 if row == 0 else diameter / 2.0
        y = 2600 + diameter / 2.0 + offset
        while y + diameter / 2.0 <= HALF_Y:
            for layer in range(4):
                wheel(f"Cheese_WheelStack_{row}_{int(y)}_{layer}", "WheelStack", x, y, layer * height, diameter, height)
            y += diameter
    # The Squeeze: two crate stacks on the walkway by the hall that overlap, so the way
    # past the stack zig-zags and no line runs along the hall's north wall from end to end.
    stack("Squeeze_A", f, "TarpedCrate", (-900, HALL_Y, 0), (-500, 2300, 300), 3, 4, 2)
    stack("Squeeze_B", f, "TarpedCrate", (500, 2100, 0), (900, 2600, 300), 3, 4, 2)


# ---------------------------------------------------------------- the middle lane: the aging hall

def hall():
    f = "Hall"
    T = 50  # wall thickness
    metal = SHEET["Grey"]

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
        box(f"Hall_Roof_{int(x)}", f + "/Roof", (x, -HALL_Y - T, HALL_HEIGHT), (min(x + 600, HALL_X + T / 2.0), HALL_Y + T, HALL_HEIGHT + 40), ROOF)
        x += 800
    # The press in the middle, on the line between the main doors: hard cover where the
    # hall's long line would otherwise run from door to door.
    cylinder("Hall_Press_Base", f + "/Press", 0, 0, 0, 520, 120, PAINT["Dark"])
    cylinder("Hall_Press_Drum", f + "/Press", 0, 0, 120, 420, 260, PAINT["Red"])
    cylinder("Hall_Press_Head", f + "/Press", 0, 0, 380, 200, 220, STEEL)
    # The racks: steel shelving in rows across the hall, three shelves of wheels each.
    # North and south of the middle aisle, at three places either side of the press,
    # leaving aisles between them and the middle aisle clear. Eaten, they're bare frames
    # and the hall is one open floor.
    for s in (-1, 1):
        for rx in (600, 1200, 1800):
            for y0 in (300, -1500):
                rack(s * rx, y0, y0 + 1000)
    # The mezzanine along the south wall, 4 m up, with ramps at either end and a rail.
    m = f + "/Mezzanine"
    box("Hall_Mezzanine", m, (-1950, -HALL_Y + T / 2.0, 370), (1950, -1300, 400), FLOOR)
    box("Hall_Mezzanine_Rail", m, (-1950, -1310, 400), (1950, -1300, 500), PAINT["Yellow"])
    for px in range(-1900, 1901, 760):
        box(f"Hall_Mezzanine_Post_{px}", m, (px - 10, -1320, 0), (px + 10, -1300, 370), PAINT["Yellow"])
    for s, side in ((-1, "West"), (1, "East")):
        ramp(f"Hall_Mezzanine_Ramp_{side}", m, (s * 2150, -300, 0), (s * 2150, -1300, 400), 300, STEEL, thickness=30)
        box(f"Hall_Mezzanine_Landing_{side}", m, (min(s * 1950, s * 2300), -HALL_Y + T / 2.0, 370), (max(s * 1950, s * 2300), -1300, 400), FLOOR)
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
            box(f"Rack_Post_{int(x)}_{int(py)}_{int(px)}", f, (px - 5, py - 5, 0), (px + 5, py + 5, 310), PAINT["Blue"])
    for z in (10, 110, 210):
        box(f"Rack_Shelf_{int(x)}_{int(y0)}_{z}", f, (x - depth / 2.0, y0, z), (x + depth / 2.0, y1, z + 10), PAINT["Orange"])
        y = y0 + 50
        while y < y1:
            wheel(f"Cheese_Rack_{int(x)}_{int(y)}_{z}", "Racks", x, y, z + 10, 98, 80)
            y += 100
    box(f"Rack_Top_{int(x)}_{int(y0)}", f, (x - depth / 2.0, y0, 300), (x + depth / 2.0, y1, 310), PAINT["Orange"])


# ---------------------------------------------------------------- the south lane: the canal and the tanks

def south():
    f = "South"
    canal_middle = (CANAL_Y[0] + CANAL_Y[1]) / 2.0
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        # A concrete bridge across the canal at either side, with low rails.
        b = f"{f}/Bridge_{side}"
        box(f"Bridge_{side}", b, (X(2800) - 200, CANAL_Y[0], -50), (X(2800) + 200, CANAL_Y[1], 0), FLOOR)
        for rx in (-190, 190):
            box(f"Bridge_{side}_Rail_{rx}", b, (X(2800) + rx - 8, CANAL_Y[0], 0), (X(2800) + rx + 8, CANAL_Y[1], 100), PAINT["Yellow"])
        # Steps out of the canal, along its north wall.
        ramp(f"Canal_Steps_{side}", f, (X(1300), -2950, -CANAL_DEPTH), (X(2500), -2950, 0), 300, FLOOR)
        # Tank Row, south of the canal: silos and barriers.
        for tx in (1200, 3200):
            cylinder(f"Silo_{side}_{tx}", f + "/Tanks", X(tx), -4300, 0, 400, 900, STEEL)
            cylinder(f"Silo_{side}_{tx}_Cap", f + "/Tanks", X(tx), -4300, 900, 300, 80, STEEL)
        for bx, by in ((2200, -3900), (400, -4550), (4000, -4400)):
            prop(f"Barrier_{side}_{bx}", f + "/Barriers", "ConcreteBarrier", X(bx), by)
        # The walkway between the hall and the canal: barriers to break its line.
        for wx, wy in ((2000, -2300), (600, -2550)):
            prop(f"Planter_{side}_{wx}", f + "/Walkway", "ConcreteBarrier", X(wx), wy)
    # The Cheese Bridge: the middle crossing over the canal, a deck of cheese. The mice
    # take it away, and the canal is then crossed only at the concrete bridges.
    for x in range(-200, 200, 100):
        for y in range(CANAL_Y[0], CANAL_Y[1], 100):
            cheese(f"Cheese_Bridge_{x}_{y}", "CheeseBridge", (x + 50, y + 50, -50), (100, 100, 100))


# ---------------------------------------------------------------- what makes it a place: the annexes, pipes, lamps and lines, and the works beyond the wall

def pipe_x(label, folder, x0, x1, y, z, diameter, material):
    solid(label, folder, ((x0 + x1) / 2.0, y, z), (diameter, diameter, abs(x1 - x0)), material, CYLINDER, (90, 0, 0))


def pipe_y(label, folder, x, y0, y1, z, diameter, material):
    solid(label, folder, (x, (y0 + y1) / 2.0, z), (diameter, diameter, abs(y1 - y0)), material, CYLINDER, (0, 0, 90))


def lamp(label, folder, x, y, height=700):
    cylinder(label, folder, x, y, 0, 18, height, STEEL)
    box(label + "_Head", folder, (x - 60, y - 15, height - 10), (x + 60, y + 15, height + 10), PAINT["Dark"])


def annex(s, side):
    """A two-room office on each plaza, with doors either side, a window towards the yard, and a ramp to its roof, the high spot over the plaza."""
    f = f"Annex_{side}"
    X = lambda x: s * x
    x0, x1, y0, y1, h, t = 3500, 4500, -1900, -500, 400, 30
    lo, hi = min(X(x0), X(x1)), max(X(x0), X(x1))
    wall = SHEET["Cream"]
    # Walls towards the middle and towards the dock, each with a door; walls north and south, the north with a window.
    for x in (x0, x1):
        box(f"{f}_Wall_{x}_A", f, (X(x) - t / 2.0, y0, 0), (X(x) + t / 2.0, -1400, h), wall)
        box(f"{f}_Wall_{x}_B", f, (X(x) - t / 2.0, -1000, 0), (X(x) + t / 2.0, y1, h), wall)
        box(f"{f}_Wall_{x}_Over", f, (X(x) - t / 2.0, -1400, 280), (X(x) + t / 2.0, -1000, h), wall)
    box(f"{f}_Wall_South", f, (lo, y0 - t / 2.0, 0), (hi, y0 + t / 2.0, h), wall)
    wx0, wx1 = min(X(3800), X(4200)), max(X(3800), X(4200))
    box(f"{f}_Wall_North_A", f, (lo, y1 - t / 2.0, 0), (wx0, y1 + t / 2.0, h), wall)
    box(f"{f}_Wall_North_B", f, (wx1, y1 - t / 2.0, 0), (hi, y1 + t / 2.0, h), wall)
    box(f"{f}_Wall_North_Sill", f, (wx0, y1 - t / 2.0, 0), (wx1, y1 + t / 2.0, 120), wall)
    box(f"{f}_Wall_North_Lintel", f, (wx0, y1 - t / 2.0, 250), (wx1, y1 + t / 2.0, h), wall)
    # A partition with a gap, a workbench and a cabinet.
    box(f"{f}_Partition", f, (min(X(4000), X(4000) + s * 20), y0, 0), (max(X(4000), X(4000) + s * 20), -1250, 280), wall)
    prop(f"{f}_Desk", f, "Workbench", X(3800), -725, 0, 90)
    prop(f"{f}_Cabinet", f, "MetalCabinet", X(3700), -1855)
    # The roof, with a low parapet to crouch behind, reached by a ramp along the south wall.
    box(f"{f}_Roof", f, (lo - t / 2.0, y0 - t / 2.0, h), (hi + t / 2.0, y1 + t / 2.0, h + 25), FLOOR)
    parapets = (((lo - t / 2.0, y1 - 10), (hi + t / 2.0, y1 + t / 2.0)),
                ((lo - t / 2.0, y0 - t / 2.0), (lo + 10, y1 + t / 2.0)),
                ((hi - 10, y0 - t / 2.0), (hi + t / 2.0, y1 + t / 2.0)))
    for a, b in parapets:
        box(f"{f}_Parapet_{int(a[0])}_{int(a[1])}", f, (a[0], a[1], h + 25), (b[0], b[1], h + 110), WALL)
    ramp(f"{f}_Ramp", f, (X(x1), y0 - 170, 0), (X(x0) + s * 100, y0 - 170, h + 25), 260, FLOOR)


def details():
    f = "Details"
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        annex(s, side)
        # The yard: another container, barriers and lamps.
        shipping_container(f"Yard_{side}_Container_C", f"Yard_{side}", (min(X(850), X(1450)), 3150, 0), (max(X(850), X(1450)), 3400, 260), "Yellow")
        for bx, by, along in ((1200, 2850, True), (2000, 4050, False), (3100, 3700, True)):
            prop(f"Yard_{side}_Barrier_{bx}", f"Yard_{side}", "ConcreteBarrier", X(bx), by, 0, 0 if along else 90)
        for lx, ly in ((2000, 2700), (3300, 4200), (1000, 4300)):
            lamp(f"Lamp_{side}_{lx}_{ly}", f + "/Lamps", X(lx), ly)
        # The hall's main doors: a canopy over each, on posts.
        c = f"Hall/Canopy_{side}"
        box(f"Canopy_{side}", c, (min(X(2400), X(3100)), -650, 560), (max(X(2400), X(3100)), 650, 590), PAINT["Dark"])
        for py in (-620, 620):
            box(f"Canopy_{side}_Post_{py}", c, (X(3070) - 10, py - 10, 0), (X(3070) + 10, py + 10, 560), STEEL)
        # A pipe bridge from the tanks over the canal to the hall's south wall.
        pipe_y(f"PipeBridge_{side}_A", f + "/Pipes", X(1900), -4100, -1800, 560, 45, STEEL)
        pipe_y(f"PipeBridge_{side}_B", f + "/Pipes", X(2000), -4100, -1800, 560, 30, PAINT["Red"])
        for py in (-3700, -2700):
            bottom = -CANAL_DEPTH if CANAL_Y[0] < py < CANAL_Y[1] else 0
            box(f"PipeBridge_{side}_Support_{py}", f + "/Pipes", (X(1950) - 15, py - 15, bottom), (X(1950) + 15, py + 15, 530), STEEL)
            box(f"PipeBridge_{side}_Cross_{py}", f + "/Pipes", (min(X(1850), X(2050)), py - 15, 530), (max(X(1850), X(2050)), py + 15, 540), STEEL)
        # Lamps along the canal.
        for lx in (1000, 3600):
            lamp(f"Lamp_{side}_Canal_{lx}", f + "/Lamps", X(lx), -2730)
    # The pipe rack along Tank Row, between the canal and the silos.
    for y, z, d, m in ((-3820, 420, 45, STEEL), (-3880, 420, 30, PAINT["Red"]), (-3850, 480, 35, PAINT["Teal"])):
        pipe_x(f"PipeRack_{y}_{z}", f + "/Pipes", -4500, 4500, y, z, d, m)
    for px in range(-4500, 4501, 1000):
        box(f"PipeRack_Post_{px}", f + "/Pipes", (px - 12, -3862, 0), (px + 12, -3838, 400), STEEL)
        box(f"PipeRack_Arm_{px}", f + "/Pipes", (px - 12, -3920, 395), (px + 12, -3780, 405), STEEL)
    # Painted lines on the ground: the walkways along the hall.
    for y in (HALL_Y + 600, -HALL_Y - 600):
        box(f"Line_{y}", f + "/Lines", (-HALL_X - 400, y - 6, 0), (HALL_X + 400, y + 6, 0.6), PAINT["Yellow"])
    # The boundary wall: pilasters along its inside, so a long wall isn't a flat sheet.
    for x in range(-HALF_X + 800, HALF_X, 800):
        for y, d in ((HALF_Y, -1), (-HALF_Y, 1)):
            box(f"Pilaster_{x}_{y}", "Ground/Boundary", (x - 40, min(y, y + d * 60), 0), (x + 40, max(y, y + d * 60), 1000), WALL)
    for y in range(-HALF_Y + 800, HALF_Y, 800):
        if any(abs(y - door) < 300 for door in DOCK_DOORS):
            continue  # not in front of a dock's doors
        for x, d in ((HALF_X, -1), (-HALF_X, 1)):
            bottom = 120 if -2400 < y < 4400 else 0  # on the docks' platforms
            box(f"Pilaster_{x}_{y}", "Ground/Boundary", (min(x, x + d * 60), y - 40, bottom), (max(x, x + d * 60), y + 40, 1000), WALL)
    # The works beyond the wall, for a skyline: cold stores, a chimney, a battery of tall silos.
    b = "Beyond"
    box("Beyond_ColdStore_West", b, (-5200, 5700, 0), (-800, 9200, 1800), SHEET["Teal"])
    box("Beyond_ColdStore_East", b, (1400, 5900, 0), (5600, 8200, 1300), SHEET["Grey"])
    cylinder("Beyond_Chimney", b, 2600, 8800, 0, 320, 3600, WALL)
    for x in range(-5000, 5001, 1500):
        cylinder(f"Beyond_Silo_{x}", b, x, -6600, 0, 600, 2400, STEEL)
        cylinder(f"Beyond_Silo_{x}_Cap", b, x, -6600, 2400, 420, 150, STEEL)
    box("Beyond_Office_West", b, (-9800, -3000, 0), (-7400, 2500, 1500), FACADE)
    box("Beyond_Office_East", b, (7400, -1000, 0), (9600, 4000, 1100), SHEET["Grey"])
    # Ground for them, outside the wall only (under the yard it would roof the canal).
    box("Beyond_Ground_North", b, (-12000, HALF_Y + 100, -60), (12000, 12000, -40), ASPHALT)
    box("Beyond_Ground_South", b, (-12000, -10000, -60), (12000, -HALF_Y - 100, -40), ASPHALT)
    box("Beyond_Ground_West", b, (-12000, -HALF_Y - 100, -60), (-HALF_X - 100, HALF_Y + 100, -40), ASPHALT)
    box("Beyond_Ground_East", b, (HALF_X + 100, -HALF_Y - 100, -60), (12000, HALF_Y + 100, -40), ASPHALT)


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
        for i, (x, y) in enumerate(((2250, 4880), (2320, 4900), (2290, 4820))):
            prop(f"{d}_YardDrum_{i}", d, "PlasticDrum", X(x), y, 0, i * 50)
        for i, x in enumerate((1520, 1580)):
            prop(f"{d}_YardGas_{i}", d, "GasTank", X(x), 4935)
        for i, (x, y) in enumerate(((1900, 2900), (3600, 4200), (600, 4000), (4800, 3300))):
            decal(f"{d}_YardCrack_{i}", d, "AsphaltCrack", (X(x), y, 0), (40, 300, 300), (-90, i * 70, 0))
        decal(f"{d}_YardOil", d, "OilStain", (X(1000), 4700, 0), (40, 220, 220), down)
        # The plaza: a bin by the annex, barrels past its ramp, oil where loads stand.
        prop(f"{d}_AnnexBin", d, "TrashCan", X(3440), -650)
        for i, (x, y) in enumerate(((4900, -2450), (4970, -2480), (4930, -2390))):
            prop(f"{d}_PlazaBarrel_{i}", d, "Barrel", X(x), y, 0, i * 60)
        decal(f"{d}_PlazaOil_A", d, "OilStain", (X(3000), 1900, 0), (40, 200, 200), down)
        decal(f"{d}_PlazaOil_B", d, "OilStain", (X(4000), 300, 0), (40, 150, 150), down)
        # The hall: barrels and a pallet of crates in the north corners, a workbench and a cabinet under the mezzanine.
        for i, (x, y) in enumerate(((2300, 1650), (2230, 1690), (2290, 1580))):
            prop(f"{d}_HallBarrel_{i}", d, "Barrel", X(x), y, 0, i * 30)
        prop(f"{d}_HallPallet", d, "Pallet", X(2250), 1250)
        for i in range(2):
            prop(f"{d}_HallCrates_{i}", d, "PlasticCrate", X(2250), 1250, 14 + i * 26, 90)
        prop(f"{d}_HallBench", d, "Workbench", X(1000), -1700)
        prop(f"{d}_HallCabinet", d, "MetalCabinet", X(600), -1720)
        decal(f"{d}_HallOil", d, "OilStain", (X(1200), -900, 0), (40, 180, 180), down)
        # Tank Row: barrels and a gas bottle between the silos; rust leaking down the south wall.
        for i, (x, y) in enumerate(((2200, -4700), (2140, -4740))):
            prop(f"{d}_TankBarrel_{i}", d, "Barrel", X(x), y, 0, i * 45)
        prop(f"{d}_TankGas", d, "GasTank", X(2280), -4720)
        for i, x in enumerate((1800, 4600)):
            decal(f"{d}_WallLeak_{i}", d, "Leakage", (X(x), -4995, 600), (30, 500, 220), (0, -90, 90))


# ---------------------------------------------------------------- starts through the lanes, for free-for-all

def free_starts():
    """In free-for-all everyone is an enemy, and comes back at the start furthest from them all: so as not to begin and come back
    in a crowd on a dock, there are starts in every lane too, each facing the middle."""
    for s, side in ((-1, "West"), (1, "East")):
        for name, x, y in (("Plaza", 4100, 200), ("Yard", 3600, 3300), ("YardNorth", 1200, 4300), ("HallNorth", 1500, 1500),
                           ("HallSouth", 1500, -1000), ("Walkway", 900, -2250), ("Tanks", 2600, -4600)):
            start(f"Start_Free_{side}_{name}", s * x, y, 0, 0 if s < 0 else 180, "Free")


# ---------------------------------------------------------------- where the bots can walk

def navigation():
    """The bounds of the navigation mesh: the playable ground, from the canal's floor to above the annexes' roofs. The mesh itself is built when the level starts (Config/DefaultEngine.ini)."""
    volume = eas.spawn_actor_from_object(unreal.NavMeshBoundsVolume.static_class(), v(0, 0, 490))
    volume.set_actor_label("Navigation")
    volume.set_folder_path("Navigation")
    # The volume's brush is a 2 m cube.
    volume.set_actor_scale3d(v((2 * HALF_X + 400) / 200.0, (2 * HALF_Y + 400) / 200.0, 1600 / 200.0))
    origin, extent = volume.get_actor_bounds(False)
    lines.append(f"navigation bounds: {extent.x * 2:.0f} x {extent.y * 2:.0f} x {extent.z * 2:.0f} cm")
    if extent.x < HALF_X:
        raise RuntimeError("the navigation volume has no brush")


# ---------------------------------------------------------------- light, sky and air

def sky():
    f = "Lighting"
    sun = place(unreal.DirectionalLight, "Sun", f, (0, 0, 2000), (-32, -40, 0))
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_editor_property("intensity", 4.0)
    light.set_editor_property("use_temperature", True)
    light.set_editor_property("temperature", 5300.0)
    light.set_editor_property("light_source_angle", 0.8)
    light.set_editor_property("atmosphere_sun_light", True)
    atmosphere = place(unreal.SkyAtmosphere, "Sky", f, (0, 0, 0))
    atmosphere.get_component_by_class(unreal.SkyAtmosphereComponent).set_editor_property("mie_scattering_scale", 0.006)
    skylight = place(unreal.SkyLight, "SkyLight", f, (0, 0, 1500))
    sky_component = skylight.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("real_time_capture", True)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    place(unreal.VolumetricCloud, "Clouds", f, (0, 0, 0))
    fog = place(unreal.ExponentialHeightFog, "Haze", f, (0, 0, 0)).get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog.set_editor_property("fog_density", 0.012)
    fog.set_editor_property("fog_height_falloff", 0.12)
    fog.set_editor_property("start_distance", 1500.0)
    fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.32, 0.27, 0.20, 1.0))
    fog.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(0.45, 0.33, 0.18, 1.0))
    volume = place(unreal.PostProcessVolume, "PostProcess", f, (0, 0, 0))
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    for name, value in (("bloom_intensity", 0.35), ("vignette_intensity", 0.3), ("auto_exposure_bias", 0.5),
                        ("color_contrast", unreal.Vector4(1.06, 1.06, 1.06, 1.0)), ("color_saturation", unreal.Vector4(0.96, 0.96, 0.96, 1.0))):
        settings.set_editor_property(name, value)
        settings.set_editor_property("override_" + name, True)
    volume.set_editor_property("settings", settings)


try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    # Built afresh in the same level: opened and emptied if it exists, made if it doesn't.
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        opened = levels.load_level(LEVEL)
        for actor in eas.get_all_level_actors():
            if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
                eas.destroy_actor(actor)
        lines.append(f"opened and emptied {LEVEL}: {opened}")
    else:
        lines.append(f"made {LEVEL}: {levels.new_level(LEVEL, False)}")
    # Never build into another level: the editor opens on the shooter level.
    current = unreal.EditorLevelLibrary.get_editor_world().get_outermost().get_name()
    if current != LEVEL:
        raise RuntimeError(f"the level open is {current}, not {LEVEL}; nothing built")
    ground()
    for s, side in ((-1, "West"), (1, "East")):
        dock(s, side)
        yard_side(s, side)
    yard_middle()
    hall()
    south()
    details()
    dressing()
    free_starts()
    navigation()
    sky()
    lines.append("saved " + str(levels.save_current_level()))
    for name in sorted(counts):
        lines.append(f"  {name}: {counts[name]}")
    lines.append("cheese blocks: " + str(sum(n for k, n in counts.items() if k.startswith("cheese:"))))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
