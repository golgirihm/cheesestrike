import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_materials.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/build_materials.txt.

# Builds the maps' materials besides the cheese, all drawn from world space
# so that a block of any size reads at the right scale, with no textures:
#   M_Concrete: grey, mottled, with panel seams every 4 m;
#   M_PaintedMetal: a colour (parameter "Colour"), grimier towards the
#     ground, corrugated on upright faces (parameter "Ribs", 0 or 1), and
#     instances of it in the colours the maps use;
#   M_Steel: bare steel, for racks, posts and railings.
OUT = unreal.Paths.project_saved_dir() + "build_materials.txt"
FOLDER = "/Game/CheeseStrike/Materials"
SEAMS = r"""
float3 f = abs(frac(Pos / 400.0 + 0.5) - 0.5) * 400.0;
float3 n = abs(Normal);
float seam = 0.0;
seam = max(seam, (1.0 - smoothstep(0.8, 2.0, f.x)) * step(n.x, 0.5));
seam = max(seam, (1.0 - smoothstep(0.8, 2.0, f.y)) * step(n.y, 0.5));
seam = max(seam, (1.0 - smoothstep(0.8, 2.0, f.z)) * step(n.z, 0.5));
return seam;
"""
# Corrugation: on an upright face, ribs every 25 cm across it, as a normal (world space).
RIBS = r"""
if (Ribs < 0.5 || abs(Normal.z) > 0.5)
{
    return Normal;
}
float3 across = normalize(cross(float3(0, 0, 1), Normal));
float u = dot(Pos, across);
float slope = cos(u * 6.2831853 / 25.0) * 0.45;
return normalize(Normal + across * slope);
"""
# The colours, and whether each is corrugated.
COLOURS = {
    "Cream": ((0.55, 0.52, 0.45), 1.0),
    "Teal": ((0.07, 0.24, 0.25), 1.0),
    "Red": ((0.36, 0.06, 0.05), 1.0),
    "Blue": ((0.06, 0.13, 0.30), 1.0),
    "Yellow": ((0.62, 0.42, 0.04), 0.0),
    "Dark": ((0.06, 0.06, 0.065), 0.0),
    "Grey": ((0.30, 0.31, 0.32), 1.0),
}
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def material(name):
    path = FOLDER + "/" + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_nanite", True)
    return m


def builder(m):
    def node(cls, x, y, **props):
        n = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, b, pin="", out=""):
        if not mel.connect_material_expressions(a, out, b, pin):
            lines.append(f"{m.get_name()}: couldn't link {a.get_class().get_name()}.{out} -> {b.get_class().get_name()}.{pin}")
    return node, link


def lerp3(node, link, a, b, alpha, x, y):
    l = node(unreal.MaterialExpressionLinearInterpolate, x, y)
    link(node(unreal.MaterialExpressionConstant3Vector, x - 200, y - 50, constant=unreal.LinearColor(*a, 1.0)), l, "A")
    link(node(unreal.MaterialExpressionConstant3Vector, x - 200, y + 50, constant=unreal.LinearColor(*b, 1.0)), l, "B")
    link(alpha, l, "Alpha")
    return l


def concrete():
    m = material("M_Concrete")
    node, link = builder(m)
    big = node(unreal.MaterialExpressionNoise, -900, 0, scale=0.0007, levels=3, output_min=0.0, output_max=1.0, quality=1)
    grain = node(unreal.MaterialExpressionNoise, -900, 200, scale=0.08, levels=2, output_min=0.0, output_max=1.0, quality=1)
    tone = lerp3(node, link, (0.34, 0.34, 0.335), (0.40, 0.40, 0.39), big, -600, 0)
    speck = node(unreal.MaterialExpressionLinearInterpolate, -400, 100)
    link(node(unreal.MaterialExpressionConstant, -550, 160, r=0.92), speck, "A")
    link(node(unreal.MaterialExpressionConstant, -550, 220, r=1.05), speck, "B")
    link(grain, speck, "Alpha")
    shaded = node(unreal.MaterialExpressionMultiply, -250, 50)
    link(tone, shaded, "A")
    link(speck, shaded, "B")
    seams = node(unreal.MaterialExpressionCustom, -600, 400, code=SEAMS, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Panel seams")
    inputs = []
    for name in ("Pos", "Normal"):
        i = unreal.CustomInput()
        i.set_editor_property("input_name", name)
        inputs.append(i)
    seams.set_editor_property("inputs", inputs)
    mel.recompile_material(m)
    link(node(unreal.MaterialExpressionWorldPosition, -850, 400), seams, "Pos")
    link(node(unreal.MaterialExpressionVertexNormalWS, -850, 500), seams, "Normal")
    darken = node(unreal.MaterialExpressionLinearInterpolate, -100, 200)
    link(node(unreal.MaterialExpressionConstant, -250, 260, r=1.0), darken, "A")
    link(node(unreal.MaterialExpressionConstant, -250, 320, r=0.45), darken, "B")
    link(seams, darken, "Alpha")
    colour = node(unreal.MaterialExpressionMultiply, 100, 100)
    link(shaded, colour, "A")
    link(darken, colour, "B")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(unreal.MaterialExpressionLinearInterpolate, 100, 300)
    link(node(unreal.MaterialExpressionConstant, -100, 400, r=0.78), rough, "A")
    link(node(unreal.MaterialExpressionConstant, -100, 460, r=0.92), rough, "B")
    link(grain, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append("concrete saved " + str(lib.save_asset(FOLDER + "/M_Concrete", False)))


def painted_metal():
    m = material("M_PaintedMetal")
    node, link = builder(m)
    paint = node(unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Colour", default_value=unreal.LinearColor(0.6, 0.6, 0.6, 1.0))
    big = node(unreal.MaterialExpressionNoise, -900, 200, scale=0.003, levels=3, output_min=0.85, output_max=1.05, quality=1)
    varied = node(unreal.MaterialExpressionMultiply, -650, 100)
    link(paint, varied, "A")
    link(big, varied, "B")
    # Grimier towards the ground: the bottom metre darkens.
    world = node(unreal.MaterialExpressionWorldPosition, -1100, 400)
    height = node(unreal.MaterialExpressionComponentMask, -900, 400, r=False, g=False, b=True, a=False)
    link(world, height)
    up = node(unreal.MaterialExpressionDivide, -750, 400, const_b=100.0)
    link(height, up, "A")
    clean = node(unreal.MaterialExpressionSaturate, -600, 400)
    link(up, clean)
    grime = node(unreal.MaterialExpressionLinearInterpolate, -450, 350)
    link(node(unreal.MaterialExpressionConstant, -600, 300, r=0.62), grime, "A")
    link(node(unreal.MaterialExpressionConstant, -600, 340, r=1.0), grime, "B")
    link(clean, grime, "Alpha")
    colour = node(unreal.MaterialExpressionMultiply, -250, 150)
    link(varied, colour, "A")
    link(grime, colour, "B")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(unreal.MaterialExpressionLinearInterpolate, -250, 400)
    link(node(unreal.MaterialExpressionConstant, -450, 450, r=0.75), rough, "A")
    link(node(unreal.MaterialExpressionConstant, -450, 500, r=0.45), rough, "B")
    link(clean, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -250, 600, r=0.15), "", unreal.MaterialProperty.MP_METALLIC)
    m.set_editor_property("tangent_space_normal", False)
    ribs = node(unreal.MaterialExpressionCustom, -500, 800, code=RIBS, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Corrugation")
    inputs = []
    for name in ("Pos", "Normal", "Ribs"):
        i = unreal.CustomInput()
        i.set_editor_property("input_name", name)
        inputs.append(i)
    ribs.set_editor_property("inputs", inputs)
    mel.recompile_material(m)
    link(world, ribs, "Pos")
    link(node(unreal.MaterialExpressionVertexNormalWS, -800, 850), ribs, "Normal")
    link(node(unreal.MaterialExpressionScalarParameter, -800, 950, parameter_name="Ribs", default_value=0.0), ribs, "Ribs")
    mel.connect_material_property(ribs, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    lines.append("painted metal saved " + str(lib.save_asset(FOLDER + "/M_PaintedMetal", False)))
    for name, (rgb, corrugated) in COLOURS.items():
        path = FOLDER + "/MI_Metal_" + name
        mi = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset("MI_Metal_" + name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, m)
        mel.set_material_instance_vector_parameter_value(mi, "Colour", unreal.LinearColor(*rgb, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Ribs", corrugated)
        lines.append(f"{name} saved " + str(lib.save_asset(path, False)))


def steel():
    m = material("M_Steel")
    node, link = builder(m)
    big = node(unreal.MaterialExpressionNoise, -700, 0, scale=0.002, levels=2, output_min=0.0, output_max=1.0, quality=1)
    tone = lerp3(node, link, (0.52, 0.53, 0.55), (0.58, 0.59, 0.61), big, -400, 0)
    mel.connect_material_property(tone, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -300, 200, r=0.9), "", unreal.MaterialProperty.MP_METALLIC)
    rough = node(unreal.MaterialExpressionLinearInterpolate, -200, 300)
    link(node(unreal.MaterialExpressionConstant, -400, 300, r=0.3), rough, "A")
    link(node(unreal.MaterialExpressionConstant, -400, 360, r=0.38), rough, "B")
    link(big, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append("steel saved " + str(lib.save_asset(FOLDER + "/M_Steel", False)))


try:
    concrete()
    painted_metal()
    steel()
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
