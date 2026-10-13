class_name UiSounds
## The interface makes sound: a tick as the pointer comes onto a button, a
## click as it is pressed, a lower one for a button that goes back, a buzz
## when something goes wrong, and a count for the last seconds of a wait.

const HOVER := "ui_hover"
const PRESS := "ui_press"
const BACK := "ui_back"
const ERROR := "ui_error"
const COUNT := "ui_count"
const CUES := [HOVER, PRESS, BACK, ERROR, COUNT]
## A button whose name has one of these in it goes back rather than on.
const BACK_WORDS := ["Back", "Done", "Close", "Leave", "Resume", "Cancel", "Dismiss", "Disconnected"]
## The last seconds of a wait that are counted aloud.
const COUNTED := 3


## The sound pressing `button` makes.
static func press_sound(button: Node) -> String:
	var name := String(button.name)
	for word: String in BACK_WORDS:
		if name.contains(word):
			return BACK
	return PRESS


## Gives `button` its sounds, once.
static func wire(button: BaseButton) -> void:
	if button.has_meta("ui_sounds"):
		return
	button.set_meta("ui_sounds", true)
	button.mouse_entered.connect(func() -> void:
		if not button.disabled and button.is_visible_in_tree():
			Sounds.play(HOVER, -14.0))
	button.pressed.connect(func() -> void: Sounds.play(press_sound(button), -8.0))


## Whether `button` has its sounds.
static func wired(button: BaseButton) -> bool:
	return button.has_meta("ui_sounds")


## Whether a wait that had `before` seconds left and now has `now` has just
## come to one of its last whole seconds, to be counted: 3, 2, 1.
static func counts(before: float, now: float) -> bool:
	if now <= 0.0 or now >= before:
		return false
	return ceili(now) <= COUNTED and ceili(now) < ceili(before)
