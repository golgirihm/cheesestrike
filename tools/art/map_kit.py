"""The building blocks CheeseStrike's maps are scripted from (docs/unreal/maps/principles.md says how to use them well).

A map is a script in tools/art/ that builds its level from nothing, run in the editor with the editor closed:

    UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_<map>.py" -unattended -nullrhi

(both paths absolute). It starts by finding this module and ends by handing its build function to build_level:

    import os, sys, unreal
    sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
    import map_kit as kit

    def build():
        kit.ground("Ground", (-4000, -3000), (4000, 3000))
        kit.boundary(-4000, -3000, 4000, 3000, 600)
        kit.building("House_West", "HouseWest", (-2600, -800), (-1400, 800), storeys=2,
                     doors=[("E", 0)], windows=[("N", -2000, 1), ("E", 400, 1)], stairs=("S", -1600, -2400))
        kit.team_starts("Gouda", [(-3600, y) for y in range(-1400, 1401, 400)], yaw=0)
        kit.team_starts("Mozza", [(3600, y) for y in range(-1400, 1401, 400)], yaw=180)
        kit.mouse_entries(-4000, -3000, 4000, 3000, 600, xs=(-2000, 0, 2000), ys=(0,))
        kit.nav_bounds((-4000, -3000, -500), (4000, 3000, 1200))
        kit.light_rig("day")

    kit.build_level("/Game/CheeseStrike/Maps/Lvl_Example", build)

build_level opens the level (or makes it), empties it, checks that the level open is that one (the editor opens on the
shooter level, and a failed open would build into it), runs the build, gives the cheese in the kept areas (keep(),
team_starts) its wax and marks it uneaten, saves, and writes Saved/<level name>.txt: what was built, counted.

Units are centimetres, the world is life size (a player is 180 cm tall, steps 20 cm, jumps 55 cm), z is up and the
floor is at 0 unless a map says otherwise. Every function takes a `label` (the actor's name in the World Outliner) or
builds labels from a `name`, and puts what it makes in a folder, so a level reads as its script does.

Every block of cheese is an ACheeseStrikeCheese with the name of the structure it belongs to; the mice eat it from the
bottom, and what rested on an eaten block falls unless what else is under it still holds it (CheeseStrikeMice::IsHeld:
its middle within what it rests on). So build cheese that stands on something: on the ground, on other cheese, on a
frame; a lintel over each opening, wider than it; floors on beams or on walls either side.
"""

import math
import traceback
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MATS = "/Game/CheeseStrike/Materials/"
PROPS = "/Game/Fab/Megascans/Props/"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
# The maps' colours: corrugated sheets, painted metal and shipping containers come in each (build_materials.py).
COLOURS = ("Cream", "Teal", "Red", "Blue", "Yellow", "Orange", "Dark", "Grey")

lines = []      # what the report says, beyond the counts
counts = {}     # how many of each thing, by kind
kept = []       # areas whose cheese the mice leave: (low, high) corners


# ---------------------------------------------------------------- materials

_materials = {}


def material(name):
    """A material by its asset name in /Game/CheeseStrike/Materials (MI_ConcreteFloor, MI_Painted_Red, M_CheeseWax)."""
    if name not in _materials:
        _materials[name] = unreal.load_asset(MATS + name)
        if _materials[name] is None:
            raise RuntimeError(f"no material {MATS + name}")
    return _materials[name]


def floor():
    return material("MI_ConcreteFloor")


def concrete():
    return material("MI_ConcreteWall")


def asphalt():
    return material("MI_Asphalt")


def steel():
    return material("MI_Steel")


def roof():
    return material("MI_Roof")


def painted(colour):
    """Painted metal in one of COLOURS: rails, posts, frames, pipes."""
    return material("MI_Painted_" + colour)


def sheet(colour):
    """Corrugated sheet in one of COLOURS: sheds and walls that aren't cheese."""
    return material("MI_Corrugated_" + colour)


def wax():
    """The rind of the cheese the mice leave alone (wax_material.py)."""
    return material("M_CheeseWax")


# ---------------------------------------------------------------- placing things

def v(x, y, z):
    return unreal.Vector(float(x), float(y), float(z))


def count(kind, n=1):
    counts[kind] = counts.get(kind, 0) + n


def place(cls, label, folder, centre, rotation=(0, 0, 0)):
    """An actor of `cls` at `centre`, turned by `rotation` (pitch, yaw, roll in degrees)."""
    actor = eas.spawn_actor_from_class(cls, v(*centre), unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def solid(label, folder, centre, size, material, mesh=None, rotation=(0, 0, 0)):
    """A static block of `size` (cm, along its own axes) centred on `centre`: the engine's cube, or `mesh` a metre across."""
    actor = place(unreal.StaticMeshActor, label, folder, centre, rotation)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh or CUBE)
    component.set_material(0, material)
    actor.set_actor_scale3d(v(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    count(folder.split("/")[0])
    return actor


def box(label, folder, low, high, material):
    """A static block from corner `low` to corner `high`."""
    centre = [(a + b) / 2.0 for a, b in zip(low, high)]
    size = [abs(b - a) for a, b in zip(low, high)]
    return solid(label, folder, centre, size, material)


def cylinder(label, folder, x, y, bottom, diameter, height, material):
    """A standing cylinder (a post, a silo, a tank) on `bottom`."""
    return solid(label, folder, (x, y, bottom + height / 2.0), (diameter, diameter, height), material, CYLINDER)


def pipe_x(label, folder, x0, x1, y, z, diameter, material):
    """A pipe lying along x."""
    return solid(label, folder, ((x0 + x1) / 2.0, y, z), (diameter, diameter, abs(x1 - x0)), material, CYLINDER, (90, 0, 0))


def pipe_y(label, folder, x, y0, y1, z, diameter, material):
    """A pipe lying along y."""
    return solid(label, folder, (x, (y0 + y1) / 2.0, z), (diameter, diameter, abs(y1 - y0)), material, CYLINDER, (0, 0, 90))


def ramp(label, folder, start, end, width, material, thickness=40):
    """A walkable slab whose top runs from `start` to `end` (points on its top, at the middle of its width). Keep it under
    30 degrees; a player walks up anything under 45, but bots and mice go by the navigation mesh, which is built for 44."""
    dx, dy, dz = end[0] - start[0], end[1] - start[1], end[2] - start[2]
    run = math.hypot(dx, dy)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, run))
    p, w = math.radians(pitch), math.radians(yaw)
    up = (-math.sin(p) * math.cos(w), -math.sin(p) * math.sin(w), math.cos(p))
    middle = [(a + b) / 2.0 for a, b in zip(start, end)]
    centre = [middle[i] - up[i] * thickness / 2.0 for i in range(3)]
    return solid(label, folder, centre, (math.hypot(run, dz) + 20, width, thickness), material, rotation=(pitch, yaw, 0))


RISE = 18.0   # a stair's riser: under the 20 cm a player steps up without jumping
TREAD = 30.0


def stairs(label, folder, start, end, width, material, solid_under=True):
    """A flight of stairs along x or y, from `start` (the foot of the bottom step, on the floor) to `end` (the top of the
    flight, at the floor above): steps of at most RISE, each a block (down to the floor under it if `solid_under`).
    Its run must give each step at least 25 cm of tread: a 350 cm storey takes 20 steps, 6 m."""
    rise = end[2] - start[2]
    steps = max(1, int(math.ceil(rise / RISE)))
    along_x = abs(end[0] - start[0]) >= abs(end[1] - start[1])
    run = (end[0] - start[0]) if along_x else (end[1] - start[1])
    tread = run / steps
    if abs(tread) < 25:
        raise RuntimeError(f"{label}: {steps} steps in {abs(run):.0f} cm is too steep")
    for i in range(steps):
        top = start[2] + rise * (i + 1) / steps
        bottom = start[2] if solid_under else top - 20
        a = (start[0] + tread * i, start[1]) if along_x else (start[0], start[1] + tread * i)
        b = (start[0] + tread * (i + 1), start[1]) if along_x else (start[0], start[1] + tread * (i + 1))
        if along_x:
            low, high = (min(a[0], b[0]), start[1] - width / 2.0, bottom), (max(a[0], b[0]), start[1] + width / 2.0, top)
        else:
            low, high = (start[0] - width / 2.0, min(a[1], b[1]), bottom), (start[0] + width / 2.0, max(a[1], b[1]), top)
        box(f"{label}_{i}", folder, low, high, material)


def rect_minus(low, high, holes):
    """The rectangle from `low` to `high` (x, y) less the rectangles `holes`, as a few rectangles."""
    xs = sorted({low[0], high[0]} | {min(max(h[0][0], low[0]), high[0]) for h in holes} | {min(max(h[1][0], low[0]), high[0]) for h in holes})
    strips = []
    for x0, x1 in zip(xs, xs[1:]):
        if x1 <= x0:
            continue
        cuts = sorted((max(h[0][1], low[1]), min(h[1][1], high[1])) for h in holes if h[0][0] < x1 and h[1][0] > x0 and h[0][1] < high[1] and h[1][1] > low[1])
        spans, y = [], low[1]
        for c0, c1 in cuts:
            if c0 > y:
                spans.append((y, c0))
            y = max(y, c1)
        if y < high[1]:
            spans.append((y, high[1]))
        for s in spans:
            # Joined to the strip before it when it spans the same.
            for r in strips:
                if r[1][0] == x0 and r[0][1] == s[0] and r[1][1] == s[1]:
                    r[1] = (x1, s[1])
                    break
            else:
                strips.append([(x0, s[0]), (x1, s[1])])
    return [(tuple(a), tuple(b)) for a, b in strips]


def slab(label, folder, low, high, material, holes=()):
    """A slab (a floor, a deck, the ground) from `low` to `high`, with rectangular `holes` ((x0, y0), (x1, y1)) through it:
    a stairwell, a pit, a tunnel's mouth."""
    for i, (a, b) in enumerate(rect_minus(low, high, holes)):
        box(f"{label}_{i}", folder, (a[0], a[1], low[2]), (b[0], b[1], high[2]), material)


def mesh(name):
    """A Megascans model imported by import_fab.py: /Game/Fab/Megascans/Props/<name>/SM_<name>."""
    return unreal.load_asset(f"{PROPS}{name}/SM_{name}")


def fill(label, folder, model, low, high, yaw=0, materials=None):
    """A model scaled to fill the block from corner `low` to corner `high`, turned by `yaw` (a multiple of 90): cover
    keeps its block's size, and its collision is its box."""
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
    for index, m in enumerate(materials or []):
        component.set_material(index, m)
    actor.set_actor_scale3d(v(*scale))
    count(folder.split("/")[0])
    return actor


def prop(label, folder, name, x, y, z=0, yaw=0, scale=1.0):
    """A Megascans model at its own size (times `scale`), standing at (x, y, z)."""
    model = mesh(name)
    b = model.get_bounding_box()
    half = ((b.max.x - b.min.x) * scale / 2.0, (b.max.y - b.min.y) * scale / 2.0)
    if yaw % 180:
        half = (half[1], half[0])
    return fill(label, folder, model, (x - half[0], y - half[1], z), (x + half[0], y + half[1], z + (b.max.z - b.min.z) * scale), yaw)


def stack(label, folder, name, low, high, nx, ny, nz):
    """A block filled with nx by ny by nz of a model, side by side and on top of each other (crates: hard cover)."""
    model = mesh(name)
    sx, sy, sz = (high[0] - low[0]) / nx, (high[1] - low[1]) / ny, (high[2] - low[2]) / nz
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                a = (low[0] + i * sx, low[1] + j * sy, low[2] + k * sz)
                fill(f"{label}_{i}_{j}_{k}", folder, model, a, (a[0] + sx, a[1] + sy, a[2] + sz), 90 * ((i + j + k) % 2))


def shipping_container(label, folder, low, high, colour):
    """A shipping container (two stacked, for a block 5.2 m high) filling the block, long side along its longer side,
    painted `colour` (Blue, Red, Teal, Yellow, Cream): hard cover, never eaten."""
    model = unreal.load_asset("/Game/Fab/SousinhoGames/ShippingContainer/SM_Container")
    slots = [str(s.get_editor_property("material_slot_name")).lower() for s in model.get_editor_property("static_materials")]
    paint = [material("MI_Container_" + ("Frame" if "frame" in s else "Doors" if "door" in s else "Sides") + "_" + colour) for s in slots]
    yaw = 0 if abs(high[0] - low[0]) >= abs(high[1] - low[1]) else 90
    levels = max(1, round(abs(high[2] - low[2]) / 260.0))
    step = abs(high[2] - low[2]) / levels
    for level in range(levels):
        z0 = min(low[2], high[2]) + level * step
        fill(f"{label}_{level}", folder, model, (low[0], low[1], z0), (high[0], high[1], z0 + step), yaw + 180 * level, paint)


def decal(label, folder, name, at, size, rotation):
    """A decal (MI_Decal_<name>: OilStain, AsphaltCrack, Leakage, PaintedOne...) of `size` (depth, width, height) at
    `at`, projecting along its rotation's forward axis: (-90, 0, 0) projects down onto the floor."""
    actor = place(unreal.DecalActor, label, folder, at, rotation)
    component = actor.get_component_by_class(unreal.DecalComponent)
    component.set_decal_material(material("MI_Decal_" + name))
    component.set_editor_property("decal_size", v(*size))
    count("decals")
    return actor


def lamp_post(label, folder, x, y, height=700):
    """A yard lamp: a steel post with a head (dressing; no light of its own)."""
    cylinder(label, folder, x, y, 0, 18, height, steel())
    box(label + "_Head", folder, (x - 60, y - 15, height - 10), (x + 60, y + 15, height + 10), painted("Dark"))


# ---------------------------------------------------------------- the ground and the edge

def ground(label, low, high, material=None, depth=400, holes=()):
    """The ground: a slab whose top is at z = 0 (or low's z if three numbers are given), `depth` deep, from (x0, y0) to
    (x1, y1), with `holes` cut through it for pits, sunken lanes and tunnels' mouths. In the folder Ground, which the
    maps' tests measure the playable area by."""
    top = low[2] if len(low) > 2 else 0
    slab(label, "Ground", (low[0], low[1], top - depth), (high[0], high[1], top), material or floor(), holes)


def boundary(x0, y0, x1, y1, height, material=None, thickness=100, bottom=-400):
    """The outer wall all round the playable ground, `height` tall: the mice come over it (mouse_entries)."""
    m = material or concrete()
    f = "Ground/Boundary"
    box("Boundary_North", f, (x0 - thickness, y1, bottom), (x1 + thickness, y1 + thickness, height), m)
    box("Boundary_South", f, (x0 - thickness, y0 - thickness, bottom), (x1 + thickness, y0, height), m)
    box("Boundary_West", f, (x0 - thickness, y0, bottom), (x0, y1, height), m)
    box("Boundary_East", f, (x1, y0, bottom), (x1 + thickness, y1, height), m)


# ---------------------------------------------------------------- cheese

def keep(low, high):
    """An area (corners (x, y, z)) whose cheese the mice leave alone: build_level gives every block whose middle is in it
    wax and marks it uneaten. Use it for the walkable way through a map and round its starts (team_starts keeps a
    space round each start itself)."""
    kept.append((tuple(low), tuple(high)))


def cheese(label, structure, centre, size, mesh=None, material=None, edible=True, rotation=(0, 0, 0)):
    """A block of cheese: its own actor, part of `structure` (a name a map's notes use: "WheelStack", "HouseWest"), with a
    block's 400 of eating in it whatever its size. Not `edible`: the mice leave it, and it wears wax."""
    actor = place(unreal.CheeseStrikeCheese, label, "Cheese/" + structure, centre, rotation)
    actor.set_editor_property("structure", structure)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(mesh or CUBE)
    if not edible:
        actor.set_editor_property("edible", False)
        component.set_material(0, wax())
    elif material is not None:
        component.set_material(0, material)
    actor.set_actor_scale3d(v(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    count("cheese:" + structure)
    return actor


def cheese_box(label, structure, low, high, edible=True, material=None):
    """A block of cheese from corner `low` to corner `high`."""
    centre = [(a + b) / 2.0 for a, b in zip(low, high)]
    return cheese(label, structure, centre, [abs(b - a) for a, b in zip(low, high)], material=material, edible=edible)


def wheel(label, structure, x, y, bottom, diameter, height, edible=True, material=None, mesh=None):
    """A standing wheel of cheese on `bottom`: the engine's cylinder, or `mesh` (a metre across and high about its
    middle, as the cylinder is: wheel_mesh.py's SM_CheeseWheel has the rounded edges of a real wheel)."""
    return cheese(label, structure, (x, y, bottom + height / 2.0), (diameter, diameter, height), mesh or CYLINDER, material, edible)


def cheese_stack(name, structure, low, high, nx, ny, nz, edible=True):
    """A block of cheese built of nx by ny by nz blocks, each standing on the one under it: the mice eat it from the
    bottom, and the blocks above come down."""
    sx, sy, sz = (high[0] - low[0]) / nx, (high[1] - low[1]) / ny, (high[2] - low[2]) / nz
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                a = (low[0] + i * sx, low[1] + j * sy, low[2] + k * sz)
                cheese_box(f"Cheese_{name}_{i}_{j}_{k}", structure, a, (a[0] + sx, a[1] + sy, a[2] + sz), edible)


def wheel_stack(name, structure, x, y, bottom, diameter, height, layers, edible=True, mesh=None):
    """Wheels on top of one another."""
    for k in range(layers):
        wheel(f"Cheese_{name}_{k}", structure, x, y, bottom + k * height, diameter, height, edible, mesh=mesh)


def cheese_wall(name, structure, start, end, bottom, courses, thickness=50, column=200, openings=(), edible=True):
    """A wall of cheese along x or y from `start` (x, y) to `end`, its bottom at `bottom`, built in courses (the heights
    of its rows of blocks, from the bottom) of blocks `column` long, so that the mice eat it a block at a time and what
    is above a block they eat comes down.

    `openings` are doorways and windows: (from, to, z0, z1), measured along the wall from `start` and up from `bottom`.
    Over each, a lintel a column wider at either end bears on the wall either side; it falls once one side is eaten.
    Keep the courses' tops at an opening's top, and openings two columns apart. Returns the blocks."""
    along_x = abs(end[0] - start[0]) >= abs(end[1] - start[1])
    length = abs(end[0] - start[0]) if along_x else abs(end[1] - start[1])
    sign = 1 if ((end[0] - start[0]) if along_x else (end[1] - start[1])) >= 0 else -1
    n = max(1, int(round(length / column)))
    unit = length / n
    tops = []
    z = 0
    for h in courses:
        tops.append((z, z + h))
        z += h

    def opening_at(i, z0, z1):
        a, b = i * unit, (i + 1) * unit
        for (o0, o1, oz0, oz1) in openings:
            if o0 < b - 1 and o1 > a + 1 and oz0 < z1 - 1 and oz1 > z0 + 1:
                return (o0, o1, oz0, oz1)
        return None

    def block(label, a, b, z0, z1):
        p0 = (start[0] + sign * a, start[1]) if along_x else (start[0], start[1] + sign * a)
        p1 = (start[0] + sign * b, start[1]) if along_x else (start[0], start[1] + sign * b)
        if along_x:
            low, high = (min(p0[0], p1[0]), start[1] - thickness / 2.0, bottom + z0), (max(p0[0], p1[0]), start[1] + thickness / 2.0, bottom + z1)
        else:
            low, high = (start[0] - thickness / 2.0, min(p0[1], p1[1]), bottom + z0), (start[0] + thickness / 2.0, max(p0[1], p1[1]), bottom + z1)
        return cheese_box(label, structure, low, high, edible)

    made = []
    for course, (z0, z1) in enumerate(tops):
        # Each column of this course: a block, part of an opening, or under a lintel.
        # Over each opening ending at this course's foot, a column either side; openings closer than that share one.
        spans = sorted((max(0.0, math.floor((o0 + 1) / unit) * unit - unit), min(length, math.ceil((o1 - 1) / unit) * unit + unit))
                       for (o0, o1, oz0, oz1) in openings if abs(oz1 - z0) < 1)
        for (o0, o1, oz0, oz1) in openings:
            if abs(oz1 - z0) < 1 and (math.floor((o0 + 1) / unit) == 0 or math.ceil((o1 - 1) / unit) >= n):
                lines.append(f"WARNING: {name}: an opening in its end column; the lintel over it rests on one side only")
        lintels = []
        for a, b in spans:
            if lintels and a <= lintels[-1][1] + 1:
                lintels[-1] = (lintels[-1][0], max(lintels[-1][1], b))
            else:
                lintels.append((a, b))
        i = 0
        while i < n:
            a, b = i * unit, (i + 1) * unit
            lintel = next((l for l in lintels if l[0] <= a + 1 and b - 1 <= l[1]), None)
            if lintel is not None and opening_at(i, z0, z1) is None:
                # The lintel, in one block over the opening and a column either side.
                j = i
                while j < n and (j + 1) * unit <= lintel[1] + 1 and opening_at(j, z0, z1) is None:
                    j += 1
                made.append(block(f"Cheese_{name}_Lintel_{course}_{i}", a, j * unit, z0, z1))
                i = j
                continue
            if opening_at(i, z0, z1) is None:
                made.append(block(f"Cheese_{name}_{course}_{i}", a, b, z0, z1))
            i += 1
    return made


def pillar_deck(name, structure, low, high, slab=400, thickness=40, pillar=160, edge=0, edge_height=100, edible=True):
    """A floor of cheese slabs on pillars of cheese, from corner `low` (x, y, z: z the level below, where the pillars
    stand) to corner `high` (x, y, z: z the deck's top): slabs about `slab` cm square and `thickness` thick, each on one
    round pillar `pillar` across under its middle. Eaten at its foot, a pillar is gone and the slab on it falls, whole,
    onto the level below (CheeseStrikeMice::IsHeld: nothing under its middle holds it), with whoever stands on it.
    One pillar a slab, never more: a slab on two falls when either goes, and would come down through the one left.
    Keep the level below clear under the slabs (nothing there but their pillars), or a slab falling past what it can't
    rest on comes to rest through it. `edge`: a low wall of cheese, `edge_height` high and 50 thick, along that side of
    the deck ("N", "S", "E" or "W"), each block on one slab: crouching cover, which falls with its slab.
    Returns the slabs' footprints ((x0, y0), (x1, y1)), for what else stands on them."""
    x0, y0, bottom = low
    x1, y1, top = high
    nx = max(1, int(round((x1 - x0) / float(slab))))
    ny = max(1, int(round((y1 - y0) / float(slab))))
    sx, sy = (x1 - x0) / nx, (y1 - y0) / ny
    under = top - thickness
    slabs = []
    for i in range(nx):
        for j in range(ny):
            a = (x0 + i * sx, y0 + j * sy)
            b = (a[0] + sx, a[1] + sy)
            cx, cy = (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0
            wheel(f"Cheese_{name}_Pillar_{i}_{j}", structure, cx, cy, bottom, pillar, under - bottom, edible)
            cheese_box(f"Cheese_{name}_Slab_{i}_{j}", structure, (a[0], a[1], under), (b[0], b[1], top), edible)
            slabs.append((a, b))
            # The edge: two blocks along each slab on that side, on the slab only.
            on_edge = {"N": j == ny - 1, "S": j == 0, "E": i == nx - 1, "W": i == 0}.get(edge, False) if edge else False
            if on_edge:
                for k in range(2):
                    if edge in "NS":
                        ey = b[1] - 50 if edge == "N" else a[1]
                        lo, hi = (a[0] + k * sx / 2.0, ey, top), (a[0] + (k + 1) * sx / 2.0, ey + 50, top + edge_height)
                    else:
                        ex = b[0] - 50 if edge == "E" else a[0]
                        lo, hi = (ex, a[1] + k * sy / 2.0, top), (ex + 50, a[1] + (k + 1) * sy / 2.0, top + edge_height)
                    cheese_box(f"Cheese_{name}_Edge_{i}_{j}_{k}", structure, lo, hi, edible)
    count("pillar decks")
    return slabs


def cheese_floor(name, structure, low, high, tile=(200, 200), edible=True, holes=()):
    """A floor of cheese tiles from `low` to `high` (corners; its thickness is high's z less low's): each tile must
    stand on something, so lay it on the ground, on beams (frame) or on walls. Tiles whose middle is in a hole
    ((x0, y0), (x1, y1)) are left out."""
    nx = max(1, int(round((high[0] - low[0]) / tile[0])))
    ny = max(1, int(round((high[1] - low[1]) / tile[1])))
    sx, sy = (high[0] - low[0]) / nx, (high[1] - low[1]) / ny
    for i in range(nx):
        for j in range(ny):
            a = (low[0] + i * sx, low[1] + j * sy)
            b = (a[0] + sx, a[1] + sy)
            mx, my = (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0
            if any(h[0][0] <= mx <= h[1][0] and h[0][1] <= my <= h[1][1] for h in holes):
                continue
            cheese_box(f"Cheese_{name}_{i}_{j}", structure, (a[0], a[1], low[2]), (b[0], b[1], high[2]), edible)


# ---------------------------------------------------------------- frames, doors, buildings, tunnels and bridges

def frame_post(label, folder, x, y, bottom, top, size=24, colour="Dark"):
    """A steel post of the frame that is never eaten."""
    box(label, folder, (x - size / 2.0, y - size / 2.0, bottom), (x + size / 2.0, y + size / 2.0, top), painted(colour))


def rail(label, folder, start, end, z, height=100, colour="Yellow"):
    """A rail along x or y at a floor's edge: a top bar and a mid bar on posts, which shots and sight pass between."""
    along_x = abs(end[0] - start[0]) >= abs(end[1] - start[1])
    length = abs(end[0] - start[0]) if along_x else abs(end[1] - start[1])
    m = painted(colour)
    for bar, h in (("Top", height), ("Mid", height / 2.0)):
        if along_x:
            box(f"{label}_{bar}", folder, (min(start[0], end[0]), start[1] - 4, z + h - 6), (max(start[0], end[0]), start[1] + 4, z + h), m)
        else:
            box(f"{label}_{bar}", folder, (start[0] - 4, min(start[1], end[1]), z + h - 6), (start[0] + 4, max(start[1], end[1]), z + h), m)
    posts = max(1, int(length // 200))
    for i in range(posts + 1):
        t = i / posts
        x, y = start[0] + (end[0] - start[0]) * t, start[1] + (end[1] - start[1]) * t
        box(f"{label}_Post_{i}", folder, (x - 4, y - 4, z), (x + 4, y + 4, z + height), m)


def door(label, folder, at, yaw, width=200, height=220, double=True, leaves=None, jambs=True):
    """A door (ACheeseStrikeDoor) in a doorway whose bottom middle is `at`, its X through the doorway (`yaw` 0 for a
    wall along y, 90 for one along x): it slides open as anyone comes up to it (player, bot or mouse) and shuts behind
    them; the navigation mesh runs through it. Steel jambs either side stay when the wall round it is eaten."""
    actor = place(unreal.CheeseStrikeDoor, label, folder, at, (0, yaw, 0))
    actor.set_editor_property("width", float(width))
    actor.set_editor_property("height", float(height))
    actor.set_editor_property("double", bool(double))
    actor.set_editor_property("leaf_material", leaves or material("MI_GarageDoor"))
    if jambs:
        turn = math.radians(yaw)
        across = (-math.sin(turn), math.cos(turn))
        for side in (-1, 1):
            jx, jy = at[0] + across[0] * side * (width / 2.0 + 6), at[1] + across[1] * side * (width / 2.0 + 6)
            box(f"{label}_Jamb_{side}", folder, (jx - 6, jy - 6, at[2]), (jx + 6, jy + 6, at[2] + height - 12), painted("Dark"))
    count("doors")
    return actor


def interior_lamp(label, folder, x, y, z, intensity=12.0, radius=900.0, temperature=3600.0, size=(120, 40)):
    """A ceiling lamp, shining down: light for a room, a tunnel or under a floor, so that whoever is there can be seen."""
    light = place(unreal.RectLight, label, folder, (x, y, z), (-90, 0, 0))
    component = light.get_component_by_class(unreal.RectLightComponent)
    component.set_editor_property("intensity", intensity)
    component.set_editor_property("attenuation_radius", radius)
    component.set_editor_property("source_width", float(size[0]))
    component.set_editor_property("source_height", float(size[1]))
    component.set_editor_property("use_temperature", True)
    component.set_editor_property("temperature", temperature)
    box(label + "_Fitting", folder, (x - size[0] / 2.0, y - size[1] / 2.0, z), (x + size[0] / 2.0, y + size[1] / 2.0, z + 6), painted("Cream"))
    count("lamps")
    return light


SIDES = ("N", "S", "E", "W")


def building(name, structure, low, high, storeys=2, storey=350, wall=50, column=250, doors=(), windows=(), stairway=None,
             cheese_roof=True, deck=None, frame="Dark", door_material=None, lamps=True, edible=True, bottom=0):
    """A house of cheese, `storeys` high, on the floor at `bottom`: its walls are cheese (eaten from the bottom, so each
    column of a wall comes down as its foot is eaten, and a lintel over each opening falls once a side goes), and its
    upper floors are concrete decks on a steel frame inside the walls, which the mice never eat: once its walls are gone
    it stands as an open frame of floors, high ground with nothing to hide behind. Its roof is cheese planks across it
    from wall to wall, which fall onto the top floor when a wall under them goes (or none, if not `cheese_roof`).

    `low` and `high` are its outer corners (x, y). Each storey is `storey` high, the deck in its top 25 cm.
    `doors`: (side, at[, width]): side one of N, S, E, W; at the door's middle along that side (x for N and S, y for E
        and W); a door (door()) on the ground floor, 200 wide unless said and 220 high (a course's top).
    `windows`: (side, at, storey[, width]): a window in that storey (0 the ground), 200 wide unless said, from 100 to
        220 cm above its floor: chest high, to shoot from standing and hide below crouched.
    `stairway`: (side, from, to): a flight of stairs inside along that side's wall, from `from` (its foot, on the
        ground floor) to `to` (its top, at the first deck: 6 m on for a 350 storey), with a hole in the deck over it and
        a rail round the hole; for more storeys, a list of flights, one a storey, each starting where the hole above
        the last one isn't.
    Returns where its floors are: {"inside": ((x0, y0), (x1, y1)), "floors": [z of each storey's floor]}."""
    f = f"Buildings/{name}"
    x0, y0 = low
    x1, y1 = high
    ix0, iy0, ix1, iy1 = x0 + wall, y0 + wall, x1 - wall, y1 - wall
    height = storeys * storey
    # Courses: each storey's sill (to 100), window (to 220) and lintel (to the storey's top), so that doors and windows
    # end on a course's top and a lintel sits over each.
    courses = []
    for _ in range(storeys):
        courses += [100, 120, storey - 220]
    # Each wall's openings, along it from its start (west or south end), up from the floor.
    walls = {"S": ((x0, y0 + wall / 2.0), (x1, y0 + wall / 2.0)), "N": ((x0, y1 - wall / 2.0), (x1, y1 - wall / 2.0)),
             "W": ((x0 + wall / 2.0, y0 + wall), (x0 + wall / 2.0, y1 - wall)), "E": ((x1 - wall / 2.0, y0 + wall), (x1 - wall / 2.0, y1 - wall))}
    openings = {side: [] for side in SIDES}
    for d in doors:
        side, at = d[0], d[1]
        w = d[2] if len(d) > 2 else 200
        h = 220
        s = walls[side][0]
        mid = at - (s[0] if side in "NS" else s[1])
        openings[side].append((mid - w / 2.0, mid + w / 2.0, 0, h))
        at_point = (at, walls[side][0][1], bottom) if side in "NS" else (walls[side][0][0], at, bottom)
        door(f"{name}_Door_{side}_{int(at)}", f, at_point, 90 if side in "NS" else 0, w, h, leaves=door_material)
    for wnd in windows:
        side, at, level = wnd[0], wnd[1], wnd[2]
        w = wnd[3] if len(wnd) > 3 else 200
        sill, top = 100, 220
        s = walls[side][0]
        mid = at - (s[0] if side in "NS" else s[1])
        openings[side].append((mid - w / 2.0, mid + w / 2.0, level * storey + sill, level * storey + top))
    for side in SIDES:
        start, end = walls[side]
        cheese_wall(f"{name}_{side}", structure, start, end, bottom, courses, wall, column, openings[side], edible)
    # The frame: posts inside along the north and south walls (not in a doorway or a stairwell), and a deck on them at
    # each storey's top, with a hole over each flight of stairs.
    m_deck = deck or floor()
    flights = [] if stairway is None else ([stairway] if isinstance(stairway[0], str) else list(stairway))

    def hole_for(flight):
        side, a, b = flight
        lo, hi = min(a, b), max(a, b)
        return {"S": ((lo, iy0), (hi, iy0 + 160)), "N": ((lo, iy1 - 160), (hi, iy1)),
                "W": ((ix0, lo), (ix0 + 160, hi)), "E": ((ix1 - 160, lo), (ix1, hi))}[side]

    bays = max(1, int(round((ix1 - ix0) / 500.0)))
    posts = []
    for k in range(bays + 1):
        px = ix0 + 12 + k * (ix1 - ix0 - 24) / bays
        for side, py in (("S", iy0 + 12), ("N", iy1 - 12)):
            in_door = any(d[0] == side and abs(px - d[1]) < (d[2] if len(d) > 2 else 200) / 2.0 + 40 for d in doors)
            in_stairs = any(h[0][0] - 20 <= px <= h[1][0] + 20 and h[0][1] - 20 <= py <= h[1][1] + 20 for h in map(hole_for, flights))
            if not in_door and not in_stairs:
                posts.append((px, py))
    floors = [bottom]
    for level in range(1, storeys):
        z = bottom + level * storey
        floors.append(z)
        for i, (px, py) in enumerate(posts):
            frame_post(f"{name}_Post_{level}_{i}", f, px, py, z - storey, z - 25, colour=frame)
        flight = flights[level - 1] if level - 1 < len(flights) else None
        hole = hole_for(flight) if flight else None
        slab(f"{name}_Deck_{level}", f, (ix0, iy0, z - 25), (ix1, iy1, z), m_deck, [hole] if hole else [])
        if hole:
            (hx0, hy0), (hx1, hy1) = hole
            # A rail round the stairwell, open where the stairs come up.
            if flight[0] in "NS":
                far_x = hx0 if abs(flight[2] - hx1) < abs(flight[2] - hx0) else hx1
                inner_y = hy1 if flight[0] == "S" else hy0
                rail(f"{name}_StairRail_{level}", f, (hx0, inner_y), (hx1, inner_y), z)
                rail(f"{name}_StairEnd_{level}", f, (far_x, hy0), (far_x, hy1), z)
            else:
                far_y = hy0 if abs(flight[2] - hy1) < abs(flight[2] - hy0) else hy1
                inner_x = hx1 if flight[0] == "W" else hx0
                rail(f"{name}_StairRail_{level}", f, (inner_x, hy0), (inner_x, hy1), z)
                rail(f"{name}_StairEnd_{level}", f, (hx0, far_y), (hx1, far_y), z)
    for level, (side, a, b) in enumerate(flights[:storeys - 1]):
        line = {"S": iy0 + 80, "N": iy1 - 80, "W": ix0 + 80, "E": ix1 - 80}[side]
        z0, z1 = bottom + level * storey, bottom + (level + 1) * storey
        if side in "NS":
            stairs(f"{name}_Stairs_{level}", f, (a, line, z0), (b, line, z1), 150, steel())
        else:
            stairs(f"{name}_Stairs_{level}", f, (line, a, z0), (line, b, z1), 150, steel())
    # The roof: planks of cheese across the shorter way, from wall to wall.
    if cheese_roof:
        across_x = (x1 - x0) <= (y1 - y0)
        span = (x0, x1) if across_x else (y0, y1)
        other = (y0, y1) if across_x else (x0, x1)
        n = max(1, int(round((other[1] - other[0]) / column)))
        unit = (other[1] - other[0]) / n
        for i in range(n):
            a, b = other[0] + i * unit, other[0] + (i + 1) * unit
            if across_x:
                cheese_box(f"Cheese_{name}_Roof_{i}", structure, (span[0], a, bottom + height), (span[1], b, bottom + height + 30), edible)
            else:
                cheese_box(f"Cheese_{name}_Roof_{i}", structure, (a, span[0], bottom + height), (b, span[1], bottom + height + 30), edible)
    if lamps:
        for level in range(storeys):
            interior_lamp(f"{name}_Lamp_{level}", f, (ix0 + ix1) / 2.0, (iy0 + iy1) / 2.0, bottom + (level + 1) * storey - 32)
    # Steel at its corners outside, up to the roof: it reads as a building, and stands with the decks once the walls go.
    for cx, cy in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
        box(f"{name}_Corner_{int(cx)}_{int(cy)}", f, (cx - 15, cy - 15, bottom), (cx + 15, cy + 15, bottom + height + 30), painted(frame))
    count("buildings")
    return {"inside": ((ix0, iy0), (ix1, iy1)), "floors": floors}


def tunnel(name, structure, start, end, width=400, height=300, wall=50, bottom=0, openings=(), ends=(False, False),
           cheese_roof=True, edible=True, lamps=600, sides=("L", "R")):
    """A covered way of cheese along x or y from `start` (x, y) to `end`: walls either side, `width` apart inside and
    `height` tall, and a roof of cheese planks from wall to wall. Dark and close until the mice eat it open; a plank
    falls when the wall under one end goes. `openings` in its walls: (side, from, to[, top]): side "L" or "R" (left or
    right walking from start to end), from and to along it from `start`, up to `top` (220 unless said, a course's top,
    with a lintel over it that carries the roof; put a door() in it if it wants one). `ends`: whether each end is walled. A lamp every `lamps` cm inside.
    `sides`: the walls it has; built against a wall that isn't cheese (a shed's), leave that side out, and a steel
    ledger on that wall carries the roof's planks there instead."""
    f = f"Tunnels/{name}"
    along_x = abs(end[0] - start[0]) >= abs(end[1] - start[1])
    length = abs(end[0] - start[0]) if along_x else abs(end[1] - start[1])
    sign = 1 if ((end[0] - start[0]) if along_x else (end[1] - start[1])) >= 0 else -1
    half = width / 2.0 + wall / 2.0
    # One course, or two with a top at 220 for doorways in its walls.
    courses = [220, height - 220] if height > 280 else [height]
    for side, offset in (("L", 1), ("R", -1)):
        # Left of a way along +x is +y; along +y it is -x.
        o = offset * sign
        if along_x:
            s, e = (start[0], start[1] + o * half), (end[0], end[1] + o * half)
        else:
            s, e = (start[0] - o * half, start[1]), (end[0] - o * half, end[1])
        if side not in sides:
            low = (min(s[0], e[0]) - (0 if along_x else wall / 2.0), min(s[1], e[1]) - (wall / 2.0 if along_x else 0), bottom + height - 25)
            high = (max(s[0], e[0]) + (0 if along_x else wall / 2.0), max(s[1], e[1]) + (wall / 2.0 if along_x else 0), bottom + height)
            box(f"{name}_Ledger_{side}", f, low, high, painted("Dark"))
            continue
        ops = [(a, b, 0, t[0] if t else courses[0]) for (sd, a, b, *t) in openings if sd == side]
        cheese_wall(f"{name}_{side}", structure, s, e, bottom, courses, wall, 200, ops, edible)
    for which, capped in enumerate(ends):
        if not capped:
            continue
        p = start if which == 0 else end
        if along_x:
            cheese_wall(f"{name}_End{which}", structure, (p[0], p[1] - width / 2.0), (p[0], p[1] + width / 2.0), bottom, courses, wall, 200, (), edible)
        else:
            cheese_wall(f"{name}_End{which}", structure, (p[0] - width / 2.0, p[1]), (p[0] + width / 2.0, p[1]), bottom, courses, wall, 200, (), edible)
    if cheese_roof:
        n = max(1, int(round(length / 200.0)))
        unit = length / n
        reach = width / 2.0 + wall
        for i in range(n):
            a, b = i * unit, (i + 1) * unit
            if along_x:
                lo, hi = (min(start[0] + sign * a, start[0] + sign * b), start[1] - reach, bottom + height), (max(start[0] + sign * a, start[0] + sign * b), start[1] + reach, bottom + height + 30)
            else:
                lo, hi = (start[0] - reach, min(start[1] + sign * a, start[1] + sign * b), bottom + height), (start[0] + reach, max(start[1] + sign * a, start[1] + sign * b), bottom + height + 30)
            cheese_box(f"Cheese_{name}_Roof_{i}", structure, lo, hi, edible)
    if lamps:
        k = 0
        d = lamps / 2.0
        while d < length:
            x = start[0] + sign * d if along_x else start[0]
            y = start[1] if along_x else start[1] + sign * d
            interior_lamp(f"{name}_Lamp_{k}", f, x, y, bottom + height - 10, intensity=6.0, radius=700.0)
            d += lamps
            k += 1
    count("tunnels")


def bridge(name, structure, start, end, width=250, beams=2, beam=40, depth=30, deck=30, cheese_deck=True, rails=True,
           supports=(), material=None, edible=True, wax_beams=False):
    """A level bridge along x or y from `start` (x, y, z: the middle of one end, at its deck's top) to `end`: steel
    beams under it, the length of it, which the mice never eat, and a deck of cheese tiles across them that they do
    (or of `material`, if not `cheese_deck`). Eaten, it leaves its beams: `beams` of them, `beam` cm wide (one 60 cm
    beam down the middle is a way across for a careful player; bots and mice need 70). Rails at its sides if `rails`.
    `supports`: points along it (from `start`) where a post goes down to the ground (z 0). `wax_beams`: the beams are
    cheese in wax (never eaten) rather than steel: the cheese's own skeleton."""
    f = f"Bridges/{name}"
    along_x = abs(end[0] - start[0]) >= abs(end[1] - start[1])
    length = abs(end[0] - start[0]) if along_x else abs(end[1] - start[1])
    sign = 1 if ((end[0] - start[0]) if along_x else (end[1] - start[1])) >= 0 else -1
    z = start[2]

    def at(a, across):
        return (start[0] + sign * a, start[1] + across) if along_x else (start[0] + across, start[1] + sign * a)

    def span(a0, a1, c0, c1, z0, z1):
        p, q = at(a0, c0), at(a1, c1)
        return (min(p[0], q[0]), min(p[1], q[1]), z0), (max(p[0], q[0]), max(p[1], q[1]), z1)

    centres = [0.0] if beams == 1 else [-width / 2.0 + beam / 2.0 + i * (width - beam) / (beams - 1) for i in range(beams)]
    under = z - (deck if cheese_deck else 0)
    for i, c in enumerate(centres):
        low, high = span(0, length, c - beam / 2.0, c + beam / 2.0, under - depth, under)
        if wax_beams:
            cheese_box(f"Cheese_{name}_Beam_{i}", structure, low, high, edible=False)
        else:
            box(f"{name}_Beam_{i}", f, low, high, painted("Dark"))
    if cheese_deck:
        n = max(1, int(round(length / 200.0)))
        unit = length / n
        for i in range(n):
            low, high = span(i * unit, (i + 1) * unit, -width / 2.0, width / 2.0, under, z)
            cheese_box(f"Cheese_{name}_Deck_{i}", structure, low, high, edible)
    else:
        low, high = span(0, length, -width / 2.0, width / 2.0, z - deck, z)
        box(f"{name}_Deck", f, low, high, material or floor())
    if rails:
        # On the beams, through the deck: they stay when it is eaten (so give a bridge with rails its beams at the sides).
        for side in (-1, 1):
            a, b = at(0, side * (width / 2.0 - 4)), at(length, side * (width / 2.0 - 4))
            rail(f"{name}_Rail_{side}", f, a, b, under, height=100 + (z - under))
    for i, a in enumerate(supports):
        for c in centres if beams > 1 else [0.0]:
            p = at(a, c)
            frame_post(f"{name}_Support_{i}_{int(c)}", f, p[0], p[1], 0, under - depth, size=30)
    count("bridges")


# ---------------------------------------------------------------- round and slanting things: walls of wheels, round walls, walls and stairs at any angle, ceilings

def polar(centre, radius, angle):
    """The point (x, y) `radius` from `centre` at `angle` degrees (0 along +x, 90 along +y: anticlockwise from above)."""
    a = math.radians(angle)
    return (centre[0] + radius * math.cos(a), centre[1] + radius * math.sin(a))


def wall_between(label, folder, a, b, thickness, bottom, top, material):
    """A straight block at any angle from point `a` to point `b` (x, y), `thickness` across, from `bottom` to `top`: a
    wall, a beam, a kerb. Its ends are square, at `a` and `b`."""
    length = math.hypot(b[0] - a[0], b[1] - a[1])
    yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    centre = ((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0, (bottom + top) / 2.0)
    return solid(label, folder, centre, (length, thickness, top - bottom), material, rotation=(0, yaw, 0))


def round_wall(name, folder, centre, inner, outer, bottom, top, material, openings=(), step=10.0):
    """A round wall (a vault's, a tower's, a well's) from radius `inner` to `outer` about `centre`, from `bottom` to
    `top`, of straight pieces about `step` degrees each, leaving `openings`: (angle, width) each, a gap `width` wide
    (measured along the wall's middle) centred on `angle` degrees. The pieces overlap a little, so it has no slits."""
    middle = (inner + outer) / 2.0
    gaps = sorted(((a - math.degrees(w / 2.0 / middle)) % 360.0, math.degrees(w / middle)) for a, w in openings)
    # The arcs of wall between the gaps, as (from, length) in degrees.
    arcs = []
    if not gaps:
        arcs.append((0.0, 360.0))
    for i, (start, length) in enumerate(gaps):
        nxt = gaps[(i + 1) % len(gaps)][0] + (360.0 if i + 1 == len(gaps) else 0.0)
        if nxt - (start + length) > 0.1:
            arcs.append((start + length, nxt - (start + length)))
    made = []
    for k, (a0, span) in enumerate(arcs):
        n = max(1, int(math.ceil(span / step)))
        d = span / n
        for i in range(n):
            mid = a0 + d * (i + 0.5)
            # Each piece a chord of the outer face, long enough that the next one meets it there.
            length = 2.0 * outer * math.tan(math.radians(d / 2.0))
            # The end pieces only to the gap's edge, not past it into the opening.
            at = polar(centre, middle, mid)
            made.append(solid(f"{name}_{k}_{i}", folder, (at[0], at[1], (bottom + top) / 2.0), (outer - inner, length + 2, top - bottom), material,
                              rotation=(0, mid, 0)))
    return made


def wheel_line(name, structure, start, end, bottom, diameter=240.0, courses=(100, 100, 100), edible=True, material=None, mesh=None, overlap=2.0):
    """A wall of wheels of cheese from `start` to `end` (x, y), at any angle: columns of wheels standing on one another
    (`courses`: each wheel's height, from the bottom), side by side and touching, the end wheels' edges at `start` and
    `end`. Their size is about `diameter`, made to fit the length exactly, and `overlap` more, so that neighbours press
    into each other a little (more for a `mesh` with rounded edges, so that no slit shows where they meet). Eaten at its
    foot, a column comes down a wheel at a time; once a column is gone there is a gap a wheel wide. Returns the blocks."""
    length = math.hypot(end[0] - start[0], end[1] - start[1])
    n = max(1, int(round(length / diameter)))
    d = length / n
    made = []
    for i in range(n):
        t = (i + 0.5) / n
        x, y = start[0] + (end[0] - start[0]) * t, start[1] + (end[1] - start[1]) * t
        z = bottom
        for k, h in enumerate(courses):
            made.append(wheel(f"Cheese_{name}_{i}_{k}", structure, x, y, z, d + overlap, h, edible, material, mesh))
            z += h
    return made


def wheel_arc(name, structure, centre, radius, a0, a1, bottom, diameter=240.0, courses=(100, 100, 100), edible=True, material=None, mesh=None, overlap=2.0):
    """A curved wall of wheels of cheese along the circle of `radius` about `centre`, from angle `a0` to `a1` (degrees,
    anticlockwise), as wheel_line: touching columns of wheels about `diameter` across, made to fit the arc exactly, the
    end wheels' edges at its ends. Returns the blocks."""
    span = (a1 - a0) % 360.0 or 360.0
    length = math.radians(span) * radius
    n = max(1, int(round(length / diameter)))
    # Neighbours touch along their chord, a little shorter than the arc between them.
    d = 2.0 * radius * math.sin(math.radians(span / n) / 2.0)
    made = []
    for i in range(n):
        x, y = polar(centre, radius, a0 + span * (i + 0.5) / n)
        z = bottom
        for k, h in enumerate(courses):
            made.append(wheel(f"Cheese_{name}_{i}_{k}", structure, x, y, z, d + overlap, h, edible, material, mesh))
            z += h
    return made


def cheese_rack(name, structure, start, end, frame, depth=80.0, shelves=(10.0, 85.0, 160.0), top=230.0, wheel_size=(70.0, 36.0),
                edible=True, mesh=None, bottom=0.0):
    """A rack of shelves from `start` to `end` (x, y, at any angle), `depth` across, with a row of wheels of cheese on
    each shelf (`wheel_size`: across, high): what an aging room is lined with. Its posts and boards (in `frame`, a
    material) stay when the wheels are eaten: a bare rack, which sight and shots pass between its boards. Returns the
    wheels."""
    f = f"Racks/{name}"
    length = math.hypot(end[0] - start[0], end[1] - start[1])
    ux, uy = (end[0] - start[0]) / length, (end[1] - start[1]) / length
    nx, ny = -uy, ux
    posts = max(1, int(round(length / 150.0)))
    for i in range(posts + 1):
        t = i / posts * (length - 8.0) + 4.0
        for side in (-1, 1):
            p = (start[0] + ux * t + nx * side * (depth / 2.0 - 4.0), start[1] + uy * t + ny * side * (depth / 2.0 - 4.0))
            box(f"{name}_Post_{i}_{side}", f, (p[0] - 4, p[1] - 4, bottom), (p[0] + 4, p[1] + 4, bottom + top), frame)
    for z in list(shelves) + [top - 4.0]:
        wall_between(f"{name}_Board_{int(z)}", f, start, end, depth, bottom + z, bottom + z + 4.0, frame)
    made = []
    n = max(1, int(length // (wheel_size[0] + 6.0)))
    for k, z in enumerate(shelves):
        for i in range(n):
            t = (i + 0.5) / n * length
            made.append(wheel(f"Cheese_{name}_{k}_{i}", structure, start[0] + ux * t, start[1] + uy * t, bottom + z + 4.0,
                              wheel_size[0], wheel_size[1], edible, mesh=mesh))
    count("racks")
    return made


def stairs_between(label, folder, start, end, width, material, solid_under=True):
    """A flight of stairs at any angle, from `start` (x, y, z: the foot of the bottom step, on the floor) to `end` (the
    top of the flight, at the floor above), steps of at most RISE, as stairs() makes along x or y."""
    rise = end[2] - start[2]
    steps = max(1, int(math.ceil(rise / RISE)))
    run = math.hypot(end[0] - start[0], end[1] - start[1])
    tread = run / steps
    if tread < 25:
        raise RuntimeError(f"{label}: {steps} steps in {run:.0f} cm is too steep")
    yaw = math.degrees(math.atan2(end[1] - start[1], end[0] - start[0]))
    for i in range(steps):
        top = start[2] + rise * (i + 1) / steps
        low = start[2] if solid_under else top - 20
        t = (i + 0.5) / steps
        x, y = start[0] + (end[0] - start[0]) * t, start[1] + (end[1] - start[1]) * t
        solid(f"{label}_{i}", folder, (x, y, (low + top) / 2.0), (tread + 1, width, top - low), material, rotation=(0, yaw, 0))


def overhead(actor, tag="Ceiling"):
    """Makes `actor` something overhead that nobody can reach (a ceiling, its beams): it blocks nothing, so shots,
    streaks and sight lines pass it and the minimap and the navigation mesh leave it out, and it is tagged `tag`, so
    that `CheeseStrikeHideTagged Ceiling` hides it for looking at a roofed map from above. Returns it."""
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_collision_profile_name("NoCollision")
    component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_editor_property("tags", [tag])
    return actor


def ceiling(label, folder, low, high, material, holes=()):
    """A ceiling from corner `low` to corner `high`, with rectangular `holes` ((x0, y0), (x1, y1)): a slab, overhead()."""
    for i, (a, b) in enumerate(rect_minus(low, high, holes)):
        overhead(box(f"{label}_{i}", folder, (a[0], a[1], low[2]), (b[0], b[1], high[2]), material))


def point_lamp(label, folder, x, y, z, intensity=8.0, radius=800.0, temperature=3000.0, shadows=True, fitting="Dark", colour=None):
    """A hanging lamp: a point light at (x, y, z) under a small shade (painted `fitting`, or none), lighting all round
    it, `radius` far. Without `shadows` it is cheap but shines through walls: use it for fill, a few metres across.
    `colour` (r, g, b) tints it instead of its `temperature`."""
    light = place(unreal.PointLight, label, folder, (x, y, z))
    component = light.get_component_by_class(unreal.PointLightComponent)
    component.set_editor_property("intensity", intensity)
    component.set_editor_property("attenuation_radius", radius)
    component.set_editor_property("source_radius", 8.0)
    component.set_editor_property("cast_shadows", bool(shadows))
    if colour is not None:
        component.set_editor_property("light_color", unreal.Color(r=int(colour[0]), g=int(colour[1]), b=int(colour[2]), a=255))
    else:
        component.set_editor_property("use_temperature", True)
        component.set_editor_property("temperature", temperature)
    if fitting:
        overhead(cylinder(label + "_Shade", folder, x, y, z + 6, 44, 18, painted(fitting)), "Lamp")
    count("lamps" if shadows else "fill lamps")
    return light


def place_areas(name, areas):
    """One named place (place_name) over several boxes, each (low, high): for a place that isn't a box, a ring's
    corridor or an L. The boxes shouldn't overlap another place's, or the smaller one wins there."""
    for i, (low, high) in enumerate(areas):
        actor = place_name(name, low, high)
        actor.set_actor_label(f"Place_{name.replace(' ', '')}_{i}")


# ---------------------------------------------------------------- what a match needs

def team_starts(team, points, yaw, z=0, room=150, name=""):
    """Starts for a side ("Gouda" or "Mozza"), at (x, y) on the floor at `z`, all facing `yaw`, labelled
    Start_<team>_<name><n>; the cheese within `room` of each is kept (keep()), so that nobody ever starts over a hole."""
    for i, (x, y) in enumerate(points):
        actor = place(unreal.PlayerStart, f"Start_{team}_{name}{i}", "Starts", (x, y, z + 100), (0, yaw, 0))
        actor.set_editor_property("player_start_tag", team)
        keep((x - room, y - room, z - 400), (x + room, y + room, z + 250))
        count("starts")


def free_starts(points, z=0, room=150):
    """Starts that free-for-all adds to the sides', ((x, y, yaw[, z[, name]]) each): in every lane, so that a match
    neither begins nor comes back in a crowd (everyone comes back at the start furthest from them all)."""
    for i, p in enumerate(points):
        x, y, yaw = p[0], p[1], p[2]
        bottom = p[3] if len(p) > 3 else z
        label = f"Start_Free_{p[4]}" if len(p) > 4 else f"Start_Free_{i}"
        actor = place(unreal.PlayerStart, label, "Starts", (x, y, bottom + 100), (0, yaw, 0))
        actor.set_editor_property("player_start_tag", "Free")
        keep((x - room, y - room, bottom - 400), (x + room, y + room, bottom + 250))
        count("starts")


def mouse_entry(x, y, z, yaw):
    """Where a mouse comes over the edge: a point on top of the outer wall at (x, y, z), facing `yaw` into the map
    (ACheeseStrikeGameMode::SpawnMice). It lands on the navigation mesh 5 m in."""
    entry = place(unreal.TargetPoint, f"MouseEntry_{int(x)}_{int(y)}", "Mice", (x, y, z), (0, yaw, 0))
    entry.set_editor_property("tags", ["MouseEntry"])
    count("mouse entries")


def mouse_entries(x0, y0, x1, y1, top, xs=(), ys=()):
    """Mouse entries on the outer wall round (x0, y0)-(x1, y1), `top` high: at each x in `xs` on the north and south
    walls, and each y in `ys` on the west and east. Eight or more, spread round, so the mice don't all start at one end;
    not where cheese meets the wall (they land 5 m in, on the navigation mesh)."""
    for x in xs:
        mouse_entry(x, y1 + 50, top, -90)
        mouse_entry(x, y0 - 50, top, 90)
    for y in ys:
        mouse_entry(x0 - 50, y, top, 0)
        mouse_entry(x1 + 50, y, top, 180)


def nav_bounds(low, high):
    """The bounds of the navigation mesh the bots and mice walk: all the playable ground, from its lowest floor to above
    its highest. The mesh is built when the level starts and rebuilt where the cheese goes (Config/DefaultEngine.ini)."""
    centre = [(a + b) / 2.0 for a, b in zip(low, high)]
    volume = eas.spawn_actor_from_object(unreal.NavMeshBoundsVolume.static_class(), v(*centre))
    volume.set_actor_label("Navigation")
    volume.set_folder_path("Navigation")
    # The volume's brush is a 2 m cube.
    volume.set_actor_scale3d(v(abs(high[0] - low[0]) / 200.0, abs(high[1] - low[1]) / 200.0, abs(high[2] - low[2]) / 200.0))
    origin, extent = volume.get_actor_bounds(False)
    lines.append(f"navigation bounds: {extent.x * 2:.0f} x {extent.y * 2:.0f} x {extent.z * 2:.0f} cm")
    if extent.x < 100:
        raise RuntimeError("the navigation volume has no brush")


def place_name(name, low, high):
    """A named place (ACheeseStrikePlace), from corner `low` to corner `high`: what the HUD and kill feed call it.
    Name every part of a map a player would tell another about; where places overlap, the smaller one wins."""
    centre = [(a + b) / 2.0 for a, b in zip(low, high)]
    actor = place(unreal.CheeseStrikePlace, f"Place_{name.replace(' ', '')}", "Places", centre)
    actor.set_editor_property("place_name", name)
    actor.get_component_by_class(unreal.BoxComponent).set_box_extent(v(*[abs(b - a) / 2.0 for a, b in zip(low, high)]), False)
    count("places")
    return actor


def kill_below(z):
    """Whoever falls below `z` dies (the world's kill height): for a map with a pit or a drop off its edge."""
    settings = next(a for a in eas.get_all_level_actors() if isinstance(a, unreal.WorldSettings))
    settings.set_editor_property("kill_z", float(z))
    lines.append(f"kill below z = {z}")


# ---------------------------------------------------------------- light, sky and air

LIGHTS = {
    # sun pitch, yaw, intensity, temperature; sky light intensity; fog density, its colour; exposure bias
    "day": (-32, -40, 4.0, 5300.0, 1.0, 0.012, (0.32, 0.27, 0.20), 0.5),
    "noon": (-62, -30, 5.0, 5800.0, 1.0, 0.008, (0.30, 0.30, 0.28), 0.3),
    "dusk": (-9, 150, 3.0, 3600.0, 0.8, 0.02, (0.40, 0.25, 0.15), 0.8),
    "overcast": (-45, -60, 1.6, 6500.0, 1.4, 0.02, (0.30, 0.31, 0.32), 0.9),
    "night": (-35, 60, 0.15, 7500.0, 0.25, 0.015, (0.05, 0.06, 0.09), 1.6),
    "cave": (-80, 0, 0.0, 6500.0, 0.15, 0.03, (0.12, 0.09, 0.06), 1.8),
}


def light_rig(preset="day", sun=None):
    """The sun (or moon), sky, clouds, haze and the picture's grading, from a preset of LIGHTS: day, noon, dusk,
    overcast, night or cave (no sun or sky; light it with lamps). `sun` (pitch, yaw) turns the sun. A map is lit for
    reading first: the sun behind neither side's view down a lane, and every place a player stands lit enough to be
    seen in."""
    pitch, yaw, intensity, temperature, sky_light, density, haze, exposure = LIGHTS[preset]
    if sun is not None:
        pitch, yaw = sun
    f = "Lighting"
    if intensity > 0:
        s = place(unreal.DirectionalLight, "Sun", f, (0, 0, 2000), (pitch, yaw, 0))
        light = s.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_editor_property("intensity", intensity)
        light.set_editor_property("use_temperature", True)
        light.set_editor_property("temperature", temperature)
        light.set_editor_property("light_source_angle", 0.8)
        light.set_editor_property("atmosphere_sun_light", True)
        atmosphere = place(unreal.SkyAtmosphere, "Sky", f, (0, 0, 0))
        atmosphere.get_component_by_class(unreal.SkyAtmosphereComponent).set_editor_property("mie_scattering_scale", 0.006)
        place(unreal.VolumetricCloud, "Clouds", f, (0, 0, 0))
    skylight = place(unreal.SkyLight, "SkyLight", f, (0, 0, 1500))
    sky_component = skylight.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("real_time_capture", intensity > 0)
    sky_component.set_editor_property("intensity", sky_light)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    if intensity <= 0:
        sky_component.set_editor_property("lower_hemisphere_is_black", False)
        sky_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    fog = place(unreal.ExponentialHeightFog, "Haze", f, (0, 0, 0)).get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog.set_editor_property("fog_density", density)
    fog.set_editor_property("fog_height_falloff", 0.12)
    fog.set_editor_property("start_distance", 1500.0)
    fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(haze[0], haze[1], haze[2], 1.0))
    fog.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(haze[0] * 1.4, haze[1] * 1.2, haze[2], 1.0))
    volume = place(unreal.PostProcessVolume, "PostProcess", f, (0, 0, 0))
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    for key, value in (("bloom_intensity", 0.35), ("vignette_intensity", 0.3), ("auto_exposure_bias", exposure),
                       ("color_contrast", unreal.Vector4(1.06, 1.06, 1.06, 1.0)), ("color_saturation", unreal.Vector4(0.96, 0.96, 0.96, 1.0))):
        settings.set_editor_property(key, value)
        settings.set_editor_property("override_" + key, True)
    volume.set_editor_property("settings", settings)
    lines.append(f"lighting: {preset}")


# ---------------------------------------------------------------- building a level

def mirrored(build_side):
    """Builds one side twice: build_side(s, side) with s = -1, "West" and s = 1, "East". Give x as a distance from the
    middle and multiply by s, so the east is the west mirrored (x to -x) and neither side has the better half."""
    for s, side in ((-1, "West"), (1, "East")):
        build_side(s, side)


def _inside(point, area):
    (lx, ly, lz), (hx, hy, hz) = area
    return lx <= point.x <= hx and ly <= point.y <= hy and lz <= point.z <= hz


def _keep_cheese():
    """Every block of cheese whose middle is in a kept area: uneaten, in wax."""
    n = 0
    for actor in eas.get_all_level_actors():
        if isinstance(actor, unreal.CheeseStrikeCheese) and actor.get_editor_property("edible"):
            middle = actor.get_actor_location()
            if any(_inside(middle, area) for area in kept):
                actor.set_editor_property("edible", False)
                actor.get_component_by_class(unreal.StaticMeshComponent).set_material(0, wax())
                n += 1
    return n


def build_level(level, build, report=None):
    """Builds `level` (a package path, /Game/CheeseStrike/Maps/Lvl_<Name>) afresh with `build()`, saves it, writes
    Saved/<report or the level's name>.txt and quits the editor. Nothing is saved if the build fails."""
    out = unreal.Paths.project_saved_dir() + (report or level.split("/")[-1]) + ".txt"
    try:
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        # Built afresh in the same level: opened and emptied if it exists, made if it doesn't.
        if unreal.EditorAssetLibrary.does_asset_exist(level):
            opened = levels.load_level(level)
            for actor in eas.get_all_level_actors():
                if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
                    eas.destroy_actor(actor)
            lines.append(f"opened and emptied {level}: {opened}")
        else:
            lines.append(f"made {level}: {levels.new_level(level, False)}")
        # Never build into another level: the editor opens on the shooter level.
        current = unreal.EditorLevelLibrary.get_editor_world().get_outermost().get_name()
        if current != level:
            raise RuntimeError(f"the level open is {current}, not {level}; nothing built")
        build()
        in_areas = _keep_cheese()
        uneaten = sum(1 for a in eas.get_all_level_actors() if isinstance(a, unreal.CheeseStrikeCheese) and not a.get_editor_property("edible"))
        lines.append(f"never eaten (in wax): {uneaten} blocks, {in_areas} of them in kept areas")
        lines.append("saved " + str(levels.save_current_level()))
        for name in sorted(counts):
            lines.append(f"  {name}: {counts[name]}")
        blocks = sum(n for k, n in counts.items() if k.startswith("cheese:"))
        lines.append(f"cheese blocks: {blocks}")
        if counts.get("starts", 0) < 16:
            lines.append(f"WARNING: {counts.get('starts', 0)} starts; a match has sixteen players")
        if blocks and counts.get("mouse entries", 0) == 0:
            lines.append("WARNING: cheese but no mouse entries: no mice will come")
    except Exception:
        lines.append(traceback.format_exc())
    finally:
        open(out, "w").write("\n".join(lines))
        unreal.SystemLibrary.quit_editor()
