import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/fondue_material.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/fondue_material.txt.
#
# Builds the materials of The Fondue Floor's vat (tools/art/build_fondue.py):
# M_Fondue, molten cheese that glows, slowly swirling, with bubbles that swell
# and pop, each in its own time (drawn in world space, so any size of surface
# shows them at their real size); and M_Steam, soft wisps rising and fading,
# for the columns of steam over the vat. Their glow is the parameter Heat,
# their colours Hot, Skin and Glow. No C++ loads them, so they can be rebuilt.
OUT = unreal.Paths.project_saved_dir() + "fondue_material.txt"
FOLDER = "/Game/CheeseStrike/Materials"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary

FONDUE = r"""
float t = Time;
float2 p = Pos.xy;
// The slow swirl of the molten cheese: wandering waves, 0 (cooler, a skin forming) to 1 (hot), and finer ripples.
float2 q = p / 350.0;
float flow = sin(q.x * 1.7 + t * 0.35 + sin(q.y * 1.3 - t * 0.2) * 1.5);
flow += sin(q.y * 2.1 - t * 0.27 + sin(q.x * 0.9 + t * 0.31) * 1.7);
flow += 0.5 * sin((q.x + q.y) * 3.3 + t * 0.6);
float2 r2 = p / 90.0;
flow += 0.35 * sin(r2.x * 1.3 + sin(r2.y * 1.7 + t * 0.9) * 2.0 - t * 0.5);
flow = saturate(flow * 0.22 + 0.5);
// Bubbles: one in each 80 cm cell, swelling and popping, each in its own time and size. A bubble's dome is skin
// stretched thin and cooler; its rim, and the ring it leaves as it pops, glow hot.
const float S = 80.0;
float2 c = floor(p / S);
float dome = 0.0;
float rim = 0.0;
float2 tilt = float2(0.0, 0.0);
for (int x = -1; x <= 1; x++)
{
    for (int y = -1; y <= 1; y++)
    {
        float2 cell = c + float2(x, y);
        float3 h = frac(sin(float3(dot(cell, float2(127.1, 311.7)), dot(cell, float2(269.5, 183.3)), dot(cell, float2(419.2, 371.9)))) * 43758.5453);
        float life = frac(t * (0.18 + 0.3 * h.z) + h.x * 7.0);
        float2 centre = (cell + 0.25 + 0.5 * h.xy) * S;
        float r = S * (0.1 + 0.28 * h.y) * smoothstep(0.0, 0.85, life);
        float2 d = p - centre;
        float dist = length(d);
        if (life < 0.9 && dist < r)
        {
            float k = dist / max(r, 1.0);
            float z = sqrt(saturate(1.0 - k * k));
            if (z > dome)
            {
                dome = z;
                tilt = (d / max(r, 1.0)) / max(z, 0.25);
            }
            rim = max(rim, smoothstep(0.65, 1.0, k) * smoothstep(0.0, 0.3, life));
        }
        else if (life >= 0.9)
        {
            float a = (life - 0.9) / 0.1;
            rim = max(rim, saturate(1.0 - abs(dist - r * (1.0 + 0.8 * a)) / (5.0 + 8.0 * a)) * (1.0 - a));
        }
    }
}
BubbleNormal = normalize(float3(tilt * 0.45, 1.0));
Glow = (0.12 + 0.9 * flow * flow + 1.4 * rim) * (1.0 - 0.65 * dome);
return lerp(Skin, Hot, flow) * (1.0 - 0.35 * dome);
"""

STEAM = r"""
// A wisp: soft noise drifting up the column, fading out at its foot and its top and at its sides as seen.
float t = Time;
float3 p = Pos;
float up = saturate((p.z - Bottom) / max(Height, 1.0));
float2 q = float2(p.x + p.y, p.z - t * 60.0) / 90.0;
float n = sin(q.x * 2.3 + sin(q.y * 1.7 + t * 0.4) * 1.3) * 0.5 + 0.5;
n *= sin(q.y * 1.9 + sin(q.x * 1.1 - t * 0.3) * 1.6) * 0.5 + 0.5;
float side = pow(saturate(abs(dot(normalize(Cam - p), Normal))), 1.5);
return saturate(n * 1.6) * smoothstep(0.0, 0.15, up) * (1.0 - smoothstep(0.45, 1.0, up)) * side;
"""


def make(name):
    path = FOLDER + "/" + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    return m, path


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
    # ---------------------------------------------------------------- the fondue
    m, path = make("M_Fondue")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    world = node(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    time = node(m, unreal.MaterialExpressionTime, -1600, 120)
    # Its colours: hot yellow-orange where it flows, a darker skin where it cools; its glow, by Heat.
    hot = node(m, unreal.MaterialExpressionVectorParameter, -1600, 240, parameter_name="Hot", default_value=unreal.LinearColor(1.0, 0.52, 0.05, 1.0))
    skin = node(m, unreal.MaterialExpressionVectorParameter, -1600, 400, parameter_name="Skin", default_value=unreal.LinearColor(0.32, 0.11, 0.02, 1.0))
    code = node(m, unreal.MaterialExpressionCustom, -1300, 0, code=FONDUE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Molten fondue")
    code.set_editor_property("inputs", [custom_input("Pos"), custom_input("Time"), custom_input("Hot"), custom_input("Skin")])
    code.set_editor_property("additional_outputs", [custom_output("Glow", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                                     custom_output("BubbleNormal", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    mel.recompile_material(m)  # so the node's pins exist before linking
    link(world, code, "Pos")
    link(time, code, "Time")
    link(hot, code, "Hot")
    link(skin, code, "Skin")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(m, unreal.MaterialExpressionVectorParameter, -1000, 500, parameter_name="Glow", default_value=unreal.LinearColor(1.0, 0.36, 0.03, 1.0))
    heat = node(m, unreal.MaterialExpressionScalarParameter, -1000, 620, parameter_name="Heat", default_value=2.5)
    scaled = node(m, unreal.MaterialExpressionMultiply, -800, 560)
    link(glow, scaled, "A")
    link(heat, scaled, "B")
    emissive = node(m, unreal.MaterialExpressionMultiply, -600, 600)
    link(scaled, emissive, "A")
    link(code, emissive, "B", "Glow")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(code, "BubbleNormal", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -600, 800, r=0.15), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -600, 880, r=0.6), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    lines.append("fondue saved " + str(lib.save_asset(path, False)))

    # ---------------------------------------------------------------- the steam
    m, path = make("M_Steam")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    world = node(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    time = node(m, unreal.MaterialExpressionTime, -1600, 100)
    camera = node(m, unreal.MaterialExpressionCameraPositionWS, -1600, 200)
    normal = node(m, unreal.MaterialExpressionVertexNormalWS, -1600, 300)
    # The column's foot and height: the object's own bounds, so any column of any size fades right.
    position = node(m, unreal.MaterialExpressionObjectPositionWS, -1900, 400)
    bounds = node(m, unreal.MaterialExpressionObjectBounds, -1900, 520)
    foot_of = node(m, unreal.MaterialExpressionComponentMask, -1700, 400, r=False, g=False, b=True, a=False)
    link(position, foot_of)
    half_of = node(m, unreal.MaterialExpressionComponentMask, -1700, 520, r=False, g=False, b=True, a=False)
    link(bounds, half_of)
    half = node(m, unreal.MaterialExpressionMultiply, -1500, 520, const_b=0.5)
    link(half_of, half, "A")
    foot = node(m, unreal.MaterialExpressionSubtract, -1350, 420)
    link(foot_of, foot, "A")
    link(half, foot, "B")
    code = node(m, unreal.MaterialExpressionCustom, -1100, 0, code=STEAM, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Steam wisps")
    code.set_editor_property("inputs", [custom_input("Pos"), custom_input("Time"), custom_input("Cam"), custom_input("Normal"), custom_input("Bottom"), custom_input("Height")])
    mel.recompile_material(m)
    link(world, code, "Pos")
    link(time, code, "Time")
    link(camera, code, "Cam")
    link(normal, code, "Normal")
    link(foot, code, "Bottom")
    link(half_of, code, "Height")
    # Soft and pale, lit a little warm from the fondue below; never solid.
    strength = node(m, unreal.MaterialExpressionScalarParameter, -900, 300, parameter_name="Thickness", default_value=0.22)
    opacity = node(m, unreal.MaterialExpressionMultiply, -700, 200)
    link(code, opacity, "A")
    link(strength, opacity, "B")
    fade = node(m, unreal.MaterialExpressionDepthFade, -500, 200, fade_distance_default=80.0)
    link(opacity, fade, "Opacity")
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    tint = node(m, unreal.MaterialExpressionVectorParameter, -700, -100, parameter_name="Tint", default_value=unreal.LinearColor(1.0, 0.9, 0.75, 1.0))
    bright = node(m, unreal.MaterialExpressionMultiply, -500, -100, const_b=1.4)
    link(tint, bright, "A")
    mel.connect_material_property(bright, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("steam saved " + str(lib.save_asset(path, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
