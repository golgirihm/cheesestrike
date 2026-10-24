"""The menus' background tile: a cut face of Swiss cheese, dark, so that the
lobby and the loading cover read over it.

Run with any Python that has Pillow (no editor needed):
    python tools/art/menu_tile.py
It writes Content/UI/lobby_tile.png, which the menu (SCheeseStrikeMenu), the
loading cover (UCheeseStrikeCurtainSubsystem) and the held results
(UCheeseStrikePostGameSubsystem) tile across the screen, MenuTileSize Slate
units to a tile.

The holes are round, of many sizes, never touching one another (each kept a
gap from the rest, across the tile's edges too, so that the tile repeats
without a seam), as the eyes of an Emmental or a Gruyère cut through: a
smooth waxy face with a little grain, each hole a hollow shaded as a light
from above would leave it, its far wall lit and a soft lip where it was cut.
"""
import math, os, random
from PIL import Image, ImageDraw, ImageFilter

SIZE = 512
# Drawn this many times bigger, then shrunk, for smooth edges.
SUPER = 4
SEED = 11
# The face's colour and its grain, and the holes' colours, as sRGB.
FACE = (46, 35, 17)
GRAIN = 3
DEEP = (19, 14, 7)
WALL = (62, 48, 24)
LIP = (56, 43, 21)
# The holes' radii (pixels of the tile), how many of each size are tried for, and the least gap between two holes' edges.
SIZES = [(58, 1), (46, 1), (38, 2), (30, 2), (23, 3), (17, 4), (12, 5), (8, 5), (5, 4)]
GAP = 24
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "Content", "UI", "lobby_tile.png")


def wrapped(a, b):
    """The distance between two points on the tile repeated both ways."""
    dx = abs(a[0] - b[0]) % SIZE
    dy = abs(a[1] - b[1]) % SIZE
    return math.hypot(min(dx, SIZE - dx), min(dy, SIZE - dy))


def place():
    """Holes (x, y, radius) that keep GAP apart, the biggest placed first."""
    rng = random.Random(SEED)
    holes = []
    for radius, count in SIZES:
        placed, tries = 0, 0
        while placed < count and tries < 4000:
            tries += 1
            at = (rng.uniform(0, SIZE), rng.uniform(0, SIZE))
            # Not quite every hole the same size: a cheese's eyes vary.
            radius_here = radius * rng.uniform(0.85, 1.15)
            if all(wrapped(at, (x, y)) >= radius_here + r + GAP for x, y, r in holes):
                holes.append((at[0], at[1], radius_here))
                placed += 1
    return holes


def main():
    rng = random.Random(SEED + 1)
    big = SIZE * SUPER
    face = Image.new("RGB", (big, big), FACE)
    # The grain: faint specks, blurred so that they read as a waxy face, not noise.
    grain = Image.new("L", (SIZE, SIZE), 128)
    pixels = grain.load()
    for y in range(SIZE):
        for x in range(SIZE):
            pixels[x, y] = 128 + rng.randint(-GRAIN, GRAIN)
    grain = grain.filter(ImageFilter.GaussianBlur(1.2)).resize((big, big), Image.BICUBIC)
    shaded = Image.new("RGB", (big, big))
    base = face.load()
    noise = grain.load()
    out = shaded.load()
    for y in range(big):
        for x in range(big):
            d = noise[x, y] - 128
            r, g, b = base[x, y]
            out[x, y] = (r + d, g + d, b + d // 2)
    draw = ImageDraw.Draw(shaded)
    holes = place()
    for hx, hy, radius in holes:
        # Each hole and its copies over the tile's edges, so that it repeats without a seam.
        for ox in (-SIZE, 0, SIZE):
            for oy in (-SIZE, 0, SIZE):
                cx, cy = (hx + ox) * SUPER, (hy + oy) * SUPER
                r = radius * SUPER
                if cx + r * 1.2 < 0 or cy + r * 1.2 < 0 or cx - r * 1.2 > big or cy - r * 1.2 > big:
                    continue
                lip = max(SUPER, int(r * 0.08))
                draw.ellipse((cx - r - lip, cy - r - lip, cx + r + lip, cy + r + lip), fill=LIP)
                # The far wall, lit from above, shows at the hole's bottom; the near wall's shadow fills the rest, deepest at the top.
                draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=WALL)
                steps = 12
                for step in range(steps):
                    t = step / (steps - 1)
                    rr = r * (1.0 - 0.18 * t)
                    lift = r * 0.30 * t
                    colour = tuple(int(WALL[i] + (DEEP[i] - WALL[i]) * (0.35 + 0.65 * t)) for i in range(3))
                    draw.ellipse((cx - rr, cy - rr - lift, cx + rr, cy + rr - lift), fill=colour)
    tile = shaded.resize((SIZE, SIZE), Image.LANCZOS)
    tile.save(os.path.abspath(OUT))
    print(f"{len(holes)} holes, written to {os.path.abspath(OUT)}")


if __name__ == "__main__":
    main()
