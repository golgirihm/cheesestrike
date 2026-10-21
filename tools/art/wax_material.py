import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wax_material.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/wax_material.txt.
#
# Builds M_CheeseWax: cheese in a rind of red wax, glossy and smooth, with no
# holes. A map gives it to the cheese the mice leave alone (its walkable way
# through, what its starts stand on; tools/art/map_kit.py), so that players
# can tell what will always be there (docs/unreal/maps/principles.md). Its
# colour is the parameter Wax. No C++ loads it, so it can be rebuilt.
OUT = unreal.Paths.project_saved_dir() + "wax_material.txt"
FOLDER = "/Game/CheeseStrike/Materials"
NAME = "M_CheeseWax"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary

try:
    path = FOLDER + "/" + NAME
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_nanite", True)

    def node(cls, x, y, **props):
        n = mel.create_material_expression(m, cls, x, y)
        for k, value in props.items():
            n.set_editor_property(k, value)
        return n

    def link(a, b, pin="", out=""):
        if not mel.connect_material_expressions(a, out, b, pin):
            lines.append(f"couldn't link {a.get_class().get_name()}.{out} -> {b.get_class().get_name()}.{pin}")

    # The wax: its colour, mottled a little darker where it pooled, as dipped wax is.
    wax = node(unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Wax", default_value=unreal.LinearColor(0.42, 0.025, 0.02, 1.0))
    mottle = node(unreal.MaterialExpressionNoise, -900, 200, scale=0.01, levels=4, output_min=0.0, output_max=1.0, quality=1)
    darker = node(unreal.MaterialExpressionMultiply, -650, 120, const_b=0.72)
    link(wax, darker, "A")
    colour = node(unreal.MaterialExpressionLinearInterpolate, -400, 0)
    link(wax, colour, "A")
    link(darker, colour, "B")
    link(mottle, colour, "Alpha")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Glossy where smooth, duller where it pooled.
    rough = node(unreal.MaterialExpressionLinearInterpolate, -400, 300)
    link(node(unreal.MaterialExpressionConstant, -650, 300, r=0.22), rough, "A")
    link(node(unreal.MaterialExpressionConstant, -650, 380, r=0.4), rough, "B")
    link(mottle, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -400, 480, r=0.6), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    lines.append("saved " + str(lib.save_asset(path, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
