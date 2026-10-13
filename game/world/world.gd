extends Node3D
## The arena and everyone in it. The host spawns and removes a player node per
## connected peer under Players, and the MultiplayerSpawner mirrors those nodes
## on every client.
##
## The arena itself is one of the Maps, loaded as the child Map. The host
## loads the one it chose before the session starts; everyone else loads the
## one named in the match state when it arrives.

const PLAYER := preload("res://player/player.tscn")
const PLAYER_COLOURS := preload("res://player/player.gd")
## Physics layer of the players, whom a shot stops at where it is shown.
const PLAYER_MASK := 2

@onready var players: Node3D = $Players
@onready var spawner: MultiplayerSpawner = $PlayerSpawner
@onready var match_: Node = $Match
@onready var effects: Node3D = $Effects

var sound_cues: Node
var objective_marks: Node3D
var watch_camera: Camera3D
var replay_view: Node3D

## The loaded arena, and which of the Maps it is.
var map: Node3D
var map_id := ""
var spawn_points: Node3D
## The loaded arena's jump pads and ladders, as Fixtures.of gives them.
var fixtures := {"pads": [], "ladders": []}
var menu_camera: Camera3D

# Which starting spot each player holds. Only the host uses it.
var _spawn_slots: SpawnSlots
# Where each player with a place of their own stands: the range's targets.
var _homes := {}
var _gravity_set := false


func _ready() -> void:
	spawner.spawn_function = _create_player
	match_.shot_shown.connect(_show_others_shot)
	match_.fell.connect(func(victim: int, weapon: String, from: Vector3) -> void:
		var player := player_node(victim)
		if player != null:
			player.fall(weapon, from))
	match_.emote_shown.connect(func(id: int, kind: int) -> void:
		var player := player_node(id)
		if player != null:
			player.show_emote(kind))
	match_.throw_shown.connect(func(id: int) -> void:
		var player := player_node(id)
		if player != null:
			player.body.act(CharacterMotion.THROW))
	match_.strike_shown.connect(func(striker: int) -> void:
		var player := player_node(striker)
		if player != null:
			player.swing_knife())
	match_.projectile_launched.connect(effects.launch_projectile)
	match_.projectile_burst.connect(effects.burst_projectile)
	match_.state_changed.connect(_on_state_changed)
	match_.block_gnawed.connect(_show_mice)
	sound_cues = preload("res://audio/sound_cues.gd").new(self)
	add_child(sound_cues)
	objective_marks = preload("res://world/objective_marks.gd").new(self)
	add_child(objective_marks)
	watch_camera = preload("res://world/watch_camera.gd").new(self)
	add_child(watch_camera)
	replay_view = preload("res://world/replay_view.gd").new(self)
	add_child(replay_view)
	match_.replay_shown.connect(func(clip: Dictionary, about: int, title: String) -> void:
		# For those who want them.
		if Settings.get_value("replays"):
			replay_view.play(clip, about, title))
	load_map(Maps.DEFAULT)
	menu_camera.make_current()
	apply_quality()
	Settings.changed.connect(func(key: String) -> void:
		if key == "quality":
			apply_quality())


## Puts the chosen graphics quality into effect.
func apply_quality() -> void:
	GraphicsQuality.apply(GraphicsQuality.level(Settings.get_value("quality")), get_viewport(), $Sun)


## Replaces the arena with map `id`, unless it is already loaded.
func load_map(id: String) -> void:
	if not Maps.exists(id):
		id = Maps.DEFAULT
	if id == map_id:
		return
	var was_menu_view := menu_camera != null and menu_camera.current
	if map != null:
		remove_child(map)
		map.queue_free()
	map = Maps.scene(id).instantiate()
	map.name = "Map"
	Scenery.dress(map)
	add_child(map)
	move_child(map, 0)
	map_id = id
	spawn_points = map.get_node("SpawnPoints")
	fixtures = Fixtures.of(map)
	menu_camera = map.get_node("MenuCamera")
	if was_menu_view:
		menu_camera.make_current()
	_build_pickups()


## Begins the session. `settings` are the host's choice, and only used there.
func start(settings := MatchSettings.new()) -> void:
	if not multiplayer.is_server():
		match_.start()
		return
	load_map(settings.map)
	_spawn_slots = SpawnSlots.new(spawn_points.get_child_count())
	multiplayer.peer_connected.connect(_add_player)
	multiplayer.peer_disconnected.connect(_remove_player)
	match_.start(settings)
	_add_player(1)
	_stand_targets()


func stop() -> void:
	replay_view.stop()
	_set_gravity(1.0)
	if multiplayer.peer_connected.is_connected(_add_player):
		multiplayer.peer_connected.disconnect(_add_player)
		multiplayer.peer_disconnected.disconnect(_remove_player)
	for player in players.get_children():
		player.queue_free()
	match_.stop()
	menu_camera.make_current()


## Whether this machine has the arena the host chose, so its player can move.
func ready_to_play() -> bool:
	return match_.has_state() and map_id == match_.state.settings.map


func _on_state_changed() -> void:
	if match_.has_state():
		load_map(match_.state.settings.map)
	_apply_gravity()
	_show_pickups()
	_show_drops()
	_show_blocks()
	objective_marks.refresh()


func _add_player(id: int) -> void:
	_join(id)
	_fill_with_bots()


func _remove_player(id: int) -> void:
	_leave(id)
	_fill_with_bots()


func _join(id: int) -> void:
	match_.add_player(id)
	if Bots.is_bot(id):
		var record: PlayerRecord = match_.state.player(id)
		match_.state.rename(id, Bots.name_for(id))
		record.look = Bots.look_for(id)
		record.main = Bots.loadout_for(id)[0]
		match_.state.rearm(record)
	spawner.spawn([id, spawn_points.get_child(_spawn_slots.assign(id)).global_position])
	print("player joined: %d (%d in session)" % [id, players.get_child_count()])


func _leave(id: int) -> void:
	_spawn_slots.release(id)
	match_.remove_player(id)
	var player := players.get_node_or_null(str(id))
	if player != null:
		players.remove_child(player)
		player.queue_free()
		print("player left: %d" % id)


# Gravity pulls as hard as the host's rules say, on this machine as on
# every other: on players, and on what they throw and launch.
func _apply_gravity() -> void:
	_set_gravity(match_.state.settings.gravity if match_.has_state() else 1.0)


func _set_gravity(share: float) -> void:
	if Projectile.gravity_scale == share and _gravity_set:
		return
	_gravity_set = true
	Projectile.gravity_scale = share
	var usual: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8)
	if is_inside_tree():
		PhysicsServer3D.area_set_param(get_world_3d().space, PhysicsServer3D.AREA_PARAM_GRAVITY, usual * share)


## The colour player `id` is drawn in: their team's, or their own.
func colour_of(id: int) -> Color:
	var record: PlayerRecord = match_.state.player(id) if match_.has_state() else null
	if record != null and record.team != MatchState.NO_TEAM:
		return TeamColors.of(record.team)
	return PLAYER_COLOURS.colour_for(id)


## Where player `id` belongs, if they have a place of their own, or null.
func home_of(id: int) -> Variant:
	return _homes.get(id)


## The loaded arena's targets, as the practice range has: each {"position",
## "distance": metres from the firing line}.
func target_spots() -> Array:
	var holder := map.get_node_or_null("Targets") if map != null else null
	if holder == null:
		return []
	return holder.get_children().map(func(marker: Node3D) -> Dictionary:
		return {"position": marker.global_position, "distance": int(marker.get_meta("distance", 0))})


# Host only, on the practice range: stands a target at each of its places,
# under a sign saying how far off it is. A target is a player that only
# stands there, so hitting one is exactly like hitting anyone.
func _stand_targets() -> void:
	_homes.clear()
	for sign in get_tree().get_nodes_in_group("range_signs"):
		sign.queue_free()
	if not match_.has_state() or not match_.state.settings.is_practice() or not match_.is_host():
		return
	var spots := target_spots()
	for index in spots.size():
		var id := Bots.target_id(index)
		var spot: Dictionary = spots[index]
		_homes[id] = spot.position
		match_.add_player(id)
		match_.state.rename(id, "Target %d m" % spot.distance)
		spawner.spawn([id, spot.position])
		var sign := Label3D.new()
		sign.text = "%d m" % spot.distance
		sign.font_size = 220
		sign.outline_size = 30
		sign.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		sign.add_to_group("range_signs")
		add_child(sign)
		sign.global_position = spot.position + Vector3.UP * (BodyShape.HEIGHT + 1.6)


## The ids of the bots in the session: not the practice range's targets.
func bot_ids() -> Array:
	var ids := []
	for id: int in match_.state.players:
		if Bots.is_bot(id) and not Bots.is_target(id):
			ids.append(id)
	ids.sort()
	return ids


# Host only: brings the number of bots to what the host asked for, less any
# whose place a real player has taken.
func _fill_with_bots() -> void:
	if not match_.has_state() or not match_.is_host():
		return
	var here := bot_ids()
	var humans: int = match_.state.players.keys().filter(func(id: int) -> bool: return not Bots.is_bot(id)).size()
	var wanted := Bots.wanted(match_.state.settings, humans)
	# The last to come are the first to go.
	while here.size() > wanted:
		_leave(here.pop_front())
	var index := 0
	while here.size() < wanted:
		if not here.has(Bots.id_for(index)):
			_join(Bots.id_for(index))
			here.append(Bots.id_for(index))
		index += 1


# A model floating over each pickup pad, shown while its pickup is there.
func _build_pickups() -> void:
	for pad: Node3D in map.get_node("Pickups").get_children():
		var kind := String(pad.get_meta("kind", Pickups.HEALTH))
		var item := PickupModels.build(kind)
		item.name = "Item"
		pad.add_child(item)
		pad.add_child(PickupModels.base())


# The weapons knocked-out players left behind, lying where they fell.
func _show_drops() -> void:
	var holder: Node3D = get_node_or_null("Drops")
	if holder == null:
		holder = Node3D.new()
		holder.name = "Drops"
		add_child(holder)
	var lying := {}
	if match_.has_state():
		for drop: Dictionary in match_.state.drops:
			lying[str(drop.id)] = drop
	for shown in holder.get_children():
		if not lying.has(String(shown.name)):
			holder.remove_child(shown)
			shown.queue_free()
	for key: String in lying:
		if holder.has_node(key):
			continue
		var model := WeaponModels.build(lying[key].weapon)
		model.name = key
		model.get_node("Hand").visible = false
		WeaponModels.show_own_view(model, false)
		model.scale = Vector3.ONE * 1.4
		# On its side on the ground, each at its own angle.
		model.rotation = Vector3(0.0, int(key) * 2.4, PI / 2.0)
		holder.add_child(model)
		model.global_position = lying[key].at + Vector3.UP * 0.12


## How many dropped weapons are shown lying about.
func drops_shown() -> int:
	var holder := get_node_or_null("Drops")
	return holder.get_child_count() if holder != null else 0


func _show_pickups() -> void:
	var pads := map.get_node("Pickups").get_children()
	for index in pads.size():
		pads[index].get_node("Item").visible = match_.state.pickup_ready(index) or match_.state.pickups.size() <= index


func _process(delta: float) -> void:
	for pad: Node3D in map.get_node("Pickups").get_children():
		pad.get_node("Item").rotate_y(delta * 1.5)


## This machine's own player fired: show the shot at once, without waiting to
## hear from the host where it went.
func _show_own_shot(origin: Vector3, direction: Vector3, seed: int, stance: int, run: int, player: Node3D) -> void:
	var weapon: WeaponStats = player.shot_stats if player.shot_stats != null else player.weapon.stats
	sound_cues.own_shot(weapon.credit(), player.muzzle.global_position)
	if weapon.fires_projectile():
		# The projectile itself shows once the host launches it.
		effects.show_shot(player.muzzle.global_position, [])
		return
	var aside: Vector3 = player.muzzle.global_basis.x
	var space := get_world_3d().direct_space_state
	var ends := []
	for pellet in HitScan.pellet_directions(direction, weapon.pellets, weapon.spread_for(stance, run), seed):
		# As far as the arena lets it go, thin cover and all, or to the first
		# player in its way.
		var reach: float = Penetration.path(origin, pellet, weapon.max_range, Match.wall_caster(space)).reach
		var query := PhysicsRayQueryParameters3D.create(origin, origin + pellet * reach, PLAYER_MASK, [player.get_rid()])
		var hit := space.intersect_ray(query)
		ends.append(hit.position if not hit.is_empty() else origin + pellet * reach)
	effects.show_shot(player.muzzle.global_position, ends, aside)


func _show_others_shot(shooter: int, ends: Array) -> void:
	var player := player_node(shooter)
	if player != null:
		player.kick()
		effects.show_shot(player.muzzle.global_position, ends, player.muzzle.global_basis.x)


## The arena's pickup pads, in order: [{"position": Vector3, "kind": String}].
func pickup_pads() -> Array:
	var pads := []
	for pad: Node3D in map.get_node("Pickups").get_children():
		pads.append({"position": pad.global_position, "kind": String(pad.get_meta("kind", Pickups.HEALTH))})
	return pads


## The loaded arena's blocks of cheese, in an order that is the same on
## every machine.
func cheese_blocks() -> Array:
	if map == null:
		return []
	return map.get_node("Geometry").get_children().filter(func(body: Node) -> bool: return String(body.name).begins_with("Block"))


## Which of the cheese blocks `collider` is, or -1 if it is none of them.
func block_index(collider: Object) -> int:
	return cheese_blocks().find(collider)


## The space each cheese block fills, in the same order.
func block_boxes() -> Array:
	return cheese_blocks().map(func(body: Node3D) -> AABB:
		var size: Vector3 = (body.get_node("CollisionShape3D").shape as BoxShape3D).size
		return AABB(body.global_position - size / 2.0, size))


# Shows each block of cheese as the match state has it: whole, bitten into,
# or gone, when it stops nothing and a scatter of crumbs marks its going.
func _show_blocks() -> void:
	var rules: BlockRules = match_.state.blocks if match_.has_state() else null
	var bodies := cheese_blocks()
	for index in bodies.size():
		var body: StaticBody3D = bodies[index]
		var stage: int = rules.stage(index) if rules != null and index < rules.health.size() else 0
		if body.get_meta("stage", 0) == stage:
			continue
		var was: int = body.get_meta("stage", 0)
		body.set_meta("stage", stage)
		var gone := stage > BlockRules.STAGES
		body.visible = not gone
		body.collision_layer = 0 if gone else 1
		body.get_node("CollisionShape3D").set_deferred("disabled", gone)
		if gone and was <= BlockRules.STAGES and is_inside_tree():
			effects.crumbs(body.global_position, Scenery.CHEESE, 40, 7.0)
			Sounds.play_at("crumble", body.global_position, self)
		Scenery.bite(body, 0 if gone else stage)


## How many cheese blocks are standing, as they are shown.
func blocks_standing() -> int:
	return cheese_blocks().filter(func(body: Node3D) -> bool: return body.visible).size()


# Mice come out round block `index` for a few seconds, and are heard.
func _show_mice(index: int) -> void:
	var bodies := cheese_blocks()
	if index < 0 or index >= bodies.size() or not is_inside_tree():
		return
	var body: Node3D = bodies[index]
	var half: Vector3 = (body.get_node("CollisionShape3D").shape as BoxShape3D).size / 2.0
	Sounds.play_at("gnaw", body.global_position, self)
	for mouse_index in 3:
		var mouse := Scenery.mouse()
		mouse.add_to_group("mice")
		add_child(mouse)
		# Round its foot, each on a side of its own, nose to the cheese.
		var angle := mouse_index * TAU / 3.0 + index
		var out := Vector3(cos(angle), 0.0, sin(angle))
		mouse.global_position = body.global_position + out * (maxf(half.x, half.z) + 0.5) + Vector3.DOWN * half.y
		mouse.rotation.y = atan2(out.x, out.z)
		var nibble := create_tween().set_loops(8)
		nibble.tween_property(mouse, "scale", Vector3(1.0, 0.85, 1.1), 0.18)
		nibble.tween_property(mouse, "scale", Vector3.ONE, 0.18)
		get_tree().create_timer(3.0).timeout.connect(mouse.queue_free)


## The loaded arena's places for the modes that have them: {"bases": the
## teams' bases, Cheddar's first, "hills": where the hill goes in turn,
## "points": the three points, in order}.
func objective_spots() -> Dictionary:
	var spots := {"bases": [], "hills": [], "points": []}
	var holder := map.get_node_or_null("Objectives") if map != null else null
	if holder == null:
		return spots
	for marker: Node3D in holder.get_children():
		var kind := String(marker.name)
		if kind.begins_with("Base"):
			spots.bases.append(marker.global_position)
		elif kind.begins_with("Hill"):
			spots.hills.append(marker.global_position)
		elif kind.begins_with("Point"):
			spots.points.append(marker.global_position)
	return spots


## The player node for peer `id`, or null.
func player_node(id: int) -> Node3D:
	var node := players.get_node_or_null(str(id))
	return node if node != null and not node.is_queued_for_deletion() else null


func spawn_positions() -> Array[Vector3]:
	var spots: Array[Vector3] = []
	for spot: Node3D in spawn_points.get_children():
		spots.append(spot.global_position)
	return spots


## Runs on every peer, with the data the host passed to `spawner.spawn`: the
## player's peer id and where the host chose for them to start.
func _create_player(data: Array) -> Node:
	var player := PLAYER.instantiate()
	player.name = str(data[0])
	player.match_ = match_
	player.world = self
	player.fired.connect(_show_own_shot.bind(player))
	player.received.connect(func(time: float, at: Vector3, teleports: int, facing: Vector2) -> void:
		match_.on_moved(time, at, teleports, data[0], facing))
	player.teleported.connect(match_.forget_positions.bind(data[0]))
	player.spawn_position = data[1]
	return player
