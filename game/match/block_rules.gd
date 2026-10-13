class_name BlockRules
extends RefCounted
## The cheese blocks get eaten. Each of an arena's blocks of cheese has some
## health; shots and bursts take it away, and every so often through a match
## mice come out and gnaw at one. A block with none left is gone, and so is
## whatever cover it gave, until the next match.
##
## Kept free of nodes so the rules can be tested alone: the host tells it
## what hit which block, and takes away the blocks it says have gone.

## What a block can take: a magazine or two of most weapons, or three visits
## from the mice.
const HEALTH := 400
const GNAW := 150
## Seconds into a match before the mice first come out, and between visits.
const FIRST_GNAW := 30.0
const GNAW_EVERY := 20.0
## How many stages of being eaten a block shows before it is gone.
const STAGES := 3

## Each block's health, in the order the arena lists them.
var health: Array[int] = []
## Seconds until the mice next come out.
var gnaw_in := FIRST_GNAW
var _rng := RandomNumberGenerator.new()


func _init(count := 0, seed := 0) -> void:
	_rng.seed = seed
	reset(count)


## Every one of `count` blocks whole again, as when a match begins.
func reset(count: int) -> void:
	health.clear()
	for index in count:
		health.append(HEALTH)
	gnaw_in = FIRST_GNAW


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


## Moves on by `delta` seconds. Returns the block the mice have come out to
## gnaw at now, for the host to hurt() by GNAW, or -1 for none.
func tick(delta: float) -> int:
	gnaw_in -= delta
	if gnaw_in > 0.0:
		return -1
	gnaw_in = GNAW_EVERY
	var left: Array[int] = []
	for index in health.size():
		if health[index] > 0:
			left.append(index)
	return left[_rng.randi_range(0, left.size() - 1)] if not left.is_empty() else -1


## Which of the blocks with these `boxes` (each an AABB, in the arena's own
## order) stand on block `index`, so that they go when it does: directly, or
## on one that does.
static func resting_on(boxes: Array, index: int) -> Array[int]:
	var found: Array[int] = []
	var under := [index]
	while not under.is_empty():
		var below: AABB = boxes[under.pop_back()]
		for other in boxes.size():
			if other == index or found.has(other):
				continue
			var box: AABB = boxes[other]
			var on_top := absf(box.position.y - below.end.y) < 0.25
			var over := box.position.x < below.end.x - 0.05 and box.end.x > below.position.x + 0.05 and box.position.z < below.end.z - 0.05 and box.end.z > below.position.z + 0.05
			if on_top and over:
				found.append(other)
				under.append(other)
	found.sort()
	return found


func to_array() -> Array:
	return health.duplicate()


## The rules `data` describes, for an arena with `count` blocks if that is
## known (below zero if not), with nonsense made whole blocks.
static func from_array(data: Variant, count := -1) -> BlockRules:
	var rules := BlockRules.new(maxi(count, 0))
	if not data is Array or data.size() > 256:
		return rules
	if count < 0:
		rules.reset(data.size())
	for index in mini(data.size(), rules.health.size()):
		if data[index] is int or data[index] is float:
			rules.health[index] = clampi(int(data[index]), 0, HEALTH) if is_finite(float(data[index])) else HEALTH
	return rules
