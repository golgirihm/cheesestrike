"""Makes another working copy of the game, on a branch of its own, so that
two pieces of work can be built, tested and played side by side.

    python tools/worktree.py add <name> [--base origin/main]
    python tools/worktree.py remove <name>
    python tools/worktree.py list

A copy is a git worktree of this repository at C:/Hamid/code/cs-wt/<name>
(CHEESESTRIKE_WORKTREES names another folder), on the branch <name>, made
from --base (origin/main unless given; an existing branch <name> is used as
it is). Its Content/ is a worktree of the assets submodule, at the commit
the branch records, detached: it shares the submodule's history and its Git
LFS objects, so nothing is cloned or downloaded twice. Don't run `git
submodule update` in a copy, which would clone the submodule afresh: move
its Content with `git -C Content checkout <commit>` instead. A copy builds
into its own Binaries/ and Intermediate/ (the first build of a copy compiles
the whole module), and its tests and played games keep to themselves
(tools/run_tests.py and tools/play.ps1 give each copy its own sessions and
port).

`remove` deletes the copy's folders, Content's too, leaving its branches.
"""

import argparse
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONTENT = ROOT / "Content"
PLACE = pathlib.Path(os.environ.get("CHEESESTRIKE_WORKTREES", str(ROOT.parent.parent / "cs-wt")))


def git(*arguments, cwd=ROOT, check=True):
    print("+ git", " ".join(str(a) for a in arguments), flush=True)
    result = subprocess.run(["git", *[str(a) for a in arguments]], cwd=cwd, text=True, capture_output=True)
    if result.stdout.strip():
        print(result.stdout.strip())
    if check and result.returncode != 0:
        sys.exit(f"error: {result.stderr.strip()}")
    return result


def has_branch(name):
    return git("rev-parse", "--verify", "--quiet", f"refs/heads/{name}", check=False).returncode == 0


def add(name, base):
    copy = PLACE / name
    if copy.exists():
        sys.exit(f"error: {copy} is there already")
    PLACE.mkdir(parents=True, exist_ok=True)
    git("fetch", "--quiet", "origin")
    if has_branch(name):
        git("worktree", "add", copy, name)
    else:
        git("worktree", "add", "-b", name, copy, base)
    # The assets at the commit the branch records, from the submodule's own history.
    recorded = git("ls-tree", "HEAD", "Content", cwd=copy).stdout.split()
    if len(recorded) < 3:
        sys.exit("error: the branch records no Content")
    commit = recorded[2]
    if git("cat-file", "-e", f"{commit}^{{commit}}", cwd=CONTENT, check=False).returncode != 0:
        git("fetch", "--quiet", "origin", cwd=CONTENT)
    git("worktree", "add", "--detach", copy / "Content", commit, cwd=CONTENT)
    print(f"\n{copy} is on the branch {name}, its Content at {commit[:7]}. Open {copy / 'CheeseStrike.uproject'}.")


def remove(name):
    copy = PLACE / name
    if (copy / "Content" / ".git").exists():
        git("worktree", "remove", "--force", copy / "Content", cwd=CONTENT)
    git("worktree", "remove", "--force", copy)
    git("worktree", "prune")
    git("worktree", "prune", cwd=CONTENT)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    adding = commands.add_parser("add", help="make a copy on a branch")
    adding.add_argument("name")
    adding.add_argument("--base", default="origin/main", help="what a new branch starts from")
    removing = commands.add_parser("remove", help="delete a copy's folders")
    removing.add_argument("name")
    commands.add_parser("list", help="the copies")
    arguments = parser.parse_args()
    if arguments.command == "add":
        add(arguments.name, arguments.base)
    elif arguments.command == "remove":
        remove(arguments.name)
    else:
        git("worktree", "list")


if __name__ == "__main__":
    main()
