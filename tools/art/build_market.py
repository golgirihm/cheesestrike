import math, os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, cylinder, solid, cheese_box, wheel, painted

# Run with the editor closed, after import_polyhaven.py (and what The Creamery needs: build_materials.py,
# cheese_material.py, wax_material.py):
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_market.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder). It writes what it built to Saved/Lvl_RindMarket.txt.
#
# Builds Rind Market (/Game/CheeseStrike/Maps/Lvl_RindMarket) from scratch, with the blocks in map_kit.py: a town
# square at dusk, about 104 by 74 m, round a stone clock tower that is never eaten; docs/unreal/maps/market.md says how
# it plays and what the mice change. Gouda come in at the west gate, Mozza at the east; three ways lead from each gate:
# High Street along the north, Main Street into the square, and the alleys along the south. The square is full of
# market stalls of cheese, and round it stand houses of cheese two storeys high: an arcaded house on the north side, a
# house with a balcony on the south, and two houses either side of Main Street with a bridge between them. The west
# half is built and mirrored for the east (x to -x); what straddles the middle is built once. Units are centimetres,
# the ground is at 0, x runs west to east and y south to north. Its surfaces and props are from Poly Haven (CC0).

LEVEL = "/Game/CheeseStrike/Maps/Lvl_RindMarket"

HALF_X, HALF_Y = 5200, 3700
EDGE_TOP = 900            # the outer wall, round the map; the outer houses' fronts stand against it
ROOF_TOP = 1150           # their roofs, where the mice come over
SQUARE_X, SQUARE_Y = 1900, 1500
TOWER = 450               # the clock tower: 9 m square
TOWER_WALL = 60
BALCONY = 540             # its top floor and balcony: 10.8 m square, at 7 m
ARCADE_Y = (1500, 1800)   # the arcade along the square's north side, the gallery over it
NORTH_Y = (1800, 2900)    # the arcade houses
HIGH_Y = (2900, 3700)     # High Street
SOUTH_Y = (-2700, -1500)  # the balcony houses
ALLEY_Y = (-3700, -2700)  # the alleys
HOUSE_X = (250, 1900)     # the arcade and balcony houses (east; x to -x for the west)
PASSAGE = 250             # the Clock Passage and the Snicket: between the houses, x from -250 to 250
EAST_X = (1900, 3300)     # the houses either side of Main Street
MAIN_Y = 300              # Main Street: y from -300 to 300
LANE_X = (3300, 4100)     # the lane inside the town wall
WALL_X = (4100, 4200)     # the town wall
WALL_TOP = 550
GATES = {"North": (3000, 3600), "Middle": (-300, 300), "South": (-3500, -2900)}
YARD_X = (4200, HALF_X)   # the gate yards, where each side starts
STOREY = 350


def team_of(s):
    return "Gouda" if s < 0 else "Mozza"


def span(s, a, b):
    """The x span from a to b (given for the east), on side s."""
    return (min(s * a, s * b), max(s * a, s * b))


def mirror(s, side):
    """A building's side on side s: east and west swap in the west."""
    return {"E": "W", "W": "E"}.get(side, side) if s < 0 else side


def M(name):
    return kit.material("MI_" + name)


def no_navigation(actor):
    """Out of the navigation mesh: an awning, a roof, a cart's load; a surface above head height that nobody walks on,
    so that no island of the mesh is made on top of it (a mouse coming in would land there)."""
    actor.static_mesh_component.set_editor_property("can_ever_affect_navigation", False)
    return actor


def model(name):
    return unreal.load_asset(f"/Game/PolyHaven/{name}/SM_{name}")


def standing(label, folder, name, x, y, z=0, yaw=0, scale=1.0):
    """A Poly Haven model at its own size (times `scale`), standing at (x, y, z), turned by `yaw`."""
    m = model(name)
    b = m.get_bounding_box()
    actor = kit.place(unreal.StaticMeshActor, label, folder, (x, y, z), (0, yaw, 0))
    actor.static_mesh_component.set_static_mesh(m)
    actor.set_actor_scale3d(kit.v(scale, scale, scale))
    # Its middle over (x, y) and its bottom on z, wherever the model's own origin is.
    mx, my = (b.min.x + b.max.x) / 2.0 * scale, (b.min.y + b.max.y) / 2.0 * scale
    t = math.radians(yaw)
    actor.set_actor_location(kit.v(x - (mx * math.cos(t) - my * math.sin(t)), y - (mx * math.sin(t) + my * math.cos(t)), z - b.min.z * scale), False, False)
    kit.count(folder.split("/")[0])
    return actor


def pitched_roof(label, folder, low, high, eaves, ridge, along_x=True, material=None, overhang=40):
    """A roof of two slopes over the block from `low` to `high` (x, y), its ridge along x (or y) at `ridge`, its
    eaves at `eaves`: two tiled slabs, out of the navigation mesh."""
    m = material or M("RoofTiles")
    (x0, y0), (x1, y1) = low, high
    rise = ridge - eaves
    half = ((y1 - y0) if along_x else (x1 - x0)) / 2.0 + overhang
    length = ((x1 - x0) if along_x else (y1 - y0)) + 2 * overhang
    slope = math.hypot(half, rise)
    pitch = math.degrees(math.atan2(rise, half))
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    for side in (-1, 1):
        # Each slope: a slab from the eave to the ridge, turned about the ridge's line.
        mid = half / 2.0
        if along_x:
            centre = (cx, cy + side * mid, eaves + rise / 2.0)
            roll = side * pitch
            no_navigation(solid(f"{label}_{side}", folder, centre, (length, slope + 10, 12), m, rotation=(0, 0, roll)))
        else:
            centre = (cx + side * mid, cy, eaves + rise / 2.0)
            no_navigation(solid(f"{label}_{side}", folder, centre, (slope + 10, length, 12), m, rotation=(-side * pitch, 0, 0)))


# ---------------------------------------------------------------- the ground and the edge

def ground():
    square = ((-SQUARE_X, -SQUARE_Y), (SQUARE_X, SQUARE_Y))
    arcade = ((-SQUARE_X, ARCADE_Y[0]), (SQUARE_X, ARCADE_Y[1]))
    # Setts laid in fans in the square, flags under the arcade, square setts everywhere else.
    kit.ground("Ground_Streets", (-HALF_X, -HALF_Y), (HALF_X, HALF_Y), M("Cobbles"), holes=[square, arcade])
    kit.ground("Ground_Square", square[0], square[1], M("Cobbles_Fan"))
    kit.ground("Ground_Arcade", arcade[0], arcade[1], M("Sandstone"))
    kit.boundary(-HALF_X, -HALF_Y, HALF_X, HALF_Y, EDGE_TOP, material=M("Plaster_Cream"))
    # Beyond the outer houses, for the view from the tower: ground, and the town going on.
    b = "Beyond"
    box("Beyond_Ground", b, (-14000, -12000, -60), (14000, 12000, -40), M("Cobbles"))
    for i, (x0, y0, x1, y1, h, colour) in enumerate((
            (-4600, 3900, -2400, 5600, 1100, "Ochre"), (-2200, 3900, 200, 5200, 1250, "Rose"), (500, 3900, 2600, 5800, 1000, "Cream"),
            (2900, 3900, 4900, 5300, 1150, "Sage"), (-4800, -5400, -2600, -3900, 1050, "Blue"), (-2300, -5800, -100, -3900, 1200, "Cream"),
            (300, -5300, 2500, -3900, 1100, "Ochre"), (2800, -5600, 4700, -3900, 1000, "Rose"), (5400, -3000, 7000, -500, 1000, "White"),
            (5400, 400, 6800, 3200, 1150, "Rose"), (-7000, -2900, -5400, -300, 1100, "Sage"), (-6800, 500, -5400, 3100, 1000, "White"))):
        box(f"Beyond_House_{i}", b, (x0, y0, -40), (x1, y1, h), M("Plaster_" + colour))
        pitched_roof(f"Beyond_House_{i}_Roof", b, (x0, y0), (x1, y1), h, h + 350, along_x=(x1 - x0) >= (y1 - y0))
    # A church's spire over the north-east roofs: a second landmark, far off.
    box("Beyond_Church", b, (6200, 5400, -40), (8200, 9400, 1500), M("Sandstone"))
    box("Beyond_Church_Tower", b, (6700, 4600, -40), (7700, 5600, 2600), M("Sandstone"))
    spire = solid("Beyond_Church_Spire", b, (7200, 5100, 2600 + 1100), (1000, 1000, 2200), M("RoofTiles"), mesh=CONE)
    no_navigation(spire)


CONE = unreal.load_asset("/Engine/BasicShapes/Cone")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")


# ---------------------------------------------------------------- the clock tower: stone, never eaten

def clock_tower():
    """The landmark and the high ground: a stone tower 9 m square in the middle of the square, never eaten. Arched
    doorways north and south, a pier between them; inside, one flight of stairs up the west wall to a landing along the
    north, and one up the east wall to the top floor at 7 m, an open loggia with a balcony all round behind a parapet
    a metre high. One way up, and up there seen from every house round the square."""
    f = "Tower"
    S = M("Sandstone")
    T, I = TOWER, TOWER - TOWER_WALL
    for sign, name in ((1, "N"), (-1, "S")):
        y0, y1 = min(sign * I, sign * T), max(sign * I, sign * T)
        box(f"Tower_Wall_{name}_W", f, (-T, y0, 0), (-110, y1, 700), S)
        box(f"Tower_Wall_{name}_E", f, (110, y0, 0), (T, y1, 700), S)
        box(f"Tower_Wall_{name}_Over", f, (-110, y0, 270), (110, y1, 700), S)
        # The doorway's surround: a band of stone proud of the wall.
        yo = sign * (T + 6)
        box(f"Tower_Door_{name}_Head", f, (-150, min(sign * T, yo), 270), (150, max(sign * T, yo), 310), S)
        for x in (-130, 130):
            box(f"Tower_Door_{name}_Jamb_{x}", f, (x - 20, min(sign * T, yo), 0), (x + 20, max(sign * T, yo), 270), S)
    box("Tower_Wall_W", f, (-T, -I, 0), (-I, I, 700), S)
    box("Tower_Wall_E", f, (I, -I, 0), (T, I, 700), S)
    # A plinth round its foot.
    for name, low, high in (("N", (-T - 15, T, 0), (T + 15, T + 15, 45)), ("S", (-T - 15, -T - 15, 0), (T + 15, -T, 45)),
                            ("W", (-T - 15, -T, 0), (-T, T, 45)), ("E", (T, -T, 0), (T + 15, T, 45))):
        if name in "NS":
            # Not across the doorways.
            box(f"Tower_Plinth_{name}_W", f, low, (-150, high[1], high[2]), S)
            box(f"Tower_Plinth_{name}_E", f, (150, low[1], low[2]), high, S)
        else:
            box(f"Tower_Plinth_{name}", f, low, high, S)
    # The pier in the middle, between the doorways: no line through the tower from door to door.
    box("Tower_Pier", f, (-80, -80, 0), (80, 80, 700), S)
    # The stairs: up the west wall to the landing along the north wall, then up the east wall to the top.
    # Each flight's foot has room in front of it (the navigation mesh needs it head on): 20 treads of 27 cm.
    kit.stairs("Tower_Stairs_0", f, (-I + 75, -300, 0), (-I + 75, 240, STOREY), 150, S)
    box("Tower_Landing", f, (-I, 240, STOREY - 25), (I, I, STOREY), S)
    kit.stairs("Tower_Stairs_1", f, (I - 75, 240, STOREY), (I - 75, -300, 700), 150, S)
    box("Tower_Stairs_1_Under", f, (I - 150, -I, 0), (I, 240, STOREY), S)
    box("Tower_Stairs_1_End", f, (I - 150, -I, STOREY), (I, -300, 700), S)
    kit.rail("Tower_LandingRail", f, (-I + 150, 240), (I - 150, 240), STOREY, material=painted("Dark"))
    # The top floor and balcony at 7 m, with the stairwell's hole; a rail round it.
    B = BALCONY
    kit.slab("Tower_Top", f, (-B, -B, 675), (B, B, 700), S, holes=[((I - 150, -300), (I, 240))])
    kit.rail("Tower_TopRail_W", f, (I - 150, -300), (I - 150, 240), 700, material=painted("Dark"))
    kit.rail("Tower_TopRail_N", f, (I - 150, 240), (I, 240), 700, material=painted("Dark"))
    # Corbels under the balcony, all round: two stepped courses.
    for k, (out, z0) in enumerate(((B, 640), (T + 45, 600))):
        box(f"Tower_Corbel_N_{k}", f, (-out, T, z0), (out, out, 675), S)
        box(f"Tower_Corbel_S_{k}", f, (-out, -out, z0), (out, -T, 675), S)
        box(f"Tower_Corbel_E_{k}", f, (T, -T, z0), (out, T, 675), S)
        box(f"Tower_Corbel_W_{k}", f, (-out, -T, z0), (-T, T, 675), S)
    # The loggia: piers at the balcony's corners carry the clock above; a parapet a metre high between them.
    P = 110
    for cx in (-1, 1):
        for cy in (-1, 1):
            box(f"Tower_Loggia_Pier_{cx}_{cy}", f, (min(cx * B, cx * (B - P)), min(cy * B, cy * (B - P)), 700),
                (max(cx * B, cx * (B - P)), max(cy * B, cy * (B - P)), 1050), S)
    for name, low, high in (("N", (-B + P, B - 20, 700), (B - P, B, 800)), ("S", (-B + P, -B, 700), (B - P, -B + 20, 800)),
                            ("E", (B - 20, -B + P, 700), (B, B - P, 800)), ("W", (-B, -B + P, 700), (-B + 20, B - P, 800))):
        box(f"Tower_Parapet_{name}", f, low, high, S)
        box(f"Tower_Parapet_{name}_Cap", f, (low[0] - 4 if name in "EW" else low[0], low[1] - 4 if name in "NS" else low[1], 800),
            (high[0] + 4 if name in "EW" else high[0], high[1] + 4 if name in "NS" else high[1], 808), S)
    # The clock stage: four faces, lit; a cornice; a spire of tiles with a ball on top.
    C = B - 20
    box("Tower_ClockStage", f, (-C, -C, 1050), (C, C, 1500), S)
    box("Tower_ClockStage_Band", f, (-B, -B, 1040), (B, B, 1065), S)
    box("Tower_Cornice", f, (-B - 20, -B - 20, 1500), (B + 20, B + 20, 1540), S)
    for name, yaw in (("N", 90), ("E", 0), ("S", -90), ("W", 180)):
        clock_face(f"Tower_Clock_{name}", f, yaw, C, 1280)
    no_navigation(solid("Tower_Spire", f, (0, 0, 1540 + 450), (2 * B, 2 * B, 900), M("RoofTiles"), mesh=CONE))
    solid("Tower_Finial_Rod", f, (0, 0, 2440 + 60), (8, 8, 120), painted("Dark"))
    solid("Tower_Finial_Ball", f, (0, 0, 2470), (40, 40, 40), painted("Yellow"), mesh=SPHERE)
    # Lanterns by the doorways, and a light in the loggia that shows who is up there.
    for sign, name in ((1, "N"), (-1, "S")):
        for x in (-230, 230):
            lantern(f"Tower_Lantern_{name}_{x}", f, x, sign * (T + 2), 230, 90 if sign > 0 else -90)
    kit.point_light("Tower_Loggia_Light", f, (0, 0, 990), intensity=6.0, radius=900.0, temperature=3000.0, shadows=False)


def clock_face(label, folder, yaw, out, z):
    """A clock's face on the tower's side facing `yaw`, `out` from the middle, at height z: a lit disc in a dark rim,
    with its hands at five to six."""
    t = math.radians(yaw)
    n = (math.cos(t), math.sin(t))
    rot = (90, yaw, 0)
    solid(f"{label}_Rim", folder, (n[0] * (out + 4), n[1] * (out + 4), z), (360, 360, 8), painted("Dark"), mesh=kit.CYLINDER, rotation=rot)
    solid(f"{label}_Face", folder, (n[0] * (out + 9), n[1] * (out + 9), z), (320, 320, 4), M("Glow_Clock"), mesh=kit.CYLINDER, rotation=rot)
    # The hands, in the face's plane: across it is (-sin, cos) of its yaw, up is z.
    across = (-n[1], n[0])
    for hand, angle, length, width in (("Minute", -30, 135, 10), ("Hour", 178, 90, 16)):
        a = math.radians(angle)
        # Seen from outside, clockwise from twelve: up, then towards the viewer's right (-across).
        direction = (-across[0] * math.sin(a), -across[1] * math.sin(a), math.cos(a))
        centre = (n[0] * (out + 13) + direction[0] * length / 2.0, n[1] * (out + 13) + direction[1] * length / 2.0, z + direction[2] * length / 2.0)
        solid(f"{label}_{hand}", folder, centre, (4, width, length), painted("Dark"), rotation=(0, yaw, angle))
    solid(f"{label}_Boss", folder, (n[0] * (out + 15), n[1] * (out + 15), z), (24, 24, 6), painted("Dark"), mesh=kit.CYLINDER, rotation=rot)


def lantern(label, folder, x, y, z, yaw, light=True):
    """A lantern on a wall at (x, y, z), its bracket out along `yaw`, lit."""
    a = standing(label, folder, "street_lamp_02", x, y, z, yaw + 90, 0.55)
    if light:
        t = math.radians(yaw)
        kit.point_light(label + "_Light", folder, (x + math.cos(t) * 30, y + math.sin(t) * 30, z + 50), intensity=6.0, radius=800.0, shadows=False)
    return a


def street_lamp(label, folder, x, y, yaw=0, shadows=False):
    """A lamp post, lit."""
    standing(label, folder, "street_lamp_01", x, y, 0, yaw)
    kit.point_light(label + "_Light", folder, (x, y, 330), intensity=20.0, radius=1500.0, shadows=shadows)


# ---------------------------------------------------------------- the houses round the square (s = -1 the west, 1 the east)

def house_style(name):
    return dict(deck=M("Planks"), frame_material=M("OakTimber"), stair_material=M("Planks"), rail_material=M("OakTimber"), door_material=M("Planks"))


def house_roof(name, low, high, eaves=740, rise=320, along_x=True, gable="Plaster_White"):
    """A tiled roof of two slopes over a house of cheese (low, high: its outer corners), on its corner posts, with a
    plastered gable at each end: it stays when the walls are eaten, over the bare frame. Out of the navigation mesh."""
    f = f"Buildings/{name}"
    (x0, y0), (x1, y1) = low, high
    pitched_roof(f"{name}_Roof", f, low, high, eaves, eaves + rise, along_x=along_x, overhang=45)
    # The gables: a cone flattened to a plate is a triangle.
    if along_x:
        for x, end in ((x0, "W"), (x1, "E")):
            no_navigation(solid(f"{name}_Gable_{end}", f, (x, (y0 + y1) / 2.0, eaves + rise / 2.0), (16, (y1 - y0), rise), M(gable), mesh=CONE))
    else:
        for y, end in ((y0, "S"), (y1, "N")):
            no_navigation(solid(f"{name}_Gable_{end}", f, ((x0 + x1) / 2.0, y, eaves + rise / 2.0), ((x1 - x0), 16, rise), M(gable), mesh=CONE))


def house_floor(name, inside):
    (x0, y0), (x1, y1) = inside
    box(f"{name}_Floor", f"Buildings/{name}", (x0, y0, 0), (x1, y1, 2), M("Planks"))


def arcade_house(s, side):
    """The arcade house on the square's north side: two storeys of cheese on a timber frame, between the Clock Passage
    and the yard. Doors onto the arcade, onto High Street and into the yard; a stair inside along the passage's side
    and one outside up from High Street; upstairs, doorways out onto the gallery over the arcade, windows over the
    square and High Street."""
    X = lambda x: s * x
    team = team_of(s)
    name = f"ArcadeHouse{team}"
    x0, x1 = span(s, *HOUSE_X)
    # Its north and south walls' columns are 275 wide from its west end: openings on their middles. The arcade side's
    # columns, from the passage out: 387.5, 662.5, 937.5, 1212.5, 1487.5, 1762.5; its ends' (y) 1975 ... 2725.
    inside = kit.building(name, name, (x0, NORTH_Y[0]), (x1, NORTH_Y[1]), storeys=2, column=275,
                          doors=[("S", X(937.5)), ("N", X(1487.5)), (mirror(s, "E"), 2225)],
                          windows=[("S", X(1487.5), 0), ("S", X(1212.5), 1), ("N", X(1212.5), 1), (mirror(s, "E"), 2475, 1)],
                          doorways=[("S", X(662.5), 1), ("S", X(1487.5), 1), ("N", X(662.5), 1)],
                          stairway=(mirror(s, "W"), 2750, 2150), **house_style(name))["inside"]
    house_floor(name, inside)
    house_roof(name, (x0, NORTH_Y[0]), (x1, NORTH_Y[1]))
    f = f"Buildings/{name}"
    # The stair outside, up from High Street along the house to a landing at the upper doorway: stone, never eaten.
    kit.stairs(f"{name}_OuterStairs", f, (X(1365), 3005, 0), (X(765), 3005, STOREY), 210, M("Sandstone"))
    box(f"{name}_Landing", f, (min(X(560), X(765)), NORTH_Y[1], 0), (max(X(560), X(765)), 3110, STOREY), M("Sandstone"))
    kit.rail(f"{name}_LandingRail_N", f, (X(560), 3105), (X(765), 3105), STOREY, material=painted("Dark"))
    kit.rail(f"{name}_LandingRail_End", f, (X(565), NORTH_Y[1]), (X(565), 3110), STOREY, material=painted("Dark"))
    # Inside: crates to fight round downstairs, cheese to crouch behind upstairs.
    standing(f"{name}_Crate_A", f, "wooden_crate_01", X(1150), 2700, 0, 90)
    standing(f"{name}_Barrel_A", f, "wine_barrel_01", X(1700), 2050, 0, 30)
    kit.cheese_stack(f"{name}_Upstairs", name, (min(X(1000), X(1200)), 2250, STOREY), (max(X(1000), X(1200)), 2450, STOREY + 100), 2, 2, 1)


def arcade(s, side):
    """The arcade along the square's north side, in front of the arcade house: stone piers and a band over them, a
    floor of planks on them and on a timber beam behind, which is the gallery upstairs. The gallery's parapet is cheese
    on a timber rail, which stays. The arcade's end by the Clock Passage is a stone wall, so no line runs along it from
    one end to the other."""
    X = lambda x: s * x
    team = team_of(s)
    f = f"Arcade/{side}"
    S = M("Sandstone")
    y0, y1 = ARCADE_Y
    box(f"Arcade_End_{side}", f, (min(X(200), X(340)), y0, 0), (max(X(200), X(340)), y1, 325), S)
    piers = [722.5, 1105, 1487.5, 1870]
    for px in piers:
        box(f"Arcade_Pier_{side}_{int(px)}", f, (X(px) - 30, y0, 0), (X(px) + 30, y0 + 60, 270), S)
        kit.frame_post(f"Arcade_Post_{side}_{int(px)}", f, X(px), y1 - 22, 0, 295, material=M("OakTimber"))
    a, b = span(s, 340, SQUARE_X)
    box(f"Arcade_Band_{side}", f, (a, y0, 270), (b, y0 + 60, 325), S)
    box(f"Arcade_Beam_{side}", f, (a, y1 - 40, 295), (b, y1, 325), M("OakTimber"))
    box(f"Arcade_Deck_{side}", f, (a, y0, 325), (b, y1, STOREY), M("Planks"))
    # The gallery's parapet: cheese a metre high along its front and its open end, on a rail that stays.
    n = 8
    for i in range(n):
        p0, p1 = 340 + (SQUARE_X - 340) * i / n, 340 + (SQUARE_X - 340) * (i + 1) / n
        lo, hi = span(s, p0, p1)
        cheese_box(f"Cheese_Gallery_{team}_{i}", f"Gallery{team}", (lo, y0, STOREY), (hi, y0 + 50, STOREY + 100))
    lo, hi = span(s, SQUARE_X - 50, SQUARE_X)
    cheese_box(f"Cheese_Gallery_{team}_End", f"Gallery{team}", (lo, y0 + 50, STOREY), (hi, y1, STOREY + 100))
    kit.rail(f"Gallery_Rail_{side}", f, (X(340), y0 + 25), (X(SQUARE_X), y0 + 25), STOREY, material=M("OakTimber"))
    kit.rail(f"Gallery_RailEnd_{side}", f, (X(SQUARE_X - 25), y0), (X(SQUARE_X - 25), y1), STOREY, material=M("OakTimber"))
    # Lamps under the arcade.
    for lx in (900, 1500):
        kit.point_light(f"Arcade_Lamp_{side}_{lx}", f, (X(lx), y0 + 150, 300), intensity=4.0, radius=700.0, shadows=False)
        solid(f"Arcade_Lamp_{side}_{lx}_Glow", f, (X(lx), y0 + 150, 312), (24, 24, 20), M("Glow_Lamp"), mesh=SPHERE)


def clock_arch():
    """Where the Clock Passage comes out under the gallery: the gallery's floor across it from one arcade to the
    other, on a stone band, with a stone parapet that stays."""
    f = "Arcade/Middle"
    S = M("Sandstone")
    y0, y1 = ARCADE_Y
    box("ClockArch_Band", f, (-340, y0, 270), (340, y0 + 60, 325), S)
    box("ClockArch_Deck", f, (-340, y0, 325), (340, y1, STOREY), M("Planks"))
    box("ClockArch_Parapet", f, (-340, y0, STOREY), (340, y0 + 40, STOREY + 100), S)
    box("ClockArch_Parapet_Cap", f, (-344, y0 - 4, STOREY + 100), (344, y0 + 44, STOREY + 108), S)
    # Nobody walks off the gallery onto the passage's roof.
    kit.rail("ClockArch_BackRail", f, (-PASSAGE, y1 - 10), (PASSAGE, y1 - 10), STOREY, material=M("OakTimber"))
    # The Clock Passage: a covered way of cheese between the arcade houses, from the square to High Street.
    kit.tunnel("ClockPassage", "ClockPassage", (0, NORTH_Y[0]), (0, NORTH_Y[1]), width=2 * PASSAGE - 100, height=300, lamps=550)


def balcony_house(s, side):
    """The balcony house on the square's south side: two storeys of cheese on a timber frame, between the Snicket and
    the court. Doors onto the square, the alleys and the court; a stair inside along the alleys' side; upstairs a
    doorway onto a balcony over the square and one onto the Snicket's bridge, windows over the square and the alleys."""
    X = lambda x: s * x
    team = team_of(s)
    name = f"BalconyHouse{team}"
    x0, x1 = span(s, *HOUSE_X)
    # Its ends' columns (y) are 275 wide from -2650: -2512.5, -2237.5, -1962.5, -1687.5.
    inside = kit.building(name, name, (x0, SOUTH_Y[0]), (x1, SOUTH_Y[1]), storeys=2, column=275,
                          doors=[("N", X(1212.5)), ("S", X(662.5)), (mirror(s, "E"), -2237.5)],
                          windows=[("N", X(662.5), 0), ("N", X(1487.5), 0), ("N", X(662.5), 1), ("N", X(1487.5), 1), ("S", X(1212.5), 1),
                                   (mirror(s, "E"), -1962.5, 1)],
                          doorways=[("N", X(1212.5), 1), (mirror(s, "W"), -1962.5, 1)],
                          stairway=("S", X(1700), X(1100)), **house_style(name))["inside"]
    house_floor(name, inside)
    house_roof(name, (x0, SOUTH_Y[0]), (x1, SOUTH_Y[1]), gable="Plaster_Cream")
    f = f"Buildings/{name}"
    # The balcony: planks on two posts, a cheese parapet on a rail that stays.
    bx0, bx1 = span(s, 1012.5, 1412.5)
    y0, y1 = SOUTH_Y[1], SOUTH_Y[1] + 200
    box(f"{name}_Balcony", f, (bx0, y0, 325), (bx1, y1, STOREY), M("Planks"))
    for px in (bx0 + 12, bx1 - 12):
        kit.frame_post(f"{name}_Balcony_Post_{int(px)}", f, px, y1 - 12, 0, 325, material=M("OakTimber"))
    cheese_box(f"Cheese_{name}_Balcony_Front_0", name, (bx0, y1 - 40, STOREY), ((bx0 + bx1) / 2.0, y1, STOREY + 100))
    cheese_box(f"Cheese_{name}_Balcony_Front_1", name, ((bx0 + bx1) / 2.0, y1 - 40, STOREY), (bx1, y1, STOREY + 100))
    cheese_box(f"Cheese_{name}_Balcony_End_0", name, (bx0, y0, STOREY), (bx0 + 40, y1 - 40, STOREY + 100))
    cheese_box(f"Cheese_{name}_Balcony_End_1", name, (bx1 - 40, y0, STOREY), (bx1, y1 - 40, STOREY + 100))
    kit.rail(f"{name}_BalconyRail_Front", f, (bx0, y1 - 20), (bx1, y1 - 20), STOREY, material=M("OakTimber"))
    for ex in (bx0 + 20, bx1 - 20):
        kit.rail(f"{name}_BalconyRail_{int(ex)}", f, (ex, y0), (ex, y1 - 20), STOREY, material=M("OakTimber"))
    standing(f"{name}_Crate", f, "wooden_crate_01", X(450), -2300, 0, 90)
    standing(f"{name}_Barrel", f, "wine_barrel_01", X(450), -1700, 0, 70)
    kit.cheese_stack(f"{name}_Upstairs", name, (min(X(600), X(800)), -2200, STOREY), (max(X(600), X(800)), -2000, STOREY + 100), 2, 2, 1)


def main_street_houses(s, side):
    """The two houses either side of Main Street, from the square to the lane inside the wall: the Inn on the north,
    the Bakery on the south, each two storeys of cheese with doors onto the square, the lane and a yard, windows down
    Main Street and over the square, and a bridge between their upper floors over the street."""
    X = lambda x: s * x
    team = team_of(s)
    x0, x1 = span(s, *EAST_X)
    west, east = mirror(s, "W"), mirror(s, "E")
    # Their long walls' columns are 280 wide from their west ends: (east) 2040, 2320, 2600, 2880, 3160; their ends'
    # (y) 275 wide: 487.5, 762.5, 1037.5, 1312.5 (and the same negative for the south one).
    for name, y_low, y_high, street, back, sign in ((f"Inn{team}", MAIN_Y, SQUARE_Y, "S", "N", 1), (f"Bakery{team}", -SQUARE_Y, -MAIN_Y, "N", "S", -1)):
        inside = kit.building(name, name, (x0, y_low), (x1, y_high), storeys=2, column=280,
                              doors=[(west, sign * 1037.5), (east, sign * 762.5), (back, X(2880))],
                              windows=[(street, X(2320), 0), (street, X(2880), 0), (street, X(2320), 1), (street, X(2880), 1),
                                       (west, sign * 762.5, 1), (west, sign * 1037.5, 1), (back, X(2320), 1)],
                              doorways=[(street, X(2600), 1)],
                              stairway=(back, X(2050), X(2650)), **house_style(name))["inside"]
        house_floor(name, inside)
        house_roof(name, (x0, y_low), (x1, y_high), gable="Plaster_Blue" if sign > 0 else "Plaster_Rose")
        f = f"Buildings/{name}"
        standing(f"{name}_Barrel_A", f, "wine_barrel_01", X(3150), sign * 1350, 0, 10)
        standing(f"{name}_Barrel_B", f, "wine_barrel_01", X(3060), sign * 1370, 0, 80)
        standing(f"{name}_Bench", f, "painted_wooden_bench", X(2300), sign * (MAIN_Y - 32), 0, 0 if sign > 0 else 180)
        kit.cheese_stack(f"{name}_Upstairs", name, (min(X(2900), X(3100)), sign * 700 - 100, STOREY), (max(X(2900), X(3100)), sign * 700 + 100, STOREY + 100), 2, 2, 1)
    # The bridge over Main Street, between their upper floors: a deck of cheese on timber beams.
    kit.bridge(f"MainBridge{team}", f"MainBridge{team}", (X(2600), -MAIN_Y, STOREY), (X(2600), MAIN_Y, STOREY), width=150, beams=2, beam=30,
               depth=25, deck=25, frame_material=M("OakTimber"))


def snicket():
    """The Snicket: the alley between the balcony houses, from the square to the alleys, with a footbridge over it
    between their upper floors."""
    kit.bridge("SnicketBridge", "SnicketBridge", (-PASSAGE, -1962.5, STOREY), (PASSAGE, -1962.5, STOREY), width=150, beams=2, beam=30,
               depth=25, deck=25, frame_material=M("OakTimber"))


# ---------------------------------------------------------------- the market: stalls of cheese under awnings

AWNINGS = ("Red", "Green", "Blue", "Ochre")


def stall(name, cx, cy, facing, colour, tall_left=True):
    """A market stall at (cx, cy), its counter towards `facing` (N, S, E or W): a timber frame under a striped awning
    (which stay), a counter of cheese, wheels on it and stacks of wheels behind it (which the mice eat). Crouch behind
    the counter; the stacks hide a standing player."""
    f = f"Market/{name}"
    structure = name
    along_x = facing in "NS"           # the counter runs along x when it faces north or south
    w, d = 300, 200                    # its length along the counter, and its depth
    hx, hy = (w / 2.0, d / 2.0) if along_x else (d / 2.0, w / 2.0)
    fx, fy = {"N": (0, 1), "S": (0, -1), "E": (1, 0), "W": (-1, 0)}[facing]
    timber = M("OakTimber")
    for i, (px, py) in enumerate(((-hx, -hy), (hx, -hy), (-hx, hy), (hx, hy))):
        box(f"{name}_Post_{i}", f, (cx + px - 5, cy + py - 5, 0), (cx + px + 5, cy + py + 5, 262), timber)
    # The awning, and a valance along its front.
    stripes = ("" if along_x else "_V")
    no_navigation(box(f"{name}_Awning", f, (cx - hx - 30, cy - hy - 30, 262), (cx + hx + 30, cy + hy + 30, 266), M(f"Awning_{colour}{stripes}")))
    vx, vy = cx + fx * (hx + 30), cy + fy * (hy + 30)
    if along_x:
        no_navigation(box(f"{name}_Valance", f, (cx - hx - 30, vy - 2, 230), (cx + hx + 30, vy + 2, 262), M(f"Awning_{colour}{stripes}")))
    else:
        no_navigation(box(f"{name}_Valance", f, (vx - 2, cy - hy - 30, 230), (vx + 2, cy + hy + 30, 262), M(f"Awning_{colour}{stripes}")))
    # The counter: two blocks along the front, a metre high.
    for k in range(2):
        a, b = -w / 2.0 + k * w / 2.0, -w / 2.0 + (k + 1) * w / 2.0
        if along_x:
            low, high = (cx + a, cy + (hy - 70 if fy > 0 else -hy), 0), (cx + b, cy + (hy if fy > 0 else -hy + 70), 100)
        else:
            low, high = (cx + (hx - 70 if fx > 0 else -hx), cy + a, 0), (cx + (hx if fx > 0 else -hx + 70), cy + b, 100)
        cheese_box(f"Cheese_{name}_Counter_{k}", structure, low, high)
        # A small wheel on each half of the counter.
        wheel(f"Cheese_{name}_Display_{k}", structure, (low[0] + high[0]) / 2.0, (low[1] + high[1]) / 2.0, 100, 46, 24)
    # Stacks of wheels at the back: three high on one side, two on the other.
    for k, layers in ((0, 3 if tall_left else 2), (1, 2 if tall_left else 3)):
        a = (-w / 4.0, w / 4.0)[k]
        bx = cx - fx * (hx - 50) + (a if along_x else 0)
        by = cy - fy * (hy - 50) + (0 if along_x else a)
        kit.wheel_stack(f"{name}_Stack_{k}", structure, bx, by, 0, 84, 62, layers)


def market():
    i = 0
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        team = team_of(s)
        towards = "W" if s > 0 else "E"
        for k, (x, y, facing) in enumerate(((1250, 0, towards), (800, 950, "S"), (800, -950, "N"), (1550, 900, towards),
                                            (1550, -900, towards), (350, 1100, "S"), (350, -1100, "N"))):
            stall(f"Stall{team}{k}", X(x), y, facing, AWNINGS[i % len(AWNINGS)], tall_left=(k % 2 == 0))
            i += 1
        # Baskets and crates by the stalls, barrels and benches round the edge.
        f = f"Market/Dressing_{side}"
        for j, (x, y, yaw) in enumerate(((1100, 260, 20), (650, 1150, 70), (1700, -1150, 10), (520, -980, 50))):
            standing(f"Market_Basket_{side}_{j}", f, "wicker_basket_01", X(x), y, 0, yaw, 1.6)
        for j, (x, y, yaw) in enumerate(((1780, 1300, 15), (1650, -1350, 80), (980, 1350, 40))):
            standing(f"Market_Crate_{side}_{j}", f, "wooden_crate_01", X(x), y, 0, yaw)
        standing(f"Market_Bench_{side}_0", f, "painted_wooden_bench", X(700), 470, 0, 90 if s > 0 else -90)
        standing(f"Market_Bench_{side}_1", f, "painted_wooden_bench", X(700), -470, 0, 90 if s > 0 else -90)
        # Lamps: at the tower's corners (shadowed: the middle's light), and at the square's corners.
        street_lamp(f"Lamp_Tower_{side}_N", "Lamps", X(720), 720, shadows=True)
        street_lamp(f"Lamp_Tower_{side}_S", "Lamps", X(720), -720, shadows=True)
        street_lamp(f"Lamp_Square_{side}_N", "Lamps", X(1750), 1320)
        street_lamp(f"Lamp_Square_{side}_S", "Lamps", X(1750), -1320)


# ---------------------------------------------------------------- High Street: the north lane

def high_street():
    """High Street, along the north: 8 m wide, from gate to gate, with the well house in its middle in front of the
    Clock Passage, the arcade houses' stairs up from it and a shed against the far side at each: no line runs along it
    from one end to the other, and its long lines are on either side of the middle."""
    f = "HighStreet"
    S = M("Sandstone")
    # The well house: stone walls, a door, a tiled roof.
    box("WellHouse", f, (-300, 3100, 0), (300, 3450, 300), S)
    box("WellHouse_Door", f, (-60, 3094, 0), (60, 3100, 210), M("Planks"))
    box("WellHouse_Window_W", f, (-300 - 6, 3220, 120), (-300, 3330, 210), painted("Dark"))
    box("WellHouse_Window_E", f, (300, 3220, 120), (306, 3330, 210), painted("Dark"))
    pitched_roof("WellHouse_Roof", f, (-300, 3100), (300, 3450), 300, 470, along_x=True)
    lantern("WellHouse_Lantern", f, 0, 3092, 240, -90)
    for s, side in ((-1, "West"), (1, "East")):
        X = lambda x: s * x
        team = team_of(s)
        # The shed against the far houses, opposite the arcade house's stairs.
        x0, x1 = span(s, 600, 1000)
        box(f"Shed_{side}", f, (x0, 3450, 0), (x1, 3700, 230), M("Planks"))
        pitched_roof(f"Shed_{side}_Roof", f, (x0, 3450), (x1, 3700), 230, 330, along_x=True, overhang=25)
        standing(f"Shed_{side}_Barrel_0", f, "wine_barrel_01", X(1100), 3620, 0, 15)
        standing(f"Shed_{side}_Barrel_1", f, "wine_barrel_01", X(1180), 3570, 0, 75)
        # A cart of cheese by the lane's end, and one by the yard.
        cart(f"Cart_{side}_Lane", f"HighStreetCart{team}", X(3000), 3480, 0 if s > 0 else 180)
        cart(f"Cart_{side}_Yard", f"YardCart{team}", X(2900), 2500, 90)
        # Lanterns along the street on the outer houses.
        for lx in (1800, 3000):
            lantern(f"HighStreet_Lantern_{side}_{lx}", f, X(lx), HALF_Y - 2, 300, -90)


def cart(label, structure, x, y, yaw, cheese=True):
    """A handcart: a bed of planks on two wheels, its shafts to the ground, loaded with six wheels of cheese (or, not
    `cheese`, with crates, which stay)."""
    f = f"Carts/{label}"
    t = math.radians(yaw)
    ax, ay = math.cos(t), math.sin(t)       # along the cart, towards its shafts
    cx, cy = -ay, ax                         # across it
    bed = solid(f"{label}_Bed", f, (x, y, 85), (240, 140, 12), M("Planks"), rotation=(0, yaw, 0))
    for side in (-1, 1):
        solid(f"{label}_Side_{side}", f, (x + cx * side * 66, y + cy * side * 66, 110), (240, 8, 40), M("Planks"), rotation=(0, yaw, 0))
        solid(f"{label}_Wheel_{side}", f, (x + cx * side * 82, y + cy * side * 82, 55), (110, 110, 10), M("OakTimber"), mesh=kit.CYLINDER, rotation=(90, yaw + 90, 0))
        solid(f"{label}_Shaft_{side}", f, (x + ax * 170 + cx * side * 50, y + ay * 170 + cy * side * 50, 45), (180, 8, 8), M("OakTimber"), rotation=(-26, yaw, 0))
    if not cheese:
        for i, along in enumerate((-55, 50)):
            standing(f"{label}_Crate_{i}", f, "wooden_crate_01", x + ax * along, y + ay * along, 91, yaw + 90)
        standing(f"{label}_Crate_Top", f, "wooden_crate_01", x, y, 126, yaw + 80)
        return
    for i, along in enumerate((-70, 0, 70)):
        for layer in (0, 1):
            wheel(f"Cheese_{label}_{i}_{layer}", structure, x + ax * along, y + ay * along, 91 + layer * 42, 66 if layer == 0 else 58, 42)


# ---------------------------------------------------------------- the yards, the courts and the alleys

def yard(s, side):
    """The yard at the arcade's end, between the arcade house, the Inn, High Street and the lane: open, with cheese
    waiting on pallets."""
    X = lambda x: s * x
    team = team_of(s)
    f = f"Yard_{side}"
    for name, x, y in (("A", 2350, 2200), ("B", 2800, 1850)):
        box(f"Yard_{side}_Pallet_{name}", f, (X(x) - 90, y - 90, 0), (X(x) + 90, y + 90, 14), M("Planks"))
        for layer in (0, 1):
            for dx in (-45, 45):
                for dy in (-45, 45):
                    wheel(f"Cheese_Yard_{team}_{name}_{layer}_{dx}_{dy}", f"YardCheese{team}", X(x) + dx, y + dy, 14 + layer * 60, 86, 60)
    standing(f"Yard_{side}_Crate_0", f, "wooden_crate_01", X(3150), 1620, 0, 0)
    standing(f"Yard_{side}_Crate_1", f, "wooden_crate_01", X(3150), 1620, 35, 0)
    street_lamp(f"Lamp_Yard_{side}", "Lamps", X(3180), 2780)


def court(s, side):
    """The court behind the balcony house, between it, the Bakery, the alleys and the lane: a cheese store of one
    storey in its middle, with a door at each end, so the ways round it are narrow."""
    X = lambda x: s * x
    team = team_of(s)
    name = f"Store{team}"
    x0, x1 = span(s, 2350, 3050)
    kit.building(name, name, (x0, -2400), (x1, -1800), storeys=1, column=175,
                 doors=[("W", -2100, 120), ("E", -2100, 120)], windows=[("N", X(2612.5), 0, 150)],
                 frame_material=M("OakTimber"), door_material=M("Planks"), deck=M("Planks"))
    house_roof(name, (x0, -2400), (x1, -1800), eaves=390, rise=220, gable="Plaster_Grey")
    f = f"Court_{side}"
    kit.wheel_stack(f"CourtWheels_{team}", f"CourtCheese{team}", X(2050), -2550, 0, 90, 66, 3)
    kit.wheel_stack(f"CourtWheels_{team}_B", f"CourtCheese{team}", X(2150), -2580, 0, 90, 66, 2)
    standing(f"Court_{side}_Barrel_0", f, "wine_barrel_01", X(3200), -1600, 0, 40)
    standing(f"Court_{side}_Barrel_1", f, "wine_barrel_01", X(3120), -1590, 0, 120)
    street_lamp(f"Lamp_Court_{side}", "Lamps", X(3180), -2600)


def alleys(s, side):
    """The alleys along the south: a woodshed against the balcony house, a drying rack of cheese wheels, and a covered
    way of cheese along the outer houses to the lane. With the wash house in the middle, no line runs along them."""
    X = lambda x: s * x
    team = team_of(s)
    f = f"Alleys/{side}"
    # The woodshed: planks and a tiled roof, never eaten.
    x0, x1 = span(s, 1300, 1700)
    box(f"Woodshed_{side}", f, (x0, -3200, 0), (x1, -2700, 250), M("Planks"))
    pitched_roof(f"Woodshed_{side}_Roof", f, (x0, -3200), (x1, -2700), 250, 400, along_x=False, overhang=25)
    # The drying rack: a timber frame of three shelves of wheels across the alley's south half.
    rx0, rx1 = span(s, 790, 910)
    timber = M("OakTimber")
    for py in (-3650, -3400, -3150):
        for px in (rx0 + 5, rx1 - 5):
            box(f"Rack_{side}_Post_{py}_{int(px)}", f, (px - 5, py - 5, 0), (px + 5, py + 5, 300), timber)
    for z in (10, 110, 210):
        box(f"Rack_{side}_Shelf_{z}", f, (rx0, -3650, z), (rx1, -3150, z + 10), M("Planks"))
        for k, y in enumerate(range(-3600, -3150, 100)):
            wheel(f"Cheese_Rack_{team}_{z}_{k}", f"DryingRack{team}", (rx0 + rx1) / 2.0, y + 0, z + 10, 92, 80)
    for py in (-3650, -3150):
        box(f"Rack_{side}_Top_{py}", f, (rx0, py - 5, 290), (rx1, py + 5, 300), timber)
    # The covered way along the outer houses, from the court's corner to the lane.
    north = "L" if s > 0 else "R"
    kit.tunnel(f"Tunnel{team}", f"Tunnel{team}", (X(2200), -3500), (X(3300), -3500), width=400, height=300, sides=(north,),
               openings=[(north, 450, 650)], ledger_material=timber, lamps=550)
    standing(f"Alleys_{side}_Crate", f, "wooden_crate_01", X(1100), -3640, 0, 0)
    for lx in (1100, 2000):
        lantern(f"Alleys_Lantern_{side}_{lx}", f, X(lx), -HALF_Y + 2, 300, 90)


def wash_house():
    f = "Alleys/Middle"
    S = M("Sandstone")
    box("WashHouse", f, (-300, -3700, 0), (300, -3100, 320), S)
    box("WashHouse_Door", f, (-70, -3100, 0), (70, -3094, 220), M("Planks"))
    pitched_roof("WashHouse_Roof", f, (-300, -3700), (300, -3100), 320, 520, along_x=False)
    lantern("WashHouse_Lantern", f, 160, -3092, 250, 90)


# ---------------------------------------------------------------- the town wall, its gates and the yards behind it

def town_wall(s, side):
    """The old town wall across each end, with three gates, one for each way in; behind it the gate yard where a side
    starts, and inside each gate a wall that hides the yard from whoever looks in."""
    X = lambda x: s * x
    team = team_of(s)
    f = f"TownWall_{side}"
    W = M("TownWall")
    S = M("Sandstone")
    x0, x1 = span(s, *WALL_X)
    edges = sorted([-HALF_Y, HALF_Y] + [y for g in GATES.values() for y in g])
    for i in range(0, len(edges), 2):
        box(f"TownWall_{side}_{i}", f, (x0, edges[i], 0), (x1, edges[i + 1], WALL_TOP), W)
    for gate, (g0, g1) in GATES.items():
        box(f"TownWall_{side}_{gate}_Over", f, (x0, g0, 400), (x1, g1, WALL_TOP), W)
        # Its surround, in stone, on the town's side.
        inner = X(WALL_X[0])
        lo, hi = span(s, WALL_X[0] - 12, WALL_X[0])
        box(f"Gate_{side}_{gate}_Head", f, (lo, g0 - 40, 400), (hi, g1 + 40, 450), S)
        for y in (g0 - 20, g1 + 20):
            box(f"Gate_{side}_{gate}_Jamb_{y}", f, (lo, y - 20, 0), (hi, y + 20, 400), S)
        lantern(f"Gate_{side}_{gate}_Lantern", f, inner - s * 14, g1 + 70, 300, 180 if s > 0 else 0)
    box(f"TownWall_{side}_Coping", f, (x0 - 10, -HALF_Y, WALL_TOP), (x1 + 10, HALF_Y, WALL_TOP + 25), S)
    # The walls in the yard behind each gate.
    bx0, bx1 = span(s, 4450, 4600)
    for gate, (y0, y1) in (("North", (2650, HALF_Y)), ("Middle", (-450, 450)), ("South", (-HALF_Y, -2650))):
        box(f"Baffle_{side}_{gate}", f, (bx0, y0, 0), (bx1, y1, 250), W)
        box(f"Baffle_{side}_{gate}_Cap", f, (bx0 - 8, y0, 250), (bx1 + 8, y1, 270), S)
    # The fountain at the end of Main Street.
    fx = X(3700)
    cylinder(f"Fountain_{side}_Basin", f, fx, 0, 0, 380, 90, S)
    cylinder(f"Fountain_{side}_Water", f, fx, 0, 80, 340, 12, painted("Teal"))
    cylinder(f"Fountain_{side}_Column", f, fx, 0, 90, 50, 150, S)
    cylinder(f"Fountain_{side}_Bowl", f, fx, 0, 240, 130, 22, S)
    # Barrels and crates along the wall.
    for j, (y, yaw) in enumerate(((1800, 10), (1880, 60), (-2200, 30))):
        standing(f"Lane_{side}_Barrel_{j}", f, "wine_barrel_01", X(4040), y, 0, yaw)
    standing(f"Lane_{side}_Crate", f, "wooden_crate_01", X(4040), -1300, 0, 90)
    # The yard: starts for the side, all facing the town; carts and barrels against the outer houses.
    yaw = 180 if s > 0 else 0
    kit.team_starts(team, [(X(4850), y) for y in (-2500, -1900, -1300, -700, 700, 1300, 1900, 2500)], yaw, name="Gate_")
    for j, y in enumerate((-3200, 3200, 0)):
        standing(f"Yard_{side}_Barrel_{j}", f, "wine_barrel_01", X(5120), y, 0, j * 40)
    standing(f"Yard_{side}_Bench", f, "painted_wooden_bench", X(5140), 1600, 0, -90 if s > 0 else 90)
    # Behind the walls at the yard's ends: a cart of crates, barrels.
    cart(f"Cart_{side}_Gate_N", None, X(4900), 3250, 90, cheese=False)
    cart(f"Cart_{side}_Gate_S", None, X(4950), -3300, -90, cheese=False)
    for j, (x, y) in enumerate(((4700, 3550), (4780, 3600), (4720, -3560))):
        standing(f"Yard_{side}_EndBarrel_{j}", f, "wine_barrel_01", X(x), y, 0, j * 50)
    lantern(f"Yard_{side}_Lantern_N", f, X(HALF_X) - s * 4, 2200, 300, 180 if s > 0 else 0)
    lantern(f"Yard_{side}_Lantern_S", f, X(HALF_X) - s * 4, -2200, 300, 180 if s > 0 else 0)
    standing(f"Yard_{side}_Bench_B", f, "painted_wooden_bench", X(5140), -1600, 0, -90 if s > 0 else 90)


# ---------------------------------------------------------------- the outer houses: plaster fronts all round

FRONTS = ("Ochre", "Grey", "Rose", "White", "Sage", "Cream", "Blue")


def fronts(name, start, end, fixed, facing, widths, heights, lit, plain=False):
    """House fronts along the outer wall, from `start` to `end` along x (on the north or south wall, at y = `fixed`)
    or y (on the east or west, at x = `fixed`), facing `facing` (yaw into the map): each a plaster front of its own
    colour and height, with a cornice and a tiled roof running back over the wall, a door and a window below and two
    windows above (some lit, some with shutters); `plain`, only the windows above."""
    f = f"Fronts/{name}"
    t = math.radians(facing)
    n = (round(math.cos(t)), round(math.sin(t)))          # out of the front, into the map
    along_x = n[0] == 0
    position = start
    i = 0
    while position < end - 50:
        w = min(widths[i % len(widths)], end - position)
        h = heights[i % len(heights)]
        colour = FRONTS[(i + len(name)) % len(FRONTS)]
        a, b = position, position + w

        def block(label, u0, u1, depth0, depth1, z0, z1, m):
            # u along the front, depth out from the wall into the map.
            d0, d1 = fixed + n[0] * depth0 + n[1] * depth0, fixed + n[0] * depth1 + n[1] * depth1
            if along_x:
                return box(label, f, (u0, min(d0, d1), z0), (u1, max(d0, d1), z1), m)
            return box(label, f, (min(d0, d1), u0, z0), (max(d0, d1), u1, z1), m)

        block(f"{name}_{i}_Front", a, b, 0, 12, 0, h, M("Plaster_" + colour))
        block(f"{name}_{i}_Cornice", a, b, 0, 30, h - 30, h, M("OakTimber"))
        # The roof: a slope from the cornice back over the outer wall.
        depth = 280
        centre_out = fixed - (n[0] + n[1]) * (depth / 2.0 - 30)
        pitch = 35
        if along_x:
            c = ((a + b) / 2.0, centre_out, h + math.tan(math.radians(pitch)) * depth / 2.0 - 10)
            no_navigation(solid(f"{name}_{i}_Roof", f, c, (w + 20, depth / math.cos(math.radians(pitch)), 12), M("RoofTiles"), rotation=(0, 0, pitch * n[1])))
        else:
            c = (centre_out, (a + b) / 2.0, h + math.tan(math.radians(pitch)) * depth / 2.0 - 10)
            no_navigation(solid(f"{name}_{i}_Roof", f, c, (depth / math.cos(math.radians(pitch)), w + 20, 12), M("RoofTiles"), rotation=(-pitch * n[0], 0, 0)))
        spots = [(a + w * 0.28, 430, 1), (a + w * 0.72, 430, 2)]
        if not plain:
            door_at = a + w * (0.3 if i % 2 else 0.7)
            block(f"{name}_{i}_Door", door_at - 60, door_at + 60, 12, 16, 0, 230, M("Planks"))
            spots.append((a + w * (0.7 if i % 2 else 0.3), 110, 0))
        shutters = M("Shutters_Green") if i % 4 == 0 else M("Shutters_Blue") if i % 4 == 2 else None
        for k, (u, z, which) in enumerate(spots):
            glow = (i * 3 + which) in lit
            block(f"{name}_{i}_Window_{k}", u - 50, u + 50, 12, 16, z, z + 150, M("Glow_Window") if glow else painted("Dark"))
            if shutters is not None and not glow and z > 300 and k == 0:
                # Shutters, open either side.
                block(f"{name}_{i}_Shutter_{k}_L", u - 98, u - 52, 12, 18, z, z + 150, shutters)
                block(f"{name}_{i}_Shutter_{k}_R", u + 52, u + 98, 12, 18, z, z + 150, shutters)
        position = b
        i += 1


def outer_houses():
    # Along High Street and the alleys, all the way between the town walls; plainer behind the gate yards.
    lit = {1, 4, 6, 11, 14, 17, 19, 22, 27, 31, 33}
    fronts("North", -WALL_X[0], WALL_X[0], HALF_Y, -90, (900, 760, 1000, 820, 940), (920, 1000, 900, 1060, 960), lit)
    fronts("South", -WALL_X[0], WALL_X[0], -HALF_Y, 90, (860, 980, 760, 1020, 880), (960, 900, 1040, 920, 980), lit)
    for s, side in ((-1, "West"), (1, "East")):
        fronts(f"Yard{side}", -HALF_Y, HALF_Y, s * HALF_X, 180 if s > 0 else 0, (900, 1100, 1000, 1200), (900, 980, 940), {2, 7, 10}, plain=True)


# ---------------------------------------------------------------- what a match needs: starts, the mice's ways in, places

def free_starts():
    """In free-for-all everyone comes back at the start furthest from them all: starts through every part of the town,
    so as not to begin and come back in a crowd at the gates. Each faces the middle."""
    points = []
    for s, side in ((-1, "West"), (1, "East")):
        for name, x, y in (("HighStreet", 2600, 3300), ("Yard", 2550, 2600), ("Court", 2150, -1700), ("Alleys", 1150, -2950),
                           ("Square", 1300, -650), ("Arcade", 1300, 1650), ("Lane", 3700, 1800)):
            points.append((s * x, y, 0 if s < 0 else 180, 0, f"{side}_{name}"))
    kit.free_starts(points)


def mouse_entries():
    """Where the mice come in: over the rooftops into the square (off the balcony houses and the
    houses on Main Street), so that they start on the stalls; and over the outer houses into High Street and the
    alleys. Each lands 5 m in, on open ground: nothing raised on the navigation mesh within 3 m (the awnings and
    roofs are off it), or the mouse would land on it."""
    for s in (-1, 1):
        kit.mouse_entry(s * 600, SOUTH_Y[1] - 25, 760, 90)                 # the balcony houses' roofs
        kit.mouse_entry(s * SQUARE_X, 900, 760, 0 if s < 0 else 180)        # the Inns'
        kit.mouse_entry(s * SQUARE_X, -900, 760, 0 if s < 0 else 180)       # the Bakeries'
        kit.mouse_entry(s * 2000, HALF_Y + 50, ROOF_TOP, -90)              # over High Street
        kit.mouse_entry(s * 1850, -HALF_Y - 50, ROOF_TOP, 90)              # over the alleys


def places():
    """What players call each part of the town (for the HUD and the kill feed)."""
    for s, side in ((-1, "West"), (1, "East")):
        team = team_of(s)

        def area(name, x0, y0, x1, y1, z0=-100, z1=1200):
            a, b = span(s, x0, x1)
            kit.place_name(name, (a, y0, z0), (b, y1, z1))
        area(f"{team} Gate", WALL_X[0], -HALF_Y, HALF_X, HALF_Y)
        area(f"{team} Wall", LANE_X[0], -HALF_Y, WALL_X[0], HALF_Y)
        area(f"{team} Arcade House", HOUSE_X[0], NORTH_Y[0], HOUSE_X[1], NORTH_Y[1], 0, 750)
        area(f"{team} Balcony House", HOUSE_X[0], SOUTH_Y[0], HOUSE_X[1], SOUTH_Y[1], 0, 750)
        area(f"{team} Balcony", 1012.5, SOUTH_Y[1], 1412.5, SOUTH_Y[1] + 200, 300, 750)
        area(f"{team} Inn", EAST_X[0], MAIN_Y, EAST_X[1], SQUARE_Y, 0, 750)
        area(f"{team} Bakery", EAST_X[0], -SQUARE_Y, EAST_X[1], -MAIN_Y, 0, 750)
        area(f"{team} Bridge", 2500, -MAIN_Y, 2700, MAIN_Y, 300, 750)
        area(f"{team} Street", EAST_X[0], -MAIN_Y, EAST_X[1], MAIN_Y)
        area(f"{team} Yard", EAST_X[0], SQUARE_Y, LANE_X[0], HIGH_Y[0])
        area(f"{team} Court", EAST_X[0], ALLEY_Y[1], LANE_X[0], -SQUARE_Y)
        area(f"{team} Store", 2350, -2400, 3050, -1800, 0, 400)
        area(f"{team} Alleys", PASSAGE, -HALF_Y, LANE_X[0], ALLEY_Y[1])
        area(f"{team} Tunnel", 2200, -HALF_Y, 3300, -3250, 0, 330)
        area(f"{team} High Street", 300, HIGH_Y[0], LANE_X[0], HALF_Y)
        area(f"{team} Stairs", 560, NORTH_Y[1], 1365, 3110, 0, 750)
        area(f"{team} Arcade", 340, ARCADE_Y[0], SQUARE_X, ARCADE_Y[1], -100, 330)
        area(f"{team} Gallery", 340, ARCADE_Y[0], SQUARE_X, ARCADE_Y[1], 330, 750)
    kit.place_name("The Square", (-SQUARE_X, -SQUARE_Y, -100), (SQUARE_X, SQUARE_Y, 1200))
    kit.place_name("The Clock Tower", (-TOWER, -TOWER, -100), (TOWER, TOWER, 680))
    kit.place_name("The Belfry", (-BALCONY, -BALCONY, 680), (BALCONY, BALCONY, 1100))
    kit.place_name("The Clock Passage", (-PASSAGE, NORTH_Y[0], -100), (PASSAGE, NORTH_Y[1], 330))
    kit.place_name("The Clock Arch", (-340, ARCADE_Y[0], -100), (340, ARCADE_Y[1], 750))
    kit.place_name("The Well House", (-300, HIGH_Y[0], -100), (300, HALF_Y, 600))
    kit.place_name("The Snicket", (-PASSAGE, ALLEY_Y[1], -100), (PASSAGE, SOUTH_Y[1], 1200))
    kit.place_name("The Snicket Bridge", (-PASSAGE, -2050, 300), (PASSAGE, -1875, 750))
    kit.place_name("The Wash House", (-300, -HALF_Y, -100), (300, -2700, 600))


def dusk():
    """The kit's dusk, made a town's evening: a low sun from the south-west, a clear sky, little haze, and the picture
    a little darker, so that the lamps and lit windows read."""
    kit.light_rig("dusk", sun=(-7, 65))
    for actor in kit.eas.get_all_level_actors():
        label = actor.get_actor_label()
        if label == "Sun":
            light = actor.get_component_by_class(unreal.DirectionalLightComponent)
            light.set_editor_property("intensity", 1.3)
            light.set_editor_property("temperature", 3000.0)
        elif label == "SkyLight":
            actor.get_component_by_class(unreal.SkyLightComponent).set_editor_property("intensity", 1.0)
        elif label == "Sky":
            actor.get_component_by_class(unreal.SkyAtmosphereComponent).set_editor_property("mie_scattering_scale", 0.003)
        elif label == "Haze":
            fog = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
            fog.set_editor_property("fog_density", 0.006)
            fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.05, 0.07, 0.12, 1.0))
            fog.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(0.45, 0.25, 0.1, 1.0))
        elif label == "PostProcess":
            settings = actor.get_editor_property("settings")
            for key, value in (("auto_exposure_bias", -0.7), ("white_temp", 5800.0), ("color_saturation", unreal.Vector4(0.92, 0.92, 0.95, 1.0))):
                settings.set_editor_property(key, value)
                settings.set_editor_property("override_" + key, True)
            actor.set_editor_property("settings", settings)


def hills():
    """King of the Hill's spots, on the middle line or in twins: the square north of the tower (by its north door),
    High Street west of the well house, the square south of the tower, and High Street east of it. Cobbles, never eaten."""
    kit.hill_spots([("SquareNorth", 0, 760, 0, 320), ("HighStreetGouda", -1750, 3300, 0, 350), ("SquareSouth", 0, -760, 0, 320),
                    ("HighStreetMozza", 1750, 3300, 0, 350)])


def pickups():
    """The weapons' stands (kit.pickup): the Roquefort in the belfry, seen from every house round the square; the
    Munster in the Clock Passage, the choke between High Street and the square; a Cheddar Blaster on each side's gallery
    over the arcade and a Swiss Shotgun in each side's store in the court."""
    kit.pickup("roquefort", 0, -250, 700, "Belfry")
    kit.pickup("munster", 0, 2350, 0, "ClockPassage")
    for s, side in ((-1, "West"), (1, "East")):
        kit.pickup("blaster", s * 1100, 1680, STOREY, f"Gallery{side}")
        kit.pickup("shotgun", s * 2700, -2100, 0, f"Store{side}")


def build():
    ground()
    clock_tower()
    clock_arch()
    snicket()
    for s, side in ((-1, "West"), (1, "East")):
        arcade_house(s, side)
        arcade(s, side)
        balcony_house(s, side)
        main_street_houses(s, side)
        yard(s, side)
        court(s, side)
        alleys(s, side)
        town_wall(s, side)
    wash_house()
    high_street()
    market()
    outer_houses()
    free_starts()
    mouse_entries()
    places()
    hills()
    pickups()
    kit.nav_bounds((-HALF_X - 100, -HALF_Y - 100, -100), (HALF_X + 100, HALF_Y + 100, 1150))
    dusk()


kit.build_level(LEVEL, build)
