extends Node3D
## Shows a clip of the match again (see Recording): how a player was knocked
## out, or the play of the match. The players in the clip are drawn as they
## were, moving along the paths the host kept, with the shots that were
## fired, seen from behind whoever it is about. The real players are put out
## of sight meanwhile, and the view goes back where it was when it is over.
##
## The world adds this as a child and plays what the match hands it.

## Seconds a clip's last moment is held before the view goes back.
const HOLD := 0.4

var world: Node3D
var playing := false
## How many clips have been played, for tests.
var played := 0
## What is written over it, such as "How you were knocked out".
var title := ""
var _clip := {}
var _about := 0
var _time := 0.0
var _camera: Camera3D
var _ghosts := {}
var _shown_shots := 0
var _last := {}


func _init(of: Node3D) -> void:
	world = of
	name = "ReplayView"
	_camera = Camera3D.new()
	_camera.name = "ReplayCamera"
	_camera.fov = 75.0
	add_child(_camera)


## Plays `clip`, seen from behind player `about`, under `heading`. Does
## nothing with a clip that has nothing in it for them.
func play(clip: Dictionary, about: int, heading: String) -> void:
	clip = Recording.clean(clip)
	if clip.is_empty() or not clip.tracks.has(about):
		return
	stop()
	_clip = clip
	_about = about
	_time = 0.0
	_shown_shots = 0
	title = heading
	playing = true
	played += 1
	world.players.visible = false
	for id: int in clip.tracks:
		var ghost: Node3D = preload("res://player/cheese_character.gd").new()
		add_child(ghost)
		ghost.set_colour(world.colour_of(id))
		var record: PlayerRecord = world.match_.state.player(id) if world.match_.has_state() else null
		if record != null:
			ghost.wear(record.look)
		_ghosts[id] = ghost
	_place(0.0)
	_camera.global_transform = _view()
	_camera.make_current()


## Ends the clip, if one is playing, and gives the view back.
func stop() -> void:
	if not playing:
		return
	playing = false
	title = ""
	for ghost: Node3D in _ghosts.values():
		ghost.queue_free()
	_ghosts.clear()
	_last.clear()
	world.players.visible = true
	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null
	if _camera.current and own != null:
		# Their own eyes; if they are out of play, the watching view takes over.
		own.camera.make_current()


## How many players the clip being played shows.
func ghosts() -> int:
	return _ghosts.size()


func _process(delta: float) -> void:
	if not playing:
		return
	_time += delta
	if _time >= _clip.length + HOLD:
		stop()
		return
	_place(delta)
	_camera.global_transform = _camera.global_transform.interpolate_with(_view(), minf(delta * 10.0, 1.0))
	# The shots fired by now.
	while _shown_shots < _clip.shots.size() and _clip.shots[_shown_shots][0] <= _time:
		var fired: Array = _clip.shots[_shown_shots]
		world.effects.show_shot(fired[2], [fired[3]])
		_shown_shots += 1


# Puts each player of the clip where they were `_time` seconds into it.
func _place(delta: float) -> void:
	var time := minf(_time, _clip.length)
	for id: int in _ghosts:
		var pose := Recording.pose(_clip.tracks[id], time)
		var ghost: Node3D = _ghosts[id]
		ghost.global_position = pose.at
		ghost.rotation.y = pose.yaw
		ghost.set_gaze(pose.pitch)
		if delta > 0.0 and _last.has(id):
			var moved: Vector3 = pose.at - _last[id]
			ghost.set_motion(Vector2(moved.x, moved.z).length() / delta, absf(moved.y) / delta > 0.5, moved.y / delta)
		_last[id] = pose.at


# Where the view sits: behind and above the player the clip is about,
# looking the way they looked.
func _view() -> Transform3D:
	var pose := Recording.pose(_clip.tracks[_about], minf(_time, _clip.length))
	var head: Vector3 = pose.at + Vector3.UP * BodyShape.HEAD_HEIGHT * BodyShape.SIZE
	return Watching.chase(head, pose.yaw, pose.pitch)
