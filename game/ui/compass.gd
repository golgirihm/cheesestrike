class_name Compass
extends Control
## A compass across the top of the view: the points of it, and on it where
## teammates are, where the mode's objectives are, what has been pinged, and
## which way gunfire was heard from.
##
## It never shows where an opponent is. Their gunfire is shown, since it is
## heard anyway, but only as a direction, only while it is fresh, and only
## from as far as it carries.

const TEAMMATE := "teammate"
const OBJECTIVE := "objective"
const PING := "ping"
const SHOT := "shot"
## How much of the way round the strip shows, in radians: what is further
## round than its ends is drawn at the end.
const SPAN := deg_to_rad(140.0)
## Seconds the direction of a shot is shown for, and the furthest a shot is
## heard from, in metres.
const SHOT_LIFE := 2.0
const HEARING := 95.0
## The most shots remembered at once.
const MAX_SHOTS := 12
const WIDE := 360.0
const TALL := 30.0
const SHOT_COLOUR := Color(1.0, 0.45, 0.3)
const POINTS := {"N": 0.0, "E": PI / 2.0, "S": PI, "W": -PI / 2.0}

var world: Node3D
# Shots heard lately: {"at", "by", "left"}.
var _shots: Array[Dictionary] = []


func _init(of: Node3D = null) -> void:
	world = of
	name = "Compass"
	custom_minimum_size = Vector2(WIDE, TALL)
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _ready() -> void:
	if world != null:
		world.match_.shot_shown.connect(func(shooter: int, _ends: Array) -> void:
			var player: Node3D = world.player_node(shooter)
			if player != null:
				hear_shot(shooter, player.global_position))


## Which way `to` is from someone at `at` facing `yaw`: 0 straight ahead,
## positive to the right, in radians.
static func bearing(at: Vector3, yaw: float, to: Vector3) -> float:
	return CombatHud.damage_direction(at, yaw, to)


## Where along the strip something at bearing `angle` is drawn: -1 at its
## left end to 1 at its right, with whatever is further round held at the
## end it is nearer.
static func along(angle: float) -> float:
	return clampf(wrapf(angle, -PI, PI) / (SPAN / 2.0), -1.0, 1.0)


## Whether something at bearing `angle` is off the end of the strip.
static func off_the_end(angle: float) -> bool:
	return absf(wrapf(angle, -PI, PI)) > SPAN / 2.0


## What the compass shows the player `own` in the match `state`: [{"kind",
## "at", "colour"}]. `positions` is where each player is, by id;
## `objectives` and `pings` are [{"at", "colour"}], already those this
## player may see; `shots` is [{"at", "by"}], the shots heard lately.
static func marks(state: MatchState, own: int, positions: Dictionary, objectives: Array, pings: Array, shots: Array) -> Array[Dictionary]:
	var found: Array[Dictionary] = []
	var record := state.player(own) if state != null else null
	if record == null:
		return found
	var teams := state.settings.is_teams()
	for mark: Dictionary in objectives:
		found.append({"kind": OBJECTIVE, "at": mark.at, "colour": mark.get("colour", Color.WHITE)})
	if teams and record.team != MatchState.NO_TEAM:
		for id: int in positions:
			var other := state.player(id)
			if id != own and other != null and other.alive and not other.watching and other.team == record.team:
				found.append({"kind": TEAMMATE, "at": positions[id], "colour": TeamColors.of(record.team)})
	for mark: Dictionary in pings:
		found.append({"kind": PING, "at": mark.at, "colour": mark.get("colour", Color.WHITE)})
	if positions.has(own):
		for shot: Dictionary in shots:
			var by := state.player(shot.by)
			# Not one's own, nor a teammate's, who is on the compass already.
			if shot.by == own or (teams and by != null and by.team == record.team and record.team != MatchState.NO_TEAM):
				continue
			if positions[own].distance_to(shot.at) <= HEARING:
				found.append({"kind": SHOT, "at": shot.at, "colour": SHOT_COLOUR})
	return found


## A shot by player `by` was fired at `at`.
func hear_shot(by: int, at: Vector3) -> void:
	# One mark for each shooter, moved to where they last fired from.
	_shots = _shots.filter(func(shot: Dictionary) -> bool: return shot.by != by)
	_shots.append({"at": at, "by": by, "left": SHOT_LIFE})
	while _shots.size() > MAX_SHOTS:
		_shots.pop_front()


## What is on the compass just now (see marks), for this machine's player.
func shown() -> Array[Dictionary]:
	if world == null or not world.match_.has_state() or multiplayer.multiplayer_peer == null:
		return []
	var positions := {}
	for player in world.players.get_children():
		positions[String(player.name).to_int()] = player.global_position
	return marks(world.match_.state, multiplayer.get_unique_id(), positions, world.objective_marks.shown(), world.pings.shown(), _shots)


func _process(delta: float) -> void:
	for shot in _shots:
		shot.left -= delta
	_shots = _shots.filter(func(shot: Dictionary) -> bool: return shot.left > 0.0)
	queue_redraw()


func _draw() -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if world != null and multiplayer.multiplayer_peer != null else null
	if own == null:
		return
	var middle := size.x / 2.0
	var half := size.x / 2.0 - 8.0
	draw_rect(Rect2(Vector2.ZERO, size), Color(0, 0, 0, 0.28))
	draw_line(Vector2(middle, 0), Vector2(middle, 6), Look.CREAM, 2.0)
	var font := get_theme_default_font()
	# The points of the compass: north is the way the arena's -z runs.
	for letter: String in POINTS:
		var angle := wrapf(POINTS[letter] + own.rotation.y, -PI, PI)
		if not off_the_end(angle):
			draw_string(font, Vector2(middle + along(angle) * half - 5.0, 20.0), letter, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Look.FADED)
	for mark: Dictionary in shown():
		var angle := bearing(own.global_position, own.rotation.y, mark.at)
		var x := middle + along(angle) * half
		match mark.kind:
			SHOT:
				draw_line(Vector2(x, 4), Vector2(x, size.y - 4), mark.colour, 3.0)
			TEAMMATE:
				draw_circle(Vector2(x, size.y / 2.0), 5.0, mark.colour)
			PING:
				draw_colored_polygon(PackedVector2Array([Vector2(x - 6, 6), Vector2(x + 6, 6), Vector2(x, size.y - 4)]), mark.colour)
			_:
				draw_rect(Rect2(x - 5, size.y / 2.0 - 5, 10, 10), mark.colour)
