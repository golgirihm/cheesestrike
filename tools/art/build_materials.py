import unreal, traceback

# Run with the editor closed, after import_fab.py:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/build_materials.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/build_materials.txt.
#
# Builds the maps' materials besides the cheese, from the Megascans textures
# import_fab.py brings in (/Game/Fab/):
#   M_Surface: a surface for the maps' blocks, drawn from world space on all
#     three axes so a block of any size and shape reads at the surface's real
#     scale (TileU, TileV: centimetres the texture covers), with its colour
#     varied over large areas (Macro) so the repeat doesn't show, recoloured
#     (Desaturate, Normalise, Tint: a painted surface takes any paint), and grimier
#     towards the ground (Grime); instances MI_<surface>[_<colour>];
#   M_Prop: a model's own textures on its own UVs, recoloured the same way;
#     each prop's mesh gets its instance MI_<prop>;
#   M_Decal, M_DecalStain: a decal with its normal and roughness, or only
#     colour (a stain or a leak); instances MI_Decal_<decal>.
OUT = unreal.Paths.project_saved_dir() + "build_materials.txt"
FOLDER = "/Game/CheeseStrike/Materials"
FAB = "/Game/Fab/Megascans"
WHITE = FOLDER + "/T_White"
FLAT = "/Engine/EngineMaterials/DefaultNormal"
GREY = "/Engine/EngineResources/DefaultTexture"
MAPS = ("BaseColor", "Normal", "Roughness", "AO", "Metalness")

TRIPLANAR = r"""
float3 n = normalize(N);
float3 w = pow(abs(n), 4.0);
w /= max(w.x + w.y + w.z, 1e-4);
// Each axis's projection: the texture's across (U) and down (D) directions in the world; upright faces
// have the texture's top up, whichever way they face.
float3 Ux = float3(0, n.x >= 0 ? -1 : 1, 0);
float3 Uy = float3(n.y >= 0 ? 1 : -1, 0, 0);
float3 Uz = float3(1, 0, 0);
float3 Dx = float3(0, 0, -1);
float3 Dy = float3(0, 0, -1);
float3 Dz = float3(0, 1, 0);
float2 tile = float2(TileU, TileV);
float2 ux = float2(dot(Pos, Ux), dot(Pos, Dx)) / tile;
float2 uy = float2(dot(Pos, Uy), dot(Pos, Dy)) / tile;
float2 uz = float2(dot(Pos, Uz), dot(Pos, Dz)) / tile;
float3 c = Texture2DSample(BaseColor, BaseColorSampler, ux).rgb * w.x + Texture2DSample(BaseColor, BaseColorSampler, uy).rgb * w.y + Texture2DSample(BaseColor, BaseColorSampler, uz).rgb * w.z;
float2 tx = Texture2DSample(Normal, NormalSampler, ux).rg * 2 - 1;
float2 ty = Texture2DSample(Normal, NormalSampler, uy).rg * 2 - 1;
float2 tz = Texture2DSample(Normal, NormalSampler, uz).rg * 2 - 1;
float3 bend = (tx.x * Ux + tx.y * Dx) * w.x + (ty.x * Uy + ty.y * Dy) * w.y + (tz.x * Uz + tz.y * Dz) * w.z;
NormalWS = normalize(n + bend * NormalStrength);
float3 r = float3(Texture2DSample(Roughness, RoughnessSampler, ux).r, Texture2DSample(Roughness, RoughnessSampler, uy).r, Texture2DSample(Roughness, RoughnessSampler, uz).r);
float3 a = float3(Texture2DSample(AO, AOSampler, ux).r, Texture2DSample(AO, AOSampler, uy).r, Texture2DSample(AO, AOSampler, uz).r);
float3 m = float3(Texture2DSample(Metalness, MetalnessSampler, ux).r, Texture2DSample(Metalness, MetalnessSampler, uy).r, Texture2DSample(Metalness, MetalnessSampler, uz).r);
// Where the texture repeats, it is varied: value noise over the world (11 m and 3 m across) blends in a
// second, turned and rescaled copy of it and shades it lighter and darker, so no grid of repeats shows.
const float3 L = float3(0.2126, 0.7152, 0.0722);
float3 q = Pos / 1100.0;
float noise = 0.0;
float amp = 0.7;
for (int o = 0; o < 2; o++)
{
    float3 i0 = floor(q);
    float3 f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    const float3 K = float3(127.1, 311.7, 74.7);
    float v000 = frac(sin(dot(i0, K)) * 43758.5453);
    float v100 = frac(sin(dot(i0 + float3(1, 0, 0), K)) * 43758.5453);
    float v010 = frac(sin(dot(i0 + float3(0, 1, 0), K)) * 43758.5453);
    float v110 = frac(sin(dot(i0 + float3(1, 1, 0), K)) * 43758.5453);
    float v001 = frac(sin(dot(i0 + float3(0, 0, 1), K)) * 43758.5453);
    float v101 = frac(sin(dot(i0 + float3(1, 0, 1), K)) * 43758.5453);
    float v011 = frac(sin(dot(i0 + float3(0, 1, 1), K)) * 43758.5453);
    float v111 = frac(sin(dot(i0 + float3(1, 1, 1), K)) * 43758.5453);
    noise += amp * lerp(lerp(lerp(v000, v100, f.x), lerp(v010, v110, f.x), f.y), lerp(lerp(v001, v101, f.x), lerp(v011, v111, f.x), f.y), f.z);
    q *= 3.7;
    amp = 0.3;
}
const float2x2 turn = float2x2(0.68, -0.47, 0.47, 0.68);
float3 c2 = Texture2DSample(BaseColor, BaseColorSampler, mul(turn, ux) + 0.37).rgb * w.x + Texture2DSample(BaseColor, BaseColorSampler, mul(turn, uy) + 0.37).rgb * w.y + Texture2DSample(BaseColor, BaseColorSampler, mul(turn, uz) + 0.37).rgb * w.z;
c = lerp(c, c2, smoothstep(0.4, 0.6, noise) * Macro);
c *= lerp(1.0, 0.7 + 0.6 * noise, Macro);
// Less contrast for a cleaner surface (a fresh coat of paint).
float3 average = Texture2DSampleLevel(BaseColor, BaseColorSampler, float2(0.5, 0.5), 14).rgb;
float level = max(dot(average, L), 0.02);
c = lerp(average, c, Contrast);
// Recoloured: greyed (Desaturate), then for paint brought to the texture's average brightness (Normalise),
// so that Tint is the paint's colour; then tinted.
c = lerp(c, dot(c, L).xxx, Desaturate);
c = lerp(c, c / level, Normalise) * Tint;
// Grimier towards the ground: the bottom 1.2 m darkens and roughens.
float clean = saturate(Pos.z / 120.0);
c *= lerp(1.0 - Grime, 1.0, clean);
Rough = saturate(dot(r, w) * RoughnessScale + Grime * 0.15 * (1.0 - clean));
Occlusion = lerp(1.0, dot(a, w), AOStrength);
Metal = dot(m, w) * Metallic;
return c;
"""

# Surfaces: name -> (texture set, TileU, TileV, settings).
SURFACES = {
    "ConcreteFloor": ("ConcreteFloor", 250, 250, {"Macro": 0.35, "Desaturate": 0.4}),
    "ConcreteWall": ("ConcreteWall", 400, 200, {"Macro": 0.3, "Grime": 0.25}),
    "ConcreteFacade": ("ConcreteFacade", 400, 400, {"Macro": 0.3}),
    "Asphalt": ("Asphalt", 150, 150, {"Macro": 0.5, "Desaturate": 0.6, "Tint": (1.2, 1.2, 1.2)}),
    "GarageDoor": ("GarageDoor", 250, 250, {"Grime": 0.3}),
    "Roof": ("PaintedRoof", 200, 200, {"Macro": 0.4}),
    "Steel": ("PaintedMetal", 200, 100, {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.6, "Tint": (0.22, 0.23, 0.24), "Metallic": 0.6, "Grime": 0.2}),
}
# Painted surfaces in the maps' colours: corrugated sheets for walls and containers, flat painted metal for the rest.
COLOURS = {
    "Cream": (0.55, 0.52, 0.45),
    "Teal": (0.07, 0.24, 0.25),
    "Red": (0.36, 0.06, 0.05),
    "Blue": (0.06, 0.13, 0.30),
    "Yellow": (0.62, 0.42, 0.04),
    "Orange": (0.60, 0.17, 0.03),
    "Dark": (0.06, 0.06, 0.065),
    "Grey": (0.30, 0.31, 0.32),
}
for colour, rgb in COLOURS.items():
    SURFACES["Corrugated_" + colour] = ("CorrugatedSheets", 200, 200, {"Desaturate": 1.0, "Normalise": 1.0, "Tint": rgb, "Grime": 0.35, "Macro": 0.25})
    SURFACES["Painted_" + colour] = ("PaintedMetal", 200, 100, {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.45, "Tint": rgb, "Grime": 0.3, "Metallic": 1.0, "Macro": 0.2})
PROPS = ["Pallet", "TarpedCrate", "Barrel", "PlasticDrum", "PlasticCrate", "CardboardBoxes", "FluorescentLamp",
         "MetalCabinet", "Workbench", "HandTruck", "GasTank", "TrashCan", "ConcreteBarrier"]
CONTAINER = "/Game/Fab/SousinhoGames/ShippingContainer"
RAT = "/Game/Fab/Nestaeric/BlackRat"
CONTAINER_COLOURS = ("Blue", "Red", "Teal", "Yellow", "Cream")
# Decals: name -> whether it has a normal (else it only stains).
DECALS = {"OilStain": True, "Leakage": False, "AsphaltCrack": True, "PaintedOne": True, "PaintedTwo": True, "PaintedThree": True}

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


def default_for(kind):
    return unreal.load_asset({"BaseColor": GREY, "Normal": FLAT}.get(kind, WHITE))


def sampler_for(kind):
    return {"BaseColor": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "Normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL}.get(kind, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)


def scalar(node, name, value, x, y):
    return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def surface():
    m = material("M_Surface")
    m.set_editor_property("used_with_nanite", True)
    m.set_editor_property("tangent_space_normal", False)
    node, link = builder(m)
    custom = node(unreal.MaterialExpressionCustom, -400, 0, code=TRIPLANAR, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="World-space surface")
    names = ["Pos", "N"] + list(MAPS) + ["TileU", "TileV", "NormalStrength", "Macro", "Desaturate", "Normalise", "Contrast", "Tint", "Grime", "RoughnessScale", "AOStrength", "Metallic"]
    inputs = []
    for name in names:
        i = unreal.CustomInput()
        i.set_editor_property("input_name", name)
        inputs.append(i)
    custom.set_editor_property("inputs", inputs)
    outputs = []
    for name, kind in (("NormalWS", unreal.CustomMaterialOutputType.CMOT_FLOAT3), ("Rough", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                       ("Occlusion", unreal.CustomMaterialOutputType.CMOT_FLOAT1), ("Metal", unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", kind)
        outputs.append(o)
    custom.set_editor_property("additional_outputs", outputs)
    mel.recompile_material(m)
    link(node(unreal.MaterialExpressionWorldPosition, -900, -200), custom, "Pos")
    link(node(unreal.MaterialExpressionVertexNormalWS, -900, -100), custom, "N")
    for i, kind in enumerate(MAPS):
        texture = node(unreal.MaterialExpressionTextureObjectParameter, -900, i * 120, parameter_name=kind, texture=default_for(kind), sampler_type=sampler_for(kind))
        link(texture, custom, kind)
    defaults = {"TileU": 200.0, "TileV": 200.0, "NormalStrength": 1.0, "Macro": 0.0, "Desaturate": 0.0, "Normalise": 0.0, "Contrast": 1.0, "Grime": 0.0, "RoughnessScale": 1.0, "AOStrength": 1.0, "Metallic": 0.0}
    for i, (name, value) in enumerate(defaults.items()):
        link(scalar(node, name, value, -700, 600 + i * 60), custom, name)
    tint = node(unreal.MaterialExpressionVectorParameter, -700, 1200, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    link(tint, custom, "Tint")
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(custom, "NormalWS", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(custom, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(custom, "Occlusion", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(custom, "Metal", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    lines.append("M_Surface saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def recolour(node, link, colour_in, x, y):
    """lerp(colour, its brightness over the texture's average, Desaturate) * Tint, as M_Surface does."""
    lum = node(unreal.MaterialExpressionDotProduct, x, y)
    link(colour_in, lum, "A")
    link(node(unreal.MaterialExpressionConstant3Vector, x - 200, y + 60, constant=unreal.LinearColor(0.2126, 0.7152, 0.0722, 1)), lum, "B")
    capped = node(unreal.MaterialExpressionMin, x + 75, y)
    link(lum, capped, "A")
    link(scalar(node, "MaxLuminance", 10.0, x - 100, y + 140), capped, "B")
    grey = node(unreal.MaterialExpressionMultiply, x + 150, y)
    link(capped, grey, "A")
    link(scalar(node, "Brightness", 2.0, x, y + 140), grey, "B")
    mix = node(unreal.MaterialExpressionLinearInterpolate, x + 300, y - 60)
    link(colour_in, mix, "A")
    link(grey, mix, "B")
    link(scalar(node, "Desaturate", 0.0, x + 150, y + 200), mix, "Alpha")
    tinted = node(unreal.MaterialExpressionMultiply, x + 450, y)
    link(mix, tinted, "A")
    link(node(unreal.MaterialExpressionVectorParameter, x + 300, y + 260, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1)), tinted, "B")
    return tinted


def prop(name="M_Prop", masked=False):
    """A model's own textures; `masked` for cut-out cards (fur, leaves), two-sided, cut by the base colour's alpha."""
    m = material(name)
    m.set_editor_property("used_with_nanite", not masked)
    m.set_editor_property("used_with_skeletal_mesh", True)
    if masked:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
    node, link = builder(m)
    samples = {}
    for i, kind in enumerate(MAPS):
        samples[kind] = node(unreal.MaterialExpressionTextureSampleParameter2D, -1200, i * 250, parameter_name=kind, texture=default_for(kind), sampler_type=sampler_for(kind))
    mel.connect_material_property(recolour(node, link, samples["BaseColor"], -800, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(samples["Normal"], "", unreal.MaterialProperty.MP_NORMAL)
    rough = node(unreal.MaterialExpressionMultiply, -300, 500)
    link(samples["Roughness"], rough, "A", "R")
    link(scalar(node, "RoughnessScale", 1.0, -500, 560), rough, "B")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(samples["AO"], "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    metal = node(unreal.MaterialExpressionMultiply, -300, 1000)
    link(samples["Metalness"], metal, "A", "R")
    link(scalar(node, "Metallic", 0.0, -500, 1060), metal, "B")
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    if masked:
        mel.connect_material_property(samples["BaseColor"], "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(m)
    lines.append(f"{name} saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def decal(name, with_normal):
    m = material(name)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    node, link = builder(m)
    colour = node(unreal.MaterialExpressionTextureSampleParameter2D, -1000, 0, parameter_name="BaseColor", texture=default_for("BaseColor"), sampler_type=sampler_for("BaseColor"))
    mel.connect_material_property(recolour(node, link, colour, -700, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    opacity = node(unreal.MaterialExpressionTextureSampleParameter2D, -1000, 400, parameter_name="Opacity", texture=default_for("Opacity"), sampler_type=sampler_for("Opacity"))
    strength = node(unreal.MaterialExpressionMultiply, -400, 400)
    link(opacity, strength, "A", "R")
    link(scalar(node, "OpacityScale", 1.0, -600, 460), strength, "B")
    mel.connect_material_property(strength, "", unreal.MaterialProperty.MP_OPACITY)
    if with_normal:
        mel.connect_material_property(node(unreal.MaterialExpressionTextureSampleParameter2D, -1000, 700, parameter_name="Normal", texture=default_for("Normal"), sampler_type=sampler_for("Normal")), "", unreal.MaterialProperty.MP_NORMAL)
        rough = node(unreal.MaterialExpressionTextureSampleParameter2D, -1000, 950, parameter_name="Roughness", texture=default_for("Roughness"), sampler_type=sampler_for("Roughness"))
        mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        # A stain or a leak: wet, so a little glossier than what it is on.
        mel.connect_material_property(scalar(node, "Roughness", 0.55, -600, 700), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append(f"{name} saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def instance(name, parent, textures, scalars=None, vectors=None):
    path = FOLDER + "/" + name
    mi = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    for kind, texture in textures.items():
        mel.set_material_instance_texture_parameter_value(mi, kind, texture)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, float(v))
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    lib.save_asset(path, False)
    return mi


def textures_in(folder, prefix):
    found = {}
    for kind in MAPS + ("Opacity",):
        path = f"{folder}/{prefix}_{kind}"
        if lib.does_asset_exist(path):
            found[kind] = unreal.load_asset(path)
    return found


try:
    m_surface = surface()
    for name, (source, tile_u, tile_v, settings) in SURFACES.items():
        settings = dict(settings)
        tint = settings.pop("Tint", None)
        scalars = {"TileU": tile_u, "TileV": tile_v}
        scalars.update(settings)
        instance("MI_" + name, m_surface, textures_in(f"{FAB}/Surfaces/{source}", "T_" + source), scalars, {"Tint": tint} if tint else None)
    lines.append(f"{len(SURFACES)} surfaces")

    m_prop = prop()
    for name in PROPS:
        folder = f"{FAB}/Props/{name}"
        textures = textures_in(folder, "T_" + name)
        mi = instance("MI_" + name, m_prop, textures, {"Metallic": 1.0} if "Metalness" in textures else None)
        mesh = unreal.load_asset(f"{folder}/SM_{name}")
        mesh.set_material(0, mi)
        lib.save_asset(mesh.get_path_name(), False)
    # The container: three parts, each in its own textures, and in each of the maps' container colours.
    mesh = unreal.load_asset(CONTAINER + "/SM_Container")
    slots = [str(s.get_editor_property("material_slot_name")) for s in mesh.get_editor_property("static_materials")]
    lines.append(f"container slots: {slots}")
    for colour in (None,) + CONTAINER_COLOURS:
        for index, slot in enumerate(slots):
            part = "Frame" if "frame" in slot.lower() else "Doors" if "door" in slot.lower() else "Sides"
            textures = textures_in(CONTAINER, "T_Container_" + part)
            if colour is None:
                mi = instance(f"MI_Container_{part}", m_prop, textures, {"Metallic": 1.0})
                mesh.set_material(index, mi)
            else:
                # Repainted, with the lettering on its sides (a real shipping line's) painted over by the brightness cap.
                instance(f"MI_Container_{part}_{colour}", m_prop, textures, {"Metallic": 1.0, "Desaturate": 1.0, "MaxLuminance": 0.14, "Brightness": 3.0}, {"Tint": COLOURS[colour]})
    lib.save_asset(mesh.get_path_name(), False)
    lines.append(f"{len(PROPS)} props and the container")
    # The rat, for the mice: its fur is cut-out cards, the rest (body, eyes) solid.
    m_fur = prop("M_PropMasked", True)
    rat = unreal.load_asset(RAT + "/SK_BlackRat")
    textures = textures_in(RAT, "T_BlackRat")
    solid = instance("MI_BlackRat", m_prop, textures, {"Metallic": 1.0} if "Metalness" in textures else None)
    fur = instance("MI_BlackRat_Fur", m_fur, textures, {"Metallic": 1.0} if "Metalness" in textures else None)
    # New slots, not the old ones changed: a slot read from the mesh is a copy, and setting it changes nothing.
    materials = []
    for old in rat.get_editor_property("materials"):
        slot = unreal.SkeletalMaterial()
        name = old.get_editor_property("material_slot_name")
        slot.set_editor_property("material_slot_name", name)
        slot.set_editor_property("material_interface", fur if "fur" in str(name).lower() else solid)
        materials.append(slot)
    rat.modify()
    rat.set_editor_property("materials", materials)
    lines.append(f"rat slots: {[(str(x.get_editor_property('material_slot_name')), str(x.get_editor_property('material_interface').get_name())) for x in rat.get_editor_property('materials')]}; saved " + str(lib.save_asset(rat.get_path_name(), False)))

    m_decal = decal("M_Decal", True)
    m_stain = decal("M_DecalStain", False)
    for name, with_normal in DECALS.items():
        textures = textures_in(f"{FAB}/Decals/{name}", "T_" + name)
        if not with_normal:
            textures = {k: v for k, v in textures.items() if k in ("BaseColor", "Opacity")}
        else:
            textures = {k: v for k, v in textures.items() if k in ("BaseColor", "Opacity", "Normal", "Roughness")}
        instance("MI_Decal_" + name, m_decal if with_normal else m_stain, textures)
    lines.append(f"{len(DECALS)} decals")
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
