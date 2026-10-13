extends CharacterBody3D
## One player's character. The node is named after the owning peer's id, and
## that peer has authority over it: it simulates movement locally and the
## MultiplayerSynchronizer replicates the result to everyone else.
##
## The owner also fires its weapon here, showing the shot at once and telling
## the host (see Match), which decides what it hit. Health and whether the
## player is in play are the host's to decide, and arrive with the match state.

## A shot this player fired, for effects on this machine: where it went once
## the weapon had kicked, how the player stood (see WeaponStats.AIMED) and
## which shot of a run of fire it was.
signal fired(origin: Vector3, direction: Vector3, seed: int, stance: int, run: int)
## Another player's update arrived at `time` (this machine's clock, seconds)
## placing them at `at`, after being moved at once `teleports` times.
signal received(time: float, at: Vector3, teleports: int, facing: Vector2)
## Another player was moved at once to somewhere new, as on coming back.
signal teleported

const MOUSE_SENSITIVITY := 0.0025
const STICK_LOOK_SPEED := 2.8
const TOUCH_LOOK_SENSITIVITY := 0.006
const TOUCH_STICK_RADIUS := 80.0
const TOUCH_TAP_MSEC := 200
const PITCH_LIMIT := deg_to_rad(85.0)

@onready var head: Node3D = $Head
@onready var camera: Camera3D = $Head/Camera3D
## How much the view is shaking, from hits and nearby bursts.
var shake := Shake.new()
@onready var body: Node3D = $Body
@onready var gun: Node3D = $Head/Gun
## Where this player's shots leave from: the muzzle of the weapon in hand.
var muzzle: Node3D
@onready var name_tag: Label3D = $NameTag
@onready var collider: CollisionShape3D = $CollisionShape3D

# Touch controls: the left half of the screen is a movement stick, the right
# half drags to look and taps to jump, and on-screen buttons fire and reload.
var _move_touch := -1
var _move_touch_origin := Vector2.ZERO
var _move_touch_vector := Vector2.ZERO
var _look_touch := -1
var _look_touch_start_msec := 0
var _touch_jump := false
# Touch index -> name of the on-screen button it is holding.
var _button_touches := {}
var _mine := false

## Where this player starts, chosen by the host and set before the node enters
## the tree.
var spawn_position := Vector3.ZERO
## The world's Match node and the world, set with spawn_position.
var match_: Node
var world: Node3D

## Seconds a weapon takes to come up after switching to it.
const SWITCH_TIME := 0.35
## Seconds to bring the sights up to the eye, or back down to the hip.
const AIM_TIME := 0.14
## How much slower the view turns when aiming down the sights.
const AIM_LOOK_FACTOR := 0.6
## How quickly a shove from a burst dies away, on the ground and in the air.
const KNOCKBACK_GROUND_DECAY := 6.0
const KNOCKBACK_AIR_DECAY := 0.8

## Where this player is and which way they face, as their owner sends it to
## everyone: [x, y, z, yaw, pitch, teleports, pose]. `teleports` counts the
## times the player has been moved at once, as on coming back into play, so
## that the move is shown at once rather than smoothed. `pose` is what they
## are doing that others should see, as bits (POSE_AIMING).
var net_state := PackedFloat32Array([0, 0, 0, 0, 0, 0, 0]):
	set(value):
		net_state = value
		if not _mine and not driven and is_inside_tree():
			_receive(value)

## The bits of a pose: aiming down the sights, and reloading.
const POSE_AIMING := 1
const POSE_RELOADING := 2
const POSE_CROUCHING := 4
const POSE_LEAN_LEFT := 8
const POSE_LEAN_RIGHT := 16
const POSE_SLIDING := 32
# What another player is doing, as their owner last sent it.
var _pose := 0

## Others' movement, drawn smoothly a moment behind (see InterpolationBuffer).
var smoothing := InterpolationBuffer.new()
var _teleports := 0

## The weapon in this player's hands. Only the owner fires it.
var weapon := WeaponState.new(Weapons.find(Weapons.STARTING))
## Every weapon this player carries, by id. Kept by the owner, who learns from
## the host which they have.
var arsenal := {Weapons.STARTING: weapon}
## Whether the host has this player in play, as opposed to knocked out.
var alive := true

var _mouse_fire_held := false
# The sideways part of a shove from a burst, which dies away; the owner's own
# movement is added to it.
var _knockback := Vector3.ZERO
# A weapon change asked for since the last physics frame: a step through the
# carried weapons, or a weapon by id.
var _switch_step := 0
var _switch_to := ""
var _shown_weapon := ""
var _base_colour := Color.WHITE
# For animating others: where this player was last frame, and how fast they
# have been moving across the ground, smoothed.
var _last_position := Vector3.ZERO
var _ground_speed := 0.0
# Recoil, from 1 just after a shot to 0 at rest, and where the weapon rests.
var _kick := 0.0
var _gun_rest := Vector3.ZERO
var _bob_phase := 0.0
## Metres walked since the last footstep sound.
const STEP_LENGTH := 1.7
var _since_step := 0.0
var _team := -2
# The health the host last gave for this player, to notice them being hurt.
var _health := PlayerRecord.MAX_HEALTH
var _kills := 0
# The soft blob under this player on graphics too low for real shadows.
var _blob: MeshInstance3D
## How far below a player the ground is looked for, to lay their blob on.
const BLOB_REACH := 30.0
## How near another player must be for a character's eyes to turn to them.
const GLANCE_RANGE := 14.0
var _badge: MeshInstance3D
# Presses noted as their events arrive, so that a click or tap that begins and
# ends between two physics frames isn't missed.
var _fire_pressed := false
var _reload_pressed := false
var _jump_pressed := false
var _overload_pressed := false
var _melee_pressed := false
var _alt_pressed := false
## The stats of the shot this player last fired: their weapon's, or those
## of its other way of firing.
var shot_stats: WeaponStats
# The emote key: whether it is down, whether an emote has been picked by
# number while it was, and the emote it makes next when it isn't.
var _emote_held := false
var _emote_picked := false
var _next_emote := Emotes.WAVE
# How far down this player is crouched, 0 to 1, and whether the touch
# screen's crouch button, which stays on until pressed again, is on.
var _crouch := 0.0
var _touch_crouch := false
# How far this player leans: -1 to their left, 1 to their right.
var _lean := 0.0
# Run, crouch and aim as turned on by a press, for players who have chosen
# to press once rather than hold; and running by the touch stick's edge.
var _toggled := {"sprint": false, "crouch": false, "aim": false}
var _touch_run := false
# The player's own speed across the ground, which off the ground can only
# be steered.
var _walk := Vector2.ZERO
# The speed across the ground a jump pad threw the player with, which lasts
# until they land: steering in the air is added to it.
var _thrown := Vector2.ZERO
# A slide: seconds of it left, the speed and way it began with, and seconds
# until another can begin; and whether crouch was wanted last frame, to tell
# when it is first pressed.
var _slide_left := 0.0
var _slide_from := 0.0
var _slide_way := Vector2.ZERO
var _slide_rest := 0.0
var _crouch_was_wanted := false
# Pulling up onto a ledge: seconds into it (below zero when not), and where
# from and to.
var _climbing := -1.0
var _climb_from := Vector3.ZERO
var _climb_to := Vector3.ZERO
# Seconds a grenade has been held with its fuse burning, or below zero when
# none is in hand; and whether a touch has asked for one to be thrown at once.
var _cooking := -1.0
var _grenade_tapped := false
# The line drawn through the air while a grenade is held, showing its throw.
var _arc: MeshInstance3D
## How far above where the player looks a grenade is thrown, in radians.
const THROW_LIFT := 0.14
# Seconds since this player last struck with their knife, and the knife as
# it is seen while it swings.
var _since_strike := INF
var _knife: Node3D
# Seconds of flinching left after being hit, which slows this player.
var _flinch_left := 0.0
# How far the sights are up: 0 at the hip, 1 at the eye.
var _aim := 0.0
# Whether the touch screen's aim button, which stays on until pressed again,
# is on.
var _touch_aim := false
# Seconds left of the weapon in hand coming up, after a change of weapon.
var _switch_left := 0.0
# The breath a player holds to steady a scope, and how far the view is
# off from swaying through it, as (to the right, upward) in radians.
var breath := Breath.new()
var _scope_sway := Vector2.ZERO
# Which shot of a run of fire the last was (-1 before any), how long ago, and
# how far the view is thrown off by the weapon's kick, as Recoil.offset gives it.
var _run := -1
var _since_shot := INF
var _recoil_view := Vector2.ZERO
# For the sway of the weapon in view (see WeaponSway): how far the view has
# turned since the last frame and how fast it is turning, smoothed; how hard
# the last landing was, dying away; and how far into a run the weapon is.
var _looked := Vector2.ZERO
var _turn_rate := Vector2.ZERO
var _landing := 0.0
var _running := 0.0
var _was_on_floor := true
# Under the name tag: the player's level and title.
var _rank_tag: Label3D
var _shown_finish := "plain"
var _from_outside := false
# When this character last made a voice, by a clock of their own, and what
# they were last seen doing, for one played on another machine.
var _voice_clock := 0.0
var _last_voice := -INF
var _seen_airborne := false
var _seen_reloading := false
var _fall_speed := 0.0
# How far the weapon is lowered for a reload, and its magazine out, 0 to 1.
var _reload_lowered := 0.0
var _magazine: Node3D
# The lens of a scoped weapon in hand, which glints when it is looked through.
var _glint: Node3D
var _magazine_rest := Vector3.ZERO
## How fast the view climbs with a kick, and settles after a run of fire.
const RECOIL_RISE := 30.0
const RECOIL_SETTLE := 7.0


## Whether this is a computer player (see Bots): the host's to move, and
## nobody's to see through.
var is_bot := false
## Whether this machine moves this player some other way than by its
## controls: the host, for a bot.
var driven := false


func _enter_tree() -> void:
	is_bot = Bots.is_bot(name.to_int())
	set_multiplayer_authority(1 if is_bot else name.to_int())


# Whether this is this machine's own player: the one it controls and sees
# through.
func _is_own() -> bool:
	return is_multiplayer_authority() and not is_bot


## Where the weapon sits in this player's own view, and how big: smaller and
## further out than in their hand as others see it, so it doesn't fill the screen.
const VIEW_GUN_POSITION := Vector3(0.2, -0.19, -0.55)
## Where others see a player hold a weapon they are aiming: up at the eye, in
## the character's measurements from the head.
const SEEN_AIM_GUN_POSITION := Vector3(0.14, -0.05, -0.5)
## How far in front of the eye the rear sight is held when aiming.
const AIM_EYE_RELIEF := 0.32
const VIEW_GUN_SCALE := 0.6
## How high the team badge floats, in the character's measurements.
const BADGE_HEIGHT := 2.5


func _ready() -> void:
	_grow()
	if _is_own():
		gun.position = VIEW_GUN_POSITION
		gun.scale = Vector3.ONE * VIEW_GUN_SCALE
	_gun_rest = gun.position
	_show_team(MatchState.NO_TEAM)

	_mine = _is_own()
	driven = is_bot and multiplayer.is_server()
	if driven:
		add_child(preload("res://bots/bot_driver.gd").new(self, match_.state.settings.bot_skill if match_ != null and match_.has_state() else BotBrain.NORMAL))
	set_physics_process(_mine)
	set_process_input(_mine)
	set_process_unhandled_input(_mine)
	body.shown = not _mine
	name_tag.visible = not _mine
	_rank_tag = Label3D.new()
	_rank_tag.name = "RankTag"
	_rank_tag.billboard = name_tag.billboard
	_rank_tag.no_depth_test = name_tag.no_depth_test
	_rank_tag.fixed_size = name_tag.fixed_size
	_rank_tag.pixel_size = name_tag.pixel_size
	_rank_tag.font_size = int(name_tag.font_size * 0.6)
	_rank_tag.outline_size = int(name_tag.outline_size * 0.6)
	_rank_tag.visible = false
	add_child(_rank_tag)
	_rank_tag.position = name_tag.position - Vector3.UP * 0.085 * BodyShape.SIZE
	# Everyone places a new character at its spot: until its owner's first
	# update arrives, it would otherwise stand in the middle of the arena, and
	# could collect what lies there.
	global_position = spawn_position
	_last_position = spawn_position
	_face_middle()
	_publish()
	if _mine:
		camera.make_current()
		camera.fov = Settings.get_value("fov")
	_show_weapon(Weapons.STARTING)
	if match_ != null and _mine:
		match_.hurt.connect(_on_hurt)
		match_.projectile_burst.connect(_on_burst)
	if match_ != null:
		match_.state_changed.connect(_on_match_state_changed)
		_on_match_state_changed()
	if _mine:
		Settings.changed.connect(_on_setting_changed)


func _on_hurt(_attacker: int, _from: Vector3, dealt: int) -> void:
	shake.add(Shake.from_damage(dealt))
	_flinch_left = Movement.FLINCH_TIME


func _on_burst(_id: int, weapon_id: String, at: Vector3) -> void:
	var stats := Weapons.find(weapon_id)
	if stats != null:
		shake.add(Shake.from_burst(global_position.distance_to(at), stats.splash_radius * 2.0))


# A change of look in the settings reaches everyone at once.
func _on_setting_changed(key: String) -> void:
	if Looks.SETTINGS.has(key) and match_ != null:
		match_.wear(Looks.chosen())
	if key.begins_with("loadout_") and match_ != null:
		var loadout := Weapons.chosen_loadout()
		match_.choose_loadout(loadout[0], loadout[1], loadout[2])
		match_.choose_overload(Overload.from_id(Settings.get_value("loadout_overload")))


# The weapon to fall back on when the one in hand runs dry: one that never
# does, which every loadout has in its sidearm.
func _fallback_weapon() -> String:
	for id: String in arsenal:
		if arsenal[id].stats.unlimited():
			return id
	return weapon.stats.id


## The host's record of this player, or null before it has arrived.
func record() -> PlayerRecord:
	return match_.state.player(name.to_int()) if match_ != null else null


## Puts this player, who has just come back into play, at `at`.
func respawn_at(at: Vector3) -> void:
	global_position = at
	_teleports += 1
	velocity = Vector3.ZERO
	head.rotation.x = 0.0
	_knockback = Vector3.ZERO
	_flinch_left = 0.0
	_face_middle()
	for carried: WeaponState in arsenal.values():
		carried.refill()
	if driven:
		get_node("BotDriver").refill()


## The host put this player back where they last were, as their movement
## couldn't have happened.
func put_back(at: Vector3) -> void:
	global_position = at
	velocity = Vector3.ZERO
	_knockback = Vector3.ZERO
	_teleports += 1
	_publish()


## A burst shoved this player.
func push(impulse: Vector3) -> void:
	velocity.y += impulse.y
	_knockback += Vector3(impulse.x, 0.0, impulse.z)


## Takes the weapon `id` in hand, if carried. The owner tells the host, so that
## others see it.
func equip(id: String) -> void:
	if not arsenal.has(id) or weapon.stats.id == id:
		return
	weapon.reload_left = 0.0
	weapon.burst_left = 0
	weapon = arsenal[id]
	_run = -1
	weapon.cooldown = maxf(weapon.cooldown, weapon.stats.draw_time)
	_show_weapon(id)
	if _mine and match_ != null:
		match_.select_weapon(id)


func _show_weapon(id: String) -> void:
	if id == _shown_weapon:
		return
	# The first weapon is simply there; one changed to comes up from below.
	if not _shown_weapon.is_empty():
		_switch_left = draw_time(id)
	_shown_weapon = id
	for child in gun.get_children():
		child.queue_free()
	var model := WeaponModels.build(id)
	# In the finish its holder chose.
	var holder := record()
	_shown_finish = Looks.unpack(holder.look).finish if holder != null else Unlocks.FINISHES[0]
	Unlocks.paint(model, _shown_finish)
	# Only its holder sees both hands and the arms behind them.
	WeaponModels.show_own_view(model, _is_own())
	gun.add_child(model)
	muzzle = model.get_node("Muzzle")
	_glint = model.get_node_or_null("Glint")
	_magazine = model.get_node_or_null("Magazine")
	_magazine_rest = _magazine.position if _magazine != null else Vector3.ZERO


func _face_middle() -> void:
	# Start facing the middle of the arena, not the wall behind the spot.
	rotation.y = atan2(global_position.x, global_position.z)


func _on_match_state_changed() -> void:
	# Not for one that has left and is on its way out.
	if not is_inside_tree():
		return
	var own := record()
	name_tag.text = match_.state.name_of(name.to_int())
	_set_alive(own == null or own.alive)
	_show_team(own.team if own != null else MatchState.NO_TEAM)
	_show_overload(own)
	_show_power(own)
	if own == null:
		return
	# Everyone sees a player who has just been hurt wince, and one who has
	# just knocked someone out cheer.
	if own.alive and own.health < _health:
		body.grimace()
		voice("hurt")
	_health = own.health
	if own.kills > _kills:
		body.cheer()
	_kills = own.kills
	body.wear(own.look)
	# A finish chosen since the weapon was taken in hand is put on it.
	if Looks.unpack(own.look).finish != _shown_finish and not _shown_weapon.is_empty():
		var held := _shown_weapon
		_shown_weapon = ""
		_show_weapon(held)
		_switch_left = 0.0
	# Under their name, their level and title.
	if _rank_tag != null:
		_rank_tag.text = Looks.rank_text(own.look)
		_rank_tag.visible = name_tag.visible and not _rank_tag.text.is_empty()
		_rank_tag.modulate = name_tag.modulate
	if _mine:
		_carry(own.weapons)
	else:
		_show_weapon(own.weapon)


# Brings the owner's weapons in line with what the host says they carry: a new
# one is taken in hand at once, and a lost one is put away.
func _carry(carried: Array) -> void:
	var picked_up := ""
	var own_now := record()
	for id: String in carried:
		# As the host has it, with whatever is fitted.
		var stats := own_now.stats(id) if own_now != null else Weapons.find(id)
		if stats == null:
			continue
		if not arsenal.has(id):
			arsenal[id] = WeaponState.new(stats)
			picked_up = id
		elif arsenal[id].stats != stats:
			# The same weapon, fitted differently: a fresh one.
			var was_held: bool = weapon == arsenal[id]
			arsenal[id] = WeaponState.new(stats)
			if was_held:
				weapon = arsenal[id]
	for id: String in arsenal.keys():
		if not carried.has(id):
			arsenal.erase(id)
	# A weapon found on a pad, or the one the host says is in hand, is taken
	# up at once; one picked up from where it was dropped is only carried.
	var own_record := record()
	if not picked_up.is_empty() and (Weapons.find(picked_up).slot == Weapons.POWER or (own_record != null and own_record.weapon == picked_up)):
		equip(picked_up)
	elif not arsenal.has(weapon.stats.id) and not arsenal.is_empty():
		# What was in hand is gone: take up what the host says is in hand, or
		# failing that the last thing carried.
		var own := record()
		var held: String = own.weapon if own != null and arsenal.has(own.weapon) else arsenal.keys()[arsenal.size() - 1]
		weapon = arsenal[held]
		_show_weapon(held)


# A ring over the head of a player with a power-up, in its colour; and,
# to a player who has Sight, everyone else marked through the walls.
var _halo: MeshInstance3D
var _seen_mark: MeshInstance3D


func _show_power(own: PlayerRecord) -> void:
	var kind := own.power if own != null and own.alive and own.power_left > 0.0 else Powers.NONE
	if _halo == null:
		var ring := TorusMesh.new()
		ring.inner_radius = 0.22 * BodyShape.SIZE
		ring.outer_radius = 0.29 * BodyShape.SIZE
		ring.rings = 20
		ring.ring_segments = 8
		var bright := StandardMaterial3D.new()
		bright.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		_halo = Rounded.instance(ring, bright, name_tag.position - Vector3.UP * 0.22 * BodyShape.SIZE)
		_halo.name = "Halo"
		_halo.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(_halo)
		var blob := SphereMesh.new()
		blob.radius = 0.3 * BodyShape.SIZE
		blob.height = 0.6 * BodyShape.SIZE
		blob.radial_segments = 12
		blob.rings = 6
		var through := StandardMaterial3D.new()
		through.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		through.no_depth_test = true
		through.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		through.albedo_color = Color(Powers.COLOURS[Powers.SIGHT], 0.55)
		through.render_priority = 5
		_seen_mark = Rounded.instance(blob, through, BodyShape.WEDGE_CENTRE * BodyShape.SIZE)
		_seen_mark.name = "SeenMark"
		_seen_mark.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(_seen_mark)
	# Their own is over their head, out of their sight.
	_halo.visible = kind != Powers.NONE and not _mine
	if kind != Powers.NONE:
		(_halo.material_override as StandardMaterial3D).albedo_color = Powers.COLOURS[kind]
	_seen_mark.visible = not _mine and own != null and own.alive and match_ != null and Powers.has(match_.own_record(), Powers.SIGHT)


# An Overloaded player glows; a melted one sags and darkens.
func _show_overload(own: PlayerRecord) -> void:
	var glowing := own != null and own.overload_left > 0.0
	var melted := own != null and own.melted_for > 0.0
	body.set_glow(glowing)
	body.set_colour(_base_colour.darkened(0.35) if melted else _base_colour)
	body.set_dizzy(melted)


## A player's own colour when not on a team: spread round the colour wheel by id.
## The scene is laid out in the character's own measurements (BodyShape);
## everything about the player's size grows from them by BodyShape.SIZE here.
func _grow() -> void:
	var capsule := CapsuleShape3D.new()
	capsule.radius = BodyShape.RADIUS
	capsule.height = BodyShape.HEIGHT
	collider.shape = capsule
	collider.position = Vector3(0, BodyShape.HEIGHT / 2.0, 0)
	head.position = Vector3(0, BodyShape.HEAD_HEIGHT * BodyShape.SIZE, 0)
	gun.position *= BodyShape.SIZE
	gun.scale = Vector3.ONE * BodyShape.SIZE
	name_tag.position *= BodyShape.SIZE


static func colour_for(id: int) -> Color:
	# Never yellow, which is kept for cheese that can be eaten: the hues run
	# from green round through blue and red to orange.
	# A bot's id is below zero, and is coloured as the same number above it.
	return Color.from_hsv(fmod(0.22 + 0.86 * fmod(absi(id) * 0.618034, 1.0), 1.0), 0.65, 0.95)


# Team colours on the body and name, and a badge over the head whose shape
# also tells the teams apart: a wedge for Cheddar, a round wheel for Brie.
func _show_team(team: int) -> void:
	if team == _team:
		return
	_team = team
	_base_colour = TeamColors.of(team) if team != MatchState.NO_TEAM else colour_for(name.to_int())
	body.set_colour(_base_colour)
	name_tag.modulate = TeamColors.of(team)
	if _badge != null:
		_badge.queue_free()
		_badge = null
	if team == MatchState.NO_TEAM:
		return
	_badge = MeshInstance3D.new()
	_badge.name = "TeamBadge"
	var material := StandardMaterial3D.new()
	material.albedo_color = TeamColors.of(team)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	if team == MatchState.CHEDDAR:
		var wedge := PrismMesh.new()
		wedge.size = Vector3(0.3, 0.26, 0.08)
		wedge.material = material
		_badge.mesh = wedge
	else:
		var wheel := CylinderMesh.new()
		wheel.top_radius = 0.15
		wheel.bottom_radius = 0.15
		wheel.height = 0.08
		wheel.material = material
		_badge.mesh = wheel
		_badge.rotation.x = PI / 2.0
	_badge.position = Vector3(0, BADGE_HEIGHT * BodyShape.SIZE, 0)
	_badge.scale = Vector3.ONE * BodyShape.SIZE
	_badge.visible = not _mine and alive
	add_child(_badge)


func _set_alive(now_alive: bool) -> void:
	if now_alive == alive:
		return
	alive = now_alive
	collider.set_deferred("disabled", not alive)
	if not _mine:
		# The body melts away, and hides itself once melted.
		body.set_in_play(alive)
		name_tag.visible = alive
		if _badge != null:
			_badge.visible = alive
	gun.visible = alive
	if not alive:
		voice("knocked_out")
		_release_controls()
		if _mine:
			Sounds.play("knocked_out")


# What a player out of play does with their controls instead: steps
# through the players to watch, or takes to flying free.
func _watch_input(event: InputEvent) -> void:
	if world == null or world.get("watch_camera") == null or _menu_open():
		return
	if event is InputEventMouseButton and event.pressed and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		if event.button_index == MOUSE_BUTTON_LEFT:
			world.watch_camera.step(1)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			world.watch_camera.step(-1)
	elif event.is_action_pressed("weapon_next") or event.is_action_pressed("fire"):
		world.watch_camera.step(1)
	elif event.is_action_pressed("weapon_previous"):
		world.watch_camera.step(-1)
	elif event.is_action_pressed("jump"):
		world.watch_camera.toggle_free()


func _input(event: InputEvent) -> void:
	# While something is being typed, as in the chat, the keys are letters:
	# none of them plays.
	if event is InputEventKey and typing():
		return
	# A vote, in whatever is being voted on.
	if match_ != null and match_.has_state() and match_.state.ballot != null:
		for option in 3:
			if event.is_action_pressed("vote_%d" % (option + 1)) and option < match_.state.ballot.options():
				match_.cast(option)
				return
	if not alive:
		_watch_input(event)
	for action: String in _toggled:
		if event.is_action_pressed(action):
			_toggled[action] = not _toggled[action]
	# Before the rest, as the mouse wheel's events are mouse buttons.
	if event.is_action_pressed("weapon_next"):
		_switch_step += 1
		return
	if event.is_action_pressed("weapon_previous"):
		_switch_step -= 1
		return
	if event.is_action_pressed("alt_fire"):
		_alt_pressed = true
		return
	if event is InputEventScreenTouch:
		_handle_touch(event)
	elif event is InputEventScreenDrag:
		_handle_drag(event)
	elif event.device == InputEvent.DEVICE_ID_EMULATION:
		return  # Mouse events synthesized from touches; handled above.
	elif event is InputEventMouseButton:
		var captured := Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
		if event.button_index == MOUSE_BUTTON_LEFT and not event.pressed:
			_mouse_fire_held = false
		elif event.pressed and captured and event.button_index == MOUSE_BUTTON_LEFT:
			_mouse_fire_held = true
			_fire_pressed = true
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		# A web build takes the movement from the browser instead; see WebPage.
		if not OS.has_feature("web"):
			mouse_look(event.relative)
	elif event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		_mouse_fire_held = false
	elif event.is_action_pressed("reload"):
		_reload_pressed = true
	elif event.is_action_pressed("overload"):
		_overload_pressed = true
	elif event.is_action_pressed("melee"):
		_melee_pressed = true
	elif event.is_action_pressed("ping"):
		ping()
	elif event.is_action_pressed("emote"):
		_emote_held = true
		_emote_picked = false
	elif event.is_action_released("emote"):
		# Let go without picking one: the next in turn.
		if _emote_held and not _emote_picked:
			_make_emote(_next_emote)
		_emote_held = false
	elif _emote_held and _number_key(event) != -1:
		if _number_key(event) < Emotes.KINDS.size():
			_make_emote(Emotes.KINDS[_number_key(event)])
		_emote_picked = true
	elif event.is_action_pressed("grenade"):
		_start_grenade()
	elif _number_key(event) != -1:
		_switch_to = Weapons.at_key(arsenal.keys(), _number_key(event))
	elif event.is_action_pressed("jump"):
		_jump_pressed = true
	elif event.is_action_pressed("fire"):
		_fire_pressed = true


# Recoil and, for this machine's own player, the weapon bobbing with each
# step; for everyone else's, their body walking.
func _animate(delta: float) -> void:
	_kick = maxf(_kick - delta * 8.0, 0.0)
	_switch_left = maxf(_switch_left - delta, 0.0)
	var rest := _gun_rest
	var bob := Vector3.ZERO
	var sway_turn := Vector3.ZERO
	var reloading := 0.0
	var magazine_out := 0.0
	if _mine:
		var sights := smoothstep(0.0, 1.0, _aim)
		rest = _gun_rest.lerp(aim_gun_position(), sights)
		var across := Vector2(velocity.x, velocity.z).length()
		# Held steadier at the eye.
		var steady := 1.0 - 0.8 * sights
		if is_on_floor() and across > 0.5:
			_bob_phase = fmod(_bob_phase + across * delta * 1.8, TAU)
			bob = Vector3(sin(_bob_phase) * 0.012, absf(cos(_bob_phase)) * 0.012, 0.0) * steady
			_step(across * delta, Vector3.INF)
		if delta > 0.0:
			_turn_rate = _turn_rate.lerp(_looked / delta, minf(delta * 12.0, 1.0))
		_looked = Vector2.ZERO
		_landing = maxf(_landing - delta * 5.0, 0.0)
		var at_a_run := is_on_floor() and across > Movement.WALK_SPEED * 1.2 and _aim <= 0.0
		_running = move_toward(_running, 1.0 if at_a_run else 0.0, delta * 6.0)
		var sideways := (global_basis.inverse() * velocity).x / Movement.WALK_SPEED
		bob += WeaponSway.turning(_turn_rate) * steady + WeaponSway.landing(_landing) + WeaponSway.RUN_OFFSET * _running
		sway_turn = WeaponSway.RUN_TURN * _running + Vector3(0, 0, WeaponSway.strafing(sideways) * steady)
		var progress := 1.0 - weapon.reload_left / weapon.stats.reload_time if weapon.is_reloading() else 0.0
		reloading = WeaponSway.reload_lowering(progress)
		magazine_out = WeaponSway.magazine_out(progress)
	else:
		# Others are seen to lower their weapon for as long as they reload.
		_reload_lowered = move_toward(_reload_lowered, 1.0 if _pose & POSE_RELOADING and alive else 0.0, delta * 5.0)
		reloading = smoothstep(0.0, 1.0, _reload_lowered)
		magazine_out = reloading
		_crouch = move_toward(_crouch, 1.0 if _pose & POSE_CROUCHING and alive else 0.0, delta * Movement.CROUCH_RATE)
		body.set_crouch(smoothstep(0.0, 1.0, _crouch))
		var leaning := (1.0 if _pose & POSE_LEAN_RIGHT else 0.0) - (1.0 if _pose & POSE_LEAN_LEFT else 0.0)
		_lean = move_toward(_lean, leaning if alive else 0.0, delta * Movement.CROUCH_RATE)
		body.set_lean(_lean)
		# Others bring their weapon up to the eye when they aim.
		_aim = move_toward(_aim, 1.0 if _pose & POSE_AIMING and alive else 0.0, delta / AIM_TIME)
		rest = _gun_rest.lerp(SEEN_AIM_GUN_POSITION * BodyShape.SIZE, smoothstep(0.0, 1.0, _aim))
		var moved := global_position - _last_position
		_last_position = global_position
		if delta > 0.0:
			var speed := Vector2(moved.x, moved.z).length() / delta
			# Updates arrive in steps, so smooth the speed they give.
			_ground_speed = lerpf(_ground_speed, minf(speed, 12.0), minf(delta * 10.0, 1.0))
			var airborne := absf(moved.y) / delta > 0.5
			body.set_motion(_ground_speed, airborne, moved.y / delta, _pose & POSE_SLIDING != 0)
			body.set_holding(_pose & POSE_AIMING != 0, _pose & POSE_RELOADING != 0)
			# Heard leaving the ground, and beginning to reload.
			if alive and airborne and not _seen_airborne and moved.y > 0.0:
				voice("jump")
			_seen_airborne = airborne
			if alive and _pose & POSE_RELOADING != 0 and not _seen_reloading:
				voice("reload")
			_seen_reloading = _pose & POSE_RELOADING != 0
			if alive and not airborne and _ground_speed > 0.5:
				_step(_ground_speed * delta, global_position)
		body.set_gaze(head.rotation.x)
		body.set_glance(glance_toward(_nearest_other()))
	_animate_knife(delta)
	_lay_blob()
	# The eyes, and what is held at them, come down with a crouch and go out
	# to the side with a lean, tipping a little as they go.
	head.position = BodyShape.head_offset(smoothstep(0.0, 1.0, _crouch), _lean) * BodyShape.SIZE
	head.rotation.z = -BodyShape.LEAN_ANGLE * _lean * 0.5
	# A weapon just changed to swings up from below.
	var lowered := switch_lowering(_switch_left, draw_time(_shown_weapon))
	# The owner sees through a scope rather than the weapon it is on; others
	# see its lens catch the light.
	gun.visible = not (_mine and (is_scoped() or _from_outside))
	if _glint != null:
		_glint.visible = not _mine and alive and _aim >= 0.9
	gun.position = rest + bob + Vector3(0.0, 0.01, 0.07) * _kick + (Vector3(0.0, -0.5, 0.1) * lowered + Vector3(0.03, -0.16, 0.04) * reloading) * gun.scale.x
	gun.rotation = Vector3(0.18 * _kick - 1.1 * lowered + 0.4 * reloading, 0.0, 0.5 * lowered + 0.7 * reloading) + sway_turn
	if _magazine != null:
		_magazine.position = _magazine_rest + Vector3.DOWN * 0.18 * magazine_out


## Where the weapon in hand sits in this player's own view when they aim: its
## rear sight straight ahead of the eye, so that the sights line up on what
## the middle of the view is on.
func aim_gun_position() -> Vector3:
	var sight: Node3D = muzzle.get_parent().get_node_or_null("Sight") if muzzle != null else null
	var at := sight.position if sight != null else Vector3.ZERO
	return Vector3(0, 0, -AIM_EYE_RELIEF) - at * VIEW_GUN_SCALE
	if not _mine and muzzle != null:
		# Their weapon arm follows the hand on the weapon.
		body.reach_for(body.to_local(muzzle.get_parent().to_global(WeaponModels.GRIP)))


## Whether this player has a blob for a shadow just now: on graphics without
## real shadows, while in play.
func blob_wanted() -> bool:
	return alive and not GraphicsQuality.shadows_on()


## How big and how dark the blob under a player `height` metres above the
## ground is, as (scale, opacity): smaller and fainter the higher they are.
static func blob_look(height: float) -> Vector2:
	var near := 1.0 - clampf(height / 8.0, 0.0, 1.0)
	return Vector2(lerpf(0.55, 1.0, near), lerpf(0.1, 0.45, near))


# Lays the blob on the ground under this player, wherever that is.
func _lay_blob() -> void:
	if not blob_wanted() or not is_inside_tree():
		if _blob != null:
			_blob.visible = false
		return
	if _blob == null:
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(0, 0, 0, 0.45)
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		_blob = Rounded.spot(BodyShape.RADIUS * 0.85, material, Vector3.ZERO, Vector3.UP, 0.0)
		_blob.name = "BlobShadow"
		_blob.top_level = true
		add_child(_blob)
	var from := global_position + Vector3.UP * 0.5
	var ground := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, from + Vector3.DOWN * BLOB_REACH, Match.WORLD_MASK))
	_blob.visible = not ground.is_empty()
	if ground.is_empty():
		return
	var look := blob_look(global_position.y - ground.position.y)
	_blob.global_transform = Transform3D(Basis.from_scale(Vector3(look.x, 1.0, look.x)), ground.position + Vector3.UP * 0.03)
	(_blob.material_override as StandardMaterial3D).albedo_color.a = look.y


## Which way this player's eyes turn to look at something at `at`: -1 to
## their left, 1 to their right, and 0 for something too far away, behind
## them or dead ahead. Vector3.INF is nothing to look at.
func glance_toward(at: Vector3) -> float:
	if at == Vector3.INF or global_position.distance_to(at) > GLANCE_RANGE:
		return 0.0
	var to := global_basis.inverse() * (at - global_position)
	if to.z >= 0.0:
		return 0.0
	return clampf(to.x / maxf(-to.z, 0.5), -1.0, 1.0)


# Where the nearest other player in play is, or Vector3.INF.
func _nearest_other() -> Vector3:
	var nearest := Vector3.INF
	var parent := get_parent()
	if parent == null:
		return nearest
	for other in parent.get_children():
		if other == self or not other is CharacterBody3D or not other.alive:
			continue
		if nearest == Vector3.INF or global_position.distance_to(other.global_position) < global_position.distance_to(nearest):
			nearest = other.global_position
	return nearest


## How far down a weapon changed to `left` seconds short of `time` ago
## still is: 1 as the change begins, 0 once it is up.
static func switch_lowering(left: float, time := SWITCH_TIME) -> float:
	var share := clampf(left / maxf(time, 0.01), 0.0, 1.0)
	return share * share


## Seconds weapon `id` takes to come up.
static func draw_time(id: String) -> float:
	var stats := Weapons.find(id)
	return stats.draw_time if stats != null else SWITCH_TIME


## Whether this player is looking through a scope, which fills their view.
func is_scoped() -> bool:
	return alive and weapon.stats.scoped and is_aiming()


## Whether this player is aiming down the sights, so their shots go where
## they point rather than spreading as from the hip.
func is_aiming() -> bool:
	return _aim >= 0.9


## How far down this player is crouched: 0 standing, 1 crouched.
func crouch() -> float:
	return _crouch


# Whether `action` (run, crouch or aim) is wanted: held, or, for a player
# who has chosen that in the settings, pressed on and not yet off.
func _on(action: String) -> bool:
	var setting := "toggle_run" if action == "sprint" else "toggle_" + action
	return _toggled[action] if Settings.get_value(setting) else Input.is_action_pressed(action)


## How far this player leans: -1 to their left, 1 to their right.
func lean() -> float:
	return _lean


# The way this player wants to lean: -1, 0 or 1. Not at a run. A lean never
# reaches as far as the player is wide, so it can't put their eyes through
# anything they are standing against.
func _wants_lean() -> float:
	if not alive or Net.host_silent or _menu_open() or is_climbing() or (_wants_run() and _walk.length() > 0.5):
		return 0.0
	return signf(Input.get_axis("lean_left", "lean_right"))


func _wants_crouch() -> bool:
	if not alive or Net.host_silent or _menu_open():
		return false
	return _touch_crouch or _on("crouch")


func _wants_run() -> bool:
	return _touch_run or _on("sprint")


## How this player stands, as a shot fired now would report it.
func stance() -> int:
	var bits := WeaponStats.AIMED if is_aiming() else 0
	if _crouch >= 0.9:
		bits |= WeaponStats.CROUCHED
	if not is_on_floor():
		bits |= WeaponStats.AIRBORNE
	elif Vector2(velocity.x, velocity.z).length() > 1.0:
		bits |= WeaponStats.MOVING
	return bits


## Which shot of a run of fire the next would be: 0 for the first.
func next_in_run() -> int:
	return ShotRules.next_in_run(weapon.stats, _run, _since_shot)


## How wide a cone this player's next shot spreads over, in degrees.
func shot_spread() -> float:
	return weapon.stats.spread_for(stance(), next_in_run())


func _wants_aim() -> bool:
	if not alive or _switch_left > 0.0 or Net.host_silent or _menu_open():
		return false
	return _touch_aim or _on("aim")


# Counts off distance walked, and makes a footstep sound each stride: at
# `at` for someone else's, or without a place for this machine's own.
func _step(distance: float, at: Vector3) -> void:
	_since_step += distance
	if _since_step < STEP_LENGTH:
		return
	_since_step = 0.0
	if world != null:
		world.effects.footprint(global_position, rotation.y, world.map)
	# What it sounds like goes by what is trodden on, and how loud by how
	# they are moving and who they are to whoever is listening (see
	# Footsteps): a crouched player treads softly, and an opponent's steps
	# are the clearest.
	var sound := Footsteps.cue(Footsteps.surface_under(world.map if world != null else null, global_position))
	var running: bool = body.state() == CharacterMotion.RUN
	if at == Vector3.INF:
		Sounds.play(sound, Footsteps.level(Footsteps.OWN, running, _crouch), Sounds.varied(0.1))
	elif world != null:
		var who := Footsteps.relation(match_.own_record() if match_ != null and match_.has_state() else null, record(), match_ != null and match_.has_state() and match_.state.settings.is_teams())
		Sounds.play_at(sound, at, world, Footsteps.level(who, running, _crouch), Sounds.varied(0.1))


## A mouse movement measured in the page's pixels, as the browser reports it,
## in the game's own interface pixels, which is what the engine reports
## natively, so that the mouse feels the same either way. `view` is the size of
## the game's view in interface pixels, `window` the canvas in the screen's
## pixels and `pixel_ratio` the screen's pixels per page pixel.
static func page_to_view(movement: Vector2, view: Vector2, window: Vector2, pixel_ratio: float) -> Vector2:
	if window.x <= 0.0:
		return movement
	return movement * pixel_ratio * view.x / window.x


## The owner's current position and facing, ready to send.
func _publish() -> void:
	if _mine:
		var pose := POSE_AIMING if _aim > 0.5 else 0
		if _crouch > 0.5:
			pose |= POSE_CROUCHING
		if absf(_lean) > 0.5:
			pose |= POSE_LEAN_RIGHT if _lean > 0.0 else POSE_LEAN_LEFT
		if is_sliding():
			pose |= POSE_SLIDING
		if weapon.is_reloading():
			pose |= POSE_RELOADING
		net_state = PackedFloat32Array([global_position.x, global_position.y, global_position.z, rotation.y, head.rotation.x, _teleports, pose])


## Another player's update arrived.
func _receive(state: PackedFloat32Array) -> void:
	if state.size() < 6:
		return
	var now := Time.get_ticks_usec() / 1000000.0
	_pose = int(state[6]) if state.size() > 6 else 0
	if int(state[5]) != _teleports:
		# Moved at once: show it at once, without sliding there.
		_teleports = int(state[5])
		smoothing.clear()
		teleported.emit()
	smoothing.push(now, state)
	if smoothing.is_empty():
		return
	received.emit(now, Vector3(state[0], state[1], state[2]), int(state[5]), Vector2(state[3], state[4]))


## Draws another player where the smoothing says they were a moment ago.
func _show_smoothed() -> void:
	var shown := smoothing.sample(Time.get_ticks_usec() / 1000000.0 - InterpolationBuffer.DELAY)
	if shown.is_empty():
		return
	global_position = Vector3(shown[0], shown[1], shown[2])
	rotation.y = shown[3]
	head.rotation.x = shown[4]


## A shot just left this player's weapon: it kicks back.
func kick() -> void:
	_kick = 1.0
	body.fire_face()
	body.act(CharacterMotion.FIRE)


func _process(delta: float) -> void:
	_voice_clock += delta
	if not _mine and not driven:
		_show_smoothed()
	_animate(delta)
	if _mine:
		var tip := shake.tick(delta, Settings.get_value("screen_shake"))
		# The view climbs with the weapon's kick, and settles once the run of
		# fire is over.
		_since_shot += delta
		var firing := _run >= 0 and _since_shot <= weapon.stats.recoil_reset
		var kicked := Recoil.offset(weapon.stats, _run, is_aiming()) if firing else Vector2.ZERO
		_recoil_view = _recoil_view.lerp(kicked, minf(delta * (RECOIL_RISE if firing else RECOIL_SETTLE), 1.0))
		# The view moves with the body, as much as the player has asked for:
		# a bob with each stride, a roll into a sidestep, a dip on landing.
		var motion: float = Settings.get_value("camera_motion") * (1.0 - 0.7 * smoothstep(0.0, 1.0, _aim))
		var across := Vector2(velocity.x, velocity.z).length() if is_on_floor() else 0.0
		var sideways := (global_basis.inverse() * velocity).x / Movement.WALK_SPEED
		camera.position = (WeaponSway.view_bob(_bob_phase, across / Movement.WALK_SPEED) + Vector3.DOWN * WeaponSway.VIEW_DIP * _landing) * motion
		_aim = move_toward(_aim, 1.0 if _wants_aim() else 0.0, delta / AIM_TIME)
		# A scope sways, until the breath is held.
		breath.tick(delta, is_scoped() and _wants_run())
		_scope_sway = breath.sway() * smoothstep(0.0, 1.0, _aim) if weapon.stats.scoped else Vector2.ZERO
		camera.rotation = Vector3(tip.x + _recoil_view.y + _scope_sway.y, -_recoil_view.x - _scope_sway.x, tip.y - clampf(sideways, -1.0, 1.0) * WeaponSway.VIEW_ROLL * motion)
		_crouch = move_toward(_crouch, 1.0 if _wants_crouch() else 0.0, delta * Movement.CROUCH_RATE)
		_lean = move_toward(_lean, _wants_lean(), delta * Movement.CROUCH_RATE)
		var run_fov := lerpf(1.0, WeaponSway.RUN_FOV, _running * Settings.get_value("camera_motion"))
		camera.fov = Settings.get_value("fov") * lerpf(1.0, weapon.stats.aim_zoom, smoothstep(0.0, 1.0, _aim)) * run_fov
	if OS.has_feature("web") and _mine:
		var moved := WebPage.take_mouse_movement()
		if moved != Vector2.ZERO and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
			var view := get_viewport().get_visible_rect().size
			var window := Vector2(DisplayServer.window_get_size())
			mouse_look(page_to_view(moved, view, window, DisplayServer.screen_get_scale()))


## Whether the in-game menu, or anything else in its group such as the controls
## help, is open, so this player's controls are set aside.
## Shows this machine's own player as others see them, or goes back to
## their own view: for a camera that has come free of them (see PhotoMode).
func seen_from_outside(on: bool) -> void:
	_from_outside = on
	body.shown = on or not _mine


## Takes `weapon` in hand, if it is carried.
func choose_weapon(weapon: String) -> void:
	if arsenal.has(weapon):
		_switch_to = weapon


## Whether the keyboard is being typed on, in a box that takes text.
func typing() -> bool:
	return is_inside_tree() and get_viewport().gui_get_focus_owner() is LineEdit


func _menu_open() -> bool:
	# With the camera free for a picture, the player stands where they are.
	if world != null and world.get("photo_mode") != null and world.photo_mode.on:
		return true
	for menu in get_tree().get_nodes_in_group("pause_menu"):
		if menu is CanvasItem and menu.visible:
			return true
	return false


# Clicking the game takes the mouse. Handled after the interface has had the
# click, so that clicking a button, such as the menu's, doesn't.
func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.device != InputEvent.DEVICE_ID_EMULATION:
		# Not while waiting on the host or in the menu: the pointer is needed there.
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED and not Net.host_silent and not _menu_open():
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


# Which number key `event` presses, counting from 0, or -1.
static func _number_key(event: InputEvent) -> int:
	for index in Weapons.KEYS:
		if event.is_action_pressed("weapon_%d" % (index + 1)):
			return index
	return -1


# Asks the host to show everyone emote `kind`.
func _make_emote(kind: int) -> void:
	# Only the ones that have been earned.
	if not Unlocks.mine(Unlocks.EMOTE, kind):
		kind = Unlocks.next(Unlocks.EMOTE, kind, Progress.level(), Progress.achievements, Progress.challenges_done())
	if alive and match_ != null and not _menu_open():
		match_.emote(kind)
		_next_emote = Emotes.next(kind)


## Pings what this player is pointing at, for their teammates: an opponent,
## a pickup, or the place (see Pings). Returns what was pinged, or {} if
## they are pointing at nothing.
func ping() -> Dictionary:
	if not alive or match_ == null or world == null or not is_inside_tree():
		return {}
	var from := camera.global_position
	var query := PhysicsRayQueryParameters3D.create(from, from - camera.global_basis.z * Pings.REACH, 3, [get_rid()])
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	if hit.is_empty():
		return {}
	var other: Node = hit.collider
	var on_player: bool = other.get_parent() == world.players
	var on_opponent: bool = on_player and not match_.state.are_allies(name.to_int(), String(other.name).to_int())
	# A ping on a player is at their feet, wherever on them was pointed at.
	var at: Vector3 = other.global_position if on_player else hit.position
	var pads: Array = world.pickup_pads().map(func(pad: Dictionary) -> Vector3: return pad.position)
	var pinged := {"at": at, "kind": Pings.kind_at(at, on_opponent, pads)}
	match_.ping(pinged.at, pinged.kind)
	return pinged


## This character's own voice as they do `event`, such as "jump" or "hurt"
## (see Voices): heard by whoever should hear it, at the pitch that is
## theirs, and not too soon after their last.
func voice(event: String) -> void:
	var sound := Voices.cue(event)
	if sound.is_empty() or not Voices.ready(sound, _voice_clock - _last_voice) or not is_inside_tree():
		return
	var teams: bool = match_ != null and match_.has_state() and match_.state.settings.is_teams()
	var who := Footsteps.OWN if _mine else Footsteps.relation(match_.own_record() if match_ != null and match_.has_state() else null, record(), teams)
	if not Voices.heard(sound, who):
		return
	_last_voice = _voice_clock
	var pitch := Voices.pitch(name.to_int()) * Sounds.varied(0.03)
	if _mine:
		Sounds.play(sound, Voices.level(sound, who), pitch)
	elif world != null:
		Sounds.play_at(sound, head.global_position, world, Voices.level(sound, who), pitch)


## Everyone sees this player make emote `kind`.
func show_emote(kind: int) -> void:
	body.emote(kind)
	voice("emote")
	if _mine:
		Sounds.play("emote", -8.0)
	elif world != null:
		Sounds.play_at("emote", head.global_position, world, -2.0)


## This player is being knocked out by `weapon`, from `from`: everyone sees
## them thrown away from it, or blown to crumbs by a burst.
func fall(weapon: String, from: Vector3) -> void:
	var away := global_basis.inverse() * (global_position - from)
	body.knock(weapon, away)
	if world != null and not _mine:
		var scattered: bool = body.ending() == body.SCATTERED
		world.effects.crumbs(global_position + Vector3.UP * BodyShape.WEDGE_CENTRE.y * BodyShape.SIZE, body.colour(), 36 if scattered else 12, 9.0 if scattered else 4.0)




func _physics_process(delta: float) -> void:
	# Not until the arena the host chose is here, or the player could fall
	# through where its floor will be.
	if Net.host_silent or (world != null and not world.ready_to_play()):
		return
	weapon.tick(delta)
	if _menu_open():
		_release_controls()
		_jump_pressed = false
		_touch_jump = false
		_reload_pressed = false
		velocity.x = 0.0
		velocity.z = 0.0
		if not is_on_floor():
			velocity += get_gravity() * delta
		move_and_slide()
		return
	if not alive:
		_jump_pressed = false
		_touch_jump = false
		_reload_pressed = false
		_fire_pressed = false
		_melee_pressed = false
		_drop_grenade()
		return
	_look(Input.get_vector("look_left", "look_right", "look_up", "look_down") * STICK_LOOK_SPEED * Settings.get_value("stick_sensitivity") * delta)

	if _climbing >= 0.0:
		# Nothing else moves a player who is pulling themselves up.
		_climbing += delta
		global_position = Movement.climb_position(_climb_from, _climb_to, _climbing)
		velocity = Vector3.ZERO
		_walk = Vector2.ZERO
		if _climbing >= Movement.CLIMB_TIME:
			_climbing = -1.0
		_jump_pressed = false
		_touch_jump = false
		_publish()
		_use_weapon()
		return

	var fixtures: Dictionary = world.fixtures if world != null else {"pads": [], "ladders": []}
	var ladder := Fixtures.ladder_at(fixtures.ladders, global_position)
	var launch: Variant = Fixtures.launch_at(fixtures.pads, global_position) if is_on_floor() else null
	if launch != null:
		# Thrown by a jump pad, the way it throws everyone.
		velocity.y = launch.y
		_thrown = Vector2(launch.x, launch.z)
		_walk = Vector2.ZERO
		_slide_left = 0.0
		Sounds.play("pad", -6.0)
		_jump_pressed = false
		_touch_jump = false
		velocity.x = _thrown.x
		velocity.z = _thrown.y
		_fall_speed = 0.0
		move_and_slide()
		_was_on_floor = false
		_publish()
		_use_weapon()
		return
	if not ladder.is_empty() and not (_jump_pressed or _touch_jump):
		# On a ladder nothing falls: pushing toward it climbs, away from it
		# goes down, and letting go of the keys holds on.
		var push := Input.get_vector("move_left", "move_right", "move_forward", "move_back") + _move_touch_vector
		var toward := (transform.basis * Vector3(push.x, 0.0, push.y)).dot(ladder.facing)
		velocity.y = Fixtures.LADDER_SPEED * clampf(toward, -1.0, 1.0)
		if is_on_floor() and velocity.y < 0.0:
			velocity.y = 0.0
	elif not is_on_floor():
		velocity += get_gravity() * delta
	elif _jump_pressed or _touch_jump:
		velocity.y = Movement.JUMP_VELOCITY
		voice("jump")
		Sounds.play("jump", -10.0)
		# A jump ends a slide.
		_slide_left = 0.0
	_jump_pressed = false
	_touch_jump = false

	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back") + _move_touch_vector
	var direction := transform.basis * Vector3(input.x, 0.0, input.y).limit_length(1.0)
	_knockback *= exp(-(KNOCKBACK_GROUND_DECAY if is_on_floor() else KNOCKBACK_AIR_DECAY) * delta)
	_flinch_left = maxf(_flinch_left - delta, 0.0)
	var speed := Movement.speed(_wants_run(), _wants_aim(), _flinch_left, match_.state.speed_factor(record()) if match_ != null else 1.0, _wants_crouch()) * weapon.stats.carry_speed
	# Slower through whatever there is to wade through.
	if world != null:
		speed *= MovingParts.speed_factor(world.moving_parts.hazards, global_position)
	var wanted := Vector2(direction.x, direction.z) * speed
	# On the ground a player goes where they push; in the air they can only
	# steer what they left it with.
	_walk = wanted if is_on_floor() or not ladder.is_empty() else Movement.air_steer(_walk, wanted, delta)
	_slide(delta)
	if is_on_floor():
		_thrown = Vector2.ZERO
	velocity.x = _walk.x + _knockback.x + _thrown.x
	velocity.z = _walk.y + _knockback.z + _thrown.y
	_fall_speed = maxf(-velocity.y, 0.0)
	if _begin_climb(direction):
		_publish()
		_use_weapon()
		return
	_step_up(delta)
	move_and_slide()
	# Coming down onto the ground jolts the view and the weapon in it, and a
	# hard landing winds the player for a moment.
	if is_on_floor() and not _was_on_floor:
		_landing = WeaponSway.impact(_fall_speed)
		if world != null:
			world.effects.landing(global_position, _landing)
		if _landing > 0.2:
			Sounds.play("land", lerpf(-16.0, -4.0, _landing))
			# And what was landed on, lower than a step on it.
			Sounds.play(Footsteps.cue(Footsteps.surface_under(world.map if world != null else null, global_position)), lerpf(-14.0, -5.0, _landing), 0.75)
		if _landing >= Movement.HARD_LANDING:
			_flinch_left = maxf(_flinch_left, Movement.FLINCH_TIME * Movement.LANDING_WIND)
	_was_on_floor = is_on_floor()
	_publish()

	_use_weapon()


## Whether this player is sliding.
func is_sliding() -> bool:
	return _slide_left > 0.0


# Begins a slide when crouch is first pressed at a run, and while one lasts
# carries the player the way they were going.
func _slide(delta: float) -> void:
	var wants := _wants_crouch()
	var pressed := wants and not _crouch_was_wanted
	_crouch_was_wanted = wants
	_slide_rest = maxf(_slide_rest - delta, 0.0)
	# How they were going a moment ago: crouching has already cut what they
	# are asking for now.
	var going := Vector2(velocity.x, velocity.z)
	if pressed and is_on_floor() and _slide_rest <= 0.0 and not is_sliding() and Movement.can_slide(going.length()):
		_slide_from = going.length()
		_slide_way = going.normalized()
		_slide_left = Movement.SLIDE_TIME
		Sounds.play("slide", -8.0)
	if not is_sliding():
		return
	_slide_left -= delta
	if not wants or not is_on_floor() or _slide_left <= 0.0:
		_slide_left = 0.0
		_slide_rest = Movement.SLIDE_REST
		return
	_walk = _slide_way * Movement.slide_speed(_slide_from, Movement.SLIDE_TIME - _slide_left)


# Walks up a step in the player's way, if it is low enough: up, over it, and
# down onto it.
func _step_up(delta: float) -> void:
	var across := Vector3(velocity.x, 0.0, velocity.z) * delta
	if not is_on_floor() or across.length() < 0.001:
		return
	var from := global_transform
	if not test_move(from, across):
		return
	var rise := Vector3.UP * Movement.STEP_HEIGHT
	if test_move(from, rise):
		return
	var raised := from.translated(rise)
	# Far enough over it to stand on, not just to touch it.
	var onto := across.normalized() * maxf(across.length(), 0.15)
	if test_move(raised, onto):
		return
	var landing := KinematicCollision3D.new()
	if not test_move(raised.translated(onto), -rise, landing):
		return
	var top := raised.origin + onto + landing.get_travel()
	if top.y - from.origin.y < 0.03 or landing.get_normal().angle_to(Vector3.UP) > floor_max_angle:
		return
	global_position = top


## The top of a ledge in front of a player at `feet` facing `forward`
## (level, normalized), if there is one they could pull themselves onto:
## above stepping height, no higher than CLIMB_HEIGHT, with room to stand. Or
## null. `space` is the arena, and `shape` the player's own, standing with
## its bottom `lift` above their feet.
static func ledge_ahead(space: PhysicsDirectSpaceState3D, feet: Vector3, forward: Vector3, shape: Shape3D, lift: float) -> Variant:
	var reach := BodyShape.RADIUS + 0.5
	var above := feet + forward * reach + Vector3.UP * (Movement.CLIMB_HEIGHT + 0.2)
	var down := PhysicsRayQueryParameters3D.create(above, above + Vector3.DOWN * (Movement.CLIMB_HEIGHT + 0.2), Match.WORLD_MASK)
	var top := space.intersect_ray(down)
	if top.is_empty() or (top.normal as Vector3).angle_to(Vector3.UP) > 0.6:
		return null
	var height: float = top.position.y - feet.y
	if height <= Movement.STEP_HEIGHT or height > Movement.CLIMB_HEIGHT:
		return null
	var stand: Vector3 = top.position + Vector3.UP * 0.05
	var room := PhysicsShapeQueryParameters3D.new()
	room.shape = shape
	room.collision_mask = Match.WORLD_MASK
	# Room to stand on it, and to rise to it from here.
	for at: Vector3 in [stand, Vector3(feet.x, stand.y, feet.z)]:
		room.transform = Transform3D(Basis.IDENTITY, at + Vector3.UP * lift)
		if not space.intersect_shape(room, 1).is_empty():
			return null
	return stand


# Begins pulling up onto a ledge the player is pushing into, if there is one.
func _begin_climb(direction: Vector3) -> bool:
	if direction.length() < 0.5 or _wants_crouch() or velocity.y > 2.0:
		return false
	var forward := -global_basis.z
	# Pushing toward it, not along or away from it.
	if direction.normalized().dot(forward) < 0.7 or not test_move(global_transform, forward * 0.25):
		return false
	var ledge: Variant = ledge_ahead(get_world_3d().direct_space_state, global_position, forward, collider.shape, collider.position.y)
	if ledge == null:
		return false
	_climbing = 0.0
	_climb_from = global_position
	_climb_to = ledge
	_slide_left = 0.0
	Sounds.play("jump", -12.0, 0.8)
	return true


## Whether this player is pulling themselves up onto a ledge.
func is_climbing() -> bool:
	return _climbing >= 0.0


# Takes a grenade in hand, its fuse burning, if there is one to take.
func _start_grenade() -> void:
	var own := record()
	if alive and _cooking < 0.0 and own != null and own.grenades > 0 and not _menu_open():
		_cooking = 0.0


# Puts away a grenade that was in hand, unthrown.
func _drop_grenade() -> void:
	_cooking = -1.0
	_grenade_tapped = false
	if _arc != null:
		_arc.visible = false


## The way a grenade thrown now would leave the hand: a little above where
## the player looks.
func throw_direction() -> Vector3:
	return Recoil.apply(-head.global_basis.z, Vector2(0.0, THROW_LIFT))


# Holds a grenade for as long as its key is, showing where it would go, and
# throws it on letting go, or before its fuse runs out in the hand.
func _hold_grenade(delta: float) -> void:
	if _cooking < 0.0:
		return
	_cooking += delta
	var grenade := Weapons.find(Weapons.GRENADE)
	var must := _cooking >= grenade.fuse - Match.COOK_MARGIN
	if _grenade_tapped or must or not Input.is_action_pressed("grenade"):
		match_.throw_grenade(camera.global_position, throw_direction(), _cooking)
		_drop_grenade()
		return
	_show_arc(grenade)


## The points of a grenade's flight thrown from `from` along `direction`, up
## to its first bounce or `seconds` of flight, whichever comes first. `wall`
## is as Projectile.advance takes it.
static func throw_arc(from: Vector3, direction: Vector3, wall: Callable, seconds := 1.6) -> PackedVector3Array:
	var flight := Projectile.new(0, 0, Weapons.find(Weapons.GRENADE), from, direction)
	var points := PackedVector3Array([from])
	var step := 0.05
	for count in int(seconds / step):
		flight.advance(step, wall, {})
		points.append(flight.position)
		if flight.bounces > 0:
			break
	return points


func _show_arc(grenade: WeaponStats) -> void:
	if _arc == null:
		_arc = MeshInstance3D.new()
		_arc.name = "ThrowArc"
		_arc.mesh = ImmediateMesh.new()
		_arc.top_level = true
		_arc.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		var line := StandardMaterial3D.new()
		line.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		line.albedo_color = Color(1, 1, 1, 0.8)
		line.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		_arc.material_override = line
		add_child(_arc)
	_arc.visible = true
	_arc.global_transform = Transform3D.IDENTITY
	# From the hand rather than the eye, so the line doesn't cross the view.
	var from := camera.global_position + head.global_basis * Vector3(0.3, -0.3, -0.4)
	var points := throw_arc(from, throw_direction(), Match.wall_finder(get_world_3d().direct_space_state))
	var mesh: ImmediateMesh = _arc.mesh
	mesh.clear_surfaces()
	if points.size() < 2 or grenade == null:
		return
	mesh.surface_begin(Mesh.PRIMITIVE_LINE_STRIP)
	for point in points:
		mesh.surface_add_vertex(point)
	mesh.surface_end()


## The knife swings across the weapon hand's side of the view, for a strike
## by this player that everyone should see.
func swing_knife() -> void:
	_since_strike = 0.0
	body.act(CharacterMotion.MELEE)
	if _knife == null:
		_knife = _build_knife()
		head.add_child(_knife)
	if _mine:
		Sounds.play("knife", -6.0)
	elif world != null:
		Sounds.play_at("knife", head.global_position, world, -2.0)


# A cheese knife: a handle, and a blade with a forked tip.
static func _build_knife() -> Node3D:
	var knife := Node3D.new()
	knife.name = "Knife"
	var steel := StandardMaterial3D.new()
	steel.albedo_color = WeaponModels.STEEL
	var wood := StandardMaterial3D.new()
	wood.albedo_color = WeaponModels.WOOD
	knife.add_child(Rounded.instance(Rounded.box(Vector3(0.035, 0.035, 0.13), 0.012), wood, Vector3(0, 0, 0.06)))
	knife.add_child(Rounded.instance(Rounded.box(Vector3(0.012, 0.06, 0.26), 0.005), steel, Vector3(0, 0.005, -0.13)))
	for side in [-1.0, 1.0]:
		knife.add_child(Rounded.instance(Rounded.box(Vector3(0.012, 0.022, 0.05), 0.004), steel, Vector3(0, 0.005 + 0.019 * side, -0.28)))
	return knife


# Moves the knife through its swing, and puts it away at the end of it.
func _animate_knife(delta: float) -> void:
	_since_strike += delta
	if _knife == null:
		return
	var through := Melee.swing(_since_strike)
	_knife.visible = through < 1.0 and alive
	# From the weapon's side, low, across to the other side, as far out as the
	# weapon is held.
	var reach := gun.scale.x
	var sweep := lerpf(-1.0, 1.0, smoothstep(0.0, 1.0, through))
	_knife.position = Vector3(-0.22 * sweep, -0.16 + 0.05 * sin(through * PI), -0.5) * reach
	_knife.rotation = Vector3(0.0, 0.9 * sweep, -1.2)
	_knife.scale = Vector3.ONE * reach


func _use_weapon() -> void:
	_hold_grenade(get_physics_process_delta_time())
	if _overload_pressed:
		_overload_pressed = false
		match_.set_off_overload()
	if _melee_pressed:
		_melee_pressed = false
		if _since_strike >= Melee.INTERVAL:
			swing_knife()
			# A step into it.
			var forward := -global_basis.z
			_knockback += Vector3(forward.x, 0.0, forward.z) * Melee.LUNGE
			match_.strike(camera.global_position, -head.global_basis.z)
	if _switch_step != 0:
		equip(Weapons.cycle(arsenal.keys(), weapon.stats.id, signi(_switch_step)))
	elif not _switch_to.is_empty():
		equip(_switch_to)
	_switch_step = 0
	_switch_to = ""
	if _reload_pressed:
		_reload_pressed = false
		if weapon.start_reload():
			match_.reload(weapon.stats.id)
			Sounds.play("reload", -6.0)
	var held := Input.is_action_pressed("fire") or _mouse_fire_held or _button_touches.values().has("fire")
	# The rest of a burst follows its first shot without being asked.
	var wants := _fire_pressed or (held and weapon.stats.automatic) or weapon.in_burst()
	# A press waits for the weapon to be ready, unless it is empty.
	if _fire_pressed and (weapon.can_fire() or weapon.is_reloading()):
		_fire_pressed = false
	# A weapon with nothing left gives way to the sidearm.
	if wants and weapon.is_dry():
		_fire_pressed = false
		Sounds.play("dry", -6.0)
		equip(_fallback_weapon())
		return
	# The weapon's other way of firing, on its second trigger, if it has one.
	var other := Weapons.alt_of(weapon.stats) if _alt_pressed else null
	_alt_pressed = false
	if other != null and weapon.fire_other(other):
		_loose(other, stance() | WeaponStats.ALT)
	elif wants and weapon.fire():
		_loose(weapon.stats, stance())


# A shot has left the weapon in hand, with `stats` (its own, or those of its
# other way of firing): tells the host and shows it here.
func _loose(stats: WeaponStats, standing: int) -> void:
	var origin := camera.global_position
	# The head's facing, not the camera's, which may be shaking; but
	# through a scope, wherever it has swayed to.
	var direction := Recoil.apply(-head.global_basis.z, _scope_sway)
	var seed := randi()
	_run = next_in_run()
	_since_shot = 0.0
	shot_stats = stats
	# The host is told where the player aims, and adds the kick itself.
	match_.fire(origin, direction, seed, weapon.stats.id, standing)
	kick()
	var kicked := Recoil.apply(direction, Recoil.offset(stats, _run, standing & WeaponStats.AIMED != 0))
	fired.emit(origin, kicked, seed, standing, _run)
	# Firing the last round starts the reload by itself.
	if weapon.is_reloading():
		match_.reload(weapon.stats.id)


func _exit_tree() -> void:
	if _mine:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _release_controls() -> void:
	_mouse_fire_held = false
	_fire_pressed = false
	_touch_aim = false
	_touch_crouch = false
	_touch_run = false
	for action: String in _toggled:
		_toggled[action] = false
	_button_touches.clear()


## Turns by a mouse movement of `relative` interface pixels.
func mouse_look(relative: Vector2) -> void:
	_look(relative * MOUSE_SENSITIVITY * Settings.get_value("mouse_sensitivity"))


func _look(delta: Vector2) -> void:
	if Net.host_silent:
		return
	if Settings.get_value("invert_look"):
		delta.y = -delta.y
	# Slower at the eye, and slower still through a scope.
	delta *= lerpf(1.0, minf(AIM_LOOK_FACTOR, weapon.stats.aim_zoom * 1.2), _aim)
	_looked += Vector2(delta.x, -delta.y)
	rotate_y(-delta.x)
	head.rotation.x = clampf(head.rotation.x - delta.y, -PITCH_LIMIT, PITCH_LIMIT)


func _touch_controls() -> Node:
	return get_tree().get_first_node_in_group("touch_controls")


func _handle_touch(event: InputEventScreenTouch) -> void:
	var controls := _touch_controls()
	if controls != null:
		controls.notice_touch()
	# A touch on the menu button or in the menu is the interface's, not a move.
	if event.pressed and controls != null and controls.blocked_at(event.position):
		return
	if event.pressed:
		var button: String = controls.button_at(event.position) if controls != null else ""
		if not alive and world != null and world.get("watch_camera") != null:
			# Out of play, the buttons choose who to watch.
			if button == "fire":
				world.watch_camera.step(1)
			elif button == "aim":
				world.watch_camera.step(-1)
			elif button == "swap":
				world.watch_camera.toggle_free()
		if button == "reload":
			_reload_pressed = true
		elif button == "swap":
			_switch_step += 1
		elif button == "aim":
			_touch_aim = not _touch_aim
		elif button == "knife":
			_melee_pressed = true
		elif button == "alt":
			_alt_pressed = true
		elif button == "emote":
			_make_emote(_next_emote)
		elif button == "crouch":
			_touch_crouch = not _touch_crouch
		elif button == "grenade":
			_start_grenade()
			_grenade_tapped = true
		elif button == "overload":
			_overload_pressed = true
		elif not button.is_empty():
			_button_touches[event.index] = button
			_fire_pressed = _fire_pressed or button == "fire"
		elif preload("res://ui/touch_controls.gd").is_movement_side(event.position, get_viewport().get_visible_rect().size, Settings.get_value("touch_left_handed")):
			if _move_touch == -1:
				_move_touch = event.index
				_move_touch_origin = event.position
		elif _look_touch == -1:
			_look_touch = event.index
			_look_touch_start_msec = Time.get_ticks_msec()
	elif _button_touches.has(event.index):
		_button_touches.erase(event.index)
	elif event.index == _move_touch:
		_move_touch = -1
		_move_touch_vector = Vector2.ZERO
		_touch_run = false
	elif event.index == _look_touch:
		_look_touch = -1
		if Time.get_ticks_msec() - _look_touch_start_msec < TOUCH_TAP_MSEC:
			_touch_jump = true
	if controls != null:
		controls.held = _button_touches.values() + (["aim"] if _touch_aim else []) + (["crouch"] if _touch_crouch else [])


func _handle_drag(event: InputEventScreenDrag) -> void:
	if event.index == _move_touch:
		var pushed := (event.position - _move_touch_origin) / TOUCH_STICK_RADIUS
		_move_touch_vector = pushed.limit_length(1.0)
		_touch_run = pushed.length() >= Movement.TOUCH_RUN_REACH
	elif event.index == _look_touch:
		_look(event.relative * TOUCH_LOOK_SENSITIVITY * Settings.get_value("touch_sensitivity"))
