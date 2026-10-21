"""Takes a map's picture for the lobby (Content/UI/Maps/<file>, which CheeseStrikeRules::Maps names): plays the map
with tools/play.ps1, looks from a view that shows it best, and cuts the screenshot to the lobby's 1040 by 585.

    python tools/art/map_picture.py <level> <file> <x> <y> <z> <pitch> <yaw> [--start x,y,z,yaw]

for example, The Creamery's:

    python tools/art/map_picture.py /Game/CheeseStrike/Maps/Lvl_Creamery creamery.png -7400 -5600 3600 -28 36

The view is a camera at (x, y, z) in centimetres, turned by pitch and yaw in degrees (CheeseStrikeViewFrom). The game
renders offscreen at 1920 by 1080, with no bots and no mice; the player starts at --start (a spot out of the view;
the first team start if not given). Close the editor first. Needs Pillow (pip install pillow)."""

import argparse
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
SIZE = (1040, 585)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("level")
    parser.add_argument("file")
    parser.add_argument("view", nargs=5, type=float, metavar="x y z pitch yaw")
    parser.add_argument("--start", default="", help="where the player starts, x,y,z,yaw")
    parser.add_argument("--wait", type=float, default=20.0, help="seconds to let the level load and its shaders compile")
    args = parser.parse_args()
    from PIL import Image

    name = "map_picture_" + pathlib.Path(args.file).stem
    script = ROOT / "Saved" / f"{name}.txt"
    script.parent.mkdir(parents=True, exist_ok=True)
    x, y, z, pitch, yaw = args.view
    # The HUD hidden (showhud), so that the picture is the map alone.
    script.write_text(f"wait {args.wait}\ncmd showhud\ncmd CheeseStrikeViewFrom {x} {y} {z} {pitch} {yaw}\nwait 3\nshot {name}\nquit\n")
    shot = ROOT / "Saved" / "Screenshots" / "WindowsEditor" / f"{name}.png"
    if shot.exists():
        shot.unlink()
    command = ["powershell", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools" / "play.ps1"), "-Script", str(script),
               "-Tag", name, "-Map", args.level, "-Res", "1920x1080", "-TimeoutSeconds", "240"]
    if args.start:
        command += ["-StartAt", args.start]
    subprocess.run(command, cwd=ROOT, check=False)
    if not shot.exists():
        sys.exit(f"no screenshot: see Saved/Logs/{name}.log")
    picture = Image.open(shot).convert("RGB")
    # Cut to the lobby's shape (16:9 already at 1920 by 1080), then down to its size.
    width, height = picture.size
    wanted = SIZE[0] / SIZE[1]
    if width / height > wanted:
        cut = int(height * wanted)
        picture = picture.crop(((width - cut) // 2, 0, (width - cut) // 2 + cut, height))
    else:
        cut = int(width / wanted)
        picture = picture.crop((0, (height - cut) // 2, width, (height - cut) // 2 + cut))
    out = ROOT / "Content" / "UI" / "Maps" / args.file
    picture.resize(SIZE, Image.LANCZOS).save(out)
    print(f"saved {out}")


if __name__ == "__main__":
    main()
