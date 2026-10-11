extends GutTest

const PLAYER := preload("res://player/player.gd")


func test_at_the_base_size_a_page_pixel_is_an_interface_pixel() -> void:
	var moved := PLAYER.page_to_view(Vector2(10, -4), Vector2(648, 648), Vector2(648, 648), 1.0)
	assert_eq(moved, Vector2(10, -4))


func test_a_smaller_window_makes_each_page_pixel_count_for_more() -> void:
	# A 400-pixel-tall page shows the 648-pixel interface scaled down.
	var moved := PLAYER.page_to_view(Vector2(100, 0), Vector2(1036.8, 648), Vector2(640, 400), 1.0)
	assert_almost_eq(moved.x, 162.0, 0.01)


func test_a_high_density_screen_is_accounted_for() -> void:
	# The same page on a screen with two pixels per page pixel.
	var moved := PLAYER.page_to_view(Vector2(100, 0), Vector2(1036.8, 648), Vector2(1280, 800), 2.0)
	assert_almost_eq(moved.x, 162.0, 0.01)


func test_an_empty_window_leaves_the_movement_alone() -> void:
	assert_eq(PLAYER.page_to_view(Vector2(5, 5), Vector2(648, 648), Vector2.ZERO, 1.0), Vector2(5, 5))
