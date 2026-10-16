class_name Emotes
## The little performances a player can put on for the others: a wave, a
## dance, a shrug, a cheer. Each lasts a moment and changes nothing about
## the fight: the player can still move and shoot, and firing or being hurt
## cuts it short.

const WAVE := 0
const DANCE := 1
const SHRUG := 2
const CHEER := 3
const KINDS := [WAVE, DANCE, SHRUG, CHEER]
const NAMES := {WAVE: "Wave", DANCE: "Dance", SHRUG: "Shrug", CHEER: "Cheer"}
## Seconds an emote lasts, and the least between two from one player.
const TIME := 1.6
const INTERVAL := 1.0
## How the free arm hangs at rest, in radians out from straight down.
const ARM_REST := -0.22


static func is_kind(kind: Variant) -> bool:
	return kind is int and KINDS.has(kind)


## The emote after `kind` in turn, coming round.
static func next(kind: int) -> int:
	return KINDS[(KINDS.find(kind) + 1) % KINDS.size()]


## How a character `elapsed` seconds into emote `kind` holds itself:
## {"arm": how far the free arm is raised, in radians out from straight down
## (below zero is outward and up), "sway": how far the body tips to one side,
## "hop": how far it lifts off the ground, in its own measurements}.
static func pose(kind: int, elapsed: float) -> Dictionary:
	# In and out gently at each end.
	var held := smoothstep(0.0, 0.2, elapsed) * (1.0 - smoothstep(TIME - 0.25, TIME, elapsed))
	var arm := ARM_REST
	var sway := 0.0
	var hop := 0.0
	match kind:
		WAVE:
			arm = lerpf(ARM_REST, -2.5 + 0.4 * sin(elapsed * 14.0), held)
		DANCE:
			arm = lerpf(ARM_REST, -1.3 + 0.5 * sin(elapsed * 10.0), held)
			sway = 0.16 * sin(elapsed * 10.0) * held
			hop = 0.04 * absf(sin(elapsed * 10.0)) * held
		SHRUG:
			arm = lerpf(ARM_REST, -1.0, held)
			hop = 0.03 * held
		CHEER:
			arm = lerpf(ARM_REST, -2.8, held)
			hop = 0.09 * absf(sin(elapsed * 9.0)) * held
	return {"arm": arm, "sway": sway, "hop": hop}
