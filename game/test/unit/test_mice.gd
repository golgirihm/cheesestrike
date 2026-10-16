extends GutTest
## The giant mice: they all come a minute into a match and eat the cheese,
## low and near first, going round what isn't cheese; shooting one enough
## sends it running.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")
const EDGE := Rect2(-50, -50, 100, 100)

var world: Node3D
var match_: Match


# Blocks of cheese 2 m across: one at each of `places` (x, y, z of its
# bottom middle), all standing.
func _cheese(places: Array) -> Dictionary:
	var boxes := []
	for at: Vector3 in places:
		boxes.append(AABB(at - Vector3(1, 0, 1), Vector3(2, 2, 2)))
	return {"blocks": BlockRules.new(boxes.size()), "boxes": boxes}


# A mouse at `at`, as the host has one.
func _mouse(at: Vector3, mode := MouseRules.WALKING) -> Dictionary:
	return MouseRules._mouse(at, 0.0, mode)


# Mice that have just come, with nothing yet chosen.
func _arrived(seed := 1) -> MouseRules:
	var rules := MouseRules.new(seed)
	rules.invade_in = 0.0
	return rules


# --- The rules alone --------------------------------------------------------

func test_they_come_a_minute_in_from_the_edge_facing_in() -> void:
	var cheese := _cheese([Vector3.ZERO])
	var rules := MouseRules.new(1)
	assert_eq(rules.invade_in, MouseRules.INVADE_AFTER)
	assert_eq(MouseRules.INVADE_AFTER, 60.0, "about a minute")
	var done := rules.tick(30.0, cheese.blocks, cheese.boxes, EDGE)
	assert_eq(done.came, 0, "not while the countdown runs")
	assert_eq(rules.mice.size(), 0)
	rules.invade_in = 0.0
	done = rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq(done.came, MouseRules.FIRST, "all of them, for so little cheese a pair")
	assert_true(rules.arrived)
	for mouse: Dictionary in rules.mice:
		var at := Vector2(mouse.at.x, mouse.at.z)
		assert_true(EDGE.has_point(at), "inside the arena")
		assert_false(EDGE.grow(-MouseRules.LENGTH).has_point(at), "but at its edge")
		var facing := Vector3.FORWARD.rotated(Vector3.UP, mouse.heading)
		assert_gt(facing.dot(-mouse.at.normalized()), 0.0, "facing in")


func test_the_countdown_runs_only_in_a_match_under_way() -> void:
	var state := MatchState.new(MatchSettings.from_dict({"map": "box"}))
	state.blocks = BlockRules.new(5)
	MatchState.countdown_length = MatchState.COUNTDOWN
	state.start_match()
	MatchState.countdown_length = 0.0
	state.tick(state.countdown)
	assert_eq(state.mice.invade_in, MouseRules.INVADE_AFTER, "not through the match's own countdown")
	state.tick(10.0)
	assert_almost_eq(state.mice.invade_in, MouseRules.INVADE_AFTER - 10.0, 0.001)
	state.tick(MouseRules.INVADE_AFTER)
	assert_eq(state.mice.invade_in, 0.0, "and stops at nothing")
	state.start_match()
	assert_eq(state.mice.invade_in, MouseRules.INVADE_AFTER, "a new match counts down again")
	state.tick(10.0)
	assert_almost_eq(state.mice.invade_in, MouseRules.INVADE_AFTER - 10.0, 0.001, "from its beginning, with no countdown of its own")
	var practice := MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.PRACTICE}))
	practice.blocks = BlockRules.new(5)
	practice.start_match()
	practice.tick(MouseRules.INVADE_AFTER + 1.0)
	assert_eq(practice.mice.invade_in, MouseRules.INVADE_AFTER, "never on the practice range")
	var bare := MatchState.new()
	bare.countdown = 0.0
	bare.tick(MouseRules.INVADE_AFTER + 1.0)
	assert_eq(bare.mice.invade_in, MouseRules.INVADE_AFTER, "nor where there is no cheese")


func test_as_many_as_the_cheese_is_worth_come_at_once_from_all_round() -> void:
	assert_eq(MouseRules.most_for(13), MouseRules.FIRST, "a pair on a small arena")
	assert_eq(MouseRules.most_for(600), MouseRules.MOST, "a pack on a large map")
	var places := []
	for index in 400:
		places.append(Vector3((index % 20) * 4.0 - 40.0, 0, (index / 20) * 4.0 - 40.0))
	var cheese := _cheese(places)
	var rules := _arrived()
	var done := rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq(done.came, MouseRules.most_for(400), "all together")
	assert_eq(rules.mice.size(), MouseRules.most_for(400))
	for minute in 3:
		done = rules.tick(60.0, cheese.blocks, cheese.boxes, EDGE)
		assert_eq(done.came, 0, "and no more after")
	assert_eq(rules.mice.size(), MouseRules.most_for(400))
	# Each from a different way round, not bunched on one side.
	var sides := {}
	rules = _arrived(3)
	rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	for mouse: Dictionary in rules.mice:
		var at := Vector2(mouse.at.x, mouse.at.z)
		sides[Vector2(signf(at.x) if absf(at.x) > absf(at.y) else 0.0, signf(at.y) if absf(at.y) >= absf(at.x) else 0.0)] = true
	assert_eq(sides.size(), 4, "from every side")


func test_a_mouse_goes_to_the_nearest_cheese_and_eats_it_then_the_next() -> void:
	var cheese := _cheese([Vector3(30, 0, 0), Vector3(-30, 0, 0)])
	var rules := _arrived()
	rules.mice.append(_mouse(Vector3(45, 0, 0)))
	rules.arrived = true
	var eaten := 0
	var seconds := 0.0
	while cheese.blocks.standing(0) and seconds < 30.0:
		var done := rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
		for bite: Dictionary in done.bites:
			assert_eq(bite.index, 0, "the nearer one first")
			cheese.blocks.hurt(bite.index, bite.amount)
			eaten += 1
		seconds += 0.1
	assert_false(cheese.blocks.standing(0), "eaten")
	assert_eq(eaten * MouseRules.BITE, BlockRules.HEALTH, "bite by bite")
	assert_lt(seconds, 10.0, "within seconds of getting there")
	# Then it sets off for the other.
	var left_from: float = rules.mice[0].at.x
	for step in 10:
		rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq(rules.mice[0].target, 1)
	assert_eq(rules.mice[0].mode, MouseRules.WALKING)
	assert_almost_eq(rules.mice[0].at.x, left_from - MouseRules.SPEED, 0.01, "at a walk")


func test_low_cheese_comes_first_and_two_mice_share_none_while_there_is_more() -> void:
	# A block high up, right by them; two on the ground a little further off.
	var cheese := _cheese([Vector3(0, 14, 0), Vector3(15, 0, 0), Vector3(-15, 0, 0)])
	var rules := _arrived()
	for x: float in [2.0, -2.0]:
		rules.mice.append(_mouse(Vector3(x, 0, 0)))
	rules.arrived = true
	rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq([rules.mice[0].target, rules.mice[1].target], [1, 2], "each its own, low down")
	# With only the high one left, both go up for it.
	cheese.blocks.remove(1)
	cheese.blocks.remove(2)
	for step in 100:
		rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq([rules.mice[0].target, rules.mice[1].target], [0, 0])
	assert_almost_eq(rules.mice[0].at.y, 14.0, 0.01, "climbing to it")


func test_a_mouse_at_the_very_edge_of_its_reach_eats() -> void:
	# Where rounding once left mice creeping at their cheese for ever.
	var cheese := _cheese([Vector3(-62, 0, -178)])
	cheese.boxes[0] = AABB(Vector3(-72, 0, -184), Vector3(20, 8, 12))
	var rules := _arrived()
	rules.mice.append(_mouse(Vector3(-40, 0, -195)))
	rules.arrived = true
	var bites := 0
	for step in 50:
		bites += rules.tick(0.1, cheese.blocks, cheese.boxes, Rect2(-320, -200, 640, 400)).bites.size()
	assert_eq(rules.mice[0].mode, MouseRules.EATING)
	assert_gt(bites, 0)


func test_a_mouse_goes_round_what_is_tall_and_over_what_is_low() -> void:
	var cheese := _cheese([Vector3(0, 0, -40)])
	var rock := AABB(Vector3(-8, 0, -24), Vector3(16, 20, 4))
	var rules := _arrived()
	rules.mice.append(_mouse(Vector3(0, 0, 0)))
	rules.arrived = true
	var bites := 0
	for step in 150:
		bites += rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE, [rock]).bites.size()
		assert_false(MouseRules._body(rules.mice[0].at).intersects(rock), "never through the rock")
	assert_gt(bites, 0, "round it to the cheese")
	# A car it can step up onto is in its way, so it walks over it.
	cheese = _cheese([Vector3(0, 0, -40)])
	var car := AABB(Vector3(-8, 0, -24), Vector3(16, 2.5, 4))
	rules = _arrived()
	rules.mice.append(_mouse(Vector3(0, 0, 0)))
	rules.arrived = true
	var highest := 0.0
	for step in 40:
		rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE, [car])
		highest = maxf(highest, rules.mice[0].at.y)
		assert_almost_eq(rules.mice[0].at.x, 0.0, 0.01, "straight on")
	assert_gt(highest, 1.0, "up onto it")


func test_a_mouse_that_cannot_get_to_its_cheese_tries_other_cheese() -> void:
	# The nearer block is shut in on all sides; the further one isn't.
	var cheese := _cheese([Vector3(0, 0, -30), Vector3(40, 0, 0)])
	var walls := [AABB(Vector3(-14, 0, -46), Vector3(28, 20, 2)), AABB(Vector3(-14, 0, -16), Vector3(28, 20, 2)),
			AABB(Vector3(-14, 0, -46), Vector3(2, 20, 32)), AABB(Vector3(12, 0, -46), Vector3(2, 20, 32))]
	var rules := _arrived()
	rules.mice.append(_mouse(Vector3(0, 0, 0)))
	rules.arrived = true
	rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE, walls)
	assert_eq(rules.mice[0].target, 0, "the nearer first")
	var bites := []
	for step in 200:
		for bite: Dictionary in rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE, walls).bites:
			bites.append(bite.index)
	assert_true(bites.has(1), "gives up on it for the other")
	assert_false(bites.has(0))


func test_a_mouse_runs_only_once_it_has_been_hurt_enough() -> void:
	var rules := _arrived()
	rules.mice.append(_mouse(Vector3(0, 0, -20), MouseRules.EATING))
	assert_false(rules.hurt(0, MouseRules.BRAVERY / 2.0, Vector3.ZERO), "brave at first")
	assert_eq(rules.mice[0].mode, MouseRules.EATING)
	assert_false(rules.hurt(5, 1000.0, Vector3.ZERO), "no such mouse")
	assert_false(rules.hurt(0, NAN, Vector3.ZERO))
	# What it has taken wears off, a while later.
	var cheese := _cheese([Vector3(0, 0, -26)])
	rules.arrived = true
	for step in 100:
		rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq(rules.mice[0].fear, 0.0, "calm again")
	assert_false(rules.hurt(0, MouseRules.BRAVERY - 1.0, Vector3.ZERO))
	assert_true(rules.hurt(0, 1.0, Vector3.ZERO), "and runs once it has had enough")
	assert_eq(rules.mice[0].mode, MouseRules.FLEEING)
	assert_false(rules.hurt(0, MouseRules.BRAVERY, Vector3.ZERO), "running already")


func test_a_shot_through_a_mouse_hits_it_and_one_past_it_does_not() -> void:
	var rules := _arrived()
	rules.mice.append(_mouse(Vector3(0, 0, -20), MouseRules.EATING))
	rules.mice.append(_mouse(Vector3(0, 0, -40), MouseRules.EATING))
	var eye := Vector3(0, 3, 0)
	assert_eq(rules.hit(eye, Vector3.FORWARD, 100.0), 0, "the nearer of two in line")
	assert_eq(rules.hit(eye, Vector3.FORWARD, 10.0), -1, "out of the shot's reach")
	assert_eq(rules.hit(eye, Vector3.BACK, 100.0), -1, "behind")
	assert_eq(rules.hit(eye + Vector3.RIGHT * (MouseRules.HIT_RADIUS + 1.0), Vector3.FORWARD, 100.0), -1, "beside")
	assert_eq(rules.near(Vector3(0, 3, -26), 3.0), [0] as Array[int])
	assert_eq(rules.near(Vector3(0, 3, -30), 6.0), [0, 1] as Array[int])


func test_a_scared_mouse_runs_away_from_whoever_scared_it_and_then_eats_again() -> void:
	var cheese := _cheese([Vector3(0, 0, -20)])
	var rules := _arrived()
	rules.mice.append(_mouse(Vector3(0, 0, -14)))
	rules.arrived = true
	rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq(rules.mice[0].mode, MouseRules.EATING)
	assert_true(rules.scare(0, Vector3(0, 0, 0)))
	assert_false(rules.scare(0, Vector3(0, 0, 0)), "it was running already")
	assert_false(rules.scare(5, Vector3.ZERO), "no such mouse")
	var bites := 0
	for step in int(MouseRules.FLEE_TIME * 10.0) - 1:
		bites += rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE).bites.size()
		assert_eq(rules.mice[0].mode, MouseRules.FLEEING)
		assert_true(EDGE.has_point(Vector2(rules.mice[0].at.x, rules.mice[0].at.z)), "never out of the arena")
		if step == 9:
			assert_almost_eq(rules.mice[0].at.z, -14.0 - MouseRules.FLEE_SPEED, 0.5, "away from the one who fired, past the cheese, at a run")
	assert_eq(bites, 0, "no eating on the run")
	for step in 100:
		rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	assert_eq(rules.mice[0].mode, MouseRules.EATING, "and back to it once it has run off")


func test_the_mice_survive_the_trip_and_nonsense_is_none() -> void:
	var state := MatchState.new()
	state.blocks = BlockRules.new(3)
	state.mice = _arrived()
	state.mice.arrived = true
	state.mice.mice.append(_mouse(Vector3(4, 1, -2), MouseRules.FLEEING))
	state.mice.mice[0].heading = 1.5
	var copy := MatchState.from_dict(bytes_to_var(var_to_bytes(state.to_dict(false))))
	assert_true(copy.mice.arrived)
	assert_eq(copy.mice.mice.size(), 1)
	assert_almost_eq(copy.mice.mice[0].at.distance_to(Vector3(4, 1, -2)), 0.0, 0.001)
	assert_almost_eq(copy.mice.mice[0].heading, 1.5, 0.001)
	assert_eq(copy.mice.mice[0].mode, MouseRules.FLEEING)
	assert_false(MatchState.new().to_dict().has("mice"), "nothing sent where there is no cheese")
	for nonsense: Variant in [null, "mice", [1, 2], [INF, true, PackedFloat32Array([NAN, 0, 0, 0, 0])], [-5, 7, PackedFloat32Array([1, 2])]]:
		var read := MouseRules.from_array(nonsense)
		assert_eq(read.mice.size(), 0, str(nonsense))
		assert_between(read.invade_in, 0.0, MouseRules.INVADE_AFTER)


func test_a_new_match_sends_them_home() -> void:
	var rules := _arrived()
	var cheese := _cheese([Vector3.ZERO])
	rules.tick(0.1, cheese.blocks, cheese.boxes, EDGE)
	rules.reset()
	assert_eq(rules.mice.size(), 0)
	assert_false(rules.arrived)
	assert_eq(rules.invade_in, MouseRules.INVADE_AFTER)


# --- In the arena -----------------------------------------------------------

func _start(map := "box") -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": map}))
	match_ = world.match_
	match_.state.countdown = 0.0
	await wait_physics_frames(2)


func test_the_host_sends_them_in_tells_everyone_and_they_eat() -> void:
	await _start()
	watch_signals(match_)
	assert_eq(HUD.mice_text(match_.state), "Mice invade in 1:00")
	match_.state.tick(MouseRules.INVADE_AFTER - 41.5)
	assert_eq(HUD.mice_text(match_.state), "Mice invade in 0:42", "counting down")
	match_.state.mice.invade_in = 0.0
	match_._run_mice(0.1)
	assert_signal_emitted_with_parameters(match_, "announced", ["The mice are here!", MatchState.NO_TEAM])
	assert_eq(HUD.mice_text(match_.state), "", "no countdown once they are here")
	assert_eq(match_.state.mice.mice.size(), MouseRules.FIRST)
	var before: int = match_.state.blocks.health.reduce(func(sum: int, left: int) -> int: return sum + left, 0)
	for step in 300:
		match_._run_mice(0.1)
	var after: int = match_.state.blocks.health.reduce(func(sum: int, left: int) -> int: return sum + left, 0)
	assert_lt(after, before, "they have eaten some")
	# Everyone else sees them where the host has them.
	var copy := MatchState.from_dict(match_.state.to_dict(false))
	assert_eq(copy.mice.mice.size(), MouseRules.FIRST)
	assert_almost_eq(copy.mice.mice[0].at.distance_to(match_.state.mice.mice[0].at), 0.0, 0.01)


func test_they_are_seen_and_nothing_bumps_into_them() -> void:
	await _start()
	match_.state.mice.invade_in = 0.0
	match_._run_mice(0.1)
	await wait_process_frames(2)
	assert_eq(world.mice_view.shown(), MouseRules.FIRST)
	var mouse: Node3D = world.mice_view.get_child(0)
	assert_eq(mouse.find_children("*", "CollisionObject3D", true, false).size(), 0, "nothing solid")
	var box := AABB()
	for part: MeshInstance3D in mouse.find_children("*", "MeshInstance3D", true, false):
		assert_false(Scenery.is_yellow((part.material_override as StandardMaterial3D).albedo_color), "not the yellow of cheese")
		box = box.merge(part.global_transform * part.get_aabb())
	assert_gt(box.size.y, BodyShape.HEIGHT * 0.8, "giant: about as tall as a player")
	assert_gt(maxf(box.size.x, box.size.z), BodyShape.HEIGHT * 1.8, "and twice as long")
	match_.state.start_match()
	await wait_process_frames(2)
	assert_eq(world.mice_view.shown(), 0, "gone in a new match")


func test_shots_at_a_mouse_hurt_it_until_it_runs_off_and_is_heard() -> void:
	await _start()
	# A bot, whose aim the host takes on trust, as it is the host's own.
	var id := Bots.id_for(0)
	match_.add_player(id)
	var shooter: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(shooter)
	match_.state.mice.invade_in = 0.0
	match_._run_mice(0.1)
	var mouse: Dictionary = match_.state.mice.mice[0]
	# Between the mouse and the middle of the arena, it come in at the edge.
	var inward: Vector3 = (-mouse.at * Vector3(1, 0, 1)).normalized()
	shooter.global_position = Vector3(mouse.at.x, 0.1, mouse.at.z) + inward * 12.0
	await wait_physics_frames(1)
	watch_signals(match_)
	var origin: Vector3 = shooter.head.global_position
	var middle: Vector3 = mouse.at + Vector3.UP * MouseRules.HEIGHT / 2.0
	match_._shot(id, origin, (middle - origin).normalized(), 1, match_.state.player(id).weapon, WeaponStats.AIMED)
	assert_signal_not_emitted(match_, "mouse_scared", "not at one shot")
	var hurt := match_.state.mice.mice.map(func(each: Dictionary) -> float: return each.fear)
	var struck: int = hurt.find(hurt.max())
	assert_gt(hurt[struck], 0.0, "but hurt")
	# Shot enough, it runs.
	match_._hurt_mice(id, {struck: MouseRules.BRAVERY - hurt[struck]}, origin)
	# That one, or the other if it was nearer in the line of fire.
	assert_signal_emitted(match_, "mouse_scared")
	var scared: int = get_signal_parameters(match_, "mouse_scared")[0]
	assert_eq(match_.state.mice.mice[scared].mode, MouseRules.FLEEING)
	await wait_process_frames(1)
	assert_gt(Sounds.counts.get("squeak", 0), 0)


func test_bursts_by_a_mouse_hurt_it_too() -> void:
	await _start()
	match_.state.mice.invade_in = 0.0
	match_._run_mice(0.1)
	var at: Vector3 = match_.state.mice.mice[1].at + Vector3(0, 1, 3)
	var launcher := Weapons.find("launcher")
	for burst in 2:
		match_._burst(Projectile.new(burst, Bots.id_for(0), launcher, at, Vector3.FORWARD), at)
	assert_eq(match_.state.mice.mice[1].mode, MouseRules.FLEEING, "two rockets see one off")


func test_the_one_who_hit_a_mouse_is_shown_the_hit() -> void:
	await _start()
	match_.state.mice.invade_in = 0.0
	match_._run_mice(0.1)
	watch_signals(match_)
	var own := multiplayer.get_unique_id()
	match_._hurt_mice(own, {0: 20.0, 1: 15.0}, Vector3.ZERO)
	assert_signal_emitted_with_parameters(match_, "mouse_hit", [35, false])
	match_._hurt_mice(own, {0: MouseRules.BRAVERY}, Vector3.ZERO)
	assert_signal_emitted_with_parameters(match_, "mouse_hit", [roundi(MouseRules.BRAVERY), true], 1)
	match_._hurt_mice(Bots.id_for(0), {1: 20.0}, Vector3.ZERO)
	assert_signal_emit_count(match_, "mouse_hit", 2, "not for a bot's shot")


func test_the_countdown_is_said_only_when_there_is_one() -> void:
	var state := MatchState.new(MatchSettings.from_dict({"map": "box"}))
	assert_eq(HUD.mice_text(state), "", "no cheese, no mice")
	state.blocks = BlockRules.new(4)
	assert_eq(HUD.mice_text(state), "Mice invade in 1:00")
	state.end_match()
	assert_eq(HUD.mice_text(state), "", "nor between matches")
