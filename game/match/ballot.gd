class_name Ballot
extends RefCounted
## Something the players are voting on. At the end of a match, what to play
## next: a few arenas and modes to choose between, the last of them always
## the same again. Or, at any time, whether to remove a player.
##
## The host keeps it, counts the votes it is sent and acts on the result;
## everyone is shown it with the match state. Kept free of nodes.

const NEXT := "next"
const REMOVE := "remove"
## Seconds a vote to remove someone runs.
const REMOVE_TIME := 30.0
## How many real players a session needs before one can be voted out, so
## that two can't simply turn out the third on a whim of one.
const MIN_TO_REMOVE := 3
const YES := 0
const NO := 1

var kind := NEXT
## For a vote on what to play next, each choice as {"map", "mode"}.
var choices: Array[Dictionary] = []
## For a vote to remove someone, who; and how many may vote on it.
var subject := 0
var eligible := 0
## Each voter's choice, by their id: which of `choices`, or YES or NO.
var votes := {}
## Seconds left to vote in.
var left := 0.0


## A vote on what to play after a match on `settings`: the two arenas and
## modes after the ones just played, in their usual orders, and the same
## again.
static func for_next(settings: MatchSettings) -> Ballot:
	var ballot := Ballot.new()
	ballot.kind = NEXT
	var maps: Array = Maps.ORDER
	var modes: Array = MatchSettings.MODES
	var map_at := maxi(maps.find(settings.map), 0)
	var mode_at := maxi(modes.find(settings.mode), 0)
	for step in [1, 2]:
		ballot.choices.append({"map": maps[(map_at + step) % maps.size()], "mode": modes[(mode_at + step) % modes.size()]})
	ballot.choices.append({"map": settings.map, "mode": settings.mode})
	ballot.left = MatchState.INTERMISSION
	return ballot


## A vote on removing player `target`, among `voters` who may vote on it.
static func to_remove(target: int, voters: int) -> Ballot:
	var ballot := Ballot.new()
	ballot.kind = REMOVE
	ballot.subject = target
	ballot.eligible = voters
	ballot.left = REMOVE_TIME
	return ballot


func options() -> int:
	return choices.size() if kind == NEXT else 2


## Records `voter`'s choice, which they may change. The player a vote is
## about has no say in it. Returns whether it counted.
func vote(voter: int, option: Variant) -> bool:
	if not option is int or option < 0 or option >= options() or (kind == REMOVE and voter == subject):
		return false
	votes[voter] = option
	return true


## A voter left: their vote goes with them.
func forget(voter: int) -> void:
	votes.erase(voter)


## How many votes each option has.
func counts() -> Array[int]:
	var found: Array[int] = []
	found.resize(options())
	found.fill(0)
	for option: int in votes.values():
		found[option] += 1
	return found


## What to play next: the choice with the most votes, the earliest of those
## level; with no votes at all, the same again.
func winner() -> Dictionary:
	var found := counts()
	var best := choices.size() - 1
	if found.max() > 0:
		best = found.find(found.max())
	return choices[best]


## How many must say yes for a player to be removed: more than half of
## those who may vote.
func needed() -> int:
	return eligible / 2 + 1


## Whether a vote to remove someone has passed, failed, or is still open:
## "yes", "no" or "".
func outcome() -> String:
	var found := counts()
	if found[YES] >= needed():
		return "yes"
	if found[NO] > eligible - needed() or left <= 0.0:
		return "no"
	return ""


## What a choice of what to play next is called.
static func choice_text(choice: Dictionary) -> String:
	return "%s  ·  %s" % [Maps.NAMES.get(choice.get("map"), "?"), MatchSettings.MODE_NAMES.get(choice.get("mode"), "?")]


## What everyone is shown of it, a line at a time. `names` gives the name of
## a player by id.
func lines(names: Callable) -> Array[String]:
	var found := counts()
	var text: Array[String] = []
	if kind == NEXT:
		text.append("Vote for the next match")
		for index in choices.size():
			var said := "The same again" if index == choices.size() - 1 else choice_text(choices[index])
			text.append("F%d   %s   (%d)" % [index + 1, said, found[index]])
	else:
		text.append("Remove %s?   %d needed   ·   %ds" % [names.call(subject), needed(), ceili(left)])
		text.append("F1   Yes   (%d)" % found[YES])
		text.append("F2   No   (%d)" % found[NO])
	return text


func to_array() -> Array:
	var given := []
	for choice: Dictionary in choices:
		given.append([choice.map, choice.mode])
	return [kind, given, subject, eligible, votes, left]


## The ballot `data` describes, or null if it describes none.
static func from_array(data: Variant) -> Ballot:
	if not data is Array or data.size() != 6 or not data[0] in [NEXT, REMOVE] or not data[1] is Array or not data[2] is int or not data[3] is int or not data[4] is Dictionary:
		return null
	var ballot := Ballot.new()
	ballot.kind = data[0]
	for choice: Variant in data[1].slice(0, 4):
		if choice is Array and choice.size() == 2 and (Maps.ORDER.has(choice[0]) or choice[0] == Maps.CUSTOM) and MatchSettings.MODES.has(choice[1]):
			ballot.choices.append({"map": choice[0], "mode": choice[1]})
	if ballot.kind == NEXT and ballot.choices.is_empty():
		return null
	ballot.subject = data[2]
	ballot.eligible = clampi(data[3], 0, MatchSettings.MAX_PLAYERS)
	for voter: Variant in data[4]:
		if voter is int and ballot.votes.size() < MatchSettings.MAX_PLAYERS:
			ballot.vote(voter, data[4][voter])
	ballot.left = clampf(float(data[5]), 0.0, REMOVE_TIME) if (data[5] is float or data[5] is int) and is_finite(float(data[5])) else 0.0
	return ballot
