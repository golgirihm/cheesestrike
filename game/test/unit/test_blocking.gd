extends GutTest

const WORLD := preload("res://world/world.tscn")


func before_each() -> void:
	Progress.reset()
	Progress.blocked = []


func after_all() -> void:
	Progress.blocked = []
	Progress.reset()


func test_names_with_bad_language_are_replaced() -> void:
	var state := MatchState.new()
	state.add_player(7)
	assert_true(state.rename(7, "Brie Lover"))
	assert_eq(state.player(7).name, "Brie Lover")
	state.rename(7, "shit head")
	assert_eq(state.player(7).name, PlayerName.stand_in(7))
	state.rename(7, "Brie")
	state.rename(7, "Sh1t")
	assert_eq(state.player(7).name, PlayerName.stand_in(7), "however it is spelt")
	state.rename(7, "Brie")
	state.rename(7, "BigFuckFan")
	assert_eq(state.player(7).name, PlayerName.stand_in(7), "or hidden in a longer word")
	assert_true(PlayerName.decent("Scunthorpe"))
	assert_true(PlayerName.decent("Cheddar 42"))


func test_a_tag_tells_players_apart_and_gives_nothing_away() -> void:
	var tag := Match.tag_of("some-secret-key")
	assert_eq(tag, Match.tag_of("some-secret-key"), "the same each session")
	assert_ne(tag, Match.tag_of("another-key"))
	assert_eq(tag.length(), Match.TAG_LENGTH)
	assert_false("some-secret-key".contains(tag) or tag.contains("secret"))
	assert_eq(Match.clean_tag(tag), tag)
	assert_eq(Match.clean_tag("not a tag"), "")


func test_the_tag_travels_with_the_record() -> void:
	var record := PlayerRecord.new()
	record.tag = Match.tag_of("k")
	assert_eq(PlayerRecord.from_dict(record.to_dict()).tag, record.tag)
	var data := record.to_dict()
	data.tag = "zz<script>"
	assert_eq(PlayerRecord.from_dict(data).tag, "")


func test_a_block_is_kept_and_survives_a_reset() -> void:
	var tag := Match.tag_of("k")
	Progress.set_blocked(tag, "Rude Brie", true)
	assert_true(Progress.is_blocked(tag))
	Progress.load_progress()
	assert_true(Progress.is_blocked(tag), "read back")
	assert_eq(Progress.blocked[0].name, "Rude Brie")
	Progress.reset()
	assert_true(Progress.is_blocked(tag), "a reset of the profile keeps it")
	Progress.set_blocked(tag, "", false)
	assert_false(Progress.is_blocked(tag))
	assert_false(Progress.is_blocked(""), "no one without a tag")


func test_a_block_list_read_back_keeps_only_what_could_be_one() -> void:
	var kept := Progress.tidy_blocked([{"tag": Match.tag_of("k"), "name": "A"}, {"tag": "bad"}, "x", {"tag": Match.tag_of("j"), "name": 4}])
	assert_eq(kept.size(), 2)
	assert_eq(kept[1].name, "")


func test_a_blocked_player_is_unheard_whatever_their_id() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	var match_: Match = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	match_._known_as(2, "their-key")
	match_.add_player(2)
	assert_false(world.voice_chat.is_muted(2))
	Progress.set_blocked(match_.state.player(2).tag, "Them", true)
	assert_true(world.voice_chat.is_muted(2))
	# The same player, back as someone new.
	match_.remove_player(2)
	match_.add_player(5)
	match_._known_as(5, "their-key")
	assert_true(world.voice_chat.is_muted(5))
