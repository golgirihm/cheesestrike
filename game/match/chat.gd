class_name Chat
## Text chat: what may be said, to whom, and how often. A message is typed
## by anyone, so the host cleans every one before passing it on: cut to a
## length, on one line, and with bad language starred out.

## The most characters in a message.
const MAX_LENGTH := 120
## The most messages a player may send in WINDOW seconds.
const MOST := 4
const WINDOW := 6.0
## Messages that can be sent without typing, as from a wheel on a gamepad
## or a touch screen.
const QUICK := ["Nice shot!", "Thanks", "Sorry", "Help!", "On my way", "Fall back", "Push!", "Good game"]
## Words that are starred out, as written plainly, when they are the whole
## of a word or have one of ENDINGS on them. Letters swapped for look-alikes,
## doubled, or spaced out with dots are found too.
const BLOCKED := ["fuck", "shit", "bitch", "cunt", "asshole", "arsehole", "bastard", "dick", "piss", "slut", "whore", "wank", "twat", "nigger", "nigga",
	"faggot", "fag", "retard", "rape", "kys"]
const ENDINGS := ["", "s", "es", "ed", "er", "ers", "ing", "in", "y"]
## The few that are starred out wherever they turn up inside a word, being
## unlikely to be there by chance.
const ANYWHERE := ["fuck", "faggot", "cunt"]
## What stands for a letter when someone is trying to get a word past.
const LOOKALIKES := {"0": "o", "1": "i", "!": "i", "3": "e", "4": "a", "@": "a", "5": "s", "$": "s", "7": "t", "+": "t", "8": "b", "9": "g", "|": "i"}
## Words that have one of those inside them and are let be.
const INNOCENT := ["scunthorpe"]


## `text` as it may be passed on: on one line, trimmed, no longer than
## allowed, and with bad language starred out. "" if nothing is left.
static func clean(text: Variant) -> String:
	if not text is String:
		return ""
	var line := ""
	for character: String in (text as String).left(MAX_LENGTH * 2):
		# Nothing that isn't printed: no new lines, no control characters.
		line += character if character.unicode_at(0) >= 32 and character.unicode_at(0) != 127 else " "
	line = line.strip_edges().left(MAX_LENGTH)
	var words: Array[String] = []
	for word in line.split(" ", false):
		words.append("*".repeat(word.length()) if is_blocked(word) else word)
	return " ".join(words)


## Whether `word` is, or hides, one that is starred out.
static func is_blocked(word: String) -> bool:
	var plain := _plain(word)
	if plain.is_empty() or INNOCENT.has(plain):
		return false
	for bad: String in ANYWHERE:
		if plain.contains(_plain(bad)):
			return true
	for bad: String in BLOCKED:
		for ending: String in ENDINGS:
			if plain == _plain(bad + ending):
				return true
	return false


## Whether the player with record `viewer` sees a message from the one with
## record `sender`, sent to their team only (`to_team`) or to everyone.
static func seen_by(sender: PlayerRecord, viewer: PlayerRecord, to_team: bool, teams: bool) -> bool:
	if sender == null or viewer == null:
		return false
	if sender == viewer or not to_team or not teams:
		return true
	return sender.team != MatchState.NO_TEAM and sender.team == viewer.team


## Whether a player who sent messages at `times` (by a clock that now reads
## `now`) may send another.
static func may_send(times: Array, now: float) -> bool:
	return times.filter(func(at: float) -> bool: return now - at < WINDOW).size() < MOST


## How a message from `name` reads in the chat, to everyone or to their team.
static func line(name: String, text: String, to_team: bool) -> String:
	return "%s%s: %s" % ["[Team] " if to_team else "", name, text]


# `word` with everything stripped that is used to disguise one: in small
# letters, look-alikes turned back to letters, anything that isn't a letter
# dropped, and runs of the same letter made one.
static func _plain(word: String) -> String:
	var plain := ""
	for character: String in word.to_lower():
		var letter: String = LOOKALIKES.get(character, character)
		if letter >= "a" and letter <= "z" and not plain.ends_with(letter):
			plain += letter
	return plain
