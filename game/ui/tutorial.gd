class_name Tutorial
extends RefCounted
## A short guided first session, on the practice range: one thing to do at a
## time, each said in terms of the device in the player's hands, and ticked
## off when they have done it.
##
## Kept free of nodes: it is told how things stand with the player and says
## what to show.

## The steps in order, by name.
const STEPS := ["move", "look", "shoot", "aim", "reload", "pick_up", "overload"]
## How far a player must walk, and turn (in radians), to have shown they can.
const WALK := 4.0
const TURN := 0.2
## What each step asks, for each device (see ControlsHelp).
const ASKS := {
	"move": {"keyboard": "Walk about with %s", "gamepad": "Walk about with the left stick", "touch": "Drag on the %s side of the screen to walk"},
	"look": {"keyboard": "Click the game, then move the mouse to look round", "gamepad": "Look round with the right stick", "touch": "Drag on the other side of the screen to look round"},
	"shoot": {"keyboard": "Fire with the left mouse button", "gamepad": "Fire with the right trigger", "touch": "Press FIRE"},
	"aim": {"keyboard": "Hold the right mouse button to aim down the sights", "gamepad": "Hold the left trigger to aim down the sights", "touch": "Press AIM to aim down the sights"},
	"reload": {"keyboard": "Reload with %s", "gamepad": "Reload with the left face button", "touch": "Press R to reload"},
	"pick_up": {"keyboard": "Walk over a weapon on the rack ahead to take it", "gamepad": "Walk over a weapon on the rack ahead to take it", "touch": "Walk over a weapon on the rack ahead to take it"},
	"overload": {"keyboard": "Your Overload is full: set it off with %s", "gamepad": "Your Overload is full: set it off with the top face button", "touch": "Your Overload is full: press GO"},
}
const DONE := "That's everything. Shoot the targets for as long as you like, or leave from the menu and find a match."

## Which step the player is on: STEPS.size() once they have done them all.
var step := 0
# How things stood when the step began, and how far they have walked and
# turned in it.
var _from := {}
# And how they stood when the tutorial began.
var _began := {}
var _walked := 0.0
var _turned := 0.0
var _last := {}


func is_done() -> bool:
	return step >= STEPS.size()


## The step the player is on, or "" once they have done them all.
func step_name() -> String:
	return STEPS[step] if not is_done() else ""


## Told how things stand with the player: {"at": their feet, "yaw": which
## way they face, "shots": how many they have fired, "aiming", "reloading",
## "reloads": how many they have finished, "weapons": how many they carry,
## "overloaded": whether they are in an Overload}. Returns whether that
## finishes the step they were on.
func update(now: Dictionary) -> bool:
	if is_done():
		return false
	if _from.is_empty():
		_from = now
		_began = now
		_last = now
		return false
	_walked += Vector2(now.at.x - _last.at.x, now.at.z - _last.at.z).length()
	_turned += absf(angle_difference(_last.yaw, now.yaw))
	_last = now
	var done := false
	match STEPS[step]:
		"move":
			done = _walked >= WALK
		"look":
			done = _turned >= TURN
		"shoot":
			done = now.shots > _from.shots
		"aim":
			done = now.aiming
		"reload":
			# Begun is enough: a player standing on the rack has their
			# weapon filled again before a reload can finish.
			done = now.reloads > _from.reloads or now.get("reloading", false)
		"pick_up":
			# One taken from the rack earlier, in passing, will do: there may
			# be no other within easy reach.
			done = now.weapons > _began.weapons
		"overload":
			done = now.overloaded
	if done:
		step += 1
		_from = now
		_walked = 0.0
		_turned = 0.0
	return done


## What to show the player: which step of how many, and what it asks, for
## someone playing on `device`. `keys` names the keys the player has chosen
## for moving, reloading and the Overload; `left_handed` is whether a touch
## screen's buttons are on the left.
func text(device: String, keys := {}, left_handed := false) -> String:
	if is_done():
		return DONE
	var name: String = STEPS[step]
	var asks: Dictionary = ASKS[name]
	var ask: String = asks.get(device, asks["keyboard"])
	if ask.contains("%s"):
		var given: String = keys.get(name, "")
		if device == "touch":
			given = "right" if left_handed else "left"
		ask = ask % given
	return "%d of %d   ·   %s" % [step + 1, STEPS.size(), ask]
