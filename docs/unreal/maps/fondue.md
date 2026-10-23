# The Fondue Floor

A vat hall whose whole floor is cheese over a vat of molten fondue, 72 by 40 m at life size, for up to sixteen players. It is built by `tools/art/build_fondue.py` into `Content/CheeseStrike/Maps/Lvl_FondueFloor`, from the blocks in `tools/art/map_kit.py` ([principles.md](principles.md) says how maps are made). At the start it plays as a close Counter-Strike floor of three lanes, crates and wheels of cheese, islands of hard cover and a gantry over the middle. The mice eat the floor itself, tile by tile, and the fondue under it shows through: by the end of a match the hall is a glowing lake crossed by paths of red wax, a few stepping stones of cheese and the islands, and every step is a choice. A fall in kills.

## How the floor is built

The floor is a grid of tiles 2 m square, each a pillar of cheese (an `ACheeseStrikeCheese`) standing on the vat's concrete floor 6 m down, its top at the floor (z 0); the fondue's surface is 2.5 m below the floor. Nothing else holds a tile up: no grid of beams under it, no ledge round it. So a tile eaten is a hole straight down into the fondue, with nothing left to walk on, and the hole's sides are the next tiles' cheese, lit orange from below. A tile stands on the vat's floor, which holds it (`CheeseStrikeMice::IsHeld`), and the tiles beside it don't lean on it, so eating one never brings down another; what stands on a tile (a crate of cheese, a wheel, a tunnel's wall) sinks with it. The tiles are 4 cm short of their cells, so the floor shows its grid.

- **Edible tiles** (plain cheese): most of the floor. 460 of them.
- **Waxed tiles** (`M_CheeseWax`, never eaten): the paths, 74 tiles and two planks.
- **Islands** (concrete): the spawn decks, the pumps, the boilers and the Stirrer's island stand in the vat on their own pylons.

`GRID` at the top of the script lays the west half out a cell to a character (`.` edible, `w` wax, `D` deck, `R` ramp, `P`, `B`, `S` islands, `-` plank, space for the open moat, `G` the gantry's stairs); the east half is its mirror (x to -x).

Falling in kills. The level's kill height (World Settings' KillZ, `kit.kill_below(-160)`) is where a falling player's feet reach the fondue: the character is killed by the fall (`ACheeseStrikeCharacter::FellOutOfWorld`, `Rules/CheeseStrikeFalls.h`) rather than destroyed, so its body is seen going in and its player watches from the hole. A fall within five seconds of being hurt is a kill for whoever hurt them (a knock into the fondue counts); otherwise nobody is credited, and the kill feed says "<name>  fell into the fondue" and the death screen "FELL INTO THE FONDUE" (the words come from the place the fall ends in, The Fondue).

## Layout

West is The Gouda Guys' side, east the Mozza Radicals'; the map is mirrored across the middle.

| Place | Where | What it is |
|---|---|---|
| Gouda Deck, Mozza Deck | each end | a concrete deck 1.2 m up against the end wall, 8 by 20 m, walled 2.2 m high on its three open sides; three ramps down (north, middle, south), each with a door across its top (which opens as anyone comes up to it) and a wall of tarped crates set back behind the door, so that no line runs into the spawn even with the door open. Eight starts each, tagged with the side |
| The Skim | the north lane | a waxed path from the north ramp to the pump, then zig-zagging along the north wall to the gantry's stairs and on to the other pump; crates and wheels of cheese along it |
| Gouda Pump, Mozza Pump | the north lane, a third of the way in | a concrete island 6 by 6 m with a pump's housing on it (hard cover) and its pipe up to the roof |
| The Moat, The Stirrer | the middle | the Stirrer's island, 8 by 8 m, in a ring of open fondue 2 m wide (jumpable, at a sprint); its gearbox (hard cover across the middle line) under the gantry's platform. Reached by a waxed plank 1.4 m wide from each side, at the end of a path that zig-zags from the middle ramp past low walls and crates |
| The Gantry | over the middle, north to south | a steel bridge 3.2 m up from stairs at the Skim to stairs at the Drain, with a platform over the Stirrer carrying its motor (hard cover) and cheese at its edges to crouch behind: the high ground, open to the whole hall, never eaten. Its stairs have no rails |
| Gouda Boiler, Mozza Boiler | the south lane, near the decks | a concrete island with a boiler lying on saddles along its south edge (hard cover) and its flue to the roof |
| Gouda Rind Run, Mozza Rind Run | the south lane | a covered way of cheese 8 m long over the waxed path from the boiler towards the middle, dark and close, open at both ends |
| The Drain | the south lane, the middle | the waxed path along the south wall to the gantry's stairs and on |
| The Fondue | under the floor | the vat: molten cheese that glows and bubbles (`M_Fondue`), with steam rising from the moat (`M_Steam`) |

Three lanes join each spawn to the other: the Skim (long and zig-zagging, along the north wall), the middle (close, round the Stirrer), and the south (the Rind Run's tunnel, then the Drain). The gantry is the connector between them over the middle, and the high ground; its stairs come down onto the Skim and the Drain. Deck to deck is about 72 m, 11 s at a sprint straight across; by the paths about 15 s.

Free-for-all has fourteen more starts (tagged "Free"), seven a side: on the pump and the boiler, the middle path, the Skim, the Drain, the Stirrer's island and the north path, each on wax or concrete, never eaten.

The mice come in by ten vents low in the walls (`MouseEntry`): four in each long wall near the middle, where the Skim and the Drain run close to it, and one in each end wall north of the decks. Twenty-four mice come, three at most of the ten vents.

The hall: walls of grey corrugated sheet over a concrete dado that goes down into the vat, steel columns every 6 m, a pipe run along each long wall, a roof of panels with strips of skylight, two rows of high-bay lamps, a lamp over each deck and the gantry; lit as an overcast day through the skylights (`kit.light_rig("overcast")`), and from below by the fondue wherever the floor is open.

## The cheese, and what the mice change

668 blocks of cheese, 594 of them edible (twenty-four mice), 74 waxed.

| Structure | Blocks | Before | Eaten |
|---|---|---|---|
| **FloorGouda**, **FloorMozza** | 230 each | The floor: the whole hall can be walked, and the paths are only the red parts of it. | Holes from the walls in, where the mice come, then a lattice of holes and ledges, then the waxed paths and the islands over a lake of fondue; the last tiles left standing alone a mouse leaps onto, eats from on top and goes down with. |
| **WaxPaths** | 74 | Red tiles in the floor. | Never eaten: the only ways across. 2 m wide, zig-zagging, open on both sides. |
| **CoverNorthGouda**, **...Mozza** | 18 each | Crates (1 and 2 m) and wheels along the Skim, and between it and the middle. | Eaten, or sunk with their tiles: the Skim has only the pumps' housings and the gantry's stairs to hide behind. |
| **CoverMiddleGouda**, **...Mozza** | 17 each | Low walls along the middle path, crates at its bends and by the moat. | The middle path is open; the Stirrer's gearbox and the gantry's motor are the cover left. |
| **CoverSouthGouda**, **...Mozza** | 17 each | Crates and wheels round the boilers and along the Drain. | The boilers are the cover left. |
| **RindRunGouda**, **RindRunMozza** | 12 each | A dark tunnel over the south path: close fighting. | Its walls go (or sink with the tiles under them) and its roof planks come down onto the path; the south path lies open over the fondue. |
| **GantryCheese** | 6 | Crouching cover at the edges of the gantry's platform. | The mice climb the stairs for it; the platform is bare. |

The mice eat what is nearest them first: the floor and cover near the long walls' middles, where they come in, so the Skim and the Drain open first, then the field between the lanes, then round the decks and the Stirrer. A mouse eats a tile standing on the next one, so it leaves holes behind it as it goes; it never eats the tile it stands on, nor one another mouse stands on, and never walks off an edge (a hole opened ahead of it, it stops short and finds its way afresh). Cut off on a tile or two it ate the floor round, it leaps across the gap to floor that goes somewhere (`ACheeseStrikeMouse::LeapOut`); floor it can't walk to (an island of tiles, a tile standing alone) it leaps to from the nearest edge, and a tile alone it eats from on top, going down with it. The whole floor is gone about a minute and a quarter after the mice come. A mouse that falls in anyway (another ate the floor from under it) screams, goes under, and comes back in at a vent eight seconds later (`EMouseDoing::Sinking`). Cheese that sinks below the kill height is gone.

The bots walk the navigation mesh, which is rebuilt where the floor goes, and on a map with a kill height they never walk off an edge (`bCanWalkOffLedges`), nor step aside off a path in a fight: they fall in only when the floor under them is eaten, or they are shot off it.

## King of the Hill

King of the Hill's hill goes round three spots (`hills()`, `ACheeseStrikeHillSpot`), a minute at each: **the Stirrer**'s island round its gearbox, then **Gouda's pump** island and **Mozza's pump**, round their housings. The Stirrer is on the middle line and the pumps are twins; the islands are concrete, never eaten. (The Drain and the Skim's paths are too narrow for a hill, two metres of wax between tiles the mice eat.)

## Tests and tooling

`CheeseStrike.Play.EveryMapHasFairHills` checks its hill spots: three to five, each on a floor the mice never eat, clear and level round its middle, on the navigation mesh and reached from both sides' starts, well away from them, the first as far from both and the round favouring neither. `CheeseStrike.Play.TheFondueFloorHoldsTogether` plays the map: the kill height, sixteen side starts at the ends and the free ones, every start on what is never eaten, no line from any Gouda start to any Mozza start even past all the cheese and the doors, nothing under any edible tile but the vat's floor below the kill height, the fondue blocking nothing, a door at the top of each ramp; then eats everything and walks from the Gouda deck to the other deck, a pump, a boiler, the Stirrer, the gantry, the Skim, the Drain and a Rind Run. `CheeseStrike.Play.AfterTheMice.TheFondueFloor` checks every start still has ground and a way to every other. `CheeseStrike.Play.AFallIntoTheFondueKills` drops a player in twice: once just after being shot (the shooter's kill) and once with nobody near ("fell into the fondue", in the feed and on both machines), and checks the body is kept and goes under. `CheeseStrike.Play.MiceOnAFloorTheyEat` eats the floor round a mouse, leaving it on an island, and watches it leap off; then eats the floor from under it and watches it sink and come back. `CheeseStrike.Play.BotsKeepOutOfTheFondue` plays nine bots for forty seconds on a floor half eaten: every death is a kill, none a fall.

`tools/play/fondue_tour.txt` takes pictures from set places; `tools/play/fondue_eaten.txt` the same with most of the floor eaten at once (the console's `CheeseStrikeEatCheese <share> [structure]`); `tools/play/fondue_fall.txt` watches a player fall in; `tools/play/fondue_mice.txt` pictures from above every minute while bots fight and the mice eat (`-Bots -Mice -Extra "?MiceAt=5?KillLimit=200" -TimeoutSeconds 720`). Run each with `tools/play.ps1 -Map /Game/CheeseStrike/Maps/Lvl_FondueFloor -StartAt -3350,0,250,0` (from Git Bash, set `MSYS_NO_PATHCONV=1`). Its lobby picture is `python tools/art/map_picture.py /Game/CheeseStrike/Maps/Lvl_FondueFloor fondue.png -3500 -1900 1100 -32 32`.

Changing the map is a change to `tools/art/build_fondue.py`, then running it (it rebuilds the level from nothing) and the tests; its materials are `tools/art/fondue_material.py`'s.
