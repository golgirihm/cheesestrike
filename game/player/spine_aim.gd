extends SkeletonModifier3D
## Turns a skeleton's spine so that the weapon in its right hand points where
## its player is looking, on top of whatever its animations are doing: the
## realistic soldier aims its upper body. The turn is shared out along the
## spine, so that no one bone takes all of it.
##
## Mixamo's animations need it. Their aim twists the hips to bring the rifle
## round, but the upper body is played over the legs' animations, whose hips
## face ahead, which would leave the weapon pointing off to one side.

## The spine bones it turns, from the waist up.
const SPINE := [&"Spine", &"Chest", &"UpperChest"]

## The right hand's bone, and where the weapon is from it.
var hand := -1
var in_hand := Transform3D()
## Which way the weapon is to point, in the skeleton's own space.
var aim := Vector3.BACK


## How the spine turns to bring a weapon pointing `now` round to point at
## `aim`, both in the skeleton's space: the whole turn, which the spine's
## bones share.
static func turn(now: Vector3, aim_at: Vector3) -> Quaternion:
	if now.is_zero_approx() or aim_at.is_zero_approx():
		return Quaternion.IDENTITY
	return Quaternion(now.normalized(), aim_at.normalized())


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or hand < 0:
		return
	var weapon := skeleton.get_bone_global_pose(hand) * in_hand
	var whole := turn(-weapon.basis.z, aim)
	# Each bone turns by its share about itself; the bones above it go with
	# it, so the shares add up to the whole turn.
	var share := Quaternion.IDENTITY.slerp(whole, 1.0 / SPINE.size())
	for name: StringName in SPINE:
		var bone := skeleton.find_bone(name)
		if bone < 0:
			continue
		var pose := skeleton.get_bone_global_pose(bone)
		pose.basis = Basis(share) * pose.basis
		skeleton.set_bone_global_pose(bone, pose)
