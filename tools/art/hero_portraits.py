"""Makes the loadout page's portraits of the four heroes (Content/UI/Heroes/<hero>.png,
256 x 256): plays tools/play/hero_portraits.txt through tools/play.ps1 at
1280 x 720 (the editor must be closed), which shows each hero in its own
colours from in front and a little to the side with the view on its head and
shoulders (CheeseStrikeBody <n> -1, CheeseStrikeOutside), then crops each
screenshot's head and shoulders square and scales it down with Pillow.

    python -I tools/art/hero_portraits.py            play, then crop
    python -I tools/art/hero_portraits.py --crop-only  crop the screenshots already in Saved/Screenshots

The crop (--crop x0 y0 x1 y1, in the 1280 x 720 screenshot) is where the
head and shoulders are with the script's camera; change it with the camera.
"""

import argparse
import os
import pathlib
import subprocess
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
SHOTS = ROOT / "Saved" / "Screenshots" / "WindowsEditor"
OUT = ROOT / "Content" / "UI" / "Heroes"
# CheeseStrikeBodies::All's order, by the words the settings keep them by (CheeseStrikeBodies::HeroKey).
HEROES = ["belica", "murdock", "wraith", "twinblast"]
DEFAULT_CROP = (430, 50, 850, 470)
SIZE = 256


def play():
    command = ["powershell", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools" / "play.ps1"), "-Script", str(ROOT / "tools" / "play" / "hero_portraits.txt"),
               "-Tag", "hero_portraits", "-Res", "1280x720"]
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def crop(box):
    OUT.mkdir(parents=True, exist_ok=True)
    for index, hero in enumerate(HEROES):
        shot = SHOTS / f"portrait_{index}.png"
        if not shot.exists():
            print(f"{hero}: no screenshot at {shot}")
            return False
        image = Image.open(shot).convert("RGB").crop(box).resize((SIZE, SIZE), Image.LANCZOS)
        target = OUT / f"{hero}.png"
        image.save(target, optimize=True)
        print(f"{hero}: {target} ({SIZE} x {SIZE} from {box})")
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--crop-only", action="store_true", help="crop the screenshots already taken, without playing")
    parser.add_argument("--crop", nargs=4, type=int, default=list(DEFAULT_CROP), metavar=("X0", "Y0", "X1", "Y1"))
    arguments = parser.parse_args()
    if not arguments.crop_only:
        play()
    sys.exit(0 if crop(tuple(arguments.crop)) else 1)


if __name__ == "__main__":
    main()
