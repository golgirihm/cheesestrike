extends GutTest
## Getting ready before anyone plays: one of everything a fight draws is
## drawn behind a cover and cleared away, and frame times are kept so a test
## can tell whether the game caught.

const WORLD := preload("res://world/world.tscn")
const MAIN := preload("res://main.tscn")
const EFFECTS := preload("res://effects/effects.gd")

var saved := {}


func before_each() -> void:
	for key: String in ["quality", "seen_help"]:
		saved[key] = Settings.get_value(key)
	Settings.set_value("quality", GraphicsQuality.HIGH)
	Settings.set_value("seen_help", true)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func test_there_is_one_of_every_model_a_fight_shows() -> void:
	var props := Warmup.props()
	add_child_autofree(props)
	assert_not_null(props.get_node_or_null("Character"))
	# A character, every weapon, each kind of pickup and the pad under it.
	assert_eq(props.get_child_count(), 1 + Weapons.ids().size() + 4 + 1)
	for model: Node3D in props.get_children():
		assert_gt(model.find_children("*", "MeshInstance3D", true, false).size() + (1 if model is MeshInstance3D else 0), 0, "%s has something to draw" % model.name)


func test_every_effect_of_a_fight_is_set_off() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	await wait_physics_frames(2)
	world.effects.set_process(false)
	Warmup.fire(world.effects, Vector3(0, 2, 0))
	for kind: String in ["flash", "tracer", "trail", "puff", "burst_flash", "shockwave", "scorch", "debris"]:
		assert_gt(world.effects.showing(kind), 0, kind)
	assert_gt(world.effects.casings_showing(), 0)
	assert_gt(world.effects.marks_showing(), 0)
	assert_eq(world.effects.projectiles_in_flight(), 2, "a grenade and a stick, each with its own shape")
	# And cleared away at once, leaving nothing counted.
	world.effects.clear()
	await wait_process_frames(2)
	assert_eq(world.effects.get_child_count(), 0)
	for count: int in [world.effects.casings_showing(), world.effects.marks_showing(), world.effects.debris_showing(), world.effects.prints_showing(), world.effects.specks_showing(), world.effects.projectiles_in_flight()]:
		assert_eq(count, 0)
	assert_eq(world.effects.tracer_ends(), [])


func test_the_arena_draws_it_all_for_a_few_frames_and_clears_it_away() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	watch_signals(world)
	assert_false(world.warming)
	world.warm_up()
	assert_true(world.warming)
	var props: Node3D = world.get_node("Warmup")
	assert_not_null(Outlines.colour_on(props.get_child(0)), "the outlines too")
	assert_true(Outlines.through_walls(props.get_child(0)))
	assert_true(Outlines.is_glow(props.get_child(1)), "and the glow")
	assert_gt(world.effects.get_child_count(), 5)
	assert_true(world.hurt_view.get_node("Red").visible, "and what being hurt looks like")
	assert_true(world.hurt_view.get_node("Blur").visible)
	assert_eq(world.hurt_view.showing(), {"red": 0.0, "blur": 0.0}, "without it counting as being hurt")
	# In front of the view, where it is drawn.
	var camera := get_viewport().get_camera_3d()
	assert_true(camera.is_position_in_frustum(props.global_position))
	assert_signal_not_emitted(world, "warmed")
	await wait_process_frames(Warmup.FRAMES + 2)
	assert_signal_emitted(world, "warmed")
	assert_false(world.warming)
	assert_null(world.get_node_or_null("Warmup"))
	assert_eq(world.effects.get_child_count(), 0)
	assert_false(world.hurt_view.get_node("Red").visible)
	assert_false(world.hurt_view.get_node("Blur").visible)


func test_the_game_starts_behind_a_cover_that_goes_when_it_is_ready() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	var hook := TestHook.new(main)
	autofree(hook)
	assert_true(main.loading_cover.visible)
	assert_eq(hook._screen(), "loading")
	assert_eq(main.loading_cover.get_global_rect().size, get_viewport().get_visible_rect().size, "over everything")
	assert_gt(main.loading_cover.get_index(), main.menu.get_index(), "and over the menu")
	assert_eq(main.loading_cover.color.a, 1.0, "nothing shows through")
	# Every sound is made behind it too, a few each frame.
	SoundBank._made.clear()
	await wait_process_frames(Warmup.FRAMES + 3)
	assert_true(main.loading_cover.visible, "still making sounds")
	for frame in 2000:
		if not main.loading_cover.visible:
			break
		await wait_process_frames(1)
	assert_false(main.loading_cover.visible)
	assert_true(SoundBank.make_some(0), "with every sound made")
	assert_eq(hook._screen(), "title")
	assert_eq(main.world.effects.get_child_count(), 0, "with nothing left lying about")


func test_frame_times_keep_the_longest_of_each_second_and_what_is_usual() -> void:
	var times := FrameTimes.new()
	assert_eq(times.typical(), 0.0)
	assert_eq(times.longest_since(0), 0.0)
	for frame in 59:
		times.add(1.0 / 60.0)
	assert_eq(times.seconds.size(), 1)
	assert_almost_eq(times.typical(), 16.67, 0.01)
	var before := times.now()
	# A frame that catches, among quick ones.
	times.add(0.25)
	for frame in 90:
		times.add(1.0 / 60.0)
	assert_almost_eq(times.typical(), 16.67, 0.01, "one slow frame is not what is usual")
	assert_almost_eq(times.longest_since(before), 250.0, 0.01, "but it is found")
	assert_almost_eq(times.longest_since(times.now() - 1), 16.67, 0.01)
	assert_eq(times.longest_since(times.now()), 0.0, "nothing after now")
	# Only so much is kept.
	for frame in 4000:
		times.add(1.0 / 60.0)
	assert_eq(times.seconds.size(), FrameTimes.SECONDS_KEPT)
	assert_gt(times.seconds[-1][0], times.seconds[0][0])
