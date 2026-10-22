import os, traceback
import unreal

# Run with the editor closed, once polyhaven.py has downloaded the files below (and after build_materials.py, whose
# M_Surface and M_Prop this uses):
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/import_polyhaven.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder). It writes what it did to Saved/import_polyhaven.txt.
#
# Imports the Poly Haven textures and models (CC0) the maps use into /Game/PolyHaven/<asset>/: each one's textures
# (base colour, normal, roughness, ambient occlusion, metalness, opacity, whichever it has) and, for a model, its mesh,
# with Nanite on and box collision (for shots too), as import_fab.py does for Fab's. Then it makes their materials in
# /Game/CheeseStrike/Materials, as new assets only (so that it leaves the shared ones as they are):
#   MI_<surface>: instances of M_Surface (build_materials.py) drawing a texture from world space, at its real size;
#   MI_<model>: instances of M_Prop, given to each model's mesh;
#   M_PropGlow, its instances MI_<lamp>: a lamp's own textures, its glass (the near-black of its base colour) lit;
#   M_Glow, its instances MI_Glow_<what>: a surface that gives light (a clock's face, a lit window);
#   M_Awning, its instances MI_Awning_<colour>[_V]: striped canvas, the stripes across the world's x (or y, _V).
FAB = os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads")
SOURCE = os.path.join(FAB, "polyhaven")
# What is imported already is left as it is; set CHEESESTRIKE_FAB_REIMPORT=1 to import everything again.
REIMPORT = os.environ.get("CHEESESTRIKE_FAB_REIMPORT") == "1"
OUT = unreal.Paths.project_saved_dir() + "import_polyhaven.txt"
ROOT = "/Game/PolyHaven"
OURS = "/Game/CheeseStrike/Materials"
MAPS = ("BaseColor", "Normal", "Roughness", "AO", "Metalness", "Opacity")

# Textures: asset -> the largest size kept (1K: a metre of setts at 1024 pixels is ten to the centimetre), and the maps
# used (all of them, unless said: the awnings' linen is only its colour).
TEXTURES = {"patterned_cobblestone": 1024, "cobblestone_square": 1024, "painted_plaster_wall": 1024, "weathered_planks": 1024,
            "rough_wood": 1024, "clay_roof_tiles_02": 1024, "sandstone_blocks_08": 1024, "castle_wall_slates": 1024,
            "rough_linen": (1024, ("BaseColor",))}
# Models: asset -> whether its glass glows (a lamp).
MODELS = {"street_lamp_01": True, "street_lamp_02": True, "wine_barrel_01": False, "wooden_crate_01": False,
          "wicker_basket_01": False, "painted_wooden_bench": False}
# Surfaces: name -> (texture, TileU, TileV: the centimetres it covers, as Poly Haven measured it; M_Surface's settings).
SURFACES = {
    "Cobbles_Fan": ("patterned_cobblestone", 250, 250, {"Macro": 0.35, "Grime": 0.15}),
    "Cobbles": ("cobblestone_square", 100, 100, {"Macro": 0.45, "Grime": 0.15}),
    "Timber": ("rough_wood", 60, 60, {"Macro": 0.2, "Desaturate": 0.3, "Tint": (0.55, 0.45, 0.36)}),
    "Planks": ("weathered_planks", 200, 200, {"Macro": 0.3, "Tint": (0.85, 0.78, 0.7)}),
    "RoofTiles": ("clay_roof_tiles_02", 250, 250, {"Macro": 0.4}),
    "Sandstone": ("sandstone_blocks_08", 300, 300, {"Macro": 0.3, "Grime": 0.3, "Tint": (0.9, 0.86, 0.8)}),
    "TownWall": ("castle_wall_slates", 250, 250, {"Macro": 0.35, "Grime": 0.25}),
}
# The town's plaster, painted, and its awnings' canvas, dyed, in these colours.
PLASTER = {"Cream": (0.6, 0.57, 0.5), "Ochre": (0.55, 0.46, 0.33), "Rose": (0.55, 0.43, 0.39), "Sage": (0.44, 0.47, 0.41),
           "Blue": (0.41, 0.46, 0.52), "White": (0.66, 0.65, 0.62), "Grey": (0.48, 0.48, 0.47)}
CANVAS = {"Red": (0.42, 0.05, 0.04), "Cream": (0.72, 0.66, 0.52), "Green": (0.08, 0.24, 0.12), "Blue": (0.07, 0.14, 0.34),
          "Ochre": (0.62, 0.38, 0.05)}
for colour, rgb in PLASTER.items():
    SURFACES["Plaster_" + colour] = ("painted_plaster_wall", 200, 200, {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.9, "Tint": rgb, "Grime": 0.35, "Macro": 0.3})
# Painted shutters.
for colour, rgb in (("Green", (0.12, 0.26, 0.16)), ("Blue", (0.14, 0.24, 0.36))):
    SURFACES["Shutters_" + colour] = ("weathered_planks", 200, 200, {"Desaturate": 1.0, "Normalise": 1.0, "Contrast": 0.8, "Tint": rgb, "Macro": 0.2})
# Glows: name -> (colour, strength).
GLOWS = {"Lamp": ((1.0, 0.6, 0.28), 40.0), "Window": ((1.0, 0.55, 0.24), 6.0), "Clock": ((1.0, 0.88, 0.66), 12.0)}

lines = []
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def import_file(path, destination, name, options=None):
    task = unreal.AssetImportTask()
    if options is not None:
        task.set_editor_property("options", options)
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]


def texture(asset, kind, limit):
    """One map of an asset (Poly Haven's DirectX normal: no flip), set up for what it is and cut to `limit`."""
    destination, name = f"{ROOT}/{asset}", f"T_{asset}_{kind}"
    if not REIMPORT and lib.does_asset_exist(f"{destination}/{name}"):
        return unreal.load_asset(f"{destination}/{name}")
    path = next((os.path.join(SOURCE, asset, f"{asset}_{kind}.{ext}") for ext in ("jpg", "png")
                 if os.path.exists(os.path.join(SOURCE, asset, f"{asset}_{kind}.{ext}"))), None)
    if path is None:
        return None
    imported = [a for a in import_file(path, destination, name) if isinstance(a, unreal.Texture2D)]
    if not imported:
        lines.append(f"  {name}: nothing imported from {path}")
        return None
    t = imported[0]
    if kind == "BaseColor":
        # Not a normal map, whatever the importer guessed from its colour (the linen is pale blue).
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        t.set_editor_property("srgb", True)
    elif kind == "Normal":
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
    else:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        t.set_editor_property("srgb", False)
    width = t.blueprint_get_size_x()
    if width > limit:
        unreal.TextureAssetActionsBlueprintFns.resize_texture_source(t, limit)
    lib.save_asset(t.get_path_name(), False)
    lines.append(f"  {name}: {width} -> {t.blueprint_get_size_x()}")
    return t


def textures(asset, limit, kinds=MAPS):
    found = {}
    for kind in kinds:
        t = texture(asset, kind, limit)
        if t is not None:
            found[kind] = t
    return found


def model(asset):
    """Its mesh, its parts combined into one: Nanite, box collision (shots meet the box too), no materials."""
    destination = f"{ROOT}/{asset}"
    if not REIMPORT and lib.does_asset_exist(f"{destination}/SM_{asset}"):
        return unreal.load_asset(f"{destination}/SM_{asset}")
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("build_nanite", True)
    imported = import_file(os.path.join(SOURCE, asset, f"{asset}.fbx"), destination, "SM_" + asset, options)
    mesh = None
    for a in imported:
        if isinstance(a, unreal.StaticMesh) and mesh is None:
            mesh = a
        elif not isinstance(a, unreal.StaticMesh):
            lib.delete_asset(a.get_path_name())
    if mesh is None:
        lines.append(f"  SM_{asset}: no mesh ({[a.get_name() for a in imported]})")
        return None
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", nanite)
    meshes.remove_collisions(mesh)
    meshes.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    lib.save_asset(mesh.get_path_name(), False)
    b = mesh.get_bounding_box()
    size = b.max - b.min
    lines.append(f"  SM_{asset}: {size.x:.0f} x {size.y:.0f} x {size.z:.0f} cm, {len(mesh.get_editor_property('static_materials'))} materials")
    return mesh


def new_material(name):
    """A material of ours made afresh: emptied if it exists."""
    path = OURS + "/" + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, OURS, unreal.Material, unreal.MaterialFactoryNew())


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


def scalar(node, name, value, x, y):
    return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def colour(node, name, rgb, x, y):
    return node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def glow():
    """M_Glow: a surface that gives light, Colour times Strength."""
    m = new_material("M_Glow")
    m.set_editor_property("used_with_nanite", True)
    node, link = builder(m)
    tint = colour(node, "Colour", (1.0, 0.7, 0.4), -600, 0)
    lit = node(unreal.MaterialExpressionMultiply, -300, 100)
    link(tint, lit, "A")
    link(scalar(node, "Strength", 10.0, -600, 200), lit, "B")
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar(node, "Roughness", 0.4, -600, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append("M_Glow saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def prop_glow():
    """M_PropGlow: a model's own textures, as M_Prop, and light from what in its base colour is near black (a lamp's
    glass): GlowColour times GlowStrength."""
    m = new_material("M_PropGlow")
    m.set_editor_property("used_with_nanite", True)
    node, link = builder(m)
    white = unreal.load_asset(OURS + "/T_White")
    defaults = {"BaseColor": unreal.load_asset("/Engine/EngineResources/DefaultTexture"), "Normal": unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")}
    samplers = {"BaseColor": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "Normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL}
    samples = {}
    for i, kind in enumerate(("BaseColor", "Normal", "Roughness", "AO", "Metalness")):
        samples[kind] = node(unreal.MaterialExpressionTextureSampleParameter2D, -1400, i * 250, parameter_name=kind,
                             texture=defaults.get(kind, white), sampler_type=samplers.get(kind, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
    mel.connect_material_property(samples["BaseColor"], "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(samples["Normal"], "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(samples["Roughness"], "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(samples["AO"], "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    metal = node(unreal.MaterialExpressionMultiply, -600, 1000)
    link(samples["Metalness"], metal, "A", "R")
    link(scalar(node, "Metallic", 1.0, -800, 1060), metal, "B")
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    # The glass: brightness under GlassBelow, lit; a soft edge over GlassSoftness.
    lum = node(unreal.MaterialExpressionDotProduct, -1000, -300)
    link(samples["BaseColor"], lum, "A", "RGB")
    link(node(unreal.MaterialExpressionConstant3Vector, -1200, -250, constant=unreal.LinearColor(0.2126, 0.7152, 0.0722, 1)), lum, "B")
    under = node(unreal.MaterialExpressionSubtract, -800, -300)
    link(scalar(node, "GlassBelow", 0.006, -1000, -180), under, "A")
    link(lum, under, "B")
    soft = node(unreal.MaterialExpressionDivide, -650, -300)
    link(under, soft, "A")
    link(scalar(node, "GlassSoftness", 0.003, -800, -180), soft, "B")
    mask = node(unreal.MaterialExpressionSaturate, -500, -300)
    link(soft, mask)
    lit = node(unreal.MaterialExpressionMultiply, -350, -300)
    link(mask, lit, "A")
    link(colour(node, "GlowColour", (1.0, 0.6, 0.28), -550, -180), lit, "B")
    bright = node(unreal.MaterialExpressionMultiply, -200, -300)
    link(lit, bright, "A")
    link(scalar(node, "GlowStrength", 40.0, -350, -180), bright, "B")
    mel.connect_material_property(bright, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("M_PropGlow saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def awning():
    """M_Awning: striped canvas for the market stalls' awnings: ColourA and ColourB in stripes StripeWidth wide, laid
    across the world's x (or its y, if AlongY), with the linen's weave over them."""
    m = new_material("M_Awning")
    m.set_editor_property("used_with_nanite", True)
    m.set_editor_property("two_sided", True)
    node, link = builder(m)
    position = node(unreal.MaterialExpressionWorldPosition, -1800, 0)
    x = node(unreal.MaterialExpressionComponentMask, -1600, -50, r=True, g=False, b=False, a=False)
    link(position, x)
    y = node(unreal.MaterialExpressionComponentMask, -1600, 50, r=False, g=True, b=False, a=False)
    link(position, y)
    across = node(unreal.MaterialExpressionLinearInterpolate, -1400, 0)
    link(x, across, "A")
    link(y, across, "B")
    link(scalar(node, "AlongY", 0.0, -1600, 150), across, "Alpha")
    times = node(unreal.MaterialExpressionDivide, -1200, 0)
    link(across, times, "A")
    link(scalar(node, "StripeWidth", 45.0, -1400, 150), times, "B")
    half = node(unreal.MaterialExpressionMultiply, -1050, 0)
    link(times, half, "A")
    link(node(unreal.MaterialExpressionConstant, -1200, 120, r=0.5), half, "B")
    stripe = node(unreal.MaterialExpressionFrac, -900, 0)
    link(half, stripe)
    hard = node(unreal.MaterialExpressionRound, -750, 0)
    link(stripe, hard)
    mix = node(unreal.MaterialExpressionLinearInterpolate, -550, 0)
    link(colour(node, "ColourA", (0.42, 0.05, 0.04), -750, -200), mix, "A")
    link(colour(node, "ColourB", (0.72, 0.66, 0.52), -750, 150), mix, "B")
    link(hard, mix, "Alpha")
    # The weave: the linen seen from above, its brightness over its average.
    xy = node(unreal.MaterialExpressionComponentMask, -1600, 400, r=True, g=True, b=False, a=False)
    link(position, xy)
    weave_uv = node(unreal.MaterialExpressionDivide, -1400, 400)
    link(xy, weave_uv, "A")
    link(scalar(node, "WeaveSize", 30.0, -1600, 500), weave_uv, "B")
    weave = node(unreal.MaterialExpressionTextureSampleParameter2D, -1200, 400, parameter_name="WeaveColour",
                 texture=unreal.load_asset(f"{ROOT}/rough_linen/T_rough_linen_BaseColor"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(weave_uv, weave, "UVs")
    lum = node(unreal.MaterialExpressionDotProduct, -950, 400)
    link(weave, lum, "A", "RGB")
    link(node(unreal.MaterialExpressionConstant3Vector, -1150, 600, constant=unreal.LinearColor(0.2126, 0.7152, 0.0722, 1)), lum, "B")
    relative = node(unreal.MaterialExpressionDivide, -800, 400)
    link(lum, relative, "A")
    link(scalar(node, "WeaveAverage", 0.22, -950, 550), relative, "B")
    coloured = node(unreal.MaterialExpressionMultiply, -350, 100)
    link(mix, coloured, "A")
    link(relative, coloured, "B")
    mel.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(node, "Roughness", 0.85, -600, 900), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lines.append("M_Awning saved " + str(lib.save_asset(m.get_path_name(), False)))
    return m


def instance(name, parent, maps, scalars=None, vectors=None):
    path = OURS + "/" + name
    mi = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, OURS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    for kind, t in maps.items():
        mel.set_material_instance_texture_parameter_value(mi, kind, t)
    for k, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, float(value))
    for k, value in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    lib.save_asset(path, False)
    return mi


try:
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
    found = {}
    for asset, spec in TEXTURES.items():
        limit, kinds = spec if isinstance(spec, tuple) else (spec, MAPS)
        found[asset] = textures(asset, limit, kinds)
        lines.append(f"{asset}: {sorted(found[asset])}")
    m_surface = unreal.load_asset(OURS + "/M_Surface")
    for name, (source, tile_u, tile_v, settings) in SURFACES.items():
        settings = dict(settings)
        tint = settings.pop("Tint", None)
        scalars = {"TileU": tile_u, "TileV": tile_v}
        scalars.update(settings)
        maps = {k: t for k, t in found[source].items() if k != "Opacity"}
        instance("MI_" + name, m_surface, maps, scalars, {"Tint": tint} if tint else None)
    lines.append(f"{len(SURFACES)} surfaces")
    m_glow = glow()
    for name, (rgb, strength) in GLOWS.items():
        instance("MI_Glow_" + name, m_glow, {}, {"Strength": strength}, {"Colour": rgb})
    m_awning = awning()
    for name in ("Red", "Green", "Blue", "Ochre"):
        for along in (0, 1):
            instance(f"MI_Awning_{name}{'_V' if along else ''}", m_awning, {}, {"AlongY": along}, {"ColourA": CANVAS[name], "ColourB": CANVAS["Cream"]})
    m_prop = unreal.load_asset(OURS + "/M_Prop")
    m_prop_glow = prop_glow()
    for asset, glows in MODELS.items():
        maps = textures(asset, 1024, [k for k in MAPS if k != "Opacity"])
        mesh = model(asset)
        if mesh is None:
            continue
        scalars = {"Metallic": 1.0} if "Metalness" in maps else {"Metallic": 0.0}
        mi = instance("MI_" + asset, m_prop_glow if glows else m_prop, maps, scalars)
        for index in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(index, mi)
        lib.save_asset(mesh.get_path_name(), False)
        lines.append(f"{asset}: {sorted(maps)}")
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
