"""Prepares the weapons' textures from the Fab downloads, outside the editor
(plain Python with Pillow): each model's colour and normal maps cut to
2048 pixels, its ambient occlusion, roughness and metalness packed into
one AORM map (red, green, blue) as Lyra's weapons have them, and a cheese
mask, which the weapons' material (tools/art/weapon_materials.py) turns
into cheese: red where the cheese goes (the wooden or green stocks, found
by their colour), green where it has a rind or wax.

    python tools/art/weapon_textures.py

Reads C:\\Hamid\\code\\FabDownloads (CHEESESTRIKE_FAB to change it) and
writes into its x/weapons folder, where tools/art/import_weapons.py looks.
"""

import colorsys
import os
import sys

from PIL import Image, ImageFilter

FAB = os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads")
OUT = os.path.join(FAB, "x", "weapons")
SIZE = 2048
MASK_SIZE = 1024

# Each model: its name in the game's assets, and its maps in the download: colour, normal, AO (or None),
# roughness, metalness; and which colours are its cheese: a test on (hue in degrees, saturation, value).
WOOD = lambda h, s, v: (h < 30 or h > 345) and s > 0.35 and v > 0.12
GREEN = lambda h, s, v: 60 < h < 170 and s > 0.25 and v > 0.08
MODELS = [
    ("PPSh41", "Max3d_PPSh41/textures/PPSH41_Albedo.png", "Max3d_PPSh41/textures/PPSH41_Normal.png", "Max3d_PPSh41/textures/PPSH41_AO.png",
     "Max3d_PPSh41/textures/PPSH41_Roughness.png", "Max3d_PPSh41/textures/PPSH41_Metallic.png", WOOD),
    ("DP28", "Max3d_DP28/textures/Dp28_albedo.png", "Max3d_DP28/textures/Dp28_normal.png", "Max3d_DP28/textures/Dp28_ao.png",
     "Max3d_DP28/textures/Dp_28roughness.png", "Max3d_DP28/textures/Dp28_metalness.png", WOOD),
    ("SV98", "SpatialNeglect_SV98/textures/sv_98_SV_98.002_BaseColor.png", "SpatialNeglect_SV98/textures/sv_98_SV_98.002_Normal.png", None,
     "SpatialNeglect_SV98/textures/sv_98_SV_98.002_Roughness.png", "SpatialNeglect_SV98/textures/sv_98_SV_98.002_Metallic.png", GREEN),
    ("SV98Scope", "SpatialNeglect_SV98/textures/sv_98_SCOPE_BaseColor.png", "SpatialNeglect_SV98/textures/sv_98_SCOPE_Normal.png", None,
     "SpatialNeglect_SV98/textures/sv_98_SCOPE_Roughness.png", "SpatialNeglect_SV98/textures/sv_98_SCOPE_Metallic.png", None),
    ("ClassicM4", "SpatialNeglect_ClassicM4/textures/m4_Base_color.png", "SpatialNeglect_ClassicM4/textures/m4_Normal_OpenGL.png", None,
     "SpatialNeglect_ClassicM4/textures/m4_Roughness.png", "SpatialNeglect_ClassicM4/textures/m4_Metallic.png", None),
    ("ClassicM4Scope", "SpatialNeglect_ClassicM4/textures/scope_Base_color.png", "SpatialNeglect_ClassicM4/textures/scope_Normal_OpenGL.png", None,
     "SpatialNeglect_ClassicM4/textures/scope_Roughness.png", None, None),
]


def grey(path, size):
    if path is None:
        return None
    return Image.open(os.path.join(FAB, path)).convert("L").resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, colour, normal, ao, rough, metal, cheese in MODELS:
        base = Image.open(os.path.join(FAB, colour)).convert("RGB").resize((SIZE, SIZE), Image.LANCZOS)
        base.save(os.path.join(OUT, f"T_{name}_Albedo.png"))
        Image.open(os.path.join(FAB, normal)).convert("RGB").resize((SIZE, SIZE), Image.LANCZOS).save(os.path.join(OUT, f"T_{name}_Normal.png"))
        white = Image.new("L", (SIZE, SIZE), 255)
        black = Image.new("L", (SIZE, SIZE), 0)
        aorm = Image.merge("RGB", (grey(ao, SIZE) or white, grey(rough, SIZE) or Image.new("L", (SIZE, SIZE), 128), grey(metal, SIZE) or black))
        aorm.save(os.path.join(OUT, f"T_{name}_AORM.png"))
        if cheese is not None:
            small = base.resize((MASK_SIZE, MASK_SIZE), Image.LANCZOS)
            mask = Image.new("L", (MASK_SIZE, MASK_SIZE), 0)
            pixels = small.load()
            out = mask.load()
            for y in range(MASK_SIZE):
                for x in range(MASK_SIZE):
                    r, g, b = pixels[x, y]
                    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
                    out[x, y] = 255 if cheese(h * 360.0, s, v) else 0
            # Whole parts, not speckles: closed up, then softened at the edges.
            mask = mask.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5)).filter(ImageFilter.GaussianBlur(1.5))
            Image.merge("RGB", (mask, black.resize((MASK_SIZE, MASK_SIZE)), black.resize((MASK_SIZE, MASK_SIZE)))).save(os.path.join(OUT, f"T_{name}_Cheese.png"))
        print(name, "done")


if __name__ == "__main__":
    sys.exit(main())
