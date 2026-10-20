# The Creamery

The first real map: a cheese works, about 130 by 100 m at life size (390 by 300 Godot metres, between Curd City and the small arenas), for up to sixteen players. A sprint from one dock to the other takes about 20 seconds. It is built by `tools/art/build_creamery.py` into `Content/CheeseStrike/Maps/Lvl_Creamery`, from the engine's basic shapes and the project's own materials (`tools/art/build_materials.py`, `tools/art/cheese_material.py`): no downloaded art yet, so it is a well-lit block-out of a real place, not finished art.

## Layout

West is The Gouda Guys' side, east the Mozza Radicals'; the map is mirrored across the middle (x to -x), so neither side has a better half. From west to east: a **dock**, a **plaza**, then three lanes side by side, then the other plaza and dock.

| Place | Where | What it is |
|---|---|---|
| The Docks | each end | a loading platform 1.2 m up along the outer wall, with two ramps down, roller doors behind; eight starts each, four up, four down, facing the middle |
| The Plazas | between the docks and the lanes | open ground with pallets of cheese wheels and a forklift; the **Annex** on each, a two-room office with a door at either end, a window to the yard, and a ramp along its south side to its roof, the high ground over the plaza |
| The Loading Yard | the north lane | shipping containers alone and stacked, a trailer, barriers, lamps; cut in two at the middle by the **Wheel Stack** |
| The Squeeze | the walkway between the yard and the hall | two overlapping stacks of crates by the hall's north wall: the only way past the Wheel Stack, zig-zagging, with no line along it |
| The Aging Hall | the middle lane | a hall 48 by 36 m, 9 m high, skylights in its roof; **racks** of cheese wheels in rows either side of a middle aisle, the **press** in the middle, a **mezzanine** 4 m up along the south wall with ramps at either end; main doors at each end (8 m), two doors each in the north and south walls (4 m) |
| The Whey Canal | the south lane | a channel 8 m wide and 3 m deep, ramps down at each end, steps out along its north wall, two concrete bridges and the **Cheese Bridge** in the middle |
| Tank Row | south of the canal | steel silos, barriers, and a pipe rack along its length; pipe bridges over the canal to the hall |

Beyond the outer wall, for a skyline and nothing else: two cold stores, a chimney, a battery of tall silos, offices.

## The cheese, and what the mice change

Every block of cheese is an `ACheeseStrikeCheese` (`Source/CheeseStrike/Map/`) with the design's 400 of eating in it and the name of the structure it belongs to. There are 692 blocks; the design's one mouse to sixty blocks puts the full eight mice on it. What each structure does before the mice, and what eating it does:

| Structure | Blocks | Before | Eaten |
|---|---|---|---|
| **WheelStack** | 100 | A wall of wheels four high across the yard. The north lane has no long line; it is fought close, through the Squeeze. | The yard opens from dock to dock: a 130 m line for whoever holds a dock, and a second wide way past the hall. The lane turns from close fighting to long. |
| **HallSeal** | 24 | The hall's two north doors are stopped. The hall is entered from the ends and the south. | The hall opens onto the yard: two more ways in, and the yard and the hall become one fight. |
| **Racks** | 360 | Shelves of wheels, a wall each, making aisles across the hall; the hall is close and full of corners. | Bare steel frames you can see and shoot through: the hall becomes one open floor, watched from the mezzanine. |
| **CheeseBridge** | 32 | The middle crossing over the canal. | Gone: the canal is crossed only at the two concrete bridges, or by going down into it, and the south lane splits in two. |
| **DockWallGouda**, **DockWallMozza** | 48 each | Low walls (2 m) in front of each dock, covering the ways out. | A side's way out is open to the plaza: holding the plaza starts to mean pinning a side in. |
| **PalletsGouda**, **PalletsMozza** | 40 each | Wheels on pallets, crouching cover across the plazas. | The plazas are bare ground to be crossed fast. |

So the map starts closed (three separate lanes, short lines, cover everywhere) and opens as a match goes on: the lanes join through the hall's north doors and the yard, the long lines appear, the hall loses its cover, and the south loses its middle crossing. Nothing the mice can eat holds up a floor anyone stands on except the Cheese Bridge's deck.

Thin cheese (the floor stripes on the shooter level) has no holes; here every block is thick enough for them.

## Tests and tooling

`CheeseStrike.Play.TheCreameryHoldsTogether` plays the map: starts for sixteen, every structure there with its blocks whole, its size, the lines above (the Wheel Stack closes the yard, the Squeeze breaks the walkway, the Seals close the doors, the press stops the hall's door-to-door line, the racks are full and the aisles clear), and a walk up or across every ramp, the steps and the Cheese Bridge. `CheeseStrike.Play.TheHostPicksTheMap` picks it in the lobby and checks everyone arrives. `tools/play/creamery_tour.txt` takes pictures of it from set places (`tools/play.ps1 -Map /Game/CheeseStrike/Maps/Lvl_Creamery`; from Git Bash, set `MSYS_NO_PATHCONV=1` so the path isn't rewritten).

Changing the map is a change to `tools/art/build_creamery.py`, then running it (it rebuilds the level from nothing) and the tests.
