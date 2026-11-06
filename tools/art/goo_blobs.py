import math, os, random, traceback
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/goo_blobs.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/goo_blobs.txt.
#
# Builds the thick globs of melted gouda the Gouda Overload leaves at a slowed foe's feet (wave 17;
# Effects/CheeseStrikeGooSplats, Rules/CheeseStrikeGoo.h), in /Game/CheeseStrike/Effects:
#   M_GooBlob        molten gouda: opaque, a deep golden orange on its sides and paler and thicker-looking on top,
#                    mottled a little, glossy and wet, the light going a little way into it and coming out warm
#                    (subsurface);
#   SM_GooBlob1..3   three globs wearing it, made here (OBJs written to Saved/ and imported): a puddle 50 cm in radius
#                    about its middle on the floor (z 0), its edge wavy with a few lobes run out further, its top a
#                    rounded mound 12 cm high with two or three humps on it, its edge rounding steeply down to the
#                    floor as a thick liquid's does, and a few small drops round it; a flat underside 1 cm under the
#                    floor. The game scales them to each puddle's size and height and melts them down as they go.
# The game loads them by path, so this script can rebuild them.
OUT = unreal.Paths.project_saved_dir() + "goo_blobs.txt"
FOLDER = "/Game/CheeseStrike/Effects"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Linear colours: the puddles' decals' deep (CheeseStrikeGooSplats.cpp's GooDeep), and a paler top a little deeper than theirs.
DEEP = unreal.LinearColor(0.86, 0.34, 0.012, 1.0)
PALE = unreal.LinearColor(0.95, 0.5, 0.035, 1.0)
RADIUS = 50.0
HEIGHT = 12.0
RINGS = 18
AROUND = 72

# Paler on top where it is thick and faces the sky, deeper down its sides; mottled slowly across the world, so that
# neighbouring globs differ.
MOTTLE = r"""
float n = sin(P.x * 0.047 + P.y * 0.031) * sin(P.y * 0.053 - P.x * 0.019) + 0.5 * sin(P.x * 0.13 - P.y * 0.11 + 1.7);
float up = saturate(N.z);
float t = saturate(pow(up, 4.0) * 0.75 + 0.15 * n);
return lerp(Deep, Pale, t);
"""


def node(m, cls, x, y, **props):
    n = mel.create_material_expression(m, cls, x, y)
    for k, value in props.items():
        n.set_editor_property(k, value)
    return n


def link(a, b, pin="", out=""):
    if not mel.connect_material_expressions(a, out, b, pin):
        lines.append(f"couldn't link {a.get_class().get_name()}.{out} -> {b.get_class().get_name()}.{pin}")


def custom_input(name):
    i = unreal.CustomInput()
    i.set_editor_property("input_name", name)
    return i


def build_look():
    path = FOLDER + "/M_GooBlob"
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset("M_GooBlob", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
    code = node(m, unreal.MaterialExpressionCustom, -450, -150, code=MOTTLE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Gouda")
    code.set_editor_property("inputs", [custom_input(n) for n in ["P", "N", "Deep", "Pale"]])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionWorldPosition, -900, -300), code, "P")
    link(node(m, unreal.MaterialExpressionVertexNormalWS, -900, -200), code, "N")
    link(node(m, unreal.MaterialExpressionVectorParameter, -900, -100, parameter_name="Colour", default_value=DEEP), code, "Deep")
    link(node(m, unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Pale", default_value=PALE), code, "Pale")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -450, 50, r=0.14), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -450, 120, r=0.75), "", unreal.MaterialProperty.MP_SPECULAR)
    # Light goes a little way into it and comes out warm, as through melted cheese.
    sss = node(m, unreal.MaterialExpressionVectorParameter, -450, 190, parameter_name="Through", default_value=unreal.LinearColor(0.9, 0.32, 0.01, 1.0))
    mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -450, 300, r=0.45), "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    lines.append("M_GooBlob saved " + str(lib.save_asset(path, False)))
    return m


def glob(rng):
    """A glob's vertices and triangles (indices from 0): a wavy-edged mound with humps, and a few drops round it."""
    verts, tris = [], []
    lobes = [(rng.uniform(0, 2 * math.pi), rng.uniform(0.18, 0.38)) for _ in range(rng.randint(2, 4))]
    waves = [(k, rng.uniform(0.03, 0.08), rng.uniform(0, 2 * math.pi)) for k in (2, 3, 5, 7, 11)]
    humps = [(rng.uniform(-0.4, 0.4) * RADIUS, rng.uniform(-0.4, 0.4) * RADIUS, rng.uniform(0.2, 0.32) * RADIUS, rng.uniform(0.2, 0.4)) for _ in range(rng.randint(2, 3))]

    def edge(a):
        r = 1.0 + sum(amp * math.sin(k * a + ph) for k, amp, ph in waves)
        r += sum(out * max(0.0, math.cos(a - at)) ** 14 for at, out in lobes)
        return RADIUS * r

    def height(x, y, s):
        # A rounded mound, steep at the edge as a thick liquid's is, with humps on it and a fine ripple.
        mound = max(0.0, 1.0 - s ** 2.6) ** 0.42
        bumps = 1.0 + sum(h * math.exp(-((x - hx) ** 2 + (y - hy) ** 2) / (2 * w * w)) for hx, hy, w, h in humps)
        ripple = 1.0 + 0.05 * math.sin(x * 0.31 + y * 0.17) * math.sin(y * 0.27 - x * 0.12)
        return HEIGHT * mound * bumps * ripple

    def dome(cx, cy, size, rings, around, wobble):
        base = len(verts)
        verts.append((cx, cy, height_at(cx, cy, 0.0, size)))
        for i in range(1, rings + 1):
            s = i / rings
            for k in range(around):
                a = 2 * math.pi * k / around
                r = wobble(a) * s
                x, y = cx + r * math.cos(a), cy + r * math.sin(a)
                verts.append((x, y, height_at(x, y, s, size)))
        # The lip, tucked under to the floor, and the flat underside.
        lip = len(verts)
        for k in range(around):
            a = 2 * math.pi * k / around
            r = wobble(a) * 0.985
            verts.append((cx + r * math.cos(a), cy + r * math.sin(a), -1.0))
        bottom = len(verts)
        verts.append((cx, cy, -1.0))

        def ring(i, k):
            return base + 1 + (i - 1) * around + (k % around)
        for k in range(around):
            tris.append((base, ring(1, k), ring(1, k + 1)))
        for i in range(1, rings):
            for k in range(around):
                a, b, c, d = ring(i, k), ring(i, k + 1), ring(i + 1, k + 1), ring(i + 1, k)
                tris.append((a, d, c))
                tris.append((a, c, b))
        for k in range(around):
            a, b = ring(rings, k), ring(rings, k + 1)
            c, d = lip + ((k + 1) % around), lip + k
            tris.append((a, d, c))
            tris.append((a, c, b))
            tris.append((bottom, lip + ((k + 1) % around), lip + k))

    sizes = {}

    def height_at(x, y, s, size):
        if size == RADIUS:
            return height(x, y, s)
        return HEIGHT * size / RADIUS * 1.6 * max(0.0, 1.0 - s ** 2.4) ** 0.45

    dome(0.0, 0.0, RADIUS, RINGS, AROUND, edge)
    # Drops thrown out round it.
    for _ in range(rng.randint(3, 6)):
        a = rng.uniform(0, 2 * math.pi)
        size = rng.uniform(4.0, 9.0)
        out = edge(a) + size * rng.uniform(1.3, 3.0)
        dome(out * math.cos(a), out * math.sin(a), size, 4, 16, lambda _a, size=size: size)
    return verts, tris


def write_obj(file, verts, tris):
    # Smooth normals: each vertex's the sum of its triangles' (area weighted).
    normals = [[0.0, 0.0, 0.0] for _ in verts]
    for a, b, c in tris:
        ax, ay, az = verts[a]
        bx, by, bz = verts[b]
        cx, cy, cz = verts[c]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
        for i in (a, b, c):
            for j in range(3):
                normals[i][j] += n[j]
    out = ["# CheeseStrike's glob of melted gouda"]
    # Written as ours, Z up (the importer keeps Z up and mirrors Y, which a glob doesn't mind).
    for x, y, z in verts:
        out.append("v %.4f %.4f %.4f" % (x, y, z))
    for x, y, z in verts:
        out.append("vt %.5f %.5f" % (x / (4 * RADIUS) + 0.5, y / (4 * RADIUS) + 0.5))
    for n in normals:
        length = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
        out.append("vn %.5f %.5f %.5f" % (n[0] / length, n[1] / length, n[2] / length))
    for a, b, c in tris:
        out.append("f " + " ".join("%d/%d/%d" % (i + 1, i + 1, i + 1) for i in (a, b, c)))
    open(file, "w").write("\n".join(out) + "\n")


def build_meshes(look):
    for number in (1, 2, 3):
        name = f"SM_GooBlob{number}"
        path = FOLDER + "/" + name
        file = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), name + ".obj")
        verts, tris = glob(random.Random(1700 + number * 31))
        write_obj(file, verts, tris)
        if lib.does_asset_exist(path):
            lib.delete_asset(path)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", file)
        task.set_editor_property("destination_path", FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset(path)
        if mesh is None:
            lines.append(name + ": nothing imported from " + file + " " + str(task.get_editor_property("imported_object_paths")))
            continue
        for other in task.get_editor_property("imported_object_paths"):
            if other.split(".")[0] != path:
                lib.delete_asset(other.split(".")[0])
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
        mesh.set_material(0, look)
        box = mesh.get_bounding_box()
        lines.append("%s: %d vertices, %d triangles; x %.1f to %.1f, y %.1f to %.1f, z %.1f to %.1f; saved %s" % (
            name, len(verts), len(tris), box.min.x, box.max.x, box.min.y, box.max.y, box.min.z, box.max.z, lib.save_asset(path, False)))


try:
    build_meshes(build_look())
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
