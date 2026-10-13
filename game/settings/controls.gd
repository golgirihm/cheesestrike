class_name Controls
## The keys players can choose for themselves, and putting their choices into
## effect. Only keyboard keys are chosen; the mouse, gamepads and touch keep
## their own controls.

## Actions that can be given another key, in the order the settings list them,
## with the name shown for each.
const REBINDABLE := {
	"move_forward": "Forward",
	"move_back": "Back",
	"move_left": "Left",
	"move_right": "Right",
	"jump": "Jump",
	"sprint": "Run",
	"crouch": "Crouch",
	"lean_left": "Lean left",
	"lean_right": "Lean right",
	"reload": "Reload",
	"alt_fire": "Other fire",
	"melee": "Knife",
	"grenade": "Grenade",
	"emote": "Emote",
	"overload": "Overload",
	"scoreboard": "Scoreboard",
	"talk": "Talk",
}

# Each rebindable action's own keys, as the project sets them, by action.
static var _defaults := {}


## Puts `bindings` (action -> physical key code) into effect: each named action
## gets that key in place of its usual keyboard keys, and every other action
## its usual ones. A key given to one action is taken from any other that had
## it by default, so one key never does two things.
static func apply(bindings: Dictionary) -> void:
	_remember_defaults()
	var taken := {}
	for action: String in bindings:
		if REBINDABLE.has(action) and bindings[action] is int:
			taken[bindings[action]] = action
	for action: String in REBINDABLE:
		for event in InputMap.action_get_events(action):
			if event is InputEventKey:
				InputMap.action_erase_event(action, event)
		var keys: Array = [bindings[action]] if bindings.get(action) is int else _defaults[action]
		for code: int in keys:
			if taken.get(code, action) != action:
				continue
			var key := InputEventKey.new()
			key.physical_keycode = code
			InputMap.action_add_event(action, key)


## The physical codes of the keys that do `action` now.
static func keys_for(action: String) -> Array:
	var codes := []
	for event in InputMap.action_get_events(action):
		if event is InputEventKey:
			codes.append(event.physical_keycode)
	return codes


## The name of the key with physical code `code`, as the keyboard labels it.
static func key_name(code: int) -> String:
	# The physical code names the key by its place on a US keyboard; where the
	# keyboard can say what this one's key is labelled, that's better.
	var shown := code
	if code != 0 and DisplayServer.get_name() != "headless":
		var labelled := DisplayServer.keyboard_get_label_from_physical(code)
		if labelled != KEY_NONE:
			shown = labelled
	var label := OS.get_keycode_string(shown)
	return label if not label.is_empty() else "?"


## How an action's keys read on its button, such as "W / Up".
static func keys_text(action: String) -> String:
	var names: Array[String] = []
	for code: int in keys_for(action):
		names.append(key_name(code))
	return " / ".join(names) if not names.is_empty() else "(none)"


static func _remember_defaults() -> void:
	if not _defaults.is_empty():
		return
	for action: String in REBINDABLE:
		_defaults[action] = keys_for(action)
