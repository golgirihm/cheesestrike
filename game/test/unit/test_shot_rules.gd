extends GutTest

var stats: WeaponStats
var rules: ShotRules


func before_each() -> void:
	stats = WeaponStats.new()
	stats.id = "test"
	stats.magazine = 10
	stats.fire_interval = 0.1
	stats.reload_time = 1.0
	rules = ShotRules.new()


func test_shots_at_the_weapons_rate_are_allowed() -> void:
	for i in 10:
		assert_true(rules.allow_shot(stats, i * 0.1), "shot %d" % i)


func test_a_few_shots_bunched_by_the_network_are_allowed() -> void:
	for i in 3:
		assert_true(rules.allow_shot(stats, 1.0), "shot %d" % i)


func test_firing_much_faster_than_the_weapon_is_refused() -> void:
	var allowed := 0
	for i in 10:
		if rules.allow_shot(stats, i * 0.01):
			allowed += 1
	assert_lte(allowed, 4)


func test_over_a_long_time_the_rate_is_held_to_the_weapons() -> void:
	stats.magazine = 100000
	var allowed := 0
	for i in 1000:
		if rules.allow_shot(stats, i * 0.05):
			allowed += 1
	# 50 seconds at a tenth of a second per shot, plus the burst.
	assert_between(allowed, 499, 504)


func test_more_shots_than_the_magazine_holds_are_refused() -> void:
	for i in 10:
		rules.allow_shot(stats, i * 0.2)
	assert_false(rules.allow_shot(stats, 5.0))


func test_reloading_allows_a_new_magazine_after_the_reload_time() -> void:
	for i in 10:
		rules.allow_shot(stats, i * 0.2)
	rules.reload(stats, 2.0)
	assert_false(rules.allow_shot(stats, 2.1), "still reloading")
	assert_true(rules.allow_shot(stats, 2.0 + stats.reload_time))


func test_a_reload_may_finish_a_little_early_by_the_hosts_clock() -> void:
	rules.allow_shot(stats, 0.0)
	rules.reload(stats, 1.0)
	assert_true(rules.allow_shot(stats, 1.0 + stats.reload_time * ShotRules.RELOAD_ALLOWANCE))


func test_reloading_a_full_magazine_changes_nothing() -> void:
	rules.reload(stats, 0.0)
	assert_true(rules.allow_shot(stats, 0.01))


func test_refilling_gives_a_full_magazine_at_once() -> void:
	for i in 10:
		rules.allow_shot(stats, i * 0.2)
	rules.reload(stats, 3.0)
	rules.refill()
	assert_true(rules.allow_shot(stats, 3.01))


func test_each_weapon_has_its_own_magazine() -> void:
	var other := stats.duplicate()
	other.id = "other"
	for i in 10:
		rules.allow_shot(stats, i * 0.2)
	assert_true(rules.allow_shot(other, 5.0))


func test_a_shot_from_the_shooters_head_is_believed() -> void:
	assert_true(ShotRules.origin_believable(Vector3(1, 1.6, 1), Vector3(1, 1.55, 1.2)))


func test_a_shot_from_across_the_arena_is_not() -> void:
	assert_false(ShotRules.origin_believable(Vector3(10, 1.6, 1), Vector3(1, 1.55, 1)))
