class_name JoiningPeers
extends RefCounted
## For a host: keeps track of players who have started joining but aren't
## connected yet, and says when to give up on one.
##
## A player part-way through joining can vanish without a word, and the
## half-made connection to them would otherwise be kept for good. The session
## server can't be relied on to say so: it reports a player's connection to it
## closing, but every player closes that connection once they are connected
## here, and that report can arrive a moment before this end sees them as
## connected. So a report only shortens the wait; it never drops a player by
## itself.
##
## Kept free of any networking so the rules can be tested on their own; Net
## feeds it and acts on the answers.

## How long a player has to finish joining, in seconds.
const TIMEOUT := 30.0
## How long they have left once the session server says they left it.
const TIMEOUT_AFTER_LEAVING := 5.0

var _time := 0.0
var _deadlines := {}  # peer id -> when to give up on them


func started(id: int) -> void:
	_deadlines[id] = _time + TIMEOUT


## The session server says this player's connection to it has closed.
func left_server(id: int) -> void:
	if _deadlines.has(id):
		_deadlines[id] = minf(_deadlines[id], _time + TIMEOUT_AFTER_LEAVING)


func connected(id: int) -> void:
	_deadlines.erase(id)


## Moves time on, and returns the players to give up on, who are forgotten.
func tick(delta: float) -> Array[int]:
	_time += delta
	var given_up: Array[int] = []
	for id: int in _deadlines:
		if _time >= _deadlines[id]:
			given_up.append(id)
	for id in given_up:
		_deadlines.erase(id)
	return given_up
