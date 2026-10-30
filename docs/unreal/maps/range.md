# The Practice Range

A walled yard about 74 by 36 m for trying the weapons alone, built by `tools/art/build_range.py` into `Content/CheeseStrike/Maps/Lvl_Range`, from the blocks in `tools/art/map_kit.py` ([principles.md](principles.md) says how maps are made). The front end's PRACTICE plays it (`ACheeseStrikePlayerController::PracticeFromMenu`, `CheeseStrikeRange::PracticeUrl`): alone and offline, at once with no lobby and no count, a free-for-all to 1000 kills (so it never ends by itself), no mice, and four dummies (`?Mode=FreeForAll?Bots=4?BotSkill=dummy?Mice=0?StartNow?Countdown=0?KillLimit=1000?RespawnDelay=2`); Escape's Leave the Match goes back to the front end. It is also in the lobby's maps (`CheeseStrikeRules::Maps`), a small open map with bots that play as anywhere else.

## Layout

x runs from the yard (west) to the far end of the range (east), y south to north; the floor is at 0.

| Place | Where | What it is |
|---|---|---|
| Yard | x -2200 to -650 | the starts: eight Gouda and eight Mozza in two files either side of the middle line (y -100 and 100), from x -900 back to -1950, all facing the range |
| Armoury | x -400 | a row of eight weapon stands 3 m apart, one for every weapon that lies on a stand (`CheeseStrikePickups::CanLieOnAStand`: all the primaries but the Colby Carbine, which everyone comes in with): the Grater, Blaster, Roquefort, BabyBomb, Stick, Munster, Burratas and Shotgun, south to north, the power weapons in the middle. Each is instant (`kit.pickup(instant=True)`, `ACheeseStrikePickup::IsInstant`): taken, its weapon is back at once, so the row is never empty |
| Firing Line | x 0 | a yellow line across the range, a yellow post at each end and a FIRING LINE sign |
| Range | x 0 to 5200 | white lines across it at 5, 10, 20 and 40 m with a sign at each end saying how far (`kit.text_sign`), the four dummy spots (`kit.dummy_spot`: 5 m at y -650, 10 m at 250, 20 m at -500, 40 m at 400, facing the line) and a dark backstop 9 m high at the far end |
| Target Wall | x 1500, y 840 to 1540 | a steel frame with a dark board behind twelve cheese targets (`kit.target`, `ACheeseStrikeTarget`) in three rows of four, 60 cm wheels on their edges facing the line. A shot takes the engine's point damage off a target's 40 health (a Carbine burst breaks one), its holes growing as it is hit; broken, it is gone and grows back three seconds later (`CheeseStrikeRange::RegrowSeconds`) |

The dummies are bots with `?BotSkill=dummy` (`CheeseStrikeBots::DummiesFrom`, `ACheeseStrikeBotController::IsDummy`): each comes in at a spot of its own (`ACheeseStrikeGameMode::ChooseDummySpot`, `CheeseStrikeRange::DummySpotFor`), never moves, aims or fires, and comes back at the same spot two seconds after it is killed. No player ever comes in at a dummy spot (it is a tagged point, not a start).

There is no cheese for the mice and no mouse entry: no mice come. The stands are fair to both sides and the power weapons are the most contested (the files of starts either side of the middle line are equally far from every stand), and three hill spots stand on the middle line in front of the firing line (Near at 7 m, Field at 28 m, Far at 44 m), so that the map passes the checks every map in the lobby has (`CheeseStrike.Play.AfterTheMice`, whose mice's checks find nothing to eat; `EveryMapHasFairHills`). Lit by day, the sun behind the line.

`CheeseStrike.Play.ThePracticeRangeNeverEmpties` plays it as PRACTICE does: every stand taken in turn and never empty, the dummies standing still where they came in and coming back there, a target breaking and growing back. `tools/play/range.txt` looks at it and `tools/play/practice.txt` plays PRACTICE from the front end; its lobby picture is `Content/UI/Maps/range.png` (`tools/art/map_picture.py /Game/CheeseStrike/Maps/Lvl_Range range.png -2100 -1700 900 -20 25 --start -2100,1700,100,0`).
