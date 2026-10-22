import math, os, sys, unreal
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit
from map_kit import box, solid, cylinder, wheel, polar, wall_between, overhead, painted

# Run with the editor closed, after the materials (build_materials.py, cheese_material.py, wax_material.py,
# polyhaven.py and import_polyhaven.py) and the wheel (wheel_mesh.py):
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_labyrinth.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder). It writes what it built to Saved/Lvl_Labyrinth.txt.
#
# Builds The Labyrinth (/Game/CheeseStrike/Maps/Lvl_Labyrinth) from scratch, with the blocks in map_kit.py: the
# aging cellar under a fromagerie, an octagon 70 m across under a vaulted ceiling, lamp-lit, with a round vault in
# the middle lit from above through an oculus. Round it, three rings of walls of stacked cheese wheels between rings
# of stone pillars make a maze of curved aisles; the gutter runs round the outside, along the cellar's walls, where
# the mice come in through the drains. Two stairs come down into it at opposite corners, one a side's, and a spine of
# waxed aisles winds from each into the vault; it is never eaten. docs/unreal/maps/labyrinth.md says how it plays and
# what the mice change.
#
# The map turns about its middle: what one side has, the other has turned half round (x, y to -x, -y), so neither
# side has the better half. Angles are in degrees, anticlockwise from east (x); north is +y. Gouda comes down in the
# south-west, Mozza in the north-east. Each half is built from angles given for Gouda's (its spine from 225 to 270,
# the north-west maze from 90 to 225) plus HALF for Mozza's.

LEVEL = "/Game/CheeseStrike/Maps/Lvl_Labyrinth"
HALF = 180.0
O = (0.0, 0.0)

# The cellar: an octagon whose walls are APOTHEM from its middle, under a ceiling at CEILING.
APOTHEM = 3500.0
CORNER = APOTHEM / math.cos(math.radians(22.5))
WALL = 100.0
CEILING = 560.0
CEILING_TOP = 640.0

# The rings, by radius: the vault, then aisles and walls of wheels (about WHEEL across, three COURSES high) outwards.
VAULT = (870.0, 930.0)
VAULT_TOP = 700.0
ARCH = (300.0, 300.0)                 # the vault's arches: wide, high
AISLES = {1: (930.0, 1370.0), 2: (1610.0, 2010.0), 3: (2250.0, 2650.0)}
WALLS = {1: 1490.0, 2: 2130.0, 3: 2770.0}
WHEEL = 240.0
COURSES = (100.0, 100.0, 100.0)
GUTTER = (2890.0, APOTHEM)
PILLAR = 120.0
SPINE = 400.0                         # the spine's width between its walls
PRESS = 14.0                          # how far neighbouring wheels in a wall press into each other (cm)

# The vault: its gallery round the inside 3.4 m up, in two halves, with stairs up between them, and the Mother
# Wheel in the middle, whose top the gallery's bridges reach.
GALLERY = (620.0, 870.0)
GALLERY_Z = 340.0
MOTHER = 600.0


def wheel_mesh():
    """Wheels with rounded edges (wheel_mesh.py), so that a stack reads as wheels."""
    return unreal.load_asset("/Game/CheeseStrike/Meshes/SM_CheeseWheel")


def rot(angle, half):
    return angle + (HALF if half else 0.0)


def team_of(half):
    return "Mozza" if half else "Gouda"


def mid(r):
    return (r[0] + r[1]) / 2.0


def stone():
    return kit.material("MI_StoneBlocks")


def flags():
    return kit.material("MI_Flagstones")


def vaulting():
    return kit.material("MI_OldBrick")


def timber():
    return kit.material("MI_OldPlanks")


def oak():
    return kit.material("MI_StuddedPlanks")


# ---------------------------------------------------------------- what is in each ring's wall, for Gouda's half (Mozza's is these plus HALF)

# Pillars stand in each wall at these angles, all round (both halves).
PILLARS = {1: [22.5 + 45 * k for k in range(8)], 2: [22.5 * k for k in range(16)], 3: [15.0 * k for k in range(24)]}
# Ways through a wall: (angle, width). The spine's are SPINE wide.
GAPS = {
    1: [(225.0, SPINE), (125.0, WHEEL), (190.0, WHEEL)],
    2: [(270.0, SPINE), (120.0, WHEEL), (165.0, WHEEL)],
    3: [(225.0, SPINE), (112.5, WHEEL), (187.5, WHEEL), (157.5, WHEEL)],
}
# The arcs of wall in wax, never eaten: the spine's sides.
WAXED = {1: [(225.0, 270.0)], 2: [(225.0, 270.0)], 3: [(225.0, 270.0)]}
def plug_after(ring, angle, width):
    """The angle just past a gap `width` wide at `angle` in wall `ring`, for a plug beside it."""
    return angle + math.degrees((width / 2.0 + PILLAR / 2.0 + 10.0) / WALLS[ring])


# Walls of wheels across an aisle, from pillar to pillar (a pillar stands in the walls either side), by aisle.
PLUGS = {
    # Aisle 3 (between walls 2 and 3): past the spine's turn in at 270, closed at first and a way into South 3 once
    # eaten; and between North 3 and West 3.
    3: [plug_after(2, 270.0, SPINE), 135.0],
    # Aisle 2 (between walls 1 and 2): before the spine's way in at 225, closed at first and a way from West 2 into
    # the spine once eaten; and between North 2 and West 2.
    2: [225.0 - (plug_after(1, 225.0, SPINE) - 225.0), 157.5],
}


def norm(a):
    return a % 360.0


def in_arc(a, arc):
    a0, a1 = arc
    return (norm(a) - norm(a0)) % 360.0 <= (norm(a1) - norm(a0)) % 360.0


# ---------------------------------------------------------------- the cellar: floor, octagon walls, annexes, ceiling

def face_frame(phi):
    """The face of the octagon whose outward normal is at `phi`: its normal and its tangent (anticlockwise)."""
    n = (math.cos(math.radians(phi)), math.sin(math.radians(phi)))
    t = (-n[1], n[0])
    return n, t


def at(phi, u, v):
    """The point `v` out from the middle along the normal at `phi` and `u` along the face."""
    n, t = face_frame(phi)
    return (n[0] * v + t[0] * u, n[1] * v + t[1] * u)


def ground():
    kit.ground("Ground", (-4900, -4900), (4900, 4900), flags())


def octagon():
    """The cellar's eight walls, up through the ceiling. Doorways in the four corner faces into the stairs and the
    rooms; drains low in the four side faces, where the mice come in."""
    f = "Cellar/Walls"
    half_face = APOTHEM * math.tan(math.radians(22.5))
    for k in range(8):
        phi = 45.0 * k
        openings = OPENINGS.get(k, [])
        # The wall's pieces between its openings, and over each opening.
        edges = [-half_face - 80.0]
        for (u0, u1, top) in sorted(openings):
            edges += [u0, u1]
        edges.append(half_face + 80.0)
        for i in range(0, len(edges), 2):
            a, b = at(phi, edges[i], APOTHEM + WALL / 2.0), at(phi, edges[i + 1], APOTHEM + WALL / 2.0)
            wall_between(f"Wall_{k}_{i}", f, a, b, WALL, -400, CEILING_TOP, stone())
        for j, (u0, u1, top) in enumerate(sorted(openings)):
            a, b = at(phi, u0, APOTHEM + WALL / 2.0), at(phi, u1, APOTHEM + WALL / 2.0)
            wall_between(f"Wall_{k}_Over_{j}", f, a, b, WALL, top, CEILING_TOP, stone())


# The openings in each face (index k: its normal at 45 k degrees): (from, to, top) along it.
DOOR = 200.0
OPENINGS = {}


def annex_openings():
    for k in range(8):
        phi = 45.0 * k
        if phi in (45.0, 225.0):
            # A stair: the spine's way in, and a doorway either side of it.
            OPENINGS[k] = [(-SPINE / 2.0, SPINE / 2.0, 300.0), (-700.0, -700.0 + DOOR, 220.0), (500.0, 500.0 + DOOR, 220.0)]
        elif phi in (135.0, 315.0):
            # A room: a door either side of its cistern.
            OPENINGS[k] = [(-700.0, -700.0 + DOOR, 220.0), (500.0, 500.0 + DOOR, 220.0)]
        else:
            # A drain either side of the middle.
            OPENINGS[k] = [(-800.0 - 100.0, -800.0 + 100.0, 120.0), (800.0 - 100.0, 800.0 + 100.0, 120.0)]


def annex_walls(name, phi, depth=900.0, width=800.0):
    """The room behind a corner face: side walls and a back wall, out to `depth` beyond the face."""
    f = f"Cellar/{name}"
    back = APOTHEM + WALL + depth
    for side in (-1, 1):
        wall_between(f"{name}_Side_{side}", f, at(phi, side * (width + WALL / 2.0), APOTHEM), at(phi, side * (width + WALL / 2.0), back + WALL), WALL, -400, CEILING_TOP, stone())
    wall_between(f"{name}_Back", f, at(phi, -width - WALL, back + WALL / 2.0), at(phi, width + WALL, back + WALL / 2.0), WALL, -400, CEILING_TOP, stone())


def drains(k):
    """Each drain in face k: a dark culvert behind iron bars, which the mice come through."""
    phi = 45.0 * k
    f = "Cellar/Drains"
    for j, (u0, u1, top) in enumerate(OPENINGS[k]):
        # The culvert behind it, closed.
        a, b = at(phi, u0 - 20, APOTHEM + WALL + 150), at(phi, u1 + 20, APOTHEM + WALL + 150)
        wall_between(f"Drain_{k}_{j}_Back", f, a, b, 100, -50, top + 40, painted("Dark"))
        for side, u in (("L", u0 - 30), ("R", u1 + 30)):
            wall_between(f"Drain_{k}_{j}_Side{side}", f, at(phi, u, APOTHEM + WALL), at(phi, u, APOTHEM + WALL + 150), 60, -50, top + 40, painted("Dark"))
        wall_between(f"Drain_{k}_{j}_Floor", f, at(phi, u0, APOTHEM + 50), at(phi, u1, APOTHEM + 50), 300, -30, 0, painted("Dark"))
        # Its bars: they stop players, not the mice (who come through anything as they arrive).
        n = 7
        for i in range(n):
            u = u0 + (u1 - u0) * (i + 0.5) / n
            p = at(phi, u, APOTHEM + 20)
            box(f"Drain_{k}_{j}_Bar_{i}", f, (p[0] - 3, p[1] - 3, 0), (p[0] + 3, p[1] + 3, top), painted("Dark"))
        wall_between(f"Drain_{k}_{j}_Rail", f, at(phi, u0, APOTHEM + 20), at(phi, u1, APOTHEM + 20), 8, top - 8, top, painted("Dark"))


def ceiling():
    """The cellar's ceiling, overhead() (it blocks nothing): pieces round the vault, out past the walls, with ribs
    under it along the rings of pillars. The vault rises above it to its dome and oculus."""
    f = "Cellar/Ceiling"
    n = 36
    far = 4900.0 * 1.42
    width = 2.0 * far * math.tan(math.radians(180.0 / n)) + 20.0
    for i in range(n):
        a = i * 360.0 / n
        c = polar(O, (VAULT[0] + far) / 2.0, a)
        overhead(solid(f"Ceiling_{i}", f, (c[0], c[1], (CEILING + CEILING_TOP) / 2.0), (far - VAULT[0], width, CEILING_TOP - CEILING), vaulting(), rotation=(0, a, 0)))
    # Ribs from pillar to pillar along each ring, and from each wall's pillars out to the next.
    for ring, r in WALLS.items():
        angles = sorted(set(norm(a) for a in PILLARS[ring]))
        for i, a in enumerate(angles):
            b = angles[(i + 1) % len(angles)] + (360.0 if i + 1 == len(angles) else 0.0)
            overhead(wall_between(f"Rib_{ring}_{i}", f, polar(O, r, a), polar(O, r, b), 50, CEILING - 50, CEILING, stone()))


def vault():
    """The round vault in the middle: a stone wall with six arches (the four ways and the two spines), a dome over it
    with an oculus to the sky, a gallery round the inside in two halves with a stair up to each, and the Mother Wheel
    in the middle (wax, never eaten) on its plinth, whose top the gallery's two bridges reach."""
    f = "Vault"
    arches = [(a, ARCH[0]) for a in (0.0, 90.0, 180.0, 270.0, 45.0, 225.0)]
    kit.round_wall("Vault_Wall", f, O, VAULT[0], VAULT[1], 0, ARCH[1], stone(), arches)
    kit.round_wall("Vault_Upper", f, O, VAULT[0], VAULT[1], ARCH[1], VAULT_TOP + 20, stone())
    # The dome: three steps up to the oculus, and the shaft above it.
    for k, (r0, r1, z0, z1) in enumerate(((560.0, 940.0, VAULT_TOP, VAULT_TOP + 70), (360.0, 600.0, VAULT_TOP + 60, VAULT_TOP + 140), (200.0, 400.0, VAULT_TOP + 130, VAULT_TOP + 210))):
        for p in kit.round_wall(f"Vault_Dome_{k}", f + "/Dome", O, r0, r1, z0, z1, vaulting()):
            overhead(p)
    for p in kit.round_wall("Vault_Oculus", f + "/Dome", O, 200.0, 260.0, VAULT_TOP + 200, VAULT_TOP + 600, stone()):
        overhead(p)
    # The Mother Wheel.
    cylinder("Vault_Plinth", f, 0, 0, 0, MOTHER + 60, 40, stone())
    wheel("Cheese_MotherWheel", "MotherWheel", 0, 0, 40, MOTHER, GALLERY_Z - 40, edible=False, mesh=wheel_mesh())
    # Its maker's band round its middle, in black wax.
    cylinder("Vault_MotherWheel_Band", f, 0, 0, 165, MOTHER + 6, 50, painted("Dark"))
    # The gallery: two arcs of deck on posts, over the arches; a stair up to each in the gap before it.
    for half in (0, 1):
        g0, g1 = rot(160.0, half), rot(302.0, half)
        span = g1 - g0
        n = int(math.ceil(span / 6.0))
        for i in range(n):
            a = g0 + span * (i + 0.5) / n
            c = polar(O, mid(GALLERY), a)
            length = 2.0 * GALLERY[1] * math.tan(math.radians(span / n / 2.0)) + 4
            solid(f"Gallery_{half}_{i}", f + "/Gallery", (c[0], c[1], GALLERY_Z - 12.5), (GALLERY[1] - GALLERY[0] + 40, length, 25), timber(), rotation=(0, a, 0))
            if i % 3 == 1:
                p = polar(O, GALLERY[0] + 20, a)
                box(f"Gallery_{half}_Post_{i}", f + "/Gallery", (p[0] - 12, p[1] - 12, 0), (p[0] + 12, p[1] + 12, GALLERY_Z - 25), painted("Dark"))
        # The stair: up along a chord from the floor under the other gallery's end to this one's.
        r = 780.0
        top, foot = polar(O, r, rot(300.0, half)), polar(O, r, rot(340.0, half))
        kit.stairs_between(f"Gallery_{half}_Stairs", f + "/Gallery", (foot[0], foot[1], 0), (top[0], top[1], GALLERY_Z), 140, kit.steel())
        # Its parapet: a low wall of small wheels along the inner edge, which the mice come up and eat; a rail stays.
        k = 0
        a = g0 + 4.0
        bridge_at = rot(270.0, half)
        step = math.degrees(84.0 / (GALLERY[0] + 50))
        while a < g1 - 8.0:
            if abs(a - bridge_at) > 12.0:
                # Open where the bridge to the Mother Wheel leaves it.
                p = polar(O, GALLERY[0] + 50, a)
                for layer in (0, 1):
                    wheel(f"Cheese_Parapet_{half}_{k}_{layer}", f"Parapet{team_of(half)}", p[0], p[1], GALLERY_Z + layer * 50, 80, 50, mesh=wheel_mesh())
                k += 1
            a += step
        # The rail along its edge, a metre high, open where the bridge leaves it.
        r_rail = GALLERY[0] + 4
        pieces = int(math.ceil((g1 - g0) / 8.0))
        for i in range(pieces):
            b0, b1 = g0 + (g1 - g0) * i / pieces, g0 + (g1 - g0) * (i + 1) / pieces
            if abs((b0 + b1) / 2.0 - bridge_at) < 8.0:
                continue
            for z in (GALLERY_Z + 94, GALLERY_Z + 48):
                wall_between(f"Gallery_{half}_Rail_{i}_{int(z)}", f + "/Gallery", polar(O, r_rail, b0), polar(O, r_rail, b1), 8, z - 6, z, painted("Dark"))
            p = polar(O, r_rail, b0)
            box(f"Gallery_{half}_RailPost_{i}", f + "/Gallery", (p[0] - 4, p[1] - 4, GALLERY_Z), (p[0] + 4, p[1] + 4, GALLERY_Z + 94), painted("Dark"))
        # The bridge from its middle to the Mother Wheel's top: a deck of cheese on beams of wax.
        y = -1 if half == 0 else 1
        kit.bridge(f"Bridge_{team_of(half)}", f"Bridge{team_of(half)}", (0, y * (GALLERY[0] + 20), GALLERY_Z), (0, y * (MOTHER / 2.0 - 60), GALLERY_Z), width=200,
                   beams=2, beam=40, wax_beams=True, supports=((GALLERY[0] + 20 - (MOTHER / 2.0 - 60)) / 2.0,))


# ---------------------------------------------------------------- the rings of the maze

def wall_features(ring):
    """Every blocker in wall `ring` all round: (from, to, kind) in degrees, sorted, kind "pillar" or "gap"."""
    r = WALLS[ring]
    found = []
    half_gap = lambda w: math.degrees(w / 2.0 / r)
    gaps = [(rot(a, h), w) for (a, w) in GAPS[ring] for h in (0, 1)]
    plugs = [rot(a, h) for h in (0, 1) for a in PLUGS.get(ring, []) + PLUGS.get(ring + 1, [])]
    pillars = set(norm(a) for a in PILLARS[ring]) | set(norm(a) for a in plugs)
    for a, w in gaps:
        found.append((a - half_gap(w), a + half_gap(w), "gap"))
    hp = math.degrees(PILLAR / 2.0 / r)
    for a in sorted(pillars):
        # No pillar in a gap.
        if any(in_arc(a, (g[0] - hp, g[1] + hp)) for g in found if g[2] == "gap"):
            continue
        found.append((a - hp, a + hp, "pillar"))
    found.sort(key=lambda b: norm(b[0]))
    return found


def ring_wall(ring):
    r = WALLS[ring]
    blockers = wall_features(ring)
    waxed = [(rot(a0, h), rot(a1, h)) for (a0, a1) in WAXED.get(ring, []) for h in (0, 1)]
    # Wax boundaries split the arcs between blockers too, so that each arc is one or the other.
    splits = sorted(set(norm(a) for arc in waxed for a in arc))
    f = f"Maze/Pillars_{ring}"
    for i, (a0, a1, kind) in enumerate(blockers):
        if kind == "pillar":
            a = (a0 + a1) / 2.0
            c = polar(O, r, a)
            solid(f"Pillar_{ring}_{i}", f, (c[0], c[1], CEILING / 2.0), (PILLAR, PILLAR, CEILING), stone(), rotation=(0, a, 0))
            solid(f"Pillar_{ring}_{i}_Base", f, (c[0], c[1], 15), (PILLAR + 24, PILLAR + 24, 30), stone(), rotation=(0, a, 0))
            solid(f"Pillar_{ring}_{i}_Cap", f, (c[0], c[1], CEILING - 60), (PILLAR + 30, PILLAR + 30, 30), stone(), rotation=(0, a, 0))
    for i, (a0, a1, kind) in enumerate(blockers):
        # The arc of wheels from this blocker to the next, cut where the wax starts or stops.
        start = a1
        stop = start + (blockers[(i + 1) % len(blockers)][0] - start) % 360.0
        cuts = [start] + sorted(start + (s - start) % 360.0 for s in splits if 0 < (s - start) % 360.0 < stop - start) + [stop]
        for j, (c0, c1) in enumerate(zip(cuts, cuts[1:])):
            if math.radians(c1 - c0) * r < 20:
                continue
            middle = (c0 + c1) / 2.0
            wax = any(in_arc(middle, w) for w in waxed)
            kit.wheel_arc(f"W{ring}_{i}_{j}", sector_of(middle, ring), O, r, c0, c1, 0, WHEEL, COURSES, edible=not wax, mesh=wheel_mesh(), overlap=PRESS)


def sector_of(angle, ring):
    """The structure a block of wall `ring` at `angle` belongs to (the name the notes use)."""
    a = norm(angle)
    if in_arc(a, (225.0, 270.0)):
        return "GoudaSpine"
    if in_arc(a, (45.0, 90.0)):
        return "MozzaSpine"
    side = "North" if in_arc(a, (90, 135)) else "West" if in_arc(a, (135, 225)) else "South" if in_arc(a, (270, 315)) else "East"
    return f"{side}Wall{ring}"


def plugs():
    """Walls of wheels across the aisles, from pillar to pillar."""
    for ring, angles in PLUGS.items():
        inner, outer = WALLS[ring - 1] + PILLAR / 2.0, WALLS[ring] - PILLAR / 2.0
        for h in (0, 1):
            for a in angles:
                b = rot(a, h)
                p, q = polar(O, inner, b), polar(O, outer, b)
                name = f"Plug{ring}_{int(round(norm(b)))}"
                kit.wheel_line(name, name, p, q, 0, 260.0, COURSES, mesh=wheel_mesh(), overlap=PRESS)


def spine(half):
    """A side's spine: from the middle of its stair's doorway in through the gutter to aisle 3, between walls of wax."""
    phi = rot(225.0, half)
    team = team_of(half)
    n, t = face_frame(phi)
    side = SPINE / 2.0 + WHEEL / 2.0
    for s in (-1, 1):
        # From the wheel of wall 3 beside the spine's gap out to the cellar's wall.
        inner = math.sqrt(WALLS[3] ** 2 - side ** 2) + WHEEL / 2.0
        p, q = at(phi, s * side, inner), at(phi, s * side, APOTHEM)
        kit.wheel_line(f"{team}Spine_Gutter_{s}", f"{team}Spine", p, q, 0, WHEEL, COURSES, edible=False, mesh=wheel_mesh(), overlap=PRESS)


# ---------------------------------------------------------------- the stairs (a side's start) and the rooms

def stair(half):
    """A side's stair down into the cellar: a room behind the corner face with three ways out (the spine in the
    middle, the gutter either side), stone blocks inside each so that nothing outside sees in, eight starts behind
    them, and the stair itself along the back wall, up to the shop's door."""
    phi = rot(225.0, half)
    team = team_of(half)
    name = f"{team}Stair"
    f = f"Stairs/{team}"
    annex_walls(name, phi)
    v0 = APOTHEM + WALL
    # A screen across the room behind the doorways, with a way round it at either end: whoever comes in turns, and
    # no line from the cellar through a doorway reaches the starts behind it.
    wall_between(f"{name}_Screen", f, at(phi, -650, v0 + 250), at(phi, 650, v0 + 250), 100, 0, 300, stone())
    # The stair along the back wall, up to a landing and the door to the shop.
    back = v0 + 900
    foot, top = at(phi, -650, back - 90), at(phi, 330, back - 90)
    kit.stairs_between(f"{name}_Flight", f, (foot[0], foot[1], 0), (top[0], top[1], 340), 160, stone())
    land0, land1 = at(phi, 330, back - 90), at(phi, 790, back - 90)
    wall_between(f"{name}_Landing", f, land0, land1, 180, 0, 340, stone())
    # The shop's door at the top, shut: planks in a stone frame.
    wall_between(f"{name}_ShopDoor", f, at(phi, 470, back - 4), at(phi, 650, back - 4), 12, 340, 560, oak())
    for u in (455, 665):
        wall_between(f"{name}_ShopDoor_Jamb_{u}", f, at(phi, u - 15, back - 10), at(phi, u + 15, back - 10), 24, 340, 580, stone())
    # Starts in two rows, facing the way out.
    points = [at(phi, u, v) for u in (-375.0, -125.0, 125.0, 375.0) for v in (v0 + 450, v0 + 650)]
    kit.team_starts(team, points, yaw=phi + 180.0, name="Stair_")
    kit.point_lamp(f"{name}_Lamp", f, *at(phi, 0, v0 + 450), CEILING - 90, intensity=10.0, radius=1200.0,
                   temperature=2600.0 if half == 0 else 7500.0)


def room(half):
    """The room behind the other corner face: the Brine Room (north-west), where wheels soak in stone tanks, or the
    Smoke Room (south-east), round a brick smoker. Two doors from the gutter, either side of the cistern that stops
    the gutter there; racks of wheels along the back wall."""
    phi = rot(135.0, half)
    name = "BrineRoom" if half == 0 else "SmokeRoom"
    f = f"Rooms/{name}"
    annex_walls(name, phi)
    for j, (u0, u1, top) in enumerate(OPENINGS[int(round(norm(phi) / 45.0))]):
        p = at(phi, (u0 + u1) / 2.0, APOTHEM + WALL / 2.0)
        kit.door(f"{name}_Door_{j}", f, (p[0], p[1], 0), phi, u1 - u0, top, jambs=False, leaves=oak())
    # The cistern across the gutter in front of the face, between the doors: stone, never eaten.
    a, b = at(phi, 0, WALLS[3] + PILLAR / 2.0), at(phi, 0, APOTHEM)
    wall_between(f"{name}_Cistern", f, a, b, 500, 0, 120, stone())
    wall_between(f"{name}_CisternBack", f, at(phi, 0, APOTHEM - 150), b, 500, 120, 300, stone())
    v0 = APOTHEM + WALL
    back = v0 + 900
    if half == 0:
        # Two brine tanks: stone kerbs a metre high round dark brine, with wheels floating in it.
        for side in (-1, 1):
            u0, u1 = side * 150.0, side * 560.0
            lo, hi = min(u0, u1), max(u0, u1)
            for label, (a0, b0) in (("Front", ((lo, v0 + 320), (hi, v0 + 320))), ("Back", ((lo, v0 + 520), (hi, v0 + 520))),
                                    ("Left", ((lo, v0 + 320), (lo, v0 + 520))), ("Right", ((hi, v0 + 320), (hi, v0 + 520)))):
                wall_between(f"{name}_Tank_{side}_{label}", f, at(phi, *a0), at(phi, *b0), 24, 0, 95, stone())
            wall_between(f"{name}_Tank_{side}_Brine", f, at(phi, lo, v0 + 420), at(phi, hi, v0 + 420), 190, 0, 70, painted("Teal"))
            for i, u in enumerate((lo + 70, (lo + hi) / 2.0, hi - 70)):
                p = at(phi, u, v0 + 420)
                wheel(f"Cheese_{name}_Brine_{side}_{i}", name, p[0], p[1], 70, 110, 40, mesh=wheel_mesh())
    else:
        # The smoker: a brick oven against the back wall, its mouth dark.
        wall_between(f"{name}_Smoker", f, at(phi, -200, back - 160), at(phi, 200, back - 160), 320, 0, 260, vaulting())
        wall_between(f"{name}_Smoker_Mouth", f, at(phi, -110, back - 322), at(phi, 110, back - 322), 6, 40, 160, painted("Dark"))
        wall_between(f"{name}_Smoker_Flue", f, at(phi, -90, back - 120), at(phi, 90, back - 120), 160, 260, CEILING, vaulting())
        # Its embers: a low red glow in its mouth.
        kit.point_lamp(f"{name}_Embers", f, *at(phi, 0, back - 360), 70, intensity=4.0, radius=500.0, shadows=False, fitting=None, colour=(255, 90, 30))
        # Wheels waiting to be smoked, stacked before it.
        for side in (-1, 1):
            p = at(phi, side * 380, v0 + 420)
            kit.wheel_stack(f"{name}_Waiting_{side}", name, p[0], p[1], 0, 130, 50, 2, mesh=wheel_mesh())
    for side in (-1, 1):
        lo, hi = (-750.0, -300.0) if side < 0 else (300.0, 750.0)
        kit.cheese_rack(f"{name}_Rack_{side}", name, at(phi, lo, back - 40), at(phi, hi, back - 40), timber(), wheel_size=(90.0, 40.0), mesh=wheel_mesh())
    kit.point_lamp(f"{name}_Lamp", f, *at(phi, 0, APOTHEM + 450), CEILING - 90, intensity=8.0, radius=1100.0,
                   temperature=6800.0 if half == 0 else 2700.0)


def gutter(half):
    """Gouda's half of the gutter (the West and North Gutters, between the stair and the rooms; Mozza's are these
    turned): racks of wheels along the walls between the drains, a column of wheels where the gutter turns past the
    rooms, so that its line breaks there, and wheels to hide behind."""
    for phi_g in (180.0, 90.0):
        phi = rot(phi_g, half)
        for side in (-1, 1):
            lo, hi = (-600.0, -60.0) if side < 0 else (60.0, 600.0)
            kit.cheese_rack(f"GutterRack_{int(norm(phi))}_{side}", f"GutterRacks{team_of(half)}", at(phi, lo, APOTHEM - 40),
                            at(phi, hi, APOTHEM - 40), timber(), wheel_size=(90.0, 40.0), mesh=wheel_mesh())
    for a in (157.5, 112.5):
        b = rot(a, half)
        kit.wheel_line(f"GutterTurn_{int(norm(b))}", f"GutterTurns{team_of(half)}", polar(O, CORNER - 160, b), polar(O, CORNER - 560, b), 0,
                       200.0, COURSES, mesh=wheel_mesh(), overlap=PRESS)
    # Wheels to hide behind: a column (whole cover) at each face's middle, low stacks (crouching) either side.
    for a, r, layers in ((180.0, 3180.0, 3), (196.0, 3230.0, 2), (166.0, 3150.0, 2), (90.0, 3180.0, 3), (104.0, 3150.0, 2), (76.0, 3230.0, 2)):
        b = rot(a, half)
        p = polar(O, r, b)
        kit.wheel_stack(f"GutterStack_{int(norm(b))}", f"GutterStacks{team_of(half)}", p[0], p[1], 0, 180.0 if layers == 3 else 150.0,
                        100.0 if layers == 3 else 50.0, layers, mesh=wheel_mesh())


def cover():
    """Low stacks of wheels to crouch behind: round the ambulatory, on the vault's floor, along aisle 3."""
    for k in range(8):
        a = 22.5 + 45.0 * k
        p = polar(O, mid(AISLES[1]), a)
        kit.wheel_stack(f"Ambulatory_{k}", "AmbulatoryStacks", p[0], p[1], 0, 140.0, 50.0, 2, mesh=wheel_mesh())
        p = polar(O, 520.0, a)
        kit.wheel_stack(f"VaultFloor_{k}", "VaultStacks", p[0], p[1], 0, 120.0, 50.0, 2, mesh=wheel_mesh())
    for half in (0, 1):
        for a in (180.0, 112.5):
            b = rot(a, half)
            p = polar(O, mid(AISLES[3]), b)
            kit.wheel_stack(f"Aisle3_{int(norm(b))}", "AisleStacks", p[0], p[1], 0, 150.0, 50.0, 2, mesh=wheel_mesh())


def stripes(half):
    """A side's colour on its stair: a painted band along the screen, so that whoever comes in knows whose it is."""
    phi = rot(225.0, half)
    colour = "Orange" if half == 0 else "Blue"
    v = APOTHEM + WALL + 250 - 52
    wall_between(f"{team_of(half)}Stair_Band", f"Stairs/{team_of(half)}", at(phi, -640, v), at(phi, 640, v), 4, 110, 150, painted(colour))


# ---------------------------------------------------------------- what a match needs

def free_starts():
    """Starts that free-for-all adds, through every part of the maze, facing along it."""
    points = []
    for half in (0, 1):
        for name, r, a, face in (("West3", mid(AISLES[3]), 200.0, 90.0), ("North3", mid(AISLES[3]), 125.0, -90.0),
                                 ("West2", mid(AISLES[2]), 185.0, 90.0), ("North2", mid(AISLES[2]), 105.0, -90.0),
                                 ("Ambulatory", mid(AISLES[1]), 135.0, 90.0), ("WestGutter", 3150.0, 207.0, 90.0),
                                 ("NorthGutter", 3150.0, 128.0, -90.0), ("Room", 0, 0, 0)):
            if name == "Room":
                phi = rot(135.0, half)
                p = at(phi, 0, APOTHEM + WALL + 150)
                points.append((p[0], p[1], phi + 90.0, 0, f"{team_of(half)}_{name}"))
                continue
            b = rot(a, half)
            p = polar(O, r, b)
            points.append((p[0], p[1], b + face, 0, f"{team_of(half)}_{name}"))
    kit.free_starts(points)


def mouse_entries():
    """The mice come in at the drains: two in each side face, eight in all, facing in."""
    for k in (0, 2, 4, 6):
        phi = 45.0 * k
        for (u0, u1, top) in OPENINGS[k]:
            p = at(phi, (u0 + u1) / 2.0, APOTHEM + 60)
            kit.mouse_entry(p[0], p[1], 0, phi + 180.0)


def places():
    """Names for every part, from the rings and angles: each a few boxes on a grid of 2.5 m, none overlapping another's."""
    step = 250.0
    cells = {}
    n = int(4900 // step)
    for i in range(-n, n):
        for j in range(-n, n):
            x, y = (i + 0.5) * step, (j + 0.5) * step
            name = name_at(x, y)
            if name:
                cells[(i, j)] = name
    # Rows of cells with one name, joined into rectangles where the rows above and below match.
    runs = []
    for j in range(-n, n):
        i = -n
        while i < n:
            name = cells.get((i, j))
            if not name:
                i += 1
                continue
            k = i
            while cells.get((k + 1, j)) == name:
                k += 1
            runs.append([name, i, k, j, j])
            i = k + 1
    merged = []
    for run in runs:
        for m in merged:
            if m[0] == run[0] and m[1] == run[1] and m[2] == run[2] and m[4] == run[3] - 1:
                m[4] = run[3]
                break
        else:
            merged.append(run)
    areas = {}
    for name, i0, i1, j0, j1 in merged:
        z0, z1 = (-50.0, 330.0) if name == "The Vault" else (-50.0, 900.0)
        areas.setdefault(name, []).append(((i0 * step, j0 * step, z0), ((i1 + 1) * step, (j1 + 1) * step, z1)))
    for name, boxes in sorted(areas.items()):
        kit.place_areas(name, boxes)
    # Over the vault's floor: its gallery and the top of the Mother Wheel.
    kit.place_areas("The Gallery", [((-VAULT[0], -VAULT[0], 330.0), (VAULT[0], VAULT[0], 900.0))])
    kit.place_name("The Mother Wheel", (-MOTHER / 2.0, -MOTHER / 2.0, 330.0), (MOTHER / 2.0, MOTHER / 2.0, 900.0))


def name_at(x, y):
    r = math.hypot(x, y)
    a = norm(math.degrees(math.atan2(y, x)))
    # The rooms behind the corner faces.
    for phi, name in ((225.0, "Gouda Stair"), (45.0, "Mozza Stair"), (135.0, "The Brine Room"), (315.0, "The Smoke Room")):
        n, t = face_frame(phi)
        v, u = x * n[0] + y * n[1], x * t[0] + y * t[1]
        if v > APOTHEM and abs(u) < 900 and v < APOTHEM + 1100:
            return name
    if any(x * math.cos(math.radians(45 * k)) + y * math.sin(math.radians(45 * k)) > APOTHEM for k in range(8)):
        return None
    if r < VAULT[1]:
        return "The Vault"
    if r < AISLES[1][1]:
        return "The Ambulatory"
    if in_arc(a, (225.0, 270.0)) and r < AISLES[3][1] + WHEEL / 2.0:
        return "Gouda Spine"
    if in_arc(a, (45.0, 90.0)) and r < AISLES[3][1] + WHEEL / 2.0:
        return "Mozza Spine"
    for phi, name in ((225.0, "Gouda Spine"), (45.0, "Mozza Spine")):
        n, t = face_frame(phi)
        v, u = x * n[0] + y * n[1], x * t[0] + y * t[1]
        if v > 0 and abs(u) < SPINE / 2.0 + WHEEL and r >= AISLES[3][1]:
            return name
    side = "North" if in_arc(a, (45, 135)) else "West" if in_arc(a, (135, 225)) else "South" if in_arc(a, (225, 315)) else "East"
    if r < AISLES[2][1] + WHEEL / 2.0:
        return f"{side} 2"
    if r < AISLES[3][1] + WHEEL / 2.0:
        return f"{side} 3"
    return f"{side} Gutter"


# ---------------------------------------------------------------- light

def lamps():
    """Lamps over the aisles, coloured by where they are: warm in the north and south, cool in the west and east,
    amber over the spines, so that a glance at the light says where you are."""
    f = "Lamps"

    def temperature(a, ring):
        if in_arc(a, (225.0, 270.0)) or in_arc(a, (45.0, 90.0)):
            return 2800.0
        if in_arc(a, (135.0, 225.0)) or in_arc(a, (315.0, 45.0)):
            return 5800.0
        return 3500.0

    def lamp(label, folder, x, y, z, shadows, **light):
        kit.point_lamp(label, folder, x, y, z, shadows=shadows, **light)
        # Hung from the ceiling on its cable.
        overhead(box(label + "_Cable", folder, (x - 1.5, y - 1.5, z + 24), (x + 1.5, y + 1.5, CEILING), painted("Dark")), "Lamp")

    # Shadows from every second lamp round the ambulatory, every third elsewhere: about twenty that cast them.
    for ring, (r0, r1), every, shadows in ((1, AISLES[1], 45.0, 2), (2, AISLES[2], 30.0, 3), (3, AISLES[3], 22.5, 3)):
        k = 0
        a = every / 2.0
        while a < 360.0:
            p = polar(O, (r0 + r1) / 2.0, a)
            lamp(f"Lamp_{ring}_{k}", f, p[0], p[1], CEILING - 120, k % shadows == 0, intensity=6.0, radius=900.0, temperature=temperature(a, ring))
            k += 1
            a += every
    k = 0
    a = 11.25
    while a < 360.0:
        p = polar(O, 3250.0, a)
        lamp(f"Lamp_Gutter_{k}", f, p[0], p[1], CEILING - 140, k % 3 == 0, intensity=5.0, radius=900.0, temperature=3800.0)
        k += 1
        a += 22.5
    for a in (0.0, 90.0, 180.0, 270.0):
        p = polar(O, mid(GALLERY), a + 22.5)
        kit.point_lamp(f"Lamp_Vault_{int(a)}", "Vault", p[0], p[1], GALLERY_Z - 60, intensity=4.0, radius=700.0, temperature=4200.0, shadows=False)


def build():
    annex_openings()
    ground()
    octagon()
    for k in (0, 2, 4, 6):
        drains(k)
    ceiling()
    vault()
    for ring in (1, 2, 3):
        ring_wall(ring)
    plugs()
    for half in (0, 1):
        spine(half)
        stair(half)
        stripes(half)
        room(half)
        gutter(half)
    cover()
    free_starts()
    mouse_entries()
    places()
    lamps()
    kit.nav_bounds((-4900, -4900, -100), (4900, 4900, 800))
    kit.light_rig("noon", sun=(-72, 30))


kit.build_level(LEVEL, build)
