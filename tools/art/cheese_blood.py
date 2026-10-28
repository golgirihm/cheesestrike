import unreal, traceback

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
#   SM_CheeseDrop       the engine's sphere (radius 50) wearing it, which the
#                       drops are instances of, stretched along their flight;
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


def build_mesh(look):
    path = FOLDER + "/SM_CheeseDrop"
    if not lib.does_asset_exist(path):
        lib.duplicate_asset("/Engine/BasicShapes/Sphere", path)
    mesh = unreal.load_asset(path)
    mesh.set_material(0, look)
    lines.append("SM_CheeseDrop saved " + str(lib.save_asset(path, False)))


try:
    look = build_drop()
    build_decal()
    build_mesh(look)
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
