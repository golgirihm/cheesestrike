extends GutTest
## The ways to play beyond knock-outs: how each is scored and won in the
## match state, and how the host and the arena carry them out.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")
const CHEDDAR := MatchState.CHEDDAR
const BRIE := MatchState.BRIE

var world: Node3D
var match_: Match


func _state(mode: String, ids := [1, 2]) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"mode": mode}))
	for id: int in ids:
		state.add_player(id)
	return state


func _start(mode: String, map := "box") -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map(map)
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": map, "mode": mode}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


# --- Settings ---------------------------------------------------------------

func test_every_mode_has_a_name_and_a_score_that_wins() -> void:
	for mode: String in MatchSettings.MODES:
		assert_true(MatchSettings.MODE_NAMES.has(mode), mode)
		var settings := MatchSettings.from_dict({"mode": mode, "kill_limit": 20})
		assert_gt(settings.limit(), 0, mode)
		assert_string_contains(settings.describe(), "first to %d" % settings.limit())
	assert_eq(MatchSettings.MODES.slice(0, 2), [MatchSettings.FREE_FOR_ALL, MatchSettings.TEAMS], "the first two as they were")
	assert_eq(Settings.CHOICES.get("host_mode", MatchSettings.MODES), MatchSettings.MODES)


func test_which_modes_are_played_in_teams_and_which_on_points() -> void:
	var teams := MatchSettings.MODES.filter(func(mode: String) -> bool: return MatchSettings.from_dict({"mode": mode}).is_teams())
	assert_eq(teams, MatchSettings.TEAM_MODES)
	for mode: String in MatchSettings.POINT_LIMITS:
		assert_true(MatchSettings.from_dict({"mode": mode}).by_points(), mode)
		assert_has(MatchSettings.TEAM_MODES, mode)
	assert_false(MatchSettings.from_dict({"mode": MatchSettings.TEAMS}).by_points())
	assert_eq(MatchSettings.from_dict({"mode": MatchSettings.GUN_GAME}).limit(), GunGame.LADDER.size())
	assert_eq(MatchSettings.from_dict({"mode": MatchSettings.TEAMS, "kill_limit": 30}).limit(), 30)


func test_the_session_list_is_told_a_mode_its_server_knows() -> void:
	for mode: String in MatchSettings.MODES:
		var settings := MatchSettings.from_dict({"mode": mode, "minutes": 5, "map": "city"})
		var told := settings.advertised()
		assert_has([MatchSettings.FREE_FOR_ALL, MatchSettings.TEAMS], told.mode, mode)
		assert_eq(told.mode == MatchSettings.TEAMS, settings.is_teams(), mode)
		assert_eq(told.minutes, 5)
		assert_eq(told.map, "city")
		assert_eq(settings.to_dict().mode, mode, "and players are told the real one")


# --- Points -----------------------------------------------------------------

func test_points_win_a_match_played_on_points() -> void:
	var state := _state(MatchSettings.CAPTURE)
	assert_false(state.score(BRIE, 1))
	assert_eq(state.team_score(BRIE), 1)
	assert_false(state.score(BRIE, 0), "nothing for nothing")
	assert_false(state.score(7, 1))
	assert_false(state.score(BRIE, 1))
	assert_true(state.score(BRIE, 1), "the third wins")
	assert_false(state.is_playing())
	assert_eq(state.winners, [BRIE] as Array[int])
	assert_eq(state.result_text(), "Brie wins!")
	assert_false(state.score(CHEDDAR, 5), "not between matches")


func test_knock_outs_do_not_win_a_match_played_on_points() -> void:
	var state := _state(MatchSettings.HILL)
	state.settings.kill_limit = 1
	state.damage(1, 2, 500.0)
	assert_true(state.is_playing())
	assert_eq(state.team_score(CHEDDAR), 0)
	assert_eq(state.team_kills[CHEDDAR], 1, "still counted for the board")


func test_time_running_out_goes_to_whoever_has_more_points() -> void:
	var state := _state(MatchSettings.POINTS)
	state.score(CHEDDAR, 7)
	state.score(BRIE, 9)
	state.tick(state.time_left)
	assert_eq(state.winners, [BRIE] as Array[int])


func test_a_new_match_clears_points_wheels_and_zones() -> void:
	var state := _state(MatchSettings.CAPTURE)
	state.score(BRIE, 2)
	state.capture.wheels[0].status = CaptureRules.DROPPED
	state.start_match()
	assert_eq(state.team_points, [0, 0])
	assert_eq(state.capture.wheels[0].status, CaptureRules.HOME)
	var held := _state(MatchSettings.POINTS)
	held.zones.owners[1] = BRIE
	held.start_match()
	assert_eq(held.zones.owners[1], ZoneRules.NOBODY)


func test_nothing_of_the_modes_own_runs_until_the_countdown_is_over() -> void:
	MatchState.countdown_length = MatchState.COUNTDOWN
	var spots := {"bases": [], "hills": [Vector3.ZERO, Vector3(9, 0, 0)], "points": []}
	var hill := _state(MatchSettings.HILL, [1, 2])
	var standing := {1: {"at": Vector3.ZERO, "team": CHEDDAR, "alive": true}}
	hill.update_objectives(ZoneRules.HILL_TIME - 1.0, spots, standing)
	assert_eq(hill.team_points, [0, 0], "nothing scored")
	assert_eq(hill.zones.hill_left, ZoneRules.HILL_TIME, "and the first hill keeps its full time")
	hill.tick(MatchState.COUNTDOWN)
	hill.update_objectives(1.0, spots, standing)
	assert_lt(hill.zones.hill_left, ZoneRules.HILL_TIME, "until the match begins")
	MatchState.countdown_length = 0.0


func test_points_and_the_modes_own_state_survive_the_trip() -> void:
	var state := _state(MatchSettings.CAPTURE)
	state.score(CHEDDAR, 2)
	state.capture.wheels[BRIE].status = CaptureRules.CARRIED
	state.capture.wheels[BRIE].carrier = 1
	var copy := MatchState.from_dict(state.to_dict())
	assert_eq(copy.team_points, [2, 0])
	assert_eq(copy.capture.carried_by(1), BRIE)
	var held := _state(MatchSettings.POINTS)
	held.zones.owners[2] = CHEDDAR
	assert_eq(MatchState.from_dict(held.to_dict()).zones.owners[2], CHEDDAR)
	var plain := MatchState.from_dict(MatchState.new().to_dict())
	assert_null(plain.capture)
	assert_null(plain.zones)
	assert_false(MatchState.new().to_dict().has("objective"), "nothing sent that the mode doesn't have")


# --- What the rules tell everyone ------------------------------------------

func test_a_capture_is_told_and_scored() -> void:
	var state := _state(MatchSettings.CAPTURE)
	state.player(1).name = "Hunter"
	var spots := {"bases": [Vector3(-20, 0, 0), Vector3(20, 0, 0)], "hills": [], "points": []}
	var standing := {1: {"at": Vector3(20, 0, 0), "team": CHEDDAR, "alive": true}}
	var told := state.update_objectives(0.1, spots, standing)
	assert_eq(told, [{"text": "Hunter has the Brie wheel!", "team": CHEDDAR, "by": 1, "points": Stats.TAKE_POINTS}] as Array[Dictionary])
	assert_eq(state.objective_text(1), "Cheddar wheel home · Brie wheel with Hunter")
	standing[1].at = Vector3(-20, 0, 0)
	told = state.update_objectives(0.1, spots, standing)
	assert_eq(told, [{"text": "Hunter scores for Cheddar!", "team": CHEDDAR, "by": 1, "points": Stats.CAPTURE_POINTS}] as Array[Dictionary])
	assert_eq(state.team_score(CHEDDAR), 1)
	state.end_match()
	assert_eq(state.update_objectives(0.1, spots, {1: {"at": Vector3(20, 0, 0), "team": CHEDDAR, "alive": true}}), [] as Array[Dictionary], "nothing between matches")


func test_the_hill_scores_and_says_when_it_moves() -> void:
	var state := _state(MatchSettings.HILL)
	var spots := {"bases": [], "hills": [Vector3.ZERO, Vector3(9, 0, 0)], "points": []}
	var standing := {2: {"at": Vector3.ZERO, "team": BRIE, "alive": true}}
	state.update_objectives(2.5, spots, standing)
	assert_eq(state.team_score(BRIE), 2)
	assert_eq(state.objective_text(1), "The hill moves in %ds" % ceili(ZoneRules.HILL_TIME - 2.5))
	var told := state.update_objectives(ZoneRules.HILL_TIME, spots, {})
	assert_eq(told, [{"text": "The hill has moved", "team": MatchState.NO_TEAM}] as Array[Dictionary])


func test_holding_the_hill_long_enough_wins() -> void:
	var state := _state(MatchSettings.HILL)
	var spots := {"bases": [], "hills": [Vector3.ZERO], "points": []}
	var standing := {2: {"at": Vector3.ZERO, "team": BRIE, "alive": true}}
	for second in MatchSettings.POINT_LIMITS[MatchSettings.HILL] + 1:
		state.update_objectives(1.0, spots, standing)
	assert_false(state.is_playing())
	assert_eq(state.winners, [BRIE] as Array[int])


func test_a_point_taken_is_told_and_scores_for_its_team() -> void:
	var state := _state(MatchSettings.POINTS)
	var spots := {"bases": [], "hills": [], "points": [Vector3(-9, 0, 0), Vector3.ZERO, Vector3(9, 0, 0)]}
	var standing := {1: {"at": Vector3(9, 0, 0), "team": CHEDDAR, "alive": true}}
	var told: Array[Dictionary] = []
	for step in int(ZoneRules.TAKE_TIME * 10.0) + 1:
		told.append_array(state.update_objectives(0.1, spots, standing))
	assert_eq(told, [{"text": "Cheddar takes point C", "team": CHEDDAR}] as Array[Dictionary])
	assert_eq(state.team_score(CHEDDAR), 0, "nothing while it was being taken")
	for step in 41:
		state.update_objectives(0.1, spots, {})
	assert_eq(state.team_score(CHEDDAR), 2)
	assert_eq(state.objective_text(2), "A – · B – · C Cheddar")


# --- Last mouse standing ----------------------------------------------------

func test_a_knocked_out_player_stays_out_until_the_round_is_over() -> void:
	var state := _state(MatchSettings.ROUNDS, [1, 2, 3, 4])
	var victim := 2 if state.player(2).team != state.player(1).team else 3
	state.damage(1, victim, 500.0)
	assert_false(state.player(victim).alive)
	assert_eq(state.tick(MatchState.RESPAWN_DELAY + 1.0), [] as Array[int], "nobody comes back")
	assert_eq(state.round_break, 0.0, "their team still has someone")
	assert_eq(state.team_points, [0, 0])


func test_the_last_team_with_anyone_left_takes_the_round() -> void:
	var state := _state(MatchSettings.ROUNDS)
	var team := state.player(1).team
	assert_ne(team, state.player(2).team)
	state.damage(1, 2, 500.0)
	assert_eq(state.team_score(team), 1)
	assert_eq(state.round_won, team)
	assert_eq(state.round_break, MatchState.ROUND_BREAK)
	assert_true(state.is_playing())
	assert_string_contains(state.objective_text(1), "Next round in")


func test_nobody_is_hurt_in_the_break_and_then_everyone_comes_back() -> void:
	var state := _state(MatchSettings.ROUNDS)
	state.damage(1, 2, 500.0)
	state.player(1).protected_for = 0.0
	assert_eq(state.damage(2, 1, 50.0).dealt, 0, "the round is over")
	assert_eq(state.tick(MatchState.ROUND_BREAK - 0.5), [] as Array[int])
	var due := state.tick(1.0)
	due.sort()
	assert_eq(due, [1, 2] as Array[int], "the winner too, to start the next from a fresh place")
	for id in due:
		state.respawn(id)
	assert_eq(state.round_break, 0.0)
	assert_true(state.player(2).alive)
	assert_eq(state.objective_text(1), "1 left of Cheddar · 1 of Brie")


func test_enough_rounds_win_the_match() -> void:
	var state := _state(MatchSettings.ROUNDS)
	var team := state.player(1).team
	for round in MatchSettings.POINT_LIMITS[MatchSettings.ROUNDS]:
		assert_true(state.is_playing())
		state.player(2).protected_for = 0.0
		var result := state.damage(1, 2, 500.0)
		assert_eq(result.ended, round == MatchSettings.POINT_LIMITS[MatchSettings.ROUNDS] - 1)
		for id in state.tick(MatchState.ROUND_BREAK):
			state.respawn(id)
	assert_false(state.is_playing())
	assert_eq(state.winners, [team] as Array[int])


func test_a_team_emptied_by_someone_leaving_loses_the_round_only_if_it_still_has_players() -> void:
	var state := _state(MatchSettings.ROUNDS, [1, 2, 3])
	# Two on one team, one on the other.
	var alone := 1
	for id: int in [1, 2, 3]:
		if [1, 2, 3].filter(func(other: int) -> bool: return state.player(other).team == state.player(id).team).size() == 1:
			alone = id
	state.remove_player(alone)
	assert_eq(state.round_break, 0.0, "a team with nobody in it hasn't lost a round")
	assert_eq(state.team_points, [0, 0])


func test_a_round_with_one_player_never_ends() -> void:
	var state := _state(MatchSettings.ROUNDS, [1])
	state.damage(1, 1, 500.0, true)
	assert_eq(state.round_break, 0.0)
	assert_eq(state.team_points, [0, 0])


func test_other_modes_bring_players_back_as_ever() -> void:
	var state := _state(MatchSettings.CAPTURE)
	state.damage(1, 2, 500.0)
	assert_eq(state.tick(MatchState.RESPAWN_DELAY), [2] as Array[int])


# --- Gun game ---------------------------------------------------------------

func test_the_ladder_is_every_weapon_once_ending_with_the_sidearm() -> void:
	var ladder := GunGame.LADDER.duplicate()
	ladder.sort()
	var all := Weapons.ids()
	all.sort()
	assert_eq(ladder, all)
	assert_eq(GunGame.LADDER[-1], Weapons.STARTING)
	assert_eq(GunGame.weapon_for(0), GunGame.LADDER[0])
	assert_eq(GunGame.weapon_for(999), GunGame.LADDER[-1])
	assert_eq(GunGame.weapon_for(-3), GunGame.LADDER[0])
	assert_eq(GunGame.text(2), "Weapon 3 of %d: %s" % [GunGame.LADDER.size(), Weapons.find(GunGame.LADDER[2]).display_name])


func test_everyone_starts_a_gun_game_with_the_first_weapon_alone() -> void:
	var state := _state(MatchSettings.GUN_GAME)
	state.player(1).main = "rifle"
	state.player(1).attachment = Attachments.GRIP
	state.rearm(state.player(1))
	assert_eq(state.player(1).weapons, [GunGame.LADDER[0]], "whatever their loadout")
	assert_eq(state.player(1).weapon, GunGame.LADDER[0])
	assert_eq(state.player(1).fitted, Attachments.NONE)
	assert_eq(state.player(2).weapons, [GunGame.LADDER[0]])
	assert_eq(state.objective_text(1), GunGame.text(0))


func test_a_knock_out_moves_its_scorer_on_and_leaves_the_other_where_they_were() -> void:
	var state := _state(MatchSettings.GUN_GAME)
	state.damage(1, 2, 500.0)
	assert_eq(state.player(1).weapons, [GunGame.LADDER[1]])
	assert_eq(state.player(1).weapon, GunGame.LADDER[1])
	assert_eq(state.player(2).weapons, [GunGame.LADDER[0]])
	state.damage(1, 1, 500.0, true)
	assert_eq(state.player(1).weapons, [GunGame.LADDER[1]], "knocking themselves out moves them nowhere")


func test_a_knock_out_with_the_last_weapon_wins() -> void:
	var state := _state(MatchSettings.GUN_GAME)
	state.settings.kill_limit = 3
	for rung in GunGame.LADDER.size():
		assert_true(state.is_playing(), "rung %d" % rung)
		assert_eq(state.player(1).weapon, GunGame.LADDER[rung])
		state.respawn(2)
		state.player(2).protected_for = 0.0
		state.damage(1, 2, 500.0)
	assert_false(state.is_playing())
	assert_eq(state.winners, [1] as Array[int])


func test_no_weapon_is_found_or_left_behind_in_a_gun_game() -> void:
	var state := _state(MatchSettings.GUN_GAME)
	state.set_pickup_count(2)
	assert_eq(state.collect(0, "launcher", 1), "")
	assert_true(state.pickup_ready(0), "it stays where it is")
	state.player(1).health = 40
	assert_eq(state.collect(1, Pickups.HEALTH, 1), "taken", "the rest is there to take")
	assert_eq(state.drop("blaster", Vector3.ZERO), 0)


# --- In the arena -----------------------------------------------------------

func test_every_arena_has_places_for_every_mode_on_open_floor() -> void:
	await _start(MatchSettings.CAPTURE)
	for id: String in Maps.ORDER:
		world.load_map(id)
		await wait_physics_frames(2)
		var spots: Dictionary = world.objective_spots()
		assert_eq(spots.bases.size(), 2, id)
		assert_gte(spots.hills.size(), 3, id)
		assert_eq(spots.points.size(), 3, id)
		assert_gt(spots.bases[0].distance_to(spots.bases[1]), 20.0, "%s: the bases are well apart" % id)
		var space := world.get_world_3d().direct_space_state
		for kind: String in spots:
			for spot: Vector3 in spots[kind]:
				# Floor under it, and room for a player to stand on it.
				var below := space.intersect_ray(PhysicsRayQueryParameters3D.create(spot + Vector3.UP * 0.5, spot + Vector3.DOWN * 0.5, 1))
				assert_false(below.is_empty(), "%s: %s at %s has no floor" % [id, kind, spot])
				var body := PhysicsShapeQueryParameters3D.new()
				var capsule := CapsuleShape3D.new()
				capsule.radius = BodyShape.RADIUS
				capsule.height = BodyShape.HEIGHT
				body.shape = capsule
				body.transform = Transform3D(Basis.IDENTITY, spot + Vector3.UP * (BodyShape.HEIGHT / 2.0 + 0.1))
				body.collision_mask = 1
				assert_eq(space.intersect_shape(body).size(), 0, "%s: %s at %s is inside something" % [id, kind, spot])


func test_the_host_carries_out_a_capture_and_everyone_is_told() -> void:
	await _start(MatchSettings.CAPTURE)
	var spots: Dictionary = world.objective_spots()
	var own := _add(1, spots.bases[0])
	_add(2, Vector3(0, 0, 6))
	var team: int = match_.state.player(1).team
	var theirs: Vector3 = spots.bases[1 - team]
	var ours: Vector3 = spots.bases[team]
	watch_signals(match_)
	own.global_position = theirs
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.capture.carried_by(1), 1 - team)
	assert_signal_emitted(match_, "announced")
	assert_eq(get_signal_parameters(match_, "announced")[1], team, "in the taker's colour")
	own.global_position = ours
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.team_score(team), 1)
	assert_signal_emit_count(match_, "announced", 2)
	assert_gt(Sounds.counts.get("objective", 0), 0)


func test_a_carrier_knocked_out_drops_the_wheel_where_they_fell() -> void:
	await _start(MatchSettings.CAPTURE)
	var spots: Dictionary = world.objective_spots()
	var own := _add(1, spots.bases[0])
	_add(2, Vector3(0, 0, 6))
	var team: int = match_.state.player(1).team
	own.global_position = spots.bases[1 - team]
	match_._physics_process(1.0 / 60.0)
	own.global_position = Vector3(3, 0, 3)
	match_._physics_process(1.0 / 60.0)
	match_.state.damage(2, 1, 500.0)
	match_._physics_process(1.0 / 60.0)
	var wheel: Dictionary = match_.state.capture.wheels[1 - team]
	assert_eq(wheel.status, CaptureRules.DROPPED)
	assert_almost_eq(wheel.at.x, 3.0, 0.01)


func test_the_arena_shows_bases_and_wheels_and_a_carried_wheel_rides_its_carrier() -> void:
	await _start(MatchSettings.CAPTURE)
	var spots: Dictionary = world.objective_spots()
	var own := _add(1, spots.bases[0])
	match_.state_changed.emit()
	var marks: Node3D = world.objective_marks
	assert_eq(marks.counts().bases, 2)
	assert_eq(marks.counts().wheels, 2)
	var team: int = match_.state.player(1).team
	var wheel: Node3D = marks._wheels[1 - team]
	assert_almost_eq(wheel.global_position.x, spots.bases[1 - team].x, 0.01, "at home")
	own.global_position = spots.bases[1 - team]
	match_._physics_process(1.0 / 60.0)
	own.global_position = Vector3(2, 0, 5)
	marks._process(0.0)
	assert_lt(Vector2(wheel.global_position.x - 2.0, wheel.global_position.z - 5.0).length(), BodyShape.RADIUS + 1.0, "with whoever has it")
	assert_gt(wheel.global_position.y, 0.8, "on their back")
	# Nothing drawn is solid.
	assert_eq(marks.find_children("*", "CollisionObject3D", true, false).size(), 0)


func test_the_arena_shows_only_the_hill_that_is_live() -> void:
	await _start(MatchSettings.HILL)
	match_.state_changed.emit()
	var marks: Node3D = world.objective_marks
	var places: int = world.objective_spots().hills.size()
	assert_eq(marks.counts().zones, places)
	assert_eq(marks.counts().shown, 1)
	assert_true(marks._zones[0].visible)
	match_.state.zones.hill = 2
	match_.state_changed.emit()
	assert_true(marks._zones[2].visible)
	assert_false(marks._zones[0].visible)


func test_the_arena_shows_three_points_in_their_owners_colours() -> void:
	await _start(MatchSettings.POINTS)
	match_.state_changed.emit()
	var marks: Node3D = world.objective_marks
	assert_eq(marks.counts().zones, 3)
	assert_eq(marks.counts().shown, 3)
	match_.state.zones.owners[1] = BRIE
	match_.state_changed.emit()
	assert_eq(marks._zones[1].get_meta("colour"), TeamColors.of(MatchState.BRIE))
	assert_eq(marks._zones[0].get_meta("colour"), marks.NEUTRAL)


func test_other_modes_draw_nothing() -> void:
	await _start(MatchSettings.TEAMS)
	match_.state_changed.emit()
	assert_eq(world.objective_marks.counts(), {"bases": 0, "wheels": 0, "zones": 0, "shown": 0})


func test_the_hud_shows_points_and_what_the_mode_is_up_to() -> void:
	await _start(MatchSettings.POINTS)
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	match_.state.score(BRIE, 12)
	match_.state.zones.owners[0] = BRIE
	hud._process(0.1)
	assert_string_contains(hud.match_shown(), "Cheddar 0  –  12 Brie")
	assert_eq(hud.objective_shown(), "A Brie · B – · C –")
	match_.announce("Brie takes point A", BRIE)
	hud._process(0.1)
	assert_eq(hud.announcement_shown(), "Brie takes point A")


func test_a_round_won_is_told_and_everyone_is_brought_back() -> void:
	await _start(MatchSettings.ROUNDS)
	var own := _add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	watch_signals(match_)
	match_.state.damage(1, 2, 500.0)
	match_._process(0.01)
	assert_signal_emitted(match_, "announced")
	assert_string_contains(get_signal_parameters(match_, "announced")[0], "takes the round")
	own.global_position = Vector3(1, 0, 1)
	match_._process(MatchState.ROUND_BREAK + 0.1)
	assert_true(match_.state.player(2).alive)
	assert_true(match_.state.player(1).alive)
	assert_eq(match_.state.player(1).health, PlayerRecord.MAX_HEALTH)


func test_a_gun_game_players_weapon_changes_in_their_hands() -> void:
	await _start(MatchSettings.GUN_GAME)
	var own := _add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	match_.state_changed.emit()
	assert_eq(own.weapon.stats.id, GunGame.LADDER[0])
	assert_eq(own.arsenal.keys(), [GunGame.LADDER[0]])
	match_._harm(1, 2, 500.0, Vector3.ZERO, GunGame.LADDER[0], false)
	match_.state_changed.emit()
	assert_eq(own.weapon.stats.id, GunGame.LADDER[1])
	assert_eq(own.arsenal.keys(), [GunGame.LADDER[1]])
	assert_eq(match_.state.drops.size(), 0)
