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
var atmosphere: Atmosphere
var ambient_life: Node3D
var lighting: Node3D

## The loaded arena, and which of the Maps it is.
var map: Node3D
var map_id := ""
var spawn_points: Node3D
## The loaded arena's jump pads and ladders, as Fixtures.of gives them.
var fixtures := {"pads": [], "ladders": []}
## The code of the custom arena to play on (see ArenaCode), when the match is
## on one: the host's own, or what the host sent.
var custom_code := ""
# The menu's view of the arena as its scene has it, which the title screen
# drifts about.
var _menu_view := Transform3D.IDENTITY
## Whether the game is getting ready (see warm_up), and when it has.
var warming := false
var _warm_frames := 0
signal warmed
## The layer that darkens the corners of the view (see Picture).
var vignette: CanvasLayer
var _outlines_in := 0.0
# The render scale dynamic resolution has settled on, and the frames it has
# counted since it last looked.
var _dynamic_scale := 1.0
var _frames := 0
var _frames_took := 0.0
## What being hurt looks like to this machine's player.
var hurt_view: HurtView
## The voice that tells this machine's player how the match stands.
var announcer: Announcer
## Talking with teammates.
var voice_chat: VoiceChat
## The markers players have put down for their teammates.
var pings: Pings
## The camera come free, for taking a picture.
var photo_mode: PhotoMode
## What counts this machine's player's match toward their record.
var match_keeper: MatchKeeper
var _built_code := ""
## The loaded arena's hazards, doors and lifts, as MovingParts.of gives them.
var moving_parts := {"hazards": [], "doors": [], "lifts": []}
# How far each door has turned on its hinges, in radians.
var _door_angles: Array[float] = []
var menu_camera: Camera3D

# The loaded arena's cheese (see cheese_blocks), each block's index by its
# body, the space each fills, and which stand on which.
var _blocks: Array = []
var _block_indices := {}
var _block_boxes: Array = []
var _block_supports: Array[PackedInt32Array] = []
## Ways across a large map, for bots (see Routes); empty on an arena.
var routes := Routes.new()
## The loaded map's buildings, each drawn as one (see Clusters).
var clusters: Clusters
## The giant mice, as they are seen (see MouseRules).
var mice_view: MiceView
var _edge := Rect2()

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
			player.body.act(CharacterMotion.THROW)
			player.voice("throw"))
	match_.strike_shown.connect(func(striker: int) -> void:
		var player := player_node(striker)
		if player != null:
			player.swing_knife()
			player.voice("strike"))
	match_.projectile_launched.connect(effects.launch_projectile)
	match_.projectile_burst.connect(effects.burst_projectile)
	match_.overloaded.connect(func(id: int) -> void:
		var player := player_node(id)
		if player != null:
			effects.overload_flare(player.global_position + Vector3.UP * 0.4, player.body.colour()))
	match_.picked_up.connect(func(id: int, kind: String) -> void:
		var player := player_node(id)
		if player != null:
			effects.sparkle(player.global_position, kind))
	match_.state_changed.connect(_on_state_changed)
	mice_view = MiceView.new(self)
	add_child(mice_view)
	sound_cues = preload("res://audio/sound_cues.gd").new(self)
	add_child(sound_cues)
	objective_marks = preload("res://world/objective_marks.gd").new(self)
	add_child(objective_marks)
	watch_camera = preload("res://world/watch_camera.gd").new(self)
	add_child(watch_camera)
	atmosphere = Atmosphere.new($WorldEnvironment.environment, $Sun)
	add_child(atmosphere)
	replay_view = preload("res://world/replay_view.gd").new(self)
	add_child(replay_view)
	match_.replay_shown.connect(func(clip: Dictionary, about: int, title: String) -> void:
		# For those who want them.
		if Settings.get_value("replays"):
			replay_view.play(clip, about, title))
	vignette = Picture.vignette()
	add_child(vignette)
	hurt_view = HurtView.new(self)
	add_child(hurt_view)
	announcer = Announcer.new(self)
	add_child(announcer)
	add_child(MusicDirector.new(self))
	voice_chat = VoiceChat.new(self)
	add_child(voice_chat)
	pings = Pings.new(self)
	add_child(pings)
	photo_mode = PhotoMode.new(self)
	add_child(photo_mode)
	match_keeper = MatchKeeper.new(self)
	add_child(match_keeper)
	photo_mode.changed.connect(func() -> void:
		var own: Node3D = player_node(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null
		if own != null:
			own.seen_from_outside(photo_mode.on))
	load_map(Maps.DEFAULT)
	menu_camera.make_current()
	apply_quality()
	Settings.changed.connect(func(key: String) -> void:
		if key == "quality" or key.begins_with("gfx_") or key == GraphicsQuality.DYNAMIC:
			apply_quality()
			# What is only made on some graphics is made, or cleared away.
			if map != null and (key == "quality" or key == "gfx_effects"):
				_show_sky()
				Detail.thin(map, GraphicsQuality.choice(GraphicsQuality.EFFECTS))
		elif key in [Picture.BLOOM, Picture.GRADE, Picture.VIGNETTE]:
			apply_picture())


## Draws one of everything a fight shows in front of the view for a few
## frames, so that the first of each in play doesn't make the game catch
## (see Warmup). Whoever calls it covers the view meanwhile.
func warm_up() -> void:
	if not is_inside_tree():
		return
	warming = true
	var camera := get_viewport().get_camera_3d()
	var at := camera.global_position - camera.global_basis.z * Warmup.AHEAD if camera != null else Vector3.UP * 3.0
	var props := Warmup.props()
	add_child(props)
	props.global_position = at
	Outlines.put(props.get_child(0), Color.WHITE, true)
	Outlines.glow(props.get_child(1), Color.WHITE)
	Warmup.fire(effects, at)
	hurt_view.warm(true)
	_warm_frames = Warmup.FRAMES


# Clears away what was drawn to get ready.
func _finish_warm_up() -> void:
	var props := get_node_or_null("Warmup")
	if props != null:
		props.queue_free()
	effects.clear()
	hurt_view.warm(false)
	warming = false
	warmed.emit()


## How far the menu's view of the arena swings to either side as it drifts,
## in radians, and how many seconds a swing there and back takes.
const DRIFT := 0.22
const DRIFT_TIME := 48.0


## Turns the menu's view of the arena a little way round its middle, as it
## is `seconds` into drifting: a slow swing to one side and back.
func drift_menu_view(seconds: float) -> void:
	if menu_camera == null or not Look.moves():
		return
	menu_camera.transform = _menu_view.rotated(Vector3.UP, sin(seconds * TAU / DRIFT_TIME) * DRIFT)


## Puts the chosen graphics quality into effect.
func apply_quality() -> void:
	GraphicsQuality.apply(GraphicsQuality.level(Settings.get_value("quality")), get_viewport(), $Sun, GraphicsQuality.chosen())
	_dynamic_scale = get_viewport().scaling_3d_scale
	apply_picture()


## Puts the glow, the arena's cast of colour and the darker corners into
## effect, as the settings have them.
func apply_picture() -> void:
	var wanted := Picture.wanted()
	Picture.apply($WorldEnvironment.environment, map_id, GraphicsQuality.choice(GraphicsQuality.EFFECTS), wanted)
	vignette.visible = wanted[Picture.VIGNETTE]


## Replaces the arena with map `id`, unless it is already loaded.
func load_map(id: String) -> void:
	if not Maps.exists(id):
		id = Maps.DEFAULT
	# A custom arena is built from its code, which a player other than the
	# host may still be waiting for.
	var arena := ArenaCode.decode(custom_code) if id == Maps.CUSTOM else {}
	if id == Maps.CUSTOM and arena.is_empty():
		return
	_door_angles.clear()
	if id == map_id and (id != Maps.CUSTOM or custom_code == _built_code):
		return
	var was_menu_view := menu_camera != null and menu_camera.current
	if map != null:
		remove_child(map)
		map.queue_free()
	_built_code = custom_code if id == Maps.CUSTOM else ""
	map = ArenaCode.build(arena) if id == Maps.CUSTOM else Maps.build(id)
	map.name = "Map"
	Scenery.dress(map)
	add_child(map)
	move_child(map, 0)
	clusters = Clusters.new()
	map.add_child(clusters)
	clusters.build(map.get_node("Geometry"))
	map_id = id
	if vignette != null:
		apply_picture()
	spawn_points = map.get_node("SpawnPoints")
	_find_blocks()
	_edge = Scenery.bounds(map.get_node("Geometry"))
	routes = Routes.new(map.get_meta("routes", {}))
	fixtures = Fixtures.of(map)
	moving_parts = MovingParts.of(map)
	menu_camera = map.get_node("MenuCamera")
	_menu_view = menu_camera.transform
	if was_menu_view:
		menu_camera.make_current()
	_build_pickups()
	_build_life()
	lighting = preload("res://world/lighting.gd").new()
	map.add_child(lighting)
	lighting.build(map_id, map.get_node("Geometry"))
	if atmosphere != null:
		_show_sky()
	Detail.thin(map, GraphicsQuality.choice(GraphicsQuality.EFFECTS))


## Loads the custom arena `code` describes.
func load_custom(code: String) -> void:
	custom_code = code
	load_map(Maps.CUSTOM)


## Shows `arena` (see ArenaCode) as it is being laid out, in the editor.
func show_custom(arena: Dictionary) -> void:
	# Even one with nothing on it, which has no code to keep.
	custom_code = ArenaCode.encode(arena)
	_built_code = "*"
	load_map(Maps.CUSTOM)


## Begins the session. `settings` are the host's choice, and only used there.
func start(settings := MatchSettings.new()) -> void:
	if not multiplayer.is_server():
		match_.start()
		return
	# The host's own custom arena, or the usual one if they have made none.
	if settings.map == Maps.CUSTOM:
		custom_code = Settings.get_value("custom_arena")
		if ArenaCode.decode(custom_code).is_empty():
			settings.map = Maps.DEFAULT
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
	atmosphere.show_look(Atmosphere.NOON, false)
	if multiplayer.peer_connected.is_connected(_add_player):
		multiplayer.peer_connected.disconnect(_add_player)
		multiplayer.peer_disconnected.disconnect(_remove_player)
	for player in players.get_children():
		player.queue_free()
	if photo_mode.on:
		photo_mode.toggle()
	match_.stop()
	pings.clear()
	# The next host's custom arena is theirs to send.
	custom_code = ""
	if map_id == Maps.CUSTOM:
		load_map(Maps.DEFAULT)
	menu_camera.make_current()


## The loaded arena's edge, across X and Z (see Scenery.bounds).
func edge() -> Rect2:
	return _edge


## Whether the loaded map is one of the large ones (see BigMap).
func is_large() -> bool:
	return map != null and map.get_meta("large", false)


## Whether this machine has the arena the host chose, so its player can move.
func ready_to_play() -> bool:
	return match_.has_state() and map_id == match_.state.settings.map


func _on_state_changed() -> void:
	if match_.has_state():
		load_map(match_.state.settings.map)
	_apply_gravity()
	_show_sky()
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
	var slot := _spawn_slots.assign(id)
	var at: Vector3 = spawn_points.get_child(slot).global_position
	# On a large map, someone joining a team starts at its own end.
	var own: Array[Vector3] = match_.own_side(id, spawn_positions())
	if not own.has(at):
		at = own[slot % own.size()]
	spawner.spawn([id, at])
	print("player joined: %d (%d in session)" % [id, players.get_child_count()])


func _leave(id: int) -> void:
	_spawn_slots.release(id)
	match_.remove_player(id)
	var player := players.get_node_or_null(str(id))
	if player != null:
		players.remove_child(player)
		player.queue_free()
		print("player left: %d" % id)


# What moves about the arena for the look of it (see AmbientLife).
func _build_life() -> void:
	var extras := GraphicsQuality.choice(GraphicsQuality.EFFECTS) != GraphicsQuality.LOW
	ambient_life = preload("res://world/ambient_life.gd").new()
	map.add_child(ambient_life)
	# Traffic lights and a train belong to the city's streets, not to every
	# arena that has a city round it.
	var kind := String(map.get_meta("scenery", Scenery.MEADOW))
	if kind == Scenery.CITY and map_id != "city":
		kind = "town"
	ambient_life.build(kind, Scenery.bounds(map.get_node("Geometry")), extras)


# The sky is as the host chose it, with rain or dust in the air only where
# the graphics setting has room for it.
func _show_sky() -> void:
	var extras := GraphicsQuality.choice(GraphicsQuality.EFFECTS) != GraphicsQuality.LOW
	atmosphere.show_look(match_.state.settings.sky if match_.has_state() else Atmosphere.NOON, extras)
	if lighting != null:
		lighting.show_for(atmosphere.showing, -$Sun.global_basis.z, extras)
	# Clouds throw shadows only where there is a sun to throw them.
	if ambient_life != null:
		ambient_life.show_cloud_shadows([Atmosphere.NOON, Atmosphere.DAWN, Atmosphere.DUSK].has(atmosphere.showing))


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
	if _warm_frames > 0:
		_warm_frames -= 1
		if _warm_frames == 0:
			_finish_warm_up()
	for pad: Node3D in map.get_node("Pickups").get_children():
		pad.get_node("Item").rotate_y(delta * 1.5)
	_outlines_in -= delta
	if _outlines_in <= 0.0:
		_outlines_in = 0.2
		show_outlines()
		# Whichever camera the view is through sees as far as is chosen.
		var camera := get_viewport().get_camera_3d()
		if camera != null and not is_equal_approx(camera.far, GraphicsQuality.view_metres()):
			camera.far = GraphicsQuality.view_metres()
	_hold_frame_rate(delta)


# Lets the render scale drop to hold the frame rate, if that is wanted, and
# come back when there is room.
func _hold_frame_rate(delta: float) -> void:
	if not Settings.get_value(GraphicsQuality.DYNAMIC):
		return
	_frames += 1
	_frames_took += delta
	if _frames_took < 0.5:
		return
	var most := GraphicsQuality.render_scale(GraphicsQuality.level(Settings.get_value("quality")), Settings.get_value("gfx_render_scale"))
	_dynamic_scale = GraphicsQuality.dynamic_scale(_dynamic_scale, _frames_took / _frames, most)
	get_viewport().scaling_3d_scale = _dynamic_scale
	_frames = 0
	_frames_took = 0.0


## Outlines this machine's player's teammates, through walls, and makes the
## pickups lying ready glow, and nobody and nothing else (see Outlines).
func show_outlines() -> void:
	var wanted: bool = Settings.get_value(Outlines.SETTING) and match_.has_state() and multiplayer.multiplayer_peer != null
	var own: PlayerRecord = match_.own_record() if wanted else null
	var teams: bool = wanted and match_.state.settings.is_teams()
	for player in players.get_children():
		var id := String(player.name).to_int()
		if wanted and Outlines.shows(own, match_.state.player(id), teams):
			Outlines.put(player.body, TeamColors.of(own.team), true)
		else:
			Outlines.take(player.body)
	var viewer: Node3D = player_node(multiplayer.get_unique_id()) if wanted else null
	var pads := map.get_node("Pickups").get_children()
	for index in pads.size():
		var item: Node3D = pads[index].get_node("Item")
		if viewer != null and own != null and own.alive and Outlines.pickup_shows(item.visible):
			Outlines.glow(item, Outlines.PICKUP_COLOUR)
		else:
			Outlines.take(item)


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
	effects.show_shot(player.muzzle.global_position, ends, aside, weapon.credit())


func _show_others_shot(shooter: int, ends: Array) -> void:
	var player := player_node(shooter)
	if player != null:
		player.kick()
		var record: PlayerRecord = match_.state.player(shooter) if match_.has_state() else null
		effects.show_shot(player.muzzle.global_position, ends, player.muzzle.global_basis.x, record.weapon if record != null else "")


## The arena's pickup pads, in order: [{"position": Vector3, "kind": String}].
func pickup_pads() -> Array:
	var pads := []
	for pad: Node3D in map.get_node("Pickups").get_children():
		pads.append({"position": pad.global_position, "kind": String(pad.get_meta("kind", Pickups.HEALTH))})
	return pads


# Each door turns toward open or shut, as the host has it, and each lift
# is where the session's clock puts it: the same on every machine, moved in
# step with the physics so that players are carried and stopped by them.
func _physics_process(delta: float) -> void:
	if map == null:
		return
	var doors := _parts_in("doors")
	_door_angles.resize(doors.size())
	for index in mini(doors.size(), moving_parts.doors.size()):
		var door: Dictionary = moving_parts.doors[index]
		var way: int = match_.state.doors[index] if match_.has_state() and index < match_.state.doors.size() else 0
		_door_angles[index] = move_toward(_door_angles[index], way * MovingParts.DOOR_OPEN_ANGLE, MovingParts.DOOR_TURN_SPEED * delta)
		doors[index].global_transform = MovingParts.door_transform(door, _door_angles[index])
	var lifts := _parts_in("lifts")
	var time: float = match_.state.uptime if match_.has_state() else 0.0
	for index in mini(lifts.size(), moving_parts.lifts.size()):
		var lift: Dictionary = moving_parts.lifts[index]
		lifts[index].global_position = lift.position + Vector3.UP * MovingParts.lift_height(time, lift.rise)


# The loaded arena's doors, or its lifts, in its own order.
func _parts_in(group: String) -> Array:
	return map.get_children().filter(func(part: Node) -> bool: return part.is_in_group(group)) if map != null else []


## How far each door has turned toward open, from 0 to 1, for tests.
func doors_shown() -> Array:
	var shown := []
	for index in mini(_door_angles.size(), moving_parts.doors.size()):
		shown.append(snappedf(absf(_door_angles[index]) / MovingParts.DOOR_OPEN_ANGLE, 0.01))
	return shown


## The loaded arena's named places: each {"name", "at"}.
func places() -> Array:
	var holder := map.get_node_or_null("Places") if map != null else null
	if holder == null:
		return []
	return holder.get_children().map(func(marker: Node3D) -> Dictionary:
		return {"name": String(marker.get_meta("label", "")), "at": marker.global_position})


## What the part of the arena at `at` is called: the name of the nearest of
## its places, with a floor's difference in height counting double. "" in an
## arena with none.
func place_at(at: Vector3) -> String:
	return nearest_place(places(), at)


static func nearest_place(named: Array, at: Vector3) -> String:
	var found := ""
	var nearest := INF
	for place: Dictionary in named:
		var off: Vector3 = at - place.at
		var distance := Vector3(off.x, off.y * 2.0, off.z).length()
		if distance < nearest:
			nearest = distance
			found = place.name
	return found


## The loaded arena's blocks of cheese, in an order that is the same on
## every machine.
func cheese_blocks() -> Array:
	return _blocks


## Which of the cheese blocks `collider` is, or -1 if it is none of them.
func block_index(collider: Object) -> int:
	return _block_indices.get(collider, -1)


## The space each cheese block fills, in the same order.
func block_boxes() -> Array:
	return _block_boxes


## Which blocks go when block `index` does: those standing on it, and on
## those (see BlockRules).
func blocks_above(index: int) -> Array[int]:
	return BlockRules.above(_block_supports, index)


# Notes the loaded arena's cheese, once, as a large map has hundreds of
# blocks and every shot asks after them.
func _find_blocks() -> void:
	_blocks = map.get_node("Geometry").get_children().filter(func(body: Node) -> bool: return String(body.name).begins_with("Block"))
	_block_indices.clear()
	_block_boxes.clear()
	for index in _blocks.size():
		var body: Node3D = _blocks[index]
		_block_indices[body] = index
		var size: Vector3 = (body.get_node("CollisionShape3D").shape as BoxShape3D).size
		# Where it is in the arena, which is where it is in the map.
		_block_boxes.append(AABB(body.position - size / 2.0, size))
	_block_supports = BlockRules.supports(_block_boxes)


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
	if clusters != null:
		clusters.refresh()


## How many cheese blocks are standing, as they are shown.
func blocks_standing() -> int:
	return cheese_blocks().filter(func(body: Node3D) -> bool: return body.visible).size()


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
