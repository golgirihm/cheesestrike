class_name AimCheck
extends RefCounted
## The host's check on where one player's shots go. A player's game tells
## the host which way they face as they move, and which way each shot went.
## A modified game can fire at someone behind it without turning: its shots
## go one way while it faces another.
##
## One shot far from where the player was last seen facing proves nothing:
## a quick turn and a shot can reach the host before the turn does. So a
## single wild shot is let through. A player most of whose shots are wild is
## doing something no hand does, and their wild shots are then refused.

## How far a shot can be from where the player faced, in radians, before it
## is wild.
const WILD := deg_to_rad(95.0)
## How many of a player's last shots are remembered, and the share of them
## that must be wild for wild ones to be refused.
const REMEMBERED := 16
const TOO_MANY := 0.4
## How many shots must be remembered before anything is refused.
const AT_LEAST := 6

# Whether each of the last shots was wild, oldest first.
var _wild: Array[bool] = []


## The way someone facing `facing` (yaw, pitch, as a player's are sent)
## looks.
static func forward(facing: Vector2) -> Vector3:
	return Vector3(-sin(facing.x) * cos(facing.y), sin(facing.y), -cos(facing.x) * cos(facing.y))


## How far `direction` is from the nearest of the ways the player `faced`
## lately (each a yaw and pitch), in radians: 0 with none to go by.
static func off_by(direction: Vector3, faced: Array) -> float:
	if faced.is_empty() or direction.length() < 0.001:
		return 0.0
	var nearest := PI
	for facing: Vector2 in faced:
		nearest = minf(nearest, direction.normalized().angle_to(forward(facing)))
	return nearest


## A shot went `off` radians from where the player faced (see off_by).
## Returns whether it is allowed.
func allow(off: float) -> bool:
	var wild := off > WILD
	_wild.append(wild)
	while _wild.size() > REMEMBERED:
		_wild.pop_front()
	if not wild or _wild.size() < AT_LEAST:
		return true
	return share() < TOO_MANY


## The share of the shots remembered that were wild.
func share() -> float:
	return float(_wild.count(true)) / _wild.size() if not _wild.is_empty() else 0.0
