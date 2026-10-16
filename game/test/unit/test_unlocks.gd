extends GutTest
## Things to earn, challenges and achievements.

const WORLD := preload("res://world/world.tscn")
const MAIN := preload("res://main.tscn")
const PLAYER := preload("res://player/player.tscn")
const DAY := Challenges.DAY_SECONDS
## A Monday at noon, long enough ago to be any day.
const NOW := 1_700_000_000

var saved := {}


func before_each() -> void:
	for key: String in Looks.SETTINGS.keys() + ["seen_help"]:
		saved[key] = Settings.get_value(key)
		Settings.set_value(key, Settings.DEFAULTS[key])
	Settings.set_value("seen_help", true)
	Progress.reset()


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])
	Progress.reset()


func _report(changed := {}) -> Dictionary:
	var report := MatchReport.blank()
	report.merge({"when": NOW, "map": "city", "mode": MatchSettings.TEAMS, "won": true, "kills": 7, "deaths": 3, "damage": 900, "assists": 2,
		"shots": 80, "hits": 30, "best_run": 4, "seconds": 300.0, "with": {"blaster": 7}}, true)
	report.merge(changed, true)
	return report


func test_nothing_to_earn_changes_a_fight() -> void:
	# Everything there is to earn is one of these kinds, each only for show.
	assert_eq(Unlocks.KINDS.keys(), [Unlocks.CHEESE, Unlocks.HAT, Unlocks.HELD, Unlocks.TITLE, Unlocks.FINISH, Unlocks.EMOTE])
	for named: String in Unlocks.NEEDS:
		var kind := named.get_slice(":", 0)
		var raw := named.get_slice(":", 1)
		assert_has(Unlocks.KINDS, kind, named)
		var id: Variant = int(raw) if kind == Unlocks.EMOTE else raw
		assert_has(Unlocks.choices(kind), id, "%s is something there is" % named)
		assert_ne(Unlocks.name_of(kind, id), "", named)
	# No weapon's numbers depend on its finish: painting a weapon leaves
	# what it does exactly as it was.
	for weapon: String in Weapons.ids():
		var stats := Weapons.find(weapon)
		var before := {}
		for property in stats.get_property_list():
			if property.usage & PROPERTY_USAGE_STORAGE:
				before[property.name] = stats.get(property.name)
		for finish: String in Unlocks.FINISHES:
			var model := WeaponModels.build(weapon)
			Unlocks.paint(model, finish)
			model.free()
		for name: String in before:
			assert_eq(stats.get(name), before[name], "%s.%s" % [weapon, name])
	# Nor a body's: whatever is worn, what can be hit is the same.
	var sizes := {}
	for look: int in [0, Looks.pack("brie", "crown", "lollipop", "grand_fromage", 99, "gold"), Looks.pack("blue", "cowboy", "flag")]:
		var player: CharacterBody3D = PLAYER.instantiate()
		add_child_autofree(player)
		player.body.wear(look)
		var shape: Shape3D = player.get_node("CollisionShape3D").shape
		sizes[[shape.get("radius"), shape.get("height")]] = true
		assert_eq(BodyShape.RADIUS, 0.4 * BodyShape.SIZE)
	assert_eq(sizes.size(), 1, "one shape, whatever the look")


func test_what_is_everyones_and_what_takes_a_level() -> void:
	assert_true(Unlocks.has(Unlocks.HAT, "none", 1, [], []))
	assert_true(Unlocks.has(Unlocks.HAT, "party", 1, [], []), "something to wear from the start")
	assert_true(Unlocks.has(Unlocks.CHEESE, "cheddar", 1, [], []))
	assert_false(Unlocks.has(Unlocks.HAT, "crown", 19, [], []))
	assert_true(Unlocks.has(Unlocks.HAT, "crown", 20, [], []))
	assert_true(Unlocks.has(Unlocks.HAT, "crown", 99, [], []))
	assert_eq(Unlocks.needs_text(Unlocks.HAT, "crown"), "Level 20")
	assert_eq(Unlocks.needs_text(Unlocks.HAT, "none"), "")
	# Each kind has something from the start, so a new player has choices.
	for kind: String in Unlocks.KINDS:
		assert_true(Unlocks.has(kind, Unlocks.choices(kind)[0], 1, [], []), kind)


func test_some_take_an_achievement_or_a_challenge() -> void:
	assert_false(Unlocks.has(Unlocks.TITLE, "sharpshooter", 99, [], []), "no level gives it")
	assert_true(Unlocks.has(Unlocks.TITLE, "sharpshooter", 1, ["marksman"], []))
	assert_eq(Unlocks.needs_text(Unlocks.TITLE, "sharpshooter"), "Achievement: Sharpshooter")
	assert_false(Unlocks.has(Unlocks.TITLE, "wheel_roller", 99, ["marksman"], []))
	assert_true(Unlocks.has(Unlocks.TITLE, "wheel_roller", 1, [], ["long:kills_with:launcher"]))
	assert_string_contains(Unlocks.needs_text(Unlocks.TITLE, "wheel_roller"), "Gouda Launcher")
	# What they take is something there is.
	var long_ones := Challenges.active(NOW).map(func(challenge: Dictionary) -> String: return challenge.id)
	for named: String in Unlocks.NEEDS:
		var need: Dictionary = Unlocks.NEEDS[named]
		if need.has("achievement"):
			assert_has(Achievements.ALL, need.achievement, named)
		if need.has("challenge"):
			assert_has(long_ones, need.challenge, named)
		if need.has("level"):
			assert_between(need.level, 2, Levels.TOP, named)


func test_a_button_steps_only_through_what_has_been_earned() -> void:
	var seen := []
	var hat := "none"
	for press in 6:
		hat = Unlocks.next(Unlocks.HAT, hat, 1, [], [])
		seen.append(hat)
	assert_eq(seen, ["party", "top", "none", "party", "top", "none"])
	assert_eq(Unlocks.next(Unlocks.HAT, "top", 6, [], []), "chef", "one more at level six")
	assert_eq(Unlocks.next(Unlocks.HAT, "crown", 1, [], []), "none", "from one that isn't theirs, back to what is")
	assert_eq(Unlocks.next(Unlocks.EMOTE, Emotes.DANCE, 1, [], []), Emotes.WAVE)
	assert_eq(Unlocks.next(Unlocks.EMOTE, Emotes.DANCE, 2, [], []), Emotes.SHRUG)
	# In the settings.
	var menu: Control = preload("res://ui/settings_menu.gd").new()
	add_child_autofree(menu)
	var list: OptionButton = menu.find_child("Setting_look_hat", true, false)
	assert_eq(menu.choices_of("Setting_look_hat"), Looks.HATS, "every hat is listed")
	menu.choose("Setting_look_hat", "cowboy")
	assert_eq(Settings.get_value("look_hat"), "none", "but one not yet earned can't be chosen")
	assert_true(list.is_item_disabled(Looks.HATS.find("cowboy")))
	assert_string_contains(list.get_item_text(Looks.HATS.find("cowboy")), "Level", "and says what it takes")
	Progress.xp = Levels.needed(20)
	menu.refresh()
	menu.choose("Setting_look_hat", "cowboy")
	assert_eq(Settings.get_value("look_hat"), "cowboy", "once it has been earned it can")
	assert_eq(list.get_item_text(Looks.HATS.find("cowboy")), Looks.HAT_NAMES["cowboy"])
	assert_not_null(menu.find_child("Setting_" + Unlocks.FINISH_SETTING, true, false))


func test_what_a_level_or_a_feat_gives_is_told() -> void:
	assert_eq(Unlocks.gained(1, 1, [], []), [])
	assert_eq(Unlocks.gained(1, 2, [], []), [[Unlocks.EMOTE, Emotes.SHRUG]])
	var to_five := Unlocks.gained(2, 5, [], []).map(func(thing: Array) -> String: return Unlocks.gained_text(thing))
	assert_eq(to_five, ["Cheese: Blue", "To hold: Flag", "Weapon finish: Red wax"])
	assert_eq(Unlocks.gained(5, 5, ["marksman"], []), [[Unlocks.TITLE, "sharpshooter"]])
	assert_eq(Unlocks.gained(5, 5, [], ["long:kills_with:launcher"]), [[Unlocks.TITLE, "wheel_roller"]])
	# Everything is given by the top, with every feat and challenge.
	var all := Unlocks.gained(1, Levels.TOP, Achievements.ALL.keys(), Challenges.active(NOW).map(func(challenge: Dictionary) -> String: return challenge.id))
	assert_eq(all.size(), Unlocks.NEEDS.size())
	var lines := Unlocks.lines(1, [], [])
	assert_eq(lines.size(), Unlocks.NEEDS.size())
	assert_true(lines.all(func(line: String) -> bool: return line.begins_with("[ ]")), "none of them yet")
	assert_true(Unlocks.lines(Levels.TOP, Achievements.ALL.keys(), ["long:kills_with:launcher"]).all(func(line: String) -> bool: return line.begins_with("[x]") and line.ends_with("yours")))


func test_a_finish_travels_with_the_look_and_paints_the_weapon() -> void:
	var look := Looks.pack("swiss", "none", "none", "none", 3, "gold")
	assert_eq(Looks.unpack(look).finish, "gold")
	assert_eq(Looks.clean(look), look)
	assert_eq(Looks.unpack(Looks.pack("swiss", "none", "none", "none", 3, "rainbow")).finish, "plain")
	assert_lt(Looks.pack(Looks.CHEESES[-1], Looks.HATS[-1], Looks.HELD[-1], Looks.TITLES[-1], Looks.TOP_LEVEL, Unlocks.FINISHES[-1]), 1 << 31)
	Settings.set_value(Unlocks.FINISH_SETTING, "wax")
	assert_eq(Looks.unpack(Looks.chosen()).finish, "wax")
	for finish: String in Unlocks.FINISHES:
		assert_has(Unlocks.FINISH_NAMES, finish)
	# Painted, a weapon's parts take the finish's colour; plain, they don't.
	var plain := WeaponModels.build("blaster")
	var painted := WeaponModels.build("blaster")
	Unlocks.paint(plain, "plain")
	Unlocks.paint(painted, "gold")
	var colours := func(model: Node) -> Array:
		return model.find_children("*", "MeshInstance3D", true, false).map(func(part: MeshInstance3D) -> Color:
			return (part.material_override as StandardMaterial3D).albedo_color if part.material_override is StandardMaterial3D else Color.BLACK)
	assert_ne(colours.call(painted), colours.call(plain))
	assert_eq(colours.call(plain), colours.call(WeaponModels.build("blaster")))
	# The hand that holds it is still a hand.
	var hands: Array = colours.call(painted).filter(func(colour: Color) -> bool: return colour.is_equal_approx(WeaponModels.HAND))
	assert_gt(hands.size(), 0)
	plain.free()
	painted.free()


func test_others_see_the_finish_a_player_chose() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	world.match_.add_player(2)
	var other: Node3D = world._create_player([2, Vector3(3, 0.1, 0)])
	world.players.add_child(other)
	await wait_process_frames(2)
	assert_eq(other._shown_finish, "plain")
	world.match_._wear(2, Looks.pack("swiss", "none", "none", "none", 9, "blue_vein"))
	other._on_match_state_changed()
	assert_eq(other._shown_finish, "blue_vein", "put on the weapon in their hand at once")


func test_an_emote_not_earned_gives_way_to_one_that_is() -> void:
	assert_true(Unlocks.mine(Unlocks.EMOTE, Emotes.WAVE))
	assert_false(Unlocks.mine(Unlocks.EMOTE, Emotes.CHEER))
	Progress.xp = Levels.needed(7)
	assert_true(Unlocks.mine(Unlocks.EMOTE, Emotes.CHEER))


func test_the_same_challenges_are_set_for_everyone_on_a_day() -> void:
	var today := Challenges.active(NOW)
	assert_eq(today, Challenges.active(NOW + 3600), "all day")
	var daily := today.filter(func(challenge: Dictionary) -> bool: return challenge.period == Challenges.DAILY)
	var weekly := today.filter(func(challenge: Dictionary) -> bool: return challenge.period == Challenges.WEEKLY)
	var long := today.filter(func(challenge: Dictionary) -> bool: return challenge.period == Challenges.LONG)
	assert_eq(daily.size(), Challenges.EACH_DAY)
	assert_eq(weekly.size(), Challenges.EACH_WEEK)
	assert_eq(long.size(), Weapons.ids().size(), "a long one for each weapon")
	# No two of a kind on a day, and every id its own.
	assert_eq(daily.map(func(challenge: Dictionary) -> String: return challenge.kind).size(), EACH_DAY_KINDS(daily))
	var ids := {}
	for challenge: Dictionary in today:
		assert_false(ids.has(challenge.id), challenge.id)
		ids[challenge.id] = true
		assert_gt(challenge.goal, 0)
		assert_eq(challenge.reward, Challenges.REWARDS[challenge.period])
		assert_ne(challenge.text, "")
		assert_eq(Challenges.text_of(challenge.id), challenge.text)
	# Tomorrow's are different, and the week's the same until the week turns.
	var tomorrow := Challenges.active(NOW + DAY)
	assert_ne(tomorrow.slice(0, Challenges.EACH_DAY), daily)
	var same_week := Challenges.active(Challenges.week_of(NOW) * 7 * DAY + 10)
	assert_eq(same_week.filter(func(challenge: Dictionary) -> bool: return challenge.period == Challenges.WEEKLY), weekly)
	assert_ne(Challenges.active(NOW + 7 * DAY).filter(func(challenge: Dictionary) -> bool: return challenge.period == Challenges.WEEKLY), weekly)
	# Over a year every challenge in the pool comes up.
	var set := {}
	for day in 365:
		for challenge: Dictionary in Challenges.active(NOW + day * DAY).slice(0, Challenges.EACH_DAY):
			set["%s:%s" % [challenge.kind, challenge.what]] = true
	assert_eq(set.size(), Challenges.pool().size())


func EACH_DAY_KINDS(daily: Array) -> int:
	var kinds := {}
	for challenge: Dictionary in daily:
		kinds[challenge.kind] = true
	return kinds.size()


func test_a_challenge_reads_plainly_and_a_weeks_asks_more() -> void:
	assert_eq(Challenges._text("kills_with", "shotgun", 6), "Knock out 6 players with the Swiss Shotgun")
	assert_eq(Challenges._text("wins_on", "city", 1), "Win 1 matches on Curd City")
	assert_eq(Challenges._text("play_mode", MatchSettings.HILL, 2), "Play 2 matches of Hold the hill")
	assert_eq(Challenges._text("medals", "", 3), "Earn 3 medals")
	assert_eq(Challenges.text_of("long:kills_with:rifle"), "Knock out %d players with the %s" % [Challenges.LONG_KILLS, Weapons.find("rifle").display_name])
	assert_eq(Challenges.text_of("nonsense"), "")
	for kind: String in Challenges.KINDS:
		assert_has(Challenges.DAY_GOALS, kind)
	assert_gt(Challenges.REWARDS[Challenges.WEEKLY], Challenges.REWARDS[Challenges.DAILY])


func test_what_a_match_does_toward_each_kind_of_challenge() -> void:
	var report := _report({"medals": {Medals.DOUBLE: 2, Medals.LONG_SHOT: 1}, "with": {"shotgun": 4, "pistol": 3}})
	assert_eq(Challenges.counted("kills", "", report), 7)
	assert_eq(Challenges.counted("kills_with", "shotgun", report), 4)
	assert_eq(Challenges.counted("kills_with", "rifle", report), 0)
	assert_eq(Challenges.counted("wins_on", "city", report), 1)
	assert_eq(Challenges.counted("wins_on", "box", report), 0, "another arena")
	assert_eq(Challenges.counted("wins_on", "city", _report({"won": false})), 0, "a loss")
	assert_eq(Challenges.counted("play_mode", MatchSettings.TEAMS, report), 1)
	assert_eq(Challenges.counted("play_mode", MatchSettings.HILL, report), 0)
	assert_eq(Challenges.counted("medals", "", report), 3)
	assert_eq(Challenges.counted("damage", "", report), 900)
	assert_eq(Challenges.counted("assists", "", report), 2)
	assert_eq(Challenges.counted("kazoo", "", report), 0)


func test_progress_toward_a_challenge_adds_up_and_finishing_it_pays_once() -> void:
	var state := {"progress": {}, "done": []}
	var rifle := "long:kills_with:rifle"
	var first := Challenges.advance(state, _report({"with": {"rifle": 60}, "kills": 60}), NOW)
	assert_eq(first.state.progress[rifle], 60)
	assert_false(first.completed.any(func(challenge: Dictionary) -> bool: return challenge.id == rifle))
	var second := Challenges.advance(first.state, _report({"with": {"rifle": 45}, "kills": 45}), NOW)
	assert_true(second.completed.any(func(challenge: Dictionary) -> bool: return challenge.id == rifle))
	assert_has(second.state.done, rifle)
	assert_does_not_have(second.state.progress, rifle)
	assert_gte(second.xp, Challenges.REWARDS[Challenges.LONG])
	var third := Challenges.advance(second.state, _report({"with": {"rifle": 500}, "kills": 0, "damage": 0, "assists": 0, "won": false, "mode": MatchSettings.GUN_GAME, "map": "duel"}), NOW)
	assert_false(third.completed.any(func(challenge: Dictionary) -> bool: return challenge.id == rifle), "once")
	# What was given isn't changed.
	assert_eq(state, {"progress": {}, "done": []})


func test_a_days_challenges_are_forgotten_with_the_day_and_long_ones_kept() -> void:
	var daily: Dictionary = Challenges.active(NOW)[0]
	var state := {"progress": {daily.id: 1, "long:kills_with:rifle": 30, "kazoo": 5}, "done": ["long:kills_with:pistol", daily.id + "x", 7]}
	var today := Challenges.tidy(state, NOW)
	assert_eq(today.progress, {daily.id: mini(1, daily.goal), "long:kills_with:rifle": 30})
	assert_eq(today.done, ["long:kills_with:pistol"])
	var next_month := Challenges.tidy(state, NOW + 30 * DAY)
	assert_eq(next_month.progress, {"long:kills_with:rifle": 30}, "the day's is gone")
	assert_eq(next_month.done, ["long:kills_with:pistol"], "a long one stays done")
	assert_eq(Challenges.tidy("nonsense", NOW), {"progress": {}, "done": []})
	assert_eq(Challenges.tidy({"progress": {"long:kills_with:rifle": 5000}}, NOW).progress["long:kills_with:rifle"], Challenges.LONG_KILLS, "no more than it asks")
	var lines := Challenges.lines(state, NOW)
	assert_eq(lines[0], "Today:")
	assert_string_contains(lines[1], daily.text)
	assert_true(lines.any(func(line: String) -> bool: return line.contains("done")))
	assert_true(lines.any(func(line: String) -> bool: return line.contains("30 of %d" % Challenges.LONG_KILLS)))


func test_each_achievement_is_earned_by_what_it_says() -> void:
	var blank := Record.blank()
	var quiet := _report({"kills": 0, "won": false, "medals": {}, "shots": 0, "hits": 0, "with": {}})
	for id: String in Achievements.ALL:
		assert_false(Achievements.earned(id, blank, quiet), "%s is not given for nothing" % id)
		assert_true(Icons.has(Achievements.ALL[id].icon), "%s has a picture" % id)
		assert_ne(Achievements.ALL[id].line, "")
	var record := Record.blank()
	record.kills = 1
	assert_true(Achievements.earned("first_blood", record, quiet))
	record.wins = 1
	assert_true(Achievements.earned("first_win", record, quiet))
	record.kills = 99
	assert_false(Achievements.earned("century", record, quiet))
	record.kills = 100
	assert_true(Achievements.earned("century", record, quiet))
	record.matches = 50
	assert_true(Achievements.earned("veteran", record, quiet))
	record.seconds = Achievements.MARATHON_SECONDS
	assert_true(Achievements.earned("marathon", record, quiet))
	# Those earned in one match.
	assert_true(Achievements.earned("flawless", blank, _report({"deaths": 0, "kills": 5})))
	assert_false(Achievements.earned("flawless", blank, _report({"deaths": 1, "kills": 9})))
	assert_false(Achievements.earned("flawless", blank, _report({"deaths": 0, "kills": 9, "won": false})))
	assert_true(Achievements.earned("marksman", blank, _report({"shots": 20, "hits": 12})))
	assert_false(Achievements.earned("marksman", blank, _report({"shots": 19, "hits": 19})), "too few shots to say")
	assert_false(Achievements.earned("marksman", blank, _report({"shots": 40, "hits": 20})))
	for medal: Array in [[Medals.TRIPLE, "hat_trick"], [Medals.LONG_SHOT, "long_arm"], [Medals.COMEBACK, "comeback_kid"], [Medals.REVENGE, "grudge"]]:
		assert_true(Achievements.earned(medal[1], blank, _report({"medals": {medal[0]: 1}})), medal[1])
	# Those earned across everything.
	for map: String in Maps.ORDER:
		assert_false(Achievements.earned("globetrotter", record, quiet))
		record.arenas[map] = 1
	assert_true(Achievements.earned("globetrotter", record, quiet))
	for mode: String in MatchSettings.MODES:
		if mode != MatchSettings.PRACTICE:
			record.modes[mode] = 1
	assert_true(Achievements.earned("all_rounder", record, quiet))
	for weapon: String in Weapons.ids():
		assert_false(Achievements.earned("arsenal", record, quiet))
		record.weapons[weapon] = {"kills": 1, "seconds": 5.0}
	assert_true(Achievements.earned("arsenal", record, quiet))


func test_an_achievement_is_earned_once_and_kept() -> void:
	var record := Record.add(Record.blank(), _report())
	var new := Achievements.newly([], record, _report())
	assert_has(new, "first_blood")
	assert_has(new, "first_win")
	assert_eq(Achievements.newly(new, record, _report()), [], "not again")
	assert_eq(Achievements.tidy(["first_blood", "kazoo", "first_blood", 7]), ["first_blood"])
	assert_eq(Achievements.tidy("nonsense"), [])
	assert_eq(Achievements.name_of("century"), "Mouser")
	assert_eq(Achievements.name_of("kazoo"), "")
	var lines := Achievements.lines(["first_blood"])
	assert_eq(lines[0], "1 of %d earned" % Achievements.ALL.size())
	assert_eq(lines.filter(func(line: String) -> bool: return line.begins_with("[x]")).size(), 1)


func test_a_match_earns_its_challenges_feats_and_things_and_they_are_kept() -> void:
	var earned: Dictionary = Progress.add_match(_report({"kills": 30, "with": {"blaster": 30}, "shots": 40, "hits": 30}))
	assert_has(earned.achievements, "first_blood")
	assert_has(earned.achievements, "marksman")
	assert_has(earned.unlocked, [Unlocks.TITLE, "sharpshooter"], "what the feat gives")
	assert_gt(earned.to, earned.from)
	assert_has(earned.unlocked, [Unlocks.EMOTE, Emotes.SHRUG], "and what the level gives")
	assert_true(Unlocks.mine(Unlocks.TITLE, "sharpshooter"))
	# Kept, between visits.
	var again: Node = autofree(preload("res://progress/progress.gd").new(Progress.path))
	assert_eq(again.achievements, Progress.achievements)
	assert_eq(again.challenges, Challenges.tidy(Progress.challenges, again.now()))
	# The end of the match says so.
	var keeper := MatchKeeper.new()
	autofree(keeper)
	keeper.last = earned
	var said := keeper.lines()
	assert_has(said, "Achievement: Sharpshooter")
	assert_has(said, "Unlocked   ·   Title: Sharpshooter")
	assert_true(said[0].ends_with("reached!"))


func test_a_finished_challenge_pays_experience_and_shows_on_the_profile() -> void:
	var before := Progress.xp
	var plain: int = Levels.earned(MatchReport.tidy(_report({"with": {"rifle": Challenges.LONG_KILLS}, "kills": Challenges.LONG_KILLS}))).total
	var earned: Dictionary = Progress.add_match(_report({"with": {"rifle": Challenges.LONG_KILLS}, "kills": Challenges.LONG_KILLS, "when": Progress.now()}))
	assert_true(earned.challenges.any(func(challenge: Dictionary) -> bool: return challenge.id == "long:kills_with:rifle"))
	assert_gte(Progress.xp - before, plain + Challenges.REWARDS[Challenges.LONG])
	assert_has(Progress.challenges_done(), "long:kills_with:rifle")
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(3)
	main.front_end.button("profile").pressed.emit()
	var page: ProfileScreen = main.profile_screen
	for part: String in ProfileScreen.PARTS:
		(page.find_child("Profile_" + part, true, false) as Button).pressed.emit()
		assert_eq(page.showing().part, part)
		assert_gt(page.showing().lines.size(), 0, part)
	(page.find_child("Profile_challenges", true, false) as Button).pressed.emit()
	assert_true(page.showing().lines.any(func(line: String) -> bool: return line.contains(Weapons.find("rifle").display_name) and line.contains("done")))
	(page.find_child("Profile_achievements", true, false) as Button).pressed.emit()
	assert_true(page.showing().lines.any(func(line: String) -> bool: return line.begins_with("[x] Mouser")))
	(page.find_child("Profile_unlocks", true, false) as Button).pressed.emit()
	assert_true(page.showing().lines.any(func(line: String) -> bool: return line.contains("Title: Mouser") and line.ends_with("yours")))
	var base := Vector2(ProjectSettings.get_setting("display/window/size/viewport_width"), ProjectSettings.get_setting("display/window/size/viewport_height"))
	assert_lte(page.find_child("Panel", true, false).get_combined_minimum_size().x, base.x, "with its five parts, it still fits")
