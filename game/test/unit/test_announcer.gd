extends GutTest
## The announcer: what triggers each line.

const WORLD := preload("res://world/world.tscn")

var announcer: Announcer
var now: Dictionary


func before_each() -> void:
	announcer = Announcer.new()
	add_child_autofree(announcer)
	announcer.set_process(false)
	now = {"playing": false, "ended": false, "won": null, "leading": null, "teams": false, "time_left": 480.0, "sudden": false}
	announcer.update(now)


# What is said when the match comes to stand as it did but for `changed`.
func _then(changed: Dictionary) -> Array[String]:
	now = now.duplicate()
	now.merge(changed, true)
	return announcer.update(now)


func test_nothing_is_said_of_what_went_before_someone_arrived() -> void:
	var late := Announcer.new()
	autofree(late)
	assert_eq(late.update({"playing": true, "ended": false, "won": null, "leading": true, "teams": false, "time_left": 30.0, "sudden": true}), [])


func test_the_match_beginning_is_said_once() -> void:
	assert_eq(_then({"playing": true}), [Announcer.BEGINS])
	assert_eq(_then({}), [])
	assert_eq(_then({"time_left": 400.0}), [])


func test_the_end_says_who_it_was_for() -> void:
	_then({"playing": true})
	assert_eq(_then({"playing": false, "ended": true, "won": true}), [Announcer.VICTORY])
	assert_eq(_then({}), [], "once")
	# The next match, lost.
	_then({"ended": false, "won": null})
	_then({"playing": true})
	assert_eq(_then({"playing": false, "ended": true, "won": false}), [Announcer.DEFEAT])
	_then({"ended": false, "won": null, "playing": true})
	assert_eq(_then({"playing": false, "ended": true, "won": null}), [Announcer.DRAW])


func test_the_lead_changing_hands_is_said_each_way() -> void:
	_then({"playing": true})
	assert_eq(_then({"leading": true}), [Announcer.LEAD_TAKEN])
	assert_eq(_then({}), [], "still ahead: nothing more")
	assert_eq(_then({"leading": null}), [Announcer.LEAD_LOST], "caught up with")
	assert_eq(_then({"leading": false}), [], "and then passed: already said")
	assert_eq(_then({"leading": null}), [], "level again is not the lead")
	assert_eq(_then({"leading": true}), [Announcer.LEAD_TAKEN])
	assert_eq(_then({"leading": false}), [Announcer.LEAD_LOST])


func test_a_team_is_told_of_its_lead() -> void:
	_then({"playing": true, "teams": true})
	assert_eq(_then({"leading": true}), [Announcer.TEAM_LEAD_TAKEN])
	assert_eq(_then({"leading": false}), [Announcer.TEAM_LEAD_LOST])


func test_a_minute_from_time_is_said_as_it_is_reached() -> void:
	_then({"playing": true})
	assert_eq(_then({"time_left": 60.5}), [])
	assert_eq(_then({"time_left": 59.9}), [Announcer.MINUTE])
	assert_eq(_then({"time_left": 59.0}), [], "once")
	assert_eq(_then({"time_left": 0.0}), [])


func test_a_match_begun_with_under_a_minute_left_does_not_say_so() -> void:
	# As in sudden death, which is given a minute of its own.
	assert_eq(_then({"playing": true, "time_left": 45.0}), [Announcer.BEGINS])
	assert_eq(_then({"time_left": 44.0}), [])


func test_sudden_death_is_said_and_not_the_minute_it_is_given() -> void:
	_then({"playing": true})
	_then({"time_left": 0.5})
	assert_eq(_then({"sudden": true, "time_left": 60.0}), [Announcer.SUDDEN_DEATH])
	assert_eq(_then({"time_left": 59.0}), [])


func test_nothing_is_said_of_the_lead_or_the_time_in_the_warm_up() -> void:
	assert_eq(_then({"leading": true}), [])
	assert_eq(_then({"time_left": 30.0}), [])


func test_knock_outs_one_after_another_are_counted_up() -> void:
	assert_eq(announcer.knock_out(10.0), "", "the first is only a knock-out")
	assert_eq(announcer.knock_out(12.0), "Double knock-out")
	assert_eq(announcer.knock_out(15.0), "Triple knock-out")
	assert_eq(announcer.knock_out(18.0), "Rampage")
	assert_eq(announcer.knock_out(20.0), "Rampage", "and it goes on")
	# Too long after, the count starts again.
	assert_eq(announcer.knock_out(20.0 + Announcer.STREAK_WINDOW + 0.1), "")
	assert_eq(announcer.knock_out(20.0 + Announcer.STREAK_WINDOW * 2.0), "Double knock-out", "just in time")


func test_a_power_up_is_named_differently_to_whoever_took_it() -> void:
	for power: int in Powers.KINDS:
		assert_eq(Announcer.power_line(power, true), "%s!" % Powers.NAMES[power])
		assert_eq(Announcer.power_line(power, false), "%s is taken" % Powers.NAMES[power])
	assert_eq(Announcer.power_line(-7, true), "")


func _state(mode: String) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"mode": mode}))
	for id in [1, 2, 3]:
		state.add_player(id)
	state.start_match()
	return state


func test_how_a_free_for_all_stands_is_each_players_own() -> void:
	var state := _state(MatchSettings.FREE_FOR_ALL)
	assert_true(Announcer.standing(state, 1).playing)
	assert_null(Announcer.standing(state, 1).leading, "nothing scored")
	state.player(1).kills = 2
	state.player(2).kills = 1
	assert_eq(Announcer.standing(state, 1).leading, true)
	assert_eq(Announcer.standing(state, 2).leading, false)
	state.player(2).kills = 2
	assert_null(Announcer.standing(state, 1).leading, "level")
	assert_null(Announcer.standing(state, 2).leading)
	state.player(2).kills = 5
	state.end_match()
	assert_true(Announcer.standing(state, 2).ended)
	assert_eq(Announcer.standing(state, 2).won, true)
	assert_eq(Announcer.standing(state, 1).won, false)
	assert_null(Announcer.standing(state, 99).won, "someone who isn't in it")


func test_how_a_team_match_stands_goes_by_the_team() -> void:
	var state := _state(MatchSettings.TEAMS)
	state.player(1).team = MatchState.CHEDDAR
	state.player(2).team = MatchState.BRIE
	state.player(3).team = MatchState.CHEDDAR
	assert_true(Announcer.standing(state, 1).teams)
	state.team_kills[MatchState.CHEDDAR] = 1
	assert_eq(Announcer.standing(state, 1).leading, true)
	assert_eq(Announcer.standing(state, 3).leading, true, "though it was not their knock-out")
	assert_eq(Announcer.standing(state, 2).leading, false)
	state.end_match()
	assert_eq(Announcer.standing(state, 3).won, true)
	assert_eq(Announcer.standing(state, 2).won, false)


func test_a_drawn_match_is_won_by_nobody_and_someone_watching_is_told_neither() -> void:
	var state := _state(MatchSettings.FREE_FOR_ALL)
	state.player(1).kills = 3
	state.player(2).kills = 3
	state.end_match()
	assert_null(Announcer.standing(state, 1).won)
	state = _state(MatchSettings.FREE_FOR_ALL)
	state.player(3).watching = true
	state.player(1).kills = 3
	assert_null(Announcer.standing(state, 3).leading)
	state.end_match()
	assert_null(Announcer.standing(state, 3).won)


func test_in_a_match_the_lines_are_said_shown_and_kept() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	var heard := []
	world.announcer.said.connect(func(line: String, show: bool) -> void: heard.append([line, show]))
	# Alone it is a warm-up; with a second player the match begins.
	world.match_.add_player(2)
	world.match_.state.start_match()
	await wait_process_frames(3)
	assert_has(heard, [Announcer.BEGINS, false], "said, but the match writes it itself")
	world.match_.state.player(1).kills = 1
	await wait_process_frames(3)
	assert_has(heard, [Announcer.LEAD_TAKEN, true], "said and written")
	world.match_.hit_confirmed.emit(2, 100, true, false)
	world.match_.hit_confirmed.emit(2, 100, true, false)
	assert_has(heard, ["Double knock-out", true])
	world.match_.hit_confirmed.emit(2, 30, false, false)
	assert_eq(heard.size(), 3, "a hit that doesn't knock out says nothing")
	world.match_.power_taken.emit(1, Powers.KINDS[0])
	assert_eq(heard[-1], ["%s!" % Powers.NAMES[Powers.KINDS[0]], false])
	world.match_.power_taken.emit(2, Powers.KINDS[0])
	assert_eq(heard[-1][0], "%s is taken" % Powers.NAMES[Powers.KINDS[0]])
	assert_eq(world.announcer.spoken.slice(-2), [heard[-2][0], heard[-1][0]])
	world.match_.state.end_match()
	await wait_process_frames(3)
	assert_eq(heard[-1], [Announcer.VICTORY, true])


func test_the_voice_has_a_switch_and_follows_the_volume() -> void:
	var saved := {}
	for key: String in [Announcer.SETTING, "master_volume", "interface_volume"]:
		saved[key] = Settings.get_value(key)
	assert_eq(Settings.DEFAULTS[Announcer.SETTING], true)
	Settings.set_value(Announcer.SETTING, false)
	assert_false(Announcer.can_speak(), "never with the voice turned off")
	Settings.set_value("master_volume", 0.5)
	Settings.set_value("interface_volume", 0.5)
	assert_eq(Announcer.voice_volume(), 25)
	Settings.set_value("interface_volume", 0.0)
	assert_eq(Announcer.voice_volume(), 0)
	var menu: Control = preload("res://ui/settings_menu.gd").new()
	add_child_autofree(menu)
	assert_not_null(menu.find_child("Setting_" + Announcer.SETTING, true, false))
	# Turned off, the lines are still decided and shown: only not spoken.
	watch_signals(announcer)
	announcer._say(Announcer.MINUTE)
	assert_signal_emitted_with_parameters(announcer, "said", [Announcer.MINUTE, true])
	for key: String in saved:
		Settings.set_value(key, saved[key])
