class_name Readings
extends Control
## Numbers for the curious: how many frames a second the game is drawing,
## the round trip to the host, how much of what the host sends is going
## astray, and a small graph of how long each frame took. Each is a setting,
## and all are off unless asked for.

const FPS := "show_fps"
const PING := "show_ping"
const LOSS := "show_loss"
const GRAPH := "show_graph"
const SETTINGS := [FPS, PING, LOSS, GRAPH]
## How many frames the rate is taken over and the graph shows, and how many
## of the host's once-a-second messages the loss is taken over.
const FRAMES := 90
const HEARD := 30
## The height of the graph stands for this long a frame, in milliseconds.
const GRAPH_TOP_MS := 50.0
const WIDE := 150.0

var world: Node3D
# How long each recent frame took, in seconds, oldest first.
var _frames: Array[float] = []
# For each of the host's recent messages, whether it arrived (true) or was
# found to be missing when a later one did (false).
var _arrived: Array[bool] = []
var _last_sent := -1.0
var _label: Label


func _init(of: Node3D = null) -> void:
	world = of
	name = "Readings"
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	custom_minimum_size = Vector2(WIDE, 0)
	_label = Label.new()
	_label.add_theme_font_size_override("font_size", 13)
	_label.add_theme_constant_override("outline_size", 5)
	_label.add_theme_color_override("font_outline_color", Color.BLACK)
	_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_label)


func _ready() -> void:
	if world != null:
		world.match_.heard_from_host.connect(func(sent: float) -> void: heard(sent, Match.PING_INTERVAL))


## A frame took `took` seconds.
func frame(took: float) -> void:
	if took <= 0.0:
		return
	_frames.append(took)
	while _frames.size() > FRAMES:
		_frames.pop_front()


## Frames a second, over the frames lately counted.
func fps() -> int:
	var total := 0.0
	for took in _frames:
		total += took
	return roundi(_frames.size() / total) if total > 0.0 else 0


## The longest of the frames lately counted, in milliseconds.
func worst_ms() -> float:
	return (_frames.max() as float) * 1000.0 if not _frames.is_empty() else 0.0


## One of the host's regular messages arrived, sent when the host's clock
## read `sent`; they are sent `every` seconds. A gap since the last says
## how many between went astray.
func heard(sent: float, every: float) -> void:
	if _last_sent >= 0.0 and sent > _last_sent and every > 0.0:
		for missed in clampi(roundi((sent - _last_sent) / every) - 1, 0, HEARD):
			_arrived.append(false)
	if sent <= _last_sent:
		# One that came late, out of turn: it was counted as missing.
		return
	_last_sent = sent
	_arrived.append(true)
	while _arrived.size() > HEARD:
		_arrived.pop_front()


## The share of the host's recent messages that went astray, as a whole
## percentage.
func loss() -> int:
	return roundi(100.0 * _arrived.count(false) / _arrived.size()) if not _arrived.is_empty() else 0


## Forgets the host's messages, as when a session ends.
func forget_host() -> void:
	_arrived.clear()
	_last_sent = -1.0


## The lines of numbers to show: those of `wanted` ({setting: bool}), with a
## round trip of `ping` milliseconds, or none for the `host`, who has no
## trip to make.
func lines(wanted: Dictionary, ping: int, host: bool) -> Array[String]:
	var said: Array[String] = []
	if wanted.get(FPS, false):
		said.append("%d fps" % fps())
	if wanted.get(PING, false):
		said.append("Ping: host" if host else "Ping %d ms" % ping)
	if wanted.get(LOSS, false):
		said.append("Loss: host" if host else "Loss %d%%" % loss())
	return said


## The heights of the graph's bars, from 0 to 1, oldest first: how long each
## frame took against the longest the graph can show.
func bars() -> Array[float]:
	var heights: Array[float] = []
	for took in _frames:
		heights.append(clampf(took * 1000.0 / GRAPH_TOP_MS, 0.0, 1.0))
	return heights


## What the settings ask to be shown.
static func wanted() -> Dictionary:
	var asked := {}
	for key: String in SETTINGS:
		asked[key] = Settings.get_value(key)
	return asked


func _process(delta: float) -> void:
	frame(delta)
	var asked := wanted()
	var record: PlayerRecord = world.match_.own_record() if world != null and world.match_.has_state() and multiplayer.multiplayer_peer != null else null
	_label.text = "\n".join(lines(asked, record.ping if record != null else 0, world != null and world.match_.is_host()))
	_label.visible = not _label.text.is_empty()
	queue_redraw()


func _draw() -> void:
	if not Settings.get_value(GRAPH):
		return
	# Under the numbers: a bar for each frame, green while it is quick,
	# amber and red as it gets long.
	var top := _label.size.y + 4.0 if _label.visible else 0.0
	var tall := 34.0
	draw_rect(Rect2(0, top, WIDE, tall), Color(0, 0, 0, 0.35))
	var heights := bars()
	var each := WIDE / FRAMES
	for index in heights.size():
		var height := maxf(heights[index] * tall, 1.0)
		var colour := Color(0.4, 1.0, 0.5) if heights[index] < 0.4 else (Color(1.0, 0.8, 0.3) if heights[index] < 0.7 else Color(1.0, 0.35, 0.3))
		draw_rect(Rect2(index * each, top + tall - height, maxf(each - 0.5, 1.0), height), colour)
