extends "res://player/body.gd"
## How a player looks to everyone else: the Tactical Soldier 02 model in
## game/licensed, moved by its Mixamo animations, showing what its body is
## doing (body.gd, which works that out):
##
## - its legs walk and run whichever way the player goes, at the pace they go,
##   and crouch, jump and slide;
## - its upper body aims, fires, reloads, throws, strikes, flinches and makes
##   the emotes, and its spine bends to look up and down;
## - its hands are pulled onto the weapon wherever the player holds it;
## - its face blinks, winces, cheers and sets, with the model's face shapes,
##   behind its sunglasses;
## - it wears its own cap and sunglasses rather than a chosen hat;
## - knocked out, it falls back rather than melting.
##
## The model and its animations are licensed and kept in a private
## repository, checked out as the game/licensed submodule, which the game
## can't do without.

const MODEL := "res://licensed/soldier/soldier.glb"
const SPINE_AIM := preload("res://player/spine_aim.gd")
## The meshes of the model's arms alone, which its player sees of
## themselves (see first_person_arms.gd).
const FIRST_PERSON := ["FirstPersonArms", "FirstPersonSleeves"]
const ANIMATIONS := "res://licensed/animations"
## How high the model's eyes are, in its own metres. It is scaled to put them
## where the player sees from (BodyShape.HEAD_HEIGHT), so that what can be hit
## (BodyShape's boxes) is where it is drawn.
const MODEL_EYE_HEIGHT := 1.62
## How fast the model's walk, run, sprint and crouched walk go across the
## ground, in its own metres a second, played at their own pace, each the
## same whichever way it goes.
const WALK_PACE := 1.86
const RUN_PACE := 4.64
const SPRINT_PACE := 6.96
const CROUCH_PACE := 1.97
## The gaits, as far along the legs' blend as each is (see stride), and the
## ways each goes, by the end of its animation's name, as [right, ahead].
const GAITS := {"walk": 1.0, "run": 2.0, "sprint": 3.0}
const WAYS := {"": Vector2(0, 1), "_back": Vector2(0, -1), "_left": Vector2(-1, 0), "_right": Vector2(1, 0), "_ahead_left": Vector2(-0.7071, 0.7071), "_ahead_right": Vector2(0.7071, 0.7071), "_back_left": Vector2(-0.7071, -0.7071), "_back_right": Vector2(0.7071, -0.7071)}
## The animations that go round and round rather than playing once, besides
## every gait's.
const LOOPED := ["idle", "aim", "pistol_aim", "dual_carry", "fire", "crouch", "crouch_aim", "jump_loop", "emote_dance", "emote_wave"]
## How its hands hold each kind of weapon, by the animation of its aim that
## the hold is worked out from: a rifle in both hands, a pistol in two hands
## out in front, and a pair of pistols one in each. Mixamo has no aim with a
## pistol in each hand, so a pair are held as they are carried, and aimed by
## reaching out with them (see DUAL_REACH). Weapons not named in GRIP_OF are
## held as rifles.
const GRIPS := {"rifle": "aim", "pistol": "pistol_aim", "dual": "dual_carry"}
const GRIP_OF := {"pistol": "pistol", "pistols": "dual", "burratas": "dual"}
## What the upper body carries a weapon with when not aiming it, by grip: a
## rifle as the legs' own animation does (nothing here), a pistol always out
## in front, and a pair of pistols low, one by each hip.
const CARRY := {"pistol": "pistol_aim", "dual": "dual_carry"}
## Aiming a pair of pistols, how far the fists reach out from between the
## shoulders along the look, as a share of an arm's length, and how far
## apart they are, as a share of the shoulders' width: out in front, side by
## side, a little apart.
const DUAL_REACH := 0.85
const DUAL_APART := 0.7
## Seconds the weapon stays up at the shoulder after the last shot.
const RAISED_AFTER_FIRING := 1.0
## What the upper body plays for each of CharacterMotion's actions, and each
## emote (see Emotes). Nothing for an emote with no animation.
const ACTIONS := {
	CharacterMotion.AIM: "aim",
	CharacterMotion.FIRE: "fire",
	CharacterMotion.RELOAD: "reload",
	CharacterMotion.THROW: "throw",
	CharacterMotion.MELEE: "melee",
	CharacterMotion.HURT: "hurt",
}
const EMOTES := {Emotes.WAVE: "emote_wave", Emotes.DANCE: "emote_dance", Emotes.SHRUG: "emote_shrug", Emotes.CHEER: "emote_cheer"}
## The bones the upper body's animations move: the waist up.
const UPPER := [
	"Spine", "Chest", "UpperChest", "Neck", "Head",
	"LeftShoulder", "LeftUpperArm", "LeftLowerArm", "LeftHand",
	"RightShoulder", "RightUpperArm", "RightLowerArm", "RightHand",
]
const FINGERS := ["ThumbMetacarpal", "ThumbProximal", "ThumbDistal", "IndexProximal", "IndexIntermediate", "IndexDistal", "MiddleProximal", "MiddleIntermediate", "MiddleDistal", "RingProximal", "RingIntermediate", "RingDistal", "LittleProximal", "LittleIntermediate", "LittleDistal"]
## The face shapes each face is made of, as {shape: how far}. The eyes'
## blinking and glancing are on top of these.
const FACES := {
	CALM: {"Mouth_Smile_L": 0.15, "Mouth_Smile_R": 0.15},
	SET: {"Brow_Compress_L": 0.8, "Brow_Compress_R": 0.8, "Brow_Drop_L": 0.4, "Brow_Drop_R": 0.4, "Mouth_Press_L": 0.7, "Mouth_Press_R": 0.7},
	WINCE: {"Eye_Squint_L": 1.0, "Eye_Squint_R": 1.0, "Brow_Drop_L": 1.0, "Brow_Drop_R": 1.0, "Mouth_Stretch_L": 0.9, "Mouth_Stretch_R": 0.9, "Mouth_Shrug_Upper": 0.5},
	CHEER: {"Mouth_Smile_L": 1.0, "Mouth_Smile_R": 1.0, "Cheek_Raise_L": 0.8, "Cheek_Raise_R": 0.8, "Eye_Wide_L": 0.4, "Eye_Wide_R": 0.4, "Brow_Raise_Inner_L": 0.5, "Brow_Raise_Inner_R": 0.5},
	DIZZY: {"Eye_Blink_L": 0.65, "Eye_Blink_R": 0.65, "Mouth_Frown_L": 0.5, "Mouth_Frown_R": 0.5, "Mouth_Shrug_Lower": 0.4},
}
## How the face moves to the next one: most of the way in this many seconds.
const FACE_TIME := 0.08
## Seconds the spine takes to turn to aim the weapon, or let it go.
const GRIP_TIME := 0.12
## How big its weapon is drawn, against the size the weapon models are made
## at, which is for the player's own view.
const WEAPON_SCALE := 0.55

var _model: Node3D
var _skeleton: Skeleton3D
var _tree: AnimationTree
var _spine: SkeletonModifier3D
var _face_mesh: MeshInstance3D
var _shirt: StandardMaterial3D
var _cap: StandardMaterial3D
var _cap_mesh: MeshInstance3D
var _overlay := StandardMaterial3D.new()
var _shapes := {}
var _crouches := false
var _left_hand_bone := -1
var _right_hand_bone := -1
# The weapon its player holds, which it puts in its right hand.
var _held_gun: Node3D
var _held_model: Node3D
# Where the weapon in its hand last was, in this node's measurements.
var _weapon_here := Transform3D()
# Seconds since it last fired.
var _since_fired := INF
# The weapon in its hands, and how they hold it (see GRIPS).
var _weapon := ""
var _grip := "rifle"
# The second of a pair of weapons, which its left hand holds.
var _left_weapon: Node3D
# Each arm's reach for aiming a pair of pistols (see _reach_out), and how far
# into aiming them it is, 0 to 1.
var _dual_reach := {}
var _dual_aim := 0.0


## How much bigger the model is drawn than it is.
static func model_scale() -> float:
	return BodyShape.HEAD_HEIGHT / MODEL_EYE_HEIGHT


## How the legs go for a player moving at `speed` across the arena (metres a
## second), as [how far along from standing (0) through walking (1) and
## running (2) to sprinting (3), how fast to play that]: the gaits are blended,
## and sped up past the sprint, so that the feet keep pace with the ground.
static func stride(speed: float) -> Vector2:
	var pace := speed / (BodyShape.SIZE * model_scale())
	var paces := [0.0, WALK_PACE, RUN_PACE, SPRINT_PACE]
	for gait in range(1, paces.size()):
		if pace < paces[gait]:
			return Vector2(gait - 1 + (pace - paces[gait - 1]) / (paces[gait] - paces[gait - 1]), 1.0)
	return Vector2(paces.size() - 1, pace / SPRINT_PACE)


## The same crouched, which only walks: [how far from crouched still (0) to
## walking (1), how fast to play that].
static func crouched_stride(speed: float) -> Vector2:
	var pace := speed / (BodyShape.SIZE * model_scale())
	return Vector2(minf(pace / CROUCH_PACE, 1.0), maxf(pace / CROUCH_PACE, 1.0))


## Whether animation `name` goes round and round.
static func loops(name: String) -> bool:
	if LOOPED.has(name) or name.begins_with("crouch_walk"):
		return true
	for gait: String in GAITS:
		if name.begins_with(gait):
			return true
	return false


## Takes the travel out of `animation`, which Mixamo leaves in many: the hips
## keep their bob but stay over the feet, for the player's own movement to
## carry the body.
static func in_place(animation: Animation) -> void:
	var track := animation.find_track(NodePath("%GeneralSkeleton:Hips"), Animation.TYPE_POSITION_3D)
	if track < 0 or animation.track_get_key_count(track) == 0:
		return
	var start: Vector3 = animation.track_get_key_value(track, 0)
	for key in animation.track_get_key_count(track):
		var at: Vector3 = animation.track_get_key_value(track, key)
		animation.track_set_key_value(track, key, Vector3(start.x, at.y, start.z))


func _build() -> void:
	_model = load(MODEL).instantiate()
	_model.name = "Soldier"
	# The model faces +Z; the body faces -Z.
	_model.rotation.y = PI
	_model.scale = Vector3.ONE * model_scale()
	add_child(_model)
	_skeleton = _model.find_children("*", "Skeleton3D")[0]
	_left_hand_bone = _skeleton.find_bone("LeftHand")
	_right_hand_bone = _skeleton.find_bone("RightHand")
	_cap_mesh = _model.find_child("Hat_LP2")
	# The arms cut from it for its player's own view aren't for others.
	for name in FIRST_PERSON:
		_model.find_child(name).visible = false
	_face_mesh = _model.find_child("CC_Base_Body")
	for name: String in _face_shape_names():
		_shapes[name] = _face_mesh.find_blend_shape_by_name(name)
	_shirt = _tinted("Shirt_02_LP2")
	_cap = _tinted("Hat_LP2")
	_overlay.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_overlay.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_overlay.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	_build_animations()
	if not _found_hold:
		_find_hold()
	_build_reach()
	_skeleton.skeleton_updated.connect(_place_weapon)


# Gives mesh `name` a material of its own, to colour, and returns it.
func _tinted(name: String) -> StandardMaterial3D:
	var mesh: MeshInstance3D = _model.find_child(name)
	var material := (mesh.get_active_material(0) as StandardMaterial3D).duplicate() as StandardMaterial3D
	mesh.material_override = material
	return material


static func _face_shape_names() -> Array:
	var names := ["Eye_Blink_L", "Eye_Blink_R", "Eye_L_Look_L", "Eye_R_Look_L", "Eye_L_Look_R", "Eye_R_Look_R", "Eye_L_Look_Up", "Eye_R_Look_Up", "Eye_L_Look_Down", "Eye_R_Look_Down"]
	for face: Dictionary in FACES.values():
		for name: String in face:
			if not names.has(name):
				names.append(name)
	return names


## The animations, made ready to play once and shared by every soldier.
static var _library: AnimationLibrary
## How the animations are blended, also shared: each soldier's own
## AnimationTree keeps where it has got to.
static var _blend_tree: AnimationNodeBlendTree


## The animations, each in place, looped or not, under the name of its file.
static func library() -> AnimationLibrary:
	if _library != null:
		return _library
	_library = AnimationLibrary.new()
	for file in DirAccess.get_files_at(ANIMATIONS):
		# Exported, the files are listed by their imported names.
		file = file.trim_suffix(".import").trim_suffix(".remap")
		if file.get_extension() != "fbx" or _library.has_animation(file.get_basename()):
			continue
		var source: AnimationLibrary = load(ANIMATIONS.path_join(file))
		# Mixamo's; a file downloaded with its character also has its bind
		# pose, a still "Take 001".
		var clip: StringName = &"mixamo_com" if source.has_animation(&"mixamo_com") else source.get_animation_list()[0]
		var animation: Animation = source.get_animation(clip).duplicate(true)
		in_place(animation)
		var name := file.get_basename()
		animation.loop_mode = Animation.LOOP_LINEAR if loops(name) else Animation.LOOP_NONE
		_library.add_animation(name, animation)
	# The pistols carried in it are held in loose hands: each closes round
	# its grip as the right does round a pistol it aims.
	if _library.has_animation("dual_carry") and _library.has_animation("pistol_aim"):
		gripping(_library.get_animation("dual_carry"), _library.get_animation("pistol_aim"))
	return _library


## Gives both hands in `animation` the fingers of the right hand at the start
## of `from`, the left's mirrored from the right's, held still.
static func gripping(animation: Animation, from: Animation) -> void:
	for finger: String in FINGERS:
		var source := from.find_track(NodePath("%GeneralSkeleton:Right" + finger), Animation.TYPE_ROTATION_3D)
		if source < 0:
			continue
		var turn := from.rotation_track_interpolate(source, 0.0)
		# The humanoid's left bones are its right ones' mirror images, so a
		# turn mirrors by flipping it about their length.
		for side: Array in [["Right", turn], ["Left", Quaternion(turn.x, -turn.y, -turn.z, turn.w)]]:
			var path := NodePath("%GeneralSkeleton:" + side[0] + finger)
			var track := animation.find_track(path, Animation.TYPE_ROTATION_3D)
			if track < 0:
				track = animation.add_track(Animation.TYPE_ROTATION_3D)
				animation.track_set_path(track, path)
			while animation.track_get_key_count(track) > 0:
				animation.track_remove_key(track, 0)
			animation.rotation_track_insert_key(track, 0.0, side[1])


func _build_animations() -> void:
	_crouches = library().has_animation("crouch")
	var player := AnimationPlayer.new()
	player.name = "Animations"
	_model.add_child(player)
	player.add_animation_library("", library())
	if _blend_tree == null:
		_blend_tree = _blending(library())
	_tree = AnimationTree.new()
	_tree.name = "Tree"
	_model.add_child(_tree)
	_tree.anim_player = _tree.get_path_to(player)
	_tree.tree_root = _blend_tree
	_tree.active = true


# How the legs' animations, the upper body's and a fall when knocked out are
# blended from those in `animations`.
static func _blending(animations: AnimationLibrary) -> AnimationNodeBlendTree:
	var crouches := animations.has_animation("crouch")
	var tree := AnimationNodeBlendTree.new()
	tree.add_node("ground", _gaits(animations, ""), Vector2(0, 0))
	tree.add_node("pace", AnimationNodeTimeScale.new(), Vector2(200, 0))
	tree.connect_node("pace", 0, "ground")
	var legs := AnimationNodeTransition.new()
	legs.xfade_time = 0.15
	for input in ["ground", "crouch", "jump", "air", "land", "slide"]:
		legs.add_input(input)
	tree.add_node("legs", legs, Vector2(400, 0))
	tree.connect_node("legs", 0, "pace")
	if crouches:
		tree.add_node("crouched", _gaits(animations, "crouch_"), Vector2(0, 150))
		tree.add_node("crouch_pace", AnimationNodeTimeScale.new(), Vector2(200, 150))
		tree.connect_node("crouch_pace", 0, "crouched")
		tree.connect_node("legs", 1, "crouch_pace")
	else:
		tree.add_node("crouched", _clip("idle"), Vector2(200, 150))
		tree.connect_node("legs", 1, "crouched")
	# Leaving the ground, in the air, and landing.
	for index in 3:
		var part: Array = [["jump", "jump_up"], ["air", "jump_loop"], ["land", "jump_down"]][index]
		tree.add_node(part[0], _clip(part[1]), Vector2(200, 300 + 60 * index))
		tree.connect_node("legs", 2 + index, part[0])
	tree.add_node("slide", _clip("slide"), Vector2(200, 500))
	tree.connect_node("legs", 5, "slide")

	var acts := AnimationNodeTransition.new()
	acts.xfade_time = 0.12
	var doing: Array = ACTIONS.values() + EMOTES.values() + ["pistol_aim", "dual_carry"]
	for index in doing.size():
		acts.add_input(doing[index])
		var clip: String = doing[index] if animations.has_animation(doing[index]) else "aim"
		tree.add_node("act_" + doing[index], _clip(clip), Vector2(400, 500 + 100 * index))
	tree.add_node("acts", acts, Vector2(600, 500))
	for index in doing.size():
		tree.connect_node("acts", index, "act_" + doing[index])

	var upper := AnimationNodeBlend2.new()
	upper.filter_enabled = true
	for bone: String in _upper_bones():
		upper.set_filter_path(NodePath("%GeneralSkeleton:" + bone), true)
	tree.add_node("upper", upper, Vector2(800, 0))
	tree.connect_node("upper", 0, "legs")
	tree.connect_node("upper", 1, "acts")

	var knock := AnimationNodeOneShot.new()
	knock.fadein_time = 0.1
	knock.fadeout_time = 0.0
	tree.add_node("knock", knock, Vector2(1000, 0))
	tree.connect_node("knock", 0, "upper")
	tree.add_node("knocked_out", _clip("knocked_out"), Vector2(800, 200))
	tree.connect_node("knock", 1, "knocked_out")
	tree.connect_node("output", 0, "knock")
	return tree


static func _upper_bones() -> Array:
	var bones := UPPER.duplicate()
	for side in ["Left", "Right"]:
		for finger: String in FINGERS:
			bones.append(side + finger)
	return bones


static func _clip(name: String) -> AnimationNodeAnimation:
	var node := AnimationNodeAnimation.new()
	node.animation = name
	return node


# The legs' blend of standing, walking, running and sprinting each of eight
# ways, from the animations named with `prefix` (none, or "crouch_" for
# crouched, which only walks).
static func _gaits(animations: AnimationLibrary, prefix: String) -> AnimationNodeBlendSpace2D:
	var space := AnimationNodeBlendSpace2D.new()
	space.min_space = Vector2(-3, -3)
	space.max_space = Vector2(3, 3)
	var still := "idle" if prefix == "" else "crouch"
	space.add_blend_point(_clip(still), Vector2.ZERO, -1, still)
	var gaits: Dictionary = GAITS if prefix == "" else {"crouch_walk": 1.0}
	for gait: String in gaits:
		var reach: float = gaits[gait]
		for way: String in WAYS:
			var name: String = gait + way
			if animations.has_animation(name):
				space.add_blend_point(_clip(name), (WAYS[way] as Vector2) * reach, -1, name)
	return space


func _build_reach() -> void:
	_spine = SPINE_AIM.new()
	_spine.name = "SpineAim"
	_spine.hand = _right_hand_bone
	_spine.in_hand = hold("rifle").in_hand
	_skeleton.add_child(_spine)
	# Each arm reaching out with a pistol, aiming a pair.
	for side: String in ["Right", "Left"]:
		_dual_reach[side] = _reach_out(side)
	var twist: SkeletonModifier3D = preload("res://player/forearm_twist.gd").new()
	twist.name = "ForearmTwist"
	_skeleton.add_child(twist)


# Reaches `side`'s arm for where its wrist is to go, and turns its hand, for
# aiming a pair of pistols; off until then.
func _reach_out(side: String) -> Dictionary:
	var wrist := Node3D.new()
	wrist.name = side + "DualWrist"
	add_child(wrist)
	var elbow := Node3D.new()
	elbow.name = side + "DualElbow"
	add_child(elbow)
	var ik := TwoBoneIK3D.new()
	ik.name = side + "DualReach"
	_skeleton.add_child(ik)
	ik.set_setting_count(1)
	ik.set_root_bone_name(0, side + "UpperArm")
	ik.set_middle_bone_name(0, side + "LowerArm")
	ik.set_end_bone_name(0, side + "Hand")
	ik.set_target_node(0, ik.get_path_to(wrist))
	ik.set_pole_node(0, ik.get_path_to(elbow))
	var turn: SkeletonModifier3D = preload("res://player/hand_pose.gd").new()
	turn.name = side + "DualHand"
	turn.bone = _skeleton.find_bone(side + "Hand")
	_skeleton.add_child(turn)
	for modifier: SkeletonModifier3D in [ik, turn]:
		modifier.influence = 0.0
		modifier.active = false
	var upper := _skeleton.find_bone(side + "UpperArm")
	var lower := _skeleton.find_bone(side + "LowerArm")
	var hand := _skeleton.find_bone(side + "Hand")
	var length := _skeleton.get_bone_global_rest(upper).origin.distance_to(_skeleton.get_bone_global_rest(lower).origin) + _skeleton.get_bone_global_rest(lower).origin.distance_to(_skeleton.get_bone_global_rest(hand).origin)
	return {"wrist": wrist, "elbow": elbow, "ik": ik, "turn": turn, "shoulder": upper, "length": length}


# Aiming a pair of pistols, by `amount` (0 to 1): each arm reaches out along
# `looking` (in the skeleton's space), the pistols in its hands pointing
# that way, as each grip holds them.
func _aim_pair(amount: float, looking: Vector3) -> void:
	var right_shoulder := _skeleton.get_bone_global_pose(_dual_reach.Right.shoulder).origin
	var left_shoulder := _skeleton.get_bone_global_pose(_dual_reach.Left.shoulder).origin
	var middle := (right_shoulder + left_shoulder) / 2.0
	# To the right across the look, whichever way the shoulders are turned.
	var level := Vector3(looking.x, 0.0, looking.z)
	var right := (level if not level.is_zero_approx() else Vector3.FORWARD).cross(Vector3.UP).normalized()
	var apart := right * right_shoulder.distance_to(left_shoulder) * DUAL_APART / 2.0
	var facing := Basis.looking_at(looking, Vector3.UP).scaled(Vector3.ONE * _weapon_size)
	for side: String in _dual_reach:
		var reach: Dictionary = _dual_reach[side]
		var on := amount > 0.0
		for modifier: SkeletonModifier3D in [reach.ik, reach.turn]:
			modifier.active = on
			modifier.influence = amount
		if not on:
			continue
		var shoulder := right_shoulder if side == "Right" else left_shoulder
		var outward := apart if side == "Right" else -apart
		var fist: Vector3 = middle + looking * reach.length * DUAL_REACH + outward
		var weapon := Transform3D(facing, fist - facing * WeaponModels.held_at(_weapon))
		var hand := weapon * (in_hand(_weapon) if side == "Right" else left_in_hand(_weapon)).affine_inverse()
		(reach.wrist as Node3D).global_position = _skeleton.global_transform * hand.origin
		reach.turn.facing = hand.basis
		# The elbow below and out to its side.
		(reach.elbow as Node3D).global_position = _skeleton.global_transform * (shoulder + looking * reach.length * 0.4 + Vector3.DOWN * reach.length * 0.5 + outward)


func weapon_scale() -> float:
	return WEAPON_SCALE


## Puts `gun`, the weapon its player holds, in its right hand, wherever its
## animations take that, rather than where the player's scene would hold it.
func hold_weapon(gun: Node3D) -> void:
	# A new weapon, or a different model of one: how its hands hold it.
	# The newest: one changed from waits to be freed beside it for a frame.
	var model := gun.get_child(-1) as Node3D if gun.get_child_count() > 0 else null
	if _held_gun != gun or _held_model != model:
		_held_gun = gun
		_held_model = model
		_weapon = model.get_meta("weapon", "") if model != null else ""
		_grip = grip_of(_weapon)
		_spine.in_hand = in_hand(_weapon)
		_left_weapon = model.find_child("LeftPistol", false, false) as Node3D if model != null else null
	# Making an emote, it puts the weapon away.
	if _emote >= 0:
		gun.visible = false
	# Where the hand last had it. The skeleton's pose doesn't keep the turn
	# of the spine between its updates, so it is placed after each, and put
	# back there in between for the player to find its other hand's place.
	if gun.is_inside_tree() and is_inside_tree():
		gun.global_transform = global_transform * _weapon_here


## How each grip (see GRIPS) holds a weapon, as {in_hand: where the weapon
## is from the right hand's bone, in its aim, pointing straight ahead with its
## grip (WeaponModels.GRIP) in the fist; left: where the left hand is in that
## aim, from the right; left_in_hand: where a second weapon is from the left
## hand's bone, as in_hand is from the right's; elbows: where each elbow is,
## from the right hand}. The same for every soldier, so it is worked out once.
static var _holds := {}
static var _found_hold := false
## Turns this node's directions into its skeleton's.
static var _body_to_skeleton: Basis
## How big a weapon is in the skeleton's space.
static var _weapon_size := 1.0


## How `grip` holds a weapon (see _holds).
static func hold(grip: String) -> Dictionary:
	return _holds[grip]


## The grip a weapon is held with, by the weapon's id.
static func grip_of(weapon: String) -> String:
	return GRIP_OF.get(weapon, "rifle")


## Where weapon `weapon` is from the right hand's bone: as its grip holds
## any weapon (see _holds), moved so that the hand closes round its own grip.
static func in_hand(weapon: String) -> Transform3D:
	return (hold(grip_of(weapon)).in_hand as Transform3D) * _to_grip(weapon)


## Where the second of a pair of weapons `weapon` is from the left hand's
## bone, as in_hand is for the first.
static func left_in_hand(weapon: String) -> Transform3D:
	return (hold(grip_of(weapon)).left_in_hand as Transform3D) * _to_grip(weapon)


# From where a grip holds any weapon to where it holds `weapon`.
static func _to_grip(weapon: String) -> Transform3D:
	return Transform3D(Basis(), WeaponModels.GRIP - WeaponModels.held_at(weapon))


## Works out how the hands hold weapons (_holds), if no soldier has yet:
## they are worked out from one.
static func find_hold_once() -> void:
	if not _found_hold:
		var soldier: Node3D = load("res://player/soldier_character.gd").new()
		soldier.free()


# Works out how each grip holds a weapon, posing the skeleton as at the
# start of its aim to do it, then letting it go.
func _find_hold() -> void:
	for grip: String in GRIPS:
		_holds[grip] = _hold_in(library().get_animation(GRIPS[grip]))
	_found_hold = true
	_skeleton.reset_bone_poses()


# How the hands hold a weapon in `aim` (see _holds).
func _hold_in(aim: Animation) -> Dictionary:
	_skeleton.reset_bone_poses()
	for track in aim.get_track_count():
		var bone := _skeleton.find_bone(aim.track_get_path(track).get_concatenated_subnames())
		if bone < 0:
			continue
		match aim.track_get_type(track):
			Animation.TYPE_ROTATION_3D:
				_skeleton.set_bone_pose_rotation(bone, aim.rotation_track_interpolate(track, 0.0))
			Animation.TYPE_POSITION_3D:
				_skeleton.set_bone_pose_position(bone, aim.position_track_interpolate(track, 0.0))
	var hand := _skeleton.get_bone_global_pose(_right_hand_bone)
	# From the skeleton's space to this node's, where the weapon points down
	# -Z, ahead, at the size others see it.
	var to_body := _model.transform
	var chain := Transform3D()
	var node: Node = _skeleton
	while node != _model:
		chain = (node as Node3D).transform * chain
		node = node.get_parent()
	to_body = to_body * chain
	_body_to_skeleton = to_body.basis.orthonormalized().inverse()
	_weapon_size = (to_body.affine_inverse().basis * Basis.from_scale(Vector3.ONE * WEAPON_SCALE)).get_scale().x
	var held := to_body.affine_inverse() * Transform3D(Basis.from_scale(Vector3.ONE * WEAPON_SCALE), Vector3.ZERO)
	# The grip in the right hand's fist, and the left hand as the aim holds
	# a rifle's fore-end.
	held.origin = _fist("Right") - held.basis * WeaponModels.GRIP
	var left := _skeleton.get_bone_global_pose(_left_hand_bone)
	var held_left := Transform3D(held.basis, _fist("Left") - held.basis * WeaponModels.GRIP)
	var elbows := {}
	for side: String in ["Right", "Left"]:
		elbows[side] = hand.affine_inverse() * _skeleton.get_bone_global_pose(_skeleton.find_bone(side + "LowerArm")).origin
	return {"in_hand": hand.affine_inverse() * held, "left": hand.affine_inverse() * left, "left_in_hand": left.affine_inverse() * held_left, "elbows": elbows}


# Where `side`'s hand closes round what it holds, in the skeleton's space:
# between the base and the tip of its middle finger.
func _fist(side: String) -> Vector3:
	var base := _skeleton.get_bone_global_pose(_skeleton.find_bone(side + "MiddleProximal")).origin
	var tip := _skeleton.get_bone_global_pose(_skeleton.find_bone(side + "MiddleDistal")).origin
	return (base + tip) / 2.0


# Puts the weapon held in the right hand as the skeleton now stands, its
# animations and the turn of its spine and all, and notes where that is. The
# left hand stays where its animations have it: the weapons are to fit the
# hands, not the hands the weapons.
func _place_weapon() -> void:
	var weapon := _skeleton.get_bone_global_pose(_right_hand_bone) * in_hand(_weapon)
	_weapon_here = global_transform.affine_inverse() * _skeleton.global_transform * weapon
	if _held_gun == null or not is_instance_valid(_held_gun) or not _held_gun.is_inside_tree():
		return
	_held_gun.global_transform = global_transform * _weapon_here
	# A second weapon goes in the left hand.
	if _left_weapon != null and is_instance_valid(_left_weapon):
		_left_weapon.global_transform = _skeleton.global_transform * _skeleton.get_bone_global_pose(_left_hand_bone) * left_in_hand(_weapon)


func set_colour(colour: Color) -> void:
	super(colour)
	if _shirt != null:
		# The camouflage takes on the player's colour, and the cap a darker
		# shade of it.
		_shirt.albedo_color = shirt_colour(colour)
		_cap.albedo_color = colour.darkened(0.3)


## The colour the camouflage is tinted for a player of `colour`.
static func shirt_colour(colour: Color) -> Color:
	return colour.lerp(Color.WHITE, 0.25)


func _light() -> void:
	if _model == null:
		return
	var lit := _glowing or is_flashing()
	_overlay.albedo_color = Color(1, 1, 1, 0.5) if is_flashing() else Color(1.0, 0.45, 0.1, 0.35)
	for mesh: MeshInstance3D in _model.find_children("*", "MeshInstance3D"):
		mesh.material_overlay = _overlay if lit else null


func set_in_play(in_play: bool) -> void:
	var was_melting := _melting
	super(in_play)
	if _tree == null:
		return
	if not in_play and not was_melting and _ending == THROWN:
		_tree.set("parameters/knock/request", AnimationNodeOneShot.ONE_SHOT_REQUEST_FIRE)
	elif in_play:
		_tree.set("parameters/knock/request", AnimationNodeOneShot.ONE_SHOT_REQUEST_ABORT)


# Knocked out, it falls back with its animation where it was thrown, rather
# than tipping and melting.
func _draw_melt(delta: float) -> void:
	position += _thrown * delta * BodyShape.SIZE
	_thrown *= exp(-5.0 * delta)


func _draw_face(face: String, delta: float) -> void:
	if _face_mesh == null:
		return
	var nearer := 1.0 - exp(-delta / FACE_TIME)
	var wanted: Dictionary = FACES[face].duplicate()
	if is_blinking():
		wanted["Eye_Blink_L"] = 1.0
		wanted["Eye_Blink_R"] = 1.0
	# The eyes turn toward whoever is near, and up or down with the gaze.
	var side := "L" if _glance < 0.0 else "R"
	for eye in ["L", "R"]:
		wanted["Eye_%s_Look_%s" % [eye, side]] = absf(_glance)
		wanted["Eye_%s_Look_%s" % [eye, "Up" if _gaze > 0.0 else "Down"]] = clampf(absf(_gaze) * 1.5, 0.0, 1.0)
	for name: String in _shapes:
		var index: int = _shapes[name]
		if index < 0:
			continue
		var now := _face_mesh.get_blend_shape_value(index)
		_face_mesh.set_blend_shape_value(index, lerpf(now, wanted.get(name, 0.0), nearer))


func _process(delta: float) -> void:
	super(delta)
	if _tree == null:
		return
	# The legs: which of their animations, which way and how fast.
	var legs := "ground"
	match _state:
		CharacterMotion.CROUCH, CharacterMotion.CROUCH_WALK:
			legs = "crouch"
		CharacterMotion.JUMP:
			legs = "jump"
		CharacterMotion.FALL:
			legs = "air"
		CharacterMotion.LAND:
			legs = "land"
		CharacterMotion.SLIDE:
			legs = "slide"
	if _tree.get("parameters/legs/current_state") != legs:
		_tree.set("parameters/legs/transition_request", legs)
	var gait := stride(_speed)
	var way := _heading.normalized() if _heading.length() > 0.01 else AHEAD
	_tree.set("parameters/ground/blend_position", way * gait.x)
	_tree.set("parameters/pace/scale", gait.y)
	if _crouches:
		var crouched := crouched_stride(_speed)
		_tree.set("parameters/crouched/blend_position", way * crouched.x)
		_tree.set("parameters/crouch_pace/scale", crouched.y)
	# The upper body: an emote, or what the arms are doing, or the weapon up
	# at the shoulder just after firing; with none of these, it carries the
	# weapon as the legs' own animation does.
	_since_fired = 0.0 if _action == CharacterMotion.FIRE else _since_fired + delta
	var doing := ""
	if _emote >= 0 and EMOTES.has(_emote):
		doing = EMOTES[_emote]
	elif ACTIONS.has(_action):
		doing = ACTIONS[_action]
	elif _since_fired < RAISED_AFTER_FIRING:
		doing = "aim"
	# Each grip aims, and carries, its own way.
	var raised := doing == "aim" or doing == "fire"
	if _grip != "rifle":
		if doing == "aim" or doing == "fire":
			doing = GRIPS[_grip]
		elif doing == "":
			doing = CARRY.get(_grip, "")
	if doing != "" and _tree.get("parameters/acts/current_state") != doing:
		_tree.set("parameters/acts/transition_request", doing)
	var upper: float = _tree.get("parameters/upper/blend_amount")
	_tree.set("parameters/upper/blend_amount", move_toward(upper, 1.0 if doing != "" and not _melting else 0.0, delta / 0.15))
	# Aiming or firing, the weapon points where the player looks, the spine
	# turning to bring it round.
	var looking := Vector3(0.0, sin(_gaze), -cos(_gaze))
	_spine.aim = _body_to_skeleton * looking
	var aimed := (["aim", "fire"].has(doing) or GRIPS.values().has(doing)) and not _melting
	# A pair of pistols is aimed by the arms alone, reaching out with them.
	var pair := raised and _grip == "dual" and not _melting
	_dual_aim = move_toward(_dual_aim, 1.0 if pair else 0.0, delta / GRIP_TIME)
	_aim_pair(_dual_aim, SPINE_AIM.pitched_at_most(_spine.aim, SPINE_AIM.AIM_DOWN, SPINE_AIM.AIM_UP))
	_spine.influence = move_toward(_spine.influence, 1.0 if aimed and _grip != "dual" else 0.0, delta / GRIP_TIME)
	# What is held in the free hand goes with it.
	if _held != null:
		_held.position = _bone_here(_left_hand_bone)


# Where bone `bone` is, in this node's measurements.
func _bone_here(bone: int) -> Vector3:
	return to_local(_skeleton.global_transform * _skeleton.get_bone_global_pose(bone).origin)
