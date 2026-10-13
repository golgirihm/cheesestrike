extends GutTest
## Medals, what each player has done in a match, and what the end of a match
## says of it.

const WORLD := preload("res://world/world.tscn")
const MAIN := preload("res://main.tscn")

var medals: Medals
var saved := {}


func before_each() -> void:
	medals = Medals.new()
	saved["seen_help"] = Settings.get_value("seen_help")
	Settings.set_value("seen_help", true)


func after_each() -> void:
	Settings.set_value("seen_help", saved["seen_help"])


func test_two_knock_outs_one_after_another_is_a_double_and_more_a_triple() -> void:
	assert_eq(medals.knock_out(2, 10.0, 5.0, false), [])
	assert_eq(medals.knock_out(3, 12.0, 5.0, false), [Medals.DOUBLE])
	assert_eq(medals.knock_out(2, 14.0, 5.0, false), [Medals.TRIPLE])
	assert_eq(medals.knock_out(3, 16.0, 5.0, false), [Medals.TRIPLE], "and each one after")
	# Too long after, it starts again.
	assert_eq(medals.knock_out(2, 16.0 + Medals.QUICK + 0.1, 5.0, false), [Medals.STREAK], "the fifth of the run, but not quick")
	assert_eq(medals.knock_out(3, 16.0 + Medals.QUICK * 2.0, 5.0, false), [Medals.DOUBLE])


func test_a_run_without_being_knocked_out_is_called_at_five_and_ten() -> void:
	var called := []
	for count in range(1, 12):
		for medal in medals.knock_out(2, count * 100.0, 5.0, false):
			called.append([count, medal])
	assert_eq(called, [[Medals.RUN, Medals.STREAK], [Medals.LONG_RUN, Medals.UNSTOPPABLE]])
	assert_eq(medals.run(), 11)
	medals.knocked_out(2)
	assert_eq(medals.run(), 0, "being knocked out ends it")
	for count in range(1, Medals.RUN):
		assert_does_not_have(medals.knock_out(3, 5000.0 + count * 100.0, 5.0, false), Medals.STREAK)
	assert_has(medals.knock_out(3, 9000.0, 5.0, false), Medals.STREAK, "and five more are a run again")


func test_a_knock_out_from_far_away_is_a_long_shot() -> void:
	assert_eq(medals.knock_out(2, 10.0, Medals.FAR - 0.1, false), [])
	assert_eq(medals.knock_out(2, 100.0, Medals.FAR, false), [Medals.LONG_SHOT])
	assert_eq(medals.knock_out(2, 200.0, 90.0, true), [Medals.LONG_SHOT, Medals.HEADSHOT], "and one to the head has its own")


func test_the_first_after_a_bad_spell_is_a_comeback() -> void:
	for time in Medals.BAD_SPELL - 1:
		medals.knocked_out(2)
	assert_eq(medals.knock_out(3, 10.0, 5.0, false), [], "not a bad enough spell")
	for time in Medals.BAD_SPELL:
		medals.knocked_out(0)
	assert_eq(medals.knock_out(3, 100.0, 5.0, false), [Medals.COMEBACK])
	assert_eq(medals.knock_out(3, 200.0, 5.0, false), [], "only the first")


func test_knocking_out_whoever_last_knocked_you_out_is_revenge() -> void:
	medals.knocked_out(7)
	assert_eq(medals.knock_out(3, 10.0, 5.0, false), [], "someone else")
	assert_eq(medals.knock_out(7, 100.0, 5.0, false), [Medals.REVENGE])
	assert_eq(medals.knock_out(7, 200.0, 5.0, false), [], "once")
	# Nobody to take it on after knocking oneself out.
	medals.knocked_out(0)
	assert_eq(medals.knock_out(0, 300.0, 5.0, false), [])
	# A bot can be had revenge on too.
	medals.knocked_out(-3)
	assert_has(medals.knock_out(-3, 400.0, 5.0, false), Medals.REVENGE)


func test_what_was_earned_is_counted_and_reads_in_a_line() -> void:
	assert_eq(medals.summary(), "")
	medals.knock_out(2, 10.0, 60.0, false)
	medals.knock_out(3, 11.0, 60.0, false)
	assert_eq(medals.earned, {Medals.LONG_SHOT: 2, Medals.DOUBLE: 1})
	assert_eq(medals.summary(), "Double knock-out, Long shot ×2")
	medals.clear()
	assert_eq(medals.earned, {})
	assert_eq(medals.run(), 0)
	assert_eq(medals.knock_out(2, 11.5, 5.0, false), [], "and nothing carries over")


func test_every_medal_has_a_name_and_an_icon() -> void:
	for medal: String in Medals.ALL:
		assert_has(Medals.NAMES, medal)
		assert_true(Icons.has(Medals.ICONS[medal]), medal)
	for icon: String in Icons.MEDALS:
		assert_has(Medals.ICONS.values(), icon)
	assert_not_null(SoundBank.get_sound("medal"))
	assert_eq(Mix.bus_of("medal"), Mix.INTERFACE)


func test_stats_count_damage_hits_and_what_was_fired() -> void:
	var stats := Stats.new()
	assert_eq(stats.of(1), Stats.blank())
	for shot in 10:
		stats.shot(1)
	stats.hit(1, 2, 30, "blaster", false, 1.0)
	stats.hit(1, 2, 45, "blaster", false, 2.0)
	stats.hit(1, 1, 50, "launcher", false, 3.0)
	stats.hit(1, 2, 0, "blaster", false, 4.0)
	var line := stats.of(1)
	assert_eq(line.damage, 75, "not what they did to themselves, nor a hit that did nothing")
	assert_eq(line.hits, 2)
	assert_eq(line.shots, 10)
	assert_eq(Stats.accuracy(line), 20)
	assert_eq(Stats.accuracy(Stats.blank()), 0, "nothing fired")
	# A shotgun's one shot can hit many times: never over a hundred.
	var pellets := Stats.blank()
	pellets.shots = 2
	pellets.hits = 9
	assert_eq(Stats.accuracy(pellets), 100)
	assert_true(stats.changed)


func test_whoever_hurt_someone_lately_helped_knock_them_out() -> void:
	var stats := Stats.new()
	stats.hit(2, 9, 40, "pistol", false, 10.0)
	stats.hit(3, 9, 10, "pistol", false, 2.0)
	stats.hit(1, 9, 50, "blaster", true, 14.0)
	assert_eq(stats.of(2).assists, 1)
	assert_eq(stats.of(3).assists, 0, "too long before")
	assert_eq(stats.of(1).assists, 0, "the one who did it is not helping")
	# Once they are knocked out, the helping is over with.
	stats.knocked_out(9)
	stats.hit(1, 9, 100, "blaster", true, 15.0)
	assert_eq(stats.of(2).assists, 1)


func test_stats_keep_the_best_run_and_the_favourite_weapon() -> void:
	var stats := Stats.new()
	for knock in 3:
		stats.hit(1, 2, 100, "blaster", true, knock)
	stats.knocked_out(1)
	stats.hit(1, 2, 100, "shotgun", true, 10.0)
	stats.hit(1, 2, 100, Melee.ID, true, 11.0)
	var line := stats.of(1)
	assert_eq(line.best, 3)
	assert_eq(line.streak, 2)
	assert_eq(Stats.favourite(line), "blaster")
	assert_eq(Stats.favourite(Stats.blank()), "")


func test_stats_travel_and_what_is_not_stats_is_left_out() -> void:
	var stats := Stats.new()
	stats.shot(1)
	stats.hit(1, 2, 80, "rifle", true, 1.0)
	stats.objective(2, 7.4)
	var back := Stats.from_array(bytes_to_var(var_to_bytes(stats.to_array())))
	assert_eq(back.of(1).damage, 80)
	assert_eq(back.of(1).with, {"rifle": 1})
	assert_eq(back.of(2).objective, 7.0)
	assert_eq(Stats.from_array("nonsense").to_array(), [])
	assert_eq(Stats.from_array(null).to_array(), [])
	var junk := [[1, 5], "x", [1, -5, 0, 0, 0, 0, 0, 0, {}], [1, 5, 0, 0, 0, 0, 0, 0, "x"], [2, 5, 0, 0, 1, 1, 0, 0, {"kazoo": 3, "rifle": "x", 7: 1, "pistol": 2}]]
	var kept := Stats.from_array(junk)
	assert_eq(kept.to_array().size(), 1)
	assert_eq(kept.of(2).with, {"pistol": 2}, "only weapons there are")
	var many := []
	for id in 200:
		many.append([id, 0, 0, 0, 0, 0, 0, 0, {}])
	assert_eq(Stats.from_array(many).to_array().size(), Stats.MOST)
	stats.forget(1)
	assert_eq(stats.of(1), Stats.blank())
	stats.clear()
	assert_eq(stats.to_array(), [])


func test_the_host_keeps_stats_as_the_match_is_played() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	var match_: Match = world.match_
	match_.add_player(2)
	match_.state.start_match()
	watch_signals(match_)
	match_._harm(1, 2, 60.0, Vector3.ZERO, "blaster", false)
	assert_eq(match_.stats.of(1).damage, 60)
	match_._harm(1, 2, 60.0, Vector3.ZERO, "blaster", true)
	assert_eq(match_.stats.of(1).with, {"blaster": 1})
	assert_eq(match_.stats.of(1).best, 1)
	# Sent with the ping, when there is something new.
	match_._since_ping = Match.PING_INTERVAL
	match_._process(0.02)
	assert_signal_emitted(match_, "stats_changed")
	assert_false(match_.stats.changed)
	# A client takes what it is sent.
	match_._stats_from([[5, 300, 2, 0, 10, 4, 1, 3, {"rifle": 3}]])
	assert_eq(match_.stats.of(5).damage, 300)
	match_._stats_from("nonsense")
	assert_eq(match_.stats.to_array(), [])


func test_standing_on_the_hill_counts_for_whoever_stands_there() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box", "mode": MatchSettings.HILL}))
	await wait_physics_frames(2)
	var match_: Match = world.match_
	match_.add_player(2)
	match_.state.start_match()
	var hill: Vector3 = world.objective_spots().hills[match_.state.zones.hill]
	world.player_node(1).global_position = hill + Vector3(1, 0.05, 0)
	for step in 60:
		match_._physics_process(1.0 / 60.0)
	assert_almost_eq(match_.stats.of(1).objective, 1.0, 0.1, "a second on it")
	world.player_node(1).global_position = hill + Vector3(ZoneRules.RADIUS + 4.0, 0.05, 0)
	for step in 60:
		match_._physics_process(1.0 / 60.0)
	assert_almost_eq(match_.stats.of(1).objective, 1.0, 0.1, "and nothing off it")


func _state() -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.FREE_FOR_ALL}))
	var scores := {1: ["Brie", 4, 2], 2: ["Edam", 9, 1], 3: ["Feta", 4, 1], 4: ["Gouda", 0, 7], 5: ["Havarti", 20, 0]}
	for id: int in scores:
		var record := state.add_player(id)
		record.name = scores[id][0]
		record.kills = scores[id][1]
		record.deaths = scores[id][2]
	state.player(5).watching = true
	return state


func test_the_podium_is_the_three_who_did_best() -> void:
	var state := _state()
	var podium := MatchSummary.podium(state)
	assert_eq(podium.map(func(stood: Dictionary) -> String: return stood.name), ["Edam", "Feta", "Brie"], "fewer times knocked out breaks a tie; someone watching isn't placed")
	assert_eq(podium.map(func(stood: Dictionary) -> String: return stood.place), ["1st", "2nd", "3rd"])
	assert_eq(MatchSummary.podium_lines(state)[0], "1st   Edam   ·   9")
	# With two playing there are two places.
	state.remove_player(1)
	state.remove_player(4)
	assert_eq(MatchSummary.podium(state).size(), 2)


func test_a_players_numbers_read_plainly() -> void:
	var line := Stats.blank()
	line.shots = 40
	line.hits = 13
	line.damage = 870
	line.best = 4
	assert_eq(MatchSummary.numbers(line), ["Accuracy 33%", "Damage 870", "Best run 4"])
	line.assists = 2
	line.with = {"rifle": 3, "pistol": 1}
	assert_eq(MatchSummary.numbers(line), ["Accuracy 33%", "Damage 870", "Best run 4", "Assists 2", "Favourite: %s" % Weapons.find("rifle").display_name])
	line.with = {Melee.ID: 2}
	assert_eq(MatchSummary.numbers(line)[-1], "Favourite: %s" % Melee.NAME)


func test_the_end_of_the_match_says_who_did_best_and_what_you_did() -> void:
	var state := _state()
	var stats := Stats.new()
	stats.shot(1)
	stats.hit(1, 2, 120, "blaster", true, 1.0)
	medals.knock_out(2, 1.0, 80.0, false)
	var lines := MatchSummary.lines(state, 1, stats, medals)
	assert_eq(lines.size(), 5)
	assert_string_starts_with(lines[0], "1st   Edam")
	assert_eq(lines[3], "Accuracy 100%   ·   Damage 120   ·   Best run 1   ·   Favourite: Cheddar Blaster")
	assert_eq(lines[4], "Medals: Long shot")
	# Someone watching sees the podium and no numbers of their own.
	assert_eq(MatchSummary.lines(state, 5, stats, Medals.new()).size(), 3)
	assert_eq(MatchSummary.lines(state, 99, null, null).size(), 3)


func test_a_medal_is_called_out_on_the_screen_one_at_a_time() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(3)
	main.world.start(MatchSettings.from_dict({"map": "box"}))
	main.world.match_.add_player(2)
	main.world.match_.state.start_match()
	var enemy: Node3D = main.world._create_player([2, Vector3(0, 0.1, -60)])
	main.world.players.add_child(enemy)
	enemy.global_position = main.world.player_node(1).global_position + Vector3(0, 0, -(Medals.FAR + 5.0))
	var hud: Control = main.combat_hud
	hud._process(0.016)
	assert_eq(hud.medal_shown(), "")
	var sounds: int = Sounds.counts.get("medal", 0)
	main.world.match_.hit_confirmed.emit(2, 100, true, true)
	hud._process(0.016)
	assert_eq(hud.medal_shown(), "Long shot")
	assert_true(hud.find_child("Medal", true, false).visible)
	assert_eq(Sounds.counts.get("medal", 0), sounds + 1)
	hud._process(hud.MEDAL_LIFE + 0.1)
	hud._process(0.016)
	assert_eq(hud.medal_shown(), "Between the eyes", "the next, once the first has had its moment")
	hud._process(hud.MEDAL_LIFE + 0.1)
	hud._process(0.016)
	assert_eq(hud.medal_shown(), "")
	assert_eq(hud.medals.earned, {Medals.LONG_SHOT: 1, Medals.HEADSHOT: 1})
	# A hit that doesn't knock out earns nothing.
	main.world.match_.hit_confirmed.emit(2, 10, false, true)
	hud._process(0.016)
	assert_eq(hud.medal_shown(), "")
	# Being knocked out ends the run, and a new match clears the count.
	main.world.match_.state.player(1).alive = false
	main.world.match_.state.player(1).last_attacker = 2
	hud._process(0.016)
	main.world.match_.state.player(1).alive = true
	hud._process(0.016)
	main.world.match_.hit_confirmed.emit(2, 100, true, false)
	assert_has(hud.medals.earned, Medals.REVENGE)
	main.world.match_.state.end_match()
	hud._process(0.016)
	main.world.match_.state.start_match()
	hud._process(0.016)
	assert_eq(hud.medals.earned, {})
	main.world.stop()
