extends GutTest
## Space: which treatment a sound gets from where it is made and where it is
## heard from, and footsteps by what is trodden on and who treads.

const WORLD := preload("res://world/world.tscn")
const OPEN := Acoustics.OPEN
const ALLEY := Acoustics.ALLEY
const INDOORS := Acoustics.INDOORS

var world: Node3D


func _arena(id: String) -> PhysicsDirectSpaceState3D:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map(id)
	await wait_physics_frames(2)
	return world.get_world_3d().direct_space_state


func _walls(near: Array) -> Array:
	var walls := []
	for way in Acoustics.WAYS:
		walls.append(near.has(way))
	return walls


func test_a_roof_and_walls_is_indoors_walls_on_both_sides_is_an_alley() -> void:
	assert_eq(Acoustics.classify(false, _walls([])), OPEN)
	assert_eq(Acoustics.classify(false, _walls([0])), OPEN, "one wall is not an alley")
	assert_eq(Acoustics.classify(false, _walls([0, 1, 2])), OPEN, "nor a corner")
	assert_eq(Acoustics.classify(false, _walls([2, 6])), ALLEY, "walls on both sides")
	assert_eq(Acoustics.classify(false, _walls([0, 1, 2, 3, 4, 5, 6, 7])), ALLEY, "a yard open to the sky")
	assert_eq(Acoustics.classify(true, _walls([0, 1, 2, 3, 4, 5, 6, 7])), INDOORS)
	assert_eq(Acoustics.classify(true, _walls([0, 2, 4, 6])), INDOORS)
	assert_eq(Acoustics.classify(true, _walls([])), ALLEY, "under an arch, with open sides")
	assert_eq(Acoustics.classify(true, _walls([1])), ALLEY)


func test_the_middle_of_an_open_arena_is_the_open() -> void:
	var space: PhysicsDirectSpaceState3D = await _arena("canyon")
	assert_eq(Acoustics.survey(space, Vector3(0, 0, 0)), OPEN)
	# Up against the unseen edge of the arena is still the open: it is a
	# fence, not a wall.
	var edge: Vector3 = world.spawn_positions()[0]
	assert_ne(Acoustics.survey(space, edge), INDOORS)


func test_the_citys_tower_is_indoors_and_its_streets_are_not() -> void:
	var space: PhysicsDirectSpaceState3D = await _arena("city")
	assert_eq(Acoustics.survey(space, Vector3(22, 0, -20)), INDOORS, "the lobby")
	assert_eq(Acoustics.survey(space, Vector3(22, 6.4, -20)), INDOORS, "and the floor above")
	assert_ne(Acoustics.survey(space, Vector3(0, 0, 0)), INDOORS, "the crossroads")
	# Somewhere on the city's streets there are walls on both sides.
	var places := {}
	for x in range(-30, 31, 5):
		for z in range(-30, 31, 5):
			places[Acoustics.survey(space, Vector3(x, 0, z))] = true
	assert_has(places, ALLEY)
	assert_has(places, INDOORS)


func test_every_arena_is_some_kind_of_place_wherever_a_player_starts() -> void:
	for id: String in Maps.ORDER:
		var space: PhysicsDirectSpaceState3D = await _arena(id)
		for spot: Vector3 in world.spawn_positions():
			assert_has(Acoustics.PLACES, Acoustics.survey(space, spot), "%s at %s" % [id, spot])
		world.free()


func test_a_place_is_worked_out_once_for_each_square_and_again_for_a_new_arena() -> void:
	await _arena("city")
	var lobby := Vector3(22, 0, -20)
	assert_eq(Acoustics.place_at(world.map, lobby), INDOORS)
	assert_eq(Acoustics._known.size(), 1)
	Acoustics.place_at(world.map, lobby + Vector3(0.2, 0, 0.2))
	assert_eq(Acoustics._known.size(), 1, "the same square")
	Acoustics.place_at(world.map, Vector3.ZERO)
	assert_eq(Acoustics._known.size(), 2)
	world.load_map("box")
	await wait_physics_frames(2)
	assert_eq(Acoustics.place_at(world.map, lobby), OPEN, "what was the lobby is open ground here")
	assert_eq(Acoustics._known.size(), 1)
	assert_eq(Acoustics.place_at(null, lobby), OPEN)


func test_a_sound_far_off_loses_its_top_and_one_behind_a_wall_is_muffled() -> void:
	assert_eq(Acoustics.cutoff(0.0), Acoustics.CLEAR_CUTOFF)
	assert_eq(Acoustics.cutoff(Acoustics.FAR_FROM), Acoustics.CLEAR_CUTOFF, "all of it, close by")
	var last := Acoustics.CLEAR_CUTOFF
	for metres in range(30, 100, 5):
		var heard := Acoustics.cutoff(metres)
		assert_lt(heard, last, "duller at %d m" % metres)
		last = heard
	assert_almost_eq(Acoustics.cutoff(Acoustics.FAR_TO), Acoustics.FAR_CUTOFF, 0.5)
	assert_almost_eq(Acoustics.cutoff(500.0), Acoustics.FAR_CUTOFF, 0.5, "a dull crack, however far")
	assert_gt(Acoustics.FAR_CUTOFF, 1000.0, "still a crack: not a thud")
	# Behind a wall it is muffled even close to.
	assert_eq(Acoustics.cutoff(5.0, true), Acoustics.MUFFLED_CUTOFF)
	assert_eq(Acoustics.cutoff(500.0, true), Acoustics.MUFFLED_CUTOFF)
	assert_lt(Acoustics.MUFFLED_CUTOFF, Acoustics.FAR_CUTOFF)


func test_which_treatment_each_place_and_position_gets() -> void:
	var near_open := Acoustics.treatment(Mix.WEAPONS, OPEN, 10.0, false)
	assert_eq(near_open, {"bus": Mix.WEAPONS, "cutoff": Acoustics.CLEAR_CUTOFF, "filter_db": Acoustics.FILTER_DB, "volume_db": 0.0})
	assert_eq(Acoustics.treatment(Mix.WEAPONS, INDOORS, 10.0, false).bus, "WeaponsIndoors")
	assert_eq(Acoustics.treatment(Mix.WEAPONS, ALLEY, 10.0, false).bus, "WeaponsAlley")
	assert_eq(Acoustics.treatment(Mix.STEPS, INDOORS, 10.0, false).bus, "StepsIndoors")
	assert_eq(Acoustics.treatment(Mix.WORLD, INDOORS, 10.0, false).bus, Mix.WORLD, "a pickup's chime is not changed by the room")
	assert_eq(Acoustics.treatment(Mix.INTERFACE, ALLEY, 10.0, false).bus, Mix.INTERFACE)
	var behind := Acoustics.treatment(Mix.WEAPONS, OPEN, 10.0, true)
	assert_eq(behind.cutoff, Acoustics.MUFFLED_CUTOFF)
	assert_eq(behind.volume_db, Acoustics.MUFFLED_DB)
	assert_lt(behind.filter_db, near_open.filter_db, "its top taken off harder")
	var far := Acoustics.treatment(Mix.WEAPONS, OPEN, 80.0, false)
	assert_lt(far.cutoff, 4000.0)
	assert_eq(far.volume_db, 0.0, "distance alone makes it quieter: nothing is added")


func test_each_kind_of_place_has_a_bus_that_gives_it_its_sound() -> void:
	for kind: String in Mix.PLACED:
		for place: String in Mix.ROOMS:
			var bus := AudioServer.get_bus_index(Mix.treated(kind, place))
			assert_ne(bus, -1, kind + place)
			assert_eq(String(AudioServer.get_bus_send(bus)), kind, "so its kind's volume still turns it down")
			assert_eq(AudioServer.get_bus_effect_count(bus), 1)
			assert_true(AudioServer.get_bus_effect(bus, 0) is AudioEffectReverb)
	for place: String in Mix.ROOMS:
		assert_has(Acoustics.PLACES, place)
	assert_eq(Mix.treated(Mix.WEAPONS, OPEN), Mix.WEAPONS)
	assert_gt(Mix.ROOMS.Alley.room_size, Mix.ROOMS.Indoors.room_size, "an alley's walls are further apart than a room's")
	var count := AudioServer.bus_count
	Mix.build()
	assert_eq(AudioServer.bus_count, count, "made once")


func test_a_shot_in_the_tower_is_played_as_one_indoors_and_muffled_from_the_street() -> void:
	await _arena("city")
	var camera := Camera3D.new()
	world.add_child(camera)
	camera.make_current()
	# Heard from across the lobby.
	camera.global_position = Vector3(25, 2, -19)
	Sounds.play_at("blaster", Vector3(19, 2, -21), world)
	assert_eq(Sounds.last_treatment.bus, "WeaponsIndoors")
	assert_eq(Sounds.last_treatment.volume_db, 0.0)
	assert_eq(Sounds.last_treatment.cutoff, Acoustics.CLEAR_CUTOFF)
	var voice: AudioStreamPlayer3D = world.find_children("*", "AudioStreamPlayer3D", false, false)[-1]
	assert_eq(String(voice.bus), "WeaponsIndoors")
	# Heard from the street, through the tower's wall.
	camera.global_position = Vector3(8, 2, -21)
	Sounds.play_at("blaster", Vector3(22, 2, -22), world)
	assert_eq(Sounds.last_treatment.cutoff, Acoustics.MUFFLED_CUTOFF)
	assert_eq(Sounds.last_treatment.volume_db, Acoustics.MUFFLED_DB)
	# A step there is treated too, and a pickup's chime is not.
	Sounds.play_at("step", Vector3(22, 0, -20), world)
	assert_eq(Sounds.last_treatment.bus, "StepsIndoors")
	Sounds.play_at("pickup", Vector3(22, 0, -20), world)
	assert_eq(Sounds.last_treatment.bus, Mix.WORLD)


func test_a_shot_carries_further_than_a_step() -> void:
	await _arena("box")
	Sounds.play_at("rifle", Vector3(0, 2, 0), world)
	Sounds.play_at("step", Vector3(1, 0, 0), world)
	var voices: Array = world.find_children("*", "AudioStreamPlayer3D", false, false)
	var shot: AudioStreamPlayer3D = voices.filter(func(voice: AudioStreamPlayer3D) -> bool: return String(voice.bus).begins_with(Mix.WEAPONS))[0]
	var step: AudioStreamPlayer3D = voices.filter(func(voice: AudioStreamPlayer3D) -> bool: return String(voice.bus).begins_with(Mix.STEPS))[0]
	assert_gt(shot.max_distance, step.max_distance)
	assert_gt(shot.max_distance, Acoustics.FAR_TO, "far enough to be heard as a dull crack")


func test_every_surface_in_every_arena_has_steps() -> void:
	for kind: String in Textures.KINDS:
		assert_has(Footsteps.CUES, kind)
		assert_has(SoundBank.SOUNDS, Footsteps.cue(kind), kind)
		assert_eq(Mix.bus_of(Footsteps.cue(kind)), Mix.STEPS, kind)
		# And is written as footsteps for those who have captions on.
		assert_eq(preload("res://ui/captions.gd").TEXTS.get(Footsteps.cue(kind), ""), "Footsteps", kind)
	assert_eq(Footsteps.cue("nonsense"), "step")
	for id: String in Maps.ORDER + [Maps.RANGE]:
		await _arena(id)
		var scenery := String(world.map.get_meta("scenery", Scenery.MEADOW))
		for body in world.map.get_node("Geometry").get_children():
			var sound := Footsteps.cue(Textures.kind_for(body, scenery))
			assert_not_null(SoundBank.get_sound(sound), "%s: %s" % [id, body.name])
		# And wherever a player starts they are standing on something.
		for spot: Vector3 in world.spawn_positions():
			assert_ne(Footsteps.surface_under(world.map, spot), "", "%s at %s" % [id, spot])
		world.free()


func test_the_main_surfaces_each_sound_different() -> void:
	var heard := {}
	for kind: String in [Textures.ASPHALT, Textures.WOOD, Textures.ROCK, Textures.DIRT, Textures.CHEESE, Textures.METAL]:
		var sound := Footsteps.cue(kind)
		assert_false(heard.has(sound), "%s has its own" % kind)
		heard[sound] = true
		assert_gte(SoundBank.takes(sound), 3, "%s: several takes" % sound)
		for other: String in heard:
			if other != sound:
				assert_ne(SoundBank.get_sound(sound).data, SoundBank.get_sound(other).data)
	# Hard floors share one, and bread sounds as cheese does.
	assert_eq(Footsteps.cue(Textures.TILE), Footsteps.cue(Textures.ASPHALT))
	assert_eq(Footsteps.cue(Textures.BREAD), Footsteps.cue(Textures.CHEESE))


func test_what_is_underfoot_is_found() -> void:
	await _arena("box")
	assert_eq(Footsteps.surface_under(world.map, Vector3(0, 0, 0)), Textures.DIRT)
	assert_eq(Footsteps.surface_under(world.map, Vector3(0, 30, 0)), "", "nothing, in the air")
	assert_eq(Footsteps.surface_under(null, Vector3.ZERO), "")
	var block: Node3D = world.map.get_node("Geometry").get_children().filter(func(body: Node) -> bool: return String(body.name).begins_with("Block"))[0]
	var size: Vector3 = (block.get_node("CollisionShape3D").shape as BoxShape3D).size
	assert_eq(Footsteps.surface_under(world.map, block.global_position + Vector3.UP * size.y / 2.0), Textures.CHEESE)
	world.load_map("city")
	await wait_physics_frames(2)
	assert_eq(Footsteps.surface_under(world.map, Vector3(0, 0, 0)), Textures.ASPHALT)


func test_opponents_steps_are_the_clearest() -> void:
	var listener := PlayerRecord.new()
	var mate := PlayerRecord.new()
	var enemy := PlayerRecord.new()
	listener.team = MatchState.CHEDDAR
	mate.team = MatchState.CHEDDAR
	enemy.team = MatchState.BRIE
	assert_eq(Footsteps.relation(listener, listener, true), Footsteps.OWN)
	assert_eq(Footsteps.relation(listener, mate, true), Footsteps.TEAMMATE)
	assert_eq(Footsteps.relation(listener, enemy, true), Footsteps.OPPONENT)
	assert_eq(Footsteps.relation(listener, mate, false), Footsteps.OPPONENT, "without teams everyone else is one")
	assert_eq(Footsteps.relation(null, enemy, true), Footsteps.OPPONENT)
	assert_eq(Footsteps.relation(listener, null, true), Footsteps.OPPONENT)
	for running: bool in [false, true]:
		for crouch: float in [0.0, 1.0]:
			assert_gt(Footsteps.level(Footsteps.OPPONENT, running, crouch), Footsteps.level(Footsteps.TEAMMATE, running, crouch) + 5.0)
			assert_gt(Footsteps.level(Footsteps.OPPONENT, running, crouch), Footsteps.level(Footsteps.OWN, running, crouch))


func test_running_is_louder_and_crouching_much_quieter() -> void:
	var walking := Footsteps.level(Footsteps.OPPONENT, false, 0.0)
	assert_gt(Footsteps.level(Footsteps.OPPONENT, true, 0.0), walking)
	assert_almost_eq(Footsteps.level(Footsteps.OPPONENT, false, 1.0), walking + Footsteps.CROUCHED_DB, 0.001)
	assert_between(Footsteps.level(Footsteps.OPPONENT, false, 0.5), walking + Footsteps.CROUCHED_DB, walking)
	assert_eq(Footsteps.level(Footsteps.OPPONENT, false, 7.0), walking + Footsteps.CROUCHED_DB)
	assert_lt(Footsteps.level(Footsteps.OPPONENT, true, 1.0), walking, "a crouched run is still quieter than a walk")
	assert_eq(Footsteps.level("nonsense", false, 0.0), walking)
