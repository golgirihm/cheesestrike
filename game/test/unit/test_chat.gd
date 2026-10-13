extends GutTest
## Text chat: the filter, who reads what, how often, the box, and the wheel
## of quick messages.

const WORLD := preload("res://world/world.tscn")
const MAIN := preload("res://main.tscn")
const CHEDDAR := MatchState.CHEDDAR
const BRIE := MatchState.BRIE

var world: Node3D
var saved := {}


func before_each() -> void:
	for key: String in [ChatBox.SETTING, "seen_help"]:
		saved[key] = Settings.get_value(key)
	Settings.set_value(ChatBox.SETTING, true)
	Settings.set_value("seen_help", true)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _record(team: int) -> PlayerRecord:
	var record := PlayerRecord.new()
	record.team = team
	return record


func _start(mode: String) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box", "mode": mode}))
	await wait_physics_frames(2)


func _box() -> ChatBox:
	var box := ChatBox.new(world)
	add_child_autofree(box)
	var wheel := Wheel.new()
	add_child_autofree(wheel)
	box.wheel = wheel
	wheel.chosen.connect(box._on_wheel_chosen)
	return box


func test_a_message_is_tidied_before_it_is_passed_on() -> void:
	assert_eq(Chat.clean("  hello there  "), "hello there")
	assert_eq(Chat.clean("one\ntwo\tthree"), "one two three", "on one line")
	assert_eq(Chat.clean("a    b"), "a b")
	assert_eq(Chat.clean(""), "")
	assert_eq(Chat.clean("   \n  "), "")
	assert_eq(Chat.clean(7), "")
	assert_eq(Chat.clean(null), "")
	assert_eq(Chat.clean(["hello"]), "")
	assert_eq(Chat.clean("x".repeat(500)).length(), Chat.MAX_LENGTH)
	assert_eq(Chat.clean("ça va? ¡sí! 好"), "ça va? ¡sí! 好", "other languages are left alone")


func test_bad_language_is_starred_out() -> void:
	assert_eq(Chat.clean("you fuck"), "you ****")
	assert_eq(Chat.clean("SHIT shot"), "**** shot")
	assert_eq(Chat.clean("what a bitch."), "what a ******")
	for word: String in Chat.BLOCKED:
		assert_true(Chat.is_blocked(word), word)
		assert_true(Chat.is_blocked(word.to_upper()), word)
	# With endings on.
	for word: String in ["fucking", "fucker", "shitty", "bitches", "raped", "wanker", "retards", "dicks"]:
		assert_true(Chat.is_blocked(word), word)


func test_a_word_disguised_is_still_found() -> void:
	for word: String in ["f.u.c.k", "fuuuuck", "sh1t", "$hit", "b!tch", "FuCk", "f_u_c_k", "sh!!!t", "a$$hole", "n1gger", "fuckface", "motherfucker", "c.u.n.t"]:
		assert_true(Chat.is_blocked(word), word)


func test_innocent_words_are_let_be() -> void:
	for word: String in ["grape", "grapes", "scrape", "therapist", "pistol", "pistols", "Scunthorpe", "class", "pass", "passing", "assassin", "cockpit", "dickens",
			"shiitake", "bass", "hello", "shot", "shoot", "duck", "luck", "sheet", "beach", "count", "country", "skill", "kiss", "peace", "raptor", "tw", "a", "?!", "123",
			"cheese", "gouda", "analysis", "button", "title", "happiness", "night", "knight", "bigger", "trigger", "digger", "faggy" if false else "foggy"]:
		assert_false(Chat.is_blocked(word), word)
	assert_eq(Chat.clean("nice shot on the grape pass"), "nice shot on the grape pass")


func test_a_team_message_is_read_by_the_team_and_one_to_all_by_everyone() -> void:
	var sender := _record(CHEDDAR)
	assert_true(Chat.seen_by(sender, _record(CHEDDAR), true, true))
	assert_false(Chat.seen_by(sender, _record(BRIE), true, true), "not by the other side")
	assert_true(Chat.seen_by(sender, _record(BRIE), false, true), "to all is to all")
	assert_true(Chat.seen_by(sender, sender, true, true), "and by whoever said it")
	assert_true(Chat.seen_by(sender, _record(MatchState.NO_TEAM), true, false), "where there are no teams everyone is told")
	assert_false(Chat.seen_by(null, sender, false, true))
	assert_false(Chat.seen_by(sender, null, false, true))
	assert_eq(Chat.line("Brie 7", "hello", false), "Brie 7: hello")
	assert_eq(Chat.line("Brie 7", "hello", true), "[Team] Brie 7: hello")


func test_only_so_much_can_be_said_in_a_while() -> void:
	assert_true(Chat.may_send([], 10.0))
	var times := []
	for sent in Chat.MOST:
		assert_true(Chat.may_send(times, 10.0 + sent * 0.1))
		times.append(10.0 + sent * 0.1)
	assert_false(Chat.may_send(times, 10.5), "that is enough for now")
	assert_true(Chat.may_send(times, 10.0 + Chat.WINDOW + 0.2), "and after a while, more")


func test_the_host_cleans_what_is_said_and_passes_it_to_who_should_read_it() -> void:
	await _start(MatchSettings.TEAMS)
	var match_: Match = world.match_
	var own_team: int = match_.own_record().team
	match_.add_player(5)
	match_.state.player(5).team = own_team
	match_.add_player(6)
	match_.state.player(6).team = BRIE if own_team == CHEDDAR else CHEDDAR
	watch_signals(match_)
	match_._pass_on_chat(5, "  push   the fucking hill\n", true)
	assert_signal_emitted_with_parameters(match_, "chatted", [5, "push the ******* hill", true])
	match_._pass_on_chat(6, "our secret plan", true)
	assert_signal_emit_count(match_, "chatted", 1, "the other team's is not for the host")
	match_._pass_on_chat(6, "good game", false)
	assert_signal_emitted_with_parameters(match_, "chatted", [6, "good game", false])
	# Nothing, not text, from nobody: not passed on.
	match_._pass_on_chat(5, "   ", false)
	match_._pass_on_chat(5, 12, false)
	match_._pass_on_chat(5, "hello", "everyone")
	match_._pass_on_chat(77, "hello", false)
	assert_signal_emit_count(match_, "chatted", 2)
	# The host's own.
	match_.chat("hello all", false)
	assert_signal_emitted_with_parameters(match_, "chatted", [1, "hello all", false])


func test_a_flood_of_messages_is_cut_off() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var match_: Match = world.match_
	match_.add_player(5)
	watch_signals(match_)
	for message in 30:
		match_._pass_on_chat(5, "spam %d" % message, false)
	assert_signal_emit_count(match_, "chatted", Chat.MOST)
	match_._clock += Chat.WINDOW + 0.1
	match_._pass_on_chat(5, "sorry", false)
	assert_signal_emit_count(match_, "chatted", Chat.MOST + 1)
	# A team message where there are no teams is simply a message.
	assert_signal_emitted_with_parameters(match_, "chatted", [5, "sorry", false])


func test_what_is_said_shows_for_a_while_a_few_lines_at_a_time() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	world.match_.add_player(5)
	var box := _box()
	box.set_process(false)
	world.match_.chatted.emit(5, "hello", false)
	assert_eq(box.lines(), ["%s: hello" % world.match_.state.name_of(5)])
	world.match_.chatted.emit(1, "hi", true)
	assert_eq(box.lines().size(), 2)
	assert_string_starts_with(box.lines()[1], "[Team] ")
	for said in ChatBox.LINES + 3:
		box.add(5, "line %d" % said, false)
	assert_eq(box.lines().size(), ChatBox.LINES)
	assert_string_ends_with(box.lines()[-1], "line %d" % (ChatBox.LINES + 2))
	box._process(ChatBox.LIFE + 0.1)
	assert_eq(box.lines(), [])


func test_a_muted_player_is_not_read_and_chat_can_be_turned_off() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	world.match_.add_player(5)
	var box := _box()
	world.voice_chat.set_muted(5, true)
	box.add(5, "you can't see this", false)
	assert_eq(box.lines(), [])
	world.voice_chat.set_muted(5, false)
	box.add(5, "now you can", false)
	assert_eq(box.lines().size(), 1)
	Settings.set_value(ChatBox.SETTING, false)
	box.add(5, "nor this", false)
	assert_eq(box.lines().size(), 1)
	box.open(false)
	assert_false(box.is_typing(), "and nothing can be typed")
	assert_eq(Settings.DEFAULTS[ChatBox.SETTING], true)


func test_typing_holds_the_player_still_and_sends_on_enter() -> void:
	await _start(MatchSettings.TEAMS)
	var box := _box()
	var own: Node3D = world.player_node(1)
	watch_signals(world.match_)
	assert_false(own._menu_open())
	box.open(true)
	assert_true(box.is_typing())
	assert_true(own._menu_open(), "the keys are for typing now")
	assert_eq((box.find_child("ChatEdit", true, false) as LineEdit).max_length, Chat.MAX_LENGTH)
	(box.find_child("ChatEdit", true, false) as LineEdit).text = "  to the hill  "
	box.submit()
	assert_signal_emitted_with_parameters(world.match_, "chatted", [1, "to the hill", true])
	assert_false(box.is_typing())
	assert_false(own._menu_open())
	# And no key typed is taken as play: a digit doesn't change the weapon,
	# nor a letter reload or ping.
	box.open(false)
	assert_true(own.typing())
	for action: String in ["weapon_1", "reload", "ping", "melee", "overload"]:
		var key := InputEventKey.new()
		key.physical_keycode = (InputMap.action_get_events(action).filter(func(event: InputEvent) -> bool: return event is InputEventKey)[0] as InputEventKey).physical_keycode
		key.pressed = true
		own._input(key)
	assert_false(own._reload_pressed)
	assert_false(own._melee_pressed)
	assert_false(own._overload_pressed)
	assert_eq(world.pings.shown(), [])
	box.close()
	assert_false(own.typing())
	# Nothing typed sends nothing; giving up sends nothing.
	box.open(false)
	box.submit()
	box.open(false)
	(box.find_child("ChatEdit", true, false) as LineEdit).text = "never mind"
	box.close()
	assert_signal_emit_count(world.match_, "chatted", 1)
	# Lines stay up while typing, to be answered.
	box.add(1, "wait", false)
	box.open(false)
	box._process(ChatBox.LIFE + 1.0)
	assert_gt(box.lines().size(), 0)


func test_to_the_team_is_to_all_where_there_are_no_teams() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var box := _box()
	watch_signals(world.match_)
	box.open(true)
	(box.find_child("ChatEdit", true, false) as LineEdit).text = "hello"
	box.submit()
	assert_signal_emitted_with_parameters(world.match_, "chatted", [1, "hello", false])


func test_a_push_of_the_stick_picks_from_the_wheel() -> void:
	assert_eq(Wheel.choice(Vector2.ZERO, 8), -1, "resting: nothing")
	assert_eq(Wheel.choice(Vector2(0.1, -0.1), 8), -1, "too slight")
	assert_eq(Wheel.choice(Vector2(0, -1), 8), 0, "up is the first")
	assert_eq(Wheel.choice(Vector2(1, 0), 8), 2, "right is a quarter of the way round")
	assert_eq(Wheel.choice(Vector2(0, 1), 8), 4)
	assert_eq(Wheel.choice(Vector2(-1, 0), 8), 6)
	assert_eq(Wheel.choice(Vector2(1, -1).normalized(), 8), 1)
	assert_eq(Wheel.choice(Vector2(-0.1, -1), 8), 0, "a little to the left of up is still up")
	assert_eq(Wheel.choice(Vector2(0, -1), 0), -1)
	# Every choice of every size of wheel can be reached, by pushing at it.
	for count in range(1, 11):
		for index in count:
			assert_eq(Wheel.choice(Wheel.place(index, count, 1.0), count), index, "%d of %d" % [index, count])
	assert_almost_eq(Wheel.place(0, 4), Vector2(0, -Wheel.RADIUS), Vector2.ONE * 0.01, "the first at the top")
	assert_almost_eq(Wheel.place(1, 4), Vector2(Wheel.RADIUS, 0), Vector2.ONE * 0.01, "and round clockwise")


func test_the_wheel_makes_the_choice_it_is_closed_on() -> void:
	var wheel := Wheel.new()
	add_child_autofree(wheel)
	watch_signals(wheel)
	wheel.open(["a", "b", "c", "d"])
	assert_true(wheel.visible)
	assert_eq(wheel.picked, -1)
	wheel.point(Vector2(1, 0))
	assert_eq(wheel.picked, 1)
	wheel.point(Vector2(0.05, 0))
	assert_eq(wheel.picked, -1, "let go of")
	wheel.point(Vector2(0, 1))
	assert_eq(wheel.close(), 2)
	assert_signal_emitted_with_parameters(wheel, "chosen", [2])
	assert_false(wheel.visible)
	wheel.open(["a", "b"])
	assert_eq(wheel.close(), -1, "closed on nothing")
	assert_signal_emit_count(wheel, "chosen", 1)


func test_a_quick_message_goes_to_the_team() -> void:
	await _start(MatchSettings.TEAMS)
	var box := _box()
	watch_signals(world.match_)
	assert_eq(Chat.QUICK.size(), 8)
	for quick: String in Chat.QUICK:
		assert_eq(Chat.clean(quick), quick, "nothing in them needs cleaning")
	box.wheel_open()
	assert_true(box.wheel.visible)
	assert_eq(box.wheel.choices, Chat.QUICK)
	box.wheel.point(Vector2(1, 0))
	box.wheel_close()
	assert_signal_emitted_with_parameters(world.match_, "chatted", [1, Chat.QUICK[2], true])
	# Closed without sending, or on nothing, nothing is said.
	box.wheel_open()
	box.wheel.point(Vector2(0, 1))
	box.wheel_close(false)
	box.wheel_open()
	box.wheel_close()
	assert_signal_emit_count(world.match_, "chatted", 1)
	# Not while typing.
	box.open(false)
	box.wheel_open()
	assert_false(box.wheel.visible)


func test_the_hud_has_the_compass_the_chat_and_its_wheel() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(3)
	assert_not_null(main.combat_hud.compass)
	assert_not_null(main.combat_hud.chat_box)
	assert_same(main.combat_hud.chat_box.wheel, main.combat_hud.find_child("ChatWheel", true, false))
	var menu: Control = main.settings_menu
	assert_not_null(menu.find_child("Setting_" + ChatBox.SETTING, true, false))
	for action: String in ["ping", "chat_team", "quick_chat"]:
		assert_has(Controls.REBINDABLE, action)
