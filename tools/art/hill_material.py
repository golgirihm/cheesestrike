import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/hill_material.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/hill_material.txt.
#
# Builds M_Hill, the look of King of the Hill's hill (ACheeseStrikeHill):
# unlit, translucent and glowing, in the colour the hill sets for whoever
# looks (Colour: the cheese's pale yellow, a side's colour, flickering white
# when contested). On the engine's plane, laid on the floor (Column 0), it is
# a bright ring at the plane's rim, a band of dashes turning slowly inside it
# and a faint wash over the ground within; on the engine's cylinder (Column 1)
# it is a wall of light rising from the ring and fading out as it goes up,
# with soft bands climbing it, open at the top and the bottom (the caps drawn
# clear). Both are worked out from where the pixel is in the object's bounds,
# so the meshes may be scaled to any hill. Strength dims it (the next hill's
# ring, while the hill warns that it is moving). The hill loads it by path as
# it comes into play, so that this can rebuild it.
OUT = unreal.Paths.project_saved_dir() + "hill_material.txt"
FOLDER = "/Game/CheeseStrike/Effects"
NAME = "M_Hill"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary

HILL = r"""
float3 d = Pos - Obj;
float glow;
float cover;
if (Column < 0.5)
{
    // The ring on the floor: r is 0 in the middle, 1 at the rim (the bounds are half the object's size).
    float r = length(d.xy) / max(1.0, Bounds.x);
    float rim = smoothstep(0.86, 0.95, r) * (1.0 - smoothstep(0.985, 1.0, r));
    float angle = atan2(d.y, d.x) / 6.2831853;
    float dashes = step(0.5, frac(angle * 28.0 + Time * 0.08)) * smoothstep(0.74, 0.77, r) * (1.0 - smoothstep(0.8, 0.83, r));
    float inside = (1.0 - smoothstep(0.95, 1.0, r)) * (0.07 + 0.04 * sin(Time * 2.0 - r * 9.0));
    glow = rim * 3.0 + dashes * 1.6 + inside * 1.2;
    cover = saturate(rim * 0.95 + dashes * 0.7 + inside);
}
else
{
    // The wall of light: h is 0 at the floor, 1 at the top; no caps.
    float h = saturate(d.z / max(1.0, Bounds.z * 2.0) + 0.5);
    float fade = pow(1.0 - h, 2.2);
    float bands = 0.6 + 0.4 * sin(Pos.z * 0.035 - Time * 2.5);
    float side = 1.0 - step(0.5, abs(N.z));
    glow = fade * bands * 1.0 * side;
    cover = saturate(fade * bands * 0.36) * side;
}
Glow = Colour * glow * Strength;
return cover * Strength;
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


try:
    path = FOLDER + "/" + NAME
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    code = node(m, unreal.MaterialExpressionCustom, -500, 0, code=HILL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Hill")
    names = ["Pos", "Obj", "Bounds", "N", "Time", "Colour", "Column", "Strength"]
    code.set_editor_property("inputs", [custom_input(n) for n in names])
    code.set_editor_property("additional_outputs", [custom_output("Glow", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    mel.recompile_material(m)  # so the node's pins exist before linking
    link(node(m, unreal.MaterialExpressionWorldPosition, -1000, -300), code, "Pos")
    link(node(m, unreal.MaterialExpressionObjectPositionWS, -1000, -200), code, "Obj")
    link(node(m, unreal.MaterialExpressionObjectBounds, -1000, -100), code, "Bounds")
    link(node(m, unreal.MaterialExpressionVertexNormalWS, -1000, 0), code, "N")
    link(node(m, unreal.MaterialExpressionTime, -1000, 100), code, "Time")
    link(node(m, unreal.MaterialExpressionVectorParameter, -1000, 200, parameter_name="Colour", default_value=unreal.LinearColor(1.0, 0.7, 0.12, 1.0)), code, "Colour")
    params = {"Column": 0.0, "Strength": 1.0}
    for i, (name, value) in enumerate(params.items()):
        link(node(m, unreal.MaterialExpressionScalarParameter, -1000, 360 + i * 80, parameter_name=name, default_value=value), code, name)
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(code, "Glow", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("saved " + str(lib.save_asset(path, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
