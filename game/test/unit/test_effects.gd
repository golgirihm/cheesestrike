extends GutTest

const EFFECTS := preload("res://effects/effects.gd")


func test_a_tracer_sits_between_its_ends() -> void:
	var from := Vector3(1, 2, 3)
	var to := Vector3(4, 2, -1)
	var placed := EFFECTS.tracer_transform(from, to)
	assert_almost_eq(placed.origin.distance_to((from + to) / 2.0), 0.0, 0.0001)
	# The box is a unit long along its own -Z, stretched to the line's length.
	var tip := placed * Vector3(0, 0, -0.5)
	var tail := placed * Vector3(0, 0, 0.5)
	assert_almost_eq(tip.distance_to(to), 0.0, 0.001)
	assert_almost_eq(tail.distance_to(from), 0.0, 0.001)


func test_a_vertical_tracer_works() -> void:
	var placed := EFFECTS.tracer_transform(Vector3.ZERO, Vector3(0, 10, 0))
	assert_true(placed.basis.determinant() != 0.0)
	assert_almost_eq((placed * Vector3(0, 0, -0.5)).distance_to(Vector3(0, 10, 0)), 0.0, 0.001)


func test_a_zero_length_tracer_does_not_break() -> void:
	var placed := EFFECTS.tracer_transform(Vector3.ONE, Vector3.ONE)
	assert_true(placed.origin.is_finite())


func test_a_shot_leaves_a_flash_and_a_tracer_per_pellet_that_soon_go() -> void:
	var effects: Node3D = EFFECTS.new()
	add_child_autofree(effects)
	effects.show_shot(Vector3.ZERO, [Vector3(0, 0, -5), Vector3(1, 0, -5), "not a point"])
	assert_eq(effects.get_child_count(), 3, "a flash and two tracers")
	await wait_seconds(0.2)
	assert_eq(effects.get_child_count(), 0)
