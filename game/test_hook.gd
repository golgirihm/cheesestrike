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
## then stops drawing, so `?test` is no use for looking at it, unless `&draw`
## is added too, as for taking screenshots. Windows start as players who have
## seen the controls help, so it doesn't cover what a test came to click,
## unless `&help` is added too.

const PUBLISH_INTERVAL := 0.05

var _main: Node
var _since_publish := 0.0


static func wanted() -> bool:
	return OS.has_feature("web") and JavaScriptBridge.eval("new URLSearchParams(location.search).has('test')")


func _init(main: Node) -> void:
	_main = main
	Settings.set_value("seen_help", not JavaScriptBridge.eval("new URLSearchParams(location.search).has('help')"))


func _ready() -> void:
	# The tests never look at the picture, and drawing it is most of the work
	# for a browser without a graphics card, as on a CI machine.
	if not JavaScriptBridge.eval("new URLSearchParams(location.search).has('draw')"):
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
		var id := player.name.to_int()
		var record: PlayerRecord = _main.world.match_.state.player(id)
		var described := {
			"id": id,
			"mine": player.is_multiplayer_authority(),
			"name": _main.world.match_.state.name_of(id),
			"position": [at.x, at.y, at.z],
			"health": record.health if record != null else null,
			"alive": record.alive if record != null else null,
			"kills": record.kills if record != null else null,
			"team": record.team if record != null else null,
			"holding": record.weapon if record != null else null,
			"overload": record.overload if record != null else null,
			"overloaded": record.overload_left > 0.0 if record != null else null,
			"deaths": record.deaths if record != null else null,
			"grenades": record.grenades if record != null else null,
			"aiming": player.is_aiming(),
			"crouching": player.crouch() >= 0.9,
		}
		if player.is_multiplayer_authority():
			described.ammo = player.weapon.ammo
			described.reserve = player.weapon.reserve
			described.reloads = player.weapon.reloads
			described.weapon = player.weapon.stats.id
			described.carrying = player.arsenal.keys()
			described.facing = [player.rotation.y, player.head.rotation.x]
			described.reloading = player.weapon.is_reloading()
		players.append(described)

	# Each line of the session list: a session to click, or the note shown when
	# there are none.
	var sessions := []
	for line: Control in _main.sessions.get_children():
		if not line.is_queued_for_deletion():
			sessions.append({"text": line.text, "at": _centre(line)})

	var controls := {}
	if _main.controls_help.visible:
		controls["HelpDone"] = _centre(_main.controls_help.find_child("HelpDone", true, false))
	if _main.settings_menu.visible:
		for control in _main.settings_menu.controls():
			controls[control.name] = _centre(control)
	for control: Control in [
		_main.name_edit,
		_main.mode_button,
		_main.max_players_button,
		_main.kill_limit_button,
		_main.minutes_button,
		_main.map_button,
		_main.private_button,
		_main.host_button,
		_main.target_edit,
		_main.join_button,
		_main.refresh_button,
		_main.leave_button,
		_main.disconnected_button,
		_main.rejoin_button,
		_main.menu_button,
		_main.resume_button,
		_main.menu_leave_button,
		_main.settings_button,
		_main.pause_settings_button,
		_main.copy_link_button,
		_main.help_button,
	]:
		if control.is_visible_in_tree():
			controls[control.name] = _centre(control)

	return {
		"screen": _screen(),
		"status": _main.status.text,
		"frame": Engine.get_process_frames(),
		"name_box": _main.name_edit.text,
		"match_buttons": [_main.mode_button.text, _main.max_players_button.text, _main.kill_limit_button.text, _main.minutes_button.text, _main.map_button.text, _main.private_button.text],
		"map": _main.world.map_id,
		"mouse_captured": Input.mouse_mode == Input.MOUSE_MODE_CAPTURED,
		"hud": _main.hud.text if _main.hud.visible else "",
		"code": Net.room_code,
		"copied_link": _main.copied_link,
		"help": _main.controls_help.shown() if _main.controls_help.visible else [],
		"address": WebPage.address(),
		"notice": _main.disconnected_detail.text if _main.disconnected_notice.visible else "",
		"standings": _main.final_standings.text if _main.disconnected_notice.visible else "",
		"players": players,
		"sounds": Sounds.counts,
		"put_backs": _main.world.match_.put_backs,
		"projectiles": _main.world.effects.projectiles_in_flight(),
		"settings": {
			"mouse_sensitivity": Settings.get_value("mouse_sensitivity"),
			"invert_look": Settings.get_value("invert_look"),
			"fov": Settings.get_value("fov"),
			"quality": GraphicsQuality.level(Settings.get_value("quality")),
			"music_volume": Settings.get_value("music_volume"),
			"jump_keys": Controls.keys_text("jump"),
			"captions": Settings.get_value("captions"),
		},
		"music": Sounds.is_music_playing(),
		"pickups": range(_main.world.pickup_pads().size()).map(func(index: int) -> bool: return _main.world.match_.state.pickup_ready(index)),
		"combat": {
			"health": _main.combat_hud.health_text() if _main.combat_hud.visible else "",
			"ammo": _main.combat_hud.ammo_shown() if _main.combat_hud.visible else "",
			"reserve": _main.combat_hud.reserve_shown() if _main.combat_hud.visible else "",
			"tally": _main.combat_hud.tally_shown() if _main.combat_hud.visible else "",
			"weapon": _main.combat_hud.weapon_shown() if _main.combat_hud.visible else "",
			"overload": _main.combat_hud.overload_shown() if _main.combat_hud.visible else "",
			"hits_shown": _main.combat_hud.hits_shown,
			"captions": _main.combat_hud.captions.showing(),
			"hurts_shown": _main.combat_hud.hurts_shown,
			"pickups_shown": _main.combat_hud.pickups_shown,
			"knocked_out": _main.combat_hud.knocked_out_shown() if _main.combat_hud.visible else "",
			"match": _main.combat_hud.match_shown() if _main.combat_hud.visible else "",
			"result": _main.combat_hud.result_shown() if _main.combat_hud.visible else "",
			"kill_feed": _main.combat_hud.kill_feed.showing(),
			"knockouts_announced": _main.combat_hud.kill_feed.announced,
			"latest_knockout": _main.combat_hud.kill_feed.latest,
			"scoreboard": _main.combat_hud.scoreboard.rows_shown() if _main.combat_hud.scoreboard.is_visible_in_tree() else [],
		},
		"sessions": sessions,
		"controls": controls,
	}


func _screen() -> String:
	if _main.controls_help.visible:
		return "help"
	if _main.settings_menu.visible:
		return "settings"
	if _main.disconnected_notice.visible:
		return "disconnected"
	if _main.waiting_notice.visible:
		return "waiting"
	if _main.pause_menu.visible:
		return "paused"
	if _main.menu.visible:
		return "menu"
	return "game"


## The middle of `control`, in pixels of the game's canvas.
func _centre(control: Control) -> Array:
	var to_canvas := control.get_viewport().get_final_transform() * control.get_global_transform_with_canvas()
	var at := to_canvas * (control.size / 2.0)
	return [at.x, at.y]
