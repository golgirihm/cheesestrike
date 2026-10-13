class_name ShotRules
extends RefCounted
## The host's check on one player's shots. Players fire on their own machines and
## tell the host; this decides whether a reported shot could really have been
## fired, so that a modified game can't fire faster, further or from somewhere
## else than the weapon allows.
##
## Shots reach the host unevenly spaced, as the network bunches them, so the
## rate is checked with a small allowance: a budget of shots that refills at the
## weapon's rate and holds a few, rather than a fixed gap between each two.

## Shots that may arrive together after a delay on the network.
const BURST := 3.0
## How far, in metres, a reported shot may start from where the host last saw
## the shooter's head. The host's view is a little behind, by the time the
## player's position took to arrive.
const ORIGIN_TOLERANCE := 2.5
## Reloads may finish this much sooner than the weapon's reload time, for the
## same reason.
const RELOAD_ALLOWANCE := 0.75

# Per weapon id: shots in hand, when the budget was last topped up, shots fired
# since the last reload, and when the reload in progress is done.
var _budget := {}
var _budget_at := {}
var _fired := {}
var _reload_done_at := {}
# Per weapon id: when its last shot was, and which shot of a run of fire
# that was (0 for the first), for its recoil and spread.
var _shot_at := {}
var _run := {}


## Whether a shot from `weapon` at time `now` (seconds) is allowed, recording it
## if so.
func allow_shot(weapon: WeaponStats, now: float) -> bool:
	var id := weapon.id
	if _reload_done_at.has(id):
		if now < _reload_done_at[id]:
			return false
		_reload_done_at.erase(id)
		_fired[id] = 0
	if _fired.get(id, 0) >= weapon.magazine:
		return false
	var budget: float = _budget.get(id, BURST)
	if _budget_at.has(id):
		budget = minf(budget + (now - _budget_at[id]) / weapon.fire_interval, BURST)
	_budget_at[id] = now
	if budget < 1.0:
		_budget[id] = budget
		return false
	_budget[id] = budget - 1.0
	_fired[id] = _fired.get(id, 0) + 1
	_run[id] = next_in_run(weapon, _run.get(id, -1), now - _shot_at.get(id, -INF))
	_shot_at[id] = now
	return true


## Which shot of a run of fire the next from `weapon` is, `since` seconds
## after one that was number `last` (-1 if there was none): the first (0) of
## a new run after a pause, or else the one after.
static func next_in_run(weapon: WeaponStats, last: int, since: float) -> int:
	return 0 if last < 0 or since > weapon.recoil_reset else last + 1


## Which shot of a run of fire the last allowed from `weapon` was: 0 for the
## first.
func run(weapon: WeaponStats) -> int:
	return _run.get(weapon.id, 0)


## The player has started reloading `weapon` at time `now`.
func reload(weapon: WeaponStats, now: float) -> void:
	if _fired.get(weapon.id, 0) > 0 and not _reload_done_at.has(weapon.id):
		_reload_done_at[weapon.id] = now + weapon.reload_time * RELOAD_ALLOWANCE


## Everything is full again, as on respawning, or just `weapon` if given.
func refill(weapon := "") -> void:
	if weapon.is_empty():
		_fired.clear()
		_reload_done_at.clear()
		_run.clear()
		_shot_at.clear()
	else:
		_fired.erase(weapon)
		_reload_done_at.erase(weapon)


static func origin_believable(reported: Vector3, head: Vector3) -> bool:
	return reported.distance_to(head) <= ORIGIN_TOLERANCE
