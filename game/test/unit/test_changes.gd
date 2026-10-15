extends GutTest


func test_the_newest_changes_are_for_this_version() -> void:
	assert_eq(Changes.ENTRIES[0][0], Changes.version(), "say what is new when the version goes up")
	assert_ne(Changes.version(), "0.0.0")


func test_versions_go_down_the_list_and_each_says_something() -> void:
	var last := ""
	for entry: Array in Changes.ENTRIES:
		assert_false(entry[1].is_empty(), entry[0])
		if not last.is_empty():
			assert_lt(_number(entry[0]), _number(last), "%s comes after %s" % [entry[0], last])
		last = entry[0]


func test_the_title_screen_shows_the_version_and_the_credits_whats_new() -> void:
	var front := FrontEnd.new()
	add_child_autofree(front)
	assert_eq((front.find_child("Version", true, false) as Label).text, "Version " + Changes.version())
	var news: Label = front.credits.find_child("Changes", true, false)
	assert_string_starts_with(news.text, "What's new in " + Changes.version())
	assert_string_contains(news.text, Changes.ENTRIES[0][1][0])


func _number(version: String) -> int:
	var parts := version.split(".")
	return int(parts[0]) * 10000 + int(parts[1]) * 100 + int(parts[2])
