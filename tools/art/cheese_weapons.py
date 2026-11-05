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
    handguard and stock of it, on a steel barrel, rail and sights, the
    receiver's block still in its wrapper (dark blue, a yellow band, CRAFT in
    plain white letters: nobody's trade dress), the cheddar bare at the cut
    end;
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
  - grater: the Cheese Grater, a steel box grater lying along the aim, its
    open end (where the shreds come out) forward as the muzzle, its coarse
    face up, a small plain steel handle at the back in the hand, Parmesan
    inside;
  - babybomb: the BabyBomb, one big red-waxed Babybel lying flat on the end of
    a short haft (a sledgehammer, held in both hands), the white pull-strip
    wide across its top, its bore out of the wheel's front rim;
  - stick: the Cheese Stick, a 1.2 m baton of string cheese 9 cm across in its
    wrapper at the hand, peeled strands splaying at the striking end;
  - burrata: one of the Dual Burratas, a pistol that is a white burrata pouch
    (its tied knot at the top the hammer) with a creamy short barrel and a
    grip of cheese rind, in the grip frame the M1911 had;
  - gatling: the Gouda Gatling's body, a housing that is a wheel of gouda in
    yellow wax on an axle, with its handle and grips; and gatling_barrels, its
    six barrels through three gouda wheels, a part of its own that the game
    turns about the axle while it fires (FWeaponLook::SpinningPart);
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
    "Grip": (0.6, 0.04, 0.04), "Burrata": (0.95, 0.94, 0.9), "Cream": (0.93, 0.86, 0.66), "WrapBlue": (0.02, 0.05, 0.25),
    "WrapYellow": (0.95, 0.8, 0.1),
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
    # A crumb or two broken off the block's bare edges.
    chunk = box("Cheddar", (0, 39.5, 3.6), (5.4, 22.0, 9.0), 1.5, 3)
    cut(PARTS[0], [cutter_box((2.9, 22.0, 10.8), (2.4, 4.0, 2.6), Matrix.Rotation(0.5, 4, 'Y'))])
    cut(chunk, [cutter_box((2.8, 48.5, 7.8), (2.2, 3.6, 2.2), Matrix.Rotation(0.6, 4, 'X'))])
    # The wrapper: a dark-blue sleeve round the back of the block (the cheddar cut bare ahead of it, its cut end showing),
    # a yellow band round the sleeve and CRAFT in plain white letters on both sides: no logo, nobody's trade dress.
    box("WrapBlue", (0, 5.0, 4.5), (6.2, 22.0, 13.6), 0.35, 2)
    box("WrapYellow", (0, 12.0, 4.5), (6.5, 3.4, 13.9), 0.3, 1)
    for side in (1.0, -1.0):
        text("Strip", "CRAFT", 3.4, 0.3, (side * 3.12, 3.0, 4.6), (side, 0, 0), (0, 0, 1))
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
    return ["Cheddar", "Steel", "Black", "WrapBlue", "WrapYellow", "Strip"]


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
    """The Cheese Grater: a steel box grater lying along the aim, its open end (where the shreds come out) forward as the
    muzzle, its coarse face up, fine holes down one side and slicer slots down the other, a small plain steel handle at
    the back in the hand, a block of Parmesan inside it with shavings spilling out of the front."""
    y0, y1 = 3.0, 37.0
    back = {"x": 5.0, "z0": -0.5, "z1": 9.5}
    front = {"x": 6.0, "z0": -1.5, "z1": 10.5}

    def corner(sx, sz, at_front):
        k = front if at_front else back
        return Vector((sx * k["x"], y1 if at_front else y0, k["z1"] if sz > 0 else k["z0"]))

    def plate(points):
        bm = bmesh.new()
        bm.faces.new([bm.verts.new(p) for p in points])
        obj = part(bm, "GraterSteel", "plate", recalc=False)
        m = obj.modifiers.new("solid", 'SOLIDIFY')
        m.thickness = 0.16
        m.offset = 0.0
        return obj

    top = plate([corner(-1, 1, False), corner(1, 1, False), corner(1, 1, True), corner(-1, 1, True)])
    bottom = plate([corner(1, -1, False), corner(-1, -1, False), corner(-1, -1, True), corner(1, -1, True)])
    left = plate([corner(1, -1, False), corner(1, 1, False), corner(1, 1, True), corner(1, -1, True)])
    right = plate([corner(-1, 1, False), corner(-1, -1, False), corner(-1, -1, True), corner(-1, 1, True)])
    plate([corner(-1, -1, False), corner(1, -1, False), corner(1, 1, False), corner(-1, 1, False)])

    def along(y):
        """The box's half width and its top and bottom at `y`, which widens a little toward the open end."""
        f = (y - y0) / (y1 - y0)
        return (back["x"] + (front["x"] - back["x"]) * f, back["z0"] + (front["z0"] - back["z0"]) * f, back["z1"] + (front["z1"] - back["z1"]) * f)

    # The coarse face on top: rows of raised, punched teeth's holes, elongated across.
    holes = []
    for row in range(14):
        y = y0 + 3.0 + row * 2.2
        for col in range(5):
            share = -0.68 + col * 0.34 + (0.17 if row % 2 else 0.0)
            if share > 0.72:
                continue
            half, _, z1 = along(y)
            p = Vector((share * half, y, z1))
            holes.append(cutter_cylinder(p - Vector((0, 0, 1.0)), p + Vector((0, 0, 1.0)), 0.5, 12, (1.5, 1.0)))
    cut(top, holes)
    # Fine holes down the left side.
    holes = []
    for row in range(7):
        for col in range(21):
            y = y0 + 2.2 + col * 1.5
            half, z0, z1 = along(y)
            z = z0 + 1.4 + row * (z1 - z0 - 2.8) / 6.0
            holes.append(cutter_cylinder((half - 1.0, y, z), (half + 1.0, y, z), 0.26, 8))
    cut(left, holes)
    # Slicer slots down the right side.
    slots = []
    for y in (9.0, 18.0, 27.0):
        half, z0, z1 = along(y)
        slots.append(cutter_box((-half, y, (z0 + z1) / 2.0), (2.0, 5.0, 0.9), Matrix.Rotation(math.radians(-2.6), 4, 'Y')))
    cut(right, slots)
    # A few star holes underneath.
    holes = []
    for row in range(8):
        y = y0 + 3.5 + row * 3.6
        half, z0, _ = along(y)
        for share in (-0.4, 0.4):
            holes.append(cutter_cylinder((share * half, y, z0 - 1.0), (share * half, y, z0 + 1.0), 0.45, 6))
    cut(bottom, holes)
    # Rolled edges: the four long corners and the rims round the back plate and the open end.
    for sx in (-1, 1):
        for sz in (-1, 1):
            cylinder("GraterSteel", corner(sx, sz, False), corner(sx, sz, True), 0.22, sides=8)
    for at_front in (False, True):
        rim = [corner(-1, -1, at_front), corner(1, -1, at_front), corner(1, 1, at_front), corner(-1, 1, at_front), corner(-1, -1, at_front)]
        tube("GraterSteel", [tuple(p) for p in rim], 0.3, smooth=False)
    # The handle: a small plain steel D at the back, from the plate's top down behind it and back in under it, the bar in
    # the hand raked back a little, with a trigger inside it.
    tube("Steel", [(0, 3.5, 8.8), (0, 0.3, 7.6), (0, -0.8, 3.5), (0, -1.6, -2.5), (0, -1.0, -6.0), (0, 1.5, -7.0), (0, 3.5, -5.5), (0, 3.6, -1.0)], 0.8)
    prism("Steel", [(1.6, -1.4), (2.2, -2.4), (1.8, -4.2), (1.3, -4.2), (1.4, -2.4), (1.0, -1.4)], -0.3, 0.3)
    # Sights along the top: a bead at the front, a notch at the back.
    box("Steel", (0, 35.5, 11.0), (0.5, 1.0, 1.3), 0.1, 1)
    for x in (-0.6, 0.6):
        box("Steel", (x, 4.8, 10.4), (0.4, 0.8, 1.4), 0.1, 1)
    # Parmesan inside it, seen through the holes and the open end, a few shavings caught at the mouth.
    box("Parmesan", (0, 19.5, 4.5), (9.6, 30.0, 9.4), 0.6, 2, Matrix.Rotation(0.06, 4, 'Y'))
    rng = random.Random(5)
    for k in range(6):
        p = Vector((rng.uniform(-3.5, 3.5), y1 - 0.5, rng.uniform(-0.5, 8.5)))
        path = [p + Vector((0.0, 0.7 * j, 0.0)) + Vector((0.3 * math.sin(j * 1.6 + k), 0.0, 0.35 * math.sin(j * 1.1 + k))) for j in range(5)]
        ribbon("Parmesan", [tuple(q) for q in path], 0.6, 0.12, lambda q: (0.0, 0.0, 1.0))
    mark("grater", sight=(0, -14.0, 12.6), muzzle=(0, 37.5, 4.5), left=(0, 24.0, -1.6))
    return ["GraterSteel", "Parmesan", "Steel"]

def babybomb():
    """The BabyBomb: one big red-waxed Babybel lying flat (its face up) on the end of a short black haft, held in both
    hands like a sledgehammer (the right at the haft's end, where it bends down into the hand, the left ahead on it),
    the white pull-strip wide across its top, its bore out of the wheel's front rim in a steel ring, a little wheel
    waiting in it."""
    axis_z, radius, thick, centre_y = 5.5, 15.0, 9.0, 44.0
    bottom = axis_z - thick / 2.0
    wheel = lathe("BabyWax", wheel_profile(radius, thick, 3.0, 0.3), centre=(0.0, centre_y, bottom), axis="Z", sides=72,
                  wobble=lambda a, t: 0.04 * math.sin(9 * a + 0.4 * t))
    # The bore, out of the front rim, in a steel ring: the cheese shows inside it, and a wheel waits in it.
    cut(wheel, [cutter_cylinder((0, centre_y + 3.0, axis_z), (0, centre_y + radius + 2.0, axis_z), 4.1, 32)])
    lathe("BabyCheese", [(4.12, centre_y + 3.5), (4.12, centre_y + 8.5), (3.95, centre_y + 8.5), (3.95, centre_y + 3.5)], centre=(0.0, 0.0, axis_z), sides=32)
    bore_end(0.0, axis_z, centre_y + 8.0, centre_y + 16.5, 4.5, 3.9, 7.5, sides=32)
    lathe("BabyWax", wheel_profile(3.4, 4.5, 1.2), centre=(0.0, centre_y + 9.0, axis_z), sides=32)
    # The pull-strip: a wide white band across the top's diameter (side to side, clear of the bore), over the rounded
    # edge and a little way down each side, its tab loose at the left end.
    fillet, top = 3.0, bottom + thick
    path, normals = [], []
    flat = radius - fillet
    for k in range(25):
        x = -flat + 2 * flat * k / 24
        path.append((x, centre_y, top))
        normals.append((0.0, 0.0, 1.0))
    for sign in (1.0, -1.0):
        over = []
        for k in range(1, 10):
            a = math.pi / 2 * k / 9
            over.append(((sign * (flat + fillet * math.sin(a)), centre_y, top - fillet + fillet * math.cos(a)), (sign * math.sin(a), 0.0, math.cos(a))))
        for z in (top - fillet - 1.2, top - fillet - 2.4):
            over.append(((sign * (radius + 0.06), centre_y, z), (sign, 0.0, 0.0)))
        if sign < 0:
            over.reverse()
            path = [q for q, n in over] + path
            normals = [n for q, n in over] + normals
        else:
            path += [q for q, n in over]
            normals += [n for q, n in over]
    lookup = {tuple(round(c, 3) for c in q): n for q, n in zip(path, normals)}
    ribbon("Strip", path, 10.0, 0.14, lambda q: lookup[tuple(round(c, 3) for c in q)])
    end = Vector(path[-1])
    ribbon("Strip", [tuple(end), tuple(end + Vector((1.4, 0.0, -0.6))), tuple(end + Vector((3.0, 0.0, -0.2)))], 10.0, 0.14, lambda q: (1.0, 0.0, 0.0))
    # The haft: a black bar under the wheel, from its middle back and bending down into the hand, a steel ferrule where it
    # meets the wheel.
    tube("Black", [(0, 46.0, 1.2), (0, 12.0, 1.2), (0, -1.0, 1.0), (0, -3.0, -2.5), (0, -4.0, -7.0), (0, -4.3, -11.5)], 2.2,
         radii=[1.0, 1.0, 1.0, 1.0, 1.0, 0.75], resolution=4)
    cylinder("Steel", (0, 26.0, 1.2), (0, 29.5, 1.2), 2.6)
    # Sights: a bead on the wheel's front top, a notched post on the haft's bend.
    box("Steel", (0, 56.0, 10.8), (0.5, 1.0, 1.8), 0.1, 1)
    box("Steel", (0, 1.5, 6.8), (0.6, 1.2, 7.6), 0.1, 1)
    for x in (-0.55, 0.55):
        box("Steel", (x, 1.5, 11.3), (0.4, 1.0, 1.6), 0.1, 1)
    mark("babybomb", sight=(0, -20.0, 12.6), muzzle=(0, centre_y + 15.5, axis_z), left=(0, 22.0, -1.0))
    return ["BabyWax", "Strip", "Black", "BabyCheese", "Steel"]

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
    """The Cheese Stick: 1.2 m of string cheese, 9 cm across, along -Y from the butt (+15) to the striking end (-105), in
    its red wrapper where the hand holds it, its strands peeled and splaying at the end."""
    sides = 36
    radius = 4.5
    profile = [(0.0, 15.4), (2.8, 15.4), (4.3, 14.9), (radius, 14.0)]
    for k in range(1, 16):
        profile.append((radius, 14.0 - (14.0 + 72.0) * k / 15.0))
    profile += [(4.2, -74.0), (3.3, -77.0), (2.9, -80.0), (2.7, -92.0), (2.5, -98.0), (2.0, -100.5), (0.0, -101.6)]
    lathe("StringCheese", [(r, t) for r, t in profile], centre=(0.0, 0.0, 0.0), sides=sides,
          wobble=lambda a, t: 0.12 * math.sin(18 * a + 0.05 * t) + 0.07 * math.sin(7 * a - 0.11 * t))
    # The strands, peeled off the last 30 cm and splaying out and back: string cheese.
    rng = random.Random(3)
    for k in range(12):
        a = 2 * math.pi * k / 12 + rng.uniform(-0.15, 0.15)
        reach = rng.uniform(5.0, 9.0)
        length = rng.uniform(27.0, 34.0)
        points, radii = [], []
        for j in range(8):
            t = j / 7.0
            r = 3.6 + reach * t ** 1.5
            aa = a + 0.3 * t * (1 if k % 2 else -1)
            y = -74.0 - length * t + 2.5 * t ** 3
            points.append((r * math.cos(aa), y, r * math.sin(aa)))
            radii.append(1.0 - 0.55 * t)
        tube("StringCheese", points, 1.15, radii=radii, resolution=2)
    # The wrapper: red, crimped at the butt, torn off below the hand.
    bm = bmesh.new()
    rings = []
    ys = [15.6, 15.0, 10.0, 4.0, -2.0, -5.0]
    for i, y in enumerate(ys):
        ring = []
        for k in range(sides):
            a = 2 * math.pi * k / sides
            r = radius + 0.25 if i else 3.6
            yy = y + ((0.8 if k % 2 else -0.6) if i == len(ys) - 1 else 0.0)
            ring.append(bm.verts.new((r * math.cos(a), yy, r * math.sin(a))))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(sides):
            bm.faces.new((r0[k], r0[(k + 1) % sides], r1[(k + 1) % sides], r1[k]))
    bm.faces.new(list(reversed(rings[0])))
    wrapper = part(bm, "Wrapper", "wrapper")
    m = wrapper.modifiers.new("solid", 'SOLIDIFY')
    m.thickness = 0.12
    box("Wrapper", (0.0, 16.3, 0.0), (7.2, 1.4, 0.18), 0.05, 1)
    mark("stick", muzzle=(0, -101.0, 0), left=(0, -24.0, 0))
    return ["StringCheese", "Wrapper"]

def burrata():
    """One of the Dual Burratas: a pistol that is a white burrata pouch, lumpy and soft, its tied knot at the top-back
    the hammer, a creamy short barrel out of its front, a grip of cheese rind and a steel trigger and sights; in the
    grip frame the M1911 had (the hand round the grip's middle, the bore 7.5 up, the muzzle 19 ahead)."""
    rng = random.Random(13)
    balls = []
    # The pouch: lumps crowded along the bore, fat at the back and narrowing toward the barrel.
    for y0, y1, z, radius, wide in ((-5.0, 11.5, 7.6, (2.6, 3.3), 0.78), (-3.5, 9.5, 5.4, (2.0, 2.6), 0.85)):
        y = y0
        while y < y1:
            taper = 1.0 - 0.4 * max(0.0, (y - 1.0) / 10.0)
            balls.append(((rng.uniform(-0.3, 0.3), y, z + rng.uniform(-0.4, 0.4)), rng.uniform(*radius) * taper, (wide, 1.0, 0.9)))
            y += rng.uniform(1.2, 1.9)
    for k in range(6):
        # Little swellings on its skin.
        balls.append(((rng.choice((-1.0, 1.0)) * rng.uniform(1.4, 2.0), rng.uniform(-4.0, 8.0), rng.uniform(5.5, 9.5)), rng.uniform(0.9, 1.3), (0.8, 0.9, 0.8)))
    metaballs("Burrata", balls, 0.3)
    # The knot: the pouch's neck twisted and tied, standing up at the back and leaning back, where a hammer would be.
    knot = lathe("Burrata", [(0.0, 0.0), (2.0, 0.0), (1.4, 1.3), (1.1, 2.3), (1.7, 3.3), (2.0, 4.1), (1.3, 4.9), (0.0, 5.3)], centre=(0.0, 0.0, 0.0), axis="Z",
                 sides=24, wobble=lambda a, t: 0.2 * math.sin(3 * a + 1.4 * t))
    knot.matrix_world = Matrix.Translation((0.0, -5.5, 9.0)) @ Matrix.Rotation(math.radians(32.0), 4, 'X')
    # The barrel: cream, short, its bore open.
    lathe("Cream", [(0.0, 9.0), (1.7, 9.0), (1.85, 10.5), (1.75, 18.3), (1.55, 19.0), (1.15, 19.0), (1.15, 16.0), (0.0, 16.0)], centre=(0.0, 0.0, 7.5), sides=24,
          wobble=lambda a, t: 0.05 * math.sin(5 * a + 0.8 * t))
    box("Steel", (0, 17.3, 9.8), (0.4, 1.0, 1.3), 0.1, 1)
    for x in (-0.55, 0.55):
        box("Steel", (x, -2.0, 10.3), (0.4, 0.9, 1.4), 0.1, 1)
    pistol_grip("Rind")
    pistol_trigger()
    box("Steel", (0, -3.6, -5.7), (3.2, 4.6, 0.6), 0.2, 1)
    mark("burrata", sight=(0, -31.6, 10.7), muzzle=(0, 19.0, 7.5), left=(-3.0, 9.5, 1.5))
    return ["Burrata", "Cream", "Rind", "Steel"]


def gatling_barrels():
    """The Gouda Gatling's barrels: six steel barrels through three gouda wheels, clamped by a steel ring at the muzzle; a
    part of their own, in the same frame as the body, that the game turns about the axle (along Y at the body's axis
    height) while it fires."""
    axis_z = 6.0
    for k in range(6):
        a = 2 * math.pi * k / 6 + math.pi / 6
        x, z = 2.7 * math.cos(a), axis_z + 2.7 * math.sin(a)
        cylinder("Steel", (x, 8.0, z), (x, 70.0, z), 0.68, sides=16)
        bore_end(x, z, 69.5, 72.0, 0.8, 0.42, 1.5, sides=16)
    for y in (20.0, 40.0, 59.0):
        lathe("Gouda", wheel_profile(5.8, 4.2, 1.7, 0.2), centre=(0.0, y, axis_z), sides=40)
    lathe("Steel", [(0.0, 67.0), (4.1, 67.0), (4.1, 68.6), (0.0, 68.6)], centre=(0.0, 0.0, axis_z), sides=32)
    return ["Gouda", "Steel"]


def gatling():
    """The Gouda Gatling's body: a housing that is a whole gouda in yellow wax on a steel axle the barrels (gatling_barrels,
    a part of their own) turn about, a carry handle over it, a waxed grip under the barrels for the other hand."""
    axis_z = 6.0
    lathe("GoudaWax", wheel_profile(11.0, 14.0, 3.6, 0.5), centre=(0.0, -5.0, axis_z), sides=56)
    cylinder("Steel", (0, 8.0, axis_z), (0, 71.0, axis_z), 1.1)
    # A gouda collar on the axle where the barrels begin, so that every slot is used (the OBJ keeps only those).
    lathe("Gouda", wheel_profile(2.2, 2.0, 0.6), centre=(0.0, 8.5, axis_z), sides=24)
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
           "grater": grater, "babybomb": babybomb, "stick": stick, "burrata": burrata, "gatling": gatling, "gatling_barrels": gatling_barrels,
           "babybomb_round": babybomb_round, "grater_shard": grater_shard}


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
