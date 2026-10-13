class_name HurtView
extends CanvasLayer
## What being hurt looks like to the player it happens to: red at the edges
## of the view that deepens as health runs low and beats when it is very low,
## a flush of it at each hit, and the view blurring for a moment when a burst
## goes off close by. It is drawn over the arena and under the interface.
##
## The blur follows the "Screen shake" setting, as it is the same kind of
## jolt, and is gone when that is at nothing.

## The share of health below which the edges redden, and below which they
## beat.
const HURT_BELOW := 0.5
const BEAT_BELOW := 0.3
## How red the edges are with no health left, before any beat or hit.
const MOST_RED := 0.6
## How red a hit can make them on its own, the damage that takes, and the
## seconds it takes to fade.
const HIT_RED := 0.45
const HIT_FOR_ALL := 60.0
const HIT_FADE := 0.6
## How far from a burst, in its radii, the view is blurred, how strongly at
## most, and the seconds it takes to clear.
const BLUR_REACH := 2.0
const BLUR_FADE := 0.9

const _RED_SHADER := """
shader_type canvas_item;
uniform float red = 0.0;
void fragment() {
	float out_from_middle = distance(UV, vec2(0.5)) / 0.7071;
	COLOR = vec4(0.7, 0.02, 0.0, red * smoothstep(0.25, 0.95, out_from_middle));
}
"""
const _BLUR_SHADER := """
shader_type canvas_item;
uniform sampler2D screen : hint_screen_texture, filter_linear;
uniform float amount = 0.0;
void fragment() {
	// The view smeared outward from its middle, more toward the edges.
	vec2 out_way = SCREEN_UV - vec2(0.5);
	vec3 sum = vec3(0.0);
	for (int tap = 0; tap < 8; tap++) {
		sum += texture(screen, SCREEN_UV - out_way * amount * 0.016 * float(tap)).rgb;
	}
	COLOR = vec4(sum / 8.0, 1.0);
}
"""

var world: Node3D
# What a hit has added to the red, and how blurred the view is: each fades.
var _hit := 0.0
var _blur := 0.0
var _time := 0.0
var _red: ColorRect
var _blurred: ColorRect


## How red the edges are with `health` of `full` left: not at all with half
## or more, deepening steadily from there down.
static func redness(health: float, full: float) -> float:
	if full <= 0.0 or health <= 0.0:
		# Knocked out: that has its own screen.
		return 0.0
	var share := clampf(health / full, 0.0, 1.0)
	return MOST_RED * clampf((HURT_BELOW - share) / HURT_BELOW, 0.0, 1.0)


## What the red is multiplied by `time` seconds in, with `health` of `full`
## left: steady until health is very low, then beating like a heart, faster
## the lower it is. Never above 1, so a beat is the red easing off.
static func beat(health: float, full: float, time: float) -> float:
	var share := clampf(health / maxf(full, 1.0), 0.0, 1.0)
	if share >= BEAT_BELOW or health <= 0.0:
		return 1.0
	var rate := lerpf(2.2, 1.2, share / BEAT_BELOW)
	return 0.7 + 0.3 * absf(sin(time * rate * PI))


## How much red a hit that dealt `dealt` adds at once.
static func hit_red(dealt: float) -> float:
	return HIT_RED * clampf(dealt / HIT_FOR_ALL, 0.25, 1.0) if dealt > 0.0 else 0.0


## How blurred the view is (0 to 1) by a burst of `radius` metres going off
## `distance` metres away, with the "Screen shake" setting at `shake`.
static func blur_from(distance: float, radius: float, shake := 1.0) -> float:
	if radius <= 0.0:
		return 0.0
	return clampf(1.0 - distance / (radius * BLUR_REACH), 0.0, 1.0) * clampf(shake, 0.0, 1.0)


func _init(of: Node3D = null) -> void:
	world = of
	name = "HurtView"
	layer = -1
	_blurred = _sheet(_BLUR_SHADER, "Blur")
	_red = _sheet(_RED_SHADER, "Red")


func _ready() -> void:
	if world == null:
		return
	world.match_.hurt.connect(func(_attacker: int, _from: Vector3, dealt: int) -> void: hit(dealt))
	world.match_.projectile_burst.connect(func(_id: int, weapon: String, at: Vector3) -> void:
		var own: Node3D = _own()
		var stats := Weapons.find(weapon)
		if own != null and stats != null:
			burst_near(own.global_position.distance_to(at), stats.splash_radius))


## The player was hit for `dealt`.
func hit(dealt: float) -> void:
	_hit = maxf(_hit, hit_red(dealt))


## A burst of `radius` went off `distance` away from the player.
func burst_near(distance: float, radius: float) -> void:
	_blur = maxf(_blur, blur_from(distance, radius, Settings.get_value("screen_shake")))


## How red the edges are and how blurred the view is, as drawn.
func showing() -> Dictionary:
	return {"red": _red.material.get_shader_parameter("red") if _red.visible else 0.0, "blur": _blur if _blurred.visible else 0.0}


func _process(delta: float) -> void:
	_time += delta
	_hit = maxf(_hit - HIT_RED * delta / HIT_FADE, 0.0)
	_blur = maxf(_blur - delta / BLUR_FADE, 0.0)
	var record: PlayerRecord = world.match_.own_record() if world != null and world.match_.has_state() and multiplayer.multiplayer_peer != null else null
	var red := 0.0
	if record != null and record.health > 0:
		var full := float(PlayerRecord.MAX_HEALTH)
		red = minf(redness(record.health, full) * beat(record.health, full, _time) + _hit, 0.85)
	else:
		# Nobody to be hurt: out of a match, watching, or knocked out.
		_hit = 0.0
		_blur = 0.0
	_red.visible = red > 0.005
	_red.material.set_shader_parameter("red", red)
	# The blur reads the whole picture back, so it is drawn only when wanted.
	_blurred.visible = _blur > 0.01
	_blurred.material.set_shader_parameter("amount", _blur)


func _own() -> Node3D:
	return world.player_node(multiplayer.get_unique_id()) if world != null and multiplayer.multiplayer_peer != null else null


func _sheet(code: String, called: String) -> ColorRect:
	var sheet := ColorRect.new()
	sheet.name = called
	sheet.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	sheet.mouse_filter = Control.MOUSE_FILTER_IGNORE
	sheet.visible = false
	var shader := Shader.new()
	shader.code = code
	var material := ShaderMaterial.new()
	material.shader = shader
	sheet.material = material
	add_child(sheet)
	return sheet
