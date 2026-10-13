extends GutTest
## How the weapon in view moves with its player, and its sights.

const PLAYER := preload("res://player/player.tscn")


func _own() -> CharacterBody3D:
	var player: CharacterBody3D = PLAYER.instantiate()
	# Offline, this process is peer 1, so a player named "1" is its own.
	player.name = "1"
	add_child_autofree(player)
	return player


func _other() -> CharacterBody3D:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "2"
	add_child_autofree(player)
	return player


func test_the_weapon_trails_behind_a_turn_and_no_further_than_its_reach() -> void:
	assert_eq(WeaponSway.turning(Vector2.ZERO), Vector3.ZERO)
	assert_lt(WeaponSway.turning(Vector2(2, 0)).x, 0.0, "turning right leaves it to the left")
	assert_gt(WeaponSway.turning(Vector2(-2, 0)).x, 0.0)
	assert_lt(WeaponSway.turning(Vector2(0, 2)).y, 0.0, "looking up leaves it low")
	assert_gt(WeaponSway.turning(Vector2(4, 0)).length(), WeaponSway.turning(Vector2(2, 0)).length())
	assert_almost_eq(WeaponSway.turning(Vector2(500, 500)).length(), WeaponSway.LAG_REACH, 0.0001)


func test_the_weapon_leans_away_from_a_sidestep() -> void:
	assert_eq(WeaponSway.strafing(0.0), 0.0)
	assert_lt(WeaponSway.strafing(1.0), 0.0)
	assert_gt(WeaponSway.strafing(-1.0), 0.0)
	assert_almost_eq(WeaponSway.strafing(9.0), -WeaponSway.STRAFE_ROLL, 0.0001, "no further when running sideways")


func test_a_harder_landing_dips_the_weapon_further() -> void:
	assert_eq(WeaponSway.impact(0.0), 0.0)
	assert_gt(WeaponSway.impact(10.0), WeaponSway.impact(4.0))
	assert_eq(WeaponSway.impact(99.0), 1.0)
	assert_eq(WeaponSway.landing(0.0), Vector3.ZERO)
	assert_lt(WeaponSway.landing(1.0).y, WeaponSway.landing(0.4).y)
	assert_almost_eq(WeaponSway.landing(1.0).y, -WeaponSway.LANDING_DIP, 0.0001)


func test_a_reload_lowers_the_weapon_holds_it_and_brings_it_back() -> void:
	assert_eq(WeaponSway.reload_lowering(0.0), 0.0)
	assert_eq(WeaponSway.reload_lowering(1.0), 0.0)
	assert_between(WeaponSway.reload_lowering(0.09), 0.01, 0.99)
	assert_almost_eq(WeaponSway.reload_lowering(0.5), 1.0, 0.0001)
	assert_between(WeaponSway.reload_lowering(0.91), 0.01, 0.99)


func test_the_magazine_is_out_in_the_middle_of_a_reload_and_back_before_its_end() -> void:
	assert_eq(WeaponSway.magazine_out(0.1), 0.0, "not before the weapon is down")
	assert_almost_eq(WeaponSway.magazine_out(0.5), 1.0, 0.0001)
	assert_eq(WeaponSway.magazine_out(0.85), 0.0, "in before the weapon comes up")
	for step in 21:
		var progress := step / 20.0
		assert_lte(WeaponSway.magazine_out(progress), WeaponSway.reload_lowering(progress) + 0.0001, "never out while the weapon is up")


func test_every_weapon_has_sights_that_point_the_way_it_fires() -> void:
	for id: String in Weapons.ids():
		var model := WeaponModels.build(id)
		var rear: Node3D = model.get_node_or_null("Sight")
		var front: Node3D = model.get_node_or_null("FrontSight")
		assert_not_null(rear, id)
		assert_not_null(front, id)
		assert_eq(rear.position.x, 0.0, "%s: on the middle of the weapon" % id)
		assert_almost_eq(front.position.x, rear.position.x, 0.0001, id)
		assert_almost_eq(front.position.y, rear.position.y, 0.0001, "%s: level, so the line runs straight ahead" % id)
		assert_lt(front.position.z, rear.position.z - 0.1, "%s: the front sight well ahead of the rear" % id)
		# Nothing on the weapon stands above the sight line and hides the target.
		for part: MeshInstance3D in model.find_children("*", "MeshInstance3D", true, false):
			var top: float = (part.transform * part.get_aabb()).end.y
			assert_lte(top, rear.position.y + 0.0001, "%s: %s stands above the sights" % [id, part.name])
		model.free()


func test_aiming_brings_the_sights_to_the_middle_of_the_view() -> void:
	var own := _own()
	await wait_process_frames(1)
	for id: String in Weapons.ids():
		own._shown_weapon = ""
		own._show_weapon(id)
		own._switch_left = 0.0
		own._aim = 1.0
		own._animate(0.0)
		for sight: String in ["Sight", "FrontSight"]:
			var at: Vector3 = own.camera.global_transform.affine_inverse() * own.muzzle.get_parent().get_node(sight).global_position
			assert_almost_eq(at.x, 0.0, 0.0005, "%s %s is left or right of the middle" % [id, sight])
			assert_almost_eq(at.y, 0.0, 0.0005, "%s %s is above or below the middle" % [id, sight])
			assert_lt(at.z, -own.camera.near, "%s %s can be seen" % [id, sight])


func test_the_owners_weapon_dips_through_a_reload_and_its_magazine_comes_out() -> void:
	var own := _own()
	await wait_process_frames(1)
	own._animate(0.0)
	var rest: Vector3 = own.gun.position
	var magazine: Node3D = own.muzzle.get_parent().get_node("Magazine")
	var seated := magazine.position
	own.weapon.ammo = 1
	assert_true(own.weapon.start_reload())
	own.weapon.tick(own.weapon.stats.reload_time / 2.0)
	own._animate(0.0)
	assert_lt(own.gun.position.y, rest.y - 0.05, "lowered half way through")
	assert_lt(magazine.position.y, seated.y - 0.1, "with the magazine out")
	own.weapon.tick(own.weapon.stats.reload_time)
	own._animate(0.0)
	assert_almost_eq(own.gun.position.distance_to(rest), 0.0, 0.001)
	assert_almost_eq(magazine.position.distance_to(seated), 0.0, 0.001)


func test_others_are_seen_to_reload() -> void:
	var own := _own()
	await wait_process_frames(1)
	own._publish()
	assert_eq(int(own.net_state[6]) & own.POSE_RELOADING, 0)
	own.weapon.ammo = 1
	own.weapon.start_reload()
	own._publish()
	assert_eq(int(own.net_state[6]) & own.POSE_RELOADING, own.POSE_RELOADING)

	var other := _other()
	other.net_state = PackedFloat32Array([4, 0, 2, 0, 0, 0, 0])
	other._animate(1.0)
	var rest: Vector3 = other.gun.position
	other.net_state = PackedFloat32Array([4, 0, 2, 0, 0, 0, other.POSE_RELOADING])
	other._animate(1.0)
	assert_lt(other.gun.position.y, rest.y - 0.1, "their weapon is down")
	other.net_state = PackedFloat32Array([4, 0, 2, 0, 0, 0, 0])
	other._animate(1.0)
	assert_almost_eq(other.gun.position.distance_to(rest), 0.0, 0.001, "and back up when they are done")


func test_landing_dips_the_owners_weapon_and_it_recovers() -> void:
	var own := _own()
	await wait_process_frames(1)
	own._animate(0.0)
	var rest: Vector3 = own.gun.position
	own._landing = WeaponSway.impact(12.0)
	own._animate(0.001)
	assert_lt(own.gun.position.y, rest.y - 0.02)
	own._animate(1.0)
	own._animate(0.001)
	assert_almost_eq(own.gun.position.y, rest.y, 0.002)


func test_turning_the_view_leaves_the_owners_weapon_behind_for_a_moment() -> void:
	var own := _own()
	await wait_process_frames(1)
	own._animate(0.0)
	var rest: Vector3 = own.gun.position
	own._look(Vector2(0.2, 0.0))
	own._animate(0.05)
	assert_lt(own.gun.position.x, rest.x, "turning right, it trails to the left")
	for frame in 30:
		own._animate(0.05)
	assert_almost_eq(own.gun.position.x, rest.x, 0.001, "and catches up")


func test_the_view_bobs_with_each_stride_and_more_at_speed() -> void:
	assert_eq(WeaponSway.view_bob(1.0, 0.0), Vector3.ZERO, "not standing still")
	assert_eq(WeaponSway.view_bob(0.0, 1.0), Vector3.ZERO, "level between strides")
	var mid := WeaponSway.view_bob(PI / 2.0, 1.0)
	assert_almost_eq(mid.y, -WeaponSway.VIEW_BOB.y, 0.0001, "down in the middle of a stride")
	assert_gt(mid.x, 0.0)
	assert_lt(WeaponSway.view_bob(3.0 * PI / 2.0, 1.0).x, 0.0, "to the other side on the next")
	assert_gt(WeaponSway.view_bob(PI / 2.0, 1.5).length(), mid.length())
	assert_eq(WeaponSway.view_bob(PI / 2.0, 9.0), WeaponSway.view_bob(PI / 2.0, 1.5), "and no more than that")


func test_the_view_moves_only_as_much_as_the_player_asks() -> void:
	var own := _own()
	await wait_process_frames(1)
	own._landing = 1.0
	own._bob_phase = PI / 2.0
	Settings.set_value("camera_motion", 1.0)
	own._process(0.0)
	assert_lt(own.camera.position.y, -0.1, "a landing dips the view")
	Settings.set_value("camera_motion", 0.0)
	own._process(0.0)
	assert_eq(own.camera.position, Vector3.ZERO, "unless they have turned it off")
	assert_eq(own.camera.rotation.z, 0.0)
	Settings.set_value("camera_motion", 1.0)


func test_every_weapon_is_held_in_two_hands_on_the_ends_of_two_arms() -> void:
	for id: String in Weapons.ids():
		var model := WeaponModels.build(id)
		var hand: Node3D = model.get_node_or_null("Hand")
		var other: Node3D = model.get_node_or_null("OffHand")
		assert_not_null(hand, id)
		assert_not_null(other, id)
		assert_true(WeaponModels.OFF_HAND.has(id), "%s has a place for the other hand" % id)
		assert_gt(hand.position.distance_to(other.position), 0.04, "%s: two hands, not one on the other" % id)
		assert_lt(other.position.y, model.get_node("Sight").position.y - 0.05, "%s: the other hand is under the weapon, clear of the sights" % id)
		for arm_name: String in ["ViewArmRight", "ViewArmLeft"]:
			var arm: MeshInstance3D = model.get_node_or_null(arm_name)
			assert_not_null(arm, "%s %s" % [id, arm_name])
			# One end at its hand, the other below and behind it, out of view.
			var grip: Vector3 = hand.position if arm_name == "ViewArmRight" else other.position
			var half: Vector3 = arm.transform.basis.y * (arm.mesh as CapsuleMesh).height / 2.0
			var ends := [arm.position + half, arm.position - half]
			var near: Vector3 = ends[0] if ends[0].distance_to(grip) < ends[1].distance_to(grip) else ends[1]
			var far: Vector3 = ends[1] if near == ends[0] else ends[0]
			assert_almost_eq(near.distance_to(grip), 0.0, 0.001, "%s %s reaches its hand" % [id, arm_name])
			assert_lt(far.y, grip.y - 0.3, "%s %s comes up from below" % [id, arm_name])
			assert_gt(far.z, grip.z + 0.3, "%s %s comes from behind" % [id, arm_name])
		model.free()


func test_only_the_holder_sees_both_hands_and_the_arms() -> void:
	var own := _own()
	var other := _other()
	await wait_process_frames(1)
	for part: String in ["OffHand", "ViewArmRight", "ViewArmLeft"]:
		assert_true(own.muzzle.get_parent().get_node(part).visible, "own %s" % part)
		assert_false(other.muzzle.get_parent().get_node(part).visible, "another player's %s" % part)
	assert_true(other.muzzle.get_parent().get_node("Hand").visible, "their hand on the grip is what their arm reaches to")
	var pickup := PickupModels.build("blaster")
	for part: String in ["Hand", "OffHand", "ViewArmRight", "ViewArmLeft"]:
		assert_false(pickup.find_child(part, true, false).visible, "nobody holds a weapon lying on a pad")
	pickup.free()
