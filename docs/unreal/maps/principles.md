# Making a map

How CheeseStrike's maps are designed and built: what Counter-Strike's maps teach, what is particular to a game whose cover is eaten, the sizes and budgets to build to, how to test a map, and the maps to build next. [The Creamery](creamery.md) is the worked example throughout. A map is a script (`CLAUDE.md`, "Maps are built by scripts") built from the blocks in [`tools/art/map_kit.py`](../../../tools/art/map_kit.py); its notes go beside this file.

## The idea

The cover is cheese, and a minute into a match giant mice come over the walls and eat it. A map starts closed (short lines, lots of cover, few ways through) and opens as the match goes on: walls go, buildings come down to their frames, the long lines appear and the fight speeds up. Every map is designed for both states and for the change between them. Most of what stands in a map is edible; what never goes is the walkable skeleton: floors, frames, stairs and the starts.

## What Counter-Strike teaches

Counter-Strike's maps (Dust2, Mirage, Inferno, Nuke) have been played for twenty years. What makes them work carries over, even though CheeseStrike's modes are deathmatches rather than bomb plants:

- **Three lanes.** Each side's spawn connects to the other's by three routes: two outside lanes and a middle that touches both. Each lane has its own character (long and open, close and twisting, high and low), so a player picks a fight by picking a lane. The Creamery has the yard (long, containers), the hall (close, racks, two floors) and the canal (sunken, bridges).
- **Chokepoints.** Each lane narrows at least once: a door, a tunnel, a gap between two stacks. A choke is 2 to 4 m wide; a lane 6 to 10 m; an open area 15 to 25 m across. Chokes are where fights happen, so give the attacker a way to approach one without being seen from a single safe spot (a second way in, cover a few metres short of it, a flank).
- **Sightlines.** Short lines (under 10 m) favour close fighting; medium (10 to 30 m) the rifle; long (30 to 60 m) whoever holds still. Give each lane one long line at most, and break the rest with cover. Never a line from spawn to spawn, or into a spawn. Make doorways meet lanes at an angle, so that whoever comes through one is seen by a few spots, not the whole lane.
- **Connectors.** Between the lanes, one or two short ways across (Mirage's connector, Dust2's mid doors), so a side can shift its weight. Each is shorter than going round, and dangerous to cross.
- **High ground.** A few raised spots (a balcony, a roof, a catwalk): each with a way up that is exposed, and a spot that answers it. High ground with no answer decides matches.
- **Timings.** Work out how long it takes from each spawn to each place at a sprint (6.75 m/s; 4.5 m/s walking). On a mirrored map both sides reach the middle together; on an asymmetric one, measure and even them out. The first fight should come 8 to 12 seconds after the start. Dock to dock across The Creamery is about 20 seconds.
- **Cover spacing.** On a lane, something to get behind every 8 to 12 m, closer in the open; a mix of crouch cover and full cover, so that a fight has steps.
- **Readable places.** Each place looks unlike the others, by shape and colour, so a player knows where they are from a glance and can name it. Landmarks that show over the walls (a chimney, a tower) help players find their way.
- **Team-sided spawns.** Eight starts a side, at their end, spread so that one grenade can't catch them, facing the way out, tagged with the side (`CheeseStrikeModes::TeamStartTags`, "Gouda" and "Mozza"; `kit.team_starts`). Hard cover in front of each spawn, so the open map after the mice never has a line into it. Free-for-all adds starts through every lane.

## What is particular to CheeseStrike

### A map is played twice

Design the closed map and the open map, and the way from one to the other. For each structure write down what it does before the mice and after them; each map's notes have that table (The Creamery's "The cheese, and what the mice change"). Good structures change the fight when they go: a wall of wheels that closes a lane and, eaten, opens a 130 m line; a seal on a door that, eaten, joins two lanes; a house that is a fortress and becomes an open deck. A structure that is eaten without changing anything is only clutter.

Order the change. The mice eat what is nearest them and lowest first (below), so decide which structures should go in the first two minutes, which by the middle of a match and which last, and place them and the mice's ways in to suit.

### What the mice never eat

Three things must survive the mice, however long a match lasts:

- **The walkable skeleton.** The floors, frames, decks, stairs, ramps and bridges' beams that make the map a place you can cross. Build them of anything but cheese (concrete, steel), or of cheese in wax.
- **The starts.** Nobody starts over a hole: what a start stands on, and the ground round it, is never eaten.
- **A way out of every place.** No spot that a player can reach before the mice becomes a spot they can't leave after them.

Cheese the mice must leave is marked not edible (`ACheeseStrikeCheese::bEdible`, `edible=False` in the kit): the mice never go for it, and it wears a rind of red wax (`M_CheeseWax`), so that players learn that wax means it stays. `kit.keep(low, high)` marks every block whose middle is in an area, and `kit.team_starts` keeps 1.5 m round each start. The Creamery's catwalks stand on beams of waxed cheese.

`CheeseStrike.Play.AfterTheMice.<map>` checks it for every map in `CheeseStrikeRules::Maps`: it eats every edible block at once, lets what was on them fall and the navigation mesh rebuild, and checks that the kept cheese is where it was, that nothing stands on nothing, that every start has ground under it, and that there is a way on the navigation mesh from the first start to every other.

### How fast the mice eat

- A block has 400 of eating in it, whatever its size; a mouse eats 40 every half second, so a block takes one mouse five seconds. A 1 m cube and a 3 m slab go as fast.
- One mouse comes for every 60 edible blocks, two at the least and eight at the most: a map with 450 blocks or more gets all eight.
- With the walking between blocks, a mouse gets through about **nine or ten blocks a minute**; eight mice **70 to 80 a minute** (measured on The Creamery, `tools/play/creamery_mice.txt`). A ten-minute match eats about 650 blocks after the mice's first minute.
- So size the cheese to the match. The Creamery has 1,026 edible blocks: the plazas, docks and houses go in the first four or five minutes, the hall's 360 wheels and the Wheel Stack last. A map that should be bare by its end has fewer, larger blocks; one that should keep its middle has more there.
- A mouse goes for the cheapest block: its distance, plus four times its height above the mouse's feet, plus 13 m if another mouse has it. So the mice eat near where they come in, low down first, and spread out. Put the ways in (`kit.mouse_entries`, ten on The Creamery for eight mice) where you want the eating to start, mirrored on a mirrored map, and never on top of a spawn.
- A mouse reaches 2.6 m across from its middle and 2.5 m up from its feet, and eats standing beside a block, never on top of it. What is out of reach is eaten when what is under it is eaten and it comes down, or by a mouse that walks up to it (they climb ramps and stairs on the navigation mesh). A block with nowhere on the navigation mesh beside it to stand is never eaten: leave a mouse room (a metre and a half) round what it should eat.
- Shooting a mouse (300 of damage) sends it off for six seconds: players can protect a structure for a while.

### Drops that kill

A map may have a drop that is death: a vat of fondue, a canyon. `kit.kill_below(z)` sets the level's kill height; whoever goes under it is killed by the fall (`Rules/CheeseStrikeFalls.h`), a kill for whoever hurt them in the five seconds before, else "fell into <the place it ends in>" in the feed (so name the drop's place with a common noun: The Fondue). On such a map the bots never walk off an edge, nor do the mice; a mouse that falls in anyway comes back over the edge a while later, and cheese that comes to rest under the kill height is gone. Put the kill height where a falling player's feet reach the drop's surface, so that the view, which stays where the eyes were, stays above it. What is left once the mice have eaten must not be an easy walkway by accident: a ledge or beam a player can stand on (a player perches on anything under the edge of their 68 cm capsule) is a way across; the waxed paths should be the ways across. `kit.liquid` makes a pool's surface that blocks nothing.

### What falls

When a block is eaten, or falls, every block resting on it is checked: if what is still under it holds it up, it stays; otherwise it falls, straight down, until it comes onto something that holds it. A block is held when its middle is inside what it rests on, taken together (`CheeseStrikeMice::IsHeld`, tested in `CheeseStrike.Mice.WhatHoldsCheeseUp`): on a block under its middle, or on two walls either side of it, but not on one wall at its edge, nor half on a block beside its middle, nor on a thin post. A falling block passes by a post or the edge of a wall it would only teeter on. So build cheese that stands on something:

- **Walls in columns.** A wall is columns of blocks, each on the one below (`kit.cheese_wall`); eaten at its foot, a column comes down a block, and keeps coming down as the mice eat its new foot. Don't bond walls like bricks: a block resting half on each of two below it falls when either goes.
- **A lintel over every opening**, a column wider than the opening at each end, bearing on the wall either side (the kit makes them). It falls once one side is eaten, and what is over it comes down with it. Keep openings out of a wall's end columns (the kit warns), and give two openings a column between them or a lintel they share.
- **Floors and roofs on two sides.** A plank of cheese across a room, a tunnel or a canal rests on a wall at each end, or on a wall and a steel ledger, or on beams under it. A floor of tiles rests on beams.
- **A floor that falls.** A deck of slabs each on one pillar under its middle (`kit.pillar_deck`) comes down a slab at a time, whole, onto the level below as the mice eat its pillars, with whoever is on it (nobody is hurt by falling). Never two pillars under a slab: it falls when either goes, and comes down through the one left. Keep the level below clear under the slabs but for their pillars, so that each lands flat. [Gruyère Grotto](grotto.md) is built of them; `CheeseStrike.Play.AGrottoDeckFallsOntoTheLevelBelow` checks that whoever is under a slab or on it ends up standing, not inside it.
- `CheeseStrikeUnheld` in the console lists every block standing on nothing that holds it: a map's mistake, or a fall that went wrong. `CheeseStrike.Play.AHouseComesDownFromItsFoot` eats a Curing House course by course and checks that nothing is ever left standing on nothing.

### Buildings of several floors

`kit.building` builds a house of cheese, of as many storeys as asked (3.5 m each): walls of cheese in columns (2.4 to 2.6 m wide) and courses (a sill to 1 m, a window course to 2.2 m, a lintel course to the storey's top), its upper floors concrete decks on a steel frame inside the walls, stairs between them, doors on the ground floor, windows anywhere, and a roof of cheese planks from wall to wall. The mice eat it from the ground: each column comes down a block at a time, and the upper storey's walls sink with it until they too are at the mice's height; a roof plank falls onto the top floor when a wall under one end goes. What is left is the frame: decks on posts with their stairs, high ground with nothing to hide behind. That is the change to design for: a fortress that becomes a platform.

Keep each building to about 100 blocks (a 12 by 14 m house of two storeys is 97). Put lamps inside (the kit puts one in each storey), and windows on the sides that matter: a window is a long line out, and a long line in.

### Tunnels, bridges and doors

- **Tunnels** (`kit.tunnel`): walls of cheese and a roof of planks, dark and close until eaten, then an open lane. Built against a wall that isn't cheese (a shed's), it leaves that side out and hangs its roof on a steel ledger. Side ways in have a lintel over them. The kit lights them every 6 m.
- **Bridges** (`kit.bridge`): a deck of cheese tiles on beams the mice never eat (steel, or cheese in wax). Eaten, a bridge is its beams: a beam 40 to 60 cm wide is a balance walk for a careful player, but too narrow for the bots and mice (the navigation mesh needs 70 cm). Give a bridge rails on beams at its sides; rails stay when the deck goes.
- **Round and slanting things.** `kit.wheel_arc` and `kit.wheel_line` build walls of stacked wheels along a circle or a line at any angle, sized to fit; give them `mesh=` the wheel from `tools/art/wheel_mesh.py` (rounded edges, so a stack reads as wheels) and an `overlap` of about 14 cm so no slit shows where they meet. `kit.round_wall` builds a stone ring with openings, `kit.wall_between` and `kit.stairs_between` a wall or a flight at any angle, `kit.cheese_rack` a rack of wheels on shelves whose frame stays when they are eaten. The Labyrinth is built of these.
- **Doors** (`kit.door`, `ACheeseStrikeDoor`): no keys and no buttons. A door slides open as anyone comes within 3.5 m of it (a player, a bot, or a mouse, whose reach counts from its nose) and shuts when nobody is near; shut, it hides who is behind it and stops shots. The navigation mesh runs through it, so bots and mice path through and it opens before they reach it. Doors make good chokepoints and connectors: whoever is behind one doesn't know who is coming. The cheese round a door can be eaten; its steel jambs stay.

### Sizes at life scale

The world is life size, in centimetres (`CLAUDE.md`, "Scale"):

| Thing | Size |
|---|---|
| A player | 180 cm tall (a capsule 192 by 68), eyes at 160 cm standing and 108 crouched |
| Steps and jumps | a player steps up 20 cm without jumping, and jumps about 55 cm |
| Speeds | walk 4.5 m/s, sprint 6.75, crouch 2.25 |
| Stairs | risers of 18 cm at most, treads of 28 cm or more: 20 steps and 6 m a storey |
| Ramps | 30 degrees or less (players manage 45, the navigation mesh 44) |
| Doorways | 1.2 to 2 m wide, 2.2 m high; a corridor 1.6 m wide, 2.5 m for two abreast |
| A storey | 3.5 m, floor to floor |
| Gaps the bots and mice walk through | 70 cm at least; nothing between 20 and 35 cm high that bots should not step up (their mesh steps 35 cm, players 20) |
| A mouse | a body 3.4 by 1.6 by 1.5 m; eats 2.6 m across and 2.5 m up; walks 2.7 m/s, runs 6 m/s |

### Cover

- **Under 55 cm**: something to step or jump over, not cover.
- **90 to 110 cm**: crouch cover. A crouched player is hidden; standing, they shoot over it. Window sills are 1 m.
- **2 m and over**: full cover.
- Rails, posts and frames are not cover: shots and sight pass them. Once the cheese goes, what is left of a structure is mostly these.

### Names for places

Every part of a map a player would tell a teammate about has a name (`kit.place_name`, `ACheeseStrikePlace`; the HUD and kill feed show them, as "Nice shot · The Squeeze"). Name a place for what it is ("The Squeeze", "The Gallery"), and mirrored places by their side ("Gouda Dock", "Mozza Curing House"). Where places overlap, the smaller wins, so a room can be named inside a yard.

### A roof over a map

A cellar or a cave has a ceiling: make it `kit.overhead` (`kit.ceiling` for a slab), so it blocks nothing: the streaks' wheels fall through it, shots and sight pass it, and the minimap and the navigation mesh leave it out; nobody can reach it anyway. It is tagged `Ceiling`, and `CheeseStrikeHideTagged Ceiling` hides it in the game for pictures from above (`tools/play/labyrinth_tour.txt`). Close it all round (walls up through it, its pieces overlapping), or daylight leaks in at its edges; an opening in it (The Labyrinth's oculus) brings in the sun.

### Lighting for readability

`kit.light_rig` sets the sun, sky, clouds, haze and the picture's grading from presets (day, noon, dusk, overcast, night, cave). Light for reading first: players must be seen against what is behind them.

- Keep the sun out of either side's eyes down a lane: on a map mirrored west to east, light it from the south, across the lanes.
- Every place a player can stand is lit well enough to be seen in: lamps inside buildings and tunnels (the kit adds them), and under decks.
- Cheese is bright and warm; keep the rest of a map's palette quieter, so that the cheese reads as the thing that changes.
- A night or cave map is lit by its lamps; give each place its own light so the dark is between places, not in them.

### Performance budgets

- **Actors.** The Creamery has about 1,800: some 750 static blocks and props and 1,026 blocks of cheese. Keep a map under 2,500, and its cheese under about 1,200 (each block is a replicated actor, dormant until bitten; each mouse looks at every block once a second, and each block eaten looks at what rests on it).
- **Draw calls.** Build blocks from the engine's cube and cylinder with the shared materials (`kit.floor`, `kit.painted`, `kit.sheet`): the renderer draws many alike in one go. Use a few more materials, not many. Fab models are imported with Nanite and box collision (`import_fab.py`), textures cut to 2K or 1K.
- **Lights.** Lumen lights everything; keep moving and shadowed lights to about 20 a map.
- **Check it.** Play the map at the game's default settings with `stat fps` on, looking down each lane (manual test MAP-05 for The Creamery).

### Navigation for bots and mice

- `kit.nav_bounds` covers all the playable ground, from the lowest floor to above the highest. The mesh is built when the level starts and rebuilt where cheese goes (`Config/DefaultEngine.ini`), so the bots walk through gaps the mice open; their paths are found afresh as it changes.
- The mesh is for an agent 34 cm in radius, 144 cm tall, stepping 35 cm and climbing 44 degrees. Anything narrower than 70 cm is a wall to the bots and mice.
- Doors don't block it; bots walk up and the door opens.
- The mice walk the same mesh. A mouse that falls through an eaten floor finds its way again from where it lands.
- `CheeseStrike.Play.BotsPlayTheCreamery` checks a path from a dock to each lane and floor and watches bots spread through the lanes and fight; write the same for a new map.

## Testing a map

1. **Build it.** Run its script (headless, as `map_kit.py` says). The report in `Saved/<level>.txt` counts what was built and warns of fewer than sixteen starts, cheese without mouse entries, and openings whose lintels would rest on one side.
2. **Look at it.** Copy `tools/play/creamery_tour.txt` (views from fixed places, `cmd CheeseStrikeViewFrom x y z pitch yaw`) and run it with `tools/play.ps1 -Map <level>`; read every picture. Then `tools/play/creamery_mice.txt` with `-Bots -Mice -Extra "?MiceAt=5?KillLimit=50"`: pictures from above every minute while the mice eat, and the cheese left in the log (`CheeseStrikeCheeseLeft`).
3. **Test it.** Add it to `CheeseStrikeRules::Maps`, and `CheeseStrike.Play.AfterTheMice.<map>` runs on it. Write a test like `CheeseStrike.Play.TheCreameryHoldsTogether`: its starts and structures, the lines its notes promise (traces that must hit a structure, or nothing), and a walk up every ramp and stair and across every bridge.
4. **Play it.** With bots (`-Bots`): do they reach every lane and fight there? With mice: does the map open as its notes say?
5. **Its picture.** `python tools/art/map_picture.py <level> <file> x y z pitch yaw` takes the lobby's picture of it from a view that shows it best and saves it into `Content/UI/Maps/` (from Git Bash, set `MSYS_NO_PATHCONV=1`, or the level's path is rewritten).

## Starting a new map

1. Lay it out on paper first: lanes, chokes, high ground, spawns, with distances and the seconds a sprint takes; the structures and what each does before and after the mice; the mice's ways in.
2. Start the script `tools/art/build_<map>.py` from the example in `map_kit.py`'s notes: ground, boundary, starts, mouse entries, navigation bounds, light. Build it and walk it.
3. Add the structures with the kit. Mirror a symmetric map with `kit.mirrored` (x given from the middle, times s).
4. Build, look, test, play, as above, until it plays as its notes say.
5. Write its notes in `docs/unreal/maps/<map>.md`: its size, layout, places, the structures table, its tests.
6. Register it: an entry in `CheeseStrikeRules::Maps` (its name, level and lobby picture), `MapsToCook` in `Config/DefaultGame.ini`, and a row in `Content/LICENSES.md` (and credits) for any new asset.

## The maps to build next

The design's list of arenas ([design.md](../design.md#arenas)) is a starting point; these five come next, each unlike the others. Each is for sixteen players unless it says otherwise.

**The Labyrinth** (built: [labyrinth.md](labyrinth.md)). The aging cellars under a fromagerie, about 70 by 70 m and lamp-lit: a maze of walls of stacked wheels, 3 m high, with dead ends, loops and three doors, between two spawn chambers at opposite corners and a round vault in the middle where the lanes of the maze meet. A spine of corridors with walls of waxed cheese runs from each spawn to the vault and never goes, so the maze always has known ways through; everything else is edible. The mice come in from grates round the edge and eat the maze from the outside in: walls become gaps, dead ends become shortcuts, and by the end the cellar is an open floor of pillars round the waxed spine. Its signature is the maze itself: at the start you fight round corners and listen for footsteps; by the end you see across the whole cellar.

**The Fondue Floor** (built: [fondue.md](fondue.md)). A cheese hall over a vat of molten fondue, 72 by 40 m: the whole floor is tiles of cheese, each a pillar standing in the vat with nothing else under it, with two raised spawn decks at the ends, a few islands of hard cover, a gantry over the middle, and paths of waxed tiles that zig-zag between the spawns and the islands and never go. Falling into the fondue kills (`kit.kill_below`). The mice eat the floor tile by tile, from the walls where they come in, so what was a floor becomes a lattice of holes, then narrow ledges and the waxed paths. Its signature is that the ground itself disappears: late in a match every step is a choice, and a knock back is a death.

**Rind Market** ([built](market.md)). A town square at dusk, about 80 by 80 m: market stalls of cheese under awnings fill the square, two-storey cheese houses with balconies and arcades line it, three alleys lead in from each side's end, and a stone clock tower in the middle (never eaten, stairs inside, a balcony at the top) is the landmark and the high ground. Lamps come on round the square. The mice eat the stalls first, then the house fronts, so the crowded market becomes one open square watched from the balconies and the tower, and the houses become frames you can see through. Its signature is the tower and the arcades: a vertical fight round one middle that everyone can see.

**Gruyère Grotto.** A cave, about 60 by 50 m, lit by lamps and glowing pools: three levels of ledges, joined by ramps and rope bridges, held up by pillars of cheese. The mice eat the pillars, and a ledge whose pillars are gone comes down onto the level below (built as decks of cheese on pillars, falling as a block when its last pillar goes), so the cave loses its upper floors over a match and the fight drops to the bottom. Its signature is collapse: whole floors fall, and the high ground of the first minutes is rubble by the end.

**Crumb Kitchen.** A giant's kitchen, where the players are the size of a mouse in a real one, about 70 by 50 m: a worktop is a plateau 4 m up, reached by a fallen ladle and a stack of cookbooks; a chopping board with a wedge of cheese the size of a house, a toaster, a kettle, a spice rack and a grater as cover; the floor below with chair legs like pillars. Here the mice are the size of a real mouse to the giant, so still car-sized to the players. They eat the giant wedge and the crumbs, opening the worktop, while the kitchen's things (never eaten) stay. Its signature is scale: familiar things made huge, and fights across a worktop and down onto the floor. Its props are Fab models scaled up; its lighting is a warm interior with a window's sun.

Later, toward ten: **Curd City** (the large map: streets of cheese houses of two and three storeys, warehouses, a train beyond the fence), **Double Cream Dairy** (tanks, a gantry with a lift, a cold store behind a door), **Cheddar Canyon** (desert mesas of cheese, eaten down to their rock cores), **Brie Box** (small and tight, for two to eight) and **Cracker Duel** (for two).
