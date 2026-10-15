class_name ParticleBudget
## How many specks of dust, sparks, smoke, crumbs and fire may be in the air
## at once, by graphics quality. Each speck is a little mesh drawn on its
## own, so a weaker graphics card can't have as many as a strong one, and a
## busy fight mustn't be what slows the game down.
##
## Effects are of two sorts. Lesser ones dress up what is already plain to
## see, such as the dust a shot knocks off a wall: low graphics has none,
## and elsewhere they stop short of the limit. Ones that matter show
## something happening, such as a burst or an Overload: every quality has
## them, and the room the lesser ones leave is kept for them.

## The most specks showing at once.
const LIMITS := {GraphicsQuality.LOW: 60, GraphicsQuality.MEDIUM: 260, GraphicsQuality.HIGH: 560}
## The share of the limit the lesser effects may fill.
const LESSER_SHARE := 0.7
## The share of what an effect asks for that each quality gives it.
const DENSITY := {GraphicsQuality.LOW: 0.4, GraphicsQuality.MEDIUM: 0.7, GraphicsQuality.HIGH: 1.0}


static func limit(level: String) -> int:
	return LIMITS.get(level, LIMITS[GraphicsQuality.HIGH])


## How many specks an effect that wants `wanted` gets, with `live` already
## showing: fewer on lower graphics, and never more than there is room for.
static func grant(wanted: int, live: int, level: String, matters := false) -> int:
	if wanted <= 0 or (not matters and level == GraphicsQuality.LOW):
		return 0
	var most := limit(level) if matters else int(limit(level) * LESSER_SHARE)
	var thinned := maxi(roundi(wanted * DENSITY.get(level, 1.0)), 1)
	return clampi(thinned, 0, maxi(most - live, 0))
