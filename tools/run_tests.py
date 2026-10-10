"""Runs the repository's test suites. CI runs exactly this.

    python tools/run_tests.py              every suite
    python tools/run_tests.py game         only the suites named

Every selected suite runs even if an earlier one fails; the exit code is
non-zero if any failed.

The game suite needs Godot. It is found through the GODOT environment
variable, or else as `godot_console` or `godot` on the PATH.
"""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GAME = ROOT / "game"


def find_godot():
    # godot_console first: on Windows, plain godot prints nothing to the terminal.
    for candidate in (os.environ.get("GODOT"), "godot_console", "godot"):
        if candidate and shutil.which(candidate):
            return shutil.which(candidate)
    return None


def run(*command):
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.run(command, cwd=ROOT).returncode == 0


def game():
    godot = find_godot()
    if godot is None:
        print("Godot not found. Put it on the PATH or set the GODOT environment variable.")
        return False
    # Import first so Godot's class list matches the scripts on disk; a fresh
    # clone has none, and a stale one fails tests that use a new class.
    return (
        run(godot, "--headless", "--path", GAME, "--import")
        and run(godot, "--headless", "--path", GAME, "-s", "addons/gut/gut_cmdln.gd", "-gexit")
    )


def signaling():
    return run(sys.executable, "-m", "unittest", "discover", "-s", "signaling")


SUITES = {"game": game, "signaling": signaling}


def main():
    parser = argparse.ArgumentParser(description="Run the repository's test suites.")
    parser.add_argument("suites", nargs="*", metavar="suite", help=f"one of: {', '.join(SUITES)} (default: all)")
    selected = parser.parse_args().suites or list(SUITES)
    unknown = [name for name in selected if name not in SUITES]
    if unknown:
        parser.error(f"unknown suite: {', '.join(unknown)}")

    results = {}
    for name in selected:
        print(f"\n=== {name} ===", flush=True)
        results[name] = SUITES[name]()

    print("\n=== summary ===")
    for name, passed in results.items():
        print(f"{name}: {'passed' if passed else 'FAILED'}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
