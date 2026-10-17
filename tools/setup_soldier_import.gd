extends SceneTree
## Sets how Godot imports the realistic soldier and its animations, so that
## every animation plays on the soldier. Both skeletons, the soldier's
## (Character Creator's CC_Base_* bones) and the animations' (Mixamo's
## mixamorig_* bones), are mapped onto Godot's humanoid skeleton, whose bone
## names they then share. Each animation file is imported as an animation
## library.
##
## It rewrites the .import files next to game/licensed/soldier/soldier.glb and
## game/licensed/animations/*.fbx. Import the project once first, so that they
## exist, then run this and import again:
##
##     godot --headless --path game --import
##     godot --headless --path game -s <this file's absolute path>
##     godot --headless --path game --import

const SOLDIER := "res://licensed/soldier/soldier.glb"
const ANIMATIONS := "res://licensed/animations"

## The humanoid bones each side's are mapped to, as [humanoid, Character
## Creator, Mixamo]. Character Creator's Hip carries both the spine and, by
## way of its Pelvis, the legs, so it is the humanoid's Hips.
const BONES := [
	["Root", "CC_Base_BoneRoot", ""],
	["Hips", "CC_Base_Hip", "mixamorig_Hips"],
	["Spine", "CC_Base_Waist", "mixamorig_Spine"],
	["Chest", "CC_Base_Spine01", "mixamorig_Spine1"],
	["UpperChest", "CC_Base_Spine02", "mixamorig_Spine2"],
	["Neck", "CC_Base_NeckTwist01", "mixamorig_Neck"],
	["Head", "CC_Base_Head", "mixamorig_Head"],
	["LeftEye", "CC_Base_L_Eye", ""],
	["RightEye", "CC_Base_R_Eye", ""],
	["Jaw", "CC_Base_JawRoot", ""],
]
## Bones each side has a left and a right of, as [humanoid, Character
## Creator, Mixamo] with the side left out.
const SIDED := [
	["Shoulder", "Clavicle", "Shoulder"],
	["UpperArm", "Upperarm", "Arm"],
	["LowerArm", "Forearm", "ForeArm"],
	["Hand", "Hand", "Hand"],
	["ThumbMetacarpal", "Thumb1", "HandThumb1"],
	["ThumbProximal", "Thumb2", "HandThumb2"],
	["ThumbDistal", "Thumb3", "HandThumb3"],
	["IndexProximal", "Index1", "HandIndex1"],
	["IndexIntermediate", "Index2", "HandIndex2"],
	["IndexDistal", "Index3", "HandIndex3"],
	["MiddleProximal", "Mid1", "HandMiddle1"],
	["MiddleIntermediate", "Mid2", "HandMiddle2"],
	["MiddleDistal", "Mid3", "HandMiddle3"],
	["RingProximal", "Ring1", "HandRing1"],
	["RingIntermediate", "Ring2", "HandRing2"],
	["RingDistal", "Ring3", "HandRing3"],
	["LittleProximal", "Pinky1", "HandPinky1"],
	["LittleIntermediate", "Pinky2", "HandPinky2"],
	["LittleDistal", "Pinky3", "HandPinky3"],
	["UpperLeg", "Thigh", "UpLeg"],
	["LowerLeg", "Calf", "Leg"],
	["Foot", "Foot", "Foot"],
	["Toes", "ToeBase", "ToeBase"],
]
const CHARACTER_CREATOR := 1
const MIXAMO := 2
## The most pixels across any of the model's textures is imported at. The
## clothes come at 4096, far more than a player ever shows of them, even up
## close in their own view; at this they take a quarter of the download and of
## the graphics card's memory.
const TEXTURE_LIMIT := 2048
## The same for the small parts, by the start of their textures' names: the
## sunglasses, also made at 4096, and the tiny parts of the face and hands.
const SMALL_PARTS := {"Glass_": 1024, "Std_Nails": 512, "Std_Eyelash": 512, "Std_Upper_Teeth": 512, "Std_Lower_Teeth": 512, "Std_Eye_": 512, "Std_Cornea_": 512}


func _init() -> void:
	_set_up(SOLDIER, "PATH:Armature/Skeleton3D", CHARACTER_CREATOR, false)
	for file in DirAccess.get_files_at(ANIMATIONS):
		if file.get_extension() == "fbx":
			_set_up(ANIMATIONS.path_join(file), "PATH:Skeleton3D", MIXAMO, true)
	for file in DirAccess.get_files_at(SOLDIER.get_base_dir()):
		if file.get_extension() == "webp":
			_compress(SOLDIER.get_base_dir().path_join(file))
	quit()


## Imports the texture at `path`, one the model's import took out of it, as
## the graphics card keeps it, where the default keeps every pixel as it is,
## and no bigger than the soldier is ever seen (TEXTURE_LIMIT).
func _compress(path: String) -> void:
	var settings := ConfigFile.new()
	if settings.load(path + ".import") != OK:
		push_error("%s.import is missing: import the project first." % path)
		return
	settings.set_value("params", "compress/mode", 2)
	var limit := TEXTURE_LIMIT
	for part: String in SMALL_PARTS:
		if path.get_file().begins_with("soldier_" + part):
			limit = SMALL_PARTS[part]
	settings.set_value("params", "process/size_limit", limit)
	if path.get_basename().ends_with("_Normal"):
		settings.set_value("params", "compress/normal_map", 1)
	settings.save(path + ".import")
	print("compressed ", path)


## Maps the skeleton at `node` in the file at `path`, from `side`'s bone names,
## and imports it as an animation library if `library`.
func _set_up(path: String, node: String, side: int, library: bool) -> void:
	var settings := ConfigFile.new()
	if settings.load(path + ".import") != OK:
		push_error("%s.import is missing: import the project first." % path)
		return
	var subresources: Dictionary = settings.get_value("params", "_subresources", {})
	var nodes: Dictionary = subresources.get("nodes", {})
	var skeleton: Dictionary = nodes.get(node, {})
	skeleton["retarget/bone_map"] = _bone_map(side)
	# The soldier rests with its arms sloping 30 degrees down, Mixamo's in a
	# level T-pose, and an animation's turns are carried across from rest to
	# rest, so both are straightened to the humanoid's own T-pose first.
	skeleton["retarget/rest_fixer/fix_silhouette/enable"] = true
	if library:
		# Mixamo's fingertips and the top of its head have nothing to map to.
		skeleton["retarget/remove_tracks/unmapped_bones"] = true
		settings.set_value("remap", "importer", "animation_library")
		settings.set_value("remap", "type", "AnimationLibrary")
	nodes[node] = skeleton
	subresources["nodes"] = nodes
	settings.set_value("params", "_subresources", subresources)
	settings.save(path + ".import")
	print("set up ", path)


func _bone_map(side: int) -> BoneMap:
	var map := BoneMap.new()
	map.profile = SkeletonProfileHumanoid.new()
	for bone: Array in BONES:
		map.set_skeleton_bone_name(bone[0], bone[side])
	for bone: Array in SIDED:
		for hand: String in ["Left", "Right"]:
			var name: String = bone[side]
			if side == CHARACTER_CREATOR:
				name = "CC_Base_%s_%s" % [hand[0], name]
			else:
				name = "mixamorig_%s%s" % [hand, name]
			map.set_skeleton_bone_name(hand + bone[0], name)
	return map
