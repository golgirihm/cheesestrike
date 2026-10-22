import unreal, traceback

# Run with the editor closed, after import_fab.py and build_materials.py:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/grotto_materials.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/grotto_materials.txt.
#
# The materials Gruyère Grotto (build_grotto.py) is dressed in, made from the Megascans Old Mine pack and quarry cliffs
# that import_fab.py brings in:
#   MI_MineRock, MI_MineRockDark, MI_MineRockWet, MI_CaveFloor, MI_Timber: instances of build_materials.py's M_Surface
#     (drawn from world space on the map's blocks) for the cave's rock, its damp lower walls, its floor and timber;
#   MI_<prop>: each new prop's own textures on M_Prop, given to its mesh; the cliffs recoloured to the cave's rock;
#   M_Lamplight: glass that glows (a lantern's, a lamp's), its colour the parameter Glow, how bright Strength;
#     instances MI_Lamplight_<colour>, and MI_Hole, black (a hole into the dark rock);
#   M_GrottoPool: still water that glows from below, dark and glassy, mottled as if lit through the depth; instances
#     MI_GrottoPool (the glowing pools) and MI_Puddle (unlit: damp on the floor).
# Neither M_Lamplight nor M_GrottoPool is loaded by C++, so they can be rebuilt.
OUT = unreal.Paths.project_saved_dir() + "grotto_materials.txt"
FOLDER = "/Game/CheeseStrike/Materials"
FAB = "/Game/Fab/Megascans"
MAPS = ("BaseColor", "Normal", "Roughness", "AO", "Metalness")

# Surfaces: name -> (texture set, TileU, TileV, settings), as build_materials.py's.
SURFACES = {
    "MineRock": ("MineRockWall", 450, 450, {"Macro": 0.45, "Desaturate": 0.25, "Tint": (0.72, 0.68, 0.62), "NormalStrength": 0.7}),
    "MineRockDark": ("MineRockDark", 400, 400, {"Macro": 0.4, "Desaturate": 0.2, "NormalStrength": 0.8}),
    # Damp: darker and glossier, near the pools and low on the walls.
    "MineRockWet": ("MineRockDark", 300, 300, {"Macro": 0.4, "Desaturate": 0.2, "Tint": (0.6, 0.62, 0.62), "RoughnessScale": 0.45}),
    "CaveFloor": ("CoalMineFloor", 250, 250, {"Macro": 0.5, "Tint": (1.1, 1.05, 1.0), "Grime": 0.1}),
    "Timber": ("../Props/WoodenBeam", 60, 240, {"Macro": 0.3}),
}
# Props: name -> (scalar settings, vector settings).
PROPS = {
    "QuarryCliff": ({"Desaturate": 0.75, "Brightness": 1.6}, {"Tint": (0.62, 0.56, 0.5)}),
    "QuarryCliffLarge": ({"Desaturate": 0.75, "Brightness": 1.6}, {"Tint": (0.62, 0.56, 0.5)}),
    "MineCart": ({}, {}),
    "BulkheadLight": ({}, {}),
    "WoodenBeam": ({}, {}),
    "RailTrack": ({}, {}),
}
# Lamp glass: name -> its glow (linear colour) and strength.
GLOWS = {
    "Amber": ((1.0, 0.55, 0.18), 18.0),
    "Gold": ((1.0, 0.7, 0.3), 18.0),
    "Ice": ((0.45, 0.75, 1.0), 18.0),
    "Red": ((1.0, 0.22, 0.12), 14.0),
    "White": ((1.0, 0.9, 0.75), 22.0),
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
    return m


def builder(m):
    def node(cls, x, y, **props):
        n = mel.create_material_expression(m, cls, x, y)
        for k, value in props.items():
            n.set_editor_property(k, value)
        return n

    def link(a, b, pin="", out=""):
        if not mel.connect_material_expressions(a, out, b, pin):
            lines.append(f"{m.get_name()}: couldn't link {a.get_class().get_name()}.{out} -> {b.get_class().get_name()}.{pin}")
    return node, link


def instance(name, parent, textures=None, scalars=None, vectors=None):
    path = FOLDER + "/" + name
    mi = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    for kind, texture in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, kind, texture)
    for k, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, float(value))
    for k, value in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    lib.save_asset(path, False)
    return mi


def textures_in(folder, prefix):
    found = {}
    for kind in MAPS:
        path = f"{folder}/{prefix}_{kind}"
        if lib.does_asset_exist(path):
            found[kind] = unreal.load_asset(path)
    return found


def lamplight():
    """Glass with a light behind it: a dark body whose emissive is Glow times Strength."""
    m = material("M_Lamplight")
    m.set_editor_property("used_with_nanite", True)
    node, link = builder(m)
    glow = node(unreal.MaterialExpressionVectorParameter, -700, 0, parameter_name="Glow", default_value=unreal.LinearColor(1.0, 0.6, 0.25, 1.0))
    strength = node(unreal.MaterialExpressionScalarParameter, -700, 200, parameter_name="Strength", default_value=15.0)
    emissive = node(unreal.MaterialExpressionMultiply, -400, 100)
    link(glow, emissive, "A")
    link(strength, emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    tint = node(unreal.MaterialExpressionMultiply, -400, -100, const_b=0.3)
    link(glow, tint, "A")
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -400, 300, r=0.25), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append("M_Lamplight saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def pool():
    """Still water lit from below: near black and glassy on top, glowing teal through it, brighter in patches that drift
    with the world position (the light through deeper and shallower water), and a little at the edges' grazing angles."""
    m = material("M_GrottoPool")
    m.set_editor_property("used_with_nanite", True)
    node, link = builder(m)
    colour = node(unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Glow", default_value=unreal.LinearColor(0.12, 0.7, 0.6, 1.0))
    strength = node(unreal.MaterialExpressionScalarParameter, -900, 160, parameter_name="Strength", default_value=3.0)
    patches = node(unreal.MaterialExpressionNoise, -900, 300, scale=0.004, levels=3, output_min=0.65, output_max=1.0, quality=1)
    lit = node(unreal.MaterialExpressionMultiply, -650, 60)
    link(colour, lit, "A")
    link(strength, lit, "B")
    emissive = node(unreal.MaterialExpressionMultiply, -400, 100)
    link(lit, emissive, "A")
    link(patches, emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant3Vector, -400, -100, constant=unreal.LinearColor(0.008, 0.016, 0.016, 1.0)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -400, 300, r=0.02), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -400, 380, r=0.8), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    lines.append("M_GrottoPool saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


try:
    surface = unreal.load_asset(FOLDER + "/M_Surface")
    for name, (source, tile_u, tile_v, settings) in SURFACES.items():
        settings = dict(settings)
        tint = settings.pop("Tint", None)
        scalars = {"TileU": tile_u, "TileV": tile_v}
        scalars.update(settings)
        if source.startswith("../Props/"):
            asset = source.split("/")[-1]
            textures = textures_in(f"{FAB}/Props/{asset}", "T_" + asset)
        else:
            textures = textures_in(f"{FAB}/Surfaces/{source}", "T_" + source)
        instance("MI_" + name, surface, textures, scalars, {"Tint": tint} if tint else None)
        lines.append(f"MI_{name}: {sorted(textures)}")
    prop = unreal.load_asset(FOLDER + "/M_Prop")
    for name, (scalars, vectors) in PROPS.items():
        folder = f"{FAB}/Props/{name}"
        textures = textures_in(folder, "T_" + name)
        scalars = dict(scalars)
        if "Metalness" in textures:
            scalars["Metallic"] = 1.0
        mi = instance("MI_" + name, prop, textures, scalars, vectors)
        mesh = unreal.load_asset(f"{folder}/SM_{name}")
        for index in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(index, mi)
        lib.save_asset(mesh.get_path_name(), False)
        lines.append(f"MI_{name}: {sorted(textures)}")
    glass = lamplight()
    for name, (rgb, strength) in GLOWS.items():
        instance("MI_Lamplight_" + name, glass, None, {"Strength": strength}, {"Glow": rgb})
    # A hole into the dark (where the mice come out of the rock): black, unlit.
    instance("MI_Hole", glass, None, {"Strength": 0.0}, {"Glow": (0.0, 0.0, 0.0)})
    water = pool()
    # The pools glow; a puddle is the same still water with nothing lit under it.
    instance("MI_GrottoPool", water, None, {"Strength": 0.025})
    instance("MI_Puddle", water, None, {"Strength": 0.0})
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
