# CheeseStrike: Gouda Overload

A multiplayer first-person shooter, made with Unreal Engine 5.8, headed for Steam. Up to sixteen players fight across maps whose cover is cheese, and a minute into every match giant mice come over the edge and start eating it.

[docs/unreal/design.md](docs/unreal/design.md) describes the whole game. [docs/unreal/spike.md](docs/unreal/spike.md) is the plan it is being built by.

The game was first made in Godot. That version is kept at the [`godot-final`](https://github.com/golgirihm/cheesestrike/releases/tag/godot-final) tag, with a web build attached and instructions for playing it without any server.

## License

Copyright (c) 2026 Hamid Golgiri. All rights reserved. The source is public for reference only; see [LICENSE](LICENSE).

The game's assets are in `Content/`, a submodule from the private repository [cheesestrike-assets](https://github.com/golgirihm/cheesestrike-assets). Most of them are licensed from Epic Games and others whose licences allow them in the released game but forbid sharing the files, so they stay out of this public repository. Without them the code builds, but there is nothing to play.

## Development

You need:

- **Unreal Engine 5.8**, from the Epic Games Launcher.
- **Visual Studio 2026 Community** with the "Game development with C++" workload and the **MSVC v14.50** toolset. The project pins that compiler; see [CLAUDE.md](CLAUDE.md).
- **Git LFS**, set up once with `git lfs install`.
- **Python 3**, for the test runner.

If you have access to the assets, fetch them after cloning:

```
git submodule update --init
git -C Content lfs pull
```

Open `CheeseStrike.uproject` to build and open the editor; it asks to build the game module the first time. To build from the command line instead:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CheeseStrikeEditor Win64 Development -Project="%CD%\CheeseStrike.uproject" -WaitMutex
```

The editor opens on the shooter level. Play it with Play in Editor. To try two players on one PC, set the number of players to 2 and the net mode to "Play As Listen Server" in the Play menu's options.

## Tests

The tests are Unreal Automation tests in `Source/CheeseStrike/Tests/`. This builds the editor and runs them all headless:

```
python tools/run_tests.py
```

Name some of them to run only those, such as `python tools/run_tests.py CheeseStrike.Project`. They can also be run from the editor's Session Frontend, under Automation. The `CheeseStrike.Play` tests play the game in the editor, alone or as a host and a joiner, and take ten to twenty seconds each. What can't be tested automatically is in [docs/manual-tests.md](docs/manual-tests.md).
