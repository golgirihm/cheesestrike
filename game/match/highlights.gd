class_name Highlights
extends RefCounted
## The play of the match: the best moment in it, chosen by a simple score
## as the match goes, and kept as a clip (see Recording) to show everyone at
## the end.
##
## A knock-out scores; one soon after another by the same player scores
## more, a head shot a little more, and one with the knife a little more
## again. The best so far is kept, and an equal one later doesn't replace it.

## Knock-outs by one player no further apart than this are a run.
const RUN_WINDOW := 5.0
const KNOCK_OUT := 100.0
const HEAD := 30.0
const KNIFE := 50.0
const RUN_NAMES := ["", "", "Double knock-out", "Triple knock-out", "Four in a row", "Five in a row"]
## Seconds of a clip of the play of the match.
const CLIP := 4.0

## The best moment so far: {} for none, or {"score", "clip", "attacker",
## "victim", "text": what it was, such as "Triple knock-out"}.
var best := {}
# Player id -> {"count": knock-outs in their run, "at": when the last was}.
var _runs := {}


func clear() -> void:
	best = {}
	_runs.clear()


## `attacker` knocked someone out at `time`. Returns {"score", "text"} for
## the moment.
func knock_out(time: float, attacker: int, head: bool, knife: bool) -> Dictionary:
	var run: Dictionary = _runs.get(attacker, {"count": 0, "at": -INF})
	run.count = run.count + 1 if time - run.at <= RUN_WINDOW else 1
	run.at = time
	_runs[attacker] = run
	var score: float = KNOCK_OUT * run.count + (HEAD if head else 0.0) + (KNIFE if knife else 0.0)
	return {"score": score, "text": describe(run.count, head, knife)}


## What a moment is called.
static func describe(run: int, head: bool, knife: bool) -> String:
	if run >= 2:
		return RUN_NAMES[mini(run, RUN_NAMES.size() - 1)]
	if knife:
		return "Knife knock-out"
	return "Head shot" if head else "Knock-out"


## Offers a moment as the best: kept if it beats the best so far. Returns
## whether it did.
func consider(moment: Dictionary, clip: Dictionary, attacker: int, victim: int) -> bool:
	if clip.is_empty() or moment.score <= best.get("score", 0.0):
		return false
	best = {"score": moment.score, "clip": clip, "attacker": attacker, "victim": victim, "text": moment.text}
	return true
