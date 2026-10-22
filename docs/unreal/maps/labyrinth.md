# The Labyrinth

The aging cellar under a fromagerie: an octagon about 70 m across under a vaulted brick ceiling, lamp-lit, for up to sixteen players. Round a stone vault in the middle, three rings of walls of stacked cheese wheels between rings of stone pillars make a maze of curved aisles; the gutter runs round the outside along the cellar's walls, where the mice come in through the drains. Each side comes down its own stair at opposite corners, and a spine of waxed aisles winds from each stair into the vault and is never eaten, so the maze always has a known way through. The mice eat the maze from the outside in: walls become gaps, dead ends become shortcuts, and by the end the cellar is an open floor of pillars round the two red spines. It is built by `tools/art/build_labyrinth.py` into `Content/CheeseStrike/Maps/Lvl_Labyrinth`, from the blocks in `tools/art/map_kit.py` ([principles.md](principles.md) says how maps are made).

## The art

Its walls, pillars and vault are dressed stone, its ceiling old red brick, its floor flagstones, its gallery, racks and doors old planks and studded oak: Poly Haven textures (CC0) that `tools/art/polyhaven.py` downloads and `tools/art/import_polyhaven.py` imports and makes into materials of `M_Surface`, drawn from world space on the map's blocks (`Content/LICENSES.md` names each). The cheese is wheels, not blocks: `tools/art/wheel_mesh.py` makes `SM_CheeseWheel`, a wheel as it is made, its sides a little rounded out and its edges rounded off, so that a stack of them reads as wheels and a wall of stacks as a wall of wheels; the kit's wheels take it as their `mesh`. The spines and the Mother Wheel are wheels in red wax (`M_CheeseWax`): what the mice never eat.

The ceiling is overhead (`kit.overhead`): it blocks nothing, so the streaks' wheels fall through it, shots and sight pass it, and the minimap and the navigation mesh leave it out. It is tagged `Ceiling`, and `CheeseStrikeHideTagged Ceiling` (a console command, `Source/CheeseStrike/Map/CheeseStrikeMapCommands.cpp`) hides it, for looking at the map from above; `CheeseStrikeHideTagged Ceiling 0` brings it back.

It is lit by its lamps, and by daylight down the vault's oculus: the sun (the `noon` rig, turned nearly overhead) falls through it onto the Mother Wheel, and its glow on the vault's dome shows over the maze's walls. Each part has a light of its own, so that a glance says where you are: warm over the north and south aisles, cool over the west and east, amber over the spines, the gutter's lamps dimmer and yellower; Gouda's stair is lit orange-warm with an orange band along its screen, Mozza's cold blue-white with a blue one; the Brine Room cold, the Smoke Room ember-orange. The stone pillars stand in lines along the rings, every 15 degrees in the outer ring, 22.5 in the middle and 45 in the inner, with ribs between them under the ceiling: they show which ring you are in, and stay when the walls go.

## Layout

Angles are from east, anticlockwise; north is +y. The map turns about its middle: whatever Gouda's half has, Mozza's has turned half round (x, y to -x, -y), so neither side has the better half. Gouda comes down in the south-west, Mozza in the north-east.

| Place | Where | What it is |
|---|---|---|
| Gouda Stair, Mozza Stair | behind the south-west and north-east faces | a side's start: a stone room with three doorways onto the cellar, the spine in the middle and the gutter either side; a stone screen across it behind them, with a way round at either end, so that no line from the cellar reaches the eight starts behind it (tagged with the side); the stair from the shop along its back wall, up to a landing and a shut door |
| The Vault | the middle, 17 m across | a round stone room with six arches: the four ways (north, south, east, west) and the two spines (south-west, north-east). In the middle the Mother Wheel, a wheel 6 m across in red wax on a stone plinth, under the oculus; low stacks of wheels round its floor |
| The Gallery | round the inside of the vault, 3.4 m up | two halves, each with a stair up from the vault's floor, a parapet of small wheels to crouch behind (a rail stays when it is eaten), and a bridge of cheese on waxed beams out to the top of the Mother Wheel, which joins the two |
| The Ambulatory | the aisle round the vault (ring 1) | 4.4 m wide, all the way round, with low stacks of wheels; the vault's arches open onto it |
| West 2, North 2, South 2, East 2 | the middle ring (ring 2) | curved aisles 4 m wide, between the inner and middle walls |
| West 3, North 3, South 3, East 3 | the outer ring (ring 3) | curved aisles 4 m wide, between the middle and outer walls |
| Gouda Spine, Mozza Spine | from each stair into the vault | in from the stair's middle doorway across the gutter, a quarter-turn along ring 3, in through the middle wall, back along ring 2, and in through the inner wall to the vault's arch: walls of wheels in red wax on both sides, never eaten |
| West, North, South and East Gutters | round the outside, between the outer wall of wheels and the cellar's walls | 6 to 9 m wide; racks of wheels along the walls, two drains in each side face (where the mice come in), stacks of wheels to hide behind, a column of wheels where it turns past each room |
| The Brine Room, The Smoke Room | behind the north-west and south-east faces | a stone room with two doors from the gutter (`ACheeseStrikeDoor`, opening as anyone comes up), either side of a stone cistern across the gutter that stops it there: the gutter's way past is through the room. The Brine Room has stone tanks with wheels soaking in them; the Smoke Room a brick smoker; both racks of wheels |

The maze. From each stair three lanes lead out: the spine, the gutter to the left and the gutter to the right. The gutters (the outer lanes) are long and open, the spine winds and is lit and known, and the rings between are the maze: curved aisles whose walls have gaps into the next ring, and walls of wheels across them (from pillar to pillar) that make them into stretches. Each side's spine opens onto the maze where it turns: Gouda's onto West 3 as it reaches ring 3, onto South 2 as it reaches ring 2, and onto the ambulatory at the vault; the other two turns are closed by walls of wheels, dead ends on the maze's side that the mice open into shortcuts. A sprint from a stair to the vault along the spine takes about 10 seconds, to the nearer room along the gutter 8, to the other side's stair 17. Gouda reaches West 3, West 2 and South 3 first, Mozza their turned twins; the vault and the two rooms are where the sides first meet, 8 to 10 seconds in.

Lines are short in the maze (the aisles' curve and the wheels in them keep them under about 25 m), longer along the gutters (up to about 45 m along a side). No line runs from stair to stair: the Mother Wheel stands between the spines in the vault, and each stair's screen hides its starts from everything outside, before the mice and after.

Its places are named (`ACheeseStrikePlace`): the stairs, the vault, the gallery, the Mother Wheel, the ambulatory, the aisles by ring and side (West 2, North 3, ...), the spines, the four gutters and the two rooms. A place that isn't a box (a ring's aisle) is a few boxes on a 2.5 m grid (`kit.place_areas`), none overlapping another's.

Free-for-all has sixteen more starts (tagged "Free"), eight a half: in rings 2 and 3, the ambulatory, the gutters and the rooms.

The mice come in at the drains, two in each side face, eight in all, and land 5 m in, in the gutter. The bots and mice walk a navigation mesh built when the level starts and rebuilt where the cheese goes; the doors don't block it.

## The cheese, and what the mice change

About 680 blocks the mice can eat, and about 140 they can't (the spines, the Mother Wheel, the bridges' beams): the full eight mice. They eat 75 to 85 blocks a minute here, so the maze is gone by about nine minutes after they come: the gutter first, then the outer ring, the middle, the inner, and last the gallery's parapet and bridges.

| Structure | Before | Eaten |
|---|---|---|
| **GutterRacks**, **GutterStacks**, **GutterTurns** (each side's) | Racks of wheels along the cellar's walls; stacks to hide behind in the gutter; a column where it turns past a room, breaking its line. | The gutter is an open lane along the walls: long lines from room to stair. |
| **West/North/South/EastWall3** (the outer ring of wheels) | Closes the maze from the gutter but for a few gaps. | The gutter and ring 3 are one; the maze is entered anywhere. |
| **West/North/South/EastWall2**, **...Wall1** | The middle and inner rings: the maze's walls, three wheels high (3 m), each column falling a wheel at a time as its foot is eaten (3 m, 2 m, 1 m: cover that steps down). | The rings join: gaps, then shortcuts, then an open floor of pillars, the lines growing from 20 m to the cellar's width. |
| **Plug2_...**, **Plug3_...** | Walls of wheels across an aisle from pillar to pillar: the stretches of the maze, and the closed turns of the spines. | Dead ends become shortcuts: West 2 into Gouda's spine, South 3 off it (and Mozza's twins). |
| **GoudaSpine**, **MozzaSpine** (wax) | Each side's winding way to the vault. | Never eaten: at the end two red serpentines in an open cellar, the cover that is left. |
| **AmbulatoryStacks**, **AisleStacks**, **VaultStacks** | Low stacks to crouch behind round the vault and in the aisles. | The vault's floor and the ambulatory are bare round the Mother Wheel. |
| **ParapetGouda**, **ParapetMozza** | The gallery's low wall of wheels: crouch cover over the vault. | Bare rails: whoever is on the gallery is seen from the whole floor. |
| **BridgeGouda**, **BridgeMozza** | A deck of cheese from each half of the gallery to the top of the Mother Wheel. | Two waxed beams each: a balance walk; the gallery's halves are joined only through the vault. |
| **BrineRoom**, **SmokeRoom** | Wheels in the tanks, by the smoker and on the racks. | Bare racks and tanks. |
| **MotherWheel** (wax) | The landmark in the vault, blocking the line between the spines. | Never eaten. |

What anyone stands on is never eaten from under them except the bridges' decks, which leave beams.

## Tests and tooling

`CheeseStrike.Play.TheLabyrinthHoldsTogether` plays the map: eight starts a side and free-for-all's, every structure there (the maze's walls edible, the spines and the Mother Wheel all wax), its size, the lines its notes promise (the spine's wax wall hides it from the gutter, wall 2 and wall 3 close their aisles, a wall of wheels closes aisle 3, the Mother Wheel stops the line between the spines, the gallery is a floor over the vault, and no line from the cellar in front of a stair reaches a start in it), and a walk up a gallery stair, over a bridge to the Mother Wheel and through a door into the Brine Room. `CheeseStrike.Play.BotsAndMiceInTheLabyrinth` checks a path from Gouda's stair to every part of it, plays fifteen bots and the mice, and checks the bots spread through the vault, the maze and the gutter and fight, and the mice eat from the outside in. `CheeseStrike.Play.AfterTheMice.TheLabyrinth` eats all its cheese and checks every start has ground and a way to every other. `tools/play/labyrinth_tour.txt` takes pictures of it from set places (the first two from above with the ceiling hidden), and `tools/play/labyrinth_mice.txt` pictures from above every minute and a half as the mice eat it, with bots fighting (`tools/play.ps1 -Map /Game/CheeseStrike/Maps/Lvl_Labyrinth -StartAt -1150,0,100,0`; from Git Bash, set `MSYS_NO_PATHCONV=1` so the path isn't rewritten). Its lobby picture, looking along West 3 to where Gouda's spine turns into it, is `python tools/art/map_picture.py /Game/CheeseStrike/Maps/Lvl_Labyrinth labyrinth.png -2220 -1035 180 -2 300 --start -1150,0,100,0`.

Changing the map is a change to `tools/art/build_labyrinth.py`, then running it (it rebuilds the level from nothing) and the tests. Its order: `polyhaven.py` (with Python, to download) and `import_polyhaven.py` (in the editor), and `wheel_mesh.py`, then `build_labyrinth.py`.
