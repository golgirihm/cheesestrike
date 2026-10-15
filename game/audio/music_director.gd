class_name MusicDirector
extends Node
## Music that follows the match: a theme on the menu, a quieter piece under a
## match, one that drives when the match is nearly decided (its last minute,
## or one side a score or two from winning), and a short
## sting at the end for whoever won and whoever lost.

const MENU := "music"
const MATCH := "music_match"
const TENSE := "music_tense"
const PIECES := [MENU, MATCH, TENSE]
const WIN := "sting_win"
const LOSE := "sting_lose"
## Seconds from time at which the music begins to drive.
const LAST := 60.0
## How near the limit the leader must be for it to be match point: within
## this share of it, and always within one.
const NEAR := 0.1

var world: Node3D
var _in := 0.0
var _ended := false


func _init(of: Node3D = null) -> void:
	world = of
	name = "MusicDirector"


## Which piece plays: on the menu or `in_session`, and there by how the match
## stands for this player (see Announcer.standing) and whether it is at
## `match_point`.
static func piece(in_session: bool, standing: Dictionary, match_point: bool) -> String:
	if not in_session or standing.is_empty():
		return MENU
	if standing.playing and (standing.time_left <= LAST or match_point):
		return TENSE
	return MATCH


## The sting for the end of a match that this player `won` (true), lost
## (false), or neither (null): "" for none.
static func sting(won: Variant) -> String:
	if won == null:
		return ""
	return WIN if won else LOSE


## Whether the match in `state` is at match point: whoever leads is within a
## score, or a tenth of the limit, of winning.
static func match_point(state: MatchState) -> bool:
	if not state.is_live():
		return false
	var limit := state.settings.limit()
	var best := 0
	if state.settings.by_points():
		best = state.team_points.max()
	elif state.settings.is_teams():
		best = maxi(state.team_score(MatchState.CHEDDAR), state.team_score(MatchState.BRIE))
	else:
		for record: PlayerRecord in state.players.values():
			best = maxi(best, record.kills)
	return limit > 1 and best >= limit - maxi(1, floori(limit * NEAR))


func _process(delta: float) -> void:
	_in -= delta
	if _in > 0.0 or world == null:
		return
	_in = 0.25
	if not world.match_.has_state() or multiplayer.multiplayer_peer == null:
		_ended = false
		Sounds.set_piece(piece(false, {}, false))
		return
	var standing := Announcer.standing(world.match_.state, multiplayer.get_unique_id())
	Sounds.set_piece(piece(Sounds.in_session, standing, match_point(world.match_.state)))
	# The sting, once, as the match ends.
	if standing.ended and not _ended:
		Sounds.play_sting(sting(standing.won))
	_ended = standing.ended
