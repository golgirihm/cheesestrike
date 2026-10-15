extends GutTest
## What is kept between visits: the report of a match, the record it adds
## to, levels, the list of past matches, and the page that shows them.

const PROGRESS := preload("res://progress/progress.gd")
const WORLD := preload("res://world/world.tscn")
const MAIN := preload("res://main.tscn")
const PATH := "user://test_progress.json"

var saved := {}


func before_each() -> void:
	DirAccess.remove_absolute(PATH)
	for key: String in ["seen_help", "name", Looks.TITLE_SETTING, "look_cheese"]:
		saved[key] = Settings.get_value(key)
	Settings.set_value("seen_help", true)
	Progress.reset()


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])
	Progress.reset()


func after_all() -> void:
	DirAccess.remove_absolute(PATH)


func _fresh() -> Node:
	return autofree(PROGRESS.new(PATH))


func _report(changed := {}) -> Dictionary:
	var report := MatchReport.blank()
	report.merge({"when": 1_700_000_000, "map": "city", "mode": MatchSettings.TEAMS, "won": true, "place": 1, "players": 4, "kills": 7, "deaths": 3,
		"damage": 900, "assists": 2, "shots": 80, "hits": 30, "best_run": 4, "seconds": 300.0, "medals": {Medals.DOUBLE: 2}, "with": {"blaster": 5, "pistol": 2},
		"held": {"blaster": 200.0, "pistol": 60.0}}, true)
	report.merge(changed, true)
	return report


func _state() -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.FREE_FOR_ALL, "map": "box"}))
	for id: int in [1, 2, 3]:
		state.add_player(id)
	state.start_match()
	state.player(1).kills = 5
	state.player(1).deaths = 1
	state.player(2).kills = 2
	return state


func test_a_report_says_what_one_player_did_in_one_match() -> void:
	var state := _state()
	state.end_match()
	var line := Stats.blank()
	line.merge({"damage": 640, "assists": 1, "shots": 50, "hits": 21, "best": 4, "with": {"blaster": 5}}, true)
	var report := MatchReport.build(state, 1, line, {Medals.STREAK: 1}, {"blaster": 120.5}, 180.0, 1_700_000_000)
	assert_eq(report.map, "box")
	assert_eq(report.mode, MatchSettings.FREE_FOR_ALL)
	assert_eq(report.won, true)
	assert_eq(report.place, 1)
	assert_eq(report.players, 3)
	assert_eq([report.kills, report.deaths, report.damage, report.assists, report.shots, report.hits, report.best_run], [5, 1, 640, 1, 50, 21, 4])
	assert_eq(report.medals, {Medals.STREAK: 1})
	assert_eq(report.with, {"blaster": 5})
	assert_eq(report.held, {"blaster": 120.5})
	assert_eq(report.seconds, 180.0)
	# The one who came second lost, in second place.
	var second := MatchReport.build(state, 2, Stats.blank(), {}, {}, 180.0, 1_700_000_000)
	assert_eq(second.won, false)
	assert_eq(second.place, 2)
	assert_eq(MatchReport.line(report), "Won  ·  Brie Box  ·  Free-for-all  ·  5 knock-outs, 1 times out")


func test_a_drawn_match_is_neither_won_nor_lost() -> void:
	var state := _state()
	state.player(2).kills = 5
	state.player(2).deaths = 1
	state.end_match()
	assert_null(MatchReport.build(state, 1, Stats.blank(), {}, {}, 60.0, 0).won)
	assert_string_starts_with(MatchReport.line(MatchReport.build(state, 1, Stats.blank(), {}, {}, 60.0, 0)), "Drew")


func test_only_a_real_match_that_was_played_in_counts() -> void:
	var state := _state()
	assert_true(MatchReport.counts(state, 1))
	assert_false(MatchReport.counts(state, 99), "not for someone who isn't in it")
	state.player(3).watching = true
	assert_false(MatchReport.counts(state, 3), "nor someone only watching")
	assert_false(MatchReport.counts(MatchState.new(MatchSettings.practice()), 1), "nor the practice range")
	var alone := MatchState.new(MatchSettings.from_dict({}))
	alone.add_player(1)
	assert_false(MatchReport.counts(alone, 1), "nor a session with nobody else in it")


func test_a_report_from_anywhere_is_tidied() -> void:
	assert_eq(MatchReport.tidy("nonsense"), {})
	assert_eq(MatchReport.tidy(null), {})
	var wild := MatchReport.tidy({"kills": -5, "deaths": "many", "map": "moon", "mode": "chess", "won": "yes", "seconds": 1e9, "damage": 12.7,
		"medals": {"kazoo": 3, Medals.DOUBLE: 2, Medals.TRIPLE: -1}, "with": {"kazoo": 4, "pistol": 2, 7: 1}, "held": {"pistol": 1e9, "blaster": "x"}})
	assert_eq(wild.kills, 0)
	assert_eq(wild.deaths, 0)
	assert_eq(wild.map, Maps.DEFAULT)
	assert_eq(wild.mode, MatchSettings.FREE_FOR_ALL)
	assert_null(wild.won)
	assert_eq(wild.seconds, MatchReport.LONGEST)
	assert_eq(wild.damage, 12)
	assert_eq(wild.medals, {Medals.DOUBLE: 2})
	assert_eq(wild.with, {"pistol": 2})
	assert_eq(wild.held, {"pistol": MatchReport.LONGEST})
	assert_eq(MatchReport.tidy(_report()), _report(), "a good one is left as it is")
	# And it survives being written down and read back.
	assert_eq(MatchReport.tidy(JSON.parse_string(JSON.stringify(_report()))), _report())


func test_a_record_adds_up_every_match() -> void:
	var record := Record.add(Record.blank(), _report())
	record = Record.add(record, _report({"won": false, "kills": 3, "deaths": 6, "best_run": 2, "map": "box", "with": {"shotgun": 3}, "held": {"shotgun": 100.0}, "medals": {}}))
	record = Record.add(record, _report({"won": null, "kills": 0, "with": {}, "held": {}, "medals": {}}))
	assert_eq([record.matches, record.wins, record.draws], [3, 1, 1])
	assert_eq([record.kills, record.deaths], [10, 12])
	assert_eq(record.best_run, 4, "the best of them, not their sum")
	assert_eq(record.medals, 2)
	assert_eq(record.seconds, 900.0)
	assert_eq(record.weapons.blaster, {"kills": 5, "seconds": 200.0})
	assert_eq(record.weapons.shotgun, {"kills": 3, "seconds": 100.0})
	assert_eq(record.arenas, {"city": 2, "box": 1})
	assert_eq(record.modes, {MatchSettings.TEAMS: 3})
	assert_eq(Record.accuracy(record), roundi(90.0 / 240.0 * 100.0))
	assert_eq(Record.ratio(record), 0.8)
	assert_eq(Record.favourite(record), "blaster")
	# Adding to a record doesn't change the one it was added to.
	var before := Record.blank()
	Record.add(before, _report())
	assert_eq(before, Record.blank())


func test_a_record_with_nothing_in_it_reads_sensibly() -> void:
	var blank := Record.blank()
	assert_eq(Record.accuracy(blank), 0)
	assert_eq(Record.ratio(blank), 0.0)
	assert_eq(Record.favourite(blank), "")
	assert_eq(Record.lines(blank).size(), 4)
	assert_string_contains(Record.lines(blank)[0], "Matches played: 0")
	assert_eq(Record.time_text(20.0), "under a minute")
	assert_eq(Record.time_text(60.0 * 12.0), "12 min")
	assert_eq(Record.time_text(3600.0 * 3.0 + 60.0 * 12.0), "3 h 12 min")
	var never_out := Record.blank()
	never_out.kills = 7
	assert_eq(Record.ratio(never_out), 7.0)


func test_a_records_page_lists_each_weapon_most_knock_outs_first() -> void:
	var record := Record.add(Record.blank(), _report())
	var lines := Record.lines(record)
	assert_eq(lines.size(), 6)
	assert_string_starts_with(lines[4], "Cheddar Blaster: 5 knock-outs")
	assert_string_starts_with(lines[5], "Curd Pistol: 2 knock-outs")
	assert_string_contains(lines[4], "3 min in hand")
	assert_string_contains(lines[3], "time played: 5 min")


func test_a_record_from_anywhere_is_tidied() -> void:
	assert_eq(Record.tidy("nonsense"), Record.blank())
	var wild := Record.tidy({"matches": -3, "kills": 12.9, "seconds": "long", "weapons": {"kazoo": {"kills": 5}, "pistol": {"kills": "x", "seconds": -4}, "rifle": 7},
		"arenas": {"moon": 4, "box": 2}, "modes": {"chess": 1, MatchSettings.TEAMS: 3}})
	assert_eq(wild.matches, 0)
	assert_eq(wild.kills, 12)
	assert_eq(wild.seconds, 0.0)
	assert_eq(wild.weapons, {"pistol": {"kills": 0, "seconds": 0.0}})
	assert_eq(wild.arenas, {"box": 2})
	assert_eq(wild.modes, {MatchSettings.TEAMS: 3})


func test_experience_comes_from_playing_knocking_out_winning_and_medals() -> void:
	var earned := Levels.earned(_report())
	var parts := {}
	for part: Array in earned.parts:
		parts[part[0]] = part[1]
	assert_eq(parts, {"Time played": 5 * Levels.PER_MINUTE, "Finishing the match": Levels.FOR_FINISHING, "Knock-outs": 7 * Levels.PER_KNOCK_OUT,
		"Assists": 2 * Levels.PER_ASSIST, "Medals": 2 * Levels.PER_MEDAL, "Winning": Levels.FOR_WIN})
	var total := 0
	for amount: int in parts.values():
		total += amount
	assert_eq(earned.total, total)
	# Losing, with nothing done, still earns for finishing.
	var lost := Levels.earned(_report({"won": false, "kills": 0, "assists": 0, "medals": {}, "seconds": 30.0}))
	assert_eq(lost.total, Levels.FOR_FINISHING)
	assert_gt(Levels.earned(_report({"won": null})).total, Levels.earned(_report({"won": false})).total, "a draw is worth more than a loss")
	assert_lt(Levels.earned(_report({"won": null})).total, earned.total, "and less than a win")
	# A match left running all day earns for only so much of it.
	assert_eq(Levels.earned(_report({"seconds": 3600.0, "kills": 0, "assists": 0, "medals": {}, "won": false})).total, Levels.MOST_MINUTES * Levels.PER_MINUTE + Levels.FOR_FINISHING)


func test_each_level_takes_a_little_more_than_the_last() -> void:
	assert_eq(Levels.level(0), 1)
	assert_eq(Levels.level(-50), 1)
	assert_eq(Levels.level(Levels.FIRST - 1), 1)
	assert_eq(Levels.level(Levels.FIRST), 2)
	assert_eq(Levels.needed(1), 0)
	assert_eq(Levels.needed(2), Levels.FIRST)
	assert_eq(Levels.needed(3), Levels.FIRST * 2 + Levels.STEP)
	for level in range(1, Levels.TOP):
		assert_gt(Levels.to_next(level + 1), Levels.to_next(level), "level %d" % level)
		assert_eq(Levels.level(Levels.needed(level)), level)
		assert_eq(Levels.level(Levels.needed(level + 1) - 1), level)
	assert_eq(Levels.level(Levels.needed(Levels.TOP)), Levels.TOP)
	assert_eq(Levels.level(1 << 40), Levels.TOP, "there is a top")
	# About a match or two for the first level, at a good match's worth.
	assert_between(Levels.FIRST / float(Levels.earned(_report()).total), 0.5, 3.0)


func test_progress_through_a_level_reads_from_nothing_to_full() -> void:
	assert_eq(Levels.progress(0), 0.0)
	assert_almost_eq(Levels.progress(Levels.FIRST / 2), 0.5, 0.01)
	assert_eq(Levels.progress(Levels.FIRST), 0.0, "the next level begins")
	assert_eq(Levels.progress(1 << 40), 1.0)
	assert_eq(Levels.text(0), "Level 1   ·   0 of %d to the next" % Levels.FIRST)
	assert_eq(Levels.text(Levels.FIRST + 40), "Level 2   ·   40 of %d to the next" % Levels.to_next(2))
	assert_eq(Levels.text(1 << 40), "Level %d" % Levels.TOP)


func test_what_is_earned_is_kept_between_visits() -> void:
	var progress := _fresh()
	assert_eq(progress.xp, 0)
	assert_eq(progress.level(), 1)
	watch_signals(progress)
	var earned: Dictionary = progress.add_match(_report())
	assert_signal_emitted(progress, "changed")
	assert_eq(earned.xp, Levels.earned(_report()).total)
	assert_eq(earned.from, 1)
	assert_eq(earned.to, Levels.level(earned.xp))
	assert_eq(progress.record.matches, 1)
	assert_eq(progress.history.size(), 1)
	# Another visit.
	var again := _fresh()
	assert_eq(again.xp, earned.xp)
	assert_eq(again.record, progress.record)
	assert_eq(again.history, [_report()])
	again.reset()
	assert_eq(_fresh().record, Record.blank())
	assert_eq(_fresh().xp, 0)


func test_only_the_last_matches_are_kept_newest_first() -> void:
	var progress := _fresh()
	for played in Progress.HISTORY + 5:
		progress.add_match(_report({"kills": played}))
	assert_eq(progress.history.size(), Progress.HISTORY)
	assert_eq(progress.history[0].kills, Progress.HISTORY + 4, "the newest first")
	assert_eq(progress.record.matches, Progress.HISTORY + 5, "the record counts them all")
	assert_eq(_fresh().history.size(), Progress.HISTORY)


func test_a_damaged_file_gives_a_fresh_start_and_junk_in_it_is_left_out() -> void:
	for junk: String in ["", "not json", "[1, 2, 3]", "{\"xp\": \"lots\", \"record\": 7, \"history\": \"x\"}"]:
		var file := FileAccess.open(PATH, FileAccess.WRITE)
		file.store_string(junk)
		file.close()
		var progress := _fresh()
		assert_eq(progress.xp, 0, junk)
		assert_eq(progress.record, Record.blank(), junk)
		assert_eq(progress.history, [], junk)
	var file := FileAccess.open(PATH, FileAccess.WRITE)
	file.store_string(JSON.stringify({"xp": 1e18, "record": {"matches": 4}, "history": ["x", _report(), 7]}))
	file.close()
	var kept := _fresh()
	assert_eq(kept.xp, Progress.MOST_XP)
	assert_eq(kept.record.matches, 4)
	assert_eq(kept.history, [_report()])
	assert_eq(kept.add_match({"nonsense": true}).xp, Levels.earned(MatchReport.blank()).total, "a report of nothing is a match of nothing")


func test_the_games_own_tests_never_touch_a_players_progress() -> void:
	assert_true(Progress.under_test())
	assert_eq(Progress.path, Progress.TEST_PATH)
	assert_ne(Progress.TEST_PATH, Progress.DEFAULT_PATH)


func test_a_title_and_a_level_travel_with_the_look() -> void:
	var look := Looks.pack("brie", "crown", "flag", "sharpshooter", 37)
	var choices := Looks.unpack(look)
	assert_eq([choices.cheese, choices.hat, choices.held, choices.title, choices.level], ["brie", "crown", "flag", "sharpshooter", 37])
	assert_eq(Looks.clean(look), look)
	for title: String in Looks.TITLES:
		assert_has(Looks.TITLE_NAMES, title)
		assert_eq(Looks.unpack(Looks.pack("swiss", "none", "none", title, 5)).title, title)
	assert_eq(Looks.unpack(Looks.pack("swiss", "none", "none", "emperor", 5000)).title, "none")
	assert_eq(Looks.unpack(Looks.pack("swiss", "none", "none", "none", 5000)).level, Looks.TOP_LEVEL)
	assert_eq(Looks.TOP_LEVEL, Levels.TOP)
	# It still fits the number a look travels as.
	assert_lt(Looks.pack(Looks.CHEESES[-1], Looks.HATS[-1], Looks.HELD[-1], Looks.TITLES[-1], Looks.TOP_LEVEL), 1 << 31)
	assert_eq(Looks.rank_text(Looks.pack("swiss", "none", "none")), "", "a bot has neither")
	assert_eq(Looks.rank_text(Looks.pack("swiss", "none", "none", "none", 12)), "Level 12")
	assert_eq(Looks.rank_text(look), "Sharpshooter  ·  Level 37")


func test_the_look_sent_carries_the_players_own_title_and_level() -> void:
	Settings.set_value(Looks.TITLE_SETTING, "mouser")
	Progress.add_match(_report({"kills": 40}))
	var chosen := Looks.unpack(Looks.chosen())
	assert_eq(chosen.title, "mouser")
	assert_eq(chosen.level, Progress.level())
	assert_gt(chosen.level, 1)


func test_others_see_a_players_level_and_title_under_their_name() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	world.match_.add_player(2)
	var other: Node3D = world._create_player([2, Vector3(3, 0.1, 0)])
	world.players.add_child(other)
	world.match_._wear(2, Looks.pack("blue", "none", "none", "old_hand", 21))
	other._on_match_state_changed()
	var tag: Label3D = other.get_node("RankTag")
	assert_eq(tag.text, "Old Hand  ·  Level 21")
	assert_true(tag.visible)
	assert_lt(tag.position.y, other.name_tag.position.y, "under the name")
	# A bot, with no level, has nothing under its name.
	world.match_._wear(2, Looks.pack("blue", "none", "none"))
	other._on_match_state_changed()
	assert_false(tag.visible)


func test_a_match_that_ends_is_kept_once_and_adds_to_the_summary() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	world.match_.add_player(2)
	world.match_.state.start_match()
	var keeper: MatchKeeper = world.match_keeper
	# Counted from here, not from the frames the match has already run.
	keeper._begin()
	watch_signals(keeper)
	for frame in 30:
		keeper._process(1.0)
	world.match_.state.player(1).kills = 3
	world.match_.state.end_match()
	keeper._process(0.016)
	keeper._process(0.016)
	assert_signal_emit_count(keeper, "kept", 1)
	assert_eq(Progress.record.matches, 1)
	assert_eq(Progress.history[0].kills, 3)
	assert_almost_eq(Progress.history[0].seconds, 30.0, 0.1)
	assert_almost_eq(float(Progress.history[0].held.values()[0]), 30.0, 0.1, "the weapon that was in hand")
	assert_string_starts_with(keeper.lines()[0], "+%d experience" % keeper.last.xp)
	assert_has(keeper.lines(), "Achievement: %s" % Achievements.name_of("first_blood"), "and the feats it was")
	# The next match starts the count again.
	world.match_.state.start_match()
	keeper._process(0.016)
	assert_eq(keeper.lines(), [])
	assert_eq(keeper._seconds, 0.016)


func test_the_practice_range_and_a_session_alone_keep_nothing() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.practice())
	await wait_physics_frames(2)
	world.match_.state.end_match()
	world.match_keeper._process(0.016)
	assert_eq(Progress.record.matches, 0)
	assert_eq(world.match_keeper.lines(), [])


func test_the_profile_page_shows_who_you_are_and_what_you_have_done() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(3)
	Settings.set_value("name", "Gouda 7")
	Settings.set_value(Looks.TITLE_SETTING, "none")
	Settings.set_value("look_cheese", "blue")
	main.front_end.button("profile").pressed.emit()
	var page: ProfileScreen = main.profile_screen
	assert_true(page.visible)
	var shown := page.showing()
	assert_eq(shown.name, "Gouda 7")
	assert_eq(shown.rank, "Blue cheese", "with no title chosen, their cheese")
	assert_eq(shown.level, Levels.text(0))
	assert_eq(shown.part, ProfileScreen.RECORD)
	assert_string_contains(shown.lines[0], "Matches played: 0")
	(page.find_child("Profile_matches", true, false) as Button).pressed.emit()
	assert_eq(page.showing().lines, ["No matches yet. Play one, and it will be here."])
	# After a match, with a title chosen.
	Progress.add_match(_report())
	Settings.set_value(Looks.TITLE_SETTING, "big_cheese")
	page.open()
	assert_eq(page.showing().rank, "The Big Cheese")
	assert_eq(page.showing().level, Levels.text(Progress.xp))
	assert_string_starts_with(page.showing().lines[0], "Won  ·  Curd City")
	(page.find_child("Profile_record", true, false) as Button).pressed.emit()
	assert_string_contains(page.showing().lines[0], "Matches played: 1")
	assert_almost_eq((page.find_child("LevelBar", true, false) as ProgressBar).value, Levels.progress(Progress.xp), 0.002)
	(page.find_child("ProfileDone", true, false) as Button).pressed.emit()
	assert_false(page.visible)
	assert_true(main.front_end.visible, "back at the title")
	# It fits the smallest screen.
	var base := Vector2(ProjectSettings.get_setting("display/window/size/viewport_width"), ProjectSettings.get_setting("display/window/size/viewport_height"))
	var needed: Vector2 = page.find_child("Panel", true, false).get_combined_minimum_size()
	assert_lte(needed.x, base.x)
	assert_lte(needed.y, base.y)
	# The title is chosen with the rest of the character.
	assert_not_null(main.settings_menu.find_child("Setting_" + Looks.TITLE_SETTING, true, false))


func test_how_long_ago_a_match_was_reads_plainly() -> void:
	assert_eq(ProfileScreen.when_text(1000, 1030), "just now")
	assert_eq(ProfileScreen.when_text(1000, 1000 + 600), "10 minutes ago")
	assert_eq(ProfileScreen.when_text(1000, 1000 + 3600 * 5), "5 hours ago")
	assert_eq(ProfileScreen.when_text(1000, 1000 + 86400 * 9), "9 days ago")
	assert_eq(ProfileScreen.when_text(5000, 1000), "just now", "a clock gone backward")
