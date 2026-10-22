"""Draws each weapon's icon for the HUD: its silhouette from the side,
facing left (towards the middle of the screen, as the HUD's weapon panel
has it), white on clear, from its model exported as OBJ. Plain Python with
Pillow; tools/art/import_weapons.py exports the models and imports the
icons.

    python tools/art/weapon_icons.py <folder of OBJs> <out folder>

Unreal's OBJ exporter writes each vertex as (X, Z, Y): the game's forward
(Y) is the third number, up (Z) the second.
"""

import os
import sys

from PIL import Image, ImageDraw

WIDTH, HEIGHT = 256, 96
MARGIN = 6
SUPER = 4
# Weapon id: its OBJ, and which way its muzzle points along Y (the knife came in pointing back).
ICONS = {
    "blaster": ("SM_Rifle", 1.0),
    "carbine": ("SM_ClassicM4", 1.0),
    "grater": ("SM_PPSh41", 1.0),
    "munster": ("SM_DP28", 1.0),
    "shotgun": ("SM_Shotgun", 1.0),
    "roquefort": ("SM_SV98", 1.0),
    "pistol": ("SM_Pistol", 1.0),
    "knife": ("SM_CheeseKnife", -1.0),
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


def icon(path, forward):
    vertices, faces = load(path)
    # Side on: forward (Y) across, muzzle to the left; up (Z) up.
    points = [(-forward * v[2], v[1]) for v in vertices]
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    scale = min((WIDTH - 2 * MARGIN) / (x1 - x0), (HEIGHT - 2 * MARGIN) / (y1 - y0)) * SUPER
    w, h = WIDTH * SUPER, HEIGHT * SUPER
    ox = (w - (x1 - x0) * scale) / 2.0
    oy = (h - (y1 - y0) * scale) / 2.0
    mask = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(mask)
    for face in faces:
        draw.polygon([(ox + (points[i][0] - x0) * scale, oy + (y1 - points[i][1]) * scale) for i in face], fill=255)
    mask = mask.resize((WIDTH, HEIGHT), Image.LANCZOS)
    out = Image.new("RGBA", (WIDTH, HEIGHT), (255, 255, 255, 0))
    out.putalpha(mask)
    return out


def main():
    source, target = sys.argv[1], sys.argv[2]
    os.makedirs(target, exist_ok=True)
    for weapon, (name, forward) in ICONS.items():
        path = os.path.join(source, name + ".obj")
        if not os.path.exists(path):
            print(weapon, "has no model at", path)
            continue
        icon(path, forward).save(os.path.join(target, f"T_Icon_{weapon}.png"))
        print(weapon, "drawn")


if __name__ == "__main__":
    main()
