extends GutTest
## What being hurt looks like: red at the edges with low health, a beat when
## it is very low, a flush at a hit, and a blur when a burst goes off close.

const WORLD := preload("res://world/world.tscn")
const FULL := float(PlayerRecord.MAX_HEALTH)

var world: Node3D
var view: HurtView
var shake: float


func before_each() -> void:
	shake = Settings.get_value("screen_shake")
	Settings.set_value("screen_shake", 1.0)
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	view = world.hurt_view
	view.set_process(false)


func after_each() -> void:
	Settings.set_value("screen_shake", shake)


func _health(left: int) -> void:
	world.match_.state.player(1).health = left


func test_the_edges_redden_only_below_half_health_and_deepen_from_there() -> void:
	assert_eq(HurtView.redness(FULL, FULL), 0.0)
	assert_eq(HurtView.redness(FULL * HurtView.HURT_BELOW, FULL), 0.0)
	var last := 0.0
	for health in range(49, 0, -1):
		var red := HurtView.redness(health, FULL)
		assert_gt(red, last, "at %d" % health)
		last = red
	assert_lt(last, HurtView.MOST_RED + 0.001)
	assert_almost_eq(HurtView.redness(25.0, FULL), HurtView.MOST_RED / 2.0, 0.001)
	assert_eq(HurtView.redness(0.0, FULL), 0.0, "knocked out has its own screen")
	assert_eq(HurtView.redness(-5.0, FULL), 0.0)
	assert_eq(HurtView.redness(50.0, 0.0), 0.0)
	# More health than full, as armour or a power-up might give, is not hurt.
	assert_eq(HurtView.redness(FULL * 2.0, FULL), 0.0)


func test_the_red_beats_only_when_health_is_very_low_and_faster_the_lower() -> void:
	for time: float in [0.0, 0.3, 1.1, 7.7]:
		assert_eq(HurtView.beat(FULL, FULL, time), 1.0)
		assert_eq(HurtView.beat(FULL * HurtView.BEAT_BELOW, FULL, time), 1.0)
		assert_eq(HurtView.beat(0.0, FULL, time), 1.0)
	var low := []
	var lowest := []
	for step in 400:
		low.append(HurtView.beat(25.0, FULL, step * 0.01))
		lowest.append(HurtView.beat(5.0, FULL, step * 0.01))
	for beats: Array in [low, lowest]:
		assert_almost_eq(float(beats.max()), 1.0, 0.01, "never brighter than the steady red")
		assert_almost_eq(float(beats.min()), 0.7, 0.01, "and never gone: a beat, not a flicker")
	# Count the beats in four seconds.
	var count := func(beats: Array) -> int:
		var dips := 0
		for index in range(1, beats.size() - 1):
			if beats[index] < beats[index - 1] and beats[index] <= beats[index + 1]:
				dips += 1
		return dips
	assert_gt(count.call(lowest), count.call(low))
	assert_lt(count.call(lowest), 12, "under three a second: not a strobe")


func test_a_harder_hit_flushes_redder() -> void:
	assert_eq(HurtView.hit_red(0.0), 0.0)
	assert_gt(HurtView.hit_red(1.0), 0.0, "even a graze shows")
	assert_gt(HurtView.hit_red(40.0), HurtView.hit_red(10.0))
	assert_eq(HurtView.hit_red(HurtView.HIT_FOR_ALL), HurtView.HIT_RED)
	assert_eq(HurtView.hit_red(500.0), HurtView.HIT_RED)


func test_a_burst_blurs_the_view_more_the_closer_it_is() -> void:
	assert_eq(HurtView.blur_from(0.0, 4.0), 1.0)
	assert_gt(HurtView.blur_from(2.0, 4.0), HurtView.blur_from(6.0, 4.0))
	assert_gt(HurtView.blur_from(6.0, 4.0), 0.0, "a near miss, outside the burst itself")
	assert_eq(HurtView.blur_from(4.0 * HurtView.BLUR_REACH, 4.0), 0.0)
	assert_eq(HurtView.blur_from(50.0, 4.0), 0.0)
	assert_eq(HurtView.blur_from(1.0, 0.0), 0.0, "what only strikes blurs nothing")
	# It follows the screen shake setting.
	assert_eq(HurtView.blur_from(0.0, 4.0, 0.0), 0.0)
	assert_almost_eq(HurtView.blur_from(0.0, 4.0, 0.5), 0.5, 0.001)


func test_the_view_is_clear_at_full_health_and_red_when_low() -> void:
	view._process(0.016)
	assert_eq(view.showing(), {"red": 0.0, "blur": 0.0})
	assert_false(view.get_node("Red").visible, "nothing is drawn when there is nothing to show")
	assert_false(view.get_node("Blur").visible)
	_health(40)
	view._process(0.016)
	assert_almost_eq(view.showing().red, HurtView.redness(40.0, FULL), 0.001)
	_health(100)
	view._process(0.016)
	assert_eq(view.showing().red, 0.0, "and clears as health comes back")


func test_being_hit_flushes_the_edges_and_it_fades() -> void:
	world.match_.hurt.emit(2, Vector3.ZERO, 30)
	view._process(0.0)
	assert_almost_eq(view.showing().red, HurtView.hit_red(30.0), 0.001, "at full health too")
	view._process(HurtView.HIT_FADE / 4.0)
	assert_between(view.showing().red, 0.01, HurtView.hit_red(30.0) - 0.01)
	view._process(HurtView.HIT_FADE)
	assert_eq(view.showing().red, 0.0)
	# However hard and often, it never covers the view.
	_health(1)
	for hit in 5:
		view.hit(500.0)
	view._process(0.0)
	assert_lte(view.showing().red, 0.85)


func test_a_burst_close_by_blurs_the_view_for_a_moment() -> void:
	var own: Node3D = world.player_node(1)
	world.match_.projectile_burst.emit(9, "launcher", own.global_position + Vector3(40, 0, 0))
	view._process(0.0)
	assert_eq(view.showing().blur, 0.0, "too far to matter")
	world.match_.projectile_burst.emit(9, "launcher", own.global_position + Vector3(1, 0, 0))
	view._process(0.0)
	assert_gt(view.showing().blur, 0.5)
	assert_true(view.get_node("Blur").visible)
	view._process(HurtView.BLUR_FADE / 2.0)
	assert_between(view.showing().blur, 0.05, 0.6)
	view._process(HurtView.BLUR_FADE)
	assert_eq(view.showing().blur, 0.0)
	assert_false(view.get_node("Blur").visible, "the picture is no longer read back")
	world.match_.projectile_burst.emit(9, "bow", own.global_position)
	view._process(0.0)
	assert_eq(view.showing().blur, 0.0, "a stick that only strikes blurs nothing")


func test_with_screen_shake_off_there_is_no_blur() -> void:
	Settings.set_value("screen_shake", 0.0)
	world.match_.projectile_burst.emit(9, "launcher", world.player_node(1).global_position)
	view._process(0.0)
	assert_eq(view.showing().blur, 0.0)


func test_nothing_shows_once_knocked_out_or_out_of_a_match() -> void:
	_health(20)
	view.hit(40.0)
	view.burst_near(0.0, 4.0)
	view._process(0.0)
	assert_gt(view.showing().red, 0.0)
	_health(0)
	view._process(0.0)
	assert_eq(view.showing(), {"red": 0.0, "blur": 0.0})
	_health(100)
	view._process(0.0)
	assert_eq(view.showing(), {"red": 0.0, "blur": 0.0}, "and nothing is left over on coming back")
	view.hit(40.0)
	world.stop()
	view._process(0.0)
	assert_eq(view.showing(), {"red": 0.0, "blur": 0.0})


func test_it_is_drawn_under_the_interface_and_takes_no_clicks() -> void:
	assert_lt(view.layer, 0)
	for sheet: ColorRect in [view.get_node("Red"), view.get_node("Blur")]:
		assert_eq(sheet.mouse_filter, Control.MOUSE_FILTER_IGNORE)
		assert_eq(sheet.get_global_rect().size, get_viewport().get_visible_rect().size)
	assert_lt(view.get_node("Blur").get_index(), view.get_node("Red").get_index(), "the red is over the blur, not blurred by it")
