class_name TestHook
extends Node
## Lets the browser tests (browser_tests/) see what the game is showing. The
## game draws everything to one canvas, so a test driving a browser has no
## buttons or text to find; this publishes a description of the screen to the
## page as `window.cheeseStrikeTest`, several times a second.
##
## It reports where each control is so that a test can click it for real, and
## offers no way to act on the game: tests use the mouse and keyboard, as a
## player would.
##
## Only active in a web build opened with `?test` in the address. The game
## then stops drawing, so `?test` is no use for looking at it.

const PUBLISH_INTERVAL := 0.05

var _main: Node
var _since_publish := 0.0


static func wanted() -> bool:
	return OS.has_feature("web") and JavaScriptBridge.eval("new URLSearchParams(location.search).has('test')")


func _init(main: Node) -> void:
	_main = main


func _ready() -> void:
	# The tests never look at the picture, and drawing it is most of the work
	# for a browser without a graphics card, as on a CI machine.
	RenderingServer.render_loop_enabled = false


func _process(delta: float) -> void:
	_since_publish += delta
	if _since_publish < PUBLISH_INTERVAL:
		return
	_since_publish = 0.0
	JavaScriptBridge.eval("window.cheeseStrikeTest = %s" % JSON.stringify(snapshot()), true)


func snapshot() -> Dictionary:
	var players := []
	for player: Node3D in _main.world.players.get_children():
		if player.is_queued_for_deletion():
			continue
		var at := player.global_position
		players.append({"id": player.name.to_int(), "mine": player.is_multiplayer_authority(), "position": [at.x, at.y, at.z]})

	# Each line of the session list: a session to click, or the note shown when
	# there are none.
	var sessions := []
	for line: Control in _main.sessions.get_children():
		if not line.is_queued_for_deletion():
			sessions.append({"text": line.text, "at": _centre(line)})

	var controls := {}
	for control: Control in [
		_main.host_button,
		_main.target_edit,
		_main.join_button,
		_main.refresh_button,
		_main.leave_button,
		_main.disconnected_button,
	]:
		if control.is_visible_in_tree():
			controls[control.name] = _centre(control)

	return {
		"screen": _screen(),
		"status": _main.status.text,
		"hud": _main.hud.text if _main.hud.visible else "",
		"code": Net.room_code,
		"notice": _main.disconnected_detail.text if _main.disconnected_notice.visible else "",
		"players": players,
		"sessions": sessions,
		"controls": controls,
	}


func _screen() -> String:
	if _main.disconnected_notice.visible:
		return "disconnected"
	if _main.waiting_notice.visible:
		return "waiting"
	if _main.menu.visible:
		return "menu"
	return "game"


## The middle of `control`, in pixels of the game's canvas.
func _centre(control: Control) -> Array:
	var to_canvas := control.get_viewport().get_final_transform() * control.get_global_transform_with_canvas()
	var at := to_canvas * (control.size / 2.0)
	return [at.x, at.y]
