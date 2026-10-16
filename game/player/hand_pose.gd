extends SkeletonModifier3D
## Turns a hand to face a set way, in its skeleton's space, after IK has
## taken it where it goes: the realistic soldier's hands in its player's own
## view, holding the weapon as its animation holds a rifle. How far that rolls
## the hand about the forearm is shared out along the forearm by
## forearm_twist.gd, which runs after.

var bone := -1
## Which way the hand faces, in the skeleton's space.
var facing := Basis()


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or bone < 0:
		return
	var pose := skeleton.get_bone_global_pose(bone)
	pose.basis = Basis(facing.get_rotation_quaternion()).scaled(pose.basis.get_scale())
	skeleton.set_bone_global_pose(bone, pose)
