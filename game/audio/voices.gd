class_name Voices
## The characters' own voices: short squeaks for effort, pain, a taunt, a
## reload called to teammates, and being knocked out. Each character's is
## pitched a little differently, so they can be told apart by ear.

const EFFORT := "voice_effort"
const PAIN := "voice_pain"
const TAUNT := "voice_taunt"
const RELOAD := "voice_reload"
const OUT := "voice_out"
const CUES := [EFFORT, PAIN, TAUNT, RELOAD, OUT]
## What a character does, and the voice it makes doing it.
const EVENTS := {"jump": EFFORT, "throw": EFFORT, "strike": EFFORT, "hurt": PAIN, "emote": TAUNT, "reload": RELOAD, "knocked_out": OUT}
## Seconds a character waits after one voice before another, so that being
## hit by a burst of fire isn't a string of squeaks. Being knocked out is
## always heard.
const GAP := 0.7
## How loud each is, in decibels, to someone else; a player's own are this
## much quieter.
const LEVELS := {EFFORT: -9.0, PAIN: -3.0, TAUNT: -2.0, RELOAD: -4.0, OUT: -1.0}
const OWN_DB := -7.0


## The voice a character makes when they do `event`, or "" for none.
static func cue(event: String) -> String:
	return EVENTS.get(event, "")


## Whether the voice `sound` of a character is heard by a listener to whom
## they are `who` (one of Footsteps' relations). A reload is called to
## teammates only: an opponent must not be told. A player's own pain and
## knock-out have sounds of their own already.
static func heard(sound: String, who: String) -> bool:
	match sound:
		RELOAD:
			return who == Footsteps.TEAMMATE
		PAIN, OUT:
			return who != Footsteps.OWN
		EFFORT, TAUNT:
			return true
	return false


## How loud the voice `sound` is, in decibels, to a listener to whom the
## character is `who`.
static func level(sound: String, who: String) -> float:
	return LEVELS.get(sound, -6.0) + (OWN_DB if who == Footsteps.OWN else 0.0)


## How high the voice of the character with id `id` is: the same every time
## for one character, and spread between a low squeak and a high one.
static func pitch(id: int) -> float:
	return lerpf(0.82, 1.28, float(absi(id * 2654435761) % 1000) / 999.0)


## Whether a character who last made a voice `since` seconds ago makes
## `sound` now.
static func ready(sound: String, since: float) -> bool:
	return sound == OUT or since >= GAP
