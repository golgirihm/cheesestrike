"""Runs the repository's test suites. CI runs exactly this.

    python tools/run_tests.py              every suite
    python tools/run_tests.py game         only the suites named

Every selected suite runs even if an earlier one fails; the exit code is
non-zero if any failed.

The game suite needs Godot. It is found through the GODOT environment
variable, or else as `godot_console` or `godot` on the PATH.

CI pins the versions of what it tests with (tools/versions.env and
signaling/requirements.txt). A local run uses whatever is installed, so this
warns when that differs from the pins. It still runs: the pull request's CI
run is what decides.
"""

import argparse
import importlib.metadata
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GAME = ROOT / "game"
VERSIONS = ROOT / "tools" / "versions.env"
REQUIREMENTS = ROOT / "signaling" / "requirements.txt"

warnings = []


def warn(message):
    warnings.append(message)
    print(f"warning: {message}", flush=True)


def pinned_versions():
    """The versions CI uses, by name: the entries of versions.env plus each
    exactly pinned package in the signaling requirements."""
    pins = {}
    for line in VERSIONS.read_text(encoding="utf-8-sig").splitlines():
        name, separator, value = line.strip().partition("=")
        if separator and not name.startswith("#"):
            pins[name] = value
    for line in REQUIREMENTS.read_text(encoding="utf-8-sig").splitlines():
        name, separator, value = line.strip().partition("==")
        if separator:
            pins[name] = value
    return pins


def find_godot():
    # godot_console first: on Windows, plain godot prints nothing to the terminal.
    for candidate in (os.environ.get("GODOT"), "godot_console", "godot"):
        if candidate and shutil.which(candidate):
            return shutil.which(candidate)
    return None


def check_python(pins):
    # Minor version only: an exact patch release often can't be matched
    # locally, and python.org has no Windows installers for the later ones.
    wanted = pins["PYTHON_VERSION"]
    running = ".".join(str(part) for part in sys.version_info[:3])
    if wanted.split(".")[:2] != running.split(".")[:2]:
        warn(f"running Python {running}; CI uses {wanted}")


def run(*command):
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.run(command, cwd=ROOT).returncode == 0


def game(pins):
    godot = find_godot()
    if godot is None:
        print("Godot not found. Put it on the PATH or set the GODOT environment variable.")
        return False
    # Reported as e.g. "4.7.2.stable.official.ed1daf0bf".
    found = subprocess.run([godot, "--version"], capture_output=True, text=True).stdout.strip()
    if not found.startswith(pins["GODOT_VERSION"] + "."):
        warn(f"running Godot {found or 'of unknown version'}; CI uses {pins['GODOT_VERSION']}")
    # Import first so Godot's class list matches the scripts on disk; a fresh
    # clone has none, and a stale one fails tests that use a new class.
    return (
        run(godot, "--headless", "--path", GAME, "--import")
        and run(godot, "--headless", "--path", GAME, "-s", "addons/gut/gut_cmdln.gd", "-gexit")
    )


def signaling(pins):
    try:
        found = importlib.metadata.version("websockets")
    except importlib.metadata.PackageNotFoundError:
        print("websockets is not installed. Run: pip install -r signaling/requirements.txt")
        return False
    wanted = pins.get("websockets")
    if wanted and found != wanted:
        warn(f"running websockets {found}; CI uses {wanted}")
    return run(sys.executable, "-m", "unittest", "discover", "-s", "signaling")


SUITES = {"game": game, "signaling": signaling}


def main():
    parser = argparse.ArgumentParser(description="Run the repository's test suites.")
    parser.add_argument("suites", nargs="*", metavar="suite", help=f"one of: {', '.join(SUITES)} (default: all)")
    selected = parser.parse_args().suites or list(SUITES)
    unknown = [name for name in selected if name not in SUITES]
    if unknown:
        parser.error(f"unknown suite: {', '.join(unknown)}")

    pins = pinned_versions()
    check_python(pins)

    results = {}
    for name in selected:
        print(f"\n=== {name} ===", flush=True)
        results[name] = SUITES[name](pins)

    print("\n=== summary ===")
    for name, passed in results.items():
        print(f"{name}: {'passed' if passed else 'FAILED'}")
    # Repeated here so they aren't lost above the test output.
    for message in warnings:
        print(f"warning: {message}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
