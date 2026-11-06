"""Counts, in still views taken a frame or two apart (tools/play/block_flicker.txt), the frames where a sizeable patch
differs from the frame before: with nothing moving, that is flicker (a block culled for a frame, faces z-fighting).
The minimap and the HUD's foot are left out; a weapon turning on its stand changes every frame, so a view with one in
it says so in every frame.

    python tools/art/flicker_frames.py flicker

for the screenshots Saved/Screenshots/WindowsEditor/flicker<view>_<frame>.png. Needs Pillow.
"""

import glob
import pathlib
import re
import sys

from PIL import Image, ImageChops

SHOTS = pathlib.Path(__file__).resolve().parent.parent.parent / "Saved" / "Screenshots" / "WindowsEditor"
CHANGED = 30        # a pixel's grey level changing by more than this has changed
PATCH = 150         # pixels changed in a frame that make it a change worth naming


def main(prefix):
    views = {}
    for path in glob.glob(str(SHOTS / f"{prefix}*.png")):
        found = re.match(r"(.*)_(\d+)\.png$", pathlib.Path(path).name)
        if found:
            views.setdefault(found.group(1), []).append((int(found.group(2)), path))
    flickers = 0
    for view, frames in sorted(views.items()):
        frames.sort()
        pictures = [Image.open(path).convert("L") for _, path in frames]
        pictures = [p.crop((p.width * 150 // 960, 0, p.width, p.height * 480 // 540)) for p in pictures]
        changes = []
        for index in range(1, len(pictures)):
            changed = ImageChops.difference(pictures[index - 1], pictures[index]).point(lambda v: 255 if v > CHANGED else 0)
            count = changed.histogram()[255]
            if count > PATCH:
                changes.append((index, count))
        flickers += len(changes)
        print(f"{view}: {len(pictures)} frames, {len(changes)} with a change: {changes}")
    return flickers


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "flicker")
