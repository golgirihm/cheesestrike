class_name SessionList
## The sessions the session server lists, as the menu shows them: sorted and
## filtered as the player asks, and the one "Play now" joins.
##
## Each session is as the server sends it: {"code", "players", "settings"}.

const FULLEST := "fullest"
const EMPTIEST := "emptiest"
const BY_ARENA := "arena"
const SORTS := [FULLEST, EMPTIEST, BY_ARENA]
const SORT_NAMES := {FULLEST: "Fullest first", EMPTIEST: "Emptiest first", BY_ARENA: "By arena"}
## Any mode, or any arena.
const ANY := ""


## How many are playing in `session`, and how many it has room for.
static func players(session: Dictionary) -> int:
	var count: Variant = session.get("players", 0)
	return int(count) if count is int or count is float else 0


static func room(session: Dictionary) -> int:
	return MatchSettings.from_dict(session.get("settings")).max_players - players(session)


## `list` with only the sessions in `mode` on `arena` (either ANY for all),
## and none that are full if `hide_full`, in the order `sort` asks for.
static func arranged(list: Array, sort := FULLEST, mode := ANY, arena := ANY, hide_full := false) -> Array:
	var kept := list.filter(func(session: Variant) -> bool:
		if not session is Dictionary:
			return false
		var settings := MatchSettings.from_dict(session.get("settings"))
		return (mode == ANY or settings.mode == mode) and (arena == ANY or settings.map == arena) \
			and (not hide_full or room(session) > 0))
	kept.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		if sort == BY_ARENA:
			var a_name: String = Maps.NAMES[MatchSettings.from_dict(a.get("settings")).map]
			var b_name: String = Maps.NAMES[MatchSettings.from_dict(b.get("settings")).map]
			if a_name != b_name:
				return a_name < b_name
		elif players(a) != players(b):
			return players(a) > players(b) if sort != EMPTIEST else players(a) < players(b)
		return String(a.get("code", "")) < String(b.get("code", "")))
	return kept


## The session "Play now" joins from `list`, or {} for none, when one should
## be hosted instead: one with room and the most people already playing,
## since a match is better with more in it; in `mode` if one is given and
## one of those has room.
static func play_now(list: Array, mode := ANY) -> Dictionary:
	var open := arranged(list, FULLEST, ANY, ANY, true)
	if mode != ANY:
		var same := open.filter(func(session: Dictionary) -> bool: return MatchSettings.from_dict(session.get("settings")).mode == mode)
		if not same.is_empty():
			return same[0]
	return open[0] if not open.is_empty() else {}


## The next of `choices` after `current`, round to the first.
static func next_of(choices: Array, current: Variant) -> Variant:
	return choices[(choices.find(current) + 1) % choices.size()]
