import os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, fill, mesh, place, painted

# Run with the editor closed, after import_fab.py, build_materials.py, cheese_material.py, wax_material.py and
# grotto_materials.py:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_grotto.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it built to Saved/Lvl_Grotto.txt.
#
# Builds Gruyère Grotto (/Game/CheeseStrike/Maps/Lvl_Grotto) from scratch, with the blocks in map_kit.py: a cave
# about 88 by 56 m, lit by lamps and glowing pools, on three levels: the cave's floor (the Pools), ledges 4 m up
# (the south Ledge, the Pulpit in the north middle) and galleries 8 m up along the north wall, joined by rock-cut
# stairs and two rope bridges through an arch in the Spine, the rock column in the middle. The upper levels are
# widened by decks of cheese slabs, each held on one pillar of cheese (kit.pillar_deck): the mice eat the pillars,
# and the slabs fall onto the level below, so the cave loses its upper floors over a match and the fight drops to
# the bottom. What never goes is rock: the floor, the walks along the walls, the stairs, the Pulpit, the landing and
# the Spine, and the bridges' waxed beams. docs/unreal/maps/grotto.md says how it plays.
#
# Each side's spawn is a hollow at its end of the cave, at the middle level, walled off from it but for three ways
# out: up to its gallery, down to the floor, and along the south Ledge. The west half is built and mirrored for the
# east (x to -x); what straddles the middle is built once. Units are centimetres, the floor is at 0, x runs west to
# east and y south to north.

LEVEL = "/Game/CheeseStrike/Maps/Lvl_Grotto"

# The cave, inside its walls.
HALF_X = 4600
HALF_Y = 2800
L1 = 400            # the middle level: the Ledge, the Pulpit, the landing, the hollows
L2 = 800            # the galleries
CEILING = 1900
# The floor runs between the hollows, which are rock at L1 from FLOOR_X out to the cave's ends.
FLOOR_X = 3400
HOLLOW_WALL = (3400, 3600)      # the hollow's front wall, rock from L1 to the ceiling
HOLLOW_DOOR = 400               # its middle opening, y from -400 to 400
# The north: a shelf of rock along the wall at L1, the gallery walk on it at L2, and decks of cheese south of it.
SHELF_Y = (2300, HALF_Y)
GALLERY_X = (1800, FLOOR_X)
GALLERY_DECK_Y = (1500, SHELF_Y[0])
PULPIT_X = 1100
PULPIT_Y = (1300, SHELF_Y[0])
# The south: the walk of rock along the wall at L1, and the Ledge's decks of cheese north of it.
WALK_Y = (-HALF_Y, -2200)
LEDGE_Y = (WALK_Y[1], -1400)
LANDING_X = 900                 # the landing in the middle, where the south bridge comes down, is rock
LEDGE_X = (LANDING_X, FLOOR_X)
# The Spine: rock from the floor to the ceiling in the middle, with an arch through it at L1 for the bridges.
SPINE = (350, 600)              # half its size, x and y
ARCH = (150, L1, 700)           # half the arch's width, its floor and its top
# The Rind Tunnel along the south walk.
TUNNEL_X = 1500
TUNNEL_Y = -2500
SPAN_WIDTH = 250


def team_of(s):
    return "Gouda" if s < 0 else "Mozza"


def rock_material():
    return kit.material("MI_MineRock")


def mirrored_box(label, folder, s, x0, y0, z0, x1, y1, z1, material):
    """A box from x0 to x1 (given for the east side, s = 1) and y0 to y1, mirrored to the west for s = -1."""
    return box(label, folder, (min(s * x0, s * x1), y0, z0), (max(s * x0, s * x1), y1, z1), material)


def rock(label, s, x0, y0, z0, x1, y1, z1, material=None):
    """Rock the mice never eat: walks, shelves, walls, the Spine."""
    return mirrored_box(label, "Rock", s, x0, y0, z0, x1, y1, z1, material or rock_material())


def no_collision(actor):
    actor.static_mesh_component.set_collision_profile_name("NoCollision")
    return actor


# ---------------------------------------------------------------- lamps

# How bright the lamps are, all together, against the exposure cave_air() sets.
LAMP_GAIN = 2.0
LAMP_COLOURS = {
    # glass, light colour (sRGB)
    "Amber": ("Amber", (255, 196, 140)),
    "Gold": ("Gold", (255, 210, 150)),
    "Ice": ("Ice", (205, 225, 255)),
    "Red": ("Red", (255, 160, 125)),
    "White": ("White", (255, 236, 215)),
}


def light(label, folder, at, colour, intensity, radius, shadows=False, source=6.0):
    """A point light: what lights the cave (it has no sun or sky). Most cast no shadow, to keep the shadowed lights
    to the twenty or so principles.md allows."""
    actor = place(unreal.PointLight, label, folder, at)
    component = actor.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("intensity", float(intensity) * LAMP_GAIN)
    component.set_editor_property("attenuation_radius", float(radius))
    component.set_editor_property("light_color", unreal.Color(r=colour[0], g=colour[1], b=colour[2], a=255))
    component.set_editor_property("cast_shadows", bool(shadows))
    component.set_editor_property("source_radius", float(source))
    kit.count("lights" + (" (shadowed)" if shadows else ""))
    return actor


def lantern(label, folder, x, y, z, colour="Amber", intensity=10.0, radius=1200.0, shadows=False):
    """A miner's lantern with its bottom at (x, y, z): a dark iron frame round glowing glass, and its light."""
    glass, rgb = LAMP_COLOURS[colour]
    iron = painted("Dark")
    no_collision(box(label + "_Base", folder, (x - 11, y - 11, z), (x + 11, y + 11, z + 4), iron))
    no_collision(box(label + "_Glass", folder, (x - 8, y - 8, z + 4), (x + 8, y + 8, z + 26), kit.material("MI_Lamplight_" + glass)))
    for dx, dy in ((-9, -9), (9, -9), (-9, 9), (9, 9)):
        no_collision(box(f"{label}_Bar_{dx}_{dy}", folder, (x + dx - 1.5, y + dy - 1.5, z + 4), (x + dx + 1.5, y + dy + 1.5, z + 26), iron))
    no_collision(box(label + "_Cap", folder, (x - 12, y - 12, z + 26), (x + 12, y + 12, z + 31), iron))
    no_collision(box(label + "_Top", folder, (x - 5, y - 5, z + 31), (x + 5, y + 5, z + 38), iron))
    light(label + "_Light", folder, (x, y, z + 16), rgb, intensity, radius, shadows)
    kit.count("lanterns")


def wall_lantern(label, folder, x, y, z, toward, colour="Amber", intensity=10.0, radius=1200.0, shadows=False):
    """A lantern hung from an iron bracket on a rock face at (x, y), its arm `z` high reaching `toward` (dx, dy)."""
    ax, ay = x + toward[0] * 45, y + toward[1] * 45
    no_collision(box(label + "_Bracket", folder, (min(x, ax) - 3, min(y, ay) - 3, z - 3), (max(x, ax) + 3, max(y, ay) + 3, z + 3), painted("Dark")))
    no_collision(box(label + "_Hook", folder, (ax - 1, ay - 1, z - 20), (ax + 1, ay + 1, z), painted("Dark")))
    lantern(label, folder, ax, ay, z - 58, colour, intensity, radius, shadows)


def lantern_post(label, folder, x, y, z, toward, colour="Amber", intensity=10.0, radius=1200.0, shadows=False):
    """A lantern hung from the arm of a timber post standing on (x, y, z), the arm reaching `toward` (dx, dy)."""
    timber = kit.material("MI_Timber")
    box(label + "_Post", folder, (x - 9, y - 9, z), (x + 9, y + 9, z + 230), timber)
    ax, ay = x + toward[0] * 55, y + toward[1] * 55
    no_collision(box(label + "_Arm", folder, (min(x, ax) - 5, min(y, ay) - 5, z + 215), (max(x, ax) + 5, max(y, ay) + 5, z + 225), timber))
    no_collision(box(label + "_Hook", folder, (ax - 1, ay - 1, z + 185), (ax + 1, ay + 1, z + 215), painted("Dark")))
    lantern(label, folder, ax, ay, z + 147, colour, intensity, radius, shadows)


def wall_lamp(label, folder, x, y, z, facing, colour="White", intensity=8.0, radius=1000.0, shadows=False):
    """A miner's bulkhead lamp on a rock face at (x, y, z), facing `facing` (yaw): a caged lamp and its light."""
    lamp = mesh("BulkheadLight")
    actor = place(unreal.StaticMeshActor, label, folder, (x, y, z), (90, facing, 0))
    actor.static_mesh_component.set_static_mesh(lamp)
    actor.set_actor_scale3d(kit.v(1.6, 1.6, 1.6))
    no_collision(actor)
    turn = unreal.MathLibrary.degrees_to_radians(facing)
    out = (unreal.MathLibrary.cos(turn), unreal.MathLibrary.sin(turn))
    light(label + "_Light", folder, (x + out[0] * 40, y + out[1] * 40, z), LAMP_COLOURS[colour][1], intensity, radius, shadows)
    kit.count("wall lamps")


POOL_DEPTH = 15     # a pool's bed is this far under the floor: stepped down into and up out of without a jump


def pool(label, folder, low, high, glow=True):
    """Still water from corner (x0, y0) to (x1, y1). A glowing pool lies in a hole in the floor (POOLS), on a bed of wet
    rock POOL_DEPTH under it; a puddle lies on the floor. The water itself has no collision: it is walked through."""
    if glow:
        box(label + "_Bed", folder, (low[0], low[1], -POOL_DEPTH - 30), (high[0], high[1], -POOL_DEPTH), kit.material("MI_MineRockWet"))
        return no_collision(box(label, folder, (low[0], low[1], -6), (high[0], high[1], -4), kit.material("MI_GrottoPool")))
    return no_collision(box(label, folder, (low[0], low[1], 0), (high[0], high[1], 0.8), kit.material("MI_Puddle")))


def holed_box(label, folder, low, high, material, face, holes, depth=250):
    """A block of rock from `low` to `high` with holes cut `depth` deep into its `face` ("x-", "x+", "y-" or "y+": the
    side it shows the cave), black at their backs: where the mice come out of the rock. Each hole is (u0, u1, z0, z1),
    u along the face (y on an x face, x on a y face)."""
    axis = 0 if face[0] == "x" else 1
    other = 1 - axis
    inward = 1 if face[1] == "-" else -1
    front = low[axis] if inward > 0 else high[axis]
    split = front + inward * depth
    behind_low, behind_high = list(low), list(high)
    if inward > 0:
        behind_low[axis] = split
    else:
        behind_high[axis] = split
    box(label, folder, tuple(behind_low), tuple(behind_high), material)
    a, b = sorted((front, split))
    cut = [((h[0], h[2]), (h[1], h[3])) for h in holes]
    for i, (p, q) in enumerate(kit.rect_minus((low[other], low[2]), (high[other], high[2]), cut)):
        lo, hi = [0.0, 0.0, p[1]], [0.0, 0.0, q[1]]
        lo[axis], hi[axis] = a, b
        lo[other], hi[other] = p[0], q[0]
        box(f"{label}_Face_{i}", folder, tuple(lo), tuple(hi), material)
    for j, h in enumerate(holes):
        lo, hi = [0.0, 0.0, h[2]], [0.0, 0.0, h[3]]
        lo[axis], hi[axis] = sorted((split - inward * 10, split))
        lo[other], hi[other] = h[0], h[1]
        no_collision(box(f"{label}_Hole_{j}", folder, tuple(lo), tuple(hi), kit.material("MI_Hole")))


# The mouse holes, by the face they are in: (u0, u1, z0, z1). Those at the floor and the Pulpit have a sill 80 cm up,
# which a player can't climb into.
HOLE = 260
NORTH_HOLES = []
SOUTH_HOLES = [(x - HOLE / 2, x + HOLE / 2, L1 + 150, L1 + 150 + HOLE) for x in (-3000, -2100, 2100, 3000)]
END_HOLES = [(y - HOLE / 2, y + HOLE / 2, 80, 80 + HOLE) for y in (-1100, 1000)]
PULPIT_HOLE_X = 300
# The glowing pools, cut into the floor: the Well round the Spine, north and south of it, and one each side.
POOLS = [("Pool_Well_North", (-500, 660), (500, 1150)), ("Pool_Well_South", (-450, -1250), (450, -660)),
         ("Pool_West", (-2100, -250), (-1300, 250)), ("Pool_West_Inlet", (-1900, 250), (-1500, 550)),
         ("Pool_East", (1300, -250), (2100, 250)), ("Pool_East_Inlet", (1500, 250), (1900, 550))]


# ---------------------------------------------------------------- the cave: floor, walls, ceiling, the Spine

def cliff(label, low, high, yaw, large=True, collide=False):
    """A Megascans quarry cliff filling the block from `low` to `high`, its face turned by `yaw` (the scans face their
    +Y, so yaw 0 faces north, 90 west, 180 south, 270 east; their backs are open): the cave's rock face,
    standing in front of the blocks that are its walls. Where players can reach it, it `collide`s, by its box (as
    import_fab.py gives every model), so that the rock they see is what stops them; elsewhere it has no collision. A face taller than a cliff is at the
    scale its length gives it is a stack of them, each taller than its share so that they overlap."""
    model = mesh("QuarryCliffLarge" if large else "QuarryCliff")
    b = model.get_bounding_box()
    length = abs(high[0] - low[0]) if yaw % 180 == 0 else abs(high[1] - low[1])
    natural = (b.max.z - b.min.z) * length / (b.max.x - b.min.x)
    height = high[2] - low[2]
    n = max(1, int(round(height / natural)))
    for k in range(n):
        z0 = low[2] + height * k / n
        z1 = low[2] + height * (k + 1) / n + (height / n * 0.4 if k < n - 1 else 0)
        # Each one up the other cliff, so that the one above's foot covers the crags along the top of the one below.
        piece = model if k % 2 == 0 else mesh("QuarryCliff" if large else "QuarryCliffLarge")
        actor = fill(f"{label}_{k}", "Ground/Cliffs", piece, (low[0], low[1], z0), (high[0], high[1], z1), yaw)
        if not collide:
            no_collision(actor)


def cliffs():
    """The cave's walls dressed in rock: along the north wall over the gallery, the south wall over the walk, the cave's
    ends over the hollows, the hollows' walls facing the cave, and the Spine's sides."""
    out, back = 60, 340

    def runs(holes):
        """Stretches along a wall about 13 m long, leaving each hole (and a metre and a half either side) clear."""
        edges = [-HALF_X]
        for h in sorted(holes):
            edges += [h[0] - 150, h[1] + 150]
        edges.append(HALF_X)
        for a, b in zip(edges[0::2], edges[1::2]):
            n = max(1, int(round((b - a) / 1300.0)))
            for k in range(n):
                yield a + (b - a) * k / n, a + (b - a) * (k + 1) / n
    for i, (x, x1) in enumerate(runs(NORTH_HOLES)):
        cliff(f"Cliff_North_{i}", (x, HALF_Y - out, L2 - 20), (x1, HALF_Y + back, CEILING + 150), 180, i % 2 == 0, collide=True)
    for i, (x, x1) in enumerate(runs(SOUTH_HOLES)):
        cliff(f"Cliff_South_{i}", (x, -HALF_Y - back, L1 - 20), (x1, -HALF_Y + out, CEILING + 150), 0, i % 2 == 1, collide=True)
    for s in (-1, 1):
        side = "West" if s < 0 else "East"
        for j, (y0, y1) in enumerate(((-HALF_Y, -1000), (-1000, 900), (900, HALF_Y))):
            lo, hi = sorted((s * (HALF_X - out), s * (HALF_X + back)))
            cliff(f"Cliff_End_{side}_{j}", (lo, y0, L1 - 20), (hi, y1, CEILING + 150), 90 if s > 0 else 270, j != 1)
        for j, (y0, y1) in enumerate(((WALK_Y[1], -HOLLOW_DOOR), (HOLLOW_DOOR, 2000))):
            lo, hi = sorted((s * (FLOOR_X - 40), s * HOLLOW_WALL[1]))
            cliff(f"Cliff_HollowWall_{side}_{j}", (lo, y0, L1 + 300), (hi, y1, CEILING + 150), 90 if s > 0 else 270, False)
        # The Spine's sides stand out 1.3 m from its block, and stop players there.
        lo, hi = sorted((s * (SPINE[0] - 90), s * (SPINE[0] + 130)))
        cliff(f"Cliff_Spine_{side}", (lo, -SPINE[1], -20), (hi, SPINE[1], CEILING + 150), 270 if s > 0 else 90, collide=True)
    hanging_rock()


def hanging(label, centre, size, top, yaw=0, large=True):
    """A cliff turned upside down under the ceiling, `size` (x, y, z) at its biggest, its top at `top`: the cave's roof
    hanging down in crags. No collision (the ceiling has none)."""
    model = mesh("QuarryCliffLarge" if large else "QuarryCliff")
    b = model.get_bounding_box()
    scale = (size[0] / (b.max.x - b.min.x), size[1] / (b.max.y - b.min.y), size[2] / (b.max.z - b.min.z))
    mid = ((b.min.x + b.max.x) / 2.0, (b.min.y + b.max.y) / 2.0)
    # Rolled over, y and z turn round; yawed half round as well, x and y do.
    flip = -1 if yaw else 1
    at = (centre[0] - flip * scale[0] * mid[0], centre[1] + flip * scale[1] * mid[1], top + scale[2] * b.min.z)
    actor = place(unreal.StaticMeshActor, label, "Ground/Ceiling", at, (0, yaw, 180))
    actor.static_mesh_component.set_static_mesh(model)
    actor.set_actor_scale3d(kit.v(*scale))
    return no_collision(actor)


def hanging_rock():
    """Crags hanging from the ceiling: over the floor between the levels, and the Curtain's own over the Pulpit."""
    hanging("Crag_Curtain", (0, 2050), (1000, 1700, 1000), CEILING + 100)
    for i, (x, y, w, d, h, yaw) in enumerate(((-2600, -300, 1500, 900, 600, 0), (2600, -300, 1500, 900, 600, 180), (-1200, -1700, 1300, 700, 500, 180),
                                              (1200, -1700, 1300, 700, 500, 0), (-2400, 2200, 1600, 800, 500, 180), (2400, 2200, 1600, 800, 500, 0),
                                              (0, -200, 900, 600, 500, 0), (-3900, 0, 1100, 2400, 500, 0), (3900, 0, 1100, 2400, 500, 180))):
        hanging(f"Crag_{i}", (x, y), (w, d, h), CEILING + 100, yaw, i % 2 == 0)


def cave_air():
    """The light rig's cave, darker and still: thin haze that the lamps light (volumetric fog), and the eye kept from
    brightening the dark between the lamps into day."""
    for actor in kit.eas.get_all_level_actors():
        label = actor.get_actor_label()
        if label == "Haze":
            fog = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
            fog.set_editor_property("fog_density", 0.015)
            fog.set_editor_property("fog_height_falloff", 0.04)
            fog.set_editor_property("start_distance", 400.0)
            fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.006, 0.0052, 0.0045, 1.0))
            fog.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
            fog.set_editor_property("enable_volumetric_fog", True)
            fog.set_editor_property("volumetric_fog_scattering_distribution", 0.5)
            fog.set_editor_property("volumetric_fog_extinction_scale", 0.6)
        elif label == "PostProcess":
            settings = actor.get_editor_property("settings")
            for key, value in (("auto_exposure_bias", 0.6), ("auto_exposure_min_brightness", -3.5), ("auto_exposure_max_brightness", 3.0),
                               ("bloom_intensity", 0.5), ("vignette_intensity", 0.4)):
                settings.set_editor_property(key, value)
                settings.set_editor_property("override_" + key, True)
            actor.set_editor_property("settings", settings)
    kit.lines.append("cave air: volumetric haze, exposure held between EV100 -3.5 and 3")


def cave():
    floor = kit.material("MI_CaveFloor")
    kit.ground("Floor", (-HALF_X, -HALF_Y), (HALF_X, HALF_Y), floor, holes=[(low, high) for _, low, high in POOLS])
    # The walls, the north and south ones with the mouse holes over the gallery and the walk.
    dark = kit.material("MI_MineRockDark")
    f, t = "Ground/Boundary", 300
    holed_box("Boundary_North", f, (-HALF_X - t, HALF_Y, -400), (HALF_X + t, HALF_Y + t, CEILING), dark, "y-", NORTH_HOLES)
    holed_box("Boundary_South", f, (-HALF_X - t, -HALF_Y - t, -400), (HALF_X + t, -HALF_Y, CEILING), dark, "y+", SOUTH_HOLES)
    box("Boundary_West", f, (-HALF_X - t, -HALF_Y, -400), (-HALF_X, HALF_Y, CEILING), dark)
    box("Boundary_East", f, (HALF_X, -HALF_Y, -400), (HALF_X + t, HALF_Y, CEILING), dark)
    # The ceiling: rock over everything, higher over the middle. It has no collision: nobody reaches it, and the
    # streaks' wheels fall through it onto the cave as they would onto open ground.
    # (GROTTO_OPEN=1 in the environment leaves it out, for looking at the map from above.)
    if os.environ.get("GROTTO_OPEN") != "1":
        no_collision(box("Ceiling", "Ground/Ceiling", (-HALF_X - 300, -HALF_Y - 300, CEILING), (HALF_X + 300, HALF_Y + 300, CEILING + 300), kit.material("MI_MineRockDark")))
    # The Spine: rock from the floor to the ceiling in the middle, with an arch through it at L1, north to south, that
    # the bridges come into. It stops every line along the cave's middle at the middle.
    for s in (-1, 1):
        rock(f"Spine_{'West' if s < 0 else 'East'}", s, ARCH[0], -SPINE[1], 0, SPINE[0], SPINE[1], CEILING)
    rock("Spine_Under", 1, -ARCH[0], -SPINE[1], 0, ARCH[0], SPINE[1], ARCH[1])
    rock("Spine_Over", 1, -ARCH[0], -SPINE[1], ARCH[2], ARCH[0], SPINE[1], CEILING)
    # The Curtain: rock hanging from the ceiling over the Pulpit, down to well above the heads of those on it, that
    # stops the line along the galleries from one side's to the other's.
    rock("Curtain", 1, -300, PULPIT_Y[0], 950, 300, HALF_Y, CEILING)


def side_rock(s, side):
    """The rock on one side: the north shelf and the gallery walk on it, the Pulpit's half, the south walk and the
    landing's half, the hollow, and the stairs between the levels."""
    X = lambda x: s * x
    dark = kit.material("MI_MineRockDark")
    # The north shelf at L1 along the wall, and the gallery walk on it at L2 from the Pulpit's stairs to the hollow.
    rock(f"Shelf_{side}", s, 0, SHELF_Y[0], 0, HALF_X, SHELF_Y[1], L1, dark)
    rock(f"Gallery_{side}", s, GALLERY_X[0], SHELF_Y[0], L1, HALF_X, SHELF_Y[1], L2)
    # The hollow's own landing at L2, over its north end, where its stairs come up.
    rock(f"Hollow_Landing_{side}", s, FLOOR_X, 2000, L1, HALF_X, SHELF_Y[0], L2)
    # The Pulpit: a balcony of rock at L1 in the north middle, over the Spine's pool.
    lo, hi = sorted((0, X(PULPIT_X)))
    hx = sorted((X(PULPIT_HOLE_X - HOLE / 2), X(PULPIT_HOLE_X + HOLE / 2)))
    holed_box(f"Pulpit_{side}", "Rock", (lo, PULPIT_Y[0], 0), (hi, PULPIT_Y[1], L1), dark, "y-", [(hx[0], hx[1], 80, 80 + HOLE)])
    # The south walk at L1 along the wall, and the landing in the middle where the south bridge comes down.
    rock(f"Walk_{side}", s, 0, WALK_Y[0], 0, FLOOR_X, WALK_Y[1], L1, dark)
    rock(f"Landing_{side}", s, 0, LEDGE_Y[0], 0, LANDING_X, LEDGE_Y[1], L1, dark)
    # On the walk, a buttress from the wall and a fin at the edge, overlapping, so that the walk zig-zags between
    # them and no line runs along it from hollow to hollow once the tunnel is eaten.
    rock(f"Walk_Buttress_{side}", s, 2300, WALK_Y[0], L1, 2600, -2400, 1300, dark)
    rock(f"Walk_Fin_{side}", s, 1700, -2450, L1, 1900, WALK_Y[1], 1100, dark)
    # The hollow: rock at L1 from the floor's end to the cave's end, walled off from the cave but for its three ways
    # out: a passage onto the south walk, an opening in the middle onto the stairs down to the floor, and its stairs
    # up to its landing and the gallery.
    lo, hi = sorted((X(FLOOR_X), X(HALF_X)))
    holed_box(f"Hollow_{side}", "Rock", (lo, -HALF_Y, 0), (hi, 2000, L1), dark, "x-" if s > 0 else "x+", END_HOLES)
    rock(f"Hollow_Wall_South_{side}", s, HOLLOW_WALL[0], WALK_Y[1], L1, HOLLOW_WALL[1], -HOLLOW_DOOR, CEILING)
    rock(f"Hollow_Wall_North_{side}", s, HOLLOW_WALL[0], HOLLOW_DOOR, L1, HOLLOW_WALL[1], 2000, CEILING)
    rock(f"Hollow_Wall_Over_{side}", s, HOLLOW_WALL[0], -HOLLOW_DOOR, 700, HOLLOW_WALL[1], HOLLOW_DOOR, CEILING)
    # Stairs cut in the rock: risers of 17 cm, treads of 30.
    stairs = rock_material()
    kit.stairs(f"Stairs_HollowUp_{side}", "Rock/Stairs", (X(3900), 1250, L1), (X(3900), 2000, L2), 400, stairs)
    kit.stairs(f"Stairs_HollowDown_{side}", "Rock/Stairs", (X(2700), 0, 0), (X(FLOOR_X), 0, L1), 600, stairs)
    kit.stairs(f"Stairs_Gallery_{side}", "Rock/Stairs", (X(PULPIT_X), 2550, L1), (X(GALLERY_X[0]), 2550, L2), 500, stairs)
    kit.stairs(f"Stairs_Pulpit_{side}", "Rock/Stairs", (X(800), 600, 0), (X(800), PULPIT_Y[0], L1), 300, stairs)
    kit.stairs(f"Stairs_Landing_{side}", "Rock/Stairs", (X(600), -700, 0), (X(600), LEDGE_Y[1], L1), 300, stairs)


# ---------------------------------------------------------------- the cheese

def decks(s, side):
    """The decks of cheese that widen the upper levels, each slab on a pillar (kit.pillar_deck): the gallery's, 8 m up
    over the floor on tall pillars (the Colonnade under them), and the Ledge's, 4 m up (the Undercroft under them).
    Each has a low edge of cheese along its open side to crouch behind."""
    team = team_of(s)
    gx = sorted((s * GALLERY_X[0], s * GALLERY_X[1]))
    kit.pillar_deck(f"GalleryDeck_{team}", f"GalleryDeck{team}", (gx[0], GALLERY_DECK_Y[0], 0), (gx[1], GALLERY_DECK_Y[1], L2), pillar=180, edge="S")
    lx = sorted((s * LEDGE_X[0], s * LEDGE_X[1]))
    kit.pillar_deck(f"LedgeDeck_{team}", f"LedgeDeck{team}", (lx[0], LEDGE_Y[0], 0), (lx[1], LEDGE_Y[1], L1), pillar=160, edge="N")


def cover(s, side):
    """Cheese to fight round on the floor, the walks and in the hollow; and rock and mine carts that stay."""
    X = lambda x: s * x
    team = team_of(s)
    f = f"Cover_{side}"
    floor = f"Floor{team}"
    # On the floor: stacks of wheels and blocks, crouching and full cover, kept out from under the decks.
    kit.wheel_stack(f"FloorWheels_{team}_A", floor, X(2300), 800, 0, 160, 70, 3)
    kit.wheel_stack(f"FloorWheels_{team}_B", floor, X(1000), 350, 0, 150, 70, 2)
    kit.wheel_stack(f"FloorWheels_{team}_C", floor, X(3050), -850, 0, 150, 70, 3)
    kit.wheel_stack(f"FloorWheels_{team}_D", floor, X(3100), 1100, 0, 150, 70, 2)
    for name, (x0, y0, x1, y1, nx, ny, nz, h) in {
        "A": (2700, -1300, 2900, -1100, 1, 1, 2, 200),
        "C": (2800, 500, 3000, 700, 2, 1, 2, 200),
        "D": (1750, 900, 1950, 1000, 2, 1, 1, 100),
        "E": (1900, -1150, 2100, -1050, 2, 1, 1, 100),
        "F": (1200, -100, 1300, 100, 1, 2, 2, 200),
        "G": (1950, 650, 2150, 750, 2, 1, 2, 200),
        "H": (700, -500, 1000, -400, 3, 1, 1, 100),
    }.items():
        kit.cheese_stack(f"FloorBlock_{team}_{name}", floor, (min(X(x0), X(x1)), y0, 0), (max(X(x0), X(x1)), y1, h), nx, ny, nz)
    # On the gallery walk and the south walk: blocks to crouch behind.
    upper = f"Walks{team}"
    kit.cheese_stack(f"GalleryBlock_{team}_A", upper, (min(X(2400), X(2600)), 2600, L2), (max(X(2400), X(2600)), 2700, L2 + 100), 2, 1, 1)
    kit.cheese_stack(f"GalleryBlock_{team}_B", upper, (min(X(3000), X(3100)), 2350, L2), (max(X(3000), X(3100)), 2550, L2 + 100), 1, 2, 1)
    kit.cheese_stack(f"WalkBlock_{team}_A", upper, (min(X(2900), X(3100)), -2720, L1), (max(X(2900), X(3100)), -2620, L1 + 100), 2, 1, 1)
    # On the Pulpit, along its edge over the pool: a low wall to crouch behind, broken where the bridge and stairs come.
    for i, (x0, x1) in enumerate(((150, 400), (400, 650), (950, PULPIT_X))):
        kit.cheese_box(f"Cheese_PulpitEdge_{team}_{i}", "PulpitEdge", (min(X(x0), X(x1)), PULPIT_Y[0], L1), (max(X(x0), X(x1)), PULPIT_Y[0] + 50, L1 + 100))
    # In the hollow, in front of its opening: a wall of cheese that hides the hollow from the stairs until eaten.
    kit.cheese_stack(f"HollowScreen_{team}", f"Hollow{team}", (min(X(3800), X(3900)), -300, L1), (max(X(3800), X(3900)), 300, L1 + 200), 1, 3, 2)
    # Rock that stays: boulders on the floor, and mine carts on their rails.
    boulder = mesh("QuarryCliff")
    for name, (x0, y0, x1, y1, h, yaw) in {
        "A": (2600, -1000, 2800, -700, 150, 90),
        "B": (1400, 900, 1650, 1150, 180, 0),
        "C": (1050, -1050, 1300, -800, 120, 0),
    }.items():
        fill(f"Boulder_{side}_{name}", f, boulder, (min(X(x0), X(x1)), y0, 0), (max(X(x0), X(x1)), y1, h), yaw)
    cart = mesh("MineCart")
    fill(f"Cart_{side}", f, cart, (min(X(2500), X(2650)), 1140, 0), (max(X(2500), X(2650)), 1360, 150), 90)
    # The rails the carts stand on, in front of the Colonnade: dressing, sunk into the floor and walked over.
    rail = mesh("RailTrack")
    for i, x in enumerate(range(1825, 3301, 250)):
        actor = fill(f"Rail_{side}_{i}", f, rail, (min(X(x - 125), X(x + 125)), 1125, -16), (max(X(x - 125), X(x + 125)), 1375, 4), 90)
        no_collision(actor)


def store(s, side):
    """The Cheese Store on each side of the floor: a house of cheese one storey high (kit.building) between the hollow's
    stairs and the Well, south of the pool, with a door at either end and a window to the north and the south. Close
    fighting through it on the floor's way to the middle, until the mice eat it to its corners."""
    X = lambda x: s * x
    team = team_of(s)
    name = f"CheeseStore{team}"
    lo, hi = sorted((X(1500), X(2500)))
    kit.building(name, name, (lo, -1300), (hi, -500), storeys=1, doors=[("E", -900), ("W", -900)],
                 windows=[("N", X(1875), 0), ("S", X(2125), 0)])


def tunnel():
    """The Rind Tunnel: a covered way of cheese along the middle of the south walk, its walls thick, a door in its north
    side onto the landing where the south bridge comes down."""
    kit.tunnel("RindTunnel", "RindTunnel", (-TUNNEL_X, TUNNEL_Y), (TUNNEL_X, TUNNEL_Y), width=400, height=300, wall=60, bottom=L1,
               openings=[("L", TUNNEL_X - 150, TUNNEL_X + 150)], lamps=600)
    kit.door("RindTunnel_Door", "Tunnels/RindTunnel", (0, TUNNEL_Y + 230, L1), 90, 260, 220)


def spans():
    """The two rope bridges at L1: from the Pulpit to the Spine's arch, and from the arch to the landing. Their decks
    are cheese on waxed beams that rest on rock at either end; eaten, each is two red beams to balance along."""
    timber = kit.material("MI_Timber")
    rope = kit.material("MI_Timber")
    for name, y0, y1 in (("NorthSpan", SPINE[1], PULPIT_Y[0]), ("SouthSpan", LEDGE_Y[1], -SPINE[1])):
        kit.bridge(name, name, (0, y0, L1), (0, y1, L1), width=SPAN_WIDTH, beams=2, beam=40, depth=30, deck=30, rails=False, wax_beams=True)
        # Where the beams' ends rest: corbels of rock standing against the rock faces at either end, under the beams.
        middle = (y0 + y1) / 2.0
        for y in (y0, y1):
            inward = 60 if middle > y else -60
            box(f"{name}_Corbel_{int(y)}", "Rock", (-SPAN_WIDTH / 2.0, min(y, y + inward), -POOL_DEPTH - 30), (SPAN_WIDTH / 2.0, max(y, y + inward), L1 - 60), rock_material())
        # Rope rails: timber posts at the ends and every 2 m, two ropes along them.
        f = f"Bridges/{name}"
        posts = int((y1 - y0) // 200)
        for side in (-1, 1):
            x = side * (SPAN_WIDTH / 2.0 + 6)
            for i in range(posts + 1):
                y = y0 + (y1 - y0) * i / posts
                box(f"{name}_Post_{side}_{i}", f, (x - 5, y - 5, L1 - 70), (x + 5, y + 5, L1 + 110), timber)
            for h in (100, 55):
                no_collision(kit.pipe_y(f"{name}_Rope_{side}_{h}", f, x, y0, y1, L1 + h, 4, rope))
    # Under the arch, the beams' middle: the arch's floor carries them through the Spine (part of Spine_Under).


def hollow_dressing(s, side):
    """What a miners' hollow has in it, along its back wall and out of the starts' way: timber props holding up the rock,
    a mine cart, barrels, crates on a pallet."""
    X = lambda x: s * x
    f = f"Hollow_{side}"
    timber = mesh("WoodenBeam")
    back = HALF_X - 75
    for i, y in enumerate((-2450, -1400, -200, 400, 1250)):
        for dy in (-90, 90):
            fill(f"{f}_Prop_{i}_{dy}", f, timber, (min(X(back - 13), X(back + 13)), y + dy - 13, L1), (max(X(back - 13), X(back + 13)), y + dy + 13, L1 + 330))
        box(f"{f}_Cap_{i}", f, (min(X(back - 16), X(back + 16)), y - 120, L1 + 330), (max(X(back - 16), X(back + 16)), y + 120, L1 + 360), kit.material("MI_Timber"))
    fill(f"{f}_Cart", f, mesh("MineCart"), (min(X(4250), X(4400)), -560, L1), (max(X(4250), X(4400)), -340, L1 + 150), 90)
    for i, (x, y) in enumerate(((4420, -2650), (4360, -2700), (4440, -2560))):
        kit.prop(f"{f}_Barrel_{i}", f, "Barrel", X(x), y, L1, i * 40)
    kit.prop(f"{f}_Pallet", f, "Pallet", X(4400), 1650, L1, 90)
    for i in range(2):
        kit.prop(f"{f}_Crate_{i}", f, "PlasticCrate", X(4400), 1650, L1 + 14 + i * 26)


# ---------------------------------------------------------------- light

def lamps(s, side):
    """Each place its own light: the hollows in their side's colour, lanterns along the gallery and on the Pulpit,
    bulkhead lamps along the south walk and in the Undercroft, the pools' glow on the floor."""
    X = lambda x: s * x
    f = f"Lamps/{side}"
    hollow = "Gold" if s < 0 else "Ice"
    # The hollow: its own colour, lit well.
    for i, y in enumerate((-2000, -800, 800, 1600)):
        wall_lantern(f"Lamp_Hollow_{side}_{i}", f, X(HALF_X - 60), y, L1 + 300, (-s, 0), hollow, 10.0, 1500.0, shadows=i in (1, 2))
    # The gallery: lanterns on posts along the walk's edge, and on the hollow's landing.
    for i, x in enumerate((2000, 2800, 3700)):
        lantern_post(f"Lamp_Gallery_{side}_{i}", f, X(x), 2400, L2, (0, -1), "Amber", 14.0, 1600.0, shadows=i == 1)
    # The Colonnade under the gallery's decks: lanterns on the shelf's face.
    for i, x in enumerate((2200, 3000)):
        wall_lantern(f"Lamp_Colonnade_{side}_{i}", f, X(x), SHELF_Y[0], 330, (0, -1), "Amber", 10.0, 1200.0)
    # The south walk and the Undercroft under the Ledge's decks: bulkhead lamps on the rock.
    for i, x in enumerate((1600, 2750)):
        wall_lamp(f"Lamp_Walk_{side}_{i}", f, X(x), WALK_Y[0] + 65, L1 + 260, 90, "White", 9.0, 1100.0)
    for i, x in enumerate((1300, 2150, 3000)):
        wall_lamp(f"Lamp_Undercroft_{side}_{i}", f, X(x), WALK_Y[1] + 5, 300, 90, "White", 8.0, 1000.0, shadows=i == 1)
    # The Pulpit: red lanterns on posts at its corners.
    lantern_post(f"Lamp_Pulpit_{side}", f, X(1000), 1400, L1, (0, -1), "Red", 10.0, 1200.0, shadows=True)
    # The floor: a lamp by the hollow's stairs.
    lantern_post(f"Lamp_Stairs_{side}", f, X(2650), -420, 0, (-1, 0), "Amber", 12.0, 1400.0, shadows=True)


def fill_light():
    """The cave's own glow: big soft lights high under the ceiling, casting no shadows, that lift the dark between the
    lamps enough to see someone in it, as light bounced round a lamp-lit cave would."""
    f = "Lamps/Fill"
    warm = (255, 214, 170)
    for i, (x, y) in enumerate(((-3000, 1200), (-3000, -1400), (-1400, 0), (0, 1800), (0, -1900), (1400, 0), (3000, 1200), (3000, -1400))):
        light(f"Fill_{i}", f, (x, y, CEILING - 300), warm, 40.0, 3200.0, source=200.0)


def pools():
    """The glowing pools on the floor: the Well round the Spine, and one each side. Their light is low teal."""
    f = "Pools"
    teal = (190, 240, 225)
    for name, low, high in POOLS:
        pool(name, f, low, high)
    light("Pool_Well_North_Light", f, (0, 900, 60), teal, 1.5, 700.0)
    light("Pool_Well_South_Light", f, (0, -950, 60), teal, 1.5, 700.0)
    for s, side in ((-1, "West"), (1, "East")):
        light(f"Pool_{side}_Light", f, (s * 1700, 100, 60), teal, 1.5, 700.0, shadows=True)
        # Puddles where the cave drips.
        pool(f"Puddle_{side}_A", f, (min(s * 2900, s * 3200), -600), (max(s * 2900, s * 3200), -300), glow=False)
        pool(f"Puddle_{side}_B", f, (min(s * 600, s * 900), 150), (max(s * 600, s * 900), 450), glow=False)


# ---------------------------------------------------------------- what a match needs

def starts():
    for s, side in ((-1, "West"), (1, "East")):
        team = team_of(s)
        yaw = 0 if s < 0 else 180
        kit.team_starts(team, [(s * x, y) for x in (3850, 4200) for y in (-1900, -1300, 700, 1150)], yaw, z=L1, name="Hollow_")
    # Free-for-all: starts through every level, each facing the middle.
    points = []
    for s, side in ((-1, "West"), (1, "East")):
        yaw = 0 if s < 0 else 180
        for name, x, y, z in (("Floor", 2200, 0, 0), ("FloorSouth", 2900, -500, 0), ("FloorNorth", 1100, 700, 0),
                              ("Pulpit", 600, 1900, L1), ("Gallery", 2200, 2650, L2), ("Walk", 2000, -2650, L1), ("Landing", 500, -1800, L1)):
            points.append((s * x, y, yaw, z, f"{side}_{name}"))
    kit.free_starts(points)


def mouse_entries():
    """Where the mice come out of the rock: four holes at the floor's ends and two in the Pulpit's face, onto the
    floor, and four in the south wall over the walk. None open onto the galleries: their decks come down as the
    mice on the floor eat their pillars, and the mice climb to what is left up there last."""
    # Each in its hole (holed_box), a metre into the rock.
    for s in (-1, 1):
        for y0, y1, z0, z1 in END_HOLES:
            kit.mouse_entry(s * (FLOOR_X + 100), (y0 + y1) / 2.0, z0 + 60, 180 if s > 0 else 0)
        kit.mouse_entry(s * PULPIT_HOLE_X, PULPIT_Y[0] + 100, 140, -90)
    for x0, x1, z0, z1 in SOUTH_HOLES:
        kit.mouse_entry((x0 + x1) / 2.0, -HALF_Y - 100, z0 + 60, 90)



def places():
    for s, side in ((-1, "West"), (1, "East")):
        team = team_of(s)

        def area(name, x0, y0, x1, y1, z0, z1):
            kit.place_name(name, (min(s * x0, s * x1), y0, z0), (max(s * x0, s * x1), y1, z1))
        area(f"{team} Hollow", FLOOR_X, -HALF_Y, HALF_X, 2000, L1 - 10, CEILING)
        area(f"{team} Gallery", GALLERY_X[0], GALLERY_DECK_Y[0], HALF_X, HALF_Y, L2 - 50, CEILING)
        area(f"{team} Colonnade", GALLERY_X[0], GALLERY_DECK_Y[0], FLOOR_X, SHELF_Y[0], -10, L2 - 60)
        area(f"{team} Ledge", LANDING_X, -HALF_Y, FLOOR_X, LEDGE_Y[1], L1 - 50, CEILING)
        area(f"{team} Undercroft", LANDING_X, LEDGE_Y[0], FLOOR_X, LEDGE_Y[1], -10, L1 - 60)
        area(f"{team} Pools", PULPIT_X, LEDGE_Y[1], FLOOR_X, GALLERY_DECK_Y[0], -10, L1 - 60)
    kit.place_name("The Pulpit", (-PULPIT_X, PULPIT_Y[0], L1 - 50), (PULPIT_X, HALF_Y, CEILING))
    kit.place_name("The Well", (-PULPIT_X, LEDGE_Y[1], -10), (PULPIT_X, PULPIT_Y[0], L1 - 60))
    kit.place_name("The Spine", (-SPINE[0], -SPINE[1], L1 - 50), (SPINE[0], SPINE[1], ARCH[2]))
    kit.place_name("North Span", (-SPAN_WIDTH, SPINE[1], L1 - 50), (SPAN_WIDTH, PULPIT_Y[0], L1 + 300))
    kit.place_name("South Span", (-SPAN_WIDTH, LEDGE_Y[1], L1 - 50), (SPAN_WIDTH, -SPINE[1], L1 + 300))
    kit.place_name("The Landing", (-LANDING_X, LEDGE_Y[0], L1 - 50), (LANDING_X, LEDGE_Y[1], CEILING))
    kit.place_name("The Rind Tunnel", (-TUNNEL_X, -HALF_Y, L1 - 50), (TUNNEL_X, WALK_Y[1], L1 + 350))


def hills():
    """King of the Hill's spots, on the middle line or in twins: the Pulpit, Gouda's pool, the landing and Mozza's pool.
    Rock, never eaten (a pool's bed is rock under water that is walked through)."""
    kit.hill_spots([("Pulpit", 0, 1800, L1, 380), ("PoolsGouda", -1700, 0, 0, 420), ("Landing", 0, -1800, L1, 380),
                    ("PoolsMozza", 1700, 0, 0, 420)])


def pickups():
    """The weapons' stands (kit.pickup), on rock and floor the mice never eat, and clear of the hills (on the Pulpit's
    middle, the landing and the pools): the Roquefort in the Spine's arch, between the bridges; a Munster at each end of
    the Pulpit; a Cheddar Blaster at the top of each gallery's stairs and a Swiss Shotgun in each Cheese Store."""
    kit.pickup("roquefort", 0, 0, L1, "Arch")
    for s, side in ((-1, "West"), (1, "East")):
        kit.pickup("munster", s * 850, 2050, L1, f"Pulpit{side}")
        kit.pickup("blaster", s * 2100, 2450, L2, f"Gallery{side}")
        kit.pickup("shotgun", s * 2000, -900, 0, f"Store{side}")


def build():
    cave()
    for s, side in ((-1, "West"), (1, "East")):
        side_rock(s, side)
        decks(s, side)
        cover(s, side)
        store(s, side)
        hollow_dressing(s, side)
        lamps(s, side)
    tunnel()
    spans()
    pools()
    fill_light()
    starts()
    mouse_entries()
    places()
    hills()
    pickups()
    kit.nav_bounds((-HALF_X - 100, -HALF_Y - 100, -100), (HALF_X + 100, HALF_Y + 100, L2 + 400))
    kit.light_rig("cave")
    cave_air()
    cliffs()


kit.build_level(LEVEL, build)
