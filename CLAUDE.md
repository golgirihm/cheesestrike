# CheeseStrike: Gouda Overload

A multiplayer first-person shooter for Steam, made with Unreal Engine 5.8 in C++. The cover is cheese, and a minute into every match giant mice come and eat it. [docs/unreal/design.md](docs/unreal/design.md) is the whole game as a specification, and [docs/unreal/spike.md](docs/unreal/spike.md) is the plan the project is being built by. The game was first made in Godot; that version is kept at the `godot-final` tag, and its code there is the reference wherever the design document is not enough.

The project is at the repository's root: `CheeseStrike.uproject`, `Source/CheeseStrike/` (one game module), `Config/`, and `Content/`, which is the private `cheesestrike-assets` repository as a git submodule (see below). It began as Unreal's First Person template with its Shooter variant; `Config/DefaultEngine.ini` redirects the template's script package and classes to ours, and fixes two mistakes in the template's own content. The template's shooter level is the playground until the game has arenas of its own; its game mode override is cleared, so it plays with the game's own character, and its weapons, pickups and enemies are the template's and don't yet work with it.

## What the port keeps

The Godot game is the source of the design, not a thing to copy. Keep what Unreal and its First Person template do well (their input, movement feel, camera, scale and conventions) and port the game's ideas and tuning where they matter: the cheese that is cover and gets eaten by the mice that come partway through a match, so that a map opens up and the fight intensifies; the weapons' roles and roughly their numbers; the modes; the Overload. Don't port the Godot key bindings, speeds, scale or maps for their own sake. The Godot maps were placeholders; a placeholder map (the template's shooter level) does until there is a plan for how levels work.

## Testing rules

Every change follows both of these, in the same pull request as the change:

1. **Automated tests.** Assess whether the change's behavior can be covered by an automated test. If it can, write the test with the change. Tests are Unreal Automation tests in `Source/CheeseStrike/Tests/`, named `CheeseStrike.<Area>.<What it checks>`. Rules belong in plain C++ that a test can call without a world, as the Godot game kept its rules in plain classes; what needs a world or a level gets a test that loads one.
2. **Manual tests.** Assess whether `docs/manual-tests.md` needs rows added, changed or removed. Behavior that an automated test can't cover gets a row. A row that becomes covered by an automated test is deleted.

Say what you concluded in the pull request description, including when the answer is that no test or no manual-test change was needed.

Before pushing, run the tests: `python tools/run_tests.py`, which builds the editor and runs every `CheeseStrike` test headless, or `python tools/run_tests.py CheeseStrike.<Area>` for some of them. It fails if no test ran.

To see a change in the game itself, play it without a person: `powershell -ExecutionPolicy Bypass -File tools/play.ps1 -Tag <name>` starts the game windowed, drives it through the keyboard and mouse (so the person at the PC must leave both alone for about 90 seconds, and the editor must be closed), screenshots each step into `Saved/Shots/` and traces the character's state ten times a second into `Saved/Logs/<name>.log` (`-TraceState`). Read the numbers first (speed, eye height, crouched, sliding, falling), then the pictures. Edit the script's steps for what a change needs, and give the game `-StartAt=X,Y,Z,Yaw` for a spot with room.

## Working in this repo

- **Pull requests only.** `main` is protected: changes arrive through a pull request, merged by rebase. Keep each commit on a branch meaningful, since every one lands on `main`.
- **After merging, move on.** Once a pull request's checks have passed on an up-to-date branch, merge it and carry on.
- **Delete the local branch after merging.** A rebase merge rewrites the commits, so git won't see the branch as merged and `git branch -D` is needed. Return to an up-to-date `main`.
- **No AI attribution.** Don't add "Generated with Claude Code" to pull request descriptions or a `Co-Authored-By` trailer for Claude to commits.
- **C++ first.** Gameplay is written in C++ so that it can be read, edited and reviewed as text. Blueprints are for wiring assets to C++ classes (meshes, sounds, animation, the values a designer tunes), not for logic. A Blueprint can't be read in a diff, so say in the pull request what changed in one. Input actions and key bindings are made in code too (`Source/CheeseStrike/Input/`), not kept as assets. Assets the C++ needs (meshes, animation Blueprints) are found by path in constructors.
- **Movement is predicted.** Anything that changes how the character moves must reach the server through the saved move (`CheeseStrikeMovementComponent`): a held key as a compressed flag, state as fields saved and restored with the move. Otherwise the server disagrees with what the player saw and snaps them back.
- **Editing assets from a script.** The editor's Python runs headless: `UnrealEditor-Cmd.exe <project> -ExecCmds="py <script>" -unattended -nullrhi` with `unreal.SystemLibrary.quit_editor()` at the script's end (the `-run=pythonscript` commandlet can't save a map). `print` goes nowhere; write to a file. The editor must be closed first: it holds the game module and the open level's file.
- **The compiler is pinned.** Both targets in `Source/*.Target.cs` build with MSVC 14.50.35717, which Unreal 5.8 is made for; the 14.51 that Visual Studio 2026 updates bring in breaks the engine's headers. Change it there when Epic supports a newer one.
- **Building from the command line.** `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CheeseStrikeEditor Win64 Development -Project="<repo>\CheeseStrike.uproject" -WaitMutex`. Close the editor first, or build from inside it with Live Coding: a running editor holds the module's DLL.
- **Assets are in a private submodule.** This repository is public; `Content/` is the private `cheesestrike-assets` repository, because most assets may not be redistributed (Epic's template content under the Unreal Engine EULA, Fab content under its licences). Never commit an asset to this repository. A change to assets is a commit in `Content/` (on its `main`, pushed first), then a commit here that moves the submodule to it, in the same pull request as any code it goes with. Add a row to `Content/LICENSES.md` for every pack or file brought in, with its source and licence, and keep the folder each pack came in (`Content/<Pack>/`). A new clone needs `git submodule update --init`, then `git lfs pull` inside `Content/`. Without the submodule the code still builds, but the project has nothing to open and the tests that check content fail.
- **Git LFS.** Binary files go through Git LFS in both repositories (`.gitattributes`). Run `git lfs install` once on a new machine. Don't commit `Binaries/`, `Intermediate/`, `Saved/` or the Visual Studio files; `.gitignore` leaves them out.
- **Scale.** The world is life size: Unreal centimetres and a 180 cm player. The Godot game was three times life size; [docs/unreal/design.md](docs/unreal/design.md#scale) says how its numbers convert.
