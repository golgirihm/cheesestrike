"""Downloads the Poly Haven textures and models the maps use (CC0: https://polyhaven.com/license), into
C:\\Hamid\\code\\FabDownloads\\polyhaven\\<asset>\\<asset>_<map>.<jpg or png> (set CHEESESTRIKE_FAB to put them
elsewhere), for import_polyhaven.py to import. Run with ordinary Python, outside the editor:

    python tools/art/polyhaven.py

Each asset's files come from Poly Haven's public API (api.polyhaven.com/files/<asset>): a texture's base colour,
OpenGL normal (green up, as Megascans' are; import_polyhaven.py turns it into Unreal's), roughness and ambient
occlusion as JPEGs at its resolution; a model's FBX with the same maps (and its metalness and opacity where it has
them). What is downloaded already is kept. A map that uses Poly Haven adds its assets here and in
import_polyhaven.py."""

import json
import os
import sys
import urllib.request

FAB = os.environ.get("CHEESESTRIKE_FAB", r"C:\Hamid\code\FabDownloads")
OUT = os.path.join(FAB, "polyhaven")
API = "https://api.polyhaven.com/files/"

# Surfaces, drawn from world space on the maps' blocks: (asset, resolution).
TEXTURES = [
    ("medieval_blocks_03", "2k"),      # The Labyrinth's dressed stone: its pillars, its vault, its walls
    ("medieval_red_brick", "2k"),      # its ceiling's vaults
    ("monastery_stone_floor", "2k"),   # its floor's flagstones
    ("old_planks_02", "2k"),           # its gallery's deck, shelves, crates
    ("medieval_wood", "2k"),           # its doors: planks and iron studs
    ("patterned_cobblestone", "2k"),   # Rind Market's square: setts laid in fans
    ("cobblestone_square", "2k"),      # its streets and alleys
    ("painted_plaster_wall", "2k"),    # the town's outer houses, painted in its colours
    ("weathered_planks", "1k"),        # floors, decks, doors, stalls, carts
    ("rough_wood", "1k"),              # posts and beams
    ("clay_roof_tiles_02", "1k"),      # roofs
    ("sandstone_blocks_08", "1k"),     # the clock tower, the well house, the arcade's piers
    ("castle_wall_slates", "1k"),      # the town wall
    ("rough_linen", "1k"),             # the stalls' awnings, dyed
    ("oak_wood_planks", "2k"),         # Crumb Kitchen's worktop: varnished oak
    ("floor_tiles_06", "2k"),          # its floor: chequered marble tiles
    ("long_white_tiles", "1k"),        # its splashback
    ("white_plaster_02", "1k"),        # its walls and ceiling, painted
    ("oak_veneer_01", "1k"),           # its cupboards' doors and chairs, painted or not
    ("wood_table_001", "1k"),          # its table
    ("brown_leather", "1k"),           # its books' covers, dyed
    ("leafy_grass", "1k"),             # the garden through its window
    ("red_brick_03", "1k"),            # the garden wall
]
# Models: (asset, resolution).
MODELS = [
    ("street_lamp_01", "1k"),          # lamp posts round the square
    ("street_lamp_02", "1k"),          # lanterns on walls
    ("wine_barrel_01", "1k"),
    ("wooden_crate_01", "1k"),
    ("wicker_basket_01", "1k"),
    ("painted_wooden_bench", "1k"),
    # Crumb Kitchen's, five times life size.
    ("vintage_electric_kettle", "2k"),
    ("vintage_microwave", "2k"),
    ("electric_stove", "2k"),
    ("wooden_cutting_board", "2k"),
    ("brass_pan_01", "1k"),
    ("pot_enamel_01", "1k"),
    ("brass_pot_01", "1k"),
    ("jug_01", "1k"),
    ("tea_set_01", "1k"),
    ("wooden_bowl_01", "1k"),
    ("food_apple_01", "1k"),
    ("lemon", "1k"),
    ("bananas", "1k"),
    ("croissant", "1k"),
    ("hamburger_buns", "1k"),
    ("carrot_cake", "1k"),
    ("long_life_food", "1k"),
    ("yellow_onion", "1k"),
    ("bleach_bottle", "1k"),
    ("multi_cleaner_bottle", "1k"),
    ("all_purpose_cleaner", "1k"),
    ("wooden_bucket_01", "1k"),
    ("metal_trash_can", "1k"),
    ("modern_ceiling_lamp_01", "1k"),
]
# The maps a texture or a model has, by Poly Haven's name for them, and ours. A model with one material that names its
# maps after it ("01_diff", "01_nor_gl") has them by these endings instead.
MAPS = {"Diffuse": "BaseColor", "nor_gl": "Normal", "Rough": "Roughness", "AO": "AO", "Metal": "Metalness", "opacity": "Opacity"}
ENDINGS = {"_diff": "BaseColor", "_nor_gl": "Normal", "_rough": "Roughness", "_ao": "AO", "_metallic": "Metalness"}


def get(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": "CheeseStrike map tools"})
    with urllib.request.urlopen(request, timeout=120) as response, open(path + ".part", "wb") as f:
        f.write(response.read())
    os.replace(path + ".part", path)
    print("  " + os.path.basename(path))


def files(asset):
    request = urllib.request.Request(API + asset, headers={"User-Agent": "CheeseStrike map tools"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.loads(response.read().decode("utf-8"))


def maps(asset, listing, resolution):
    """The asset's texture maps, as JPEG (or PNG for opacity) files named <asset>_<our map name>.<ext>."""
    named = {theirs: ours for theirs, ours in MAPS.items() if theirs in listing}
    for key in listing:
        ours = next((o for e, o in ENDINGS.items() if key.lower().endswith(e)), None)
        if ours is not None and ours not in named.values():
            named[key] = ours
    for theirs, ours in named.items():
        if theirs not in listing or resolution not in listing[theirs]:
            continue
        formats = listing[theirs][resolution]
        kind = "png" if theirs == "opacity" else "jpg"
        if kind not in formats:
            kind = next((k for k in ("jpg", "png") if k in formats), None)
        if kind is None:
            continue
        get(formats[kind]["url"], os.path.join(OUT, asset, f"{asset}_{ours}.{kind}"))


def main():
    for asset, resolution in TEXTURES:
        print(asset)
        maps(asset, files(asset), resolution)
    for asset, resolution in MODELS:
        print(asset)
        listing = files(asset)
        maps(asset, listing, resolution)
        get(listing["fbx"][resolution]["fbx"]["url"], os.path.join(OUT, asset, f"{asset}.fbx"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
