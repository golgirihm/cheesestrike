extends Node3D
## What a player's body is doing, as everyone else sees it: the soldier
## (soldier_character.gd) shows it.
##
## The player sets its colour and tells it how it is moving, where it is
## looking and what it is doing with its hands. From that it works out what
## the legs and the arms are doing (see CharacterMotion), which face it is
## making, when it blinks, and how it goes when knocked out and comes back.
## How all that is drawn is the soldier's: _build, _draw_face and _draw_melt.

const INK := Color(0.1, 0.09, 0.09)
## Seconds a knocked-out body takes to fall back before it is gone.
const MELT_TIME := 1.6
## How a knocked-out character goes, by what did it: thrown back by a shot
## or a strike and melting where it lands, or blown to crumbs by a burst.
const THROWN := "thrown"
const SCATTERED := "scattered"
## How fast a thrown character leaves, in its own measurements a second.
const THROW_SPEED := 2.2
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
## Seconds the whole body flashes pale when hit.
const FLASH_TIME := 0.09
## Going straight ahead, as set_heading has it.
const AHEAD := Vector2(0.0, 1.0)

## Whether to draw it at all: not for the player's own machine, which sees
## from inside it.
var shown := true

# The look being worn (see Looks), and what it puts in the free hand.
var _look := -1
var _held: Node3D
var _speed := 0.0
var _airborne := false
var _heading := AHEAD
# How fast the player is rising, whether they are sliding, and how long
# since they last touched down.
var _rising := 0.0
var _sliding := false
var _since_landing := INF
# The state the legs are in and what the arms are doing (see
# CharacterMotion), and what is going on that the arms might act on.
var _state := CharacterMotion.IDLE
var _acting := {}
var _holding := {}
var _action := CharacterMotion.NONE
var _action_for := 0.0
var _gaze := 0.0
var _grimace_left := 0.0
# How far down the player is crouched, 0 to 1.
var _crouch := 0.0
var _lean := 0.0
var _flash_left := 0.0
var _cheer_left := 0.0
# The emote being made (see Emotes), or -1, and seconds into it.
var _emote := -1
var _emote_for := 0.0
var _set_left := 0.0
var _dizzy := false
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
# The player's colour.
var _colour := Color.WHITE


func _init() -> void:
	_build()
	scale = Vector3.ONE * BodyShape.SIZE


## The player's colour, which the uniform takes on.
func set_colour(colour: Color) -> void:
	_colour = colour


func colour() -> Color:
	return _colour


## Whether the body glows, as in an Overload.
func set_glow(on: bool) -> void:
	_glowing = on
	_light()


# The body lights up orange in an Overload, and white for an instant when hit.
func _light() -> void:
	pass


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


## Which way the player is going, as the body faces: x to its right and y
## ahead of it, for the legs to step that way.
func set_heading(direction: Vector2) -> void:
	_heading = direction


## Whether the player is aiming, or reloading.
func set_holding(aiming: bool, reloading: bool) -> void:
	_holding = {CharacterMotion.AIM: aiming, CharacterMotion.RELOAD: reloading}


## Something momentary has happened that the arms act out: one of
## CharacterMotion's FIRE, THROW, MELEE and HURT.
func act(kind: String) -> void:
	if CharacterMotion.ACTION_TIME.has(kind):
		_acting[kind] = CharacterMotion.ACTION_TIME[kind]


## The state the legs are in: one of CharacterMotion's, such as RUN or FALL.
func state() -> String:
	return _state


## What the arms are doing: one of CharacterMotion's, such as AIM, or NONE.
func action() -> String:
	return _action


## Wears `look` (see Looks): something in the free hand. The soldier keeps
## its own cap and sunglasses whatever hat is chosen. None of it changes the
## shape that can be hit.
func wear(look: int) -> void:
	if look == _look:
		return
	_look = look
	if _held != null:
		remove_child(_held)
		_held.queue_free()
	_held = _build_held(Looks.unpack(look).held)
	if _held != null:
		add_child(_held)


## What is being worn, as Looks.unpack gives it.
func worn() -> Dictionary:
	return Looks.unpack(maxi(_look, 0))


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


## How far the player is crouched, from 0 (standing) to 1.
func set_crouch(amount: float) -> void:
	_crouch = clampf(amount, 0.0, 1.0)


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


## Makes emote `kind` (see Emotes): a few moments of arms and body, which
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


## Holds `gun`, the weapon its player holds, in its hands.
func hold_weapon(_gun: Node3D) -> void:
	pass


## How big others see this body's weapon, against the size it is made at.
func weapon_scale() -> float:
	return 1.0


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


func is_melted() -> bool:
	return _melting and _melt >= 1.0


func _process(delta: float) -> void:
	# What the legs are doing.
	_since_landing += delta
	_state = CharacterMotion.state(_speed, _rising, _airborne, _crouch, _sliding, _since_landing)
	# What the arms are doing on top of that.
	var doing := _holding.duplicate()
	for kind: String in _acting.keys():
		_acting[kind] -= delta
		doing[kind] = _acting[kind] > 0.0
		if _acting[kind] <= 0.0:
			_acting.erase(kind)
	var now := CharacterMotion.action(doing)
	_action_for = _action_for + delta if now == _action else 0.0
	_action = now
	var sway := 0.0
	var hop := 0.0
	if _emote >= 0:
		_emote_for += delta
		if _emote_for >= Emotes.TIME or _melting:
			_emote = -1
		else:
			var emote_pose := Emotes.pose(_emote, _emote_for)
			sway = emote_pose.sway
			hop = emote_pose.hop
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
	# Blinking, now and then, when nothing else is going on with the eyes.
	_blink_left = maxf(_blink_left - delta, 0.0)
	_blink_in -= delta
	if _blink_in <= 0.0:
		_blink_in = randf_range(BLINK_GAP.x, BLINK_GAP.y)
		if face == CALM or face == SET:
			_blink_left = BLINK_TIME
	_draw_face(face, delta)
	if _melting:
		_melt = minf(_melt + delta / MELT_TIME, 1.0)
		_draw_melt(delta)
	_pop = minf(_pop + delta / POP_TIME, 1.0)
	# Popping back grows the body from the floor.
	scale = Vector3.ONE * _ease_out(_pop) * BodyShape.SIZE
	visible = shown and not is_melted()


# Shows the body going as it is knocked out (_melt, 0 to 1), `delta`
# seconds on.
func _draw_melt(_delta: float) -> void:
	pass


# Shows `face` (see expression), with the eyes blinking, glancing and
# following the gaze, `delta` seconds on.
func _draw_face(_face: String, _delta: float) -> void:
	pass


static func _ease_out(t: float) -> float:
	return 1.0 - (1.0 - t) * (1.0 - t)


# Builds what is drawn.
func _build() -> void:
	pass


static func _flat(colour: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = colour
	material.roughness = 1.0
	# Drawn-on marks are flat, and seen from either side.
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material

