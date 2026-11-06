import traceback
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/smoke_cloud.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/smoke_cloud.txt.
#
# Builds the look of the Smoked Gouda's cloud (Weapons/CheeseStrikeSmokeCloud,
# Weapons/CheeseStrikeGrenadeRules.h's Cloud* numbers), in /Game/CheeseStrike/Effects:
#   M_SmokeMist   the cloud's cards (the blood's mist cards, cheese_blood.py's
#                 M_CheeseMist, made again in smoke's colours): a smoky grey
#                 with a faint gouda-orange tint, thickest in the middle of
#                 each card and fading to nothing at a round, wavy edge, each
#                 card's thickness its instance's custom data 0, with no light
#                 of its own (smoke is lit, not glowing). Since wave 16 a card
#                 may be solid (custom data 1, 0 to 1): a solid one is whole
#                 out to three quarters of its radius and soft only at its
#                 edge, which the cloud's veil is made of (the cards nothing is
#                 seen through, between this machine's camera and the cloud's
#                 middle), which fades softly where it goes into the floor or
#                 anything else (a depth fade; a swirling card drawn over the
#                 veil mustn't, or its fade would outline whoever stands behind
#                 it); each card is lit a little lighter or darker (custom data
#                 2, its shade, 1 for none), and a solid card is mottled and
#                 lighter toward its top, so that the veil reads as a body of
#                 smoke.
# The game loads it by path, so this script can rebuild it.
OUT = unreal.Paths.project_saved_dir() + "smoke_cloud.txt"
FOLDER = "/Game/CheeseStrike/Effects"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Linear: a mid grey warmed a little toward gouda (the arenas' sun warms it further: any more orange reads as dust).
SMOKE_COLOUR = unreal.LinearColor(0.3, 0.29, 0.28, 1.0)
# How far a card fades as it nears what is behind it (cm).
SOFT_DISTANCE = 60.0

MIST = r"""
float2 p = UV * 2.0 - 1.0;
float r = length(p);
// Thickest in the middle, nothing at the edge, a little uneven round it so that the cards don't read as discs.
float a = atan2(p.y, p.x);
float edge = 0.92 + 0.06 * sin(a * 3.0 + Fade * 7.0) + 0.04 * sin(a * 7.0 - Fade * 3.0);
float body = saturate((edge - r) / edge);
// A solid card is whole to three quarters of its radius; a swirling one soft all through.
float solid = saturate(Solid);
// Only a solid card fades where it nears what is behind it: a swirling one drawn over the veil would show the
// outline of whoever stands behind it by its fade.
return saturate(pow(body, lerp(1.6, 0.35, solid)) * lerp(0.55, 3.0, solid) * Fade) * lerp(1.0, Soft, solid);
"""

# A solid card's light: mottled by a slow stirring, and lighter toward its top (the card's -V; its UV's V runs down),
# so that the veil reads as a body of smoke rather than a flat shape. A swirling card is its shade alone.
MOTTLE = r"""
float2 p = UV * 2.0 - 1.0;
float n = sin(p.x * 4.1 + T * 0.3) * sin(p.y * 3.3 - T * 0.25) + 0.5 * sin(p.x * 7.7 - p.y * 5.9 + T * 0.5);
return Shade * (1.0 + saturate(Solid) * (0.07 * n - 0.12 * p.y));
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


def material(name):
    path = FOLDER + "/" + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    return m, path


def build_smoke():
    m, path = material("M_SmokeMist")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    # Writing its depth (for the temporal upscaler) would stop it reading the depth its soft edge needs.
    m.set_editor_property("output_translucent_velocity", False)
    code = node(m, unreal.MaterialExpressionCustom, -450, 100, code=MIST, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Smoke")
    code.set_editor_property("inputs", [custom_input(n) for n in ["UV", "Fade", "Solid", "Soft"]])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -900, 0), code, "UV")
    link(node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, 150, data_index=0), code, "Fade")
    link(node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, 250, data_index=1), code, "Solid")
    link(node(m, unreal.MaterialExpressionDepthFade, -900, 350, fade_distance_default=SOFT_DISTANCE), code, "Soft")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_OPACITY)
    colour = node(m, unreal.MaterialExpressionVectorParameter, -700, -200, parameter_name="Colour", default_value=SMOKE_COLOUR)
    mottle = node(m, unreal.MaterialExpressionCustom, -700, -350, code=MOTTLE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Mottle")
    mottle.set_editor_property("inputs", [custom_input(n) for n in ["UV", "Solid", "Shade", "T"]])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -1000, -450), mottle, "UV")
    link(node(m, unreal.MaterialExpressionPerInstanceCustomData, -1000, -350, data_index=1), mottle, "Solid")
    link(node(m, unreal.MaterialExpressionPerInstanceCustomData, -1000, -250, data_index=2, const_default_value=1.0), mottle, "Shade")
    link(node(m, unreal.MaterialExpressionTime, -1000, -150), mottle, "T")
    shaded = node(m, unreal.MaterialExpressionMultiply, -450, -150)
    link(colour, shaded, "A")
    link(mottle, shaded, "B")
    mel.connect_material_property(shaded, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(m, unreal.MaterialExpressionConstant, -450, -50, r=1.0)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append("M_SmokeMist saved " + str(lib.save_asset(path, False)))


try:
    build_smoke()
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
