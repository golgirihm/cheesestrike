extends SkeletonModifier3D
## Bends a skeleton's spine to look up or down, on top of whatever its
## animations are doing: the realistic soldier aims its upper body where its
## player is looking. The bend is shared out along the spine, so that no one
## bone takes all of it.

## The spine bones it bends, from the waist up.
const SPINE := [&"Spine", &"Chest", &"UpperChest"]

## How far up to look, in radians: below zero, down.
var pitch := 0.0
## The axis the spine bends about to look up, in the skeleton's own space:
## across the body, from its left to its right.
var across := Vector3.RIGHT


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or is_zero_approx(pitch):
		return
	var share := pitch / SPINE.size()
	for name: StringName in SPINE:
		var bone := skeleton.find_bone(name)
		if bone < 0:
			continue
		var pose := skeleton.get_bone_global_pose(bone)
		pose.basis = Basis(across, share) * pose.basis
		skeleton.set_bone_global_pose(bone, pose)
