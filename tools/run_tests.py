"""Builds the editor and runs the game's Automation tests, headless. CI runs
exactly this.

    python tools/run_tests.py                     every CheeseStrike test
    python tools/run_tests.py CheeseStrike.Project  only those under a name
    python tools/run_tests.py --no-build          skip building first

The engine is found through the UE_ROOT environment variable, or else at
Epic's default install path for the version the project names
(EngineAssociation in CheeseStrike.uproject).

It fails if the build fails, if any test fails, or if no test ran at all,
which is what a filter that matches nothing, or a test module that didn't
load, would look like.
"""

import argparse
import json
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROJECT = ROOT / "CheeseStrike.uproject"
TARGET = "CheeseStrikeEditor"
LOG = ROOT / "Saved" / "Logs" / "run_tests.log"
COMPLETED = re.compile(r"Test Completed\. Result=\{(\w+)\} Name=\{([^}]*)\} Path=\{([^}]*)\}")


def engine():
    if os.environ.get("UE_ROOT"):
        return pathlib.Path(os.environ["UE_ROOT"])
    version = json.loads(PROJECT.read_text(encoding="utf-8"))["EngineAssociation"]
    return pathlib.Path(rf"C:\Program Files\Epic Games\UE_{version}")


def build(root):
    command = [str(root / "Engine" / "Build" / "BatchFiles" / "Build.bat"), TARGET, "Win64", "Development",
               f"-Project={PROJECT}", "-WaitMutex", "-NoHotReload"]
    print("+", " ".join(command), flush=True)
    return subprocess.run(command, cwd=ROOT).returncode == 0


def run_tests(root, name):
    LOG.parent.mkdir(parents=True, exist_ok=True)
    command = [str(root / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"), str(PROJECT),
               f"-ExecCmds=Automation RunTests {name}", "-TestExit=Automation Test Queue Empty",
               "-unattended", "-nullrhi", "-nosplash", "-nosound", "-stdout", "-FullStdOutLogOutput", f"-abslog={LOG}"]
    print("+", " ".join(command), flush=True)
    process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
    results = []
    for line in process.stdout:
        if "LogAutomationController" in line and ("Error" in line or "Test Completed" in line):
            print(line.rstrip(), flush=True)
        found = COMPLETED.search(line)
        if found:
            results.append((found.group(1), found.group(3)))
    process.wait()
    failed = [path for result, path in results if result != "Success"]
    print(f"\n{len(results)} tests, {len(results) - len(failed)} passed, {len(failed)} failed. Full log: {LOG}")
    for path in failed:
        print(f"  failed: {path}")
    if not results:
        print("error: no test ran. Check the filter, and look for the test module failing to load in the log.")
    return bool(results) and not failed


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name", nargs="?", default="CheeseStrike", help="run the tests whose names begin with this")
    parser.add_argument("--no-build", action="store_true", help="don't build the editor first")
    arguments = parser.parse_args()
    root = engine()
    if not (root / "Engine").is_dir():
        sys.exit(f"error: no engine at {root}; set UE_ROOT to where Unreal is installed")
    if not arguments.no_build and not build(root):
        sys.exit("error: the build failed")
    sys.exit(0 if run_tests(root, arguments.name) else 1)


if __name__ == "__main__":
    main()
