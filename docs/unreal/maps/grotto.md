# Gruyère Grotto

A cave on three levels, about 92 by 56 m inside its walls and 19 m from its floor to its roof, for up to sixteen players, lit by lamps and glowing pools. It is built by `tools/art/build_grotto.py` into `Content/CheeseStrike/Maps/Lvl_Grotto`, from the blocks in `tools/art/map_kit.py` ([principles.md](principles.md) says how maps are made). Its levels are rock: the cave's floor, ledges along its walls 4 m up, and galleries along the north wall 8 m up, joined by stairs cut in the rock and two rope bridges. Decks of cheese widen the upper levels, each slab held up on one pillar of cheese; the mice eat the pillars, and the slabs come down whole onto the level below, so the cave loses its upper floors over a match and the fight drops to the bottom. Its signature is collapse: the high ground of the first minutes is gone by the end.

## The art

The cave's rock is Megascans from Fab: the Old Mine pack's mine rock walls and coal mine floor drawn on its blocks from world space (`M_Surface`, as The Creamery's surfaces are), and two quarry cliffs, recoloured to the mine's rock, standing along its walls and the Spine and hanging from its roof in crags (stacked where a face is taller than a cliff, each overlapping the one below). Where a player can reach a cliff it stops them by its box, so the rock they see is the rock that stops them; elsewhere it has no collision. The roof has none: nobody reaches it, and the streaks' wheels fall through it onto the cave as onto open ground. The Old Mine pack also gives the mine carts on rails on the floor, the bulkhead lamps on the rock, the timber props in the hollows; the hollows have barrels and crates from the Warehouse pack. `tools/art/grotto_materials.py` makes the Grotto's materials: the rock and floor instances, the props', lamp glass that glows (`M_Lamplight`), and the pools' water (`M_GrottoPool`, still and dark, glowing teal from below; unlit, the puddles where the cave drips). The bridges' beams are cheese in red wax (`M_CheeseWax`): cheese the mice never eat.

It has no sun or sky (the light rig's `cave`): each place is lit by its own lamps, in its own colour. The Gouda hollow is lit gold and the Mozza hollow ice white, the galleries by amber lanterns on timber posts, the Colonnade by lanterns on the shelf's face, the south walk and the Undercroft by white bulkhead lamps, the Pulpit by red lanterns, the pools glow teal, and big soft lights high under the roof lift the dark between the lamps enough to see someone in it. A thin haze hangs in the cave (volumetric fog, lit by the lamps), and the eye is kept from brightening the dark into day (auto exposure held between EV100 -3.5 and 3). About twenty lights cast shadows.

## Layout

West is The Gouda Guys' end, east the Mozza Radicals'; the map is mirrored across the middle (x to -x). North is +Y.

| Place | Level | What it is |
|---|---|---|
| The Hollows (Gouda Hollow, Mozza Hollow) | middle (4 m) | each side's spawn, a chamber of rock 10 m deep across the cave's end, walled off from the cave but for three ways out: up its stairs to its landing and the gallery, out of its opening in the middle and down its stairs to the floor, and along its south end onto the walk. Eight starts each, facing the middle, tagged with the side. A wall of cheese in front of its opening hides it from the stairs until eaten |
| The Galleries (Gouda Gallery, Mozza Gallery) | top (8 m) | a walk of rock about 4 m wide along the north wall, from the hollow's landing to stairs down to the Pulpit, widened by a deck of cheese 8 m deep over the floor, on 8 m pillars, with a low edge to crouch behind. The high ground over the floor |
| The Colonnade | floor | the floor under each gallery's deck: a forest of tall cheese pillars, lit by lanterns on the shelf's face |
| The Pulpit | middle (4 m) | a balcony of rock in the north middle, over the Well, with a low edge of cheese; stairs down to the Well either side, stairs up to each gallery, and the North Span south from it. The Curtain hangs from the roof over it, well above the heads of those on it |
| The Spine | floor to roof | a column of rock about 10 by 12 m in the middle, an arch through it at the middle level, north to south, for the bridges. It stops every line along the cave's middle |
| North Span, South Span | middle (4 m) | rope bridges, decks of cheese on waxed beams with rope rails: from the Pulpit to the Spine's arch, and from the arch to the landing |
| The Well | floor | the floor round the Spine, with its glowing pools north and south of it |
| The Pools (Gouda Pools, Mozza Pools) | floor | the floor between the hollows' stairs and the Well: a glowing pool each side, the Cheese Store, boulders, mine carts, stacks of wheels and blocks |
| The Cheese Stores | floor | a house of cheese one storey high on each side of the floor, south of the pool, a door at either end and a window to the north and the south |
| The Ledges (Gouda Ledge, Mozza Ledge) | middle (4 m) | the walk of rock about 5 m wide along the south wall, widened by a deck of cheese 8 m deep over the floor, on 4 m pillars, with a low edge. A buttress and a fin of rock on the walk overlap, so it zig-zags between them |
| The Undercroft | floor | the floor under each Ledge's deck, among its pillars, lit by bulkhead lamps |
| The Rind Tunnel | middle (4 m) | a covered way of cheese along the middle of the south walk, 30 m long, with a door in its north side onto the landing |
| The Landing | middle (4 m) | rock in the south middle where the South Span comes down, by the tunnel's door, with stairs down to the Well either side |

So each side has three lanes from its hollow, one a level: the high one along the gallery, which dips to the Pulpit in the middle; the low one across the Pools past the Cheese Store to the Well; and the middle one along the south walk and through the Rind Tunnel. The Pulpit, the bridges through the Spine and the landing join the north and south in the middle, and the Pulpit's and the landing's stairs join every level there. A hollow is about 7 s from the Spine at a sprint by any lane.

Free-for-all has fourteen more starts (tagged "Free"), seven a side through every level: on the floor (three), the Pulpit, the gallery, the south walk and the landing.

The rock stops the lines that matter whatever the mice eat (`CheeseStrike.Play.TheGrottoHoldsTogether`): no start sees an enemy start; the Spine stops the floor's middle line and the line between the hollows' openings; the Curtain stops the line along the galleries; the buttress and the fin stop the one along the south walk.

## The cheese, and what the mice change

There are 356 blocks the mice can eat and four they can't (the bridges' waxed beams); one mouse to 25 blocks brings fourteen. They come out of ten holes in the rock: four at the floor's ends under the hollows and two in the Pulpit's face, onto the floor, and four in the south wall over the walk. None open onto the galleries: their decks come down as the mice on the floor eat their pillars, and the mice climb to what is left up there last. Keeping to the mice's pace, fourteen mice leave the cave bare of cheese about a minute and a quarter after they come: the floor's cover and the stores first, then the decks, side by side as the mice spread, and the tunnel and the bridges last (`tools/play/grotto_mice.txt`).

| Structure | Blocks | Before | Eaten |
|---|---|---|---|
| **GalleryDeckGouda**, **GalleryDeckMozza** | 24 each | Eight slabs 4 m square on 8 m pillars, with a low edge along the deck's front: the gallery is about 12 m wide, high ground over the Colonnade and the Pools, with cover to crouch behind. | Each slab falls whole onto the floor when its pillar is eaten, with whoever is on it; then it is cheap cheese on the floor and goes. The gallery is its rock walk, 4 m wide, with nothing to hide behind. |
| **LedgeDeckGouda**, **LedgeDeckMozza** | 36 each | Twelve slabs on 4 m pillars with a low edge: the Ledge is about 13 m wide, over the Undercroft. | The slabs fall onto the floor of the Undercroft; the Ledge is its 5 m walk of rock, zig-zagging between the buttress and the fin. |
| **RindTunnel** | 68 | A dark, close way along the walk's middle: the middle lane is fought at short range, hidden from the floor and the galleries. | The walk is open; the lane is seen from the floor and the gallery. |
| **NorthSpan**, **SouthSpan** | 4 each, and 2 waxed beams | Bridges through the Spine's arch: the north and south of the middle level joined over the Well. | Two red beams each, a balance walk for a careful player, too narrow for the bots. The middle level splits at the Spine; the floor and the stairs still join it. |
| **CheeseStoreGouda**, **CheeseStoreMozza** | 32 each | A house on the floor's way to the middle: close fighting through its doors and windows. | Its steel corners and door frames: the floor is open from the hollow's stairs to the Well. |
| **FloorGouda**, **FloorMozza** | 31 each | Stacks of wheels and blocks across the floor, crouching and full cover. | The floor's cover is its rock: boulders, mine carts, the stairs, the Spine. |
| **HollowGouda**, **HollowMozza** | 6 each | A wall of cheese in the hollow, in front of its opening. | The hollow can be seen into from the stairs below its opening. |
| **PulpitEdge**, **WalksGouda**, **WalksMozza** | 6 each | Low cover on the Pulpit's edge, the gallery walks and the south walks. | Bare rock. |

The decks are `kit.pillar_deck`: a slab on one pillar under its middle, never two (a slab on two would fall when either goes, and come down through the one left), the level below kept clear under the slabs but for their pillars, so that a slab always lands flat on the floor. A slab landing round someone standing under it leaves them standing on it, and someone on a falling slab rides it down and walks off it (`CheeseStrike.Play.AGrottoDeckFallsOntoTheLevelBelow`). Riding a slab down is no fall, and a drop from the middle level (4 m) is safe; only a drop from a gallery (8 m) to the floor hurts, by about a third of the health (`CheeseStrikeFalls`), so a collapse never kills; it moves the fight.

What never goes is rock: the floor, the walks, the shelf, the Pulpit, the landing, the hollows, every flight of stairs and the Spine, and the bridges' waxed beams. Every start stands on rock and every level stays joined to every other by stairs of rock (`CheeseStrike.Play.AfterTheMice.GruyèreGrotto`).

## King of the Hill

King of the Hill's hill goes round four spots (`hills()`, `ACheeseStrikeHillSpot`), a minute at each: **the Pulpit** (on the middle level), **Gouda's pool** on the floor, **the Landing** (the middle level, by the tunnel's door) and **Mozza's pool**. The Pulpit and the Landing are on the middle line; the pools are twins, a side's and its mirror; all are rock, the pools' beds under water that is walked through.

## Tests and tooling

`CheeseStrike.Play.EveryMapHasFairHills` checks its hill spots: three to five, each on a floor the mice never eat, clear and level round its middle, on the navigation mesh and reached from both sides' starts, well away from them, the first as far from both and the round favouring neither. - `CheeseStrike.Play.TheGrottoHoldsTogether`: its starts (eight a side in its hollow, free-for-all ones on all three levels), its size, every structure whole, every slab on its pillar and held, the decks and the tunnel and the edges hiding what they should, the lines the rock stops whatever is eaten, every mouse hole landing on the navigation mesh (on the floor and the middle level, not the galleries), and a walk up or down every flight of stairs, over both bridges through the arch and through the tunnel's door.
- `CheeseStrike.Play.AGrottoDeckFallsOntoTheLevelBelow`: a Ledge slab's pillar eaten with a player under it, and a gallery slab's with a player on it: each slab falls at once, whole and straight down onto the floor with its edge on it; the player under it is not left inside anything, the one on it comes down standing on it, and both walk away.
- `CheeseStrike.Play.BotsPlayTheGrotto`: every level and place on it can be walked to from a hollow, and fifteen bots spread over all three levels, find each other and kill.
- `CheeseStrike.Play.AfterTheMice.GruyèreGrotto`: all the cheese eaten at once, every start on rock and joined to every other.
- `tools/play/grotto_tour.txt` takes pictures from set places, and `tools/play/grotto_mice.txt` pictures from over each gallery every minute while the mice eat it, with bots fighting (`tools/play.ps1 -Map /Game/CheeseStrike/Maps/Lvl_Grotto -StartAt 4200,-1900,500,180`, with `-Bots -Mice -Extra "?MiceAt=5?KillLimit=200" -TimeoutSeconds 600` for the mice; from Git Bash, set `MSYS_NO_PATHCONV=1`). Building it with `GROTTO_OPEN=1` in the environment leaves out the roof, for looking at it from above.
- Its lobby picture is `python tools/art/map_picture.py /Game/CheeseStrike/Maps/Lvl_Grotto grotto.png 3300 -300 1100 -14 175 --start 4200,-1900,500,180`.

Changing the map is a change to `tools/art/build_grotto.py` (after `import_fab.py`, `build_materials.py`, `cheese_material.py`, `wax_material.py` and `grotto_materials.py`), then running it (it rebuilds the level from nothing) and the tests.
