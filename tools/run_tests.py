"""Runs the repository's test suites. CI runs exactly this.

    python tools/run_tests.py              every suite
    python tools/run_tests.py game         only the suites named

Every selected suite runs even if an earlier one fails; the exit code is
non-zero if any failed.

The game and browser suites need Godot. It is found through the GODOT
environment variable, or else as `godot_console` or `godot` on the PATH.

The browser suite exports the web build to build/browser-tests and plays it in
headless browser windows. Besides Godot's Web export template it needs:

    pip install -r browser_tests/requirements.txt
    python -m playwright install --only-shell chromium

CI pins the versions of what it tests with (tools/versions.env and the
requirements.txt files). A local run uses whatever is installed, so this
warns when that differs from the pins. It still runs: the pull request's CI
run is what decides.
"""

import argparse
import importlib.metadata
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GAME = ROOT / "game"
VERSIONS = ROOT / "tools" / "versions.env"
REQUIREMENTS = [ROOT / "signaling" / "requirements.txt", ROOT / "browser_tests" / "requirements.txt"]
BROWSER_BUILD = ROOT / "build" / "browser-tests"

warnings = []


def warn(message):
    warnings.append(message)
    print(f"warning: {message}", flush=True)


def pinned_versions():
    """The versions CI uses, by name: the entries of versions.env plus each
    exactly pinned package in the requirements files."""
    pins = {}
    for line in VERSIONS.read_text(encoding="utf-8-sig").splitlines():
        name, separator, value = line.strip().partition("=")
        if separator and not name.startswith("#"):
            pins[name] = value
    for requirements in REQUIREMENTS:
        for line in requirements.read_text(encoding="utf-8-sig").splitlines():
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
    wanted = pins["PYTHON_VERSION"]
    running = ".".join(str(part) for part in sys.version_info[:3])
    if running != wanted:
        warn(f"running Python {running}; CI uses {wanted}")


def run(*command):
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.run(command, cwd=ROOT).returncode == 0


def checked_godot(pins):
    """Godot's path, having warned if it isn't the pinned version; None, with
    a message, if it isn't installed."""
    godot = find_godot()
    if godot is None:
        print("Godot not found. Put it on the PATH or set the GODOT environment variable.")
        return None
    # Reported as e.g. "4.7.2.stable.official.ed1daf0bf".
    found = subprocess.run([godot, "--version"], capture_output=True, text=True).stdout.strip()
    if not found.startswith(pins["GODOT_VERSION"] + "."):
        warn(f"running Godot {found or 'of unknown version'}; CI uses {pins['GODOT_VERSION']}")
    return godot


def checked_package(pins, name, requirements):
    """Whether the Python package `name` is installed, having warned if it
    isn't the pinned version."""
    try:
        found = importlib.metadata.version(name)
    except importlib.metadata.PackageNotFoundError:
        print(f"{name} is not installed. Run: pip install -r {requirements}")
        return False
    wanted = pins.get(name)
    if wanted and found != wanted:
        warn(f"running {name} {found}; CI uses {wanted}")
    return True


def import_project(godot):
    # Import first so Godot's class list matches the scripts on disk; a fresh
    # clone has none, and a stale one fails tests that use a new class.
    return run(godot, "--headless", "--path", GAME, "--import")


def game(pins):
    godot = checked_godot(pins)
    return godot is not None and import_project(godot) and run_gut(godot)


def run_gut(godot):
    """Runs the GUT tests, and also fails if any test file didn't run. GUT skips
    a file it can't parse and still reports success, which would let a broken
    test file pass unnoticed."""
    command = [godot, "--headless", "--path", GAME, "-s", "addons/gut/gut_cmdln.gd", "-gexit"]
    print("+", " ".join(str(part) for part in command), flush=True)
    process = subprocess.Popen(
        command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace"
    )
    output = []
    for line in process.stdout:
        print(line, end="", flush=True)
        output.append(line)
    passed = process.wait() == 0

    expected = len(list((GAME / "test").rglob("test_*.gd")))
    ran = re.search(r"^Scripts\s+(\d+)\s*$", "".join(output), re.MULTILINE)
    if ran is None or int(ran.group(1)) != expected:
        print(f"error: {expected} test files exist but GUT ran {ran.group(1) if ran else 'an unknown number'}.")
        print("A test file that fails to load is skipped; look for a script error above.")
        return False
    return passed


def signaling(pins):
    if not checked_package(pins, "websockets", "signaling/requirements.txt"):
        return False
    return run(sys.executable, "-m", "unittest", "discover", "-s", "signaling")


def browser(pins):
    # The browser tests start the signaling server, so they need its package too.
    if not checked_package(pins, "websockets", "signaling/requirements.txt"):
        return False
    if not checked_package(pins, "playwright", "browser_tests/requirements.txt"):
        return False
    godot = checked_godot(pins)
    if godot is None or not import_project(godot):
        return False
    # Export afresh, so the tests never run against a stale or half-made build.
    shutil.rmtree(BROWSER_BUILD, ignore_errors=True)
    BROWSER_BUILD.mkdir(parents=True)
    index = BROWSER_BUILD / "index.html"
    if not run(godot, "--headless", "--path", GAME, "--export-debug", "Web", index) or not index.exists():
        print("error: the web export failed. Godot's Web export template may be missing;")
        print("install the export templates from the editor, or run: python tools/fetch_export_template.py")
        return False
    return run(sys.executable, "-m", "unittest", "discover", "-v", "-s", "browser_tests")


SUITES = {"game": game, "signaling": signaling, "browser": browser}


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
