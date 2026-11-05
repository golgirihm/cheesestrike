"""Reads a log played with CheeseStrike.TraceView on (tools/play/look_trace.txt:
a steady look up, held, then down, with the pistol and the carbine) and says,
for each stretch the script marked with `log`, how the weapon's height in view
went: the most it moved from one frame to the next, how far it moved over the
stretch, and the furthest it went back against that way. A weapon that never
jumps between frames (under 0.08 cm; the hold's own movement is under 0.04)
is right; a jump of 0.18 cm at 0.3 degrees a frame, a dip as the look begins
or a pop as it ends, was the aim jitter (the arms animated from the frame
before the camera's). The slow breathing the hold keeps may wander a few
millimetres back over a second, which is not a jump.

    python -I tools/art/look_trace.py Saved/Logs/look60.log
"""

MOST_JUMP = 0.08

import re
import sys

LINE = re.compile(r"View: weapon at \(([-\d.]+), ([-\d.]+), ([-\d.]+)\) turned \(([-\d.]+), [-\d.]+, [-\d.]+\).*? pitch ([-\d.]+)")
MARK = re.compile(r"PlayScript says: (.*)$")


def furthest_back(heights):
    """How far the height went back against the way it went overall, and that overall move."""
    if len(heights) < 2:
        return 0.0, 0.0
    moved = heights[-1] - heights[0]
    direction = 1.0 if moved >= 0.0 else -1.0
    furthest = heights[0] * direction
    back = 0.0
    for height in heights:
        along = height * direction
        furthest = max(furthest, along)
        back = max(back, furthest - along)
    return back, moved


def main(path):
    stretches = []
    name = None
    frames = []
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            mark = MARK.search(line)
            if mark:
                if name is not None:
                    stretches.append((name, frames))
                name = mark.group(1).strip()
                frames = []
                continue
            found = LINE.search(line)
            if found and name is not None:
                frames.append((float(found.group(5)), float(found.group(3)), float(found.group(4))))
    if name is not None:
        stretches.append((name, frames))
    worst = 0.0
    for name, frames in stretches:
        if len(frames) < 2:
            continue
        heights = [frame[1] for frame in frames]
        pitches = [frame[0] for frame in frames]
        jump = max(abs(b - a) for a, b in zip(heights, heights[1:]))
        back, moved = furthest_back(heights)
        worst = max(worst, jump)
        print(f"{name}: {len(frames)} frames, the view pitched {pitches[-1] - pitches[0]:+.1f}, the weapon's height jumped at most {jump:.3f} cm between frames, moved {moved:+.3f} cm, at most {back:.3f} back against it")
    print(f"worst jump: {worst:.3f} cm" + (" (the aim jitter)" if worst >= MOST_JUMP else " (no jump)"))
    return 0 if worst < MOST_JUMP else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "Saved/Logs/look60.log"))
