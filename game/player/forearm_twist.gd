extends SkeletonModifier3D
## Turns the realistic soldier's forearm twist bones with its hands, as its
## rig is made to have them turned.
##
## Character Creator's forearms have two twist bones each, along the forearm,
## for the skin near the wrist to hang from. Its own tools turn them by part
## of however far the hand rolls about the forearm, so that a turn of the
## hand is shared out along the forearm. Mixamo's skeleton has no such bones,
## so nothing in its animations turns them: left at rest, they leave the
## whole roll to the wrist, which pinches the skin there. This turns them,
## leaving the hands as the animations have them.

## Each forearm's twist bones (Character Creator's names, which the humanoid
## skeleton leaves alone), from the elbow down, with the share of the hand's
## roll each adds: the first a third of it, the second, which turns with the
## first, another third.
const TWISTS := [["CC_Base_%s_ForearmTwist01", 1.0 / 3.0], ["CC_Base_%s_ForearmTwist02", 1.0 / 3.0]]


## The part of `turn` that rolls about `axis` (normalized): its twist, with
## the swing that remains taken out.
static func roll(turn: Quaternion, axis: Vector3) -> Quaternion:
	var along := axis * Vector3(turn.x, turn.y, turn.z).dot(axis)
	var twist := Quaternion(along.x, along.y, along.z, turn.w)
	if twist.length_squared() < 0.000001:
		return Quaternion.IDENTITY
	return twist.normalized()


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null:
		return
	for side in ["Left", "Right"]:
		_share(skeleton, side)


static func _share(skeleton: Skeleton3D, side: String) -> void:
	var forearm := skeleton.find_bone(side + "LowerArm")
	var hand := skeleton.find_bone(side + "Hand")
	if forearm < 0 or hand < 0:
		return
	var arm := skeleton.get_bone_global_pose(forearm)
	var turned := skeleton.get_bone_global_pose(hand)
	# How the hand is turned against the forearm, from how it rests against
	# it, in the forearm's own space.
	var arm_now := arm.basis.get_rotation_quaternion()
	var now := arm_now.inverse() * turned.basis.get_rotation_quaternion()
	var rest := skeleton.get_bone_global_rest(forearm).basis.get_rotation_quaternion().inverse() * skeleton.get_bone_global_rest(hand).basis.get_rotation_quaternion()
	var along := arm_now.inverse() * (turned.origin - arm.origin)
	if along.is_zero_approx():
		return
	var twist := roll(now * rest.inverse(), along.normalized())
	for twisting: Array in TWISTS:
		var bone := skeleton.find_bone(twisting[0] % side[0])
		if bone < 0:
			continue
		# The share of the roll, about the forearm, in the skeleton's space.
		var share := arm_now * Quaternion.IDENTITY.slerp(twist, twisting[1]) * arm_now.inverse()
		var pose := skeleton.get_bone_global_pose(bone)
		pose.basis = Basis(share) * pose.basis
		skeleton.set_bone_global_pose(bone, pose)
