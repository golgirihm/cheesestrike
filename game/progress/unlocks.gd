class_name Unlocks
## Things to earn: cheeses, hats, things to hold, titles, weapon finishes
## and emotes, each unlocked by a level, an achievement or a challenge.
##
## Every one of them is for show. None changes what can be hit, how a
## character moves, or what a weapon does: the kinds there are below are all
## the kinds there can be, and a test holds them to that.
##
## Which of them a player has is their own machine's word, as everything
## they have earned is (see Progress). Wearing one that wasn't earned takes
## nothing from anyone.

const CHEESE := "cheese"
const HAT := "hat"
const HELD := "held"
const TITLE := "title"
const FINISH := "finish"
const EMOTE := "emote"
## The kinds of thing there are to earn, with what each is called.
const KINDS := {CHEESE: "Cheese", HAT: "Hat", HELD: "To hold", TITLE: "Title", FINISH: "Weapon finish", EMOTE: "Emote"}
## The finishes a weapon can have, with what each is called and its colour.
const FINISHES := ["plain", "wax", "blue_vein", "gold"]
const FINISH_NAMES := {"plain": "Plain", "wax": "Red wax", "blue_vein": "Blue vein", "gold": "Golden"}
const FINISH_COLOURS := {"wax": Color(0.78, 0.13, 0.1), "blue_vein": Color(0.25, 0.42, 0.8), "gold": Color(1.0, 0.78, 0.2)}
const FINISH_SETTING := "look_finish"
## What each thing takes: a level, an achievement (see Achievements) or a
## challenge (see Challenges). What isn't here is everyone's from the start.
const NEEDS := {
	"cheese:blue": {"level": 4}, "cheese:brie": {"level": 8},
	"hat:chef": {"level": 6}, "hat:cowboy": {"level": 12}, "hat:crown": {"level": 20},
	"held:flag": {"level": 5}, "held:lollipop": {"level": 10},
	"title:big_cheese": {"level": 10}, "title:old_hand": {"level": 30}, "title:grand_fromage": {"level": 60},
	"title:mouser": {"achievement": "century"}, "title:sharpshooter": {"achievement": "marksman"}, "title:wheel_roller": {"challenge": "long:kills_with:launcher"},
	"finish:wax": {"level": 3}, "finish:blue_vein": {"level": 9}, "finish:gold": {"level": 25},
	"emote:2": {"level": 2}, "emote:3": {"level": 7},
}


## The name a thing of `kind` called `id` goes by in NEEDS.
static func key(kind: String, id: Variant) -> String:
	return "%s:%s" % [kind, id]


## Whether a player at `level`, with `achievements` earned and `challenges`
## done (lists of their ids), has the thing of `kind` called `id`.
static func has(kind: String, id: Variant, level: int, achievements: Array, challenges: Array) -> bool:
	var need: Dictionary = NEEDS.get(key(kind, id), {})
	if need.has("level"):
		return level >= need.level
	if need.has("achievement"):
		return achievements.has(need.achievement)
	if need.has("challenge"):
		return challenges.has(need.challenge)
	return true


## Whether this player has it, by what is kept of them.
static func mine(kind: String, id: Variant) -> bool:
	return has(kind, id, Progress.level(), Progress.achievements, Progress.challenges_done())


## What a thing takes, as it reads: "Level 6", or the name of what must be
## done. "" for one that is everyone's.
static func needs_text(kind: String, id: Variant) -> String:
	var need: Dictionary = NEEDS.get(key(kind, id), {})
	if need.has("level"):
		return "Level %d" % need.level
	if need.has("achievement"):
		return "Achievement: %s" % Achievements.name_of(need.achievement)
	if need.has("challenge"):
		return "Challenge: %s" % Challenges.text_of(need.challenge)
	return ""


## Every choice of `kind` there is, in order.
static func choices(kind: String) -> Array:
	match kind:
		CHEESE:
			return Looks.CHEESES
		HAT:
			return Looks.HATS
		HELD:
			return Looks.HELD
		TITLE:
			return Looks.TITLES
		FINISH:
			return FINISHES
		EMOTE:
			return Emotes.KINDS
	return []


## What a choice of `kind` called `id` is called.
static func name_of(kind: String, id: Variant) -> String:
	match kind:
		CHEESE:
			return Looks.CHEESE_NAMES.get(id, "")
		HAT:
			return Looks.HAT_NAMES.get(id, "")
		HELD:
			return Looks.HELD_NAMES.get(id, "")
		TITLE:
			return Looks.TITLE_NAMES.get(id, "")
		FINISH:
			return FINISH_NAMES.get(id, "")
		EMOTE:
			return Emotes.NAMES.get(id, "")
	return ""


## The choice after `current` among those of `kind` this player has, coming
## round: so that a button steps only through what has been earned.
static func next(kind: String, current: Variant, level: int, achievements: Array, challenges: Array) -> Variant:
	var all := choices(kind)
	var from := maxi(all.find(current), 0)
	for step in range(1, all.size() + 1):
		var candidate: Variant = all[(from + step) % all.size()]
		if has(kind, candidate, level, achievements, challenges):
			return candidate
	return all[0] if not all.is_empty() else current


## What a player has that they hadn't: the things that take more than
## `level_before` but no more than `level_after`, and those that take one of
## `new_achievements` or `new_challenges`. Each as [kind, id].
static func gained(level_before: int, level_after: int, new_achievements: Array, new_challenges: Array) -> Array:
	var things := []
	for named: String in NEEDS:
		var need: Dictionary = NEEDS[named]
		var is_new: bool = (need.has("level") and need.level > level_before and need.level <= level_after) \
			or (need.has("achievement") and new_achievements.has(need.achievement)) \
			or (need.has("challenge") and new_challenges.has(need.challenge))
		if is_new:
			var kind := named.get_slice(":", 0)
			var id: Variant = named.get_slice(":", 1)
			things.append([kind, int(id) if kind == EMOTE else id])
	return things


## How something gained reads: "Hat: Crown".
static func gained_text(thing: Array) -> String:
	return "%s: %s" % [KINDS[thing[0]], name_of(thing[0], thing[1])]


## Everything there is to earn, as it reads on its page: each with what it
## takes, or that the player has it.
static func lines(level: int, achievements: Array, challenges: Array) -> Array[String]:
	var said: Array[String] = []
	for named: String in NEEDS:
		var kind := named.get_slice(":", 0)
		var raw := named.get_slice(":", 1)
		var id: Variant = int(raw) if kind == EMOTE else raw
		var got := has(kind, id, level, achievements, challenges)
		said.append("%s %s: %s   ·   %s" % ["[x]" if got else "[ ]", KINDS[kind], name_of(kind, id), "yours" if got else needs_text(kind, id)])
	return said


## Gives every part of the weapon `model` the finish called `finish`: a
## wash of its colour over the weapon's own. It is paint, and changes
## nothing a weapon does.
static func paint(model: Node, finish: String) -> void:
	if not FINISH_COLOURS.has(finish):
		return
	for part in model.find_children("*", "MeshInstance3D", true, false):
		var mesh := part as MeshInstance3D
		var material := mesh.material_override as StandardMaterial3D
		if material == null:
			continue
		var painted := material.duplicate() as StandardMaterial3D
		painted.albedo_color = material.albedo_color.lerp(FINISH_COLOURS[finish], 0.6)
		painted.metallic = 0.5 if finish == "gold" else painted.metallic
		mesh.material_override = painted
