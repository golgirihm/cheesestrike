"""The friends panel's refresh glyph: a white circular arrow, drawn
anti-aliased into Content/UI/refresh.png, which the menu shows in a rounded
square button by FRIENDS and turns once after each refresh
(SCheeseStrikeMenu::MakeRefreshButton, SCheeseStrikeTurnedImage in
UI/CheeseStrikeMenu.cpp).

Run with any Python that has Pillow (no editor needed):
    python -I tools/art/refresh_icon.py

The picture is 48 pixels square, shown 22 Slate units across (RefreshGlyphSize),
so that it is still sharp when the menu is scaled up for a 1440p or 4K screen:
drawn on a grid of 18 units square: a ring 13.6 of them across, open over a fifth of its turn at the top
right, its stroke 2.4 units with a round end where it starts, and at its other
end a solid arrowhead pointing on round the way the glyph turns (clockwise),
drawn eight times over and shrunk, so that its edges are smooth. Its middle is
the picture's, so that it turns about itself.
"""
import math
import os

from PIL import Image, ImageDraw

SIZE = 48
SUPER = 8
UNITS = 18.0
# On the picture's grid of 18 units: the ring's radius (to the middle of its stroke) and its stroke; where the arc
# begins and ends (degrees clockwise from three o'clock, as PIL measures them); and the arrowhead's length and half width.
RADIUS = 6.8
STROKE = 2.4
START = 20.0
END = 290.0
HEAD_LENGTH = 3.8
HEAD_HALF = 3.2
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "Content", "UI", "refresh.png")


def main():
    pixels = SIZE * SUPER
    unit = pixels / UNITS
    middle = pixels / 2.0
    radius = RADIUS * unit
    width = STROKE * unit
    # The glyph as a coverage mask, white everywhere with the mask as its alpha (an edge pixel is white half seen, never a
    # grey one).
    mask = Image.new("L", (pixels, pixels), 0)
    draw = ImageDraw.Draw(mask)
    outer = radius + width / 2.0
    # The arc (PIL draws its width inward from the box), ending a little short of the arrowhead's base, which covers its end.
    draw.arc([middle - outer, middle - outer, middle + outer, middle + outer], START, END - 4.0, fill=255, width=int(round(width)))
    # A round end where it starts.
    a = math.radians(START)
    x, y = middle + radius * math.cos(a), middle + radius * math.sin(a)
    draw.ellipse([x - width / 2.0, y - width / 2.0, x + width / 2.0, y + width / 2.0], fill=255)
    # The arrowhead at the end, along the way round (clockwise: the angle growing, y down), its base across the ring.
    a = math.radians(END)
    base = (middle + radius * math.cos(a), middle + radius * math.sin(a))
    along = (-math.sin(a), math.cos(a))
    across = (math.cos(a), math.sin(a))
    head = HEAD_LENGTH * unit
    half = HEAD_HALF * unit
    tip = (base[0] + along[0] * head, base[1] + along[1] * head)
    left = (base[0] + across[0] * half, base[1] + across[1] * half)
    right = (base[0] - across[0] * half, base[1] - across[1] * half)
    draw.polygon([tip, left, right], fill=255)
    alpha = mask.resize((SIZE, SIZE), Image.LANCZOS)
    small = Image.new("RGBA", (SIZE, SIZE), (255, 255, 255, 255))
    small.putalpha(alpha)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    small.save(OUT)
    print(f"{os.path.normpath(OUT)}: {SIZE} x {SIZE}, the ring {2 * RADIUS:.0f} of {UNITS:.0f} across")


if __name__ == "__main__":
    main()
