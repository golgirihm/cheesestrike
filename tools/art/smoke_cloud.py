import math, os, struct, sys, traceback, zlib

# Two steps, from the repository's root:
#   1. C:\Hamid\code\FabDownloads\piper\venv\Scripts\python.exe -I tools/art/smoke_cloud.py prepare
#      (any Python with numpy) draws the puffs' texture into Saved/smoke_puffs/T_SmokePuffs.png.
#   2. With the editor closed:
#      UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/smoke_cloud.py" -unattended -nullrhi
#      (both paths absolute: the editor runs from its own folder) imports it and builds the material; it writes what it
#      did to Saved/smoke_cloud.txt.
#
# Builds the look of the Smoked Gouda's cloud (Weapons/CheeseStrikeSmokeCloud,
# Weapons/CheeseStrikeGrenadeRules.h's Cloud* numbers), in /Game/CheeseStrike/Effects:
#   T_SmokePuffs  (since wave 17, which made the smoke look real rather than cartoony) eight puffs of smoke in a 4 by 2
#                 atlas, each drawn from nothing: a cauliflower of overlapping rounded billows, its density eaten into
#                 at the edges by noise so that it frays into wisps, nothing at the cell's edge; red its density, green
#                 and blue the billows' surface's slope (a normal's x and y, from a height field of the billows and the
#                 noise), alpha how thick it is there;
#   M_SmokeMist   the cloud's cards (flat cards the game turns to the camera, the blood's mist cards): a smoke grey, each
#                 card one of the puffs (by its instance's random), lit from the sky through the billows' slopes (bright
#                 on their tops and sides toward the light, darker in their folds and underneath; thin wisps a little
#                 lighter), then by the level's light like anything translucent, with a little light of its own; each card's thickness its instance's
#                 custom data 0, softened where it meets the floor or anything else (a depth fade). A card may be solid
#                 (custom data 1, 0 to 1): a solid one is whole out to 0.6 of its radius and frays along the puff's
#                 shape into its wisps past it, which the cloud's veil is made of (the cards nothing is seen through, between
#                 this machine's camera and the cloud's middle), its whole middle stirring slowly through the world and
#                 lighter above the eye, with a longer fade into what is behind it; each card is
#                 lit a little lighter or darker (custom data 2, its shade, 1 for none).
# The game loads it by path, so this script can rebuild it.
SAVED = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Saved", "smoke_puffs")
PUFFS = "T_SmokePuffs.png"
CELL = 512
COLUMNS, ROWS = 4, 2


def write_png(path, rgba):
    """`rgba`: a height x width x 4 array of bytes."""
    height, width = rgba.shape[0], rgba.shape[1]
    raw = b"".join(b"\x00" + rgba[y].tobytes() for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def prepare():
    import numpy as np
    rng = np.random.default_rng(1717)
    axis = (np.arange(CELL) + 0.5) / CELL * 2.0 - 1.0
    x, y = np.meshgrid(axis, axis)
    r = np.sqrt(x * x + y * y)

    def value_noise(freq):
        grid = rng.random((freq + 2, freq + 2))
        u = (x + 1.0) / 2.0 * freq
        v = (y + 1.0) / 2.0 * freq
        i, j = np.floor(u).astype(int), np.floor(v).astype(int)
        fu, fv = u - i, v - j
        fu, fv = fu * fu * (3 - 2 * fu), fv * fv * (3 - 2 * fv)
        a, b = grid[j, i], grid[j, i + 1]
        c, d = grid[j + 1, i], grid[j + 1, i + 1]
        return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv

    def fbm(base, octaves):
        total, amp, norm = np.zeros_like(x), 1.0, 0.0
        for o in range(octaves):
            total += value_noise(base * 2 ** o) * amp
            norm += amp
            amp *= 0.55
        return total / norm

    def blur(field, sigma):
        # A Gaussian blur, by rows then columns.
        k = np.arange(-3 * sigma, 3 * sigma + 1)
        kernel = np.exp(-k * k / (2.0 * sigma * sigma))
        kernel /= kernel.sum()
        field = np.apply_along_axis(lambda row: np.convolve(row, kernel, mode="same"), 1, field)
        return np.apply_along_axis(lambda column: np.convolve(column, kernel, mode="same"), 0, field)

    def warped(field_x, field_y, amount, freq):
        # The coordinates pushed about by slow noise, so that the billows swirl rather than sit as balls.
        return field_x + amount * (fbm(freq, 3) - 0.5), field_y + amount * (fbm(freq, 3) - 0.5)

    atlas = np.zeros((CELL * ROWS, CELL * COLUMNS, 4), dtype=np.uint8)
    for cell in range(COLUMNS * ROWS):
        # A soft round body whose outline is pushed about by slow noise, so that each puff is its own shape.
        wx, wy = warped(x, y, 0.7, 2)
        wr = np.sqrt(wx * wx + wy * wy)
        falloff = np.clip(1.0 - wr / 0.8, 0.0, 1.0) ** 0.7
        # Billows: rounded domes over it, big and small, blended into one lumpy surface.
        domes = np.zeros_like(x)
        for _ in range(int(rng.integers(22, 30))):
            radius = rng.uniform(0.1, 0.3)
            angle = rng.uniform(0, 2 * math.pi)
            out = rng.uniform(0.0, 0.85 - radius * 0.5)
            bx, by = out * math.cos(angle), out * math.sin(angle)
            d2 = ((wx - bx) ** 2 + (wy - by) ** 2) / (radius * radius)
            domes += (radius * np.sqrt(np.clip(1.0 - d2, 0.0, 1.0))) ** 4
        domes = domes ** 0.25
        domes /= domes.max()
        broad = fbm(3, 3)
        wisps = fbm(6, 4)
        # Soft all through: thickest in its middle and thinning slowly outward, eaten into by the noise so that its
        # edge frays into wisps; never quite solid (the cloud is thick where its cards overlap); nothing at the cell's
        # edge.
        body = np.clip(falloff * 1.3 + 0.3 * (domes - 0.4) + 0.55 * (broad - 0.5) + 0.35 * (wisps - 0.5) - 0.08, 0.0, 1.0)
        density = 0.95 * body ** 1.3
        density *= np.clip((0.97 - r) / 0.2, 0.0, 1.0)
        base = np.clip(falloff, 0.0, 1.0)
        # The surface's slope: the body's dome with the billows on it and the noise's broad lumps, smoothed.
        surface = blur(0.45 * falloff + 0.25 * domes + 0.15 * broad, 12)
        gy, gx = np.gradient(surface, 2.0 / CELL)
        strength = 0.5
        nx, ny = -gx * strength, -gy * strength
        length = np.sqrt(nx * nx + ny * ny + 1.0)
        nx, ny = nx / length, ny / length
        row, column = divmod(cell, COLUMNS)
        block = atlas[row * CELL:(row + 1) * CELL, column * CELL:(column + 1) * CELL]
        block[..., 0] = np.round(density * 255).astype(np.uint8)
        block[..., 1] = np.round((nx * 0.5 + 0.5) * 255).astype(np.uint8)
        block[..., 2] = np.round((ny * 0.5 + 0.5) * 255).astype(np.uint8)
        block[..., 3] = np.round(np.clip(base, 0.0, 1.0) * 255).astype(np.uint8)
        print(f"puff {cell}: covers {float((density > 0.05).mean()):.2f} of its cell, whole over {float((density > 0.95).mean()):.2f}")
    os.makedirs(SAVED, exist_ok=True)
    write_png(os.path.join(SAVED, PUFFS), atlas)
    print("wrote " + os.path.join(SAVED, PUFFS))


if len(sys.argv) > 1 and sys.argv[1] == "prepare":
    prepare()
    sys.exit(0)

import unreal
OUT = unreal.Paths.project_saved_dir() + "smoke_cloud.txt"
FOLDER = "/Game/CheeseStrike/Effects"
lines = []
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Linear: a light smoke grey, a touch cool (the arenas' warm light and floors tint it: any warmer reads as dust).
SMOKE_COLOUR = unreal.LinearColor(0.5, 0.5, 0.51, 1.0)
# How far a card fades as it nears what is behind it (cm): a solid one of the veil, and a swirling one (only where it
# meets the floor or someone, so that it never shows a hard line there; a swirling card is never within this of anyone
# in the core, which the veil is a metre before).
SOFT_DISTANCE = 60.0
SWIRL_SOFT_DISTANCE = 25.0
# The light the billows are shaded by: from the sky, a little to one side (world space).
SKY_LIGHT = unreal.LinearColor(0.3, 0.2, 0.93, 0.0)

SMOKE = r"""
float2 p = UV * 2.0 - 1.0;
float r = length(p);
// One of the atlas's eight puffs, by the card's own random.
float cell = floor(saturate(Pick) * 7.999);
float2 at = (UV + float2(fmod(cell, 4.0), floor(cell / 4.0))) / float2(4.0, 2.0);
float4 s = Texture2DSample(Tex, TexSampler, at);
float density = s.r;
// A solid card is whole to 0.6 of its radius (CheeseStrikeGrenades::CloudVeilWhole) and frays past it along the puff's
// own shape, out further where the puff is thick, into its wisps; a swirling one is the puff as it is.
float solid = saturate(Solid);
float core = saturate((0.98 - (r + 0.3 * (0.5 - density))) / 0.23);
float body = lerp(saturate(density * 1.4), saturate(max(density * 2.5, core)), solid);
// Lit from the sky through the billows' slopes: bright on their tops and the sides toward the light, darker in their
// folds and underneath, the thin wisps a little lighter as the light comes through them.
// (The texture's y runs down the card, its tangent space's up it.)
float3 n = float3(s.g * 2.0 - 1.0, 1.0 - s.b * 2.0, 0.0);
n.z = sqrt(saturate(1.0 - dot(n.xy, n.xy)));
float lit = saturate(dot(n, normalize(Light)));
float tone = (0.55 + 0.55 * lit) * lerp(1.12, 0.92, s.a);
// A solid card's whole middle (which fills the view from inside the cloud) stirs slowly through the world, in swirls a
// metre or so across, lighter above the eye and darker below, so that it reads as a body of smoke, not a flat grey.
float3 q = P * 0.012;
float stir = sin(q.x * 1.3 + q.y * 0.7 + T * 0.25) * sin(q.y * 1.1 - q.z * 0.9 - T * 0.2) + 0.6 * sin(q.x * 2.9 - q.z * 2.3 + q.y * 1.7 + T * 0.35) + 0.35 * sin(q.y * 5.3 + q.z * 4.1 - q.x * 3.7 - T * 0.45);
float above = clamp((P.z - Cam.z) / 150.0, -1.0, 1.0);
tone *= 1.0 + solid * core * (0.16 * stir + 0.14 * above);
Colour = Base * Shade * tone;
return saturate(body * Fade) * lerp(SwirlSoft, Soft, solid);
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


def material(name):
    path = FOLDER + "/" + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    return m, path


def import_puffs():
    path = FOLDER + "/T_SmokePuffs"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SAVED, PUFFS))
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", "T_SmokePuffs")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(path)
    if texture is None:
        raise RuntimeError("nothing imported from " + os.path.join(SAVED, PUFFS))
    # Data, not a colour: no sRGB; its four channels kept apart (BC7).
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    # Whole from the first frame a cloud shows (streamed, its first seconds were a blur of its smallest mips).
    texture.set_editor_property("never_stream", True)
    lines.append("T_SmokePuffs saved " + str(lib.save_asset(path, False)))
    return texture


def build_smoke(puffs):
    m, path = material("M_SmokeMist")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    # Writing its depth (for the temporal upscaler) would stop it reading the depth its soft edge needs.
    m.set_editor_property("output_translucent_velocity", False)
    code = node(m, unreal.MaterialExpressionCustom, -450, 100, code=SMOKE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Smoke")
    code.set_editor_property("inputs", [custom_input(n) for n in ["UV", "Fade", "Solid", "Shade", "Soft", "SwirlSoft", "Pick", "Light", "Base", "T", "P", "Cam", "Tex"]])
    code.set_editor_property("additional_outputs", [custom_output("Colour", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    mel.recompile_material(m)
    link(node(m, unreal.MaterialExpressionTextureCoordinate, -1000, -200), code, "UV")
    link(node(m, unreal.MaterialExpressionPerInstanceCustomData, -1000, -100, data_index=0), code, "Fade")
    link(node(m, unreal.MaterialExpressionPerInstanceCustomData, -1000, 0, data_index=1), code, "Solid")
    link(node(m, unreal.MaterialExpressionPerInstanceCustomData, -1000, 100, data_index=2, const_default_value=1.0), code, "Shade")
    link(node(m, unreal.MaterialExpressionDepthFade, -1000, 200, fade_distance_default=SOFT_DISTANCE), code, "Soft")
    link(node(m, unreal.MaterialExpressionDepthFade, -1000, 300, fade_distance_default=SWIRL_SOFT_DISTANCE), code, "SwirlSoft")
    link(node(m, unreal.MaterialExpressionPerInstanceRandom, -1000, 400), code, "Pick")
    light = node(m, unreal.MaterialExpressionTransform, -1000, 500, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    link(node(m, unreal.MaterialExpressionConstant3Vector, -1250, 500, constant=SKY_LIGHT), light)
    link(light, code, "Light")
    link(node(m, unreal.MaterialExpressionVectorParameter, -1000, 600, parameter_name="Colour", default_value=SMOKE_COLOUR), code, "Base")
    link(node(m, unreal.MaterialExpressionTime, -1000, 650), code, "T")
    link(node(m, unreal.MaterialExpressionWorldPosition, -1250, 650), code, "P")
    link(node(m, unreal.MaterialExpressionCameraPositionWS, -1250, 750), code, "Cam")
    link(node(m, unreal.MaterialExpressionTextureObject, -1000, 700, texture=puffs), code, "Tex")
    mel.connect_material_property(code, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(code, "Colour", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -450, 300, r=1.0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # A little light of its own, as the sky's scattered through it, so that the floor's bounce doesn't brown it.
    glow = node(m, unreal.MaterialExpressionMultiply, -200, 200)
    link(code, glow, "A", "Colour")
    link(node(m, unreal.MaterialExpressionConstant, -450, 400, r=0.18), glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lines.append("M_SmokeMist saved " + str(lib.save_asset(path, False)))


try:
    build_smoke(import_puffs())
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
