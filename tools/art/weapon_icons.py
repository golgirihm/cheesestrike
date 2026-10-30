"""Draws each weapon's icon for the HUD: its silhouette from the side,
facing left (towards the middle of the screen, as the HUD's weapon panel
has it), white on clear, from its model exported as OBJ. Plain Python with
Pillow; tools/art/import_weapons.py exports the models and imports the
icons.

    python tools/art/weapon_icons.py <folder of OBJs> <out folder>

Unreal's OBJ exporter writes each vertex as (X, Z, Y): the game's forward
(Y) is the third number, up (Z) the second. The cheese weapons' OBJs, which
tools/art/cheese_weapons.py writes from Blender, are (X, -Y, Z).
"""

import os
import sys

from PIL import Image, ImageDraw

WIDTH, HEIGHT = 256, 96
MARGIN = 6
SUPER = 4
# Weapon id: its OBJ, which way its muzzle points along Y (the knife and the stick point back), and whose OBJ it is.
ICONS = {
    "blaster": ("cheese_blaster", 1.0, "ours"),
    "carbine": ("cheese_carbine", 1.0, "ours"),
    "grater": ("cheese_grater", 1.0, "ours"),
    "munster": ("cheese_munster", 1.0, "ours"),
    "shotgun": ("cheese_shotgun", 1.0, "ours"),
    "roquefort": ("cheese_roquefort", 1.0, "ours"),
    "pistol": ("cheese_pistol", 1.0, "ours"),
    "babybomb": ("cheese_babybomb", 1.0, "ours"),
    "stick": ("cheese_stick", -1.0, "ours"),
    "gatling": ("cheese_gatling", 1.0, "ours"),
    # Two M1911s back to back, the one in each hand.
    "burratas": ("SM_M1911", 1.0, "unreal"),
    "knife": ("SM_CheeseKnife", -1.0, "unreal"),
}


def load(path):
    vertices, faces = [], []
    for line in open(path):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            vertices.append(tuple(map(float, parts[1:4])))
        elif parts[0] == "f":
            faces.append([int(p.split("/")[0]) - 1 for p in parts[1:]])
    return vertices, faces


# Those drawn twice, back to back, for a weapon in each hand.
DUAL = {"burratas", "munster"}


def icon(path, forward, whose, width=WIDTH):
    vertices, faces = load(path)
    # Side on: forward (Y) across, muzzle to the left; up (Z) up.
    if whose == "ours":
        points = [(forward * v[1], v[2]) for v in vertices]
    else:
        points = [(-forward * v[2], v[1]) for v in vertices]
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    scale = min((width - 2 * MARGIN) / (x1 - x0), (HEIGHT - 2 * MARGIN) / (y1 - y0)) * SUPER
    w, h = width * SUPER, HEIGHT * SUPER
    ox = (w - (x1 - x0) * scale) / 2.0
    oy = (h - (y1 - y0) * scale) / 2.0
    mask = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(mask)
    for face in faces:
        draw.polygon([(ox + (points[i][0] - x0) * scale, oy + (y1 - points[i][1]) * scale) for i in face], fill=255)
    mask = mask.resize((width, HEIGHT), Image.LANCZOS)
    out = Image.new("RGBA", (width, HEIGHT), (255, 255, 255, 0))
    out.putalpha(mask)
    return out


def main():
    source, target = sys.argv[1], sys.argv[2]
    os.makedirs(target, exist_ok=True)
    for weapon, (name, forward, whose) in ICONS.items():
        path = os.path.join(source, name + ".obj")
        if not os.path.exists(path):
            print(weapon, "has no model at", path)
            continue
        if weapon in DUAL:
            # The left hand's facing left, the right hand's its mirror, side by side.
            half = icon(path, forward, whose, WIDTH // 2)
            both = Image.new("RGBA", (WIDTH, HEIGHT), (255, 255, 255, 0))
            both.paste(half, (0, 0))
            both.paste(half.transpose(Image.FLIP_LEFT_RIGHT), (WIDTH // 2, 0))
            both.save(os.path.join(target, f"T_Icon_{weapon}.png"))
        else:
            icon(path, forward, whose).save(os.path.join(target, f"T_Icon_{weapon}.png"))
        print(weapon, "drawn")


if __name__ == "__main__":
    main()
