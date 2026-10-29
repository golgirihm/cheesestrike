import math, os, unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/cheese_blood.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/cheese_blood.txt.
#
# Builds the looks of the cheese players bleed (Effects/CheeseStrikeCheeseBlood,
# Rules/CheeseStrikeBlood.h), in /Game/CheeseStrike/Effects:
#   M_CheeseBlood       the drops: molten cheese, an opaque deep yellow-orange,
#                       glossy, a little lighter where it faces the light and
#                       glowing faintly at its rim so it reads in the shade;
#   SM_CheeseDrop       a small teardrop wearing it, made here (an OBJ written
#                       to Saved/ and imported): radius 1 cm, 3 cm long along
#                       +X, its round head ahead and its tail tapering behind,
#                       which the drops are instances of, drawn out further
#                       along their flight the faster they go;
#   M_CheeseMist        the soft puffs of mist (flat cards the game turns to
#                       the camera): translucent yellow, thickest in the
#                       middle and fading to nothing at a round edge, each
#                       card's thickness its instance's custom data 0;
#   M_CheeseBloodDecal  a splat or pool on the floor and the walls: a deferred
#                       decal, a blob with a wavy edge and a few drops round it
#                       (its shape from Seed, which each decal sets at random),
#                       paler and thicker in the middle, glossy, faded by
#                       Opacity.
# The game loads them by path, so this script can rebuild them.
OUT = unreal.Paths.project_saved_dir() + "cheese_blood.txt"
FOLDER = "/Game/CheeseStrike/Effects"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Linear colours: a deep molten orange-yellow, and its paler, thicker middle.
DEEP = unreal.LinearColor(0.80, 0.30, 0.006, 1.0)
PALE = unreal.LinearColor(0.93, 0.48, 0.025, 1.0)
# The mist: between the two, deep enough to stand out against the arenas' cream walls.
MIST_COLOUR = unreal.LinearColor(0.85, 0.36, 0.01, 1.0)

DECAL = r"""
float2 p = UV * 2.0 - 1.0;
float r = length(p);
float a = atan2(p.y, p.x);
// A blob with a wavy edge, its waves turned by the seed.
float edge = 0.74 + 0.08 * sin(a * 3.0 + Seed) + 0.05 * sin(a * 5.0 + Seed * 1.7) + 0.035 * sin(a * 9.0 + Seed * 2.3);
float inside = edge - r;
float body = saturate(inside * 16.0);
// A few drops flung round it.
float drops = 0.0;
for (int i = 0; i < 6; i++)
{
    float ang = Seed * 0.37 + i * 1.047 + sin(Seed + i * 1.9) * 0.5;
    float d = 0.84 + 0.1 * sin(Seed * 1.3 + i * 2.1);
    float2 c = float2(cos(ang), sin(ang)) * d;
    float rr = 0.035 + 0.035 * frac(sin(Seed * 3.1 + i * 7.7) * 43758.5);
    drops = max(drops, saturate((rr - length(p - c)) * 40.0));
}
float cover = max(body, drops);
// Thicker toward the middle: paler there, and a dome whose slope at the edge catches the light.
Thick = saturate(inside * 3.0);
float slope = (1.0 - saturate(inside * 7.0)) * body;
float2 outward = r > 0.001 ? p / r : float2(0.0, 0.0);
Bump = normalize(float3(outward * slope * 0.7, 1.0));
return saturate(cover) * Opacity;
"""

DROP = r"""
float3 V = normalize(Cam - Pos);
float facing = saturate(dot(normalize(N), V));
float rim = pow(1.0 - facing, 3.0);
return Colour * (0.03 + 0.12 * rim);
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


def custom_output(name, kind):
    o = unreal.CustomOutput()
    o.set_editor_property("output_name", name)
    o.set_editor_property("output_type", kind)
    return o


def material(name):
    path = FOLDER + "/" + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    return m, path


def build_drop():
    m, path = material("M_CheeseBlood")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("two_sided", True)
    colour = node(m, unreal.MaterialExpressionVectorParameter, -900, -200, parameter_name="Colour", default_value=DEEP)
    pale = node(m, unreal.MaterialExpressionVectorParameter, -900, -50, parameter_name="Pale", default_value=PALE)
    # Paler where the drop faces up (the light's way, mostly), deeper underneath.
    up = node(m, unreal.MaterialExpressionVertexNormalWS, -900, 100)
    mask = node(m, unreal.MaterialExpressionComponentMask, -700, 100, r=False, g=False, b=True, a=False)
    link(up, mask)
    blend = node(m, unreal.MaterialExpressionLinearInterpolate, -450, -150)
    link(colour, blend, "A")
    link(pale, blend, "B")
    sat = node(m, unreal.MaterialExpressionSaturate, -550, 100)
    link(mask, sat)
    link(sat, blend, "Alpha")
    mel.connect_material_property(blend, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(m, unreal.MaterialExpressionScalarParameter, -450, 50, parameter_name="Roughness", default_value=0.16)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = node(m, unreal.MaterialExpressionConstant, -450, 150, r=0.7)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    code = node(m, unreal.MaterialExpressionCustom, -450, 300, code=DROP, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Rim")
    code.set_editor_property("inputs", [custom_input(n) for n in ["Pos", "Cam", "N", "Colour"]])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionWorldPosition, -900, 300), code, "Pos")
    link(node(m, unreal.MaterialExpressionCameraPositionWS, -900, 400), code, "Cam")
    link(node(m, unreal.MaterialExpressionVertexNormalWS, -900, 500), code, "N")
    link(colour, code, "Colour")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("M_CheeseBlood saved " + str(lib.save_asset(path, False)))
    return m


def build_decal():
    m, path = material("M_CheeseBloodDecal")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    code = node(m, unreal.MaterialExpressionCustom, -500, 0, code=DECAL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Splat")
    code.set_editor_property("inputs", [custom_input(n) for n in ["UV", "Seed", "Opacity"]])
    code.set_editor_property("additional_outputs", [custom_output("Thick", unreal.CustomMaterialOutputType.CMOT_FLOAT1), custom_output("Bump", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -900, -100), code, "UV")
    link(node(m, unreal.MaterialExpressionScalarParameter, -900, 0, parameter_name="Seed", default_value=0.0), code, "Seed")
    link(node(m, unreal.MaterialExpressionScalarParameter, -900, 100, parameter_name="Opacity", default_value=1.0), code, "Opacity")
    blend = node(m, unreal.MaterialExpressionLinearInterpolate, -200, -250)
    link(node(m, unreal.MaterialExpressionVectorParameter, -500, -400, parameter_name="Colour", default_value=DEEP), blend, "A")
    link(node(m, unreal.MaterialExpressionVectorParameter, -500, -250, parameter_name="Pale", default_value=PALE), blend, "B")
    link(code, blend, "Alpha", "Thick")
    mel.connect_material_property(blend, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(code, "Bump", unreal.MaterialProperty.MP_NORMAL)
    rough = node(m, unreal.MaterialExpressionScalarParameter, -200, 150, parameter_name="Roughness", default_value=0.14)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append("M_CheeseBloodDecal saved " + str(lib.save_asset(path, False)))


MIST = r"""
float r = length(UV * 2.0 - 1.0);
// Soft all the way: thickest in the middle, nothing at the card's round edge.
float soft = saturate(1.0 - r);
return pow(soft, 1.5) * Fade;
"""


def build_mist():
    m, path = material("M_CheeseMist")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    code = node(m, unreal.MaterialExpressionCustom, -450, 100, code=MIST, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Mist")
    code.set_editor_property("inputs", [custom_input(n) for n in ["UV", "Fade"]])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -900, 0), code, "UV")
    fade = node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, 150, data_index=0)
    link(fade, code, "Fade")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_OPACITY)
    colour = node(m, unreal.MaterialExpressionVectorParameter, -450, -150, parameter_name="Colour", default_value=MIST_COLOUR)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # A little of its own light, so that it reads in the shade as a fine spray does.
    glow = node(m, unreal.MaterialExpressionMultiply, -200, 0)
    link(colour, glow, "A")
    link(node(m, unreal.MaterialExpressionConstant, -450, 0, r=0.08), glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("M_CheeseMist saved " + str(lib.save_asset(path, False)))


def drop_obj(file):
    """A teardrop along +X: a round head of radius 1 ahead (x 0 to 1), a tail tapering to a point at x = -2."""
    around = 10
    profile = []
    for deg in (0, 25, 50, 70, 90):
        a = math.radians(deg)
        profile.append((math.cos(a), math.sin(a)))
    for x in (-0.4, -0.8, -1.2, -1.6):
        profile.append((x, math.cos(math.pi / 2 * (-x) / 2.0) ** 1.5))
    tail = (-2.0, 0.0)
    # Each profile point's normal in the (x, r) plane, from its neighbours.
    full = profile + [tail]
    normals = []
    for i, (x, r) in enumerate(profile):
        if i == 0:
            normals.append((1.0, 0.0))
            continue
        px, pr = full[i - 1]
        nx, nr = full[i + 1]
        dx, dr = nx - px, nr - pr
        length = math.hypot(dx, dr) or 1.0
        normals.append((dr / length, -dx / length))
    out = ["# CheeseStrike's cheese drop"]
    verts = []
    norms = []
    # The nose: one point.
    verts.append((1.0, 0.0, 0.0))
    norms.append((1.0, 0.0, 0.0))
    for i in range(1, len(profile)):
        x, r = profile[i]
        nx, nr = normals[i]
        # Outward: the normal's r part points away from the axis.
        if nr < 0:
            nx, nr = -nx, -nr
        for k in range(around):
            t = 2 * math.pi * k / around
            verts.append((x, r * math.cos(t), r * math.sin(t)))
            norms.append((nx, nr * math.cos(t), nr * math.sin(t)))
    verts.append((tail[0], 0.0, 0.0))
    norms.append((-1.0, 0.0, 0.0))
    for v in verts:
        out.append("v %.5f %.5f %.5f" % v)
    for n in norms:
        out.append("vn %.5f %.5f %.5f" % n)
    rings = len(profile) - 1
    def ring(i, k):
        return 2 + (i * around) + (k % around)
    faces = []
    for k in range(around):
        faces.append((1, ring(0, k + 1), ring(0, k)))
    for i in range(rings - 1):
        for k in range(around):
            a, b, c, d = ring(i, k), ring(i, k + 1), ring(i + 1, k + 1), ring(i + 1, k)
            faces.append((a, b, c))
            faces.append((a, c, d))
    last = len(verts)
    for k in range(around):
        faces.append((ring(rings - 1, k), ring(rings - 1, k + 1), last))
    for f in faces:
        out.append("f " + " ".join("%d//%d" % (i, i) for i in f))
    open(file, "w").write("\n".join(out) + "\n")


def build_mesh(look):
    path = FOLDER + "/SM_CheeseDrop"
    file = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "SM_CheeseDrop.obj")
    drop_obj(file)
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", "SM_CheeseDrop")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if mesh is None:
        lines.append("SM_CheeseDrop: nothing imported from " + file + " " + str(task.get_editor_property("imported_object_paths")))
        return
    for other in task.get_editor_property("imported_object_paths"):
        if other.split(".")[0] != path:
            lib.delete_asset(other.split(".")[0])
    # Hundreds of instances of a few dozen triangles: no Nanite (whose materials would need its usage too).
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", nanite)
    mesh.set_material(0, look)
    box = mesh.get_bounding_box()
    lines.append("SM_CheeseDrop: %.2f x %.2f x %.2f, saved %s" % (box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z, lib.save_asset(path, False)))


try:
    look = build_drop()
    build_decal()
    build_mist()
    build_mesh(look)
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
