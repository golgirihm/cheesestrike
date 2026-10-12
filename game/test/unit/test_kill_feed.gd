extends GutTest

const FEED := preload("res://ui/kill_feed.gd")

var feed: VBoxContainer


func before_each() -> void:
	feed = FEED.new()
	add_child_autofree(feed)


func test_an_entry_names_both_players_and_the_weapon() -> void:
	assert_eq(FEED.entry_text("Brie", "Edam", "blaster", false), "Brie  [Cheddar Blaster]  Edam")


func test_a_player_who_knocked_themself_out_is_the_only_one_named() -> void:
	assert_eq(FEED.entry_text("", "Edam", "launcher", false), "Edam  knocked themself out  [Gouda Launcher]")


func test_a_headshot_says_so() -> void:
	assert_eq(FEED.entry_text("Brie", "Edam", "blaster", true), "Brie  [Cheddar Blaster, head]  Edam")


func test_an_unknown_weapon_is_shown_by_its_id() -> void:
	assert_eq(FEED.entry_text("Brie", "Edam", "spoon", false), "Brie  [spoon]  Edam")


func test_entries_show_newest_last() -> void:
	feed.add("A", "B", "blaster", false)
	feed.add("C", "D", "blaster", false)
	assert_eq(feed.showing().size(), 2)
	assert_string_starts_with(feed.showing()[1], "C")
	assert_eq(feed.latest, feed.showing()[1])
	assert_eq(feed.announced, 2)


func test_only_the_last_few_are_kept() -> void:
	for i in FEED.MAX_ENTRIES + 3:
		feed.add("P%d" % i, "Q", "blaster", false)
	assert_eq(feed.showing().size(), FEED.MAX_ENTRIES)
	assert_string_starts_with(feed.showing()[0], "P3")
	assert_eq(feed.announced, FEED.MAX_ENTRIES + 3)


func test_entries_fade_away() -> void:
	feed.add("A", "B", "blaster", false)
	feed._process(FEED.ENTRY_LIFE + 0.1)
	assert_eq(feed.showing().size(), 0)
	assert_eq(feed.announced, 1, "the count stays")


func test_clearing_empties_the_feed() -> void:
	feed.add("A", "B", "blaster", false)
	feed.clear()
	assert_eq(feed.showing().size(), 0)
