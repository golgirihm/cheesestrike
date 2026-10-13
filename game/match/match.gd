class_name Match
extends Node
## The host's side of the rules, and everyone's copy of what it decided.
##
## The host keeps the MatchState and sends all of it to every player whenever it
## changes, at most SYNC_INTERVAL apart. Players ask the host for things (a new
## name, a shot) by RPC, and the host decides.
##
## A shot is fired on the shooter's machine, which shows it at once and tells
## the host where it started and which way it went. The host checks it could
## have been fired (ShotRules), works out what it hit against where it sees the
## players (HitScan), and applies the damage.

signal state_changed
## This player's shot hit `victim` (on this machine only).
signal hit_confirmed(victim: int, dealt: int, knocked_out: bool, head: bool)
## This player was hurt by `attacker`, who fired from `from` (on this machine only).
signal hurt(attacker: int, from: Vector3, dealt: int)
## Another player fired a shot whose pellets ended at `ends`, for effects.
signal shot_shown(shooter: int, ends: Array)
## Player `id` threw a grenade, on everyone's machine, for their arm.
signal throw_shown(id: int)
## Player `id` made emote `kind` (see Emotes), on everyone's machine.
signal emote_shown(id: int, kind: int)
## Another player struck with their knife, for effects.
signal strike_shown(striker: int)
## Player `victim` is knocked out by `weapon`, from `from` (on everyone's
## machine, just before the state that says so), for how they go.
signal fell(victim: int, weapon: String, from: Vector3)
## Someone knocked someone out, for the kill feed. Names are as they were then.
signal knocked_out(attacker: String, victim: String, weapon: String, head: bool)
## One of the host's once-a-second messages arrived, sent when its clock
## read `sent`: for telling how many go astray (see Readings).
signal heard_from_host(sent: float)
## What each player has done in the match (see Stats) has been heard from
## the host.
signal stats_changed
## Player `by` pinged `at`, for this machine's player to see (see Pings).
signal pinged(by: int, at: Vector3, kind: String)
## Player `by` said `text` in the chat, to their team alone or to everyone,
## for this machine's player to read (see Chat).
signal chatted(by: int, text: String, to_team: bool)
## A packet of player `speaker`'s talk, for this machine's player to hear
## (see VoiceChat).
signal voice_heard(speaker: int, packet: PackedByteArray)
## Player `id` set off their Overload (on everyone's machine).
signal overloaded(id: int)
## Player `id` collected a pickup of `kind` (on everyone's machine).
signal picked_up(id: int, kind: String)
## A clip of the match to show again (see Recording), seen from behind
## player `about` and headed `title`: on a knocked-out player's machine, how
## it happened; on everyone's at the end, the play of the match.
signal replay_shown(clip: Dictionary, about: int, title: String)
## The mice have come out to gnaw at cheese block `index` (on everyone's
## machine).
signal block_gnawed(index: int)
## Something everyone is told, across the screen: `text`, in the colour of
## `team` (MatchState.NO_TEAM for nobody's).
signal announced(text: String, team: int)
## Player `id` took power-up `power` (see Powers), which everyone is told.
signal power_taken(id: int, power: int)
## A projectile was launched, or burst, for effects.
signal projectile_launched(id: int, weapon: String, from: Vector3, velocity: Vector3)
signal projectile_burst(id: int, weapon: String, at: Vector3)

const SYNC_INTERVAL := 0.1
## The share of a magazine, for each weapon carried, that knocking a player
## out gives.
const SPOILS := 0.5
## The host sends the state at least this often, even unchanged, so that
## everyone's clocks stay in step with its own.
const RESYNC_INTERVAL := 2.0
## Physics layer of the arena's walls, floor and cover, which stop shots.
const WORLD_MASK := 1
## Height above a player's feet at which the host looks for walls in the way
## of a move: low enough that every block in the arena counts.
const MOVE_CHECK_HEIGHT := 0.9
## What players are told when the host ends the session on purpose.
const HOST_ENDED := "The host ended the session."
## The most a shot is checked back in time. Beyond it, a shooter on a slow
## connection has to lead their target.
const MAX_REWIND := 0.3
## Seconds between the host's pings to each player.
const PING_INTERVAL := 1.0
## A ping that changes by less than this many milliseconds isn't news.
const PING_CHANGE := 5

## Everyone's copy of the host's decisions. Replaced, not changed, on players
## other than the host, so hold on to it only briefly.
var state := MatchState.new()

@onready var world := get_parent() as Node3D

# Seconds since this node started, as the host's clock for ShotRules.
var _clock := 0.0
# Host only: when each player's last packets of talk came, by player.
var _talk_times := {}
## What each player has done in the match beyond their score: kept by the
## host, and sent to everyone every so often.
var stats := Stats.new()
# Host only: when each player last pinged, and when they lately chatted.
var _point_times := {}
var _chat_times := {}
# The host's ShotRules for each player, by id.
var _rules := {}
# Where each player has lately been, as the host saw them, by id.
var _histories := {}
# The host's check on each player's movement, by id, and when it last put
# each back.
var _checks := {}
var _put_back_at := {}
## How many times the host has put a player back, for the browser tests.
var put_backs := 0
## How many shots the host has refused for going where their shooter wasn't
## facing, for tests.
var wild_shots := 0
# Each player's aim, as the host checks it.
var _aims := {}
## How far back the host looks for which way a player faced when they fired.
const AIM_WINDOW := 0.5
# The host's measure of each player's round trip, by id.
var _pings := {}
var _since_ping := 0.0
## Host only: what each weapon has done this match (see WeaponTally).
var tally := WeaponTally.new()
# When the host last passed on an emote from each player, by id.
var _emoted_at := {}
# When the host last allowed each player a strike with the knife, by id.
var _struck_at := {}
# The host's projectiles in flight, by id.
var _projectiles := {}
var _next_projectile := 1
# Names asked for by players the host hasn't seen join yet, by player id.
var _pending_names := {}
# And the looks they chose, likewise.
var _pending_looks := {}
# And their loadouts, as [main, side].
var _pending_loadouts := {}
# The arena's hazards and doors, as the host runs them.
var _moving := MovingParts.new()
# Where players have lately been knocked out: each {"where", "at": when on
# the host's clock, "victim"}. A returning player is kept away from them.
var _fights: Array[Dictionary] = []
## Seconds a knock-out counts as recent fighting.
const FIGHT_MEMORY := 10.0
# The keys (see introduce) of players removed by a vote, who are not let
# back into the session.
var _removed_keys := {}
const REMOVED := "You were removed from the session by a vote."
## The last few seconds of the match, and the best moment of it so far:
## the host's, for showing players how they were knocked out and the play
## of the match.
var _was_playing := true
var recording := Recording.new()
var highlights := Highlights.new()
## Seconds of a clip of how a player was knocked out: no longer than they
## wait to come back.
const KNOCK_OUT_CLIP := 2.6
const KNOCK_OUT_TITLE := "How you were knocked out"
# And the kind of Overload each has chosen.
var _pending_overloads := {}
# Rains of wheels under way: each {"shooter": id, "at": the feet they fall
# round, "next": which wheel is next, "wait": seconds until it falls}.
var _rains: Array[Dictionary] = []
# Each player's key (see WebPage.player_key), by id, including keys sent
# before the host saw the player join; and the records of players who left,
# by key, kept for a while in case they come back.
var _keys := {}
var _departed := {}
var _dirty := false
var _since_sync := 0.0
# Whether this machine has the host's decisions yet: at once on the host, and
# with the first state that arrives elsewhere.
var _has_state := false


## Begins the session's matches; on the host, with `settings`.
func start(settings := MatchSettings.new()) -> void:
	# A player other than the host keeps whatever the host has already sent.
	if is_host():
		state = MatchState.new(settings)
		_has_state = true
		state.set_pickup_count(world.pickup_pads().size() if world != null else 0)
		state.blocks = BlockRules.new(world.cheese_blocks().size() if world != null else 0, randi())
		_hold_back_powers()
		_pending_names.clear()
		_pending_looks.clear()
		_pending_loadouts.clear()
		_pending_overloads.clear()
		_removed_keys.clear()
		_rains.clear()
		_keys.clear()
		_departed.clear()
		_rules.clear()
		_projectiles.clear()
		tally.clear()
		_histories.clear()
		_checks.clear()
		_put_back_at.clear()
		_aims.clear()
		recording.clear()
		highlights.clear()
		state_changed.emit()
	else:
		introduce(Settings.get_value("name"))
		wear(Looks.chosen())
		var loadout := Weapons.chosen_loadout()
		choose_loadout(loadout[0], loadout[1], loadout[2])
		choose_overload(Overload.from_id(Settings.get_value("loadout_overload")))


func stop() -> void:
	stats = Stats.new()
	state = MatchState.new()
	_has_state = false
	_dirty = false
	state_changed.emit()


## Whether the host's decisions have reached this machine yet.
func has_state() -> bool:
	return _has_state or is_host()


func is_host() -> bool:
	return multiplayer.multiplayer_peer != null and multiplayer.is_server()


## Seconds the host keeps a departed player's score, for if they come back.
const REJOIN_WINDOW := 120.0
const MAX_KEY_LENGTH := 64


## Host only: a player has joined the session.
func add_player(id: int) -> void:
	state.add_player(id)
	if id == multiplayer.get_unique_id():
		state.rename(id, Settings.get_value("name"))
		state.player(id).look = Looks.chosen()
		_pending_loadouts[id] = Weapons.chosen_loadout()
		_pending_overloads[id] = Overload.from_id(Settings.get_value("loadout_overload"))
	elif _pending_names.has(id):
		state.rename(id, _pending_names[id])
	if _pending_looks.has(id):
		state.player(id).look = _pending_looks[id]
	if _pending_loadouts.has(id):
		_arm(id, _pending_loadouts[id][0], _pending_loadouts[id][1], _pending_loadouts[id][2])
	_pending_names.erase(id)
	_pending_looks.erase(id)
	if _pending_overloads.has(id):
		state.choose_overload(id, _pending_overloads[id])
	_pending_loadouts.erase(id)
	_pending_overloads.erase(id)
	if _keys.has(id):
		_welcome_back(id)
	# Whoever has just come is sent what everyone has done, with the next ping.
	stats.changed = true
	# Someone joining a match on a custom arena is sent it.
	if is_host() and state.settings.map == Maps.CUSTOM and multiplayer.get_peers().has(id):
		_custom_arena.rpc_id(id, world.custom_code)
	_changed()


## Host only: a player has left. Their score is kept for a while, in case
## they come back.
func remove_player(id: int) -> void:
	var record := state.player(id)
	if record != null and _keys.has(id):
		_departed[_keys[id]] = {"record": record, "at": now()}
	_keys.erase(id)
	state.remove_player(id)
	_pending_names.erase(id)
	_rules.erase(id)
	recording.forget(id)
	_sent_to.erase(id)
	_struck_at.erase(id)
	_emoted_at.erase(id)
	_pings.erase(id)
	_histories.erase(id)
	_checks.erase(id)
	_put_back_at.erase(id)
	_changed()


## Tells the host what this player wants to be called, and the key by which
## the host can know them if they come back after losing their connection.
func introduce(wanted: String, key := WebPage.player_key()) -> void:
	if is_host():
		_rename(1, wanted)
	elif multiplayer.multiplayer_peer != null:
		_introduce.rpc_id(1, wanted, key)


## Tells the host this player's vote in whatever is being voted on: which
## of the choices for the next match, or Ballot.YES or NO.
func cast(option: int) -> void:
	if is_host():
		_cast(1, option)
	elif multiplayer.multiplayer_peer != null:
		_request_vote.rpc_id(1, option)


@rpc("any_peer", "call_remote", "reliable")
func _request_vote(option: Variant) -> void:
	if is_host():
		_cast(multiplayer.get_remote_sender_id(), option)


func _cast(voter: int, option: Variant) -> void:
	var ballot := state.ballot
	if ballot == null or not state.voters().has(voter) or not ballot.vote(voter, option):
		return
	_changed()
	_settle_removal()


## Asks the host for a vote on removing player `target`.
func propose_removal(target: int) -> void:
	if is_host():
		_propose(1, target)
	elif multiplayer.multiplayer_peer != null:
		_request_removal.rpc_id(1, target)


@rpc("any_peer", "call_remote", "reliable")
func _request_removal(target: Variant) -> void:
	if is_host() and target is int:
		_propose(multiplayer.get_remote_sender_id(), target)


## Whether player `by` may start a vote on removing `target`: a real player
## other than themselves and the host, in a session with enough players to
## vote, while nothing else is being voted on.
func can_propose(by: int, target: int) -> bool:
	var voters := state.voters()
	return state.ballot == null and by != target and target != 1 and voters.has(by) and voters.has(target) and voters.size() >= Ballot.MIN_TO_REMOVE


func _propose(by: int, target: int) -> void:
	if not can_propose(by, target):
		return
	# Everyone but the one it is about may vote, and whoever asked says yes.
	state.ballot = Ballot.to_remove(target, state.voters().size() - 1)
	state.ballot.vote(by, Ballot.YES)
	announce("%s wants %s removed: F1 for yes, F2 for no" % [state.name_of(by), state.name_of(target)], MatchState.NO_TEAM)
	_changed()
	_settle_removal()


# Host only: closes a vote to remove someone once it has passed or failed.
func _settle_removal() -> void:
	var ballot := state.ballot
	if ballot == null or ballot.kind != Ballot.REMOVE:
		return
	var outcome := ballot.outcome()
	if outcome.is_empty():
		return
	state.ballot = null
	var name := state.name_of(ballot.subject)
	if outcome == "yes" and state.player(ballot.subject) != null:
		announce("%s was removed by a vote" % name, MatchState.NO_TEAM)
		_remove_by_vote(ballot.subject)
	else:
		announce("%s stays" % name, MatchState.NO_TEAM)
	_changed()


# Host only: puts player `id` out of the session, and keeps them out.
func _remove_by_vote(id: int) -> void:
	if _keys.has(id):
		_removed_keys[_keys[id]] = true
	if not multiplayer.get_peers().has(id):
		return
	_removed.rpc_id(id)
	# In case their game doesn't leave of its own accord.
	get_tree().create_timer(1.0).timeout.connect(func() -> void:
		if multiplayer.multiplayer_peer != null and multiplayer.get_peers().has(id):
			multiplayer.multiplayer_peer.disconnect_peer(id))


@rpc("authority", "call_remote", "reliable")
func _removed() -> void:
	Net.leave(REMOVED)


# Host only: after a match, the next is whatever the players voted for. A
# change of arena or of mode makes a new match of the same players.
func _apply_vote() -> void:
	var ballot := state.ballot
	if ballot == null or ballot.kind != Ballot.NEXT:
		return
	var chosen := ballot.winner()
	if chosen.map == state.settings.map and chosen.mode == state.settings.mode:
		return
	var next := MatchSettings.from_dict(state.settings.to_dict().merged({"map": chosen.map, "mode": chosen.mode}, true))
	state = state.with_settings(next)
	world.load_map(next.map)
	state.set_pickup_count(world.pickup_pads().size())
	state.blocks = BlockRules.new(world.cheese_blocks().size(), randi())
	_projectiles.clear()
	_rules.clear()
	_histories.clear()
	_checks.clear()
	recording.clear()
	announce("Next: %s" % Ballot.choice_text(chosen), MatchState.NO_TEAM)


## Tells the host this player wants to watch rather than play, or to play
## after all.
func watch(on: bool) -> void:
	if is_host():
		_watch(1, on)
	elif multiplayer.multiplayer_peer != null:
		_request_watch.rpc_id(1, on)


func _watch(id: int, on: Variant) -> void:
	if on is bool and state.set_watching(id, on):
		_changed()


## Pings `at`, as a ping of `kind` (see Pings), for whoever should see it.
func ping(at: Vector3, kind: String) -> void:
	if is_host():
		_pass_on_ping(multiplayer.get_unique_id(), at, kind)
	elif multiplayer.multiplayer_peer != null:
		_point.rpc_id(1, at, kind)


@rpc("any_peer", "call_remote", "reliable")
func _point(at: Variant, kind: Variant) -> void:
	if is_host():
		_pass_on_ping(multiplayer.get_remote_sender_id(), at, kind)


# Host only: passes a ping by `by` on to each player who should see it, if
# it is one, they are in play, and it isn't too soon after their last.
func _pass_on_ping(by: int, at: Variant, kind: Variant) -> void:
	var pinger := state.player(by)
	if not Pings.is_ping(at, kind) or pinger == null or not pinger.alive or pinger.watching:
		return
	if not Pings.may_ping(_clock - _point_times.get(by, -INF)):
		return
	_point_times[by] = _clock
	var teams := state.settings.is_teams()
	for id: int in state.players:
		if Bots.is_bot(id) or not Pings.seen_by(pinger, state.player(id), teams):
			continue
		if id == multiplayer.get_unique_id():
			pinged.emit(by, at, kind)
		elif multiplayer.get_peers().has(id):
			_point_from.rpc_id(id, by, at, kind)


@rpc("authority", "call_remote", "reliable")
func _point_from(by: Variant, at: Variant, kind: Variant) -> void:
	if by is int and Pings.is_ping(at, kind):
		pinged.emit(by, at, kind)


## Says `text` in the chat, to this player's team alone or to everyone.
func chat(text: String, to_team: bool) -> void:
	if is_host():
		_pass_on_chat(multiplayer.get_unique_id(), text, to_team)
	elif multiplayer.multiplayer_peer != null:
		_chat.rpc_id(1, text, to_team)


@rpc("any_peer", "call_remote", "reliable")
func _chat(text: Variant, to_team: Variant) -> void:
	if is_host():
		_pass_on_chat(multiplayer.get_remote_sender_id(), text, to_team)


# Host only: passes what `by` said on to each player who should read it,
# cleaned, if there is anything left of it and they haven't said too much.
func _pass_on_chat(by: int, text: Variant, to_team: Variant) -> void:
	var said := Chat.clean(text)
	var sender := state.player(by)
	if said.is_empty() or sender == null or not to_team is bool:
		return
	var times: Array = _chat_times.get(by, []).filter(func(at: float) -> bool: return _clock - at < Chat.WINDOW)
	_chat_times[by] = times
	if not Chat.may_send(times, _clock):
		return
	times.append(_clock)
	var teams := state.settings.is_teams()
	for id: int in state.players:
		if Bots.is_bot(id) or not Chat.seen_by(sender, state.player(id), to_team, teams):
			continue
		if id == multiplayer.get_unique_id():
			chatted.emit(by, said, to_team and teams)
		elif multiplayer.get_peers().has(id):
			_chat_from.rpc_id(id, by, said, to_team and teams)


@rpc("authority", "call_remote", "reliable")
func _chat_from(by: Variant, text: Variant, to_team: Variant) -> void:
	# Cleaned again here: it has come over a connection.
	if by is int and to_team is bool and not Chat.clean(text).is_empty():
		chatted.emit(by, Chat.clean(text), to_team)


## Sends a packet of this player's talk to whoever should hear it.
func say(packet: PackedByteArray) -> void:
	if is_host():
		_pass_on_talk(multiplayer.get_unique_id(), packet)
	elif multiplayer.multiplayer_peer != null:
		_talk.rpc_id(1, packet)


@rpc("any_peer", "call_remote", "unreliable")
func _talk(packet: Variant) -> void:
	if is_host():
		_pass_on_talk(multiplayer.get_remote_sender_id(), packet)


# Host only: passes a packet of `speaker`'s talk on to each player who
# should hear it, if it is a packet and they haven't sent too many.
func _pass_on_talk(speaker: int, packet: Variant) -> void:
	if not VoiceCodec.is_packet(packet) or state.player(speaker) == null:
		return
	# A second's worth of when each of their packets came.
	var times: Array = _talk_times.get(speaker, [])
	times = times.filter(func(at: float) -> bool: return _clock - at < 1.0)
	if not VoiceChat.allowed(times.size()):
		_talk_times[speaker] = times
		return
	times.append(_clock)
	_talk_times[speaker] = times
	var teams := state.settings.is_teams()
	for id: int in state.players:
		if Bots.is_bot(id) or not VoiceChat.hears(state.player(id), state.player(speaker), teams):
			continue
		if id == multiplayer.get_unique_id():
			voice_heard.emit(speaker, packet)
		elif multiplayer.get_peers().has(id):
			_talk_from.rpc_id(id, speaker, packet)


@rpc("authority", "call_remote", "unreliable")
func _talk_from(speaker: Variant, packet: Variant) -> void:
	if speaker is int and VoiceCodec.is_packet(packet):
		voice_heard.emit(speaker, packet)


@rpc("any_peer", "call_remote", "reliable")
func _request_watch(on: Variant) -> void:
	if is_host():
		_watch(multiplayer.get_remote_sender_id(), on)


## Tells the host the kind of Overload this player wants to set off.
func choose_overload(kind: int) -> void:
	if is_host():
		_choose_overload(1, kind)
	elif multiplayer.multiplayer_peer != null:
		_request_overload_kind.rpc_id(1, kind)


func _choose_overload(id: int, kind: Variant) -> void:
	if state.player(id) == null:
		if Overload.is_kind(kind):
			_pending_overloads[id] = kind
	elif state.choose_overload(id, kind):
		_changed()


@rpc("any_peer", "call_remote", "reliable")
func _request_overload_kind(kind: Variant) -> void:
	if is_host():
		_choose_overload(multiplayer.get_remote_sender_id(), kind)


# Host only: the Overloads that have just built up begin. A rain starts to
# fall round where its player stands now.
func _begin_overloads() -> void:
	for id: int in state.overloads_begun:
		var record := state.player(id)
		var node: Node3D = world.player_node(id) if world != null else null
		if record != null and node != null and Overload.is_active(record, Overload.RAIN):
			_rains.append({"shooter": id, "at": node.global_position, "next": 0, "wait": 0.0})
	state.overloads_begun.clear()
	_changed()


# Host only: lets fall whichever wheels of the rains under way are due.
func _rain(delta: float) -> void:
	for rain: Dictionary in _rains:
		rain.wait -= delta
		while rain.wait <= 0.0 and rain.next < Overload.RAIN_WHEELS:
			_launch(rain.shooter, Weapons.find(Overload.RAIN_WEAPON), Overload.rain_start(rain.at, rain.next), Vector3.DOWN)
			rain.next += 1
			rain.wait += Overload.RAIN_INTERVAL
	_rains = _rains.filter(func(rain: Dictionary) -> bool: return rain.next < Overload.RAIN_WHEELS)


## Tells the host the weapons this player wants to come into play with, and
## what they want fitted to the main one.
func choose_loadout(main: String, side: String, attachment := 0) -> void:
	if is_host():
		_arm(1, main, side, attachment)
	elif multiplayer.multiplayer_peer != null:
		_request_loadout.rpc_id(1, main, side, attachment)


# Host only: player `id` has chosen a loadout. It is theirs from when they
# next come back into play, or at once if they have only just come in and
# have what they came with.
func _arm(id: int, main: Variant, side: Variant, attachment: Variant = 0) -> void:
	var chosen := Weapons.clean_loadout(main, side, attachment)
	var record := state.player(id)
	if record == null:
		_pending_loadouts[id] = chosen
		return
	if record.main == chosen[0] and record.side == chosen[1] and record.attachment == chosen[2]:
		return
	var untouched := record.alive and record.health == state.settings.health and record.has_only_loadout()
	record.main = chosen[0]
	record.side = chosen[1]
	record.attachment = chosen[2]
	if untouched:
		state.rearm(record)
		_rules_for(id).refill()
	_changed()


@rpc("any_peer", "call_remote", "reliable")
func _request_loadout(main: Variant, side: Variant, attachment: Variant = 0) -> void:
	if is_host():
		_arm(multiplayer.get_remote_sender_id(), main, side, attachment)


## Tells the host what this player's character looks like (see Looks).
func wear(look: int) -> void:
	if is_host():
		_wear(1, look)
	elif multiplayer.multiplayer_peer != null:
		_request_look.rpc_id(1, look)


func _wear(id: int, look: Variant) -> void:
	var chosen := Looks.clean(look)
	var record := state.player(id)
	if record == null:
		_pending_looks[id] = chosen
	elif record.look != chosen:
		record.look = chosen
		_changed()


@rpc("any_peer", "call_remote", "reliable")
func _request_look(look: Variant) -> void:
	if is_host():
		_wear(multiplayer.get_remote_sender_id(), look)


## Host only: a player whose key is `key` has said who they are. If they left
## lately, their score is theirs again.
func _known_as(id: int, key: String) -> void:
	if key.is_empty() or key.length() > MAX_KEY_LENGTH or _keys.values().has(key):
		return
	_keys[id] = key
	if state.player(id) != null:
		_welcome_back(id)


func _welcome_back(id: int) -> void:
	var cutoff := now() - REJOIN_WINDOW
	for key: String in _departed.keys():
		if _departed[key].at < cutoff:
			_departed.erase(key)
	var key: String = _keys[id]
	if not _departed.has(key):
		return
	var old: PlayerRecord = _departed[key].record
	_departed.erase(key)
	var record := state.player(id)
	record.kills = old.kills
	record.deaths = old.deaths
	record.overload = old.overload
	if old.team != MatchState.NO_TEAM:
		record.team = old.team
	_changed()


## This player fires `weapon` from `origin` along `direction`. `seed` decides
## where any spread sends each pellet, and `stance` how they stand as they
## fire (see WeaponStats.AIMED and the rest), which sets how wide it is.
## `direction` is where they aim: the host adds the weapon's kick itself.
func fire(origin: Vector3, direction: Vector3, seed: int, weapon := Weapons.STARTING, stance := 0) -> void:
	if is_host():
		_shot(1, origin, direction, seed, weapon, stance)
	elif multiplayer.multiplayer_peer != null:
		_request_shot.rpc_id(1, origin, direction, seed, weapon, stance)


## This player throws a grenade from `origin` along `direction`, having held
## it for `cooked` seconds of its fuse first.
func throw_grenade(origin: Vector3, direction: Vector3, cooked := 0.0) -> void:
	if is_host():
		_throw(1, origin, direction, cooked)
	elif multiplayer.multiplayer_peer != null:
		_request_throw.rpc_id(1, origin, direction, cooked)


## The longest a grenade can be held before it must be thrown: this much
## short of its fuse.
const COOK_MARGIN := 0.3


func _throw(thrower: int, origin: Vector3, direction: Vector3, cooked: float) -> void:
	var record := state.player(thrower)
	var node: Node3D = world.player_node(thrower)
	if record == null or node == null:
		return
	if not origin.is_finite() or not direction.is_finite() or direction.length() < 0.5 or not is_finite(cooked):
		return
	if not ShotRules.origin_believable(origin, node.head.global_position):
		return
	if not state.throw_grenade(thrower):
		return
	tally.shot(Weapons.GRENADE)
	stats.shot(thrower)
	_throw_seen.rpc(thrower)
	throw_shown.emit(thrower)
	record.protected_for = 0.0
	_changed()
	var grenade := Weapons.find(Weapons.GRENADE)
	var projectile := _launch(thrower, grenade, origin, direction)
	projectile.age = clampf(cooked, 0.0, grenade.fuse - COOK_MARGIN)


@rpc("any_peer", "call_remote", "reliable")
func _request_throw(origin: Variant, direction: Variant, cooked: Variant = 0.0) -> void:
	if is_host() and origin is Vector3 and direction is Vector3 and (cooked is float or cooked is int):
		_throw(multiplayer.get_remote_sender_id(), origin, direction, float(cooked))


## This player makes emote `kind` (see Emotes), for everyone to see.
func emote(kind: int) -> void:
	if is_host():
		_emote(1, kind)
	elif multiplayer.multiplayer_peer != null:
		_request_emote.rpc_id(1, kind)


func _emote(id: int, kind: Variant) -> void:
	var record := state.player(id)
	if record == null or not record.alive or not Emotes.is_kind(kind):
		return
	# No faster than they can be watched.
	if _clock - _emoted_at.get(id, -INF) < Emotes.INTERVAL:
		return
	_emoted_at[id] = _clock
	_emote_seen.rpc(id, kind)
	emote_shown.emit(id, kind)


@rpc("any_peer", "call_remote", "reliable")
func _request_emote(kind: Variant) -> void:
	if is_host():
		_emote(multiplayer.get_remote_sender_id(), kind)


@rpc("authority", "call_remote", "unreliable")
func _throw_seen(id: int) -> void:
	throw_shown.emit(id)


@rpc("authority", "call_remote", "reliable")
func _emote_seen(id: int, kind: int) -> void:
	emote_shown.emit(id, kind)


## This player strikes with their knife from `origin` along `direction`.
func strike(origin: Vector3, direction: Vector3) -> void:
	if is_host():
		_strike(1, origin, direction)
	elif multiplayer.multiplayer_peer != null:
		_request_strike.rpc_id(1, origin, direction)


## The host checks and carries out a strike that `striker` reports: it
## reaches the nearest player within the knife's reach and sweep, unless the
## arena is in the way.
func _strike(striker: int, origin: Vector3, direction: Vector3) -> void:
	var record := state.player(striker)
	var node: Node3D = world.player_node(striker)
	if record == null or not record.alive or node == null:
		return
	if not origin.is_finite() or not direction.is_finite() or direction.length() < 0.5:
		return
	if not ShotRules.origin_believable(origin, node.head.global_position):
		return
	# A little grace for strikes bunched up on the way here.
	if _clock - _struck_at.get(striker, -INF) < Melee.INTERVAL * 0.8:
		return
	_struck_at[striker] = _clock
	tally.shot(Melee.ID)
	stats.shot(striker)
	if record.is_protected():
		record.protected_for = 0.0
		_changed()
	for peer in multiplayer.get_peers():
		if peer != striker:
			_strike_seen.rpc_id(peer, striker)
	if striker != multiplayer.get_unique_id():
		strike_shown.emit(striker)
	var targets := _targets_except(striker, now() - rewind_for(striker))
	var nearest := {}
	var along := Vector3.ZERO
	for way in Melee.sweep(direction.normalized()):
		var reach := _distance_to_wall(origin, way, Melee.REACH)
		var hit := HitScan.first_hit(origin, way, reach, targets)
		if not hit.is_empty() and (nearest.is_empty() or hit.distance < nearest.distance):
			nearest = hit
			along = way
	if not nearest.is_empty():
		var yaw: float = targets[nearest.id].yaw
		_harm(striker, nearest.id, Melee.damage(along, yaw), origin, Melee.ID, false)


@rpc("any_peer", "call_remote", "reliable")
func _request_strike(origin: Variant, direction: Variant) -> void:
	if is_host() and origin is Vector3 and direction is Vector3:
		_strike(multiplayer.get_remote_sender_id(), origin, direction)


@rpc("authority", "call_remote", "unreliable")
func _strike_seen(striker: int) -> void:
	strike_shown.emit(striker)


## This player has started reloading `weapon`.
func reload(weapon := Weapons.STARTING) -> void:
	if is_host():
		_reload(1, weapon)
	elif multiplayer.multiplayer_peer != null:
		_request_reload.rpc_id(1, weapon)


## Player `id`'s round trip to the host in seconds, as the host last measured
## it; 0 for the host itself or before any.
func round_trip(id: int) -> float:
	return _pings[id].seconds() if _pings.has(id) else 0.0


@rpc("authority", "call_remote", "unreliable")
func _ping(sent: float) -> void:
	_pong.rpc_id(1, sent)
	heard_from_host.emit(sent)


@rpc("any_peer", "call_remote", "unreliable")
func _pong(sent: Variant) -> void:
	if not is_host() or not (sent is float) or sent > _clock:
		return
	var id := multiplayer.get_remote_sender_id()
	if not _pings.has(id):
		_pings[id] = PingTracker.new()
	_pings[id].add(_clock - sent)
	var record := state.player(id)
	if record != null and absi(record.ping - _pings[id].milliseconds()) >= PING_CHANGE:
		record.ping = _pings[id].milliseconds()
		_changed()


## Host only: the host is leaving, which ends the session for everyone.
func end_session() -> void:
	if is_host():
		_session_over.rpc(state.to_dict())


## The host ended the session; the state it sends is how it stood.
@rpc("authority", "call_remote", "reliable")
func _session_over(final: Variant) -> void:
	if final is Dictionary:
		state = MatchState.from_dict(final)
	Net.leave(HOST_ENDED)


## This player sets off their Overload, if it is ready.
func set_off_overload() -> void:
	if is_host():
		_overload(1)
	elif multiplayer.multiplayer_peer != null:
		_request_overload.rpc_id(1)


func _overload(id: int) -> void:
	if state.set_off_overload(id):
		_changed()
		_overload_seen.rpc(id)
		overloaded.emit(id)


@rpc("any_peer", "call_remote", "reliable")
func _request_overload() -> void:
	if is_host():
		_overload(multiplayer.get_remote_sender_id())


@rpc("authority", "call_remote", "reliable")
func _overload_seen(id: int) -> void:
	overloaded.emit(id)


## This player has taken `weapon` in hand, so others see it.
func select_weapon(weapon: String) -> void:
	if is_host():
		_select(1, weapon)
	elif multiplayer.multiplayer_peer != null:
		_request_select.rpc_id(1, weapon)


## This player's own record, or null before the host has sent it.
func own_record() -> PlayerRecord:
	return state.player(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null


func _process(delta: float) -> void:
	_clock += delta
	# Everyone runs the clocks, so countdowns show smoothly between updates;
	# only the host acts on them.
	var was_playing := state.is_playing()
	var due := state.tick(delta)
	if is_host():
		_since_ping += delta
		if _since_ping >= PING_INTERVAL:
			_since_ping = 0.0
			for peer in multiplayer.get_peers():
				_ping.rpc_id(peer, _clock)
			# With it, what everyone has done, if that has changed.
			if stats.changed:
				stats.changed = false
				_stats_from.rpc(stats.to_array())
				stats_changed.emit()
		for id in due:
			_respawn(id)
		if state.is_playing():
			var gnawed := state.blocks.tick(delta)
			if gnawed != -1:
				_gnaw_seen.rpc(gnawed)
				block_gnawed.emit(gnawed)
				_hurt_block(gnawed, BlockRules.GNAW)
		if state.just_started:
			state.just_started = false
			var rules: String = state.settings.rules
			announce("The match begins" if rules == MatchSettings.STANDARD else "The match begins  ·  %s" % MatchSettings.RULE_NAMES[rules], MatchState.NO_TEAM)
			_hold_back_powers()
			_changed()
		if state.just_overtime:
			state.just_overtime = false
			announce("Sudden death: the next score wins", MatchState.NO_TEAM)
			_changed()
		if state.ballot != null and state.ballot.kind == Ballot.REMOVE:
			_settle_removal()
		if state.round_won != -2:
			var taken: int = state.round_won
			state.round_won = -2
			if state.is_playing():
				announce("Nobody takes the round" if taken == MatchState.NO_TEAM else "%s takes the round" % MatchState.TEAM_NAMES[taken], taken)
			_changed()
		# Before any that begins now, whose first wheel is the next frame's.
		_rain(delta)
		if not state.overloads_begun.is_empty():
			_begin_overloads()
		if state.power_ended or state.drop_ended:
			state.power_ended = false
			state.drop_ended = false
			_changed()
		if was_playing != state.is_playing():
			_changed()
		# A match can end on a knock-out, between one frame and the next, as
		# well as on the clock just now.
		if _was_playing and not state.is_playing():
			_show_play_of_the_match()
		_was_playing = state.is_playing()
		if state.match_due():
			_new_match()
	_since_sync += delta
	if _since_sync >= RESYNC_INTERVAL:
		_dirty = true
	if _dirty and _since_sync >= SYNC_INTERVAL and is_host():
		_dirty = false
		_since_sync = 0.0
		_receive_state.rpc(state.to_dict())


func _changed() -> void:
	_dirty = true
	state_changed.emit()


func _rename(id: int, wanted: String) -> void:
	if state.player(id) == null:
		_pending_names[id] = wanted
	elif state.rename(id, wanted):
		_changed()


func _rules_for(id: int) -> ShotRules:
	if not _rules.has(id):
		_rules[id] = ShotRules.new()
	return _rules[id]


## The host checks and carries out a shot from `weapon` that `shooter`
## reports; by default, the weapon the host last saw in their hands.
func _shot(shooter: int, origin: Vector3, direction: Vector3, seed: int, weapon_id := "", stance := WeaponStats.AIMED) -> void:
	var record := state.player(shooter)
	var shooter_node: Node3D = world.player_node(shooter)
	if record == null or not record.alive or shooter_node == null:
		return
	if weapon_id.is_empty():
		weapon_id = record.weapon
	# As this player has it: with whatever they have fitted.
	var weapon := record.stats(weapon_id)
	if weapon == null or not record.weapons.has(weapon_id):
		return
	if not origin.is_finite() or not direction.is_finite() or direction.length() < 0.5:
		return
	if not ShotRules.origin_believable(origin, shooter_node.head.global_position):
		return
	# Nor is a shot from someone most of whose shots go where they aren't
	# facing (see AimCheck). A bot is the host's own, and isn't asked.
	if not Bots.is_bot(shooter) and not _aim_believable(shooter, direction):
		wild_shots += 1
		return
	# The weapon's other way of firing, if that is what this is.
	var other := Weapons.alt_of(weapon) if stance & WeaponStats.ALT else null
	if stance & WeaponStats.ALT and other == null:
		return
	if not _rules_for(shooter).allow_shot(weapon, _clock, other):
		return
	tally.shot(weapon.id)
	stats.shot(shooter)
	var credited := weapon.id
	if other != null:
		weapon = other
	if record.is_protected():
		record.protected_for = 0.0
		_changed()
	# The kick and the spread of this shot of the run, as the host counts it.
	var run := _rules_for(shooter).run(weapon)
	var cone := weapon.spread_for(stance, run)
	direction = Recoil.apply(direction.normalized(), Recoil.offset(weapon, run, stance & WeaponStats.AIMED != 0))
	if weapon.fires_projectile():
		_launch(shooter, weapon, origin, HitScan.pellet_directions(direction, 1, cone, seed)[0])
		return

	var targets := _targets_except(shooter, now() - rewind_for(shooter))
	var damage_to := {}
	var headshot := {}
	var ends := []
	for pellet in HitScan.pellet_directions(direction, weapon.pellets, cone, seed):
		# Through thin cover, at a cost; stopped by anything more.
		var way := Penetration.path(origin, pellet, weapon.max_range, wall_caster(world.get_world_3d().direct_space_state))
		var hit := HitScan.first_hit(origin, pellet, way.reach, targets)
		# A shot that meets a block of cheese before anyone takes some of it away.
		if hit.is_empty() or hit.distance > way.first:
			_shoot_block(origin, pellet, way.first, weapon.damage_at(way.first))
		if hit.is_empty():
			ends.append(origin + pellet * way.reach)
		else:
			ends.append(hit.point)
			var amount := weapon.damage_at(hit.distance) * (weapon.headshot_multiplier if hit.head else 1.0) * Penetration.share(way, hit.distance)
			damage_to[hit.id] = damage_to.get(hit.id, 0.0) + amount
			headshot[hit.id] = headshot.get(hit.id, false) or hit.head
	_show_shot_to_others(shooter, ends)
	recording.shot(now(), shooter, shooter_node.muzzle.global_position, ends)
	for victim: int in damage_to:
		_harm(shooter, victim, damage_to[victim], origin, credited, headshot[victim])


## Every player in play other than `except`, by id, as a HitScan.pose(): where
## they are and how they face, or, given a time, as the host saw them then.
func _targets_except(except: int, then := -1.0) -> Dictionary:
	var targets := {}
	for node: Node3D in world.players.get_children():
		var id := node.name.to_int()
		var target := state.player(id)
		if id != except and target != null and target.alive and not node.is_queued_for_deletion():
			var history: PositionHistory = _histories.get(id)
			if then >= 0.0 and history != null and not history.is_empty():
				var facing := history.facing_at(then)
				# Where they were then; crouched and leaning as they are now,
				# which the host doesn't keep a history of.
				targets[id] = HitScan.pose(history.at(then), facing.x, facing.y, node.crouch(), node.lean())
			else:
				targets[id] = HitScan.pose(node.global_position, node.rotation.y, node.head.rotation.x, node.crouch(), node.lean())
	return targets


## The host's clock for positions and shots, in seconds.
static func now() -> float:
	return Time.get_ticks_usec() / 1000000.0


## How far back to look for where `shooter`'s targets were when they fired.
## They saw the others a moment behind (InterpolationBuffer.DELAY), as others'
## movement came to them through the host, and their shot took half a round
## trip to arrive: a round trip and the delay in all. The host's own view has
## only the delay.
func rewind_for(shooter: int) -> float:
	return minf(round_trip(shooter) + InterpolationBuffer.DELAY, MAX_REWIND)


# Whether a shot by `shooter` along `direction` is one to allow, going by
# where they have lately been seen to face.
func _aim_believable(shooter: int, direction: Vector3) -> bool:
	if not _aims.has(shooter):
		_aims[shooter] = AimCheck.new()
	var history: PositionHistory = _histories.get(shooter)
	var faced: Array = history.facings_since(_clock - AIM_WINDOW) if history != null else []
	return _aims[shooter].allow(AimCheck.off_by(direction, faced))


## Shortest time between two put-backs of the same player, so that a player
## whose updates are all rejected isn't flooded with them.
const PUT_BACK_INTERVAL := 0.5


func _check_for(id: int) -> MovementCheck:
	if not _checks.has(id):
		_checks[id] = MovementCheck.new()
		# As the host's rules have players move.
		_checks[id].speed_scale = state.settings.speed
		_checks[id].air_scale = 1.0 / state.settings.gravity
	return _checks[id]


## Host only: player `id`'s update arrived at `time` placing them at `at`,
## after being moved at once `teleports` times, turned by `facing` (yaw,
## pitch). Movement that couldn't have happened is ignored, and the player is
## put back where they last were.
func on_moved(time: float, at: Vector3, teleports: int, id: int, facing := Vector2.ZERO) -> void:
	if not is_host():
		return
	var space := world.get_world_3d().direct_space_state
	var waist := Vector3.UP * MOVE_CHECK_HEIGHT
	var blocked := func(from: Vector3, to: Vector3) -> bool:
		return not space.intersect_ray(PhysicsRayQueryParameters3D.create(from + waist, to + waist, WORLD_MASK)).is_empty()
	var ground := PhysicsRayQueryParameters3D.create(at + Vector3.UP * 0.1, at + Vector3.DOWN * 0.6, WORLD_MASK)
	# Holding a ladder is as good as standing: it isn't hanging in the air.
	var on_ground := not space.intersect_ray(ground).is_empty() or not Fixtures.ladder_at(world.fixtures.ladders, at).is_empty()
	var check := _check_for(id)
	# A player last seen on a jump pad is about to be thrown by it.
	var launch: Variant = Fixtures.launch_at(world.fixtures.pads, check.last_position())
	if launch != null:
		check.pushed(time, launch)
	if check.check(time, at, teleports, blocked, on_ground):
		record_position(time, at, id, facing)
		return
	if time - _put_back_at.get(id, -INF) < PUT_BACK_INTERVAL:
		return
	_put_back_at[id] = time
	put_backs += 1
	var back := check.last_position()
	check.expect_move(back)
	if multiplayer.get_peers().has(id):
		_put_back.rpc_id(id, back)


## The host put this player back where they last were.
@rpc("authority", "call_remote", "reliable")
func _put_back(at: Vector3) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		own.put_back(at)


## Player `id` has moved at once to somewhere new; where they were before
## says nothing about where they are, so it is forgotten.
func forget_positions(id: int) -> void:
	_histories.erase(id)


## Host only: player `id` was at `at`, facing `facing` (yaw, pitch), at time
## `time` on the host's clock.
func record_position(time: float, at: Vector3, id: int, facing := Vector2.ZERO) -> void:
	if not is_host():
		return
	if not _histories.has(id):
		_histories[id] = PositionHistory.new()
	_histories[id].push(time, at, facing)
	recording.add(time, id, at, facing)


## `attacker` hurts `victim` for `amount` with `weapon`, from `from`, and
## everyone who needs to know is told.
func _harm(attacker: int, victim: int, amount: float, from: Vector3, weapon: String, head: bool) -> void:
	var own := attacker == victim
	var in_hand := state.player(victim).weapon if state.player(victim) != null else ""
	var result := state.damage(attacker, victim, amount, own)
	stats.hit(attacker, victim, result.dealt, weapon, result.knocked_out, _clock)
	if result.knocked_out:
		stats.knocked_out(victim)
	if result.knocked_out:
		_leave_behind(victim, in_hand)
		var fallen: Node3D = world.player_node(victim)
		if fallen != null:
			_fights.append({"where": fallen.global_position, "at": _clock, "victim": victim})
			while _fights.size() > 16:
				_fights.pop_front()
		# Told first, so that everyone knows how they went by the time they
		# hear that they have.
		_fall_seen.rpc(victim, weapon, from)
		fell.emit(victim, weapon, from)
	if result.dealt > 0 or result.absorbed > 0:
		_changed()
	if result.dealt > 0:
		if not own:
			tally.hit(weapon, result.dealt, result.knocked_out)
			_tell_hit(attacker, victim, result.dealt, result.knocked_out, head)
		_tell_hurt(victim, attacker, from, result.dealt)
	if result.knocked_out and not own:
		# A little ammunition is taken from whoever is knocked out.
		_resupply(attacker, SPOILS)
		_replay_knock_out(attacker, victim, weapon, head)
	if result.knocked_out:
		# Nobody is named as knocking out a player who did it themselves.
		_announce_knockout("" if own else state.name_of(attacker), state.name_of(victim), weapon, head)


func _launch(shooter: int, weapon: WeaponStats, origin: Vector3, direction: Vector3) -> Projectile:
	var projectile := Projectile.new(_next_projectile, shooter, weapon, origin, direction)
	_next_projectile += 1
	_projectiles[projectile.id] = projectile
	_projectile_seen.rpc(projectile.id, weapon.id, origin, projectile.velocity)
	projectile_launched.emit(projectile.id, weapon.id, origin, projectile.velocity)
	return projectile


func _physics_process(delta: float) -> void:
	if not is_host():
		return
	# The host's own player moves here, so its history is kept here too.
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		record_position(now(), own.global_position, own.name.to_int(), Vector2(own.rotation.y, own.head.rotation.x))
	_collect_pickups()
	_collect_drops()
	_run_objectives(delta)
	_run_moving_parts(delta)
	if _projectiles.is_empty():
		return
	var wall := wall_finder(world.get_world_3d().direct_space_state)
	for id: int in _projectiles.keys():
		var projectile: Projectile = _projectiles[id]
		var burst: Variant = projectile.advance(delta, wall, _targets_except(projectile.shooter))
		if burst != null:
			_projectiles.erase(id)
			_burst(projectile, burst)


## What Penetration is told of the arena in `space`: a Callable(from, to)
## giving how far along the segment the arena is first met, or INF.
static func wall_caster(space: PhysicsDirectSpaceState3D) -> Callable:
	return func(from: Vector3, to: Vector3) -> float:
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, WORLD_MASK))
		return from.distance_to(hit.position) if not hit.is_empty() else INF


## What a Projectile is told of the arena in `space`: a Callable(from, to)
## giving where along the segment the arena stops it, as {distance, normal},
## or INF if nowhere.
static func wall_finder(space: PhysicsDirectSpaceState3D) -> Callable:
	return func(from: Vector3, to: Vector3) -> Variant:
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, WORLD_MASK))
		if hit.is_empty():
			return INF
		return {"distance": from.distance_to(hit.position), "normal": hit.normal}


# Player `id`, just knocked out, leaves the weapon they held where they
# were: on the ground under them, if they were off it.
func _leave_behind(id: int, weapon: String) -> void:
	var node: Node3D = world.player_node(id)
	if node == null:
		return
	var at := node.global_position
	var below := PhysicsRayQueryParameters3D.create(at + Vector3.UP * 0.5, at + Vector3.DOWN * 40.0, WORLD_MASK)
	var ground := world.get_world_3d().direct_space_state.intersect_ray(below)
	if not ground.is_empty():
		at = ground.position
	state.drop(weapon, at)


# The host hands each weapon left behind to the first player in play to
# walk over it.
func _collect_drops() -> void:
	for lying: Dictionary in state.drops.duplicate():
		for node: Node3D in world.players.get_children():
			var id := node.name.to_int()
			if node.is_queued_for_deletion() or not _is_where_shown(id, node) or not Pickups.within_reach(lying.at, node.global_position):
				continue
			var outcome := state.take_drop(lying.id, id)
			if outcome.is_empty():
				continue
			_rules_for(id).refill(lying.weapon)
			if outcome == "refill":
				_tell_refill(id, lying.weapon)
			_changed()
			_pickup_seen.rpc(id, lying.weapon)
			picked_up.emit(id, lying.weapon)
			break


@rpc("authority", "call_remote", "reliable")
func _gnaw_seen(index: int) -> void:
	block_gnawed.emit(index)


# Host only: a shot from `origin` along `direction` met the arena `distance`
# away. If that was a block of cheese, it loses `amount`.
func _shoot_block(origin: Vector3, direction: Vector3, distance: float, amount: float) -> void:
	if state.blocks.health.is_empty():
		return
	var query := PhysicsRayQueryParameters3D.create(origin, origin + direction * (distance + 0.1), WORLD_MASK)
	var met := world.get_world_3d().direct_space_state.intersect_ray(query)
	if not met.is_empty():
		_hurt_block(world.block_index(met.collider), amount)


# Host only: a burst of `weapon` at `at` takes from every block of cheese it
# reaches, measured to the block's nearest side.
func _burst_blocks(at: Vector3, weapon: WeaponStats) -> void:
	var boxes: Array = world.block_boxes()
	for index in boxes.size():
		var box: AABB = boxes[index]
		var nearest := at.clamp(box.position, box.end)
		_hurt_block(index, Splash.damage_at(at.distance_to(nearest), weapon.splash_damage, weapon.splash_radius))


# Host only: block `index` loses `amount`. One that is finished goes, and
# takes with it whatever blocks stood on it.
func _hurt_block(index: int, amount: float) -> void:
	if not state.blocks.standing(index) or amount <= 0.0:
		return
	if state.blocks.hurt(index, amount):
		for above: int in BlockRules.resting_on(world.block_boxes(), index):
			state.blocks.remove(above)
	_changed()


# Host only: `attacker` has knocked `victim` out. The victim is shown how,
# from behind the attacker, and the moment is weighed as the play of the
# match.
func _replay_knock_out(attacker: int, victim: int, weapon: String, head: bool) -> void:
	var time := now()
	var moment := highlights.knock_out(time, attacker, head, weapon == Melee.ID)
	highlights.consider(moment, recording.clip([attacker, victim], time, Highlights.CLIP), attacker, victim)
	var clip := recording.clip([attacker, victim], time, KNOCK_OUT_CLIP)
	if victim == multiplayer.get_unique_id():
		replay_shown.emit(clip, attacker, KNOCK_OUT_TITLE)
	elif multiplayer.get_peers().has(victim):
		_replay.rpc_id(victim, clip, attacker, KNOCK_OUT_TITLE)


# Host only: the match is over, and everyone is shown the best moment of it.
func _show_play_of_the_match() -> void:
	var best := highlights.best
	if best.is_empty():
		return
	var title := "Play of the match: %s  ·  %s" % [state.name_of(best.attacker), best.text]
	_replay.rpc(best.clip, best.attacker, title)
	replay_shown.emit(best.clip, best.attacker, title)


@rpc("authority", "call_remote", "reliable")
func _replay(clip: Variant, about: Variant, title: Variant) -> void:
	if clip is Dictionary and about is int and title is String:
		replay_shown.emit(clip, about, title.left(120))


# The host hurts whoever stands in a hazard, and opens doors for whoever
# comes near.
func _run_moving_parts(delta: float) -> void:
	# Only in the arena the match is being played on.
	if world.map_id != state.settings.map:
		return
	var parts: Dictionary = world.moving_parts
	state.has_lifts = not parts.lifts.is_empty()
	if parts.hazards.is_empty() and parts.doors.is_empty():
		return
	var standing := {}
	for node: Node3D in world.players.get_children():
		var record := state.player(node.name.to_int())
		if record != null and not node.is_queued_for_deletion():
			standing[record.id] = {"at": node.global_position, "alive": record.alive and _is_where_shown(record.id, node)}
	if state.is_playing():
		for hurt: Dictionary in _moving.hurt(delta, parts.hazards, standing):
			_harm(hurt.id, hurt.id, hurt.amount, hurt.at, hurt.kind, false)
	var open := _moving.doors_open(delta, parts.doors, standing, state.doors)
	if open != state.doors:
		state.doors = open
		_changed()


@rpc("authority", "call_remote", "reliable")
func _custom_arena(code: Variant) -> void:
	if code is String and not ArenaCode.decode(code).is_empty():
		world.load_custom(code)


## Host only: tells everyone `text`, in `team`'s colour.
func announce(text: String, team: int) -> void:
	_announced.rpc(text, team)
	announced.emit(text, team)


@rpc("authority", "call_remote", "reliable")
func _announced(text: String, team: int) -> void:
	announced.emit(text, team)


# The host moves the mode's own rules on: wheels, the hill, points.
func _run_objectives(delta: float) -> void:
	if state.capture == null and state.zones == null:
		return
	var standing := {}
	for node: Node3D in world.players.get_children():
		var record := state.player(node.name.to_int())
		if record != null and not node.is_queued_for_deletion():
			# One just brought back isn't yet where they are seen.
			standing[record.id] = {"at": node.global_position, "team": record.team, "alive": record.alive and _is_where_shown(record.id, node)}
	var before := [state.team_points[0], state.team_points[1], state.is_playing()]
	var spots: Dictionary = world.objective_spots()
	var told := state.update_objectives(delta, spots, standing)
	for line: Dictionary in told:
		announce(line.text, line.team)
		# Whoever took a wheel, or brought it home, is credited with it.
		if line.has("by"):
			stats.objective(line.by, line.points)
	# And whoever stands on the hill, or on a point, with the time they do.
	if state.zones != null and state.is_playing():
		var hill := state.settings.mode == MatchSettings.HILL
		var zones: Array = spots.get("hills", []) if hill else spots.get("points", [])
		for id: int in standing:
			if not standing[id].alive:
				continue
			for index in zones.size():
				if (not hill or index == state.zones.hill) and ZoneRules.within(zones[index], standing[id].at):
					stats.objective(id, delta)
					break
	if not told.is_empty() or before != [state.team_points[0], state.team_points[1], state.is_playing()]:
		_changed()


# A match begins with no power-up on its pad: it comes in a while.
func _hold_back_powers() -> void:
	if world == null:
		return
	var pads: Array = world.pickup_pads()
	for index in pads.size():
		if pads[index].kind == Pickups.POWER:
			state.hold_back(index, Pickups.POWER_FIRST)


@rpc("authority", "call_remote", "reliable")
func _power_seen(id: int, power: int) -> void:
	power_taken.emit(id, power)


## The host hands each pickup that is there to the first player in play
## standing on its pad who has a use for it.
func _collect_pickups() -> void:
	var pads: Array = world.pickup_pads()
	for index in mini(pads.size(), state.pickups.size()):
		if not state.pickup_ready(index):
			continue
		var pad: Dictionary = pads[index]
		for node: Node3D in world.players.get_children():
			var id := node.name.to_int()
			if node.is_queued_for_deletion() or not _is_where_shown(id, node) or not Pickups.within_reach(pad.position, node.global_position):
				continue
			var outcome := state.collect(index, pad.kind, id)
			if outcome.is_empty():
				continue
			if outcome == "refill":
				_rules_for(id).refill(pad.kind)
				_tell_refill(id, pad.kind)
			elif pad.kind == Pickups.AMMO:
				_resupply(id, 1.0)
			_changed()
			_pickup_seen.rpc(id, pad.kind)
			picked_up.emit(id, pad.kind)
			if pad.kind == Pickups.POWER:
				_power_seen.rpc(id, state.player(id).power)
				power_taken.emit(id, state.player(id).power)
			break


## Host only: gives player `id` `magazines` magazines' worth of rounds for
## every weapon they carry that can run out.
func _resupply(id: int, magazines: float) -> void:
	var record := state.player(id)
	if record == null:
		return
	for carried: String in record.weapons:
		var weapon := record.stats(carried)
		_rules_for(id).add_reserve(weapon, roundi(weapon.magazine * magazines))
	if id == multiplayer.get_unique_id():
		_resupplied(magazines)
	elif multiplayer.get_peers().has(id):
		_resupplied.rpc_id(id, magazines)


## This player was given `magazines` magazines' worth for each weapon.
@rpc("authority", "call_remote", "reliable")
func _resupplied(magazines: float) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		for carried: WeaponState in own.arsenal.values():
			carried.add_reserve(roundi(carried.stats.magazine * magazines))


func _tell_refill(id: int, weapon: String) -> void:
	if id == multiplayer.get_unique_id():
		_refilled(weapon)
	elif multiplayer.get_peers().has(id):
		_refilled.rpc_id(id, weapon)


## A pickup refilled a weapon this player already carried.
@rpc("authority", "call_remote", "reliable")
func _refilled(weapon: String) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null and own.arsenal.has(weapon):
		own.arsenal[weapon].refill()


@rpc("authority", "call_remote", "reliable")
func _pickup_seen(id: int, kind: String) -> void:
	picked_up.emit(id, kind)


## A projectile bursts at `at`, hurting and shoving everyone in reach who
## isn't behind a wall from it, its shooter included: firing one at their own
## feet costs them.
func _burst(projectile: Projectile, at: Vector3) -> void:
	_burst_seen.rpc(projectile.id, projectile.weapon.id, at)
	projectile_burst.emit(projectile.id, projectile.weapon.id, at)
	var weapon := projectile.weapon
	if projectile.struck != 0 and weapon.impact_damage > 0.0:
		_harm(projectile.shooter, projectile.struck, weapon.impact_damage * (weapon.headshot_multiplier if projectile.struck_head else 1.0), projectile.position - projectile.velocity.normalized(), weapon.credit(), projectile.struck_head)
	if weapon.splash_radius <= 0.0:
		return
	_burst_blocks(at, weapon)
	var space := world.get_world_3d().direct_space_state
	# A burst against a wall is at its face; sight lines are checked from a
	# hand's width back along the way it came, so the wall itself counts.
	var eye := at - projectile.velocity.normalized() * 0.1 if projectile.velocity.length() > 0.001 else at
	for node: Node3D in world.players.get_children():
		var id := node.name.to_int()
		var record := state.player(id)
		if record == null or not record.alive or node.is_queued_for_deletion():
			continue
		# Measured to the nearest point up the middle of the body.
		var middle := BodyShape.nearest_middle(node.global_position, at)
		if at.distance_to(middle) >= weapon.splash_radius:
			continue
		var query := PhysicsRayQueryParameters3D.create(eye, middle, WORLD_MASK)
		if not space.intersect_ray(query).is_empty():
			continue
		_tell_push(id, Splash.push(at, middle, weapon.knockback, weapon.splash_radius))
		_harm(projectile.shooter, id, Splash.damage_at(at.distance_to(middle), weapon.splash_damage, weapon.splash_radius), at, weapon.credit(), false)


func _tell_push(id: int, push: Vector3) -> void:
	if push == Vector3.ZERO:
		return
	_check_for(id).pushed(now(), push)
	if id == multiplayer.get_unique_id():
		_pushed(push)
	elif multiplayer.get_peers().has(id):
		_pushed.rpc_id(id, push)


## A burst shoved this player. Players move themselves, so the host can only
## tell them.
@rpc("authority", "call_remote", "reliable")
func _pushed(push: Vector3) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		own.push(push)


@rpc("authority", "call_remote", "reliable")
func _projectile_seen(id: int, weapon: String, from: Vector3, velocity: Vector3) -> void:
	projectile_launched.emit(id, weapon, from, velocity)


@rpc("authority", "call_remote", "reliable")
func _burst_seen(id: int, weapon: String, at: Vector3) -> void:
	projectile_burst.emit(id, weapon, at)


func _show_shot_to_others(shooter: int, ends: Array) -> void:
	for peer in multiplayer.get_peers():
		if peer != shooter:
			_shot_seen.rpc_id(peer, shooter, ends)
	if shooter != multiplayer.get_unique_id():
		shot_shown.emit(shooter, ends)


func _tell_hit(shooter: int, victim: int, dealt: int, knocked_out: bool, head: bool) -> void:
	if shooter == multiplayer.get_unique_id():
		hit_confirmed.emit(victim, dealt, knocked_out, head)
	elif multiplayer.get_peers().has(shooter):
		_hit.rpc_id(shooter, victim, dealt, knocked_out, head)


func _tell_hurt(victim: int, attacker: int, from: Vector3, dealt: int) -> void:
	if victim == multiplayer.get_unique_id():
		hurt.emit(attacker, from, dealt)
	elif multiplayer.get_peers().has(victim):
		_hurt.rpc_id(victim, attacker, from, dealt)


func _announce_knockout(attacker: String, victim: String, weapon: String, head: bool) -> void:
	_knockout_seen.rpc(attacker, victim, weapon, head)
	knocked_out.emit(attacker, victim, weapon, head)


@rpc("authority", "call_remote", "reliable")
func _fall_seen(victim: int, weapon: String, from: Vector3) -> void:
	fell.emit(victim, weapon, from)


@rpc("authority", "call_remote", "reliable")
func _knockout_seen(attacker: String, victim: String, weapon: String, head: bool) -> void:
	knocked_out.emit(attacker, victim, weapon, head)


@rpc("authority", "call_remote", "unreliable")
func _shot_seen(shooter: int, ends: Array) -> void:
	shot_shown.emit(shooter, ends)


@rpc("authority", "call_remote", "reliable")
func _hit(victim: int, dealt: int, knocked_out: bool, head: bool) -> void:
	hit_confirmed.emit(victim, dealt, knocked_out, head)


@rpc("authority", "call_remote", "reliable")
func _hurt(attacker: int, from: Vector3, dealt: int) -> void:
	hurt.emit(attacker, from, dealt)


## How far a shot from `origin` along `direction` travels before the arena
## stops it, up to `max_range`.
func _distance_to_wall(origin: Vector3, direction: Vector3, max_range: float) -> float:
	var query := PhysicsRayQueryParameters3D.create(origin, origin + direction * max_range, WORLD_MASK)
	var hit := world.get_world_3d().direct_space_state.intersect_ray(query)
	return origin.distance_to(hit.position) if not hit.is_empty() else max_range


func _reload(id: int, weapon_id: String) -> void:
	var record := state.player(id)
	var weapon := record.stats(weapon_id) if record != null else null
	if record != null and weapon != null and record.weapons.has(weapon_id):
		_rules_for(id).reload(weapon, _clock)


func _select(id: int, weapon: String) -> void:
	var record := state.player(id)
	if record != null and record.weapons.has(weapon) and record.weapon != weapon:
		record.weapon = weapon
		_changed()


## The host brings a knocked-out player back, at the spot farthest from the
## players who could hurt them.
func _respawn(id: int) -> void:
	var enemies := []
	var allies := []
	for node: Node3D in world.players.get_children():
		var other := state.player(node.name.to_int())
		if other == null or other.id == id or not other.alive or node.is_queued_for_deletion():
			continue
		if state.are_allies(id, other.id):
			allies.append(node.global_position)
		else:
			enemies.append({"at": node.global_position, "yaw": node.rotation.y})
	# One with a place of its own, as a target on the range has, goes back to it.
	var home: Variant = world.home_of(id)
	if home != null:
		_bring_back(id, home)
		return
	# Where there has lately been fighting, other than their own knock-out:
	# whoever did that is already someone to keep away from.
	var fights := []
	for fight: Dictionary in _fights:
		if fight.victim != id and _clock - fight.at <= FIGHT_MEMORY:
			fights.append(fight.where)
	var space := world.get_world_3d().direct_space_state
	var eye := Vector3.UP * BodyShape.HEAD_HEIGHT * BodyShape.SIZE
	var sees := func(from: Vector3, to: Vector3) -> bool:
		return space.intersect_ray(PhysicsRayQueryParameters3D.create(from + eye, to + eye, WORLD_MASK)).is_empty()
	var spots: Array[Vector3] = world.spawn_positions()
	_bring_back(id, spots[SpawnChooser.best(spots, enemies, allies, fights, sees)])


@rpc("authority", "call_remote", "reliable")
func _stats_from(sent: Variant) -> void:
	stats = Stats.from_array(sent)
	stats_changed.emit()


## The host starts the next match, with everyone back in play at a spot of
## their own.
func _new_match() -> void:
	stats.clear()
	# Scores start again, so there are none to give back.
	_departed.clear()
	tally.clear()
	_rains.clear()
	highlights.clear()
	_apply_vote()
	state.start_match()
	_hold_back_powers()
	_fights.clear()
	var spots: Array[Vector3] = world.spawn_positions()
	spots.shuffle()
	# In a mode with bases, each team starts on its own side of the arena.
	var bases: Array = world.objective_spots().bases
	var sides: Array = SpawnChooser.by_side(spots, bases) if state.capture != null and bases.size() == 2 else []
	var taken := [0, 0]
	var index := 0
	for id: int in state.players:
		var record := state.player(id)
		# Not those who are only watching.
		if record.watching:
			continue
		if not sides.is_empty() and record.team >= 0 and not sides[record.team].is_empty():
			_bring_back(id, sides[record.team][taken[record.team] % sides[record.team].size()])
			taken[record.team] += 1
			continue
		_bring_back(id, spots[index % spots.size()])
		index += 1
	_changed()


# Where each player other than the host's own was last sent on coming back,
# until they are seen there. Players move themselves, so for a moment after
# one is brought back the host still sees them where they were knocked out,
# and they must not collect what lies there: least of all the weapon they
# have just dropped.
var _sent_to := {}
## How near to where they were sent a player must be seen to count as there.
const ARRIVED_WITHIN := 4.0


# Whether player `id`, drawn at `node`, is really where the host sees them.
func _is_where_shown(id: int, node: Node3D) -> bool:
	if not _sent_to.has(id):
		return true
	if node.global_position.distance_to(_sent_to[id]) > ARRIVED_WITHIN:
		return false
	_sent_to.erase(id)
	return true


func _bring_back(id: int, at: Vector3) -> void:
	forget_positions(id)
	_check_for(id).expect_move(at)
	if id != multiplayer.get_unique_id() and not Bots.is_bot(id):
		_sent_to[id] = at
	state.respawn(id)
	_rules_for(id).refill()
	_changed()
	if id == multiplayer.get_unique_id():
		_respawned(at)
	elif multiplayer.get_peers().has(id):
		_respawned.rpc_id(id, at)
	elif Bots.is_bot(id) and world.player_node(id) != null:
		# The host moves its bots itself.
		world.player_node(id).respawn_at(at)


## Tells a player where they have come back. Players move themselves, so the
## host can only ask.
@rpc("authority", "call_remote", "reliable")
func _respawned(at: Vector3) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		own.respawn_at(at)


@rpc("any_peer", "call_remote", "reliable")
func _request_shot(origin: Variant, direction: Variant, seed: Variant, weapon: Variant, stance: Variant = 0) -> void:
	if is_host() and origin is Vector3 and direction is Vector3 and seed is int and weapon is String:
		_shot(multiplayer.get_remote_sender_id(), origin, direction, seed, weapon, stance & 31 if stance is int else 0)


@rpc("any_peer", "call_remote", "reliable")
func _request_reload(weapon: Variant) -> void:
	if is_host() and weapon is String:
		_reload(multiplayer.get_remote_sender_id(), weapon)


@rpc("any_peer", "call_remote", "reliable")
func _request_select(weapon: Variant) -> void:
	if is_host() and weapon is String:
		_select(multiplayer.get_remote_sender_id(), weapon)


@rpc("any_peer", "call_remote", "reliable")
func _introduce(wanted: Variant, key: Variant = "") -> void:
	if not is_host():
		return
	var id := multiplayer.get_remote_sender_id()
	# Someone voted out stays out.
	if key is String and _removed_keys.has(key):
		_remove_by_vote(id)
		return
	if key is String:
		_known_as(id, key)
	if wanted is String:
		_rename(id, wanted)


@rpc("authority", "call_remote", "reliable")
func _receive_state(data: Variant) -> void:
	if data is Dictionary:
		state = MatchState.from_dict(data)
		_has_state = true
		state_changed.emit()
