extends Camera3D
## The view of someone who is watching rather than playing (see Watching):
## behind whichever player they follow, or flying free. The world adds this
## as a child. It takes over from this machine's own player's view while
## they are out of play, and gives it back when they return.

var world: Node3D
## Who is being followed (0 for nobody), and whether the view is flying free.
var target := 0
var free := false
var _was_out := false


func _init(of: Node3D) -> void:
	world = of
	name = "WatchCamera"
	fov = 75.0


## Steps to the next (or with -1, the previous) player to follow.
func step(by: int) -> void:
	var state: MatchState = world.match_.state
	free = false
	target = Watching.next(Watching.choices(state, multiplayer.get_unique_id()), target, by)


## Turns flying free on or off: only for someone who chose to watch.
func toggle_free() -> void:
	var mine: PlayerRecord = world.match_.own_record()
	if mine != null and mine.watching:
		free = not free


## What the watcher is told, or "" for someone who is playing.
func text(touch := false) -> String:
	if not world.match_.has_state() or multiplayer.multiplayer_peer == null:
		return ""
	return Watching.text(world.match_.state, multiplayer.get_unique_id(), target, free, touch)


func _process(delta: float) -> void:
	if multiplayer.multiplayer_peer == null or not world.match_.has_state():
		return
	var id := multiplayer.get_unique_id()
	var state: MatchState = world.match_.state
	var mine := state.player(id)
	var own: Node3D = world.player_node(id)
	var out := mine != null and own != null and not mine.alive
	if not out:
		if _was_out and own != null:
			own.camera.make_current()
		_was_out = false
		free = false
		return
	if not _was_out:
		# Just out of play: start on whoever there is most reason to watch.
		_was_out = true
		target = Watching.first_for(state, id)
		global_transform = own.camera.global_transform
	var among := Watching.choices(state, id)
	if not among.has(target):
		target = Watching.next(among, target)
	if free and mine.watching:
		# The way the watcher looks, which their own controls still turn.
		global_basis = Basis.from_euler(Vector3(own.head.rotation.x, own.rotation.y, 0.0))
		var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
		var rise := Input.get_axis("crouch", "sprint")
		global_position += (global_basis * Vector3(input.x, 0.0, input.y) + Vector3.UP * rise) * Watching.FLY_SPEED * delta
		make_current()
		return
	var followed: Node3D = world.player_node(target) if target != 0 else null
	if followed == null:
		# Nobody to follow: stay where the view was.
		if mine.watching:
			make_current()
		return
	var wanted := Watching.chase(followed.head.global_position, followed.rotation.y, followed.head.rotation.x)
	# Pulled in front of anything between it and the player it follows.
	var from: Vector3 = followed.head.global_position
	var hit := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, wanted.origin, 1))
	if not hit.is_empty():
		wanted.origin = from.lerp(hit.position, 0.9)
	global_transform = global_transform.interpolate_with(wanted, minf(delta * 12.0, 1.0)) if current else wanted
	make_current()
