class_name Sensitivity
## How far the view turns for a move of the mouse or the stick: at the eye,
## through a scope, and up and down, each as the player sets it, and how to
## match the feel of another game.

## How far the view turns, in radians, for a mouse moved one count at the
## usual sensitivity. There is never any acceleration: the same move always
## turns the view as far, however fast it is made.
const MOUSE_RADIANS := 0.0025
const AIMING := "aim_sensitivity"
const SCOPED := "scope_sensitivity"
const UP_AND_DOWN := "look_vertical"
const OTHER_GAME := "match_game"
## Games whose sensitivity can be matched: what each is called and how far,
## in degrees, it turns for a count at a sensitivity of 1.
const OTHER_GAMES := {
	"source": ["Counter-Strike", 0.022],
	"valorant": ["Valorant", 0.07],
	"overwatch": ["Overwatch", 0.0066],
	"apex": ["Apex Legends", 0.022],
	"quake": ["Quake", 0.022],
}


## The mouse sensitivity here that turns the view as far for a count as
## `theirs` does in `game` (see OTHER_GAMES), within what can be set; or -1
## if `game` isn't one of them or `theirs` makes no sense.
static func matching(game: String, theirs: float) -> float:
	if not OTHER_GAMES.has(game) or not is_finite(theirs) or theirs <= 0.0:
		return -1.0
	var range: Array = Settings.RANGES["mouse_sensitivity"]
	return clampf(deg_to_rad(theirs * OTHER_GAMES[game][1]) / MOUSE_RADIANS, range[0], range[1])


## How much of the usual speed the view turns at, `aim` of the way to the
## eye (0 to 1) with a weapon zooming `zoom`, through a scope if `scoped`;
## with the player's own share for aiming and for a scope.
static func aim_factor(zoom: float, aim: float, scoped: bool, aiming: float, scope: float, slower := 0.6) -> float:
	return lerpf(1.0, minf(slower, zoom * 1.2) * (scope if scoped else aiming), aim)
