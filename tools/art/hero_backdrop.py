import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/hero_backdrop.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/hero_backdrop.txt.
#
# Builds M_HeroBackdrop (wave 16), the plain soft fog the loadout page's hero stands in (UI/CheeseStrikeHeroView): drawn
# on the inside of a sphere round the hero and the camera, unlit and two-sided, by where it is on the screen rather than
# where it is in the world, so that the whole picture is one smooth fog with nothing to see in it: a white-grey that is a
# little darker at the top and lighter toward the floor, with a soft haze of brighter white behind the hero's middle and
# the corners a touch greyer. Its parameters: Top, Bottom and Haze (colours) and Brightness, which the view's exposure
# takes as it is. The hero view loads it by path (ACheeseStrikeHeroView::BackdropPath), so it can be rebuilt.
OUT = unreal.Paths.project_saved_dir() + "hero_backdrop.txt"
FOLDER = "/Game/CheeseStrike/UI"
NAME = "M_HeroBackdrop"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary

FOG = r"""
float2 p = UV;
float3 c = lerp(Top, Bottom, smoothstep(0.0, 1.0, p.y));
// A brighter haze behind the hero's middle, wider than tall, and the corners a touch greyer.
float2 d = (p - float2(0.5, 0.52)) * float2(1.5, 1.0);
float haze = exp(-dot(d, d) * 3.2);
c = lerp(c, Haze, haze * 0.65);
float2 e = p - 0.5;
c *= 1.0 - 0.18 * saturate(dot(e, e) * 2.0);
return c * Brightness;
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


try:
    path = FOLDER + "/" + NAME
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    code = node(m, unreal.MaterialExpressionCustom, -500, 0, code=FOG, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Fog")
    names = ["UV", "Top", "Bottom", "Haze", "Brightness"]
    code.set_editor_property("inputs", [custom_input(n) for n in names])
    mel.recompile_material(m)  # so the node's pins exist before linking
    # The screen's position, 0 to 1 across and down (its first output, ViewportUV).
    link(node(m, unreal.MaterialExpressionScreenPosition, -1000, -200), code, "UV", "ViewportUV")
    colours = {"Top": (0.78, 0.79, 0.81), "Bottom": (0.93, 0.93, 0.92), "Haze": (1.0, 1.0, 0.99)}
    for i, (name, value) in enumerate(colours.items()):
        link(node(m, unreal.MaterialExpressionVectorParameter, -1000, -60 + i * 140, parameter_name=name, default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0)), code, name)
    link(node(m, unreal.MaterialExpressionScalarParameter, -1000, 400, parameter_name="Brightness", default_value=1.0), code, "Brightness")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("saved " + str(lib.save_asset(path, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
