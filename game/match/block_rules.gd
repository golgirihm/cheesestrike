class_name BlockRules
extends RefCounted
## The cheese blocks get eaten. Each of an arena's blocks of cheese has some
## health; shots, bursts and the giant mice (see MouseRules) take it away. A
## block with none left is gone, and so is whatever cover it gave, until the
## next match.
##
## Kept free of nodes so the rules can be tested alone: the host tells it
## what hit which block, and takes away the blocks it says have gone.

## What a block can take: a magazine or two of most weapons, or a few
## seconds of a mouse.
const HEALTH := 400
## How many stages of being eaten a block shows before it is gone.
const STAGES := 3
## The most blocks an arena may have: a large map's houses are built of
## hundreds.
const MOST := 2000

## Each block's health, in the order the arena lists them.
var health: Array[int] = []


func _init(count := 0) -> void:
	reset(count)


## Every one of `count` blocks whole again, as when a match begins.
func reset(count: int) -> void:
	health.clear()
	for index in count:
		health.append(HEALTH)


func standing(index: int) -> bool:
	return index >= 0 and index < health.size() and health[index] > 0


## How far gone block `index` looks: 0 whole, up to STAGES, then gone.
func stage(index: int) -> int:
	if not standing(index):
		return STAGES + 1
	return mini(int((1.0 - float(health[index]) / HEALTH) * (STAGES + 1)), STAGES)


## Takes `amount` from block `index`. Returns whether that finished it.
func hurt(index: int, amount: float) -> bool:
	if not standing(index) or amount <= 0.0 or not is_finite(amount):
		return false
	health[index] = maxi(health[index] - maxi(roundi(amount), 1), 0)
	return health[index] == 0


## Block `index` goes at once, as one does whose support has gone. Returns
## whether it was there to go.
func remove(index: int) -> bool:
	if not standing(index):
		return false
	health[index] = 0
	return true


## Which of the blocks with these `boxes` (each an AABB, in the arena's own
## order) stand on block `index`, so that they go when it does: directly, or
## on one that does.
static func resting_on(boxes: Array, index: int) -> Array[int]:
	return above(supports(boxes), index)


## For each of the blocks with these `boxes`, the blocks standing directly
## on it: worked out once for an arena, as a large map has hundreds.
static func supports(boxes: Array) -> Array[PackedInt32Array]:
	var found: Array[PackedInt32Array] = []
	found.resize(boxes.size())
	# Blocks by the height their bottoms are at, to the nearest half metre,
	# so that each looks only at those that could be standing on it.
	var by_bottom := {}
	for index in boxes.size():
		found[index] = PackedInt32Array()
		var key := roundi((boxes[index] as AABB).position.y * 2.0)
		if not by_bottom.has(key):
			by_bottom[key] = PackedInt32Array()
		by_bottom[key].append(index)
	for index in boxes.size():
		var below: AABB = boxes[index]
		var key := roundi(below.end.y * 2.0)
		for near in [key - 1, key, key + 1]:
			for other: int in by_bottom.get(near, PackedInt32Array()):
				var box: AABB = boxes[other]
				var on_top := absf(box.position.y - below.end.y) < 0.25
				var over := box.position.x < below.end.x - 0.05 and box.end.x > below.position.x + 0.05 and box.position.z < below.end.z - 0.05 and box.end.z > below.position.z + 0.05
				if other != index and on_top and over:
					found[index].append(other)
	return found


## Which blocks go when block `index` does, as `supports` has them: those on
## it, those on those, and so on.
static func above(supported: Array[PackedInt32Array], index: int) -> Array[int]:
	var found: Array[int] = []
	if index < 0 or index >= supported.size():
		return found
	var under := [index]
	while not under.is_empty():
		for other: int in supported[under.pop_back()]:
			if other != index and not found.has(other):
				found.append(other)
				under.append(other)
	found.sort()
	return found


## What is sent to everyone: how much of each block is left, two bytes a
## block, so that a large map's hundreds go in a packet or two.
func to_array() -> PackedByteArray:
	var sent := PackedByteArray()
	sent.resize(health.size() * 2)
	for index in health.size():
		sent.encode_u16(index * 2, health[index])
	return sent


## The rules `data` describes, for an arena with `count` blocks if that is
## known (below zero if not), with nonsense made whole blocks.
static func from_array(data: Variant, count := -1) -> BlockRules:
	var rules := BlockRules.new(maxi(count, 0))
	if data is PackedByteArray:
		if data.size() > MOST * 2 or data.size() % 2 != 0:
			return rules
		if count < 0:
			rules.reset(data.size() / 2)
		for index in mini(data.size() / 2, rules.health.size()):
			rules.health[index] = mini(data.decode_u16(index * 2), HEALTH)
		return rules
	if not data is Array or data.size() > MOST:
		return rules
	if count < 0:
		rules.reset(data.size())
	for index in mini(data.size(), rules.health.size()):
		if data[index] is int or data[index] is float:
			rules.health[index] = clampi(int(data[index]), 0, HEALTH) if is_finite(float(data[index])) else HEALTH
	return rules
