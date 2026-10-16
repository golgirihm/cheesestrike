extends "res://player/cheese_character.gd"
## The realistic soldier: the Tactical Soldier 02 model in game/licensed,
## moved by its Mixamo animations. It works out what it is doing just as the
## drawn soldier does (cheese_character.gd, whose workings it shares), and
## shows it its own way:
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
## The model and its animations are licensed and kept private, so this is
## only used where they are (see available); everywhere else, and always on
## the web, players are the drawn soldier.

const MODEL := "res://licensed/soldier/soldier.glb"
const ANIMATIONS := "res://licensed/animations"
## How high the model's eyes are, in its own metres. It is scaled to put them
## where the player sees from (BodyShape.HEAD_HEIGHT), so that what can be hit
## (BodyShape's boxes) is where it is drawn.
const MODEL_EYE_HEIGHT := 1.69
## How fast the model's walk and run go across the ground, in its own metres a
## second, played at their own pace.
const WALK_PACE := 1.26
const RUN_PACE := 3.03
## The animations that go round and round rather than playing once.
const LOOPED := ["idle", "aim", "walk", "walk_back", "walk_left", "walk_right", "run", "run_back", "run_left", "run_right", "fire", "crouch", "crouch_aim", "crouch_walk", "crouch_walk_back", "crouch_walk_left", "crouch_walk_right", "emote_dance"]
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
## Seconds a knocked-out soldier takes to fall back before it is gone.
const FALL_TIME := 1.6
## Seconds the hands take to go onto the weapon, or off it.
const GRIP_TIME := 0.12
## Where its hand holds the weapon's grip, from the eyes in its own
## measurements (BodyShape's), at rest and aimed: within reach of real arms,
## in front of the right shoulder, and up near the eye to aim.
const HELD_GRIP := Vector3(0.085, -0.19, -0.19)
const AIMED_GRIP := Vector3(0.07, -0.1, -0.17)
## How big its weapon is drawn, against the drawn soldier's, which is made
## for chunkier hands.
const WEAPON_SCALE := 0.55

var _model: Node3D
var _skeleton: Skeleton3D
var _tree: AnimationTree
var _spine: SkeletonModifier3D
var _right_arm: TwoBoneIK3D
var _left_arm: TwoBoneIK3D
var _grip: Node3D
var _off_hand: Node3D
var _face_mesh: MeshInstance3D
var _shirt: StandardMaterial3D
var _cap: StandardMaterial3D
var _cap_mesh: MeshInstance3D
var _overlay := StandardMaterial3D.new()
var _shapes := {}
var _crouches := false
var _left_hand_bone := -1
var _steadying := false


## Whether the realistic soldier can be shown here: its licensed model is
## checked out, and this isn't the web build, which leaves it out.
static func available() -> bool:
	return not OS.has_feature("web") and ResourceLoader.exists(MODEL)


## How much bigger the model is drawn than it is.
static func model_scale() -> float:
	return BodyShape.HEAD_HEIGHT / MODEL_EYE_HEIGHT


## How the legs go for a player moving at `speed` across the arena (metres a
## second), as [how far along from standing (0) through walking (1) to running
## (2), how fast to play that]: the walk and the run are blended and sped up so
## that the feet keep pace with the ground.
static func stride(speed: float) -> Vector2:
	var pace := speed / (BodyShape.SIZE * model_scale())
	if pace < WALK_PACE:
		return Vector2(pace / WALK_PACE, 1.0)
	if pace < RUN_PACE:
		return Vector2(1.0 + (pace - WALK_PACE) / (RUN_PACE - WALK_PACE), 1.0)
	return Vector2(2.0, pace / RUN_PACE)


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
	_cap_mesh = _model.find_child("Hat_LP2")
	_face_mesh = _model.find_child("CC_Base_Body")
	for name: String in _face_shape_names():
		_shapes[name] = _face_mesh.find_blend_shape_by_name(name)
	_shirt = _tinted("Shirt_02_LP2")
	_cap = _tinted("Hat_LP2")
	_overlay.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_overlay.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_overlay.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	_build_animations()
	_build_reach()


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
		var animation: Animation = source.get_animation(source.get_animation_list()[0]).duplicate(true)
		in_place(animation)
		var name := file.get_basename()
		animation.loop_mode = Animation.LOOP_LINEAR if LOOPED.has(name) else Animation.LOOP_NONE
		_library.add_animation(name, animation)
	return _library


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
	for input in ["ground", "crouch", "jump", "slide"]:
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
	tree.add_node("jump", _clip("jump"), Vector2(200, 300))
	tree.connect_node("legs", 2, "jump")
	tree.add_node("slide", _clip("slide"), Vector2(200, 400))
	tree.connect_node("legs", 3, "slide")

	var acts := AnimationNodeTransition.new()
	acts.xfade_time = 0.12
	var doing: Array = ACTIONS.values() + EMOTES.values()
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


# The legs' blend of standing, walking and running each way, from the
# animations named with `prefix` (none, or "crouch_" for crouched, which has
# no run).
static func _gaits(animations: AnimationLibrary, prefix: String) -> AnimationNodeBlendSpace2D:
	var space := AnimationNodeBlendSpace2D.new()
	space.min_space = Vector2(-2, -2)
	space.max_space = Vector2(2, 2)
	var still := "idle" if prefix == "" else "crouch"
	space.add_blend_point(_clip(still), Vector2.ZERO, -1, still)
	var gaits := {"walk": 1.0, "run": 2.0} if prefix == "" else {"crouch_walk": 1.0}
	for gait: String in gaits:
		var reach: float = gaits[gait]
		for way: Array in [["", Vector2(0, 1)], ["_back", Vector2(0, -1)], ["_left", Vector2(-1, 0)], ["_right", Vector2(1, 0)]]:
			var name: String = gait + way[0]
			if animations.has_animation(name):
				space.add_blend_point(_clip(name), (way[1] as Vector2) * reach, -1, name)
	return space


func _build_reach() -> void:
	_grip = Node3D.new()
	_grip.name = "Grip"
	add_child(_grip)
	_off_hand = Node3D.new()
	_off_hand.name = "OffHand"
	add_child(_off_hand)
	_spine = preload("res://player/spine_aim.gd").new()
	_spine.name = "SpineAim"
	# The model faces +Z in its skeleton's space, so its right is -X.
	_spine.across = Vector3.LEFT
	_skeleton.add_child(_spine)
	_right_arm = _reach_ik("Right", _grip, Vector3(0.45, 0.35, 0.25))
	_left_arm = _reach_ik("Left", _off_hand, Vector3(-0.45, 0.35, 0.1))
	_left_arm.influence = 0.0


# Pulls the `side` hand onto `target`, its elbow toward `elbow` in this node's
# measurements.
func _reach_ik(side: String, target: Node3D, elbow: Vector3) -> TwoBoneIK3D:
	var ik := TwoBoneIK3D.new()
	ik.name = side + "ArmReach"
	_skeleton.add_child(ik)
	var pole := Node3D.new()
	pole.name = side + "Elbow"
	pole.position = elbow
	add_child(pole)
	ik.set_setting_count(1)
	ik.set_root_bone_name(0, side + "UpperArm")
	ik.set_middle_bone_name(0, side + "LowerArm")
	ik.set_end_bone_name(0, side + "Hand")
	ik.set_target_node(0, ik.get_path_to(target))
	ik.set_pole_node(0, ik.get_path_to(pole))
	return ik


# Where the weapon is held for its grip to be at `grip`, from the eyes in the
# arena's metres, at the size others see it.
static func _holding_at(grip: Vector3) -> Vector3:
	return grip * BodyShape.SIZE - WeaponModels.GRIP * BodyShape.SIZE * WEAPON_SCALE


func weapon_scale() -> float:
	return WEAPON_SCALE


func weapon_rest(_rest: Vector3) -> Vector3:
	return _holding_at(HELD_GRIP)


func weapon_aim(_aim: Vector3) -> Vector3:
	return _holding_at(AIMED_GRIP)


func reach_for(hand: Vector3) -> void:
	if _grip != null:
		_grip.position = hand


func steady_with(hand: Vector3) -> void:
	super(hand)
	if _off_hand != null and hand.is_finite():
		_off_hand.position = hand


func set_colour(colour: Color) -> void:
	super(colour)
	if _shirt != null:
		# The camouflage takes on the player's colour, and the cap a darker
		# shade of it.
		_shirt.albedo_color = colour.lerp(Color.WHITE, 0.25)
		_cap.albedo_color = colour.darkened(0.3)


func _light() -> void:
	super()
	if _model == null:
		return
	var lit := _glowing or is_flashing()
	_overlay.albedo_color = Color(1, 1, 1, 0.5) if is_flashing() else Color(1.0, 0.45, 0.1, 0.35)
	for mesh: MeshInstance3D in _model.find_children("*", "MeshInstance3D"):
		mesh.material_overlay = _overlay if lit else null


# It always wears its own cap and sunglasses, whatever hat is chosen.
func wear(look: int) -> void:
	super(look)
	if _hat != null:
		_hat.visible = false


func set_in_play(in_play: bool) -> void:
	var was_melting := _melting
	super(in_play)
	if _tree == null:
		return
	if not in_play and not was_melting and _ending == THROWN:
		_tree.set("parameters/knock/request", AnimationNodeOneShot.ONE_SHOT_REQUEST_FIRE)
	elif in_play:
		_tree.set("parameters/knock/request", AnimationNodeOneShot.ONE_SHOT_REQUEST_ABORT)


func _melt_time() -> float:
	return FALL_TIME


# Knocked out, it falls back with its animation where it was thrown, rather
# than tipping and melting.
func _draw_melt(delta: float) -> void:
	position += _thrown * delta * BodyShape.SIZE
	_thrown *= exp(-5.0 * delta)


func _melt_scale() -> Vector3:
	return Vector3.ONE


# The arms are moved by the animations and the reach, set each frame in
# _process; this only notes whether the free hand is on the weapon.
func _draw_arm(steadying: bool, _emote_pose: Dictionary) -> void:
	_steadying = steadying


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
		CharacterMotion.JUMP, CharacterMotion.FALL:
			legs = "jump"
		CharacterMotion.SLIDE:
			legs = "slide"
	if _tree.get("parameters/legs/current_state") != legs:
		_tree.set("parameters/legs/transition_request", legs)
	var gait := stride(_speed)
	var way := _heading.normalized() if _heading.length() > 0.01 else AHEAD
	_tree.set("parameters/ground/blend_position", way * gait.x)
	_tree.set("parameters/pace/scale", gait.y)
	if _crouches:
		_tree.set("parameters/crouched/blend_position", way * minf(gait.x, 1.0))
		_tree.set("parameters/crouch_pace/scale", maxf(gait.x, 1.0) * gait.y)
	# The upper body: an emote, or what the arms are doing, or the legs'
	# own animation, which holds the weapon.
	var doing := ""
	if _emote >= 0 and EMOTES.has(_emote):
		doing = EMOTES[_emote]
	elif ACTIONS.has(_action):
		doing = ACTIONS[_action]
	if doing != "" and _tree.get("parameters/acts/current_state") != doing:
		_tree.set("parameters/acts/transition_request", doing)
	var upper: float = _tree.get("parameters/upper/blend_amount")
	_tree.set("parameters/upper/blend_amount", move_toward(upper, 1.0 if doing != "" else 0.0, delta / 0.12))
	# Looking up and down, and the hands on the weapon.
	_spine.pitch = clampf(_gaze, -1.2, 1.2)
	var holding := not _melting
	_right_arm.influence = move_toward(_right_arm.influence, 1.0 if holding else 0.0, delta / GRIP_TIME)
	_left_arm.influence = move_toward(_left_arm.influence, 1.0 if _steadying and holding else 0.0, delta / GRIP_TIME)
	# What is held in the free hand goes with it.
	if _held != null:
		_held.position = _bone_here(_left_hand_bone)


# Where bone `bone` is, in this node's measurements.
func _bone_here(bone: int) -> Vector3:
	return to_local(_skeleton.global_transform * _skeleton.get_bone_global_pose(bone).origin)
