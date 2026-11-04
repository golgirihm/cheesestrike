"""Compares how sharp the front end's figure is on the first frame after a
change of hero and thirty frames later (tools/play/hero_switch.txt's
screenshots): a hero's textures kept resident look the same at once as later,
one still streaming in blurry at first. Sharpness is the variance of the
Laplacian of the figure's crop in grey, the usual measure; a pair within 10%
of each other passes.

Run with any Python that has Pillow (no numpy needed):
    python -I tools/art/figure_sharpness.py [Saved/Screenshots/WindowsEditor] [--crop x0 y0 x1 y1]

The crop is where the figure stands in a 960 x 540 screenshot by default
(the pedestal in the middle of the view); give another for another size.
"""

import argparse
import os
import sys

from PIL import Image, ImageFilter, ImageStat

HEROES = ["1_murdock", "2_wraith", "3_twinblast", "4_belica"]
DEFAULT_CROP = (380, 110, 580, 480)


def sharpness(path, crop):
    """The variance of the Laplacian of the crop in grey: higher is sharper."""
    image = Image.open(path).convert("L").crop(crop)
    laplacian = image.filter(ImageFilter.Kernel((3, 3), [0, 1, 0, 1, -4, 1, 0, 1, 0], scale=1, offset=128))
    return ImageStat.Stat(laplacian).var[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("folder", nargs="?", default=os.path.join("Saved", "Screenshots", "WindowsEditor"))
    parser.add_argument("--crop", nargs=4, type=int, default=list(DEFAULT_CROP), metavar=("X0", "Y0", "X1", "Y1"))
    parser.add_argument("--within", type=float, default=0.10, help="the share the two may differ by (0.10 by default)")
    arguments = parser.parse_args()
    crop = tuple(arguments.crop)
    failed = 0
    for hero in HEROES:
        first = os.path.join(arguments.folder, f"switch_{hero}_first.png")
        later = os.path.join(arguments.folder, f"switch_{hero}_30.png")
        if not (os.path.exists(first) and os.path.exists(later)):
            print(f"{hero}: no screenshots ({first}, {later})")
            failed += 1
            continue
        a = sharpness(first, crop)
        b = sharpness(later, crop)
        apart = abs(a - b) / max(a, b, 1e-6)
        ok = apart <= arguments.within
        failed += 0 if ok else 1
        print(f"{hero}: first frame {a:.1f}, thirty frames later {b:.1f}, {apart * 100:.1f}% apart: {'ok' if ok else 'NOT within ' + str(int(arguments.within * 100)) + '%'}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
