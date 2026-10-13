extends GutTest
## Outlines on teammates, through walls, and on pickups close by. Above
## all: never on an enemy.

const WORLD := preload("res://world/world.tscn")

var world: Node3D
var saved: bool


func before_each() -> void:
	saved = Settings.get_value(Outlines.SETTING)
	Settings.set_value(Outlines.SETTING, true)


func after_each() -> void:
	Settings.set_value(Outlines.SETTING, saved)


func _record(team: int, alive := true) -> PlayerRecord:
	var record := PlayerRecord.new()
	record.team = team
	record.alive = alive
	return record


func _start(mode: String) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box", "mode": mode}))
	await wait_physics_frames(2)


# Adds player `id` on `team`, standing at `at`.
func _add(id: int, team: int, at: Vector3) -> Node3D:
	world.match_.add_player(id)
	world.match_.state.player(id).team = team
	var player: Node3D = world._create_player([id, at])
	world.players.add_child(player)
	player.global_position = at
	return player


func test_only_a_living_teammate_is_outlined() -> void:
	var cheddar := MatchState.CHEDDAR
	var brie := MatchState.BRIE
	var own := _record(cheddar)
	assert_true(Outlines.shows(own, _record(cheddar), true))
	assert_false(Outlines.shows(own, _record(brie), true), "never an enemy")
	assert_false(Outlines.shows(own, _record(cheddar, false), true), "nor one who is knocked out")
	assert_false(Outlines.shows(own, own, true), "nor oneself")
	assert_false(Outlines.shows(own, null, true))
	assert_false(Outlines.shows(null, _record(cheddar), true), "nor anyone, to someone only watching")


func test_nobody_is_outlined_where_everyone_is_an_enemy() -> void:
	# Without teams every record's team is the same "none": they are not
	# teammates for it.
	var none := MatchState.NO_TEAM
	assert_false(Outlines.shows(_record(none), _record(none), false))
	assert_false(Outlines.shows(_record(none), _record(none), true))
	assert_false(Outlines.shows(_record(MatchState.CHEDDAR), _record(MatchState.CHEDDAR), false))


func test_an_enemy_never_gets_one_whoever_is_looking_and_however_they_stand() -> void:
	for mode: String in MatchSettings.MODES:
		var teams: bool = MatchSettings.from_dict({"mode": mode}).is_teams()
		for viewer_team: int in [MatchState.NO_TEAM, MatchState.CHEDDAR, MatchState.BRIE]:
			for other_team: int in [MatchState.NO_TEAM, MatchState.CHEDDAR, MatchState.BRIE]:
				for alive: bool in [true, false]:
					var enemy: bool = not teams or viewer_team != other_team or viewer_team == MatchState.NO_TEAM
					if enemy:
						assert_false(Outlines.shows(_record(viewer_team), _record(other_team, alive), teams), "%s: %d sees %d" % [mode, viewer_team, other_team])


func test_a_pickup_is_outlined_only_when_near_and_there_to_take() -> void:
	assert_true(Outlines.pickup_shows(3.0, true))
	assert_true(Outlines.pickup_shows(Outlines.PICKUP_REACH, true))
	assert_false(Outlines.pickup_shows(Outlines.PICKUP_REACH + 0.1, true))
	assert_false(Outlines.pickup_shows(3.0, false), "one that has been taken")


func test_an_outline_goes_on_every_mesh_and_comes_off_again() -> void:
	var thing := Node3D.new()
	add_child_autofree(thing)
	var meshes: Array[MeshInstance3D] = []
	for index in 3:
		var mesh := MeshInstance3D.new()
		mesh.mesh = BoxMesh.new()
		(thing if index == 0 else meshes[index - 1]).add_child(mesh)
		meshes.append(mesh)
	assert_null(Outlines.colour_on(thing))
	Outlines.put(thing, Color.RED, true)
	assert_eq(Outlines.colour_on(thing), Color.RED)
	assert_true(Outlines.through_walls(thing))
	for mesh in meshes:
		assert_not_null(mesh.material_overlay, "however deep it is")
	# The fatter copy is drawn only where the thing is not, whatever hides it.
	var mark: StandardMaterial3D = meshes[0].material_overlay
	var hull: StandardMaterial3D = mark.next_pass
	assert_eq(mark.albedo_color.a, 0.0, "the mark itself is not seen")
	assert_eq(hull.albedo_color, Color.RED)
	assert_true(hull.no_depth_test)
	assert_almost_eq(hull.grow_amount, Outlines.THICKNESS, 0.0001)
	assert_eq(hull.stencil_compare, BaseMaterial3D.STENCIL_COMPARE_NOT_EQUAL)
	assert_eq(hull.stencil_reference, mark.stencil_reference)
	assert_gt(hull.render_priority, mark.render_priority, "drawn after every mark")
	# One that doesn't show through walls is hidden by what is in front.
	Outlines.put(thing, Color.RED, false)
	assert_false(Outlines.through_walls(thing))
	assert_false((meshes[0].material_overlay.next_pass as StandardMaterial3D).no_depth_test)
	assert_eq(meshes[0].material_overlay.stencil_flags & BaseMaterial3D.STENCIL_FLAG_WRITE_DEPTH_FAIL, 0)
	Outlines.take(thing)
	assert_null(Outlines.colour_on(thing))
	for mesh in meshes:
		assert_null(mesh.material_overlay)
	Outlines.take(thing)
	assert_lt(Outlines.THICKNESS, 0.08, "thin")


func test_in_a_team_match_teammates_are_outlined_in_the_teams_colour_and_enemies_are_not() -> void:
	await _start(MatchSettings.TEAMS)
	var own_team: int = world.match_.own_record().team
	var other_team := MatchState.BRIE if own_team == MatchState.CHEDDAR else MatchState.CHEDDAR
	var mate := _add(5, own_team, Vector3(4, 0.1, 0))
	var enemy := _add(6, other_team, Vector3(-4, 0.1, 0))
	world.show_outlines()
	assert_eq(Outlines.colour_on(mate.body), TeamColors.of(own_team))
	assert_true(Outlines.through_walls(mate.body), "so they are never lost behind cover")
	assert_null(Outlines.colour_on(enemy.body))
	assert_null(Outlines.colour_on(world.player_node(1).body), "nor on oneself")
	for mesh: MeshInstance3D in enemy.body.find_children("*", "MeshInstance3D", true, false):
		assert_null(mesh.material_overlay, "not on any part of an enemy")
	# A teammate who changes sides, or is knocked out, loses it.
	world.match_.state.player(5).team = other_team
	world.show_outlines()
	assert_null(Outlines.colour_on(mate.body))
	world.match_.state.player(5).team = own_team
	world.match_.state.player(5).alive = false
	world.show_outlines()
	assert_null(Outlines.colour_on(mate.body))
	world.match_.state.player(5).alive = true
	world.show_outlines()
	assert_not_null(Outlines.colour_on(mate.body))
	# And it keeps up without being asked.
	world.match_.state.player(5).team = other_team
	await wait_seconds(0.4)
	assert_null(Outlines.colour_on(mate.body))


func test_in_a_free_for_all_nobody_is_outlined() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var other := _add(5, MatchState.NO_TEAM, Vector3(4, 0.1, 0))
	world.show_outlines()
	assert_null(Outlines.colour_on(other.body))


func test_pickups_near_the_player_are_outlined_and_far_or_taken_ones_are_not() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var own: Node3D = world.player_node(1)
	var pads: Array = world.map.get_node("Pickups").get_children()
	own.global_position = pads[0].global_position + Vector3(2, 0, 0)
	world.show_outlines()
	assert_eq(Outlines.colour_on(pads[0].get_node("Item")), Outlines.PICKUP_COLOUR)
	assert_false(Outlines.through_walls(pads[0].get_node("Item")), "what is behind cover stays hidden")
	for pad: Node3D in pads:
		var near: bool = own.global_position.distance_to(pad.global_position) <= Outlines.PICKUP_REACH
		assert_eq(Outlines.colour_on(pad.get_node("Item")) != null, near and pad.get_node("Item").visible, pad.name)
	pads[0].get_node("Item").visible = false
	world.show_outlines()
	assert_null(Outlines.colour_on(pads[0].get_node("Item")))
	pads[0].get_node("Item").visible = true
	own.global_position = pads[0].global_position + Vector3(0, 200, 0)
	world.show_outlines()
	for pad: Node3D in pads:
		assert_null(Outlines.colour_on(pad.get_node("Item")), pad.name)


func test_the_setting_turns_every_outline_off() -> void:
	assert_eq(Settings.DEFAULTS[Outlines.SETTING], true)
	await _start(MatchSettings.TEAMS)
	var mate := _add(5, world.match_.own_record().team, Vector3(4, 0.1, 0))
	var own: Node3D = world.player_node(1)
	var pad: Node3D = world.map.get_node("Pickups").get_child(0)
	own.global_position = pad.global_position + Vector3(2, 0, 0)
	world.show_outlines()
	assert_not_null(Outlines.colour_on(mate.body))
	assert_not_null(Outlines.colour_on(pad.get_node("Item")))
	Settings.set_value(Outlines.SETTING, false)
	world.show_outlines()
	assert_null(Outlines.colour_on(mate.body))
	assert_null(Outlines.colour_on(pad.get_node("Item")))
	var menu: Control = preload("res://ui/settings_menu.gd").new()
	add_child_autofree(menu)
	assert_not_null(menu.find_child("Setting_" + Outlines.SETTING, true, false))


func test_once_the_match_is_left_nothing_is_outlined() -> void:
	await _start(MatchSettings.TEAMS)
	var pad: Node3D = world.map.get_node("Pickups").get_child(0)
	world.player_node(1).global_position = pad.global_position + Vector3(2, 0, 0)
	world.show_outlines()
	assert_not_null(Outlines.colour_on(pad.get_node("Item")))
	world.stop()
	world.show_outlines()
	for shown: Node3D in world.map.get_node("Pickups").get_children():
		assert_null(Outlines.colour_on(shown.get_node("Item")))
