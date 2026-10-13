class_name Fixtures
## Arena pieces that move a player in ways walking and jumping don't: jump
## pads, which throw whoever steps on them along a set arc, and ladders,
## which can be climbed.
##
## An arena lists its own: Marker3Ds under "JumpPads", each with the
## velocity it launches at as its `launch` metadata, and under "Ladders",
## each at the foot of a ladder, facing the way a climber faces (its -Z),
## with how tall it is as its `height`. Scenery draws them. Players move
## themselves, so each applies these to itself, and the host's check on
## movement (see Match.on_moved) allows for both and for nothing more.

## How close to a pad's middle a player's feet must be to be launched.
const PAD_RADIUS := 1.5
## How far from a ladder's line a player can be and still hold on to it, and
## how fast they go up or down it, in metres a second.
const LADDER_REACH := 1.5
const LADDER_SPEED := 5.0


## What an arena has, read from its scene: {"pads": [{position, launch}],
## "ladders": [{position, height, facing}]}.
static func of(map: Node) -> Dictionary:
	var found := {"pads": [], "ladders": []}
	var pads := map.get_node_or_null("JumpPads")
	if pads != null:
		for pad: Node3D in pads.get_children():
			found.pads.append({"position": pad.global_position if pad.is_inside_tree() else pad.position, "launch": pad.get_meta("launch", Vector3.UP * 10.0)})
	var ladders := map.get_node_or_null("Ladders")
	if ladders != null:
		for ladder: Node3D in ladders.get_children():
			found.ladders.append({
				"position": ladder.global_position if ladder.is_inside_tree() else ladder.position,
				"height": float(ladder.get_meta("height", 4.0)),
				"facing": -ladder.basis.z,
			})
	return found


## The velocity the pad under a player whose feet are at `feet` launches
## them at, or null if they aren't standing on one of `pads`.
static func launch_at(pads: Array, feet: Vector3) -> Variant:
	for pad: Dictionary in pads:
		var across := Vector2(feet.x - pad.position.x, feet.z - pad.position.z).length()
		if across <= PAD_RADIUS and absf(feet.y - pad.position.y) <= 0.6:
			return pad.launch
	return null


## The one of `ladders` a player whose feet are at `feet` can hold on to, or
## an empty dictionary: within reach of its line, from its foot to just
## above its top.
static func ladder_at(ladders: Array, feet: Vector3) -> Dictionary:
	for ladder: Dictionary in ladders:
		var across := Vector2(feet.x - ladder.position.x, feet.z - ladder.position.z).length()
		var up: float = feet.y - ladder.position.y
		if across <= LADDER_REACH and up >= -0.3 and up <= ladder.height + 0.2:
			return ladder
	return {}


## Where a player launched from `from` at `launch` is after `time` seconds
## in the air, with gravity `gravity`.
static func flight(from: Vector3, launch: Vector3, gravity: float, time: float) -> Vector3:
	return from + launch * time + Vector3.DOWN * gravity * 0.5 * time * time
