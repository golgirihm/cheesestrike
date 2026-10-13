extends Node
## Moves one computer player, on the host's machine: looks at the arena for
## its BotBrain, walks its character where the brain says, fires for it
## through the same checks a real player's shots go through, and tells
## everyone else where it is the way a real player's machine would.
##
## A child of the bot's player node. Cheap on purpose: a few rays a frame and
## no pathfinding, as the host may be a phone.

const WORLD_MASK := 1
## How high off the ground it looks for things in its way.
const KNEE := 0.8
## How far it will knife from.
const KNIFE_REACH := Melee.REACH * 0.8

var player: CharacterBody3D
var brain: BotBrain
## The weapons it carries, by id, as it keeps count of them.
var weapons := {}
## Microseconds the last think took, for the soak test.
var last_cost := 0
var _gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8)
var _fire_wait := 0.0


func _init(of: CharacterBody3D, skill: String) -> void:
	player = of
	name = "BotDriver"
	brain = BotBrain.new(skill, hash(of.name))


func _physics_process(delta: float) -> void:
	var match_: Match = player.match_
	var world: Node3D = player.world
	if match_ == null or world == null or not match_.is_host() or not match_.has_state():
		return
	var id := player.name.to_int()
	var record := match_.state.player(id)
	if record == null:
		return
	if not record.alive or not match_.state.is_playing():
		player.velocity = Vector3.ZERO
		_publish(match_, id, 0)
		return
	var began := Time.get_ticks_usec()
	var weapon := _weapon(record)
	weapon.tick(delta)
	_fire_wait = maxf(_fire_wait - delta, 0.0)

	var space := player.get_world_3d().direct_space_state
	var at := player.global_position
	var eye: Vector3 = player.head.global_position
	var enemies := []
	for other: Node3D in world.players.get_children():
		var theirs := match_.state.player(other.name.to_int())
		if other == player or theirs == null or not theirs.alive or other.is_queued_for_deletion() or match_.state.are_allies(id, theirs.id):
			continue
		var middle: Vector3 = other.global_position + Vector3.UP * BodyShape.WEDGE_CENTRE.y * BodyShape.SIZE
		var seen := space.intersect_ray(PhysicsRayQueryParameters3D.create(eye, middle, WORLD_MASK)).is_empty()
		enemies.append({"id": theirs.id, "at": other.global_position, "visible": seen})
	var me := {"at": at, "health": record.health, "ammo": weapon.ammo, "magazine": weapon.stats.magazine, "reloading": weapon.is_reloading(),
		"range": weapon.stats.max_range if not weapon.stats.fires_projectile() else 30.0}
	var blocked := func(from: Vector3, to: Vector3) -> bool:
		var lift := Vector3.UP * KNEE
		return not space.intersect_ray(PhysicsRayQueryParameters3D.create(from + lift, to + lift, WORLD_MASK)).is_empty()
	var orders := brain.think(delta, me, enemies, _places(match_, world, record), blocked)

	# Turn toward what it aims at, or the way it walks, no faster than it can.
	var facing: Vector3 = orders.move
	if orders.look != null:
		facing = orders.look - eye
	if facing.length() > 0.01:
		var turn: float = BotBrain.LEVELS[brain.skill].turn * delta
		var yaw := atan2(-facing.x, -facing.z)
		player.rotation.y = rotate_toward(player.rotation.y, yaw, turn)
		var pitch := atan2(facing.y, Vector2(facing.x, facing.z).length()) if orders.look != null else 0.0
		player.head.rotation.x = rotate_toward(player.head.rotation.x, clampf(pitch, -1.4, 1.4), turn)

	var aiming: bool = orders.look != null
	var speed := Movement.speed(orders.run, aiming, 0.0, match_.state.speed_factor(record)) * weapon.stats.carry_speed
	var move: Vector3 = orders.move
	player.velocity.x = move.x * speed
	player.velocity.z = move.z * speed
	if player.is_on_floor():
		player.velocity.y = Movement.JUMP_VELOCITY if orders.jump else 0.0
	else:
		player.velocity.y -= _gravity * delta
	player.move_and_slide()

	if orders.reload and weapon.start_reload():
		match_._reload(id, weapon.stats.id)
	# A weapon with nothing left gives way to one that never runs out.
	if weapon.is_dry():
		for carried: String in record.weapons:
			if Weapons.find(carried).unlimited():
				match_._select(id, carried)
	var enemy: Variant = _enemy_at(enemies, brain.target)
	if orders.fire and enemy != null and _fire_wait <= 0.0:
		if at.distance_to(enemy) <= KNIFE_REACH + BodyShape.RADIUS * 2.0:
			match_._strike(id, eye, -player.head.global_basis.z)
			_fire_wait = Melee.INTERVAL
		elif _facing(orders.look, eye) and weapon.fire():
			var stance := WeaponStats.AIMED | (WeaponStats.MOVING if move != Vector3.ZERO else 0)
			match_._shot(id, eye, -player.head.global_basis.z, randi(), weapon.stats.id, stance)
			if not weapon.stats.automatic:
				_fire_wait = maxf(weapon.stats.fire_interval, 0.25)
			if weapon.is_reloading():
				match_._reload(id, weapon.stats.id)
	if Overload.is_ready(record) and enemy != null:
		match_._overload(id)

	_publish(match_, id, (player.POSE_AIMING if aiming else 0) | (player.POSE_RELOADING if weapon.is_reloading() else 0))
	last_cost = Time.get_ticks_usec() - began


# Whether the weapon is pointing nearly enough at `point` to be worth firing.
func _facing(point: Variant, eye: Vector3) -> bool:
	if point == null:
		return false
	var way: Vector3 = (point - eye).normalized()
	return way.dot(-player.head.global_basis.z) > 0.985


static func _enemy_at(enemies: Array, id: int) -> Variant:
	for enemy: Dictionary in enemies:
		if enemy.id == id and enemy.visible:
			return enemy.at
	return null


# The weapon in hand, as this driver counts its rounds: the one the host has
# it holding, fitted as the host has it.
func _weapon(record: PlayerRecord) -> WeaponState:
	var stats := record.stats(record.weapon)
	if not weapons.has(record.weapon) or weapons[record.weapon].stats != stats:
		weapons[record.weapon] = WeaponState.new(stats)
	for id: String in weapons.keys():
		if not record.weapons.has(id):
			weapons.erase(id)
	return weapons[record.weapon]


## Back in play with everything full, as the host does for anyone.
func refill() -> void:
	for carried: WeaponState in weapons.values():
		carried.refill()
	_fire_wait = 0.0
	brain.goal = null


# Where things are that the brain might head for.
func _places(match_: Match, world: Node3D, record: PlayerRecord) -> Dictionary:
	var state := match_.state
	var health := []
	var roam := []
	var pads: Array = world.pickup_pads()
	for index in pads.size():
		if not state.pickup_ready(index):
			continue
		roam.append(pads[index].position)
		if pads[index].kind == Pickups.HEALTH:
			health.append(pads[index].position)
	return {"health": health, "roam": roam, "objective": objective_for(state, record, world.objective_spots())}


## Where the mode being played wants `record`'s owner to go, or null: to the
## other team's wheel, or home with it; to the hill; to the nearest point
## their team doesn't hold.
static func objective_for(state: MatchState, record: PlayerRecord, spots: Dictionary) -> Variant:
	if record.team < 0:
		return null
	if state.capture != null and spots.bases.size() == 2:
		if state.capture.carried_by(record.id) != -1:
			return spots.bases[record.team]
		var wanted: Dictionary = state.capture.wheels[1 - record.team]
		return null if wanted.status == CaptureRules.CARRIED else wanted.at
	if state.zones != null and state.settings.mode == MatchSettings.HILL and not spots.hills.is_empty():
		return spots.hills[posmod(state.zones.hill, spots.hills.size())]
	if state.zones != null and state.settings.mode == MatchSettings.POINTS:
		var best: Variant = null
		for index in mini(spots.points.size(), state.zones.owners.size()):
			if state.zones.owners[index] != record.team and (best == null or record.id % 3 == index):
				best = spots.points[index]
		return best
	return null


# Tells everyone where the bot is, as its own machine would, and the host
# where it was, for shots at it to be judged by.
func _publish(match_: Match, id: int, pose: int) -> void:
	player._pose = pose
	player.net_state = PackedFloat32Array([player.global_position.x, player.global_position.y, player.global_position.z, player.rotation.y, player.head.rotation.x, player._teleports, pose])
	match_.record_position(Match.now(), player.global_position, id, Vector2(player.rotation.y, player.head.rotation.x))
