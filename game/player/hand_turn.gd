extends SkeletonModifier3D
## Turns a hand to face a set way, in its skeleton's space, after whatever
## moved it there, and curls its fingers: the realistic soldier's left hand,
## which IK takes to its weapon's fore-end, closed round it.

const FINGERS := ["Index", "Middle", "Ring", "Little"]
const JOINTS := ["Proximal", "Intermediate", "Distal"]

var bone := -1
## Which way the hand is to face, in the skeleton's space.
var facing := Basis()
## How far each finger joint bends toward the palm, in radians; the thumb
## bends a third as far.
var curl := 0.0
## "Left" or "Right": whose fingers.
var side := "Left"


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or bone < 0:
		return
	var pose := skeleton.get_bone_global_pose(bone)
	pose.basis = Basis(facing.get_rotation_quaternion()).scaled(pose.basis.get_scale())
	skeleton.set_bone_global_pose(bone, pose)
	# A finger runs along its bones' Y and bends about their X, toward the
	# palm.
	for finger: String in FINGERS:
		for joint: String in JOINTS:
			_bend(skeleton, side + finger + joint, curl)
	for joint: String in ["ThumbProximal", "ThumbDistal"]:
		_bend(skeleton, side + joint, curl / 3.0)


static func _bend(skeleton: Skeleton3D, name: String, angle: float) -> void:
	var joint := skeleton.find_bone(name)
	if joint >= 0:
		# Bent that far from where it lies straight, at rest.
		skeleton.set_bone_pose_rotation(joint, skeleton.get_bone_rest(joint).basis.get_rotation_quaternion() * Quaternion(Vector3.RIGHT, angle))
