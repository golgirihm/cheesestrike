extends SkeletonModifier3D
## Turns a skeleton's spine so that the weapon in its right hand points
## where its player is looking, on top of whatever its animations are doing:
## the realistic soldier aims its upper body.
##
## Mixamo's animations need it. Their aim twists the hips to bring the rifle
## round, but the upper body is played over the legs' animations, whose hips
## face ahead, which would leave the weapon pointing off to one side.
##
## The turn is shared out along the spine, so that no one bone takes all of
## it. Looking further up than AIM_UP, or down than AIM_DOWN, the weapon
## stops there:
## nobody can shoulder a rifle pointing at the sky, and bending the spine,
## or swinging the arm, further than that looks broken.

## The spine bones it turns, from the waist up.
const SPINE := [&"Spine", &"Chest", &"UpperChest"]
## How far up, and down, the weapon is aimed at most, in radians. Down
## hunches the body over, so it stops sooner.
const AIM_UP := 0.9
const AIM_DOWN := 0.6

## The right hand's bone, and where the weapon is from it.
var hand := -1
var in_hand := Transform3D()
## Which way the weapon is to point, in the skeleton's own space, whose up
## is +Y.
var aim := Vector3.BACK


## How the spine turns to bring a weapon pointing `now` round to point at
## `aim_at`, both in the skeleton's space: the whole turn, which the spine's
## bones share.
static func turn(now: Vector3, aim_at: Vector3) -> Quaternion:
	if now.is_zero_approx() or aim_at.is_zero_approx():
		return Quaternion.IDENTITY
	return Quaternion(now.normalized(), aim_at.normalized())


## `direction` tipped no further than `up` radians above level or `down`
## below, still facing the same way across the ground.
static func pitched_at_most(direction: Vector3, down: float, up: float) -> Vector3:
	var level := Vector3(direction.x, 0.0, direction.z)
	if level.is_zero_approx():
		return direction.normalized()
	var pitch := clampf(atan2(direction.y, level.length()), -down, up)
	return level.normalized() * cos(pitch) + Vector3.UP * sin(pitch)


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or hand < 0:
		return
	var weapon := skeleton.get_bone_global_pose(hand) * in_hand
	var whole := turn(-weapon.basis.z, pitched_at_most(aim, AIM_DOWN, AIM_UP))
	# Each bone turns by its share about itself; the bones above it go with
	# it, so the shares add up to the whole turn.
	var share := Quaternion.IDENTITY.slerp(whole, 1.0 / SPINE.size())
	for name: StringName in SPINE:
		_turn_bone(skeleton, skeleton.find_bone(name), share)


static func _turn_bone(skeleton: Skeleton3D, bone: int, by: Quaternion) -> void:
	if bone < 0:
		return
	var pose := skeleton.get_bone_global_pose(bone)
	pose.basis = Basis(by) * pose.basis
	skeleton.set_bone_global_pose(bone, pose)
