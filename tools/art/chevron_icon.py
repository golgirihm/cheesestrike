"""The menu's one chevron: a white V pointing down, drawn anti-aliased into
Content/UI/chevron.png, which the menu turns by a render transform to point
right (a folded section), left (the friends tab) or up (a dropdown open):
SCheeseStrikeChevron in UI/CheeseStrikeMenu.cpp.

Run with any Python that has Pillow (no editor needed):
    python -I tools/art/chevron_icon.py

The picture is 32 pixels square, shown 16 Slate units across, so that it is
still sharp when the menu is scaled up for a 1440p or 4K screen: the V spans
12 of the 16 units, 6 high, its stroke 2.4 units with round ends, drawn eight
times over and shrunk, so that its edges are smooth.
"""
import os
from PIL import Image, ImageDraw

SIZE = 32
SUPER = 8
# In the 16-unit box the menu shows it in: the V's two ends and its point, and its stroke.
SPAN = 12.0
RISE = 6.0
STROKE = 2.4
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "Content", "UI", "chevron.png")


def main():
    pixels = SIZE * SUPER
    unit = pixels / 16.0
    # The V as a coverage mask, white everywhere with the mask as its alpha: an edge pixel is white half seen, never
    # a grey one (shrinking a picture with its colour and alpha together would darken the edges).
    mask = Image.new("L", (pixels, pixels), 0)
    draw = ImageDraw.Draw(mask)
    middle = pixels / 2.0
    left = (middle - SPAN / 2.0 * unit, middle - RISE / 2.0 * unit)
    point = (middle, middle + RISE / 2.0 * unit)
    right = (middle + SPAN / 2.0 * unit, middle - RISE / 2.0 * unit)
    width = STROKE * unit
    draw.line([left, point, right], fill=255, width=int(round(width)), joint="curve")
    # Round ends: a disc on each end and on the point, so that no corner is square.
    for x, y in (left, point, right):
        draw.ellipse([x - width / 2.0, y - width / 2.0, x + width / 2.0, y + width / 2.0], fill=255)
    alpha = mask.resize((SIZE, SIZE), Image.LANCZOS)
    small = Image.new("RGBA", (SIZE, SIZE), (255, 255, 255, 255))
    small.putalpha(alpha)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    small.save(OUT)
    print(f"{os.path.normpath(OUT)}: {SIZE} x {SIZE}, the V {SPAN} of 16 across")


if __name__ == "__main__":
    main()
