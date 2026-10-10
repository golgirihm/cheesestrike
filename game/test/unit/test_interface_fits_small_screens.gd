extends GutTest
## The interface is scaled by the screen's short side: the base size in the
## project settings is square, so a phone held upright gets the same scale as
## one held sideways. Everything on screen therefore has to fit inside that
## square, or it runs off the edge of a narrow screen.

const MAIN := preload("res://main.tscn")
const PANELS := ["UI/Menu/Panel", "UI/WaitingNotice/Panel", "UI/DisconnectedNotice/Panel"]
# The longest reason the game gives for a lost session.
const LONGEST_REASON := "The connection to the host was lost."

var main: Node
var base := Vector2(
	ProjectSettings.get_setting("display/window/size/viewport_width"),
	ProjectSettings.get_setting("display/window/size/viewport_height"),
)


func before_each() -> void:
	main = MAIN.instantiate()
	add_child_autofree(main)
	main.get_node("%DisconnectedDetail").text = LONGEST_REASON
	await wait_frames(2)


func test_base_size_is_square() -> void:
	assert_eq(base.x, base.y)


func test_menu_and_notices_fit_inside_the_base_size() -> void:
	for path: String in PANELS:
		var needed: Vector2 = main.get_node(path).get_combined_minimum_size()
		assert_lte(needed.x, base.x, "%s is too wide" % path)
		assert_lte(needed.y, base.y, "%s is too tall" % path)
