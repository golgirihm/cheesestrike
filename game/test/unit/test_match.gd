extends GutTest
## The Match node as the host runs it. Offline, this process is the host and
## peer 1.

const MATCH := preload("res://match/match.gd")

var match_: Node


func before_each() -> void:
	match_ = MATCH.new()
	add_child_autofree(match_)
	match_.start()


func test_offline_it_is_the_host() -> void:
	assert_true(match_.is_host())


func test_the_host_is_called_by_its_own_setting() -> void:
	match_.add_player(1)
	assert_eq(match_.state.name_of(1), Settings.get_value("name"))


func test_a_name_asked_for_before_joining_is_used_on_joining() -> void:
	match_._rename(44, "Early Edam")
	assert_null(match_.state.player(44))
	match_.add_player(44)
	assert_eq(match_.state.name_of(44), "Early Edam")


func test_a_name_asked_for_after_joining_is_used_at_once() -> void:
	match_.add_player(44)
	match_._rename(44, "Late Leerdammer")
	assert_eq(match_.state.name_of(44), "Late Leerdammer")


func test_a_pending_name_is_dropped_when_the_player_leaves() -> void:
	match_._rename(44, "Gone Gouda")
	match_.remove_player(44)
	match_.add_player(44)
	assert_eq(match_.state.name_of(44), PlayerName.FALLBACK)


func test_changes_are_announced() -> void:
	watch_signals(match_)
	match_.add_player(44)
	match_._rename(44, "Feta")
	match_.remove_player(44)
	assert_signal_emit_count(match_, "state_changed", 3)


func test_stopping_forgets_everyone() -> void:
	match_.add_player(1)
	match_.add_player(44)
	match_.stop()
	assert_eq(match_.state.players.size(), 0)
