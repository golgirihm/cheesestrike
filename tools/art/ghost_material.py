import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/ghost_material.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/ghost_material.txt.
#
# Builds M_Ghost, the look of a player protected just after coming in
# (CheeseStrikeStart): laid over the body, the first-person arms and the
# weapons as an overlay material (UMeshComponent::SetOverlayMaterial), so it
# works on every hero's materials without touching them. Unlit and
# translucent: a pale wash over the whole body, brighter at its rim (where
# the surface turns away from the view), with soft bands rising up it and a
# faint flicker. Its parameters: Amount (0 to 1, which the character fades
# as the protection ends), Colour, and how strong the wash and the rim are
# (Wash, RimGlow, Cover, RimCover; the first-person arms and weapon, seen up
# close and mostly edge-on, take a softer rim). The character loads it by path
# (ACheeseStrikeCharacter::UpdateGhost), so it can be rebuilt.
OUT = unreal.Paths.project_saved_dir() + "ghost_material.txt"
FOLDER = "/Game/CheeseStrike/Effects"
NAME = "M_Ghost"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary

GHOST = r"""
float3 V = normalize(Cam - Pos);
float facing = saturate(abs(dot(normalize(N), V)));
float rim = pow(1.0 - facing, 2.5);
// Bands rising up the body, and a faint flicker across it.
float bands = 0.5 + 0.5 * sin(Pos.z * 0.08 - Time * 4.0);
float flicker = 0.88 + 0.12 * sin(Time * 13.0 + (Pos.x + Pos.y) * 0.05);
float shimmer = lerp(0.72, 1.0, bands) * flicker;
Glow = Colour * (Wash + RimGlow * rim) * shimmer * Amount;
return saturate((Cover + RimCover * rim) * shimmer) * Amount;
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
    # A cooked game compiles only the usages it was told of.
    m.set_editor_property("used_with_skeletal_mesh", True)
    code = node(m, unreal.MaterialExpressionCustom, -500, 0, code=GHOST, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Ghost")
    names = ["Pos", "Cam", "N", "Time", "Colour", "Amount", "Wash", "RimGlow", "Cover", "RimCover"]
    code.set_editor_property("inputs", [custom_input(n) for n in names])
    code.set_editor_property("additional_outputs", [custom_output("Glow", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    mel.recompile_material(m)  # so the node's pins exist before linking
    link(node(m, unreal.MaterialExpressionWorldPosition, -1000, -200), code, "Pos")
    link(node(m, unreal.MaterialExpressionCameraPositionWS, -1000, -100), code, "Cam")
    link(node(m, unreal.MaterialExpressionVertexNormalWS, -1000, 0), code, "N")
    link(node(m, unreal.MaterialExpressionTime, -1000, 100), code, "Time")
    link(node(m, unreal.MaterialExpressionVectorParameter, -1000, 200, parameter_name="Colour", default_value=unreal.LinearColor(0.62, 0.86, 1.0, 1.0)), code, "Colour")
    params = {"Amount": 1.0, "Wash": 0.16, "RimGlow": 1.3, "Cover": 0.2, "RimCover": 0.6}
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
