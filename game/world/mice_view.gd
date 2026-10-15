class_name MiceView
extends Node3D
## The giant mice as everyone sees them (see MouseRules): each drawn where the
## host last said it was, gliding there between updates, bobbing as it walks,
## nibbling as it eats and scurrying when it runs, and heard gnawing and
## squeaking.
##
## None of it is solid: a mouse is only seen and heard. The world adds this
## as a child of itself.

## How much bigger than a small mouse (see Scenery.mouse) a giant one is.
const SCALE := MouseRules.LENGTH / 0.96
## How quickly one closes on where it was last said to be: the share of the
## gap left after a second is e to the minus this.
const FOLLOW := 8.0
## Seconds between gnawing sounds from one eating mouse.
const GNAW_EVERY := 0.9

var _world: Node3D
var _shown: Array[Node3D] = []
var _time := 0.0


func _init(world: Node3D) -> void:
	_world = world
	name = "Mice"


func _ready() -> void:
	_world.match_.mouse_scared.connect(_on_scared)


## How many mice are shown, for tests.
func shown() -> int:
	return _shown.size()


func _process(delta: float) -> void:
	_time += delta
	var match_: Match = _world.match_
	var mice: Array = match_.state.mice.mice if match_.has_state() else []
	while _shown.size() > mice.size():
		_shown.pop_back().queue_free()
	while _shown.size() < mice.size():
		_shown.append(_make(mice[_shown.size()]))
	for index in mice.size():
		var mouse: Dictionary = mice[index]
		var shown: Node3D = _shown[index]
		var follow := 1.0 - exp(-FOLLOW * delta)
		shown.position = shown.position.lerp(mouse.at, follow)
		shown.rotation.y = lerp_angle(shown.rotation.y, mouse.heading, follow)
		_animate(shown, int(mouse.mode), index, delta)


# A new giant mouse, where `mouse` is.
func _make(mouse: Dictionary) -> Node3D:
	var shown := Node3D.new()
	var body := Scenery.mouse()
	body.name = "Body"
	body.scale = Vector3.ONE * SCALE
	shown.add_child(body)
	shown.set_meta("gnaw_in", 0.0)
	shown.set_meta("mode", -1)
	add_child(shown)
	shown.position = mouse.at
	shown.rotation.y = mouse.heading
	return shown


# Bobs as it walks, quicker as it runs, and nibbles as it eats, with the sound
# of it.
func _animate(shown: Node3D, mode: int, index: int, delta: float) -> void:
	var body: Node3D = shown.get_node("Body")
	var phase := _time + index * 0.7
	match mode:
		MouseRules.EATING:
			var nibble := absf(sin(phase * TAU / 0.36))
			body.scale = Vector3(1.0, 1.0 - 0.15 * nibble, 1.0 + 0.1 * nibble) * SCALE
			body.position.y = 0.0
			var gnaw_in: float = shown.get_meta("gnaw_in") - delta
			if gnaw_in <= 0.0:
				gnaw_in = GNAW_EVERY
				Sounds.play_at("gnaw", shown.global_position + Vector3.UP * MouseRules.HEIGHT / 2.0, _world)
			shown.set_meta("gnaw_in", gnaw_in)
		_:
			var speed := 16.0 if mode == MouseRules.FLEEING else 8.0
			body.scale = Vector3.ONE * SCALE
			body.position.y = absf(sin(phase * speed)) * (0.8 if mode == MouseRules.FLEEING else 0.4)
			shown.set_meta("gnaw_in", 0.0)
	shown.set_meta("mode", mode)


func _on_scared(index: int) -> void:
	if index >= 0 and index < _shown.size():
		Sounds.play_at("squeak", _shown[index].global_position + Vector3.UP * MouseRules.HEIGHT, _world)
