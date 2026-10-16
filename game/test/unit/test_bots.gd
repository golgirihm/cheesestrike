extends GutTest
## Computer players: what they decide, in an arena made up for the test, and
## how the host's machine carries it out in a real one.

const WORLD := preload("res://world/world.tscn")
const BOARD := preload("res://ui/scoreboard.gd")

var world: Node3D
var match_: Match
var open := func(_from: Vector3, _to: Vector3) -> bool: return false


func _me(at := Vector3.ZERO, health := 100, ammo := 24, reloading := false) -> Dictionary:
	return {"at": at, "health": health, "ammo": ammo, "magazine": 24, "reloading": reloading, "range": 150.0}


func _enemy(id: int, at: Vector3, visible := true) -> Dictionary:
	return {"id": id, "at": at, "visible": visible}


func _places(health := [], roam := [], objective: Variant = null) -> Dictionary:
	return {"health": health, "roam": roam, "objective": objective}


# Thinks for `seconds`, a sixtieth at a time, and returns the last orders and
# how many of those moments it fired in.
func _think_for(brain: BotBrain, seconds: float, me: Dictionary, enemies: Array, places: Dictionary, blocked: Callable = open) -> Dictionary:
	var orders := {}
	var shots := 0
	for step in int(seconds * 60.0):
		orders = brain.think(1.0 / 60.0, me, enemies, places, blocked)
		if orders.fire:
			shots += 1
	orders["shots"] = shots
	return orders


func _start(settings: Dictionary) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict(settings))
	match_ = world.match_
	await wait_physics_frames(2)


# --- Who they are -----------------------------------------------------------

func test_a_bot_is_known_by_an_id_no_player_has() -> void:
	assert_true(Bots.is_bot(Bots.id_for(0)))
	assert_false(Bots.is_bot(1))
	assert_false(Bots.is_bot(2000000000))
	var ids := {}
	for index in MatchSettings.MAX_PLAYERS:
		ids[Bots.id_for(index)] = true
		assert_true(Bots.name_for(Bots.id_for(index)).begins_with(Bots.TAG))
		assert_eq(PlayerName.clean(Bots.name_for(Bots.id_for(index))), Bots.name_for(Bots.id_for(index)), "a name the game accepts")
		assert_has(Weapons.MAINS, Bots.loadout_for(Bots.id_for(index))[0])
		assert_eq(Looks.clean(Bots.look_for(Bots.id_for(index))), Bots.look_for(Bots.id_for(index)))
	assert_eq(ids.size(), MatchSettings.MAX_PLAYERS)
	assert_ne(Bots.name_for(-1), Bots.name_for(-2))


func test_no_bot_or_target_is_the_yellow_of_cheese() -> void:
	const PLAYER := preload("res://player/player.gd")
	for index in MatchSettings.MAX_PLAYERS:
		for id: int in [Bots.id_for(index), Bots.target_id(index)]:
			assert_false(Scenery.is_yellow(PLAYER.colour_for(id)), str(id))


func test_bots_make_way_for_players() -> void:
	var settings := MatchSettings.from_dict({"bots": 5, "max_players": 6})
	assert_eq(Bots.wanted(settings, 1), 5)
	assert_eq(Bots.wanted(settings, 2), 4)
	assert_eq(Bots.wanted(settings, 6), 0)
	assert_eq(Bots.wanted(MatchSettings.new(), 1), 0, "none unless asked for")
	assert_eq(MatchSettings.from_dict({"bots": 99}).bots, MatchSettings.MAX_PLAYERS - 1)
	assert_eq(MatchSettings.from_dict({"bots": -4}).bots, 0)
	assert_eq(MatchSettings.from_dict({"bot_skill": "godlike"}).bot_skill, BotBrain.NORMAL)
	assert_false(settings.advertised().has("bots"), "the session list's server isn't told of them")
	assert_has(Bots.COUNTS, 0)


# --- What they decide -------------------------------------------------------

func test_with_nobody_about_it_wanders_to_somewhere_and_then_somewhere_else() -> void:
	var brain := BotBrain.new(BotBrain.NORMAL, 3)
	var spots := [Vector3(10, 0, 0), Vector3(-10, 0, 0), Vector3(0, 0, 10)]
	var orders := brain.think(0.1, _me(), [], _places([], spots), open)
	assert_has(spots, brain.goal)
	assert_almost_eq(orders.move, (brain.goal as Vector3).normalized(), Vector3.ONE * 0.001, "toward it")
	assert_null(orders.look, "looking where it walks")
	assert_false(orders.fire)
	assert_true(orders.run, "at a run, with nothing to fight")
	var first: Vector3 = brain.goal
	var seen := {}
	for visit in 12:
		brain.think(0.1, _me(brain.goal), [], _places([], spots), open)
		seen[brain.goal] = true
	assert_gt(seen.size(), 1, "it doesn't stand on %s for ever" % first)


func test_with_nowhere_to_go_it_stands() -> void:
	var orders := BotBrain.new().think(0.1, _me(), [], _places(), open)
	assert_eq(orders.move, Vector3.ZERO)
	assert_false(orders.jump)


func test_it_goes_where_the_mode_wants_it() -> void:
	var brain := BotBrain.new()
	var orders := brain.think(0.1, _me(), [], _places([], [Vector3(-9, 0, 0)], Vector3(20, 0, 0)), open)
	assert_almost_eq(orders.move, Vector3.RIGHT, Vector3.ONE * 0.001)


func test_it_fights_the_nearest_enemy_it_can_see() -> void:
	var brain := BotBrain.new()
	var enemies := [_enemy(5, Vector3(0, 0, -30)), _enemy(6, Vector3(0, 0, -8), false), _enemy(7, Vector3(12, 0, 0))]
	brain.think(0.1, _me(), enemies, _places(), open)
	assert_eq(brain.target, 7, "not the one behind the wall, though nearer")
	# It stays on them while it can see them, though another comes closer.
	enemies[0].at = Vector3(0, 0, -5)
	brain.think(0.1, _me(), enemies, _places(), open)
	assert_eq(brain.target, 7)
	enemies[2].visible = false
	brain.think(0.1, _me(), enemies, _places(), open)
	assert_eq(brain.target, 5)
	assert_eq(brain.seen_for, 0.0, "and starts again with a new one")


func test_it_takes_a_moment_to_notice_and_the_better_it_is_the_shorter() -> void:
	var enemy := [_enemy(5, Vector3(0, 0, -15))]
	var last := INF
	for skill: String in BotBrain.SKILLS:
		var brain := BotBrain.new(skill, 1)
		var waited := 0.0
		while not brain.think(1.0 / 60.0, _me(), enemy, _places(), open).fire and waited < 5.0:
			waited += 1.0 / 60.0
		assert_almost_eq(waited, BotBrain.LEVELS[skill].reaction, 0.05, skill)
		assert_lt(waited, last, "%s is quicker than the one before" % skill)
		last = waited


func test_it_aims_at_the_middle_of_its_enemy_and_steadies_as_it_tracks() -> void:
	var feet := Vector3(0, 0, -20)
	var middle := feet + Vector3.UP * BodyShape.CHEST.y * BodyShape.SIZE
	for skill: String in BotBrain.SKILLS:
		var brain := BotBrain.new(skill, 9)
		var worst := 0.0
		for step in 300:
			var look: Vector3 = brain.think(1.0 / 60.0, _me(), [_enemy(5, feet)], _places(), open).look
			worst = maxf(worst, look.distance_to(middle))
		var most := 20.0 * tan(deg_to_rad(BotBrain.LEVELS[skill].error)) * 1.3
		assert_lt(worst, most, "%s is never further off than its skill" % skill)
		assert_gt(worst, 0.0, "%s is never exact" % skill)
	assert_gt(BotBrain.aim_error(BotBrain.EASY, 0.0), BotBrain.aim_error(BotBrain.NORMAL, 0.0))
	assert_gt(BotBrain.aim_error(BotBrain.NORMAL, 0.0), BotBrain.aim_error(BotBrain.HARD, 0.0))
	for skill: String in BotBrain.SKILLS:
		assert_gt(BotBrain.aim_error(skill, 0.0), BotBrain.aim_error(skill, BotBrain.STEADY_TIME), skill)
		assert_eq(BotBrain.aim_error(skill, 99.0), BotBrain.aim_error(skill, BotBrain.STEADY_TIME), "%s only steadies so far" % skill)
		assert_gt(BotBrain.aim_error(skill, 99.0), 0.3, "%s never aims like a machine" % skill)


func test_it_fires_in_bursts_with_a_rest_between() -> void:
	var brain := BotBrain.new(BotBrain.NORMAL, 2)
	var level: Dictionary = BotBrain.LEVELS[BotBrain.NORMAL]
	var fired := []
	for step in 240:
		fired.append(brain.think(1.0 / 60.0, _me(), [_enemy(5, Vector3(0, 0, -15))], _places(), open).fire)
	var first := fired.find(true)
	assert_eq(fired.slice(first, first + level.burst), range(level.burst).map(func(_i: int) -> bool: return true), "a burst")
	assert_false(fired[first + level.burst], "then a rest")
	assert_gt(fired.slice(first + level.burst).count(true), 0, "and another")


func test_it_holds_its_fire_out_of_range_empty_or_reloading() -> void:
	var enemy := [_enemy(5, Vector3(0, 0, -40))]
	var far := _me()
	far.range = 30.0
	assert_eq(_think_for(BotBrain.new(), 2.0, far, enemy, _places()).shots, 0, "too far for the weapon")
	var empty := _think_for(BotBrain.new(), 2.0, _me(Vector3.ZERO, 100, 0), enemy, _places())
	assert_eq(empty.shots, 0)
	assert_true(empty.reload, "it reloads instead")
	var busy := _think_for(BotBrain.new(), 2.0, _me(Vector3.ZERO, 100, 0, true), enemy, _places())
	assert_eq(busy.shots, 0)
	assert_false(busy.reload, "it is already")


func test_it_tops_up_when_nobody_is_about() -> void:
	assert_true(BotBrain.new().think(0.1, _me(Vector3.ZERO, 100, 5), [], _places(), open).reload)
	assert_false(BotBrain.new().think(0.1, _me(Vector3.ZERO, 100, 20), [], _places(), open).reload, "not with most of a magazine")
	assert_false(BotBrain.new().think(0.1, _me(Vector3.ZERO, 100, 5), [_enemy(5, Vector3(0, 0, -9))], _places(), open).reload, "nor in a fight")


func test_it_sidesteps_in_a_fight_one_way_and_then_the_other() -> void:
	var brain := BotBrain.new(BotBrain.NORMAL, 4)
	var enemy := [_enemy(5, Vector3(0, 0, -15))]
	var sides := {}
	for step in 600:
		var move: Vector3 = brain.think(1.0 / 60.0, _me(), enemy, _places(), open).move
		assert_almost_eq(move.length(), 1.0, 0.001)
		sides[signf(move.x)] = true
	assert_true(sides.has(1.0) and sides.has(-1.0))


func test_it_closes_on_a_far_enemy_and_backs_off_a_near_one() -> void:
	var far := BotBrain.new(BotBrain.NORMAL, 4).think(0.1, _me(), [_enemy(5, Vector3(0, 0, -80))], _places(), open)
	assert_lt(far.move.z, -0.5, "toward them")
	var near := BotBrain.new(BotBrain.NORMAL, 4).think(0.1, _me(), [_enemy(5, Vector3(0, 0, -3))], _places(), open)
	assert_gt(near.move.z, 0.5, "away from them")


func test_hurt_it_goes_for_the_nearest_wedge_still_shooting() -> void:
	var brain := BotBrain.new(BotBrain.NORMAL, 4)
	var wedges := [Vector3(30, 0, 0), Vector3(-8, 0, 0)]
	var orders := brain.think(0.1, _me(Vector3.ZERO, 30), [_enemy(5, Vector3(0, 0, -15))], _places(wedges), open)
	assert_almost_eq(orders.move, Vector3.LEFT, Vector3.ONE * 0.001)
	assert_false(orders.run, "it can't run and aim")
	assert_gt(_think_for(brain, 2.0, _me(Vector3.ZERO, 30), [_enemy(5, Vector3(0, 0, -15))], _places(wedges)).shots, 0)
	var healthy := BotBrain.new(BotBrain.NORMAL, 4).think(0.1, _me(Vector3.ZERO, 90), [], _places(wedges, [Vector3(0, 0, 9)]), open)
	assert_almost_eq(healthy.move, Vector3.BACK, Vector3.ONE * 0.001, "unhurt, it has no use for one")
	var none := BotBrain.new(BotBrain.NORMAL, 4).think(0.1, _me(Vector3.ZERO, 30), [], _places([], [Vector3(0, 0, 9)]), open)
	assert_almost_eq(none.move, Vector3.BACK, Vector3.ONE * 0.001, "with none to be had, it carries on")


func test_it_goes_round_what_is_in_its_way() -> void:
	# A wall across the way ahead, open to either side.
	var wall := func(from: Vector3, to: Vector3) -> bool: return signf(from.z + 2.0) != signf(to.z + 2.0) and absf(to.x) < 2.0
	var orders := BotBrain.new().think(0.1, _me(), [], _places([], [], Vector3(0, 0, -20)), wall)
	assert_lt(orders.move.z, 0.0, "still getting on")
	assert_gt(absf(orders.move.x), 0.5, "but to one side")
	assert_false(orders.jump)
	assert_almost_eq(orders.move.length(), 1.0, 0.001)


func test_boxed_in_it_jumps() -> void:
	var everywhere := func(_from: Vector3, _to: Vector3) -> bool: return true
	var orders := BotBrain.new().think(0.1, _me(), [], _places([], [], Vector3(0, 0, -20)), everywhere)
	assert_true(orders.jump)
	assert_ne(orders.move, Vector3.ZERO)


func test_getting_nowhere_it_jumps_and_tries_another_way() -> void:
	var brain := BotBrain.new(BotBrain.NORMAL, 6)
	var spots := [Vector3(0, 0, -20)]
	var jumped := false
	var ways := {}
	# It walks and walks, and is never anywhere else.
	for step in int((BotBrain.STUCK_AFTER * 3.0) * 60.0):
		var orders := brain.think(1.0 / 60.0, _me(), [], _places([], spots), open)
		jumped = jumped or orders.jump
		ways[orders.move.snapped(Vector3.ONE * 0.1)] = true
	assert_true(jumped)
	assert_gt(ways.size(), 1)


func test_getting_somewhere_it_is_not_stuck() -> void:
	var brain := BotBrain.new(BotBrain.NORMAL, 6)
	var at := Vector3.ZERO
	for step in 300:
		var orders := brain.think(1.0 / 60.0, _me(at), [], _places([], [], Vector3(0, 0, -200)), open)
		assert_false(orders.jump)
		at += orders.move * 0.1


# --- Where the mode sends them ---------------------------------------------

func test_in_capture_it_goes_for_the_wheel_and_then_home() -> void:
	const DRIVER := preload("res://bots/bot_driver.gd")
	var state := MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.CAPTURE}))
	var record := state.add_player(-1)
	var spots := {"bases": [Vector3(-20, 0, 0), Vector3(20, 0, 0)], "hills": [], "points": []}
	state.capture.reset(spots.bases)
	var theirs: Vector3 = spots.bases[1 - record.team]
	assert_eq(DRIVER.objective_for(state, record, spots), theirs)
	state.capture.wheels[1 - record.team].status = CaptureRules.DROPPED
	state.capture.wheels[1 - record.team].at = Vector3(3, 0, 3)
	assert_eq(DRIVER.objective_for(state, record, spots), Vector3(3, 0, 3), "wherever it lies")
	state.capture.wheels[1 - record.team].status = CaptureRules.CARRIED
	state.capture.wheels[1 - record.team].carrier = -1
	assert_eq(DRIVER.objective_for(state, record, spots), spots.bases[record.team], "home with it")
	state.capture.wheels[1 - record.team].carrier = 7
	assert_null(DRIVER.objective_for(state, record, spots), "a teammate has it: nothing to fetch")


func test_it_goes_to_the_hill_and_to_points_its_team_lacks() -> void:
	const DRIVER := preload("res://bots/bot_driver.gd")
	var spots := {"bases": [], "hills": [Vector3(1, 0, 0), Vector3(2, 0, 0)], "points": [Vector3(-9, 0, 0), Vector3.ZERO, Vector3(9, 0, 0)]}
	var hill := MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.HILL}))
	var on_hill := hill.add_player(-1)
	hill.zones.hill = 1
	assert_eq(DRIVER.objective_for(hill, on_hill, spots), Vector3(2, 0, 0))
	var points := MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.POINTS}))
	var taker := points.add_player(-1)
	points.zones.owners = [taker.team, taker.team, ZoneRules.NOBODY] as Array[int]
	assert_eq(DRIVER.objective_for(points, taker, spots), Vector3(9, 0, 0), "the one they don't have")
	points.zones.owners = [taker.team, taker.team, taker.team] as Array[int]
	assert_null(DRIVER.objective_for(points, taker, spots), "with all three, it is free to fight")
	var plain := MatchState.new()
	assert_null(DRIVER.objective_for(plain, plain.add_player(-1), spots))


# --- In the arena -----------------------------------------------------------

func test_the_host_adds_the_bots_it_asked_for_and_the_match_begins() -> void:
	await _start({"map": "box", "bots": 3})
	assert_eq(world.bot_ids(), [-3, -2, -1])
	assert_eq(world.players.get_child_count(), 4)
	assert_false(match_.state.is_warm_up(), "someone to play against")
	for id: int in world.bot_ids():
		var record := match_.state.player(id)
		var node: Node3D = world.player_node(id)
		assert_eq(record.name, Bots.name_for(id))
		assert_eq(record.weapons, [Weapons.STARTING, Bots.loadout_for(id)[0]])
		assert_true(node.is_bot)
		assert_true(node.driven)
		assert_false(node._mine, "nobody sees through it")
		assert_eq(node.get_multiplayer_authority(), 1, "the host's to move")
		assert_not_null(node.get_node_or_null("BotDriver"))
		assert_true(node.body.shown)
	var own: Node3D = world.player_node(1)
	assert_true(own._mine)
	assert_null(own.get_node_or_null("BotDriver"))
	assert_false(own.is_bot)


func test_bots_fill_teams_evenly() -> void:
	await _start({"map": "box", "bots": 3, "mode": "teams"})
	var teams := [0, 0]
	for record: PlayerRecord in match_.state.players.values():
		teams[record.team] += 1
	assert_eq(teams, [2, 2])


func test_a_bot_gives_up_its_place_to_a_player_and_comes_back_when_they_go() -> void:
	await _start({"map": "box", "bots": 3, "max_players": 4})
	assert_eq(world.bot_ids().size(), 3)
	world._add_player(77)
	assert_eq(world.bot_ids(), [-2, -1], "the last to come is the first to go")
	assert_eq(match_.state.players.size(), 4)
	assert_null(world.player_node(-3))
	world._remove_player(77)
	assert_eq(world.bot_ids(), [-3, -2, -1])
	assert_eq(match_.state.players.size(), 4)


func test_a_session_without_bots_has_none() -> void:
	await _start({"map": "box"})
	assert_eq(world.bot_ids(), [])
	assert_eq(world.players.get_child_count(), 1)
	assert_true(match_.state.is_warm_up())


func test_the_scoreboard_marks_a_bot() -> void:
	var record := PlayerRecord.new(-1)
	assert_eq(BOARD.ping_text(record, null), "bot")


func test_a_bot_walks_about_and_tells_everyone_where_it_is() -> void:
	await _start({"map": "box", "bots": 1})
	var bot: CharacterBody3D = world.player_node(-1)
	# Keep the host's own player out of its sight, so it wanders.
	world.player_node(1).global_position = Vector3(0, 40, 0)
	match_.state.player(1).alive = false
	var from := bot.global_position
	await wait_physics_frames(150)
	assert_gt(Vector2(bot.global_position.x - from.x, bot.global_position.z - from.z).length(), 3.0, "it has gone somewhere")
	assert_almost_eq(bot.net_state[0], bot.global_position.x, 0.01, "and says where")
	assert_almost_eq(bot.net_state[2], bot.global_position.z, 0.01)
	assert_gt(bot.global_position.y, -0.5, "on the floor, not through it")
	assert_true(match_._histories.has(-1), "the host keeps where it was, to judge shots at it by")


func test_a_bot_shoots_a_player_it_can_see_and_can_be_shot() -> void:
	await _start({"map": "box", "bots": 1, "bot_skill": "hard"})
	var own: Node3D = world.player_node(1)
	var bot: CharacterBody3D = world.player_node(-1)
	own.global_position = Vector3(11, 0.1, -11)
	bot.global_position = Vector3(11, 0.1, 1)
	match_.state.player(1).protected_for = 0.0
	match_.state.player(-1).protected_for = 0.0
	watch_signals(match_)
	var waited := 0
	while match_.state.player(1).health == PlayerRecord.MAX_HEALTH and match_.state.player(1).alive and waited < 600:
		own.global_position = Vector3(11, 0.1, -11)
		await wait_physics_frames(1)
		waited += 1
	assert_lt(waited, 600, "it hit them within ten seconds")
	assert_signal_emitted(match_, "shot_shown")
	assert_gt(match_.tally.of(match_.state.player(-1).weapon).shots, 0, "through the host's own checks")
	# And it can be hit, where the host sees it.
	var before: int = match_.state.player(-1).health
	var origin: Vector3 = own.head.global_position
	var middle: Vector3 = bot.global_position + Vector3.UP * BodyShape.CHEST.y * BodyShape.SIZE
	match_._shot(1, origin, (middle - origin).normalized(), 1, "blaster", WeaponStats.AIMED)
	assert_lt(match_.state.player(-1).health, before)


func test_a_knocked_out_bot_stands_still_and_comes_back_refilled() -> void:
	await _start({"map": "box", "bots": 1})
	var bot: CharacterBody3D = world.player_node(-1)
	var driver: Node = bot.get_node("BotDriver")
	await wait_physics_frames(3)
	driver._weapon(match_.state.player(-1)).ammo = 1
	match_.state.player(-1).protected_for = 0.0
	match_.state.damage(1, -1, 500.0)
	var fell := bot.global_position
	await wait_physics_frames(20)
	assert_almost_eq(bot.global_position.x, fell.x, 0.01)
	match_._bring_back(-1, Vector3(-12, 0.1, 12))
	assert_almost_eq(bot.global_position.x, -12.0, 0.01, "the host puts it there itself")
	assert_true(match_.state.player(-1).alive)
	for carried: WeaponState in driver.weapons.values():
		assert_eq(carried.ammo, carried.stats.magazine)
	assert_false(match_._sent_to.has(-1), "nothing to wait for")


func test_a_full_session_of_bots_plays_itself_cheaply() -> void:
	await _start({"map": "canyon", "bots": 15, "bot_skill": "normal", "kill_limit": 50})
	assert_eq(world.players.get_child_count(), 16)
	# The host's own player stays out of it.
	match_.state.player(1).alive = false
	world.player_node(1).global_position = Vector3(0, 60, 0)
	var worst := 0
	var total := 0
	var frames := 900
	for frame in frames:
		await wait_physics_frames(1)
		var cost := 0
		for id: int in world.bot_ids():
			cost += world.player_node(id).get_node("BotDriver").last_cost
		worst = maxi(worst, cost)
		total += cost
	var kills := 0
	var moved := 0
	for id: int in world.bot_ids():
		kills += match_.state.player(id).kills
		var node: Node3D = world.player_node(id)
		assert_gt(node.global_position.y, -2.0, "%d fell out of the arena" % id)
		assert_true(node.global_position.is_finite())
		if node.velocity.length() > 0.1 or match_.state.player(id).deaths > 0 or match_.state.player(id).kills > 0:
			moved += 1
	assert_gt(kills, 0, "fifteen seconds of fifteen bots and somebody is knocked out")
	assert_gt(moved, 10, "most of them are up to something")
	var average := float(total) / frames
	gut.p("15 bots: %.0f µs a frame on average, %d at worst" % [average, worst])
	# A sixtieth of a second is 16,667 µs: all of them together use a small part of it.
	assert_lt(average, 4000.0, "cheap enough for the host")
