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
#                 each card and fading to nothing at a round edge, each card's
#                 thickness its instance's custom data 0, with no light of its
#                 own (smoke is lit, not glowing).
# The game loads it by path, so this script can rebuild it.
OUT = unreal.Paths.project_saved_dir() + "smoke_cloud.txt"
FOLDER = "/Game/CheeseStrike/Effects"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Linear: a mid grey warmed a little toward gouda (the arenas' sun warms it further: any more orange reads as dust).
SMOKE_COLOUR = unreal.LinearColor(0.3, 0.29, 0.28, 1.0)

MIST = r"""
float2 p = UV * 2.0 - 1.0;
float r = length(p);
// Thickest in the middle, nothing at the edge, a little uneven round it so that the cards don't read as discs.
float a = atan2(p.y, p.x);
float edge = 0.92 + 0.06 * sin(a * 3.0 + Fade * 7.0) + 0.04 * sin(a * 7.0 - Fade * 3.0);
float body = saturate((edge - r) / edge);
return pow(body, 1.6) * 0.42 * Fade;
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
    code = node(m, unreal.MaterialExpressionCustom, -450, 100, code=MIST, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Smoke")
    code.set_editor_property("inputs", [custom_input(n) for n in ["UV", "Fade"]])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -900, 0), code, "UV")
    fade = node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, 150, data_index=0)
    link(fade, code, "Fade")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_OPACITY)
    colour = node(m, unreal.MaterialExpressionVectorParameter, -450, -150, parameter_name="Colour", default_value=SMOKE_COLOUR)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
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
