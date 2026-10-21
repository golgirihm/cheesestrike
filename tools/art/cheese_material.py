import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/cheese_material.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/cheese_material.txt.

# Rebuilds M_Cheese with carved holes. The holes are spheres in world space
# (one in most 42 cm cells); a pixel of the surface inside one follows the
# view ray to the sphere's far wall and draws that wall: its normal (the
# cavity's, facing its middle), shading darker with depth. The wall moves
# with the view as a real hollow's does. As the mice eat a block (its stage,
# 0 to 3, in its mesh's custom primitive data 0; ACheeseStrikeCheese), its
# holes grow and more open. (A pixel depth offset to the wall
# would make the sun's shadows treat the flat surface as a lid over every
# hole and black some of them out, so the depth buffer keeps the surface.)
OUT = unreal.Paths.project_saved_dir() + "cheese_material.txt"
CODE = r"""
if (Holes < 0.5)
{
    InnerNormal = Normal;
    Shade = 0.0;
    return 0.0;
}
const float S = 42.0;
float3 p = Pos / S;
float3 d = normalize(Pos - Cam);
float3 c = floor(p);
float best = 10.0;
float3 centre = float3(0, 0, 0);
float radius = 0.0;
for (int x = -1; x <= 1; x++)
{
    for (int y = -1; y <= 1; y++)
    {
        for (int z = -1; z <= 1; z++)
        {
            float3 cell = c + float3(x, y, z);
            float3 h = frac(sin(float3(dot(cell, float3(127.1, 311.7, 74.7)), dot(cell, float3(269.5, 183.3, 246.1)), dot(cell, float3(113.5, 271.9, 124.6)))) * 43758.5453);
            // Eaten (0 to 3), the holes grow and more of them open.
            float keep = step(0.12 - 0.035 * Eaten, frac(h.x * 7.31 + h.y * 3.17 + h.z * 1.93));
            float r = lerp(0.16, 0.46, h.z) * (1.0 + 0.3 * Eaten);
            float3 at = cell + 0.2 + 0.6 * h;
            float dist = length(p - at) - r;
            if (keep > 0.5 && dist < best)
            {
                best = dist;
                centre = at;
                radius = r;
            }
        }
    }
}
InnerNormal = Normal;
Shade = 0.0;
if (best < 0.0)
{
    float3 o = p - centre;
    float b = dot(o, d);
    float cc = dot(o, o) - radius * radius;
    float t = -b + sqrt(max(b * b - cc, 0.0));
    float3 hit = o + t * d;
    // Leaned a little towards the surface's own normal: real cheese's hollows are never black (light bounces
    // round them and passes through the wax), and the global lighting sees only the flat surface.
    InnerNormal = normalize(lerp(normalize(-hit), Normal, 0.3));
    Shade = saturate(t / (2.0 * radius));
    return 1.0;
}
return 0.0;
"""
lines = []
mel = unreal.MaterialEditingLibrary
try:
    m = unreal.load_asset("/Game/CheeseStrike/Materials/M_Cheese")
    mel.delete_all_material_expressions(m)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("used_with_nanite", True)
    # The normal given is in world space: the cavity's, or the surface's own.
    m.set_editor_property("tangent_space_normal", False)

    def node(cls, x, y, **props):
        n = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
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

    world = node(unreal.MaterialExpressionWorldPosition, -1500, 0)
    camera = node(unreal.MaterialExpressionCameraPositionWS, -1500, 100)
    normal = node(unreal.MaterialExpressionVertexNormalWS, -1500, 200)
    holes = node(unreal.MaterialExpressionCustom, -1200, 0, code=CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Carved cheese holes")
    holes.set_editor_property("inputs", [custom_input("Pos"), custom_input("Cam"), custom_input("Normal"), custom_input("Holes"), custom_input("Eaten")])
    holes.set_editor_property("additional_outputs", [
        custom_output("InnerNormal", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
        custom_output("Shade", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
    ])
    mel.recompile_material(m)  # so the node's pins exist before linking
    link(world, holes, "Pos")
    link(camera, holes, "Cam")
    link(normal, holes, "Normal")
    # Thin pieces (a slab on the floor) have none: carving them would look like holes in paper (MI_CheeseThin).
    holes_on = node(unreal.MaterialExpressionScalarParameter, -1500, 300, parameter_name="Holes", default_value=1.0)
    link(holes_on, holes, "Holes")
    eaten = node(unreal.MaterialExpressionCustomPrimitiveData, -1500, 400, primitive_data_index=0)
    link(eaten, holes, "Eaten")

    mottle = node(unreal.MaterialExpressionNoise, -1000, 400, scale=0.003, levels=3, output_min=0.0, output_max=1.0, quality=1)
    pale = node(unreal.MaterialExpressionConstant3Vector, -800, 360, constant=unreal.LinearColor(0.93, 0.68, 0.20, 1.0))
    deep = node(unreal.MaterialExpressionConstant3Vector, -800, 460, constant=unreal.LinearColor(0.85, 0.53, 0.11, 1.0))
    cheese = node(unreal.MaterialExpressionLinearInterpolate, -550, 400)
    link(pale, cheese, "A")
    link(deep, cheese, "B")
    link(mottle, cheese, "Alpha")
    # Inside, the same cheese, a little deeper in colour towards the bottom.
    darker = node(unreal.MaterialExpressionMultiply, -550, 560)
    link(cheese, darker, "A")
    tint = node(unreal.MaterialExpressionLinearInterpolate, -750, 600)
    link(node(unreal.MaterialExpressionConstant3Vector, -950, 560, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)), tint, "A")
    link(node(unreal.MaterialExpressionConstant3Vector, -950, 660, constant=unreal.LinearColor(0.9, 0.82, 0.72, 1.0)), tint, "B")
    link(holes, tint, "Alpha", "Shade")
    link(tint, darker, "B")
    colour = node(unreal.MaterialExpressionLinearInterpolate, -300, 300)
    link(cheese, colour, "A")
    link(darker, colour, "B")
    link(holes, colour, "Alpha")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)

    mel.connect_material_property(holes, "InnerNormal", unreal.MaterialProperty.MP_NORMAL)

    rough = node(unreal.MaterialExpressionLinearInterpolate, -300, 800)
    link(node(unreal.MaterialExpressionConstant, -500, 800, r=0.45), rough, "A")
    link(node(unreal.MaterialExpressionConstant, -500, 880, r=0.6), rough, "B")
    link(holes, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Deeper in a hole, less of the sky reaches.
    dim = node(unreal.MaterialExpressionMultiply, -450, 1000, const_b=0.35)
    link(holes, dim, "A", "Shade")
    occlusion = node(unreal.MaterialExpressionOneMinus, -300, 1000)
    link(dim, occlusion)
    mel.connect_material_property(occlusion, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -300, 1100, r=0.6), "", unreal.MaterialProperty.MP_SPECULAR)

    mel.recompile_material(m)
    lines.append("saved " + str(unreal.EditorAssetLibrary.save_asset("/Game/CheeseStrike/Materials/M_Cheese", False)))
    # The instance for thin pieces, with no holes.
    thin_path = "/Game/CheeseStrike/Materials/MI_CheeseThin"
    if unreal.EditorAssetLibrary.does_asset_exist(thin_path):
        thin = unreal.load_asset(thin_path)
    else:
        thin = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_CheeseThin", "/Game/CheeseStrike/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(thin, m)
    mel.set_material_instance_scalar_parameter_value(thin, "Holes", 0.0)
    lines.append("thin saved " + str(unreal.EditorAssetLibrary.save_asset(thin_path, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
