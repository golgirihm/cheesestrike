"""Models the cheese weapons in Blender, by script, as OBJ files that
tools/art/import_weapons.py imports into /Game/CheeseStrike/Weapons/ and
tools/art/weapon_icons.py draws the HUD's pictures from:

    blender.exe -b --factory-startup -P tools/art/cheese_weapons.py -- [folder] [weapon ...] [--preview folder]

Each weapon is a function below, made of simple parts (rounded boxes, side
outlines pushed out across the gun, turned profiles, tubes along paths,
metaballs, Blender's text) joined into one mesh whose material slots are in
the order CheeseStrikeWeaponLooks gives their cheeses. Every one is cheese-
shaped rather than a gun with a cheese texture:

  - blaster: the Cheddar Blaster, a block of cheddar for a receiver and a
    handguard and stock of it, on a steel barrel, rail and sights;
  - carbine: the Colby Carbine, its stock and handguard Colby longhorns (logs
    of marbled cheese) on a steel receiver;
  - roquefort: the Roquefort Rifle, a thumbhole stock of blue-veined cheese
    and a scope dipped in wax;
  - pistol: the Curd Pistol, a pistol of lumps of fresh curd (metaballs) with
    a steel muzzle and sights;
  - shotgun: the Swiss Shotgun, a wedge of Swiss holed through and through,
    tip forward, a double barrel out of the tip and the rind its stock;
  - munster: one of the Munster Twin SMGs, a stubby block of Munster in its
    orange rind with a belt of cheese slices hanging from it;
  - grater: the Cheese Grater, a steel box grater held by its handle over the
    top and down the back, its punched face forward, Parmesan inside;
  - babybomb: the BabyBomb, a shoulder tube of three red-waxed Babybel wheels
    with the white pull-strip spiralling round them and BABYBOMB embossed;
  - stick: the Cheese Stick, a 1.2 m baton of string cheese in its wrapper at
    the hand, peeled strands splaying at the striking end;
  - gatling: the Gouda Gatling, six barrels through gouda wheels before a
    housing that is a wheel of gouda in yellow wax;
  - babybomb_round and grater_shard: the BabyBomb's wheel (along X, as a
    projectile flies) and the Grater's sliver of grated cheese.

Coordinates are the game's (Unreal's) in centimetres, in the rifle's grip
frame: Y forward, Z up, X to the gun's left, the hand's grip at the origin
(a rifle's on the top of its pistol grip, a pistol's in the middle of it),
except the stick, which points along -Y as the knife does. The OBJ is
written mirrored in Y, as Unreal's importer mirrors it back. With
--preview, each is also rendered side on and from over the shoulder, with
its sight (red), muzzle (green) and left hand (blue) marked.
"""

import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

WORK = os.path.join(os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads"), "x", "weapons")

# The preview's colours for each material (the game's are M_CheeseWeapon instances, tools/art/weapon_materials.py).
COLOURS = {
    "Cheddar": (0.93, 0.42, 0.05), "Colby": (0.9, 0.55, 0.15), "Roquefort": (0.84, 0.8, 0.62), "Curd": (0.92, 0.88, 0.74),
    "Swiss": (0.95, 0.8, 0.35), "Rind": (0.62, 0.42, 0.18), "Munster": (0.95, 0.85, 0.5), "MunsterRind": (0.85, 0.35, 0.06),
    "Parmesan": (0.9, 0.78, 0.45), "StringCheese": (0.96, 0.94, 0.86), "Wrapper": (0.62, 0.03, 0.03), "BabyWax": (0.75, 0.02, 0.02),
    "Strip": (0.95, 0.95, 0.95), "BabyCheese": (0.98, 0.85, 0.45), "Gouda": (0.98, 0.72, 0.15), "GoudaWax": (0.95, 0.75, 0.05),
    "Steel": (0.45, 0.45, 0.47), "Black": (0.05, 0.05, 0.05), "ScopeWax": (0.3, 0.03, 0.05), "GraterSteel": (0.8, 0.8, 0.82),
    "Grip": (0.6, 0.04, 0.04),
}

PARTS = []
CUTTERS = []
MARKS = {}


# ---------------------------------------------------------------------------------------------------------------------
# Parts


def material(name):
    found = bpy.data.materials.get(name)
    if found is None:
        found = bpy.data.materials.new(name)
        colour = COLOURS.get(name, (0.5, 0.5, 0.5))
        found.diffuse_color = (colour[0], colour[1], colour[2], 1.0)
    return found


def link(obj):
    bpy.context.scene.collection.objects.link(obj)
    return obj


def part(bm, mat, name="part", recalc=True):
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = link(bpy.data.objects.new(name, me))
    me.materials.append(material(mat))
    PARTS.append(obj)
    return obj


def bevel(obj, width, segments=2, angle=40.0):
    m = obj.modifiers.new("bevel", 'BEVEL')
    m.width = width
    m.segments = segments
    m.limit_method = 'ANGLE'
    m.angle_limit = math.radians(angle)
    return obj


def box(mat, centre, size, round_by=0.0, segments=2, turn=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    if turn is not None:
        bmesh.ops.transform(bm, matrix=turn, verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector(centre), verts=bm.verts)
    obj = part(bm, mat, "box")
    if round_by:
        bevel(obj, round_by, segments)
    return obj


def prism(mat, outline, lo, hi, round_by=0.0, segments=2, axis="X"):
    """A side outline pushed out across an axis: X takes (y, z) points, Z takes (x, y) points, Y takes (x, z)."""
    bm = bmesh.new()

    def at(p, w):
        if axis == "X":
            return (w, p[0], p[1])
        if axis == "Z":
            return (p[0], p[1], w)
        return (p[0], w, p[1])

    a = [bm.verts.new(at(p, lo)) for p in outline]
    b = [bm.verts.new(at(p, hi)) for p in outline]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    n = len(outline)
    for i in range(n):
        bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    obj = part(bm, mat, "prism")
    if round_by:
        bevel(obj, round_by, segments)
    return obj


def lathe(mat, profile, centre=(0.0, 0.0, 0.0), axis="Y", sides=40, wobble=None):
    """A turned shape: `profile` is (radius, along) from one end to the other, about an axis through `centre`; a radius
    of 0 closes an end. `wobble(angle, along)` adds to the radius."""
    bm = bmesh.new()
    rings = []
    for r, t in profile:
        ring = []
        for k in range(sides):
            a = 2.0 * math.pi * k / sides
            rr = r + (wobble(a, t) if wobble is not None and r > 0.0 else 0.0)
            c, s = rr * math.cos(a), rr * math.sin(a)
            if axis == "Y":
                p = (centre[0] + c, centre[1] + t, centre[2] + s)
            elif axis == "X":
                p = (centre[0] + t, centre[1] + c, centre[2] + s)
            else:
                p = (centre[0] + c, centre[1] + s, centre[2] + t)
            ring.append(bm.verts.new(p))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(sides):
            k1 = (k + 1) % sides
            bm.faces.new((r0[k], r0[k1], r1[k1], r1[k]))
    if profile[0][0] > 0.0:
        bm.faces.new(list(reversed(rings[0])))
    if profile[-1][0] > 0.0:
        bm.faces.new(rings[-1])
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    # Faces left with two corners where a ring closed to a point.
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-4)
    return part(bm, mat, "lathe")


def cylinder(mat, a, b, radius, radius_b=None, sides=24):
    a, b = Vector(a), Vector(b)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=radius,
                          radius2=radius if radius_b is None else radius_b, depth=(b - a).length)
    turn = Vector((0.0, 0.0, 1.0)).rotation_difference((b - a).normalized()).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((a + b) / 2.0) @ turn, verts=bm.verts)
    return part(bm, mat, "cylinder")


def sphere(mat, centre, radius, scale=(1.0, 1.0, 1.0), cutter=False):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=12, radius=radius)
    for v in bm.verts:
        v.co = Vector((v.co.x * scale[0], v.co.y * scale[1], v.co.z * scale[2])) + Vector(centre)
    if cutter:
        return bm
    return part(bm, mat, "sphere")


def tube(mat, points, radius, closed=False, radii=None, resolution=3, smooth=True):
    """A round bar along a path (a NURBS through the points), its ends capped; `radii` tapers it point by point."""
    curve = bpy.data.curves.new("tube", 'CURVE')
    curve.dimensions = '3D'
    curve.bevel_depth = radius
    curve.bevel_resolution = resolution
    curve.use_fill_caps = True
    spline = curve.splines.new('NURBS' if smooth else 'POLY')
    spline.points.add(len(points) - 1)
    for i, p in enumerate(points):
        spline.points[i].co = (p[0], p[1], p[2], 1.0)
        spline.points[i].radius = radii[i] if radii is not None else 1.0
    if smooth:
        spline.order_u = min(3, len(points))
        spline.use_endpoint_u = not closed
        spline.resolution_u = 10
    spline.use_cyclic_u = closed
    obj = link(bpy.data.objects.new("tube", curve))
    curve.materials.append(material(mat))
    PARTS.append(obj)
    return obj


def ribbon(mat, path, width, thickness, normal_of):
    """A flat strip along `path` (points), `width` across it in the surface whose normal `normal_of(point)` gives,
    `thickness` out of it."""
    bm = bmesh.new()
    rings = []
    for i, p in enumerate(path):
        p = Vector(p)
        ahead = Vector(path[min(i + 1, len(path) - 1)]) - Vector(path[max(i - 1, 0)])
        n = Vector(normal_of(p)).normalized()
        across = n.cross(ahead).normalized()
        w = width(i / (len(path) - 1)) if callable(width) else width
        rings.append([bm.verts.new(p + across * sw * w / 2.0 + n * sn * thickness / 2.0) for sw, sn in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(4):
            bm.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    return part(bm, mat, "ribbon")


# The lettering's face: the engine's Roboto Black (Apache 2.0), as the countdown's.
FONT = os.environ.get("CHEESESTRIKE_FONT", r"C:\Program Files\Epic Games\UE_5.8\Engine\Content\Slate\Fonts\Roboto-Black.ttf")


def text(mat, body, size, depth, origin, normal, up):
    """Lettering standing `depth` out of a surface at `origin`, facing `normal`, its top toward `up`, reading as the
    game's left-handed space has it."""
    curve = bpy.data.curves.new("text", 'FONT')
    curve.body = body
    curve.size = size
    curve.extrude = depth / 2.0
    curve.align_x = 'CENTER'
    curve.align_y = 'CENTER'
    if os.path.exists(FONT):
        curve.font = bpy.data.fonts.load(FONT, check_existing=True)
    temp = link(bpy.data.objects.new("text", curve))
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(temp.evaluated_get(dg))
    bpy.data.objects.remove(temp)
    n, u = Vector(normal).normalized(), Vector(up).normalized()
    # Left-handed: a reader facing the surface (looking along -normal) has `right` = up x forward.
    r = u.cross(-n)
    basis = Matrix((r, u, n)).transposed().to_4x4()
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(origin)) @ basis, verts=bm.verts)
    if basis.determinant() < 0.0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    obj = part(bm, mat, "text", recalc=False)
    return obj


def metaballs(mat, balls, resolution=0.3, threshold=0.6):
    """Lumps: (centre, radius, (sx, sy, sz)) ellipsoids melted together."""
    mb = bpy.data.metaballs.new("lumps")
    mb.resolution = resolution
    mb.render_resolution = resolution
    mb.threshold = threshold
    for centre, radius, scale in balls:
        e = mb.elements.new()
        e.type = 'ELLIPSOID'
        e.co = centre
        e.radius = radius
        e.size_x, e.size_y, e.size_z = scale
    obj = link(bpy.data.objects.new("lumps", mb))
    mb.materials.append(material(mat))
    PARTS.append(obj)
    return obj


def cut(obj, cutter_bms):
    """Cuts the cutters (bmeshes) out of a part."""
    bm = bmesh.new()
    for c in cutter_bms:
        me = bpy.data.meshes.new("c")
        c.to_mesh(me)
        c.free()
        bm.from_mesh(me)
    me = bpy.data.meshes.new("cutter")
    bm.to_mesh(me)
    bm.free()
    cutter = link(bpy.data.objects.new("cutter", me))
    cutter.hide_render = True
    cutter.display_type = 'WIRE'
    CUTTERS.append(cutter)
    m = obj.modifiers.new("cut", 'BOOLEAN')
    m.operation = 'DIFFERENCE'
    m.solver = 'EXACT'
    m.use_self = True
    m.use_hole_tolerant = True
    m.object = cutter
    return obj


def cutter_box(centre, size, turn=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    if turn is not None:
        bmesh.ops.transform(bm, matrix=turn, verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector(centre), verts=bm.verts)
    return bm


def cutter_cylinder(a, b, radius, sides=16, scale=None):
    a, b = Vector(a), Vector(b)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=radius, radius2=radius, depth=(b - a).length)
    if scale is not None:
        for v in bm.verts:
            v.co = Vector((v.co.x * scale[0], v.co.y * scale[1], v.co.z))
    turn = Vector((0.0, 0.0, 1.0)).rotation_difference((b - a).normalized()).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((a + b) / 2.0) @ turn, verts=bm.verts)
    return bm


def faces_facing(obj, direction, mat, least=0.7):
    """Gives the faces of a part facing `direction` another material (the cut face of a cheese, its rind)."""
    me = obj.data
    if mat not in [m.name for m in me.materials]:
        me.materials.append(material(mat))
    index = [m.name for m in me.materials].index(mat)
    d = Vector(direction).normalized()
    for poly in me.polygons:
        if poly.normal.dot(d) >= least:
            poly.material_index = index


# ---------------------------------------------------------------------------------------------------------------------
# Common pieces

# A rifle's pistol grip, its top at the hand's grip (the origin), raked back as an M4's (y, z).
GRIP_RIFLE = [(-1.0, 0.6), (-1.4, -1.5), (-4.4, -11.2), (-5.2, -12.0), (-8.6, -11.7), (-9.0, -10.8), (-6.2, 0.6)]
# A pistol's grip, the hand round its middle.
GRIP_PISTOL = [(1.0, 5.5), (0.6, 4.0), (-1.6, -4.6), (-2.1, -5.4), (-5.4, -5.2), (-5.6, -4.4), (-3.3, 5.5)]


def rifle_grip(mat, half=1.5, drop=0.0):
    prism(mat, [(y, z - drop) for y, z in GRIP_RIFLE], -half, half, round_by=0.6, segments=3)


def rifle_trigger(drop=0.0):
    tube("Steel", [(0, -1.4, -1.4 - drop), (0, 1.5, -3.9 - drop), (0, 5.0, -3.6 - drop), (0, 6.0, -1.0 - drop)], 0.3)
    prism("Steel", [(1.6, -0.5 - drop), (2.0, -1.6 - drop), (1.4, -3.0 - drop), (0.9, -3.0 - drop), (1.2, -1.6 - drop), (0.9, -0.5 - drop)], -0.3, 0.3)


def pistol_grip(mat, half=1.45):
    prism(mat, GRIP_PISTOL, -half, half, round_by=0.55, segments=3)


def pistol_trigger():
    tube("Steel", [(0, -0.4, 3.4), (0, 1.6, 1.4), (0, 4.0, 1.6), (0, 4.6, 3.4), (0, 4.4, 5.0)], 0.28)
    prism("Steel", [(2.3, 4.6), (2.5, 3.6), (2.0, 2.5), (1.6, 2.5), (1.8, 3.6), (1.6, 4.6)], -0.28, 0.28)


def bore_end(centre_x, centre_z, y0, y1, outer, inner, deep=3.0, sides=24):
    """A muzzle: a steel tube from y0 to y1 with its bore open at y1."""
    lathe("Steel", [(0.0, y0), (outer, y0), (outer, y1 - 0.15), (outer - 0.12, y1), (inner, y1), (inner, y1 - deep), (0.0, y1 - deep)],
          centre=(centre_x, 0.0, centre_z), sides=sides)


def rail(y0, y1, z, width=2.2):
    box("Steel", (0, (y0 + y1) / 2.0, z + 0.5), (width, y1 - y0, 1.0), 0.12, 1)
    y = y0 + 0.8
    while y < y1 - 0.6:
        box("Steel", (0, y, z + 1.15), (width + 0.2, 0.5, 0.35), 0.05, 1)
        y += 1.2


def wheel_profile(radius, length, fillet, bulge=0.0, steps=8):
    """A cheese wheel along its axis (radius, along), from 0 to `length`: flat faces, the edge rounded by `fillet`,
    its side bulging a little."""
    profile = [(0.0, 0.0)]
    for k in range(steps + 1):
        a = math.pi / 2.0 * k / steps
        profile.append((radius - fillet + fillet * math.sin(a), fillet - fillet * math.cos(a)))
    for k in range(1, 6):
        t = fillet + (length - 2 * fillet) * k / 6.0
        profile.append((radius + bulge * math.sin(math.pi * k / 6.0), t))
    for k in range(steps + 1):
        a = math.pi / 2.0 * (steps - k) / steps
        profile.append((radius - fillet + fillet * math.sin(a), length - fillet + fillet * math.cos(a)))
    profile.append((0.0, length))
    return profile


def mark(name, sight=None, muzzle=None, left=None):
    MARKS[name] = (sight, muzzle, left)


# ---------------------------------------------------------------------------------------------------------------------
# The weapons


def blaster():
    """The Cheddar Blaster: Lyra's rifle's length, sights and hold, its receiver, handguard and stock blocks of cheddar."""
    box("Cheddar", (0, 11.0, 4.5), (5.6, 34.0, 13.0), 0.9, 3)
    # A crumb or two broken off the block's edges.
    chunk = box("Cheddar", (0, 39.5, 3.6), (5.4, 22.0, 9.0), 1.5, 3)
    cut(PARTS[0], [cutter_box((2.9, 22.0, 10.8), (2.4, 4.0, 2.6), Matrix.Rotation(0.5, 4, 'Y')),
                   cutter_box((-2.9, 1.0, -1.6), (2.2, 3.0, 2.2), Matrix.Rotation(0.6, 4, 'Z'))])
    cut(chunk, [cutter_box((2.8, 48.5, 7.8), (2.2, 3.6, 2.2), Matrix.Rotation(0.6, 4, 'X'))])
    prism("Cheddar", [(-6.0, 10.2), (-6.0, 0.5), (-12.0, -1.0), (-26.0, -6.5), (-26.0, 9.6), (-14.0, 10.2)], -2.4, 2.4, 0.8, 3)
    prism("Black", [(-26.0, 9.8), (-26.0, -6.7), (-27.4, -6.9), (-27.4, 10.0)], -2.5, 2.5, 0.35, 2)
    rail(-5.0, 27.5, 11.0)
    for x in (-0.95, 0.95):
        box("Steel", (x, -3.6, 14.4), (0.5, 1.4, 4.4), 0.1, 1)
    box("Steel", (0, -3.6, 12.6), (2.4, 1.6, 0.9), 0.1, 1)
    prism("Steel", [(25.4, 12.2), (28.4, 12.2), (27.5, 14.0), (27.2, 16.1), (26.9, 16.1), (26.6, 14.0)], -0.3, 0.3)
    for x in (-1.0, 1.0):
        box("Steel", (x, 26.9, 13.8), (0.35, 1.8, 3.2), 0.1, 1)
    cylinder("Steel", (0, 27.0, 4.2), (0, 64.0, 4.2), 1.0)
    box("Steel", (0, 51.5, 5.0), (2.4, 2.4, 3.2), 0.3, 2)
    bore_end(0.0, 4.2, 63.0, 69.5, 1.6, 0.6, 3.0)
    for y in (65.0, 66.6, 68.2):
        box("Black", (0, y, 4.2), (3.4, 0.5, 1.0))
    prism("Black", [(5.5, -1.0), (11.0, -1.0), (11.6, -6.0), (12.8, -13.5), (7.5, -14.2), (6.4, -6.5)], -1.5, 1.5, 0.3, 2)
    rifle_grip("Black")
    rifle_trigger()
    mark("blaster", sight=(-0.25, -13.0, 16.2), muzzle=(0, 69.0, 4.0), left=(0, 33.0, 3.5))
    return ["Cheddar", "Steel", "Black"]


def carbine():
    """The Colby Carbine: an M4's steel receiver and barrel, its stock and handguard logs of marbled Colby."""
    prism("Steel", [(-4.5, 8.8), (13.5, 8.8), (14.0, 7.0), (14.0, -0.5), (9.5, -1.2), (3.0, -1.2), (1.0, -3.0), (-2.0, -1.2), (-4.5, 1.0)], -2.0, 2.0, 0.3, 2)
    rail(-4.5, 14.0, 8.8)
    for x in (-0.85, 0.85):
        box("Steel", (x, -3.0, 11.6), (0.45, 1.2, 4.0), 0.1, 1)
    # The handguard: a longhorn of Colby round the barrel.
    lathe("Colby", wheel_profile(4.1, 17.0, 1.2, 0.15), centre=(0.0, 14.2, 4.5), wobble=lambda a, t: 0.08 * math.sin(5 * a + t))
    cylinder("Steel", (0, 13.0, 4.5), (0, 43.5, 4.5), 0.85)
    cylinder("Steel", (0, 31.0, 4.5), (0, 33.0, 4.5), 1.4)
    prism("Steel", [(31.2, 5.5), (32.8, 5.5), (32.3, 13.4), (31.7, 13.4)], -0.35, 0.35)
    bore_end(0.0, 4.5, 43.0, 46.4, 1.1, 0.5, 2.0)
    prism("Black", [(3.0, -1.0), (9.0, -1.0), (9.6, -6.0), (10.6, -13.0), (5.8, -13.6), (4.6, -6.0)], -1.3, 1.3, 0.3, 2)
    # The stock: a longhorn of Colby, its end in a butt pad.
    stock = lathe("Colby", wheel_profile(4.3, 25.0, 1.4, 0.2), centre=(0.0, 0.0, 0.0), wobble=lambda a, t: 0.1 * math.sin(4 * a + 0.7 * t))
    stock.matrix_world = Matrix.Translation((0.0, -4.0, 5.0)) @ Matrix.Rotation(math.radians(7.0), 4, 'X') @ Matrix.Rotation(math.pi, 4, 'Z')
    cylinder("Black", (0, -28.4, 2.0), (0, -30.2, 1.8), 4.4, 4.5)
    rifle_grip("Black")
    rifle_trigger()
    mark("carbine", sight=(0, -11.2, 13.4), muzzle=(0, 46.4, 4.5), left=(0, 26.9, 4.5))
    return ["Colby", "Steel", "Black"]


def roquefort():
    """The Roquefort Rifle: a long thumbhole stock of blue-veined cheese, a steel action and barrel, a scope in wax."""
    stock = prism("Roquefort", [(50.0, 4.6), (12.0, 4.6), (-3.0, 4.6), (-8.0, 6.0), (-26.0, 6.6), (-28.2, 6.9), (-28.2, -9.6), (-26.0, -10.0),
                                (-12.0, -11.2), (-9.0, -11.6), (-5.0, -11.4), (-1.4, -1.5), (3.0, -2.6), (12.0, -2.6), (44.0, -2.0), (50.0, 0.2)],
                  -2.3, 2.3, 0.8, 3)
    cut(stock, [cutter_box((0, -16.2, -3.2), (8.0, 14.6, 6.4), None), cutter_cylinder((0, 30.0, 6.2), (0, 52.0, 6.2), 1.25)])
    box("Steel", (0, 4.5, 5.0), (3.2, 15.0, 3.4), 0.6, 3)
    tube("Steel", [(-1.5, 1.2, 5.8), (-3.2, 0.6, 5.2), (-4.8, 0.0, 4.4)], 0.32)
    sphere("Steel", (-4.9, 0.0, 4.3), 0.75)
    cylinder("Steel", (0, 11.0, 5.5), (0, 69.0, 5.5), 0.95, 0.8)
    bore_end(0.0, 5.5, 68.0, 72.1, 1.25, 0.5, 2.0)
    # The scope: a tube dipped in dark red wax, on two steel rings.
    lathe("ScopeWax", [(0.0, -5.5), (1.85, -5.5), (2.0, -4.2), (1.9, -2.8), (1.3, -1.8), (1.25, 12.8), (1.5, 14.4), (2.25, 17.0), (2.35, 20.6), (2.25, 21.2), (0.0, 21.2)],
          centre=(0.0, 0.0, 8.0), wobble=lambda a, t: 0.05 * math.sin(3 * a + 0.4 * t))
    for y in (-5.6, 21.3):
        cylinder("Black", (0, y - 0.05, 8.0), (0, y + 0.05, 8.0), 1.6)
    for y in (1.5, 9.5):
        lathe("Steel", [(0.0, y), (1.55, y), (1.55, y + 1.2), (0.0, y + 1.2)], centre=(0.0, 0.0, 8.0), sides=24)
        box("Steel", (0, y + 0.6, 6.6), (1.6, 1.2, 1.4), 0.15, 1)
    tube("Steel", [(1.4, 7.0, 9.4), (2.2, 7.0, 9.2)], 0.45)
    prism("Black", [(6.0, -2.4), (11.0, -2.4), (11.0, -5.0), (6.4, -5.2)], -1.3, 1.3, 0.2, 1)
    rifle_trigger()
    mark("roquefort", sight=(0, -8.0, 8.0), muzzle=(0, 72.1, 5.5), left=(0, 25.0, 1.7))
    return ["Roquefort", "Steel", "ScopeWax", "Black"]


def pistol():
    """The Curd Pistol: lumps of fresh curd melted into a pistol, with a steel muzzle, sights and trigger guard."""
    rng = random.Random(7)
    balls = []
    # The slide, the frame under it and the grip: curds of every size, crowded into one lumpy mass.
    for y0, y1, z, radius, wide in ((-4.6, 18.6, 8.3, (2.0, 2.9), 0.62), (-3.0, 15.0, 6.6, (1.8, 2.5), 0.66), (-2.6, 12.5, 4.8, (1.8, 2.5), 0.62)):
        y = y0
        while y < y1:
            balls.append(((rng.uniform(-0.45, 0.45), y, z + rng.uniform(-0.6, 0.6)), rng.uniform(*radius), (wide, 1.0, 0.85)))
            y += rng.uniform(1.0, 1.9)
    for k in range(10):
        t = k / 9.0
        balls.append(((rng.uniform(-0.35, 0.35), -1.4 - 2.6 * t + rng.uniform(-0.5, 0.5), 4.0 - 8.6 * t), rng.uniform(2.3, 3.2), (0.55, 0.85, 0.85)))
    for k in range(10):
        # Little curds stuck on.
        y = rng.uniform(-4.0, 17.0)
        balls.append(((rng.choice((-1.0, 1.0)) * rng.uniform(0.9, 1.2), y, rng.uniform(5.0, 9.0)), rng.uniform(0.9, 1.3), (0.8, 0.8, 0.8)))
    metaballs("Curd", balls, 0.32)
    bore_end(0.0, 7.6, 16.0, 20.8, 0.85, 0.42, 2.0)
    box("Steel", (0, 18.4, 10.0), (0.35, 1.0, 1.6), 0.1, 1)
    for x in (-0.6, 0.6):
        box("Steel", (x, -3.4, 10.0), (0.45, 0.9, 1.6), 0.1, 1)
    pistol_trigger()
    box("Steel", (0, -3.6, -5.7), (3.2, 4.6, 0.6), 0.2, 1)
    mark("pistol", sight=(0, -32.0, 10.6), muzzle=(0, 20.8, 7.6), left=(-3.0, 9.5, 1.5))
    return ["Curd", "Steel"]


def shotgun():
    """The Swiss Shotgun: a wedge of Swiss on its side, tip forward, its cut faces holed through and through, a double
    barrel out of the tip, its rind at the back and curling back into the stock and grip."""
    rng = random.Random(11)
    tip = Vector((35.0, 3.0))
    radius, half = 36.0, 6.5
    spread = math.asin(half / radius)
    outline = [tuple(tip)]
    for k in range(13):
        a = spread - 2 * spread * k / 12
        outline.append((tip.x - radius * math.cos(a), tip.y + radius * math.sin(a)))
    width = 5.0
    wedge = prism("Swiss", outline, -width, width, 0.35, 2)
    faces_facing(wedge, (0, -1, 0), "Rind", 0.9)
    back_top, back_bottom = Vector(outline[1]), Vector(outline[-1])
    holes = []
    for k in range(60):
        r = rng.uniform(0.7, 2.2)
        u, v = rng.random(), rng.random()
        if u + v > 1.0:
            u, v = 1.0 - u, 1.0 - v
        on = tip + (back_top - tip) * u + (back_bottom - tip) * v
        if on.x < 2.5 or on.x > 29.0:
            continue
        kind = rng.random()
        if kind < 0.7:
            # A cut face, either side.
            side = 1.0 if kind < 0.42 else -1.0
            p = Vector((side * (width + rng.uniform(-0.6, 0.25) * r), on.x, on.y))
        else:
            # The top or the bottom, sloping to the tip.
            edge_from = back_top if kind < 0.88 else back_bottom
            t = rng.uniform(0.1, 0.85)
            e = edge_from + (tip - edge_from) * t
            out = Vector((0.0, 1.0 if edge_from is back_top else -1.0))
            p = Vector((rng.uniform(-3.5, 3.5), e.x, e.y + out.y * rng.uniform(-0.6, 0.2) * r))
            if p.y > 27.0:
                continue
        holes.append(sphere(None, p, r, (rng.uniform(0.85, 1.15), rng.uniform(0.9, 1.25), rng.uniform(0.85, 1.1)), cutter=True))
    # Eyes right through.
    for y, z, r in ((12.0, 5.5, 1.5), (20.0, 1.2, 1.1), (7.0, -0.5, 1.0), (16.0, 7.2, 0.9)):
        holes.append(cutter_cylinder((-width - 1, y, z), (width + 1, y, z), r, 20))
    cut(wedge, holes)
    prism("Rind", [(-0.6, 8.6), (-0.6, -3.0), (-9.5, -2.2), (-30.5, -7.0), (-32.0, -7.2), (-32.0, 5.6), (-30.5, 6.0), (-8.0, 7.0)], -1.9, 1.9, 0.7, 3)
    rifle_grip("Rind", 1.45)
    rifle_trigger(1.0)
    for x in (-1.12, 1.12):
        lathe("Steel", [(0.0, 29.0), (1.05, 29.0), (1.05, 47.7), (0.95, 48.0), (0.72, 48.0), (0.72, 44.0), (0.0, 44.0)], centre=(x, 0.0, 3.0), sides=24)
    box("Steel", (0, 40.0, 4.05), (0.8, 15.0, 0.5), 0.1, 1)
    box("Rind", (0, 40.0, 0.8), (4.2, 8.0, 2.4), 0.7, 3)
    mark("shotgun", sight=(0, -12.0, 14.0), muzzle=(0, 48.0, 3.0), left=(0, 40.0, 0.8))
    return ["Swiss", "Rind", "Steel"]


def munster():
    """One of the Munster Twin SMGs: a stubby block of Munster, its orange rind round it and its pale paste at the cut
    ends, a short steel barrel, and a belt of cheese slices hanging from it in place of a magazine."""
    body = box("MunsterRind", (0, 6.0, 6.2), (5.2, 22.0, 7.6), 1.0, 3)
    faces_facing(body, (0, 1, 0), "Munster", 0.9)
    faces_facing(body, (0, -1, 0), "Munster", 0.9)
    cut(body, [sphere(None, (2.6, 9.0, 8.4), 0.9, cutter=True), sphere(None, (-2.5, 2.0, 4.0), 0.7, cutter=True), sphere(None, (1.0, 17.1, 7.6), 0.8, cutter=True)])
    lathe("Steel", [(0.0, 16.6), (1.3, 16.6), (1.3, 22.6), (0.0, 22.6)], centre=(0.0, 0.0, 6.0), sides=20)
    for y in (18.0, 19.5, 21.0):
        lathe("Black", [(0.0, y), (1.38, y), (1.38, y + 0.5), (0.0, y + 0.5)], centre=(0.0, 0.0, 6.0), sides=20)
    bore_end(0.0, 6.0, 22.0, 25.3, 0.65, 0.35, 1.5)
    pistol_grip("Black")
    pistol_trigger()
    # The belt: slices of Munster hung edge to edge on two steel wires, in a curve down from under the barrel.
    path = []
    for k in range(10):
        t = k / 9.0
        path.append(Vector((0.0, 11.0 + 3.2 * math.sin(math.pi * t * 0.8) - 1.5 * t, 2.6 - 10.5 * t)))
    for x in (-2.0, 2.0):
        tube("Steel", [(x, p.y, p.z) for p in path], 0.14)
    for k in range(9):
        a, b = path[k], path[k + 1]
        mid = (a + b) / 2.0
        ahead = (b - a).normalized()
        turn = Vector((0.0, 0.0, 1.0)).rotation_difference(ahead).to_matrix().to_4x4()
        s = box("Munster", (0, 0, 0), (3.7, 3.5, 0.55), 0.12, 1)
        s.matrix_world = Matrix.Translation(mid) @ turn
        faces_facing(s, (0, 1, 0), "MunsterRind", 0.8)
    mark("munster", sight=(0, -14.0, 11.0), muzzle=(0, 25.3, 6.0))
    return ["Munster", "MunsterRind", "Steel", "Black"]


def grater():
    """The Cheese Grater: a steel box grater, its coarse face forward, held by a D handle over its top and down its back
    (the grip in red), a block of Parmesan inside it."""
    top_z, bottom_z = 9.0, -13.0
    top = {"x": 4.0, "y0": 5.2, "y1": 11.2}
    bot = {"x": 5.0, "y0": 4.2, "y1": 12.2}

    def corner(sx, front, z):
        k = top if z == top_z else bot
        return Vector((sx * k["x"], k["y1"] if front else k["y0"], z))

    def plate(points):
        bm = bmesh.new()
        bm.faces.new([bm.verts.new(p) for p in points])
        obj = part(bm, "GraterSteel", "plate", recalc=False)
        m = obj.modifiers.new("solid", 'SOLIDIFY')
        m.thickness = 0.16
        m.offset = 0.0
        return obj

    front = plate([corner(-1, True, bottom_z), corner(1, True, bottom_z), corner(1, True, top_z), corner(-1, True, top_z)])
    back = plate([corner(1, False, bottom_z), corner(-1, False, bottom_z), corner(-1, False, top_z), corner(1, False, top_z)])
    left = plate([corner(1, True, bottom_z), corner(1, False, bottom_z), corner(1, False, top_z), corner(1, True, top_z)])
    right = plate([corner(-1, False, bottom_z), corner(-1, True, bottom_z), corner(-1, True, top_z), corner(-1, False, top_z)])
    plate([corner(-1, False, top_z), corner(1, False, top_z), corner(1, True, top_z), corner(-1, True, top_z)])

    def on_face(sx_frac, z, front_face, across=True):
        """A point on the front or back face, a share across it, at a height."""
        f = (z - bottom_z) / (top_z - bottom_z)
        half = bot["x"] + (top["x"] - bot["x"]) * f
        y = (bot["y1"] + (top["y1"] - bot["y1"]) * f) if front_face else (bot["y0"] + (top["y0"] - bot["y0"]) * f)
        return Vector((sx_frac * half, y, z))

    # The coarse face: rows of raised, punched teeth's holes.
    holes = []
    for row in range(12):
        z = bottom_z + 1.8 + row * 1.62
        for col in range(6):
            share = -0.78 + col * 0.312 + (0.156 if row % 2 else 0.0)
            if share > 0.82:
                continue
            p = on_face(share, z, True)
            holes.append(cutter_cylinder(p - Vector((0, 1.0, 0)), p + Vector((0, 1.0, 0)), 0.5, 12, (1.5, 1.0)))
    cut(front, holes)
    holes = []
    for row in range(16):
        z = bottom_z + 1.4 + row * 1.25
        for col in range(9):
            share = -0.8 + col * 0.2
            p = on_face(share, z, False)
            holes.append(cutter_cylinder(p - Vector((0, 1.0, 0)), p + Vector((0, 1.0, 0)), 0.26, 8))
    cut(back, holes)
    holes = []
    for row in range(9):
        z = bottom_z + 2.0 + row * 2.2
        for col in range(4):
            f = (z - bottom_z) / (top_z - bottom_z)
            y0 = bot["y0"] + (top["y0"] - bot["y0"]) * f
            y1 = bot["y1"] + (top["y1"] - bot["y1"]) * f
            y = y0 + (y1 - y0) * (0.2 + 0.2 * col + (0.1 if row % 2 else 0.0))
            if y > y1 - 0.8:
                continue
            x = bot["x"] + (top["x"] - bot["x"]) * f
            holes.append(cutter_cylinder((x - 1.0, y, z), (x + 1.0, y, z), 0.55, 6))
    cut(left, holes)
    cut(right, [cutter_box((-4.6, 8.2, -3.0), (2.0, 4.8, 0.9), Matrix.Rotation(math.radians(-2.6), 4, 'Y'))])
    # Rolled edges: the corners and the rims.
    for sx in (-1, 1):
        for fr in (True, False):
            cylinder("GraterSteel", corner(sx, fr, bottom_z), corner(sx, fr, top_z), 0.22, sides=8)
    rim = [corner(-1, False, bottom_z), corner(1, False, bottom_z), corner(1, True, bottom_z), corner(-1, True, bottom_z), corner(-1, False, bottom_z)]
    tube("GraterSteel", [tuple(p) for p in rim], 0.3, smooth=False)
    rim = [corner(-1, False, top_z), corner(1, False, top_z), corner(1, True, top_z), corner(-1, True, top_z), corner(-1, False, top_z)]
    tube("GraterSteel", [tuple(p) for p in rim], 0.3, smooth=False)
    # The handle: a steel D from the top's front edge back over it and down the back of the grip, round under the
    # hand and into the box's back; the grip itself red.
    tube("GraterSteel", [(0, 10.6, 9.0), (0, 10.0, 12.2), (0, 7.0, 13.2), (0, 1.0, 13.1), (0, -2.8, 11.6), (0, -4.6, 7.0), (0, -4.0, 0.6)], 0.55)
    tube("GraterSteel", [(0, -6.6, -11.6), (0, -5.0, -12.8), (0, 0.0, -12.9), (0, 3.0, -12.0), (0, 4.4, -10.6)], 0.5)
    rifle_grip("Grip", 1.35)
    # Parmesan inside it, its block seen through the holes, a few shavings caught in the coarse face.
    box("Parmesan", (0, 8.0, -0.8), (6.6, 4.6, 14.0), 0.6, 2, Matrix.Rotation(0.12, 4, 'Y'))
    rng = random.Random(5)
    for k in range(5):
        p = on_face(rng.uniform(-0.6, 0.6), rng.uniform(-10.0, 4.0), True) + Vector((0, 0.5, 0))
        path = [p + Vector((0.0, 0.15 * j, -0.5 * j)) + Vector((0.25 * math.sin(j * 1.6), 0.4 * math.sin(j * 1.1), 0.0)) for j in range(5)]
        ribbon("Parmesan", [tuple(q) for q in path], 0.55, 0.12, lambda q: (0.0, 1.0, 0.0))
    mark("grater", sight=(6.5, -16.0, 10.0), muzzle=(0, 11.9, -2.0), left=(0, 8.2, -14.2))
    return ["GraterSteel", "Grip", "Parmesan"]


def babybomb():
    """The BabyBomb: a shoulder tube of three red-waxed Babybel wheels, the white pull-strip spiralling round them,
    BABYBOMB embossed in the middle one's wax, a wheel waiting in its bore."""
    axis_z, radius, length = 9.0, 9.0, 15.0
    starts = (-20.0, -4.5, 11.0)
    wheels = []
    for k, y in enumerate(starts):
        wheels.append(lathe("BabyWax", wheel_profile(radius, length, 3.3, 0.25), centre=(0.0, y, axis_z), sides=56,
                            wobble=lambda a, t: 0.04 * math.sin(7 * a + 0.3 * t)))
    front = starts[-1] + length
    cut(wheels[-1], [cutter_cylinder((0, front - 8.0, axis_z), (0, front + 2.0, axis_z), 4.6, 40)])
    # The wax cut through shows the cheese: the bore's lining and the ring round it.
    lathe("BabyCheese", [(4.62, front - 7.6), (4.62, front - 0.02), (4.45, front - 0.02), (4.45, front - 7.6)], centre=(0.0, 0.0, axis_z), sides=40)
    lathe("BabyCheese", [(4.6, front - 0.05), (6.1, front - 0.05), (6.1, front + 0.04), (4.6, front + 0.04)], centre=(0.0, 0.0, axis_z), sides=40)
    lathe("BabyWax", wheel_profile(4.3, 5.0, 1.6), centre=(0.0, front - 7.4, axis_z), sides=32)
    # The pull-strip: a helix a little proud of the wax, its tab loose at the front.
    turns, path = 1.75, []
    steps = 140
    for k in range(steps + 1):
        t = k / steps
        a = 2 * math.pi * turns * t + 0.9
        path.append((radius * 1.045 * math.cos(a), starts[0] + 0.6 + (front - starts[0] - 1.2) * t, axis_z + radius * 1.045 * math.sin(a)))
    ribbon("Strip", path, 1.25, 0.1, lambda p: (p[0], 0.0, p[2] - axis_z))
    a = 2 * math.pi * turns + 0.9
    end = Vector(path[-1])
    out = Vector((math.cos(a), 0.0, math.sin(a)))
    ribbon("Strip", [tuple(end), tuple(end + Vector((0, 1.0, 0)) + out * 0.3), tuple(end + Vector((0, 2.2, 0)) + out * 1.2)], 1.25, 0.1,
           lambda p: (out.x, 0.0, out.z))
    for side in (1.0, -1.0):
        text("BabyWax", "BABYBOMB", 2.2, 0.36, (side * (radius + 0.13), starts[1] + length / 2.0, axis_z), (side, 0, 0), (0, 0, 1))
    rifle_grip("Black")
    rifle_trigger()
    lathe("Black", [(0.0, 0.8), (1.35, 0.8), (1.45, -1.0), (1.4, -8.0), (1.2, -8.8), (0.0, -8.8)], centre=(0.0, 17.5, 0.0), axis="Z", sides=20)
    box("Steel", (0, -14.0, 18.4), (2.4, 1.2, 1.0), 0.15, 1)
    for x in (-0.85, 0.85):
        box("Steel", (x, -14.0, 20.0), (0.45, 0.8, 2.6), 0.1, 1)
    box("Steel", (0, 23.0, 18.4), (1.4, 1.4, 1.0), 0.15, 1)
    prism("Steel", [(22.6, 18.6), (23.4, 18.6), (23.2, 21.0), (22.8, 21.0)], -0.3, 0.3)
    mark("babybomb", sight=(0, -26.0, 21.0), muzzle=(0, front, axis_z), left=(0, 17.5, -4.0))
    # Four slots at most: its little steel is black.
    return ["BabyWax", "Strip", "Black", "BabyCheese"]


def babybomb_round():
    """The BabyBomb's round: a little red-waxed Babybel, its faces fore and aft along X, its strip round its middle."""
    radius, length = 5.5, 7.0
    lathe("BabyWax", wheel_profile(radius, length, 2.2, 0.15), centre=(-length / 2.0, 0.0, 0.0), axis="X", sides=40)
    lathe("Strip", [(radius + 0.05, -0.6), (radius + 0.3, -0.55), (radius + 0.3, 0.55), (radius + 0.05, 0.6)], centre=(0.0, 0.0, 0.0), axis="X", sides=40)
    box("Strip", (0.0, 0.0, radius + 0.8), (1.0, 0.12, 1.6), 0.03, 1)
    return ["BabyWax", "Strip"]


def grater_shard():
    """A shard of the Grater's: a curled sliver of grated Parmesan along X."""
    path = []
    for k in range(13):
        t = k / 12.0
        path.append((-4.0 + 8.0 * t, 0.5 * math.sin(math.pi * t), 0.6 * math.sin(math.pi * t * 1.4)))
    ribbon("Parmesan", path, lambda t: 1.3 * (0.35 + 0.65 * math.sin(math.pi * min(max(t, 0.05), 0.95))), 0.35, lambda p: (0.0, 0.3, 1.0))
    return ["Parmesan"]


def stick():
    """The Cheese Stick: 1.2 m of string cheese along -Y from the butt (+15) to the striking end (-105), in its red
    wrapper where the hand holds it, its strands peeled and splaying at the end."""
    sides = 32
    profile = [(0.0, 15.4), (1.6, 15.4), (2.45, 14.9), (2.6, 14.0)]
    for k in range(1, 16):
        profile.append((2.6, 14.0 - (14.0 + 72.0) * k / 15.0))
    profile += [(2.4, -74.0), (1.8, -77.0), (1.55, -80.0), (1.5, -92.0), (1.4, -98.0), (1.1, -100.5), (0.0, -101.6)]
    lathe("StringCheese", [(r, t) for r, t in profile], centre=(0.0, 0.0, 0.0), sides=sides,
          wobble=lambda a, t: 0.07 * math.sin(16 * a + 0.05 * t) + 0.04 * math.sin(7 * a - 0.11 * t))
    # The strands, peeled off the last 30 cm and splaying out and back.
    rng = random.Random(3)
    for k in range(8):
        a = 2 * math.pi * k / 8 + rng.uniform(-0.2, 0.2)
        reach = rng.uniform(4.0, 7.0)
        length = rng.uniform(26.0, 32.0)
        points, radii = [], []
        for j in range(8):
            t = j / 7.0
            r = 2.2 + reach * t ** 1.5
            aa = a + 0.25 * t * (1 if k % 2 else -1)
            y = -74.0 - length * t + 2.5 * t ** 3
            points.append((r * math.cos(aa), y, r * math.sin(aa)))
            radii.append(1.0 - 0.6 * t)
        tube("StringCheese", points, 0.95, radii=radii, resolution=2)
    # The wrapper: red, crimped at the butt, torn off below the hand.
    bm = bmesh.new()
    rings = []
    ys = [15.6, 15.0, 10.0, 4.0, -2.0, -5.0]
    for i, y in enumerate(ys):
        ring = []
        for k in range(sides):
            a = 2 * math.pi * k / sides
            r = 2.82 if i else 2.0
            yy = y + ((0.7 if k % 2 else -0.5) if i == len(ys) - 1 else 0.0)
            ring.append(bm.verts.new((r * math.cos(a), yy, r * math.sin(a))))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(sides):
            bm.faces.new((r0[k], r0[(k + 1) % sides], r1[(k + 1) % sides], r1[k]))
    bm.faces.new(list(reversed(rings[0])))
    wrapper = part(bm, "Wrapper", "wrapper")
    m = wrapper.modifiers.new("solid", 'SOLIDIFY')
    m.thickness = 0.12
    box("Wrapper", (0.0, 16.3, 0.0), (4.4, 1.4, 0.18), 0.05, 1)
    mark("stick", muzzle=(0, -101.0, 0))
    return ["StringCheese", "Wrapper"]


def gatling():
    """The Gouda Gatling: six steel barrels through three gouda wheels before a housing that is a whole gouda in
    yellow wax, a carry handle over it, a waxed grip under the barrels for the other hand."""
    axis_z = 6.0
    lathe("GoudaWax", wheel_profile(11.0, 14.0, 3.6, 0.5), centre=(0.0, -5.0, axis_z), sides=56)
    cylinder("Steel", (0, 8.0, axis_z), (0, 71.0, axis_z), 1.1)
    for k in range(6):
        a = 2 * math.pi * k / 6 + math.pi / 6
        x, z = 2.7 * math.cos(a), axis_z + 2.7 * math.sin(a)
        cylinder("Steel", (x, 8.0, z), (x, 70.0, z), 0.68, sides=16)
        bore_end(x, z, 69.5, 72.0, 0.8, 0.42, 1.5, sides=16)
    for y in (20.0, 40.0, 59.0):
        lathe("Gouda", wheel_profile(5.8, 4.2, 1.7, 0.2), centre=(0.0, y, axis_z), sides=40)
    lathe("Steel", [(0.0, 67.0), (4.1, 67.0), (4.1, 68.6), (0.0, 68.6)], centre=(0.0, 0.0, axis_z), sides=32)
    tube("Steel", [(0, -3.0, 15.4), (0, -2.0, 19.0), (0, 2.0, 19.6), (0, 6.0, 19.0), (0, 7.0, 15.4)], 0.6)
    for x in (-0.7, 0.7):
        box("Steel", (x, -1.8, 20.0), (0.4, 0.8, 1.6), 0.1, 1)
    lathe("GoudaWax", [(0.0, 25.0), (1.2, 25.0), (1.35, 26.0), (1.35, 32.0), (1.2, 33.0), (0.0, 33.0)], centre=(0.0, 0.0, 1.5), sides=20)
    for y in (25.5, 32.5):
        cylinder("Steel", (0, y, 1.5), (0, y, 3.6), 0.35, sides=12)
    rifle_grip("Black")
    rifle_trigger()
    mark("gatling", sight=(0, -14.0, 20.0), muzzle=(0, 72.0, axis_z), left=(0, 29.0, 1.5))
    return ["Gouda", "GoudaWax", "Steel", "Black"]


WEAPONS = {"blaster": blaster, "carbine": carbine, "roquefort": roquefort, "pistol": pistol, "shotgun": shotgun, "munster": munster,
           "grater": grater, "babybomb": babybomb, "stick": stick, "gatling": gatling, "babybomb_round": babybomb_round, "grater_shard": grater_shard}


# ---------------------------------------------------------------------------------------------------------------------
# Joining, exporting, looking


# A part's material a weapon has no slot for goes on this one's.
FALLBACK = {"Steel": "Black"}


def joined(name, slots):
    """Every part's evaluated mesh in one, its faces on the slots in order, smooth but for its sharp edges; mirrored in
    Y for Unreal's importer."""
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    for obj in PARTS:
        me = bpy.data.meshes.new_from_object(obj.evaluated_get(dg))
        me.transform(obj.matrix_world)
        names = [m.name if m is not None else slots[0] for m in me.materials] or [slots[0]]
        offset = len(bm.verts)
        bm.verts.ensure_lookup_table()
        verts = [bm.verts.new(v.co) for v in me.vertices]
        for poly in me.polygons:
            try:
                f = bm.faces.new([verts[i] for i in poly.vertices])
            except ValueError:
                continue
            used = names[min(poly.material_index, len(names) - 1)]
            f.material_index = slots.index(used if used in slots else FALLBACK[used])
        bpy.data.meshes.remove(me)
    for f in bm.faces:
        f.smooth = True
    for e in bm.edges:
        if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(42.0):
            e.smooth = False
    for v in bm.verts:
        v.co.y = -v.co.y
    bmesh.ops.reverse_faces(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for s in slots:
        me.materials.append(material(s))
    obj = link(bpy.data.objects.new(name, me))
    # Normals weighted by the faces' areas: a block's flat sides stay flat and its rounded edges smooth.
    m = obj.modifiers.new("normals", 'WEIGHTED_NORMAL')
    m.mode = 'FACE_AREA'
    m.keep_sharp = True
    m.weight = 50
    return obj


def export(obj, path):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True, forward_axis='Y', up_axis='Z', export_materials=True,
                          export_normals=True, export_uv=True, export_triangulated_mesh=True, apply_modifiers=True, global_scale=1.0)


def preview(obj, name, folder):
    """Two pictures of it as it will be in the game: from its right side, and from behind and over it, as its player
    sees it; with its marks."""
    for o in list(bpy.context.scene.objects):
        if o is not obj:
            o.hide_render = True
    sight, muzzle, left = MARKS.get(name, (None, None, None))
    dots = []
    for p, colour in ((sight, (1, 0, 0)), (muzzle, (0, 1, 0)), (left, (0, 0.3, 1))):
        if p is None:
            continue
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.7)
        bmesh.ops.translate(bm, vec=Vector((p[0], -p[1], p[2])), verts=bm.verts)
        me = bpy.data.meshes.new("dot")
        bm.to_mesh(me)
        m = bpy.data.materials.new("dot")
        m.diffuse_color = (colour[0], colour[1], colour[2], 1.0)
        me.materials.append(m)
        dots.append(link(bpy.data.objects.new("dot", me)))
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_cavity = True
    scene.display.shading.show_object_outline = False
    scene.render.film_transparent = False
    scene.render.resolution_x, scene.render.resolution_y = 1000, 600
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.render.image_settings.color_depth = '8'
    corners = [v.co for v in obj.data.vertices]
    lo = Vector([min(c[i] for c in corners) for i in range(3)])
    hi = Vector([max(c[i] for c in corners) for i in range(3)])
    middle = (lo + hi) / 2.0
    size = max(hi - lo)
    cam = link(bpy.data.objects.new("cam", bpy.data.cameras.new("cam")))
    scene.camera = cam
    sun = link(bpy.data.objects.new("sun", bpy.data.lights.new("sun", 'SUN')))
    # The grip's place (the origin), a small cross.
    for d in ((1.6, 0.12, 0.12), (0.12, 1.6, 0.12), (0.12, 0.12, 1.6)):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = Vector((v.co.x * d[0], v.co.y * d[1], v.co.z * d[2]))
        me = bpy.data.meshes.new("cross")
        bm.to_mesh(me)
        m = bpy.data.materials.new("cross")
        m.diffuse_color = (1.0, 0.0, 1.0, 1.0)
        me.materials.append(m)
        dots.append(link(bpy.data.objects.new("cross", me)))
    across = max(hi.y - lo.y, (hi.z - lo.z) * 1000.0 / 600.0) * 1.12
    views = [("side", 'ORTHO', middle + Vector((-200.0, 0.0, 0.0)), across),
             ("shoulder", 'PERSP', middle + Vector((size * 0.5, size * 1.15, size * 0.5)), 0.0)]
    world = bpy.data.worlds.new("grey")
    world.color = (0.35, 0.37, 0.4)
    scene.world = world
    for label, kind, at, ortho in views:
        cam.data.type = kind
        if kind == 'ORTHO':
            cam.data.ortho_scale = ortho
        else:
            cam.data.lens = 35
        cam.location = at
        cam.rotation_euler = (middle - at).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = os.path.join(folder, f"{name}_{label}.png")
        bpy.ops.render.render(write_still=True)
    for o in dots + [cam, sun]:
        bpy.data.objects.remove(o)


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    folder, previews, names = WORK, None, []
    i = 0
    while i < len(args):
        if args[i] == "--preview":
            previews = args[i + 1]
            i += 2
            continue
        if args[i] in WEAPONS:
            names.append(args[i])
        else:
            folder = args[i]
        i += 1
    names = names or list(WEAPONS)
    os.makedirs(folder, exist_ok=True)
    for name in names:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        PARTS.clear()
        CUTTERS.clear()
        slots = WEAPONS[name]()
        obj = joined("SM_" + name, slots)
        for o in PARTS + CUTTERS:
            o.hide_render = True
            o.hide_set(True)
        export(obj, os.path.join(folder, f"cheese_{name}.obj"))
        print(f"{name}: {len(obj.data.vertices)} vertices, {len(obj.data.polygons)} faces, slots {slots}")
        if previews:
            os.makedirs(previews, exist_ok=True)
            preview(obj, name, previews)


main()
