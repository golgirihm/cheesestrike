"""Reads the friends panel's width, height and top, frame by frame, off the
game's log from tools/play/friends_expand.txt ("Friends panel: W wide, H high,
T down", written each frame it moves) and off the screenshots the script takes
through its growing open (friends_expand_NN.png) and shrinking shut
(friends_collapse_NN.png), to show the motion is smooth both ways at once: a
different width and height every frame through the quarter second, never a
step, the tile growing into the panel and back.

Run with any Python that has Pillow, after the play script:
    python -I tools/art/friends_widths.py [Saved/Screenshots/WindowsEditor] [Saved/Logs/friends_expand.log]

The log is the proof: its first run of growing widths (the slide open) and
the first run of shrinking ones (the slide shut) must each have at least six
distinct widths, with no step most of the way, and the heights and tops logged
on the same frames likewise (growing and rising open, shrinking and dropping
shut). The pictures are read too, for looking at: the panel darkens the scene
it lies over, so each is compared with the first (friends_expand_00.png, the
panel folded to its tile, which is at the top only) down a band of rows low on
the screen and clear of the figure, the width counted from the right edge to the
first column that still looks as it did without the panel (none until the panel
has grown down to the band), and the height down a column near the right edge to
the last row that differs. (It moves on Slate's clock, and a screenshot takes a
while to write, so the pictures catch fewer frames of it than the log.)
"""
import glob
import os
import re
import sys

from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
SHOTS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "Screenshots", "WindowsEditor")
LOG = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "Saved", "Logs", "friends_expand.log")
# The band of rows compared (shares of the picture's height), and how far a column's brightness must differ from the
# picture without the panel to count as under it.
BAND = (0.6, 0.92)
DIFFERENT = 14.0


def columns(path):
    """The mean brightness of each column down the band."""
    image = Image.open(path).convert("L")
    w, h = image.size
    top, bottom = int(h * BAND[0]), int(h * BAND[1])
    band = image.crop((0, top, w, bottom)).resize((w, 1), Image.BOX)
    return [band.getpixel((x, 0)) for x in range(w)]


def width_of(path, bare):
    mine = columns(path)
    for x in range(len(mine) - 1, -1, -1):
        if abs(mine[x] - bare[x]) < DIFFERENT:
            return len(mine) - 1 - x
    return len(mine)


def column_near_edge(path):
    """The brightness of each row of a column near the right edge, inside both the tile and the open panel."""
    image = Image.open(path).convert("L")
    w, h = image.size
    x = w - max(8, w // 60)
    return [image.getpixel((x, y)) for y in range(h)]


def height_of(path, bare):
    mine = column_near_edge(path)
    differing = [y for y in range(len(mine)) if abs(mine[y] - bare[y]) >= DIFFERENT]
    return differing[-1] + 1 if differing else 0


def widths(prefix):
    bare = columns(os.path.join(SHOTS, "friends_expand_00.png"))
    bare_column = column_near_edge(os.path.join(SHOTS, "friends_expand_00.png"))
    files = sorted(glob.glob(os.path.join(SHOTS, prefix + "_[0-9][0-9].png")))
    return [(os.path.basename(f), width_of(f, bare), height_of(f, bare_column)) for f in files]


def smooth(values, grows):
    """Whether a run of widths is a slide: six or more distinct, in order, no step most of the way."""
    distinct = len(set(values))
    steps = list(zip(values, values[1:]))
    monotonic = all((b >= a) if grows else (b <= a) for a, b in steps)
    biggest = max((abs(b - a) for a, b in steps), default=0)
    span = max(values) - min(values) if values else 0
    return distinct >= 6 and monotonic and (not span or biggest <= span * 0.4), distinct, monotonic, biggest, span


def runs(values):
    """Where the first run of growing widths is (the slide open) and the run of shrinking ones that follows it (the slide shut), as index ranges."""
    start = next((i for i in range(len(values) - 1) if values[i + 1] > values[i]), None)
    if start is None:
        return (0, 0), (0, 0)
    top = start
    while top + 1 < len(values) and values[top + 1] >= values[top]:
        top += 1
    bottom = top
    while bottom + 1 < len(values) and values[bottom + 1] <= values[bottom]:
        bottom += 1
    return (start, top + 1), (top, bottom + 1)


def main():
    ok = True
    if not os.path.exists(LOG):
        print(f"no log at {LOG}")
        sys.exit(1)
    text = open(LOG, encoding="utf-8", errors="replace").read()
    logged = [(float(m.group(1)), float(m.group(2)), float(m.group(3)))
              for m in re.finditer(r"Friends panel: ([0-9.]+) wide, ([0-9.]+) high, ([0-9.]+) down", text)]
    if not logged:
        print("the log has no \"Friends panel: W wide, H high, T down\" lines")
        sys.exit(1)
    widths_logged = [w for w, _, _ in logged]
    print(f"the log: {len(logged)} frames of the panel moving (width x height, top):")
    print("  " + ", ".join(f"{w:.0f}x{h:.0f}@{t:.0f}" for w, h, t in logged))
    opening, shutting = runs(widths_logged)
    for name, (first, last), grows in (("open", opening, True), ("shut", shutting, False)):
        for what, index, rises in (("widths", 0, grows), ("heights", 1, grows), ("tops", 2, not grows)):
            values = [frame[index] for frame in logged[first:last]]
            fine, distinct, monotonic, biggest, span = smooth(values, rises)
            print(f"  the first move {name}: {len(values)} frames, {distinct} distinct {what}, {'growing' if rises else 'shrinking'} throughout: {monotonic}, the biggest step {biggest:.1f} of {span:.1f} units: {'smooth' if fine else 'NOT SMOOTH'}")
            ok = ok and fine
    for name, prefix, grows in (("open", "friends_expand", True), ("shut", "friends_collapse", False)):
        series = widths(prefix)
        print(f"the pictures on the way {name}:")
        for file, width, height in series:
            print(f"  {file}: {width} px wide (low on the screen), {height} px down to its foot")
        for what, index in (("widths", 1), ("heights", 2)):
            fine, distinct, monotonic, biggest, span = smooth([s[index] for s in series], grows)
            print(f"  {distinct} distinct {what}, {'growing' if grows else 'shrinking'} throughout: {monotonic}, the biggest step {biggest} px of {span}")
    print("OK" if ok else "NOT SMOOTH")
    sys.exit(0 if ok else 1)


main()
