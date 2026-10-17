extends GutTest
## What players choose their characters to look like, and that none of it
## changes what can be hit.

const CHARACTER := preload("res://player/soldier_character.gd")
const WORLD := preload("res://world/world.tscn")

var character: Node3D


func before_each() -> void:
	character = CHARACTER.new()
	add_child_autofree(character)


func after_each() -> void:
	for key: String in Looks.SETTINGS:
		Settings.set_value(key, Looks.SETTINGS[key][0][0])


func _every_look() -> Array:
	var looks := []
	for cheese: String in Looks.CHEESES:
		for hat: String in Looks.HATS:
			for held: String in Looks.HELD:
				looks.append(Looks.pack(cheese, hat, held))
	return looks


func test_a_look_survives_being_packed_into_a_number() -> void:
	for cheese: String in Looks.CHEESES:
		for hat: String in Looks.HATS:
			for held: String in Looks.HELD:
				assert_eq(Looks.unpack(Looks.pack(cheese, hat, held)), {"finish": "plain", "title": "none", "level": 0, "cheese": cheese, "hat": hat, "held": held})
	assert_eq(Looks.pack("swiss", "none", "none"), 0, "the plain look is nothing at all")


func test_every_choice_has_a_name() -> void:
	for key: String in Looks.SETTINGS:
		var choice: Array = Looks.SETTINGS[key]
		for id: String in choice[0]:
			assert_true(choice[1].has(id), "%s: %s" % [key, id])
		assert_true(Settings.CHOICES.has(key), key)
		assert_eq(Settings.CHOICES[key], choice[0])


func test_a_look_from_another_machine_is_put_right() -> void:
	assert_eq(Looks.clean("fancy"), 0)
	assert_eq(Looks.clean(null), 0)
	assert_eq(Looks.clean(3.5), 0)
	assert_eq(Looks.unpack(Looks.clean(0xFFF)), {"finish": "plain", "title": "none", "level": 0, "cheese": "swiss", "hat": "none", "held": "none"}, "choices that don't exist become the plain ones")
	var wild := Looks.unpack(Looks.clean(-1))
	assert_eq([wild.cheese, wild.hat, wild.held, wild.title], ["swiss", "none", "none", "none"])
	assert_lte(wild.level, Looks.TOP_LEVEL, "and a level no higher than there is")
	var real := Looks.pack("blue", "crown", "flag")
	assert_eq(Looks.clean(real), real)
	assert_eq(Looks.clean(real | 1 << 27), real, "anything extra is dropped")
	assert_eq(Looks.unpack(99999).keys(), ["finish", "title", "level", "cheese", "hat", "held"])


func test_the_chosen_look_comes_from_the_settings() -> void:
	Settings.set_value("look_cheese", "brie")
	Settings.set_value("look_hat", "chef")
	Settings.set_value("look_held", "balloon")
	assert_eq(Looks.unpack(Looks.chosen()), {"finish": "plain", "title": "none", "level": Progress.level(), "cheese": "brie", "hat": "chef", "held": "balloon"})
	Settings.set_value("look_hat", "sombrero")
	assert_push_error("can't be sombrero")
	assert_eq(Settings.get_value("look_hat"), "chef", "a hat that doesn't exist isn't taken")


func test_one_thing_is_held_at_a_time() -> void:
	for held: String in Looks.HELD:
		character.wear(Looks.pack("swiss", "none", held))
		var carried := character.get_node_or_null("Held")
		assert_eq(carried == null, held == "none", held)
	character.wear(Looks.pack("swiss", "top", "flag"))
	character.wear(Looks.pack("swiss", "crown", "balloon"))
	await wait_process_frames(2)
	assert_eq(character.get_children().filter(func(child: Node) -> bool: return child.name.begins_with("Held")).size(), 1, "one at a time")


func test_what_is_held_stays_in_the_players_own_space() -> void:
	for look: int in _every_look():
		character.wear(look)
		var held := character.get_node_or_null("Held") as Node3D
		if held == null:
			continue
		for frame in 3:
			character._process(1.0 / 30.0)
		await wait_process_frames(1)
		var bounds := AABB()
		var meshes: Array = held.find_children("*", "MeshInstance3D", true, false)
		for index in meshes.size():
			var mesh: MeshInstance3D = meshes[index]
			var box: AABB = mesh.global_transform * mesh.get_aabb()
			bounds = box if index == 0 else bounds.merge(box)
		var named := str(Looks.unpack(look))
		assert_gte(bounds.position.y, -0.01, named)
		assert_lte(bounds.end.y, BodyShape.HEIGHT + 1.0, "%s: about the player's height" % named)
		assert_lte(maxf(absf(bounds.position.x), absf(bounds.end.x)), BodyShape.RADIUS + 0.5, "%s: by their side" % named)
		assert_lte(maxf(absf(bounds.position.z), absf(bounds.end.z)), BodyShape.RADIUS + 0.5, named)


func test_a_look_survives_the_trip_with_the_rest_of_the_record() -> void:
	var state := MatchState.new()
	state.add_player(1).look = Looks.pack("blue", "cowboy", "lollipop")
	var copy := MatchState.from_dict(state.to_dict())
	assert_eq(Looks.unpack(copy.player(1).look), {"finish": "plain", "title": "none", "level": 0, "cheese": "blue", "hat": "cowboy", "held": "lollipop"})
	var numbers := PackedInt32Array()
	numbers.resize(PlayerRecord._COUNT)
	numbers.fill(0)
	numbers[PlayerRecord._LOOK] = 0x7FFFFFFF
	assert_eq(PlayerRecord.from_packed(["X", numbers]).look, Looks.clean(0x7FFFFFFF), "and nonsense is put right on arrival")


func test_the_host_takes_a_players_look_before_or_after_they_join() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var match_: Match = world.match_
	match_.start()
	var early := Looks.pack("cheddar", "party", "none")
	match_._wear(5, early)
	assert_null(match_.state.player(5), "not here yet")
	match_.add_player(5)
	assert_eq(match_.state.player(5).look, early, "worn from the moment they arrive")
	watch_signals(match_)
	var later := Looks.pack("brie", "crown", "flag")
	match_._wear(5, later)
	assert_eq(match_.state.player(5).look, later)
	assert_signal_emitted(match_, "state_changed", "everyone is told")
	match_._wear(5, "a trilby")
	assert_eq(match_.state.player(5).look, 0, "nonsense is the plain look")


func test_everyone_sees_what_a_player_wears() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var match_: Match = world.match_
	match_.start()
	match_.add_player(2)
	var other: Node3D = world._create_player([2, Vector3.ZERO])
	world.players.add_child(other)
	assert_eq(other.body.worn().hat, "none")
	match_._wear(2, Looks.pack("blue", "chef", "balloon"))
	assert_eq(other.body.worn(), {"finish": "plain", "title": "none", "level": 0, "cheese": "blue", "hat": "chef", "held": "balloon"})
	assert_not_null(other.body.get_node_or_null("Held"))


func test_the_settings_say_what_is_chosen_and_step_through_the_choices() -> void:
	const MENU := preload("res://ui/settings_menu.gd")
	assert_eq(MENU.look_text("look_hat"), "Hat: None")
	Settings.set_value("look_hat", "crown")
	assert_eq(MENU.look_text("look_hat"), "Hat: Crown")
	assert_eq(MENU.look_text("look_cheese"), "Cheese: Swiss")
	assert_eq(MENU.next_choice(Looks.HATS, "none"), "party")
	assert_eq(MENU.next_choice(Looks.HATS, Looks.HATS[Looks.HATS.size() - 1]), "none", "and round again")
	var menu: Control = MENU.new()
	add_child_autofree(menu)
	menu.refresh()
	var list: OptionButton = menu.find_child("Setting_look_cheese", true, false)
	assert_eq(list.text, "Swiss")
	menu.choose("Setting_look_cheese", "cheddar")
	assert_eq(Settings.get_value("look_cheese"), "cheddar")
