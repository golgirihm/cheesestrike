import sys, traceback
import unreal

# Run with the editor closed, after tools/art/import_weapons.py:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/weapon_materials.py" -unattended -nullrhi
# (both paths absolute). It writes what it did to Saved/weapon_materials.txt.
#
# Builds the weapons' materials in /Game/CheeseStrike/Weapons/Materials/: M_CheeseWeapon, which draws a weapon from
# its own maps (colour, normal, AORM) and turns parts of it into cheese, and an instance for each weapon's every slot;
# M_MuzzleFlash with its two instances, and M_Tracer, a shot's streak of light. The weapons load them by path
# (CheeseStrikeWeaponLooks), never in a constructor, so this can rebuild them.
#
# What turns to cheese: a channel of the weapon's mask, a box in the model's own space, or (the cheese weapons', which
# have no maps) a whole slot. The cheese is mottled, holed (Swiss, cut right through within a box), veined (Roquefort),
# or rind and wax (all of a slot: the knife's handle), or plain (steel, black, the Blaster's wrapper). Everything is in
# the model's own space, unscaled, so it moves with the weapon, skinned or not. (The instances for the Fab and Lyra guns
# the older looks borrowed were deleted in wave 15, tools/art/delete_unused_weapons.py.)
#
# With instance names after the script ("py weapon_materials.py MI_Burratas MI_BurratasCream"), only those are made,
# over the M_CheeseWeapon already there, and nothing else is touched; "M_Tracer" among them makes the tracer's too.
MATERIALS = "/Game/CheeseStrike/Weapons/Materials"
OUT = unreal.Paths.project_saved_dir() + "weapon_materials.txt"
ONLY = [a for a in sys.argv[1:] if a]

CODE = r"""
float3 p = LocalPos;
// What is cheese here, what is rind, what is lettering.
float cheese = saturate(dot(Mask.rgb, CheeseChannel.rgb) + CheeseAll);
float3 ina = step(BoxAMin.xyz, p) * step(p, BoxAMax.xyz);
float3 inb = step(BoxBMin.xyz, p) * step(p, BoxBMax.xyz);
cheese = max(cheese, max(ina.x * ina.y * ina.z, inb.x * inb.y * inb.z));
float rind = saturate(dot(Mask.rgb, RindChannel.rgb) + RindAll);
float text = saturate(dot(Mask.rgb, TextChannel.rgb));
// Value noise in the model's space, for mottling and veins.
float n1 = 0.0;
float n2 = 0.0;
for (int octave = 0; octave < 2; octave++)
{
    float3 q = p / max(octave == 0 ? MottleScale : VeinScale, 0.01);
    float3 c = floor(q);
    float3 f = frac(q);
    float3 u = f * f * (3.0 - 2.0 * f);
    float v[8];
    for (int k = 0; k < 8; k++)
    {
        float3 o = float3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
        v[k] = frac(sin(dot(c + o, float3(127.1, 311.7, 74.7))) * 43758.5453);
    }
    float n = lerp(lerp(lerp(v[0], v[1], u.x), lerp(v[2], v[3], u.x), u.y), lerp(lerp(v[4], v[5], u.x), lerp(v[6], v[7], u.x), u.y), u.z);
    if (octave == 0) n1 = n; else n2 = n;
}
float marble = saturate((n1 - 0.5) * Marble + 0.5);
float3 cheeseColour = lerp(CheeseColor.rgb, CheeseColorB.rgb, marble);
// Roquefort's veins: where the second noise passes through its middle.
float vein = (1.0 - smoothstep(0.0, VeinWidth, abs(n2 - 0.5))) * Veins;
cheeseColour = lerp(cheeseColour, VeinColor.rgb, vein);
// Swiss holes: spheres in cells of the model's space; inside one, the hollow, darker deeper in, or cut through.
float hole = 0.0;
float depth = 0.0;
if (Holes > 0.0 && HoleSize > 0.0)
{
    float S = HoleSize * 2.6;
    float3 hp = p / S;
    float3 hc = floor(hp);
    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++)
    {
        float3 cell = hc + float3(x, y, z);
        float3 h = frac(sin(float3(dot(cell, float3(127.1, 311.7, 74.7)), dot(cell, float3(269.5, 183.3, 246.1)), dot(cell, float3(113.5, 271.9, 124.6)))) * 43758.5453);
        if (h.x < Holes)
        {
            float r = lerp(0.22, 0.4, h.y);
            float d = length(hp - (cell + 0.25 + 0.5 * h)) - r;
            if (d < 0.0)
            {
                hole = 1.0;
                depth = max(depth, saturate(-d / r));
            }
        }
    }
}
hole *= cheese;
cheeseColour *= lerp(1.0, lerp(0.75, 0.42, depth), hole);
float3 inc = step(CutBoxMin.xyz, p) * step(p, CutBoxMax.xyz);
float cut = hole * CutHoles * inc.x * inc.y * inc.z;
// The rind: a washed or waxed coat, a little uneven.
float3 rindColour = RindColor.rgb * (0.85 + 0.3 * n1);
// The metal and plastic as the maps have them, or plain.
float3 base = UseTextures > 0.5 ? Albedo.rgb * MetalTint.rgb : PlainColor.rgb;
base = lerp(base, TextColor.rgb, text);
float3 colour = lerp(lerp(base, cheeseColour, cheese), rindColour, rind);
float roughness = UseTextures > 0.5 ? AORM.g : PlainRoughness;
roughness = lerp(lerp(roughness, CheeseRoughness + 0.15 * hole, cheese), RindRoughness, rind);
float metal = (UseTextures > 0.5 ? AORM.b : PlainMetallic) * (1.0 - cheese) * (1.0 - rind);
Roughness = roughness;
Metallic = metal;
Occlusion = (UseTextures > 0.5 ? lerp(1.0, AORM.r, AOStrength) : 1.0) * (1.0 - 0.5 * hole * depth);
Opacity = (1.0 - cut) * (Hide > 0.5 ? 0.0 : 1.0);
Flat = saturate(cheese + rind) * 0.7;
return colour;
"""

FLASH_CODE = r"""
float2 uv = UV;
float glow;
if (Radial > 0.5)
{
    float2 d = (uv - 0.5) * 2.0;
    float r = length(d);
    float a = atan2(d.y, d.x);
    glow = exp(-r * r * 7.0) + 0.5 * exp(-r * r * 2.5) * pow(abs(cos(a * 3.0)), 8.0) * saturate(1.0 - r);
}
else
{
    float u = uv.x;
    float width = 0.06 + 0.34 * (1.0 - u);
    glow = pow(saturate(1.0 - u), 1.5) * exp(-pow((uv.y - 0.5) / width, 2.0)) * smoothstep(0.0, 0.06, u);
}
return glow;
"""

lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def material(name):
    path = f"{MATERIALS}/{name}"
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())


def instance(name, parent):
    path = f"{MATERIALS}/{name}"
    if lib.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    return mi


def builder(m):
    def node(cls, x, y, **props):
        n = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, b, pin="", out=""):
        if not mel.connect_material_expressions(a, out, b, pin):
            lines.append(f"couldn't link {a.get_class().get_name()}.{out} -> {b.get_class().get_name()}.{pin}")
    return node, link


def custom_input(name):
    i = unreal.CustomInput()
    i.set_editor_property("input_name", name)
    return i


def custom_output(name, kind):
    o = unreal.CustomOutput()
    o.set_editor_property("output_name", name)
    o.set_editor_property("output_type", kind)
    return o


SCALARS = {"UseTextures": 1.0, "PlainRoughness": 0.3, "PlainMetallic": 0.0, "CheeseAll": 0.0, "RindAll": 0.0, "Holes": 0.0, "HoleSize": 1.0,
           "CutHoles": 0.0, "Veins": 0.0, "VeinScale": 1.5, "VeinWidth": 0.05, "MottleScale": 2.0, "Marble": 1.0, "CheeseRoughness": 0.55, "RindRoughness": 0.4, "AOStrength": 0.5, "Hide": 0.0}
FAR = 100000.0
VECTORS = {"PlainColor": (0.5, 0.5, 0.5, 1.0), "CheeseColor": (0.95, 0.75, 0.3, 1.0), "CheeseColorB": (0.88, 0.65, 0.22, 1.0), "VeinColor": (0.22, 0.36, 0.38, 1.0),
           "RindColor": (0.8, 0.35, 0.08, 1.0), "TextColor": (0.05, 0.05, 0.05, 1.0), "MetalTint": (1.0, 1.0, 1.0, 1.0),
           "CheeseChannel": (1.0, 0.0, 0.0, 0.0), "RindChannel": (0.0, 1.0, 0.0, 0.0), "TextChannel": (0.0, 0.0, 0.0, 0.0),
           "BoxAMin": (FAR, FAR, FAR, 0.0), "BoxAMax": (-FAR, -FAR, -FAR, 0.0), "BoxBMin": (FAR, FAR, FAR, 0.0), "BoxBMax": (-FAR, -FAR, -FAR, 0.0),
           "CutBoxMin": (-FAR, -FAR, -FAR, 0.0), "CutBoxMax": (FAR, FAR, FAR, 0.0)}


def no_mask():
    """A small black map, linear, for a weapon with no mask: nothing of it is cheese but what its boxes say."""
    import os, struct, zlib
    path = f"{MATERIALS}/T_NoMask"
    if lib.does_asset_exist(path):
        return unreal.load_asset(path)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    # Four rows of four black pixels, each row led by its filter byte: all zeros.
    rows = bytes(4 * 13)
    signature = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])
    png = signature + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    file = unreal.Paths.project_saved_dir() + "T_NoMask.png"
    with open(file, "wb") as f:
        f.write(png)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file)
    task.set_editor_property("destination_path", MATERIALS)
    task.set_editor_property("destination_name", "T_NoMask")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    tools.import_asset_tasks([task])
    t = unreal.load_asset(path)
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("srgb", False)
    lib.save_asset(path, False)
    return t


def build_weapon_material():
    m = material("M_CheeseWeapon")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_static_lighting", False)
    node, link = builder(m)
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    # The maps the masks sampler reads must be linear masks: the maps' white (made by import_fab.py) and a black of our own.
    linear_white = unreal.load_asset("/Game/CheeseStrike/Materials/T_White")
    black = no_mask()
    flat = unreal.load_asset("/Engine/EngineMaterials/FlatNormal")
    albedo = node(unreal.MaterialExpressionTextureSampleParameter2D, -1800, -400, parameter_name="Albedo", texture=white)
    aorm = node(unreal.MaterialExpressionTextureSampleParameter2D, -1800, -100, parameter_name="AORM", texture=linear_white, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mask = node(unreal.MaterialExpressionTextureSampleParameter2D, -1800, 200, parameter_name="Mask", texture=black, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    normal = node(unreal.MaterialExpressionTextureSampleParameter2D, -1800, 500, parameter_name="Normal", texture=flat, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    # The model's own space, before skinning: known to the vertices only, so carried across to the pixels.
    skinless = node(unreal.MaterialExpressionPreSkinnedPosition, -2000, 800)
    local = node(unreal.MaterialExpressionVertexInterpolator, -1800, 800)
    link(skinless, local, "VS")
    inputs = ["LocalPos", "Albedo", "AORM", "Mask"] + list(SCALARS) + list(VECTORS)
    custom = node(unreal.MaterialExpressionCustom, -1000, 0, code=CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Cheese weapon")
    custom.set_editor_property("inputs", [custom_input(name) for name in inputs])
    custom.set_editor_property("additional_outputs", [
        custom_output("Roughness", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
        custom_output("Metallic", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
        custom_output("Occlusion", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
        custom_output("Opacity", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
        custom_output("Flat", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
    ])
    mel.recompile_material(m)
    link(local, custom, "LocalPos", "PS")
    link(albedo, custom, "Albedo", "RGB")
    link(aorm, custom, "AORM", "RGB")
    link(mask, custom, "Mask", "RGBA")
    y = 1000
    for name, value in SCALARS.items():
        link(node(unreal.MaterialExpressionScalarParameter, -1500, y, parameter_name=name, default_value=value), custom, name)
        y += 60
    for name, value in VECTORS.items():
        link(node(unreal.MaterialExpressionVectorParameter, -1500, y, parameter_name=name, default_value=unreal.LinearColor(*value)), custom, name)
        y += 60
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(custom, "Roughness", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(custom, "Metallic", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(custom, "Occlusion", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(custom, "Opacity", unreal.MaterialProperty.MP_OPACITY_MASK)
    # The cheese and the rind are smoother than the machined metal under them: its normals flattened there.
    up = node(unreal.MaterialExpressionConstant3Vector, -700, 600, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    smooth = node(unreal.MaterialExpressionLinearInterpolate, -500, 500)
    link(normal, smooth, "A", "RGB")
    link(up, smooth, "B")
    link(custom, smooth, "Alpha", "Flat")
    mel.connect_material_property(smooth, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    lines.append("M_CheeseWeapon saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def build_flash_material():
    m = material("M_MuzzleFlash")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_skeletal_mesh", False)
    node, link = builder(m)
    uv = node(unreal.MaterialExpressionTextureCoordinate, -900, 0)
    radial = node(unreal.MaterialExpressionScalarParameter, -900, 100, parameter_name="Radial", default_value=0.0)
    glow = node(unreal.MaterialExpressionCustom, -600, 0, code=FLASH_CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Flash")
    glow.set_editor_property("inputs", [custom_input("UV"), custom_input("Radial")])
    mel.recompile_material(m)
    link(uv, glow, "UV")
    link(radial, glow, "Radial")
    colour = node(unreal.MaterialExpressionVectorParameter, -600, 200, parameter_name="FlashColor", default_value=unreal.LinearColor(1.0, 0.62, 0.22, 1.0))
    bright = node(unreal.MaterialExpressionScalarParameter, -600, 300, parameter_name="Brightness", default_value=30.0)
    tint = node(unreal.MaterialExpressionMultiply, -350, 100)
    link(glow, tint, "A")
    link(colour, tint, "B")
    lit = node(unreal.MaterialExpressionMultiply, -200, 150)
    link(tint, lit, "A")
    link(bright, lit, "B")
    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("M_MuzzleFlash saved " + str(lib.save_asset(m.get_path_name(), False)))
    side = instance("MI_MuzzleFlash", m)
    lib.save_asset(side.get_path_name(), False)
    front = instance("MI_MuzzleFlashFront", m)
    mel.set_material_instance_scalar_parameter_value(front, "Radial", 1.0)
    lib.save_asset(front.get_path_name(), False)


def build_tracer_material():
    # A tracer: a thin rod drawn unlit and additive in the colour the weapon gives it, bright enough to read in daylight.
    m = material("M_Tracer")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("used_with_skeletal_mesh", False)
    node, link = builder(m)
    colour = node(unreal.MaterialExpressionVectorParameter, -600, 0, parameter_name="Colour", default_value=unreal.LinearColor(1.0, 0.72, 0.08, 1.0))
    bright = node(unreal.MaterialExpressionScalarParameter, -600, 200, parameter_name="Brightness", default_value=12.0)
    lit = node(unreal.MaterialExpressionMultiply, -300, 100)
    link(colour, lit, "A")
    link(bright, lit, "B")
    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("M_Tracer saved " + str(lib.save_asset(m.get_path_name(), False)))


def texture(path):
    t = unreal.load_asset(path)
    if t is None:
        lines.append(f"  missing texture {path}")
    return t


def weapon_instance(name, parent, textures=None, scalars=None, vectors=None, two_sided=False):
    if ONLY and name not in ONLY:
        return None
    mi = instance(name, parent)
    for key, path in (textures or {}).items():
        t = texture(path)
        if t is not None:
            mel.set_material_instance_texture_parameter_value(mi, key, t)
    for key, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, key, value)
    for key, value in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(*value))
    if two_sided:
        overrides = mi.get_editor_property("base_property_overrides")
        overrides.set_editor_property("override_two_sided", True)
        overrides.set_editor_property("two_sided", True)
        mi.set_editor_property("base_property_overrides", overrides)
    mel.update_material_instance(mi)
    lines.append(f"{name} saved {lib.save_asset(mi.get_path_name(), False)}")
    return mi


try:
    if ONLY:
        parent = unreal.load_asset(MATERIALS + "/M_CheeseWeapon")
    else:
        parent = build_weapon_material()
        build_flash_material()
    if not ONLY or "M_Tracer" in ONLY:
        build_tracer_material()
    # The Gouda Gatling's gouda wheels (tools/art/cheese_weapons.py): young gouda, glossy and golden with a few small eyes.
    weapon_instance("MI_Gatling", parent, None,
                    {"UseTextures": 0.0, "CheeseAll": 1.0, "MottleScale": 1.2, "Marble": 1.4, "Holes": 0.18, "HoleSize": 0.7, "CheeseRoughness": 0.32},
                    {"CheeseColor": (0.98, 0.7, 0.1, 1.0), "CheeseColorB": (0.92, 0.58, 0.06, 1.0)})
    # The Dual Burratas (tools/art/cheese_weapons.py): each pistol's pouch soft white burrata, glossy and faintly marbled,
    # its barrel the creamy stracciatella heart; its grip the Swiss rind's, its trigger and sights gun steel.
    weapon_instance("MI_Burratas", parent, None, {"UseTextures": 0.0, "CheeseAll": 1.0, "MottleScale": 1.6, "Marble": 1.2, "CheeseRoughness": 0.3},
                    {"CheeseColor": (0.95, 0.94, 0.9, 1.0), "CheeseColorB": (0.86, 0.84, 0.78, 1.0)})
    weapon_instance("MI_BurratasCream", parent, None, {"UseTextures": 0.0, "CheeseAll": 1.0, "MottleScale": 0.35, "Marble": 2.5, "CheeseRoughness": 0.55},
                    {"CheeseColor": (0.93, 0.86, 0.66, 1.0), "CheeseColorB": (0.84, 0.74, 0.5, 1.0)})
    # The Cheese Knife: bright steel, and a handle dipped in red wax.
    weapon_instance("MI_KnifeSteel", parent, None, {"UseTextures": 0.0, "PlainRoughness": 0.22, "PlainMetallic": 1.0}, {"PlainColor": (0.82, 0.82, 0.84, 1.0)})
    weapon_instance("MI_KnifeWax", parent, None, {"UseTextures": 0.0, "RindAll": 1.0, "RindRoughness": 0.28, "MottleScale": 0.8}, {"RindColor": (0.62, 0.03, 0.03, 1.0)})
    # The cheese grenade: a bright yellow Emmental frag, holed all over its blocks, with a dull steel fuse, spoon and ring.
    weapon_instance("MI_GrenadeCheese", parent, None, {"UseTextures": 0.0, "CheeseAll": 1.0, "MottleScale": 0.9, "Marble": 1.6, "Holes": 0.5, "HoleSize": 0.75, "CheeseRoughness": 0.5},
                    {"CheeseColor": (0.98, 0.78, 0.16, 1.0), "CheeseColorB": (0.93, 0.66, 0.1, 1.0)})
    weapon_instance("MI_GrenadeMetal", parent, None, {"UseTextures": 0.0, "PlainRoughness": 0.42, "PlainMetallic": 1.0}, {"PlainColor": (0.42, 0.42, 0.4, 1.0)})
    # The Smoked Gouda (wave 15): the same frag in a dark smoked rind, its holes few and small, matte.
    weapon_instance("MI_GrenadeSmoked", parent, None, {"UseTextures": 0.0, "CheeseAll": 1.0, "MottleScale": 1.4, "Marble": 0.8, "Holes": 0.15, "HoleSize": 0.5, "CheeseRoughness": 0.78},
                    {"CheeseColor": (0.26, 0.14, 0.06, 1.0), "CheeseColorB": (0.17, 0.09, 0.04, 1.0)})
    # The cheese weapons modelled in Blender (tools/art/cheese_weapons.py), slot by slot: plain colours, no maps, each
    # part all cheese, all rind or wax, or plain steel and black polymer.
    plain = {"UseTextures": 0.0}

    def cheese(name, colour, colour_b, **scalars):
        weapon_instance(name, parent, None, dict(plain, CheeseAll=1.0, **scalars), {"CheeseColor": colour, "CheeseColorB": colour_b})

    def rind(name, colour, roughness):
        weapon_instance(name, parent, None, dict(plain, RindAll=1.0, RindRoughness=roughness, MottleScale=0.8), {"RindColor": colour})

    weapon_instance("MI_GunSteel", parent, None, dict(plain, PlainRoughness=0.35, PlainMetallic=1.0), {"PlainColor": (0.3, 0.3, 0.32, 1.0)})
    weapon_instance("MI_GunBlack", parent, None, dict(plain, PlainRoughness=0.6, PlainMetallic=0.0), {"PlainColor": (0.025, 0.025, 0.025, 1.0)})
    weapon_instance("MI_BabyStrip", parent, None, dict(plain, PlainRoughness=0.3, PlainMetallic=0.0), {"PlainColor": (0.9, 0.9, 0.88, 1.0)})
    # The Cheddar Blaster's blocks: a deep cheddar orange; its wrapper dark blue with a yellow band (wave 15), the letters
    # the strip's white.
    cheese("MI_Cheddar", (0.86, 0.27, 0.02, 1.0), (0.74, 0.2, 0.01, 1.0), MottleScale=2.5, Marble=1.5, CheeseRoughness=0.5)
    weapon_instance("MI_WrapBlue", parent, None, dict(plain, PlainRoughness=0.45, PlainMetallic=0.0), {"PlainColor": (0.01, 0.03, 0.2, 1.0)})
    weapon_instance("MI_WrapYellow", parent, None, dict(plain, PlainRoughness=0.45, PlainMetallic=0.0), {"PlainColor": (0.95, 0.72, 0.06, 1.0)})
    # The Colby Carbine's longhorns: orange marbled with cream.
    cheese("MI_Colby", (0.86, 0.3, 0.03, 1.0), (0.9, 0.66, 0.3, 1.0), MottleScale=1.2, Marble=3.0, CheeseRoughness=0.55)
    # The Roquefort Rifle: creamy and veined blue-green; its scope dipped in dark red wax.
    weapon_instance("MI_RoquefortCheese", parent, None, dict(plain, CheeseAll=1.0, MottleScale=3.0, Marble=1.0, Veins=1.0, VeinScale=2.2, VeinWidth=0.11, CheeseRoughness=0.5),
                    {"CheeseColor": (0.84, 0.79, 0.6, 1.0), "CheeseColorB": (0.76, 0.71, 0.52, 1.0), "VeinColor": (0.08, 0.2, 0.22, 1.0)})
    rind("MI_ScopeWax", (0.28, 0.02, 0.04, 1.0), 0.3)
    # The Curd Pistol: fresh white curd.
    cheese("MI_Curd", (0.95, 0.92, 0.8, 1.0), (0.85, 0.8, 0.62, 1.0), MottleScale=0.5, Marble=2.0, CheeseRoughness=0.6)
    # The Swiss Shotgun: pale gold Emmental (its eyes are cut in the model, a few small ones besides) and its rind.
    cheese("MI_Swiss", (0.95, 0.7, 0.2, 1.0), (0.88, 0.6, 0.14, 1.0), MottleScale=3.0, Marble=1.0, Holes=0.12, HoleSize=0.35, CheeseRoughness=0.45)
    rind("MI_SwissRind", (0.55, 0.32, 0.08, 1.0), 0.55)
    # The Munster Twin SMGs: the pale paste and the orange washed rind.
    cheese("MI_MunsterCheese", (0.95, 0.8, 0.42, 1.0), (0.9, 0.72, 0.34, 1.0), MottleScale=1.5, Marble=1.5, CheeseRoughness=0.5)
    rind("MI_MunsterRind", (0.72, 0.2, 0.02, 1.0), 0.45)
    # The Cheese Grater's Parmesan, and its shards: grainy.
    cheese("MI_Parmesan", (0.88, 0.7, 0.34, 1.0), (0.76, 0.58, 0.26, 1.0), MottleScale=0.35, Marble=2.0, CheeseRoughness=0.7)
    # The Cheese Stick: string cheese, white and faintly streaked along it.
    cheese("MI_StringCheese", (0.97, 0.95, 0.88, 1.0), (0.88, 0.86, 0.78, 1.0), MottleScale=4.0, Marble=0.8, CheeseRoughness=0.45)
    # The BabyBomb: glossy red wax, and the cheese under it.
    rind("MI_BabyWax", (0.6, 0.01, 0.01, 1.0), 0.22)
    cheese("MI_BabyCheese", (0.96, 0.78, 0.38, 1.0), (0.92, 0.7, 0.3, 1.0), MottleScale=0.8, Marble=1.2, CheeseRoughness=0.45)
    # The Gouda Gatling's housing: a whole gouda in yellow wax.
    rind("MI_GoudaWax", (0.95, 0.68, 0.04, 1.0), 0.25)
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
