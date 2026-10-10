class_name SpawnSlots
extends RefCounted
## Hands each player their own starting spot, by index, out of a fixed number of
## spots. The host owns one of these, so two players are never given the same
## spot while a free one remains.

var _count: int
var _slot_by_player := {}


func _init(count: int) -> void:
	_count = count


## The slot for `player`: the one they already hold, or else the lowest free
## one. If every slot is taken, the least crowded.
func assign(player: int) -> int:
	if _slot_by_player.has(player):
		return _slot_by_player[player]
	var occupants := []
	occupants.resize(_count)
	occupants.fill(0)
	for taken: int in _slot_by_player.values():
		occupants[taken] += 1
	var slot := occupants.find(occupants.min())
	_slot_by_player[player] = slot
	return slot


func release(player: int) -> void:
	_slot_by_player.erase(player)
