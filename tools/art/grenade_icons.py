"""Draws the loadout page's pictures of the two grenades (Content/UI/Loadout/grenade_cheese.png
and grenade_smoke.png, 128 x 128 with a clear ground), as the HUD draws them
beside the ammunition (UI/CheeseStrikeGrenadeHUD.cpp): the cheese grenade, a
frag of yellow holed cheese with its grooves, a steel fuse on top, the spoon
down its right and the pin's ring on its left; and the Smoked Gouda, a dark
smoked wheel seen a little from above, its rind edge lighter and a paler cut
face, with a wisp of smoke curling up off it. Drawn four times over and
scaled down, so that the edges are smooth.

    python -I tools/art/grenade_icons.py
"""

import math
import pathlib

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
OUT = ROOT / "Content" / "UI" / "Loadout"
SIZE = 128
OVER = 4


def rgba(r, g, b, a=1.0):
    return (int(r * 255), int(g * 255), int(b * 255), int(a * 255))


CHEESE = rgba(1.0, 0.78, 0.18)
GROOVE = rgba(0.62, 0.4, 0.06)
HOLE = rgba(0.7, 0.46, 0.06)
STEEL = rgba(0.72, 0.72, 0.74)
RIND = rgba(0.3, 0.17, 0.07)
EDGE = rgba(0.5, 0.3, 0.12)
FACE = rgba(0.62, 0.42, 0.18)
SMOKE = rgba(0.78, 0.78, 0.8, 0.75)


def polygon(points):
    return [(x, y) for x, y in points]


def cheese(draw, middle, radius):
    """The HUD's cheese grenade, point for point."""
    mx, my = middle
    thin = max(1.0, radius * 0.12)
    wide = radius * 0.8
    egg = []
    for index in range(56):
        angle = 2.0 * math.pi * index / 56.0
        down = math.sin(angle)
        egg.append((mx + math.cos(angle) * wide, my + down * radius * (1.0 if down > 0.0 else 0.92)))
    top = (mx, my - radius * 0.88)
    # The ring on the left and the fuse's cap first, so that the egg's rim sits over them.
    ring = (top[0] - radius * 0.62, top[1] - radius * 0.28)
    draw.ellipse([ring[0] - radius * 0.32, ring[1] - radius * 0.32, ring[0] + radius * 0.32, ring[1] + radius * 0.32], outline=STEEL, width=int(thin))
    draw.rectangle([top[0] - radius * 0.3, top[1] - radius * 0.42, top[0] + radius * 0.3, top[1] + radius * 0.08], fill=STEEL)
    draw.rectangle([top[0] - radius * 0.38, top[1] - radius * 0.56, top[0] + radius * 0.38, top[1] - radius * 0.38], fill=STEEL)
    draw.polygon(polygon(egg), fill=CHEESE)
    for across in (-0.36, 0.36):
        draw.line([(mx + across * wide, my - radius * 0.72), (mx + across * wide * 1.15, my + radius * 0.74)], fill=GROOVE, width=int(thin))
    for down in (-0.3, 0.3):
        half = wide * math.sqrt(1.0 - down * down) * 0.9
        draw.line([(mx - half, my + down * radius), (mx + half, my + down * radius)], fill=GROOVE, width=int(thin))
    for (ox, oy, r) in ((-wide * 0.62, radius * 0.02, radius * 0.12), (wide * 0.05, radius * 0.56, radius * 0.1), (wide * 0.12, -radius * 0.5, radius * 0.09)):
        draw.ellipse([mx + ox - r, my + oy - r, mx + ox + r, my + oy + r], fill=HOLE)
    cap = (top[0] + radius * 0.3, top[1] - radius * 0.48)
    over = (mx + wide * 0.98, my - radius * 0.62)
    down_at = (mx + wide * 1.06, my + radius * 0.35)
    draw.line([cap, over], fill=STEEL, width=int(thin * 1.3))
    draw.line([over, down_at], fill=STEEL, width=int(thin * 1.3))


def smoke(draw, middle, radius):
    """The HUD's Smoked Gouda, point for point."""
    mx, my = middle
    thin = max(1.0, radius * 0.12)
    wide = radius * 0.92
    cx, cy = mx, my + radius * 0.12
    draw.ellipse([cx - wide, cy - radius * 0.72, cx + wide, cy + radius * 0.72], fill=RIND)
    # The lit edge: the lower half of the rim.
    draw.arc([cx - wide, cy - radius * 0.72, cx + wide, cy + radius * 0.72], start=160, end=380, fill=EDGE, width=int(thin * 0.8))
    draw.ellipse([cx - wide * 0.86, cy - radius * 0.14 - radius * 0.42, cx + wide * 0.86, cy - radius * 0.14 + radius * 0.42], fill=FACE)
    foot = (cx + radius * 0.1, cy - radius * 0.5)
    a = (foot[0] - radius * 0.22, foot[1] - radius * 0.38)
    b = (foot[0] + radius * 0.12, foot[1] - radius * 0.72)
    c = (foot[0] - radius * 0.08, foot[1] - radius * 1.02)
    draw.line([foot, a], fill=SMOKE, width=int(thin * 1.1))
    draw.line([a, b], fill=SMOKE, width=int(thin * 0.9))
    draw.line([b, c], fill=SMOKE, width=int(thin * 0.7))


def icon(name, draw_it):
    big = SIZE * OVER
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw_it(draw, (big / 2.0, big * 0.54), big * 0.36)
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / f"grenade_{name}.png"
    image.resize((SIZE, SIZE), Image.LANCZOS).save(target, optimize=True)
    print(f"{name}: {target}")


def main():
    icon("cheese", cheese)
    icon("smoke", smoke)


if __name__ == "__main__":
    main()
