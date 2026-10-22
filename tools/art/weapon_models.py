"""Makes the weapons' models that aren't anyone else's, as OBJ files that
tools/art/import_weapons.py imports. Plain Python, no editor:

    python tools/art/weapon_models.py [folder]

The Cheese Knife (cheese_knife.obj): a cheese knife as they are made, its
blade pierced with round holes (so that cheese doesn't stick) and forked at
the tip (to pick up what it has cut), on a handle dipped in red cheese wax,
with a steel bolster and rivets. It lies along +Y (as the game's rifle
does), its spine up (+Z), its blade's flat sides facing +-X; the origin is
in the middle of the grip, where the fist closes on it. Two materials:
Steel (the blade, bolster and rivets) and Wax (the handle).

The muzzle flash (muzzle_flash.obj): two crossed quads along +Y from the
muzzle, a flame drawn on each by its material, and one facing along the
barrel for the flare seen from in front. Two materials: Side and Front.

Lengths are centimetres at life size.
"""

import math
import os
import sys

# The blade's outline (Y along the knife, Z up), going round: along the edge from the bolster, round the forked tip, and
# back along the spine.
BLADE_FROM = 6.0
OUTLINE = [
    (6.0, -2.0), (8.0, -2.08), (10.5, -2.1), (13.0, -2.02), (15.0, -1.86), (16.8, -1.62), (18.3, -1.33), (19.6, -1.02),
    (20.7, -0.72),  # the lower prong's point
    (20.45, -0.42), (19.75, 0.12),  # into the fork
    (20.55, 0.62), (21.15, 1.02),  # the upper prong's point
    (20.2, 1.2), (17.0, 1.17), (13.0, 1.18), (9.0, 1.22), (6.0, 1.25),
]
# Its holes: (Y, Z, radius).
HOLES = [(9.2, -0.42, 0.58), (12.0, -0.44, 0.58), (14.8, -0.38, 0.55), (17.4, -0.24, 0.46)]
HOLE_SIDES = 20
# How thick the blade is: thin at the edge, thicker at the spine.
EDGE_THICKNESS = 0.05
SPINE_THICKNESS = 0.24
EDGE_Z = -2.1
SPINE_Z = 1.25
# The handle: cross-sections along it (Y, height, width), its middle at HANDLE_Z, and the bolster between it and the blade.
HANDLE = [(-6.0, 2.05, 1.55), (-5.6, 2.35, 1.75), (-4.0, 2.55, 1.85), (-1.0, 2.7, 1.95), (2.0, 2.6, 1.9), (4.6, 2.45, 1.8), (5.2, 2.4, 1.76)]
HANDLE_Z = -0.42
HANDLE_SIDES = 24
BOLSTER = [(5.2, 2.75, 1.95), (5.5, 2.8, 2.0), (6.05, 3.0, 0.9), (6.3, 3.1, 0.3)]
RIVETS = [-3.0, 1.2]
RIVET_RADIUS = 0.32


class Mesh:
    def __init__(self):
        self.positions = []
        self.normals = []
        self.uvs = []
        self.faces = {}  # material -> list of triangles, each three (position, uv, normal) index triples

    def vertex(self, p, n, uv):
        self.positions.append(p)
        length = math.sqrt(sum(c * c for c in n)) or 1.0
        self.normals.append(tuple(c / length for c in n))
        self.uvs.append(uv)
        return len(self.positions)  # OBJ counts from 1

    def triangle(self, material, a, b, c):
        self.faces.setdefault(material, []).append((a, b, c))

    def write(self, path):
        with open(path, "w") as f:
            f.write("# The Cheese Knife, made by tools/art/cheese_knife.py\n")
            for p in self.positions:
                f.write("v %.5f %.5f %.5f\n" % p)
            for uv in self.uvs:
                f.write("vt %.5f %.5f\n" % uv)
            for n in self.normals:
                f.write("vn %.5f %.5f %.5f\n" % n)
            for material, triangles in self.faces.items():
                f.write("g %s\nusemtl %s\n" % (material, material))
                for a, b, c in triangles:
                    f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))


def signed_area(points):
    return 0.5 * sum(points[i][0] * points[(i + 1) % len(points)][1] - points[(i + 1) % len(points)][0] * points[i][1] for i in range(len(points)))


def circle(y, z, r, sides, clockwise):
    points = [(y + r * math.cos(2 * math.pi * k / sides), z + r * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
    return points[::-1] if clockwise else points


def cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def segments_cross(p1, p2, p3, p4):
    """Whether the segments p1-p2 and p3-p4 cross, not counting touching at an end."""
    d1, d2, d3, d4 = cross(p3, p4, p1), cross(p3, p4, p2), cross(p1, p2, p3), cross(p1, p2, p4)
    return ((d1 > 0) != (d2 > 0)) and ((d3 > 0) != (d4 > 0)) and min(abs(d1), abs(d2), abs(d3), abs(d4)) > 1e-12


def bridge(outer, hole):
    """Joins a hole (clockwise) into the outer polygon (counter-clockwise) by a cut from its rightmost point to the nearest
    outer point it can see, and returns the one polygon."""
    h = max(range(len(hole)), key=lambda i: (hole[i][0], hole[i][1]))
    hp = hole[h]
    best = None
    for i, op in sorted(enumerate(outer), key=lambda e: (e[1][0] - hp[0]) ** 2 + (e[1][1] - hp[1]) ** 2):
        clear = True
        for poly in (outer, hole):
            for k in range(len(poly)):
                a, b = poly[k], poly[(k + 1) % len(poly)]
                if a in (op, hp) or b in (op, hp):
                    continue
                if segments_cross(hp, op, a, b):
                    clear = False
                    break
            if not clear:
                break
        if clear:
            best = i
            break
    i = best
    return outer[:i + 1] + hole[h:] + hole[:h + 1] + outer[i:]


def inside_triangle(p, a, b, c):
    return cross(a, b, p) >= -1e-12 and cross(b, c, p) >= -1e-12 and cross(c, a, p) >= -1e-12


def ear_clip(polygon):
    """Triangles (as index triples into `polygon`) covering a simple counter-clockwise polygon, cuts and all."""
    remaining = list(range(len(polygon)))
    triangles = []
    guard = 0
    while len(remaining) > 3 and guard < 100000:
        guard += 1
        found = False
        for k in range(len(remaining)):
            ia, ib, ic = remaining[k - 1], remaining[k], remaining[(k + 1) % len(remaining)]
            a, b, c = polygon[ia], polygon[ib], polygon[ic]
            if cross(a, b, c) <= 1e-12:
                continue
            if any(inside_triangle(polygon[j], a, b, c) for j in remaining if j not in (ia, ib, ic) and polygon[j] not in (a, b, c)):
                continue
            triangles.append((ia, ib, ic))
            remaining.pop(k)
            found = True
            break
        if not found:
            raise RuntimeError("the blade's outline could not be cut into triangles")
    triangles.append(tuple(remaining))
    return triangles


def thickness(z):
    through = min(max((z - EDGE_Z) / (SPINE_Z - EDGE_Z), 0.0), 1.0)
    return EDGE_THICKNESS + (SPINE_THICKNESS - EDGE_THICKNESS) * math.sqrt(through)


def blade(mesh):
    outer = OUTLINE if signed_area(OUTLINE) > 0 else OUTLINE[::-1]
    holes = [circle(y, z, r, HOLE_SIDES, True) for y, z, r in HOLES]
    polygon = outer
    for hole in sorted(holes, key=lambda h: -max(p[0] for p in h)):
        polygon = bridge(polygon, hole)
    triangles = ear_clip(polygon)
    uv = lambda y, z: (y / 16.0, z / 16.0)
    # The two flat sides: +X and -X, each its own vertices.
    for side in (1.0, -1.0):
        index = {}
        for i, (y, z) in enumerate(polygon):
            key = (round(y, 6), round(z, 6))
            if key not in index:
                index[key] = mesh.vertex((side * thickness(z) / 2.0, y, z), (side, 0.0, 0.0), uv(y, z))
        for a, b, c in triangles:
            ka, kb, kc = [(round(polygon[t][0], 6), round(polygon[t][1], 6)) for t in (a, b, c)]
            if side > 0:
                mesh.triangle("Steel", index[ka], index[kb], index[kc])
            else:
                mesh.triangle("Steel", index[ka], index[kc], index[kb])
    # The walls: round the outline and round each hole, facing out of the steel.
    for loop, outward in [(outer, True)] + [(h, True) for h in holes]:
        n = len(loop)
        for k in range(n):
            (y0, z0), (y1, z1) = loop[k], loop[(k + 1) % n]
            dy, dz = y1 - y0, z1 - z0
            # Outer loop counter-clockwise, holes clockwise: the steel is on the left, so out is to the right.
            normal = (0.0, dz, -dy)
            quad = []
            for (y, z) in ((y0, z0), (y1, z1)):
                for side in (1.0, -1.0):
                    quad.append(mesh.vertex((side * thickness(z) / 2.0, y, z), normal, (y / 16.0, side * 0.01)))
            a0, a1, b0, b1 = quad
            mesh.triangle("Steel", a0, b1, b0)
            mesh.triangle("Steel", a0, a1, b1)


def loft(mesh, material, stations, sides, z_middle, cap_back, cap_front, power=4.0):
    """A rounded box swept along Y through `stations` of (Y, height, width), its ends capped."""
    def ring(y, height, width):
        points = []
        for k in range(sides):
            angle = 2 * math.pi * k / sides
            c, s = math.cos(angle), math.sin(angle)
            # A superellipse: round at the corners, flat on the sides.
            x = (width / 2.0) * math.copysign(abs(c) ** (2.0 / power), c)
            z = (height / 2.0) * math.copysign(abs(s) ** (2.0 / power), s)
            nx = math.copysign(abs(c) ** (2.0 - 2.0 / power), c) / max(width, 1e-3)
            nz = math.copysign(abs(s) ** (2.0 - 2.0 / power), s) / max(height, 1e-3)
            points.append(((x, y, z + z_middle), (nx, 0.0, nz), k / sides))
        return points

    rings = [ring(*station) for station in stations]
    indices = []
    for r, (y, _, _) in zip(rings, stations):
        indices.append([mesh.vertex(p, n, (u, y / 12.0)) for p, n, u in r])
    for a, b in zip(indices, indices[1:]):
        for k in range(sides):
            k1 = (k + 1) % sides
            mesh.triangle(material, a[k], b[k], b[k1])
            mesh.triangle(material, a[k], b[k1], a[k1])
    for ends, facing, cap in ((rings[0], -1.0, cap_back), (rings[-1], 1.0, cap_front)):
        if not cap:
            continue
        y = ends[0][0][1]
        centre = mesh.vertex((0.0, y, z_middle), (0.0, facing, 0.0), (0.5, 0.5))
        rim = [mesh.vertex(p, (0.0, facing, 0.0), (0.5 + p[0] / 4.0, 0.5 + (p[2] - z_middle) / 4.0)) for p, _, _ in ends]
        for k in range(sides):
            k1 = (k + 1) % sides
            if facing > 0:
                mesh.triangle(material, centre, rim[k1], rim[k])
            else:
                mesh.triangle(material, centre, rim[k], rim[k1])


def rivet(mesh, y, side):
    """A steel dome set into the side of the handle."""
    sides = 12
    surface = HANDLE[3][2] / 2.0 * side
    rim = []
    for k in range(sides):
        angle = 2 * math.pi * k / sides
        dy, dz = RIVET_RADIUS * math.cos(angle), RIVET_RADIUS * math.sin(angle)
        rim.append(mesh.vertex((surface, y + dy, HANDLE_Z + dz), (side * 0.4, dy, dz), (0.0, 0.0)))
    top = mesh.vertex((surface + side * 0.06, y, HANDLE_Z), (side, 0.0, 0.0), (0.0, 0.0))
    for k in range(sides):
        k1 = (k + 1) % sides
        if side > 0:
            mesh.triangle("Steel", top, rim[k], rim[k1])
        else:
            mesh.triangle("Steel", top, rim[k1], rim[k])


def flash(path):
    """The muzzle flash's cards: 14 cm of flame each way across the barrel, and a 7 cm flare facing along it."""
    mesh = Mesh()
    length, half = 14.0, 4.0
    for across in ((1.0, 0.0, 0.0), (0.0, 0.0, 1.0)):
        corners = []
        for u, v in ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)):
            offset = (v * 2.0 - 1.0) * half
            p = (across[0] * offset, u * length, across[2] * offset)
            normal = (across[2], 0.0, -across[0])
            corners.append(mesh.vertex(p, normal, (u, v)))
        mesh.triangle("Side", corners[0], corners[1], corners[2])
        mesh.triangle("Side", corners[0], corners[2], corners[3])
    flare = 3.5
    corners = []
    for u, v in ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)):
        corners.append(mesh.vertex(((u * 2.0 - 1.0) * flare, 0.5, (v * 2.0 - 1.0) * flare), (0.0, 1.0, 0.0), (u, v)))
    mesh.triangle("Front", corners[0], corners[2], corners[1])
    mesh.triangle("Front", corners[0], corners[3], corners[2])
    mesh.write(path)
    return mesh


def make(path):
    mesh = Mesh()
    blade(mesh)
    loft(mesh, "Wax", HANDLE, HANDLE_SIDES, HANDLE_Z, True, False)
    loft(mesh, "Steel", BOLSTER, HANDLE_SIDES, HANDLE_Z + 0.1, False, True, power=3.0)
    for y in RIVETS:
        for side in (1.0, -1.0):
            rivet(mesh, y, side)
    mesh.write(path)
    return mesh


if __name__ == "__main__":
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads"), "x", "weapons")
    os.makedirs(folder, exist_ok=True)
    for name, maker in (("cheese_knife.obj", make), ("muzzle_flash.obj", flash)):
        out = os.path.join(folder, name)
        made = maker(out)
        print("%s: %d vertices, %d triangles" % (out, len(made.positions), sum(len(t) for t in made.faces.values())))
