extends Node3D
## How a player looks to everyone else: a soldier in a uniform of the
## player's colour, with a helmet, boots and a chest rig, and a face that
## shows how things are going. One arm holds the weapon. It is built from
## simple shapes and animated in code. Its measurements are BodyShape's,
## which shots are tested against too, so the soldier is hit where it is
## seen. It stands on the player's feet and fits inside their collision
## capsule.
##
## The player sets its colour and tells it how it is moving, where it is
## looking and where its weapon is. From that it works out what it is doing
## (see CharacterMotion) and moves its limbs from one pose to the next: it
## walks, runs, crouches, slides, jumps, falls and lands, steadies its weapon
## to aim, flinches when hurt, melts when knocked out and pops back on
## returning.

const INK := Color(0.1, 0.09, 0.09)
## The blotches of camouflage each kind of cheese wears (see Looks): a blue's
## are the blue of its veins and a brie's the white of its rind, as in the
## snow; a cheddar's are a darker shade of the uniform, and a Swiss goes
## plain.
const VEIN := Color(0.16, 0.42, 0.55)
const BLOOM := Color(0.96, 0.95, 0.92)
const SKIN := Color(0.86, 0.66, 0.52)
const GEAR := Color(0.22, 0.23, 0.18)
const BOOTS := Color(0.15, 0.12, 0.1)
const BLUSH := Color(0.93, 0.48, 0.45)
const TEETH := Color(0.97, 0.97, 0.95)
## How far the legs swing, in radians, at full running speed.
const STRIDE := 0.7
## Steps per metre walked.
const STEPS_PER_METRE := 0.9
const MELT_TIME := 0.45
## How a knocked-out character goes, by what did it: thrown back by a shot
## or a strike and melting where it lands, or blown to crumbs by a burst.
const THROWN := "thrown"
const SCATTERED := "scattered"
## How fast a thrown character leaves, in its own measurements a second, and
## how far over it tips.
const THROW_SPEED := 2.2
const THROW_TIP := 1.1
const POP_TIME := 0.2
## Seconds the face stays screwed up after being hurt.
const GRIMACE_TIME := 0.5
## The faces a character makes, the first that applies showing: wincing
## when hurt, dizzy when melted by an Overload, cheering after a knock-out,
## set when firing, and otherwise calm.
const WINCE := "wince"
const DIZZY := "dizzy"
const CHEER := "cheer"
const SET := "set"
const CALM := "calm"
## Seconds a cheer lasts, and a set face after the last shot.
const CHEER_TIME := 1.4
const SET_TIME := 0.5
## Seconds a blink lasts, and the shortest and longest between two.
const BLINK_TIME := 0.12
const BLINK_GAP := Vector2(2.0, 5.5)
## How far the eyes slide toward someone nearby.
const GLANCE_SHIFT := 0.008
## Seconds the whole body flashes pale when hit.
const FLASH_TIME := 0.09
## How far above the surface each layer of what is drawn on it sits, so that
## none flickers through another.
const LAYER := 0.003
## How far the eyes shift up or down as the player looks up or down.
const GAZE_SHIFT := 0.008
## The faces of the torso that camouflage is drawn on.
enum { FRONT, BACK }
## Blotches of camouflage, as [face, across, up, radius], `across` and `up`
## from the middle of that face.
const CAMO := [
	[FRONT, -0.12, 0.1, 0.035], [FRONT, 0.12, -0.02, 0.04], [FRONT, -0.125, -0.07, 0.03], [FRONT, 0.125, 0.11, 0.025],
	[BACK, 0.0, 0.08, 0.045], [BACK, -0.11, -0.05, 0.04], [BACK, 0.12, -0.08, 0.03], [BACK, 0.11, 0.1, 0.03],
]

## Whether to draw it at all: not for the player's own machine, which sees
## from inside it.
var shown := true

var _material := StandardMaterial3D.new()
var _helmet_material := StandardMaterial3D.new()
var _camo_material := _flat(VEIN)
var _skin := _solid(SKIN)
var _gear := _solid(GEAR)
var _boot := _solid(BOOTS)
# The look being worn (see Looks), and the parts that show it: camouflage,
# a hat and something held.
var _look := -1
var _camo: Array[Node3D] = []
var _hat: Node3D
var _held: Node3D
var _ink := _flat(INK)
var _torso: MeshInstance3D
var _legs: Array[Node3D] = []
var _arms: Array[Node3D] = []
var _eyes: Array[Node3D] = []
var _calm: Node3D
var _wince: Node3D
var _phase := 0.0
var _swing := 0.0
var _speed := 0.0
var _airborne := false
# How fast the player is rising, whether they are sliding, and how long
# since they last touched down.
var _rising := 0.0
var _sliding := false
var _since_landing := INF
# The state the legs are in and what the free arm is doing (see
# CharacterMotion), where the limbs have got to on their way to its pose,
# what is going on that the arm might act on, and the arm itself.
var _state := CharacterMotion.IDLE
var _blend: Dictionary = CharacterMotion.POSES[CharacterMotion.IDLE].duplicate()
var _acting := {}
var _holding := {}
var _action := CharacterMotion.NONE
var _action_for := 0.0
var _arm := Vector3(0.0, 0.0, Emotes.ARM_REST)
var _gaze := 0.0
var _grimace_left := 0.0
# How far down the player is crouched, 0 to 1, and everything above the legs,
# which comes down with them, with where each rests when standing.
var _crouch := 0.0
var _lean := 0.0
var _upper := {}
var _flash_left := 0.0
var _cheer_left := 0.0
# The emote being made (see Emotes), or -1, and seconds into it.
var _emote := -1
var _emote_for := 0.0
var _set_left := 0.0
var _dizzy := false
var _faces := {}
var _stars: Node3D
# Seconds until the next blink, and left of the one going on.
var _blink_in := 3.0
var _blink_left := 0.0
# How far to one side the eyes are turned, -1 to 1, toward someone near.
var _glance := 0.0
var _glowing := false
# 0 standing, 1 fully melted.
var _melt := 0.0
var _melting := false
# 0 just appeared, 1 full size.
var _pop := 1.0
# How the character was knocked out, and the way it is being thrown, in the
# player's own directions, slowing as it goes.
var _ending := THROWN
var _thrown := Vector3.ZERO


func _init() -> void:
	_material.roughness = 0.85
	_helmet_material.roughness = 0.6
	_build()
	scale = Vector3.ONE * BodyShape.SIZE
	# Until told otherwise, the weapon is where the player's scene holds it.
	reach_for(Vector3(0.36, BodyShape.HEAD_HEIGHT - 0.29, -0.34))


## The uniform's colour. The helmet is a darker shade of the same.
func set_colour(colour: Color) -> void:
	_material.albedo_color = colour
	_helmet_material.albedo_color = colour.darkened(0.45)
	_show_camo()


func colour() -> Color:
	return _material.albedo_color


## Whether the body glows, as in an Overload.
func set_glow(on: bool) -> void:
	_glowing = on
	_light()


# The body lights up orange in an Overload, and white for an instant when hit.
func _light() -> void:
	_material.emission_enabled = _glowing or is_flashing()
	_material.emission = Color.WHITE if is_flashing() else Color(1.0, 0.45, 0.1)
	_material.emission_energy_multiplier = 0.9 if is_flashing() else 1.2


## How the player is moving, for the walk: speed across the ground in metres
## per second, and whether they are off the ground.
## `rising` is how fast they are going up (below zero, down), and `sliding`
## whether they are in a slide.
func set_motion(speed: float, airborne: bool, rising := 0.0, sliding := false) -> void:
	if _airborne and not airborne:
		_since_landing = 0.0
	_speed = speed
	_airborne = airborne
	_rising = rising
	_sliding = sliding


## Whether the player is aiming, or reloading: the free arm steadies the
## weapon for as long as either is so.
func set_holding(aiming: bool, reloading: bool) -> void:
	_holding = {CharacterMotion.AIM: aiming, CharacterMotion.RELOAD: reloading}


## Something momentary has happened that the free arm acts out: one of
## CharacterMotion's FIRE, THROW, MELEE and HURT.
func act(kind: String) -> void:
	if CharacterMotion.ACTION_TIME.has(kind):
		_acting[kind] = CharacterMotion.ACTION_TIME[kind]


## The state the legs are in: one of CharacterMotion's, such as RUN or FALL.
func state() -> String:
	return _state


## What the free arm is doing: one of CharacterMotion's, such as AIM, or NONE.
func action() -> String:
	return _action


## Wears `look` (see Looks): the camouflage of the kind of cheese, a hat on
## the helmet and something in the free hand. None of it changes the shape
## that can be hit.
func wear(look: int) -> void:
	if look == _look:
		return
	_look = look
	var choices := Looks.unpack(look)
	_show_camo()
	for old: Node3D in [_hat, _held]:
		if old != null:
			_upper.erase(old)
			remove_child(old)
			old.queue_free()
	_hat = _build_hat(choices.hat)
	_held = _build_held(choices.held)
	for added: Node3D in [_hat, _held]:
		if added != null:
			add_child(added)
			_upper[added] = added.position
	set_crouch(_crouch)


## What is being worn, as Looks.unpack gives it.
func worn() -> Dictionary:
	return Looks.unpack(maxi(_look, 0))


# Camouflage in the colours of the kind of cheese worn, or none for a Swiss.
func _show_camo() -> void:
	var cheese: String = worn().cheese
	for blotch in _camo:
		blotch.visible = cheese != "swiss"
	match cheese:
		"blue":
			_camo_material.albedo_color = VEIN
		"brie":
			_camo_material.albedo_color = BLOOM
		_:
			_camo_material.albedo_color = _material.albedo_color.darkened(0.4)


# A hat of `kind` for the top of the helmet, or null for none.
func _build_hat(kind: String) -> Node3D:
	if kind == "none":
		return null
	var hat := Node3D.new()
	hat.name = "Hat"
	# Sitting a little down over the top of the helmet.
	hat.position = Vector3(0, BodyShape.HEAD_CENTRE.y + BodyShape.HEAD_SIZE.y / 2.0 - 0.03, 0)
	match kind:
		"party":
			var cone := CylinderMesh.new()
			cone.top_radius = 0.0
			cone.bottom_radius = 0.085
			cone.height = 0.24
			cone.radial_segments = 14
			hat.add_child(Rounded.instance(cone, _flat(Color(0.9, 0.3, 0.6)), Vector3(0, 0.12, 0)))
			hat.add_child(_dab(0.03, Color(0.95, 0.95, 0.95), Vector3(0, 0.25, 0)))
		"top":
			hat.add_child(Rounded.instance(Rounded.cylinder(0.15, 0.025, 0.01), _flat(INK.lightened(0.08)), Vector3(0, 0.0, 0)))
			hat.add_child(Rounded.instance(Rounded.cylinder(0.095, 0.2, 0.02), _flat(INK.lightened(0.08)), Vector3(0, 0.1, 0)))
			hat.add_child(Rounded.instance(Rounded.cylinder(0.098, 0.035, 0.01), _flat(Color(0.7, 0.12, 0.12)), Vector3(0, 0.03, 0)))
		"crown":
			var silver := Color(0.82, 0.84, 0.88)
			hat.add_child(Rounded.instance(Rounded.cylinder(0.1, 0.07, 0.015), _flat(silver), Vector3(0, 0.03, 0)))
			for point in 5:
				var around := TAU * point / 5.0
				hat.add_child(_dab(0.028, silver, Vector3(cos(around) * 0.085, 0.085, sin(around) * 0.085)))
			hat.add_child(_dab(0.022, Color(0.75, 0.12, 0.2), Vector3(0, 0.035, -0.1)))
		"chef":
			hat.add_child(Rounded.instance(Rounded.cylinder(0.09, 0.1, 0.02), _flat(BLOOM), Vector3(0, 0.04, 0)))
			var puff := _dab(0.125, BLOOM, Vector3(0, 0.16, 0))
			puff.scale = Vector3(1.0, 0.7, 1.0)
			hat.add_child(puff)
		"cowboy":
			var leather := Color(0.42, 0.27, 0.17)
			hat.add_child(Rounded.instance(Rounded.cylinder(0.2, 0.025, 0.012), _flat(leather), Vector3(0, 0.0, 0)))
			hat.add_child(Rounded.instance(Rounded.cylinder(0.095, 0.13, 0.04), _flat(leather), Vector3(0, 0.07, 0)))
			hat.add_child(Rounded.instance(Rounded.cylinder(0.098, 0.025, 0.008), _flat(INK), Vector3(0, 0.03, 0)))
	return hat


# Something of `kind` for the free hand to hold, or null for nothing. It
# stands up from where that hand hangs.
func _build_held(kind: String) -> Node3D:
	if kind == "none":
		return null
	var held := Node3D.new()
	held.name = "Held"
	var arm: Array = BodyShape.ARMS[0]
	# Where the glove is, at rest.
	held.position = (arm[0] as Vector3) + Vector3(sin(Emotes.ARM_REST), -cos(Emotes.ARM_REST), 0) * arm[1]
	var stick := CylinderMesh.new()
	stick.top_radius = 0.006
	stick.bottom_radius = 0.006
	stick.radial_segments = 6
	match kind:
		"balloon":
			stick.height = 0.7
			held.add_child(Rounded.instance(stick, _flat(Color(0.9, 0.9, 0.9)), Vector3(0.02, 0.35, 0)))
			var balloon := _dab(0.085, Color(0.85, 0.15, 0.2), Vector3(0.02, 0.78, 0))
			balloon.scale = Vector3(1.0, 1.2, 1.0)
			held.add_child(balloon)
		"flag":
			stick.height = 0.6
			stick.top_radius = 0.01
			stick.bottom_radius = 0.01
			held.add_child(Rounded.instance(stick, _flat(INK.lightened(0.3)), Vector3(0, 0.3, 0)))
			held.add_child(Rounded.instance(Rounded.box(Vector3(0.012, 0.11, 0.17), 0.004), _flat(Color(0.95, 0.95, 0.95)), Vector3(0, 0.54, -0.09)))
		"lollipop":
			stick.height = 0.3
			stick.top_radius = 0.008
			stick.bottom_radius = 0.008
			held.add_child(Rounded.instance(stick, _flat(Color(0.95, 0.95, 0.95)), Vector3(0, 0.15, 0)))
			var sweet := Rounded.instance(Rounded.cylinder(0.07, 0.025, 0.01), _flat(Color(0.9, 0.3, 0.6)), Vector3(0, 0.33, 0))
			sweet.rotation.x = PI / 2.0
			held.add_child(sweet)
	return held


# A small ball of `colour`.
static func _dab(radius: float, colour: Color, at: Vector3) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 10
	sphere.rings = 5
	return Rounded.instance(sphere, _flat(colour), at)


## How far the player is crouched, from 0 (standing) to 1: the knees bend
## and the rest comes down.
func set_crouch(amount: float) -> void:
	_crouch = clampf(amount, 0.0, 1.0)
	var drop := Vector3.DOWN * BodyShape.CROUCH_DROP * _crouch
	for part: Node3D in _upper:
		part.position = _upper[part] + drop
	for index in _legs.size():
		_legs[index].position = (BodyShape.LEGS[index][0] as Vector3) + drop
	_bend_legs()


## How far the player leans: -1 to their left, 1 to their right.
func set_lean(lean: float) -> void:
	_lean = clampf(lean, -1.0, 1.0)
	if not _melting:
		rotation = BodyShape.lean_basis(_lean).get_euler()


## How far up (or, below zero, down) the player is looking, in radians: the
## eyes follow.
func set_gaze(pitch: float) -> void:
	_gaze = pitch


## The player was just hurt: the body flashes for an instant, and the face
## screws up for a moment.
func grimace() -> void:
	act(CharacterMotion.HURT)
	_grimace_left = GRIMACE_TIME
	_flash_left = FLASH_TIME
	_emote = -1
	_light()


## Makes emote `kind` (see Emotes): a few moments of arm and body, which
## being hurt or firing cuts short.
func emote(kind: int) -> void:
	_emote = kind
	_emote_for = 0.0


## The emote being made, or -1.
func emoting() -> int:
	return _emote


## The player has just knocked someone out: a cheer.
func cheer() -> void:
	_cheer_left = CHEER_TIME


## The player has just fired: a set face, for as long as they keep firing.
func fire_face() -> void:
	_set_left = SET_TIME
	_emote = -1


## Whether the player is melted by someone's Overload, and seeing stars.
func set_dizzy(on: bool) -> void:
	_dizzy = on


## Which way the eyes are turned to look at someone nearby: -1 to the
## character's left, 1 to its right, 0 straight ahead.
func set_glance(side: float) -> void:
	_glance = clampf(side, -1.0, 1.0)


## The face the character is making now: one of WINCE, DIZZY, CHEER, SET
## and CALM.
func expression() -> String:
	if is_grimacing():
		return WINCE
	if _dizzy:
		return DIZZY
	if _cheer_left > 0.0 or _emote == Emotes.CHEER:
		return CHEER
	if _set_left > 0.0:
		return SET
	return CALM


## Whether the eyes are shut in a blink.
func is_blinking() -> bool:
	return _blink_left > 0.0


func is_flashing() -> bool:
	return _flash_left > 0.0


func is_grimacing() -> bool:
	return _grimace_left > 0.0


## Stretches the weapon arm to the hand holding the weapon, at `hand` in this
## node's own measurements.
func reach_for(hand: Vector3) -> void:
	var arm := _arms[1]
	var reach := hand - arm.position
	var length := reach.length()
	if length < 0.001:
		return
	# The sleeve hangs down its own Y axis, so point that at the hand.
	arm.quaternion = Quaternion(Vector3.DOWN, reach / length)
	arm.get_child(0).scale.y = length / BodyShape.ARMS[1][1]


## How a character knocked out with `weapon` goes: SCATTERED by anything
## that bursts, THROWN by anything else.
static func ending_for(weapon: String) -> String:
	var stats := Weapons.find(weapon)
	return SCATTERED if stats != null and stats.fires_projectile() else THROWN


## The character is about to be knocked out by `weapon`, from the direction
## `away` points away from, in the player's own directions: it is thrown
## that way as it melts, or, by a burst, gone at once in a scatter of crumbs.
func knock(weapon: String, away: Vector3) -> void:
	_ending = ending_for(weapon)
	var level := Vector3(away.x, 0.0, away.z)
	_thrown = level.normalized() * THROW_SPEED if level.length() > 0.001 else Vector3.ZERO
	# Should word of how arrive after word that they are out.
	if _melting and _ending == SCATTERED:
		_melt = 1.0


## How the character was last knocked out: THROWN or SCATTERED.
func ending() -> String:
	return _ending


## Knocked out (melting away) or back in play (popping up).
func set_in_play(in_play: bool) -> void:
	if in_play and _melting:
		_melting = false
		_melt = 0.0
		_pop = 0.0
		_ending = THROWN
		_thrown = Vector3.ZERO
		position = Vector3.ZERO
	elif not in_play and not _melting:
		_melting = true
		# A burst leaves nothing to melt.
		if _ending == SCATTERED:
			_melt = 1.0


# Turns each leg at the hip and the knee: swinging with the stride, drawn up
# in the air, thrown forward in a slide, spread when crouched or landing, and
# bent at the knee to keep the feet on the ground in a crouch. The boots stay
# level with the ground.
func _bend_legs() -> void:
	var crouched: float = BodyShape.knee_bend(_crouch) * (1.0 - _blend.reach)
	for index in _legs.size():
		var side := 1.0 if index == 0 else -1.0
		var out := -1.0 if (BodyShape.LEGS[index][0] as Vector3).x < 0.0 else 1.0
		var thigh: float = lerpf(_swing * side * _blend.stride, 0.6, _blend.tuck) + 1.15 * _blend.reach + crouched
		var knee: float = -2.0 * crouched - 1.3 * _blend.tuck - maxf(-_swing * side * _blend.stride, 0.0) * 0.8
		_legs[index].rotation = Vector3(thigh, 0.0, _blend.splay * out)
		var lower: Node3D = _legs[index].get_node("Knee")
		lower.rotation.x = knee
		lower.get_node("Boot").rotation.x = -(thigh + knee)


## How far a leg swings forward at walk phase `phase` (radians) when moving at
## `speed`: a sine wave, larger at speed, nothing standing still.
static func leg_swing(phase: float, speed: float) -> float:
	return sin(phase) * STRIDE * clampf(speed / 6.0, 0.0, 1.0)


func is_melted() -> bool:
	return _melting and _melt >= 1.0


func _process(delta: float) -> void:
	_phase = fmod(_phase + _speed * STEPS_PER_METRE * TAU * delta, TAU)
	var swing := leg_swing(_phase, _speed)
	# What the legs are doing, and the limbs a step nearer its pose.
	_since_landing += delta
	_state = CharacterMotion.state(_speed, _rising, _airborne, _crouch, _sliding, _since_landing)
	var heading: Dictionary = CharacterMotion.POSES[_state]
	var nearer := CharacterMotion.blend(delta)
	for key: String in _blend:
		_blend[key] = lerpf(_blend[key], heading[key], nearer)
	_swing = swing
	_bend_legs()
	# What the free arm is doing on top of that; the other holds the weapon.
	var doing := _holding.duplicate()
	for kind: String in _acting.keys():
		_acting[kind] -= delta
		doing[kind] = _acting[kind] > 0.0
		if _acting[kind] <= 0.0:
			_acting.erase(kind)
	var now := CharacterMotion.action(doing)
	_action_for = _action_for + delta if now == _action else 0.0
	_action = now
	var rest := Vector3(-swing * 0.6 * _blend.pump, 0.0, Emotes.ARM_REST - _blend.lift)
	_arm = _arm.lerp(CharacterMotion.arm(_action, _action_for, rest), nearer)
	_arms[0].rotation = _arm
	var sway := 0.0
	var hop := 0.0
	if _emote >= 0:
		_emote_for += delta
		if _emote_for >= Emotes.TIME or _melting:
			_emote = -1
		else:
			var pose := Emotes.pose(_emote, _emote_for)
			_arms[0].rotation = Vector3(0.0, 0.0, pose.arm)
			sway = pose.sway
			hop = pose.hop
	if not _melting:
		# Tipped on its feet by a lean or a dance, and lifted by a hop.
		rotation = BodyShape.lean_basis(_lean).get_euler() + Vector3(0.0, 0.0, sway)
		position = Vector3(0.0, hop * BodyShape.SIZE, 0.0)
	_grimace_left = maxf(_grimace_left - delta, 0.0)
	if is_flashing():
		_flash_left = maxf(_flash_left - delta, 0.0)
		_light()
	_cheer_left = maxf(_cheer_left - delta, 0.0)
	_set_left = maxf(_set_left - delta, 0.0)
	var face := expression()
	for kind: String in _faces:
		_faces[kind].visible = kind == face
	# Blinking, now and then, when nothing else is going on with the eyes.
	_blink_left = maxf(_blink_left - delta, 0.0)
	_blink_in -= delta
	if _blink_in <= 0.0:
		_blink_in = randf_range(BLINK_GAP.x, BLINK_GAP.y)
		if face == CALM or face == SET:
			_blink_left = BLINK_TIME
	_stars.visible = face == DIZZY
	_stars.rotate_y(delta * 5.0)
	for index in _eyes.size():
		var eye := _eyes[index]
		eye.position.y = BodyShape.head_height(_crouch) + clampf(_gaze, -1.0, 1.0) * GAZE_SHIFT
		eye.position.x = BodyShape.eye_centres()[index].x + _glance * GLANCE_SHIFT
		# Screwed up tight when wincing, shut in a blink, wide in a cheer.
		var open := 1.0
		if face == WINCE:
			open = 0.35
		elif is_blinking() or face == DIZZY:
			open = 0.12
		elif face == CHEER:
			open = 1.25
		eye.scale.z = open
	if _melting:
		_melt = minf(_melt + delta / MELT_TIME, 1.0)
		# Thrown back, tipping over, and slowing to a stop as it melts.
		position += _thrown * delta * BodyShape.SIZE
		_thrown *= exp(-5.0 * delta)
		if _thrown != Vector3.ZERO:
			rotation = Vector3(0, 0, 0)
			rotate(_thrown.normalized().cross(Vector3.DOWN).normalized(), THROW_TIP * _melt)
	_pop = minf(_pop + delta / POP_TIME, 1.0)
	# Melting sags and spreads the body; popping back grows it from the floor.
	var sag := 1.0 - 0.85 * _melt
	var spread := 1.0 + 0.6 * _melt
	scale = Vector3(spread, sag, spread) * _ease_out(_pop) * BodyShape.SIZE
	visible = shown and not is_melted()


static func _ease_out(t: float) -> float:
	return 1.0 - (1.0 - t) * (1.0 - t)


func _build() -> void:
	# The torso, in the uniform's colour, with a belt and a chest rig over it.
	_torso = Rounded.instance(Rounded.box(BodyShape.TORSO_SIZE, BodyShape.ROUNDING), _material, BodyShape.TORSO_CENTRE)
	_torso.name = "Torso"
	add_child(_torso)
	var half := BodyShape.TORSO_SIZE / 2.0
	_torso.add_child(Rounded.instance(Rounded.box(Vector3(half.x * 2.0 + 0.008, 0.045, half.z * 2.0 + 0.008), 0.01), _gear, Vector3(0, -half.y + 0.04, 0)))
	_torso.add_child(Rounded.instance(Rounded.box(Vector3(0.04, 0.035, 0.008), 0.003), _flat(Color(0.62, 0.58, 0.45)), Vector3(0, -half.y + 0.04, -half.z - 0.006)))
	for side in [-1.0, 1.0]:
		# Pouches on the belt, and straps over the shoulders, front and back.
		_torso.add_child(Rounded.instance(Rounded.box(Vector3(0.07, 0.065, 0.012), 0.005), _gear, Vector3(0.12 * side, -half.y + 0.075, -half.z - 0.004)))
		for face in [-1.0, 1.0]:
			_torso.add_child(Rounded.instance(Rounded.box(Vector3(0.045, half.y * 2.0 - 0.06, 0.008), 0.003), _gear, Vector3(0.09 * side, 0.03, (half.z + 0.002) * face)))
		_torso.add_child(Rounded.instance(Rounded.box(Vector3(0.05, 0.006, half.z * 2.0 + 0.004), 0.002), _gear, Vector3(0.09 * side, half.y, 0)))
	# Magazine pouches across the chest.
	for across in [-0.035, 0.035]:
		_torso.add_child(Rounded.instance(Rounded.box(Vector3(0.055, 0.08, 0.012), 0.005), _gear, Vector3(across, 0.02, -half.z - 0.004)))

	for place: Array in CAMO:
		var normal := Vector3.FORWARD if place[0] == FRONT else Vector3.BACK
		var at := BodyShape.TORSO_CENTRE + Vector3(place[1], place[2], normal.z * half.z)
		var blotch := Rounded.spot(place[3], _camo_material, at, normal, LAYER)
		blotch.name = "Camo%d" % (_camo.size() + 1)
		blotch.scale = Vector3(1.4, 1.0, 1.0)
		blotch.visible = false
		add_child(blotch)
		_camo.append(blotch)

	_build_head()
	_build_face()

	for leg: Array in BodyShape.LEGS:
		var hip := Node3D.new()
		hip.name = "Leg%d" % (_legs.size() + 1)
		hip.position = leg[0]
		add_child(hip)
		# The thigh, down to the knee, and the shin and boot below it, which
		# bends at the knee.
		var trousers := Vector3(BodyShape.LEG_WIDTH, BodyShape.THIGH + 0.03, BodyShape.LEG_DEPTH)
		hip.add_child(Rounded.instance(Rounded.box(trousers, 0.02), _material, Vector3(0, -BodyShape.THIGH / 2.0, 0)))
		var knee := Node3D.new()
		knee.name = "Knee"
		knee.position = Vector3(0, -BodyShape.THIGH, 0)
		hip.add_child(knee)
		knee.add_child(Rounded.instance(Rounded.box(trousers, 0.02), _material, Vector3(0, -BodyShape.THIGH / 2.0 + 0.02, 0)))
		var boot := Node3D.new()
		boot.name = "Boot"
		boot.position = Vector3(0, -BodyShape.THIGH, 0)
		knee.add_child(boot)
		boot.add_child(Rounded.instance(Rounded.box(Vector3(BodyShape.LEG_WIDTH, 0.075, BodyShape.LEG_DEPTH), 0.025), _boot, Vector3(0, 0.0375, 0)))
		boot.add_child(Rounded.instance(Rounded.box(Vector3(BodyShape.LEG_WIDTH - 0.004, 0.02, BodyShape.LEG_DEPTH - 0.004), 0.008), _ink, Vector3(0, 0.01, 0)))
		_legs.append(hip)
	for arm: Array in BodyShape.ARMS:
		var shoulder := Node3D.new()
		shoulder.name = "Arm%d" % (_arms.size() + 1)
		shoulder.position = arm[0]
		add_child(shoulder)
		shoulder.add_child(_sleeve(arm[1]))
		_arms.append(shoulder)
	# The free arm hangs out from the side, with a glove on the end. The
	# weapon arm's hand is part of the weapon it holds.
	_arms[0].rotation.z = Emotes.ARM_REST
	_arms[0].add_child(_ball(0.042, Vector3(0, -BodyShape.ARMS[0][1], 0)))
	for part in get_children():
		if not _legs.has(part):
			_upper[part] = part.position


# The head: a face under a helmet in a darker shade of the uniform, with its
# rim and a strap under the chin. All of it fills the box that is hit as the
# head.
func _build_head() -> void:
	var size := BodyShape.HEAD_SIZE
	var bottom := BodyShape.HEAD_FROM
	var head := Node3D.new()
	head.name = "Head"
	add_child(head)
	var face := Vector3(size.x, BodyShape.HEAD_HEIGHT + 0.05 - bottom, size.z)
	head.add_child(Rounded.instance(Rounded.box(face, 0.02), _skin, Vector3(0, bottom + face.y / 2.0, 0)))
	var top := bottom + size.y
	var shell := Vector3(size.x, top - (BodyShape.HEAD_HEIGHT + 0.035), size.z)
	head.add_child(Rounded.instance(Rounded.box(shell, BodyShape.ROUNDING), _helmet_material, Vector3(0, top - shell.y / 2.0, 0)))
	# The rim, a hair proud all round, and the shell coming down over the
	# ears and the back of the neck.
	var rim := top - shell.y + 0.006
	head.add_child(Rounded.instance(Rounded.box(Vector3(size.x + 0.006, 0.016, size.z + 0.006), 0.005), _helmet_material, Vector3(0, rim, 0)))
	var guard := Vector3(size.x + 0.006, 0.075, size.z * 0.62)
	head.add_child(Rounded.instance(Rounded.box(guard, 0.012), _helmet_material, Vector3(0, rim - guard.y / 2.0 + 0.008, size.z / 2.0 - guard.z / 2.0 + 0.003)))
	for side in [-1.0, 1.0]:
		var strap := Rounded.instance(Rounded.box(Vector3(0.006, 0.09, 0.014), 0.002), _gear, Vector3((face.x / 2.0 + 0.003) * side, bottom + 0.045, -0.03))
		head.add_child(strap)
	head.add_child(Rounded.instance(Rounded.box(Vector3(0.11, 0.008, 0.014), 0.002), _gear, Vector3(0, bottom + 0.003, -0.05)))


# The face, drawn on the front of the head: two dot eyes, and either a
# smile between rosy cheeks or, when wincing, gritted teeth under a frown.
func _build_face() -> void:
	var front := -BodyShape.HEAD_SIZE.z / 2.0
	var eye_level := BodyShape.HEAD_HEIGHT
	for centre in BodyShape.eye_centres():
		var eye := Rounded.spot(BodyShape.EYE_RADIUS, _ink, centre, Vector3.FORWARD, LAYER * 2.0)
		eye.name = "Eye%d" % (_eyes.size() + 1)
		# A glint, so the eyes look alive.
		eye.add_child(Rounded.spot(BodyShape.EYE_RADIUS * 0.32, _flat(Color.WHITE), Vector3(0.006, 0.0, 0.006), Vector3.UP, LAYER))
		add_child(eye)
		_eyes.append(eye)

	_calm = Node3D.new()
	_calm.name = "Calm"
	add_child(_calm)
	var blush := _flat(BLUSH)
	var smile := MeshInstance3D.new()
	smile.mesh = _arc(0.03, 0.008, deg_to_rad(205.0), deg_to_rad(335.0))
	smile.material_override = _ink
	smile.position = Vector3(0, eye_level - 0.03, front - LAYER * 2.0)
	_calm.add_child(smile)

	_wince = Node3D.new()
	_wince.name = "Wince"
	_wince.visible = false
	add_child(_wince)
	var mouth := Vector3(0, eye_level - 0.065, front)
	_wince.add_child(_patch(Vector2(0.08, 0.034), _ink, mouth, 1))
	_wince.add_child(_patch(Vector2(0.07, 0.024), _flat(TEETH), mouth, 2))
	_wince.add_child(_patch(Vector2(0.07, 0.005), _ink, mouth, 3))
	for gap in [-0.018, 0.0, 0.018]:
		_wince.add_child(_patch(Vector2(0.005, 0.024), _ink, mouth + Vector3(gap, 0, 0), 3))
	# Brows drawn down toward the middle.
	for side in [-1.0, 1.0]:
		var brow := _patch(Vector2(0.042, 0.01), _ink, Vector3(BodyShape.EYE_APART * side, eye_level + 0.02, front), 2)
		brow.rotation.z = 0.4 * side
		_wince.add_child(brow)

	# Firing: a straight mouth under level brows.
	var set_face := Node3D.new()
	set_face.name = "Set"
	add_child(set_face)
	set_face.add_child(_patch(Vector2(0.045, 0.008), _ink, Vector3(0, eye_level - 0.06, front), 2))
	for side in [-1.0, 1.0]:
		var brow := _patch(Vector2(0.04, 0.009), _ink, Vector3(BodyShape.EYE_APART * side, eye_level + 0.026, front), 2)
		brow.rotation.z = 0.15 * side
		set_face.add_child(brow)

	# A knock-out: a wide open grin, cheeks and all.
	var cheer_face := Node3D.new()
	cheer_face.name = "Cheer"
	add_child(cheer_face)
	for side in [-1.0, 1.0]:
		cheer_face.add_child(Rounded.spot(0.015, blush, Vector3(0.057 * side, eye_level - 0.038, front), Vector3.FORWARD, LAYER))
	var grin := MeshInstance3D.new()
	grin.mesh = _arc(0.018, 0.036, deg_to_rad(180.0), deg_to_rad(360.0))
	grin.material_override = _ink
	grin.position = Vector3(0, eye_level - 0.04, front - LAYER * 2.0)
	cheer_face.add_child(grin)
	var tongue := MeshInstance3D.new()
	tongue.mesh = _arc(0.011, 0.018, deg_to_rad(200.0), deg_to_rad(340.0))
	tongue.material_override = _flat(BLUSH.darkened(0.15))
	tongue.position = Vector3(0, eye_level - 0.049, front - LAYER * 3.0)
	cheer_face.add_child(tongue)

	# Melted by an Overload: a wavering mouth, and stars going round the head.
	var dizzy_face := Node3D.new()
	dizzy_face.name = "Dizzy"
	add_child(dizzy_face)
	for step in 4:
		var wave := MeshInstance3D.new()
		var up := step % 2 == 0
		wave.mesh = _arc(0.011, 0.007, deg_to_rad(0.0 if up else 180.0), deg_to_rad(180.0 if up else 360.0))
		wave.material_override = _ink
		wave.position = Vector3(-0.033 + step * 0.022, eye_level - 0.06, front - LAYER * 2.0)
		dizzy_face.add_child(wave)
	_stars = Node3D.new()
	_stars.name = "Stars"
	_stars.position = Vector3(0, BodyShape.HEAD_CENTRE.y + BodyShape.HEAD_SIZE.y / 2.0 + 0.06, 0)
	_stars.visible = false
	add_child(_stars)
	var star := _flat(Color(0.95, 0.95, 1.0))
	for step in 3:
		var around := TAU * step / 3.0
		var twinkle := SphereMesh.new()
		twinkle.radius = 0.028
		twinkle.height = 0.056
		twinkle.radial_segments = 4
		twinkle.rings = 2
		var point := MeshInstance3D.new()
		point.mesh = twinkle
		point.material_override = star
		point.position = Vector3(cos(around), 0, sin(around)) * 0.16
		_stars.add_child(point)

	_faces = {WINCE: _wince, DIZZY: dizzy_face, CHEER: cheer_face, SET: set_face, CALM: _calm}


# A plain surface of `colour`, as of cloth, skin or rubber.
static func _solid(colour: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = colour
	material.roughness = 0.9
	return material


static func _flat(colour: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = colour
	material.roughness = 1.0
	# Drawn-on marks are flat, and seen from either side.
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material


# A flat rectangle of `size` on the front of the head, centred on `at`.
static func _patch(size: Vector2, material: Material, at: Vector3, layer: int) -> MeshInstance3D:
	var box := BoxMesh.new()
	box.size = Vector3(size.x, size.y, 0.001)
	var patch := MeshInstance3D.new()
	patch.mesh = box
	patch.material_override = material
	patch.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	patch.position = at + Vector3.FORWARD * LAYER * layer
	return patch


# A flat curved line `width` thick: part of a circle of `radius`, from angle
# `from` round to `to`, lying in the XY plane.
static func _arc(radius: float, width: float, from: float, to: float) -> ArrayMesh:
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	var steps := 12
	for step in steps + 1:
		var angle := lerpf(from, to, float(step) / steps)
		var out := Vector3(cos(angle), sin(angle), 0)
		vertices.append(out * (radius - width / 2.0))
		vertices.append(out * (radius + width / 2.0))
		normals.append(Vector3.FORWARD)
		normals.append(Vector3.FORWARD)
		if step > 0:
			var last := step * 2
			indices.append_array([last - 2, last - 1, last, last - 1, last + 1, last])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


# A sleeve of the uniform `length` long hanging down from its shoulder.
func _sleeve(length: float) -> Node3D:
	var capsule := CapsuleMesh.new()
	capsule.radius = BodyShape.LIMB_RADIUS
	capsule.height = length
	capsule.radial_segments = 10
	capsule.rings = 3
	var mesh := MeshInstance3D.new()
	mesh.mesh = capsule
	mesh.material_override = _material
	mesh.position = Vector3(0, -length / 2.0, 0)
	# Held by a node of its own, so that stretching it stretches it from the
	# shoulder.
	var holder := Node3D.new()
	holder.add_child(mesh)
	return holder


# A gloved hand.
func _ball(radius: float, at: Vector3) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 12
	sphere.rings = 6
	var mesh := MeshInstance3D.new()
	mesh.mesh = sphere
	mesh.material_override = _gear
	mesh.position = at
	return mesh
