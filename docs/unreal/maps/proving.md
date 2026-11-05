# The Proving Ground

A basic, symmetric arena, 60 by 40 m inside its walls, for learning the game and trying things out, built by `tools/art/build_proving.py` into `Content/CheeseStrike/Maps/Lvl_ProvingGround` from the blocks in `tools/art/map_kit.py` ([principles.md](principles.md) says how maps are made). It took the place of the First Person template's shooter level in `CheeseStrikeRules::Maps` in wave 15: that level (`Variant_Shooter/Lvl_Shooter`) stays in Content as the front end's backdrop (the menu's figure stands in it) and the play tests' playground, but is no map of the game's. The arena has everything a map of the game's has, as plainly as it can: a flat floor, cheese to fight round and to lose, two bits of high ground, four weapon stands, three hill spots, the mice.

## Layout

x runs west (the Gouda Guys' end) to east (the Mozza Radicals'), y south to north; the floor is at 0 and the walls are 5 m high. The map is mirrored across x = 0.

| Place | Where | What it is |
|---|---|---|
| Gouda End, Mozza End | x beyond ±2200 | each side's eight starts in a file at x ±2700, 4 m apart from y -1400 to 1400, facing the middle, behind a screen of waxed cheese at x ±2200 (two walls of two courses, 2 m high, from y 100 to 1600 and -100 to -1600: a 2 m gap in the middle and 4 m open at each end), so that every way out of a spawn turns a corner and no line reaches a start from the arena |
| Gouda Lane, Mozza Lane | x ±800 to ±2200 | the open floor between the screen and the middle: a wall of cheese two courses high at x ±1200 north and south of the lane's middle (y 500 to 1500 and -500 to -1500), a 2 m stack at (±600, ±500), a crouching block a metre high by each side wall at (±2000, ±1700), and a Munster Twin SMGs stand on the lane's middle line at (±1600, 0) |
| The Middle | x -800 to 800 | the first hill spot at (0, 0) on the floor, a low wall of cheese two blocks high at x ±450 either side of it, and a yellow line down the middle of the map |
| North Platform, South Platform | (0, ±1100) | a platform of waxed cheese 7 m square and a metre up, stepped up from the west and the east by three steps a metre deep with risers of 25 cm (which the bots' navigation mesh walks; 35 is its most), never eaten: the second and third hill spots, on its top |
| the stands at the walls | (0, ±1750) | the BabyBomb by the north wall and the Cheese Stick by the south, on the middle line: the power weapons where both sides reach them alike, in the open |

Free-for-all adds six starts: at the four corners of the lanes by the north and south walls and one in each lane at (±900, 0), facing in.

## The cheese, and what the mice change

84 blocks the mice eat and 60 they don't (the screens and the platforms, in wax). One mouse to 25 blocks brings three, over the north and south walls at x ±500 and ±1500 (never over a spawn), and they leave the arena bare in about a minute and a quarter.

| Structure | Blocks | Before | Eaten |
|---|---|---|---|
| **CoverGouda**, **CoverMozza** | 36 each | The lanes' walls at x ±1200 close the long lines from the screen's gaps to the middle; the stacks and the crouching blocks give each lane cover every 6 to 8 m. | The lanes are open floor from screen to screen; only the platforms, the screens' ends and the walls' corners are left to hide behind. |
| **Middle** | 12 | Two low walls either side of the middle hill: crouch cover on it. | The middle is bare. |
| **ScreenGouda**, **ScreenMozza**, **PlatformNorth**, **PlatformSouth** | 60, in wax | Hard cover in front of each spawn, and the high ground. | Never eaten: the open arena has no line into a spawn, and the platforms still stand. |

King of the Hill's hill goes round the Middle, the North Platform and the South Platform, a minute at each: all on the middle line, as far from both sides, the platforms on cheese the mice never eat.

## Tests and tooling

- `CheeseStrike.Play.AfterTheMice.ProvingGround`: its stands (four, fair, the power weapons the most contested, each with a way to it from a start before the mice and after), all its cheese eaten and the mice gone, the screens and platforms standing where they were, every start on the ground and joined to every other.
- `CheeseStrike.Play.EveryMapHasFairHills.ProvingGround` and `TheStreaksWorkOnEveryMap.ProvingGround` run on it as on every map of the game's.
- `tools/play.ps1 -Map /Game/CheeseStrike/Maps/Lvl_ProvingGround` plays it (`-Bots -Mice -Extra "?MiceAt=5"` for the mice; from Git Bash, set `MSYS_NO_PATHCONV=1`); `tools/play/proving_tour.txt` takes pictures of it from set places.
- Its lobby picture is `python tools/art/map_picture.py /Game/CheeseStrike/Maps/Lvl_ProvingGround proving_ground.png -3700 -2900 1900 -30 38 --start -2700,-1400,100,0`.

Changing the map is a change to `tools/art/build_proving.py`, then running it (it rebuilds the level from nothing) and the tests.
