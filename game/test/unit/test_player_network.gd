extends GutTest
## What a player sends of their movement, and how others show it. Offline, this
## process is peer 1, so a player named "1" is its own and "2" someone else's.

const PLAYER := preload("res://player/player.tscn")


func _player(id: int, at := Vector3.ZERO) -> CharacterBody3D:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = str(id)
	player.spawn_position = at
	add_child_autofree(player)
	return player


func test_the_owner_sends_where_they_are_and_which_way_they_face() -> void:
	var own := _player(1, Vector3(3, 0, 4))
	await wait_physics_frames(2)
	own.rotation.y = 0.5
	own.head.rotation.x = -0.2
	own._publish()
	var sent: PackedFloat32Array = own.net_state
	assert_almost_eq(sent[0], own.global_position.x, 0.0001)
	assert_almost_eq(sent[2], own.global_position.z, 0.0001)
	assert_almost_eq(sent[3], 0.5, 0.0001)
	assert_almost_eq(sent[4], -0.2, 0.0001)


func test_coming_back_into_play_counts_as_a_jump_to_a_new_place() -> void:
	var own := _player(1)
	var before: float = own.net_state[5]
	own.respawn_at(Vector3(5, 0, 5))
	own._publish()
	assert_eq(own.net_state[5], before + 1.0)


func test_someone_elses_player_is_drawn_smoothly_a_moment_behind() -> void:
	var other := _player(2)
	var now := Time.get_ticks_usec() / 1000000.0
	other.smoothing.push(now - InterpolationBuffer.DELAY - 0.05, PackedFloat32Array([0, 0, 0, 0, 0]))
	other.smoothing.push(now - InterpolationBuffer.DELAY + 0.05, PackedFloat32Array([10, 0, 0, 0, 0]))
	other._show_smoothed()
	assert_between(other.global_position.x, 3.0, 7.0, "about halfway between")


func test_an_update_arriving_is_passed_on() -> void:
	var other := _player(2)
	watch_signals(other)
	other.net_state = PackedFloat32Array([4, 0, 2, 0, 0, 0])
	assert_signal_emitted(other, "received")
	assert_eq(get_signal_parameters(other, "received")[1], Vector3(4, 0, 2))


func test_a_jump_to_a_new_place_is_not_smoothed() -> void:
	var other := _player(2)
	other.net_state = PackedFloat32Array([0, 0, 0, 0, 0, 0])
	await wait_seconds(0.05)
	other.net_state = PackedFloat32Array([1, 0, 0, 0, 0, 0])
	await wait_seconds(0.05)
	other.net_state = PackedFloat32Array([20, 0, 20, 0, 0, 1])
	await wait_seconds(0.01)
	other._show_smoothed()
	assert_eq(other.global_position, Vector3(20, 0, 20))


func test_the_owners_own_updates_are_not_smoothed_back_onto_them() -> void:
	var own := _player(1, Vector3(1, 0, 1))
	await wait_physics_frames(1)
	own.net_state = PackedFloat32Array([9, 9, 9, 0, 0, 0])
	assert_true(own.smoothing.is_empty())
