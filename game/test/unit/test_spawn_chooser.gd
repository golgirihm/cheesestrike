extends GutTest

var spots: Array[Vector3] = [Vector3(-10, 0, -10), Vector3(10, 0, 10), Vector3(-10, 0, 10), Vector3(10, 0, -10)]


func test_the_spot_farthest_from_the_only_other_player_is_chosen() -> void:
	var others: Array[Vector3] = [Vector3(9, 0, 9)]
	assert_eq(SpawnChooser.farthest(spots, others), 0)


func test_it_is_judged_by_the_nearest_other_player() -> void:
	# The corner at (10, -10) is far from one player but next to the other.
	var others: Array[Vector3] = [Vector3(-9, 0, -9), Vector3(9, 0, -8)]
	assert_eq(SpawnChooser.farthest(spots, others), 2)


func test_with_nobody_else_any_spot_will_do() -> void:
	var rng := RandomNumberGenerator.new()
	var seen := {}
	for i in 50:
		rng.seed = i
		seen[SpawnChooser.farthest(spots, [], rng)] = true
	assert_eq(seen.size(), spots.size())


func test_no_spots_gives_none() -> void:
	assert_eq(SpawnChooser.farthest([], [Vector3.ZERO]), -1)
