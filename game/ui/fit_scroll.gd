class_name FitScroll
extends ScrollContainer
## A list that is as tall as what is in it while the screen has room, and
## scrolls once it hasn't, so that a panel holding it is never cut off at the
## top and bottom of a short screen, with big text or a long list.
##
## Put in place of the list with wrap(). It sizes itself every frame it is
## shown, as both the screen and the list can change at any time.

## Room left free above and below the panel it is in, in the interface's
## pixels.
const MARGIN := 16.0


func _init() -> void:
	horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	follow_focus = true


## Puts `list` inside a new FitScroll, where `list` was. Returns the scroll.
static func wrap(list: Control) -> FitScroll:
	var scroll := FitScroll.new()
	scroll.name = "%sScroll" % list.name
	scroll.size_flags_horizontal = list.size_flags_horizontal
	scroll.size_flags_vertical = list.size_flags_vertical
	var parent := list.get_parent()
	var at := list.get_index()
	parent.add_child(scroll)
	parent.move_child(scroll, at)
	# Moved, not taken out and put back, so that it and what is in it stay
	# part of their scene, and are still found by their unique names.
	scroll.owner = list.owner
	list.reparent(scroll, false)
	list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.fit()
	return scroll


func _process(_delta: float) -> void:
	if is_visible_in_tree():
		fit()


## Sizes this to what it holds, or to as much as the screen has room for.
func fit() -> void:
	if get_child_count() == 0 or not is_inside_tree():
		return
	var list := get_child(0) as Control
	var wanted := list.get_combined_minimum_size()
	var room := get_viewport_rect().size.y - MARGIN * 2.0 - _around()
	custom_minimum_size = Vector2(wanted.x, clampf(wanted.y, minf(120.0, wanted.y), maxf(room, 0.0)))


## Whether the list is too tall for the screen and scrolls.
func scrolls() -> bool:
	return get_child_count() > 0 and (get_child(0) as Control).get_combined_minimum_size().y > custom_minimum_size.y + 0.5


# How tall everything round this is, up to the outermost container that sits
# on the screen: the panel's edges, a title, buttons beneath.
func _around() -> float:
	var outermost: Control = self
	var node := get_parent()
	while node is Container and not (node is CenterContainer):
		outermost = node
		node = node.get_parent()
	return outermost.get_combined_minimum_size().y - get_combined_minimum_size().y
