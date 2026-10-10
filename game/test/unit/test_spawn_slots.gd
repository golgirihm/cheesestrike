extends GutTest

var slots: SpawnSlots


func before_each() -> void:
	slots = SpawnSlots.new(4)


func test_players_get_different_slots() -> void:
	var given := {}
	for player in [1, 907, 1900960591, 12]:
		given[slots.assign(player)] = true
	assert_eq(given.size(), 4)


func test_slots_are_handed_out_lowest_first() -> void:
	assert_eq(slots.assign(50), 0)
	assert_eq(slots.assign(20), 1)
	assert_eq(slots.assign(90), 2)


func test_asking_again_gives_the_same_slot() -> void:
	slots.assign(1)
	var first := slots.assign(2)
	slots.assign(3)
	assert_eq(slots.assign(2), first)


func test_a_released_slot_is_reused() -> void:
	slots.assign(1)
	var freed := slots.assign(2)
	slots.assign(3)
	slots.release(2)
	assert_eq(slots.assign(4), freed)


func test_releasing_an_unknown_player_does_nothing() -> void:
	slots.assign(1)
	slots.release(99)
	assert_eq(slots.assign(2), 1)


func test_when_full_extra_players_spread_over_the_slots() -> void:
	for player in range(1, 5):
		slots.assign(player)
	var extras := {}
	for player in range(5, 9):
		extras[slots.assign(player)] = true
	assert_eq(extras.size(), 4, "four extra players should double up one per slot")
