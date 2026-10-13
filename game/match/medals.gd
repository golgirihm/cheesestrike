class_name Medals
extends RefCounted
## Medals: what a player is called out for as they play. Two knock-outs one
## after another, a run of them without being knocked out, a shot from far
## away, the first after a bad spell, and one on whoever last knocked them
## out. Each machine works out its own player's, from what it is told of
## their hits.

const DOUBLE := "double"
const TRIPLE := "triple"
const STREAK := "streak"
const UNSTOPPABLE := "unstoppable"
const LONG_SHOT := "long_shot"
const HEADSHOT := "headshot"
const COMEBACK := "comeback"
const REVENGE := "revenge"
const ALL := [DOUBLE, TRIPLE, STREAK, UNSTOPPABLE, LONG_SHOT, HEADSHOT, COMEBACK, REVENGE]
const NAMES := {DOUBLE: "Double knock-out", TRIPLE: "Triple knock-out", STREAK: "On a run", UNSTOPPABLE: "Unstoppable", LONG_SHOT: "Long shot",
	HEADSHOT: "Between the eyes", COMEBACK: "Comeback", REVENGE: "Revenge"}
## The icon of each (see Icons).
const ICONS := {DOUBLE: "medal_double", TRIPLE: "medal_triple", STREAK: "medal_streak", UNSTOPPABLE: "medal_streak", LONG_SHOT: "medal_long",
	HEADSHOT: "medal_head", COMEBACK: "medal_comeback", REVENGE: "medal_revenge"}
## Seconds after a knock-out in which the next is one after another.
const QUICK := 4.5
## Knock-outs without being knocked out that make a run, and a long one.
const RUN := 5
const LONG_RUN := 10
## Metres from which a knock-out is a long shot.
const FAR := 40.0
## Times knocked out without a knock-out between that make the next a
## comeback.
const BAD_SPELL := 3

## How many of each have been earned this match.
var earned := {}
var _quick := 0
var _last := -INF
var _run := 0
var _spell := 0
var _nemesis := 0


## The player knocked out `victim` at `clock` seconds, from `distance`
## metres, with a hit to the head or not: the medals that earns, in the
## order they are called out.
func knock_out(victim: int, clock: float, distance: float, head: bool) -> Array[String]:
	var won: Array[String] = []
	_quick = _quick + 1 if clock - _last <= QUICK else 1
	_last = clock
	if _quick == 2:
		won.append(DOUBLE)
	elif _quick >= 3:
		won.append(TRIPLE)
	_run += 1
	if _run == RUN:
		won.append(STREAK)
	elif _run == LONG_RUN:
		won.append(UNSTOPPABLE)
	if distance >= FAR:
		won.append(LONG_SHOT)
	if head:
		won.append(HEADSHOT)
	if _spell >= BAD_SPELL:
		won.append(COMEBACK)
	_spell = 0
	if victim != 0 and victim == _nemesis:
		won.append(REVENGE)
		_nemesis = 0
	for medal in won:
		earned[medal] = earned.get(medal, 0) + 1
	return won


## The player was knocked out by `attacker` (0 for nobody, or themselves).
func knocked_out(attacker: int) -> void:
	_run = 0
	_quick = 0
	_spell += 1
	_nemesis = attacker


## Their best run of knock-outs so far in this one.
func run() -> int:
	return _run


## Forgets everything, as when a match begins.
func clear() -> void:
	earned.clear()
	_quick = 0
	_last = -INF
	_run = 0
	_spell = 0
	_nemesis = 0


## What was earned, as it reads: "Double knock-out ×2, Long shot".
func summary() -> String:
	var parts: Array[String] = []
	for medal: String in ALL:
		if earned.get(medal, 0) > 0:
			parts.append(NAMES[medal] if earned[medal] == 1 else "%s ×%d" % [NAMES[medal], earned[medal]])
	return ", ".join(parts)
