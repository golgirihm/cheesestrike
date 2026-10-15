class_name SoundRing
extends Control
## A ring round the crosshair with a mark toward each sound lately heard,
## for players who can't hear well, or at all, where a sound comes from.
## Turned on in the settings.

const SETTING := "sound_ring"
## How long a mark shows, in seconds, the most shown at once, and the ring's
## size in pixels.
const LIFE := 1.5
const MOST := 8
const RADIUS := 70.0
## How wide a mark is, in radians.
const WIDTH := 0.35
## The sounds that get a mark, and how strongly each is shown, from 0 to 1.
const LOUDNESS := {"pistol": 0.8, "pistols": 0.8, "gnaw": 0.7, "squeak": 0.8,"crumble": 0.6, "blaster": 0.9, "shotgun": 1.0, "grater": 0.8, "launcher": 1.0,
	"carbine": 0.85, "rifle": 0.9, "lmg": 0.9, "bow": 0.6, "burst": 1.0, "knife": 0.5, "throw": 0.5, "slide": 0.5, "pad": 0.6, "step": 0.45,
	"step_rock": 0.45, "step_grass": 0.4, "step_wood": 0.45, "step_metal": 0.5, "step_cheese": 0.4, "voice_pain": 0.6, "voice_out": 0.6,
	"voice_taunt": 0.5, "voice_reload": 0.5, "pickup": 0.4}

var world: Node3D
# Each {"at": where, "strength", "left": seconds}.
var _marks: Array[Dictionary] = []


func _init() -> void:
	name = "SoundRing"
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)


## The sound `name` was heard from `at`.
func heard(name: String, at: Vector3) -> void:
	if not Settings.get_value(SETTING) or not LOUDNESS.has(name):
		return
	_marks.append({"at": at, "strength": LOUDNESS[name], "left": LIFE})
	while _marks.size() > MOST:
		_marks.pop_front()
	queue_redraw()


## The marks showing, for a player at `listener` facing `yaw`: each
## {"angle": from straight ahead, as damage_direction gives it, clockwise;
## "alpha": how strongly it shows}.
func marks(listener: Vector3, yaw: float) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	for mark in _marks:
		shown.append({"angle": CombatHud.damage_direction(listener, yaw, mark.at), "alpha": mark.strength * clampf(mark.left / LIFE * 1.5, 0.0, 1.0)})
	return shown


func clear() -> void:
	_marks.clear()
	queue_redraw()


func _process(delta: float) -> void:
	if _marks.is_empty():
		return
	for mark in _marks:
		mark.left -= delta
	_marks = _marks.filter(func(mark: Dictionary) -> bool: return mark.left > 0.0)
	queue_redraw()


func _draw() -> void:
	if world == null or multiplayer.multiplayer_peer == null or _marks.is_empty():
		return
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own == null:
		return
	var middle := size / 2.0
	for mark in marks(own.global_position, own.rotation.y):
		# Straight ahead is up the screen.
		var towards: float = mark.angle - PI / 2.0
		draw_arc(middle, RADIUS, towards - WIDTH / 2.0, towards + WIDTH / 2.0, 12, Color(1, 1, 1, mark.alpha), 4.0, true)
