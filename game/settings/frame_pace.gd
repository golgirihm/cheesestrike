class_name FramePace
## How many frames a second the game may draw: as many as the screen shows,
## or no more than a cap the player chooses; and, to make a phone's battery
## last, no more than BATTERY a second whatever the cap.

const CAP := "frame_cap"
const BATTERY_SAVER := "battery_saver"
const CAPS := ["", "30", "60", "90", "120", "144"]
const CAP_NAMES := {"": "As the screen allows", "30": "30 a second", "60": "60 a second", "90": "90 a second", "120": "120 a second", "144": "144 a second"}
const BATTERY := 30


## The most frames a second for a cap of `cap` (see CAPS) with the battery
## saver on or off: 0 for no limit.
static func most(cap: String, battery_saver: bool) -> int:
	var capped := int(cap) if cap.is_valid_int() else 0
	if battery_saver:
		return mini(capped, BATTERY) if capped > 0 else BATTERY
	return capped


## Puts the chosen cap into effect.
static func apply() -> void:
	Engine.max_fps = most(Settings.get_value(CAP), Settings.get_value(BATTERY_SAVER))
