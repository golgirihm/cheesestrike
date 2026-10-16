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
## unless `&help` is added too. Matches begin at once, without counting down
## (see MatchState.COUNTDOWN), unless `&countdown` is added too.

const PUBLISH_INTERVAL := 0.05

var _main: Node
var _since_publish := 0.0


static func wanted() -> bool:
	return OS.has_feature("web") and JavaScriptBridge.eval("new URLSearchParams(location.search).has('test')")


## How long frames have taken, for tests that look for the game catching.
var frame_times := FrameTimes.new()
var _last_frame := 0


func _init(main: Node) -> void:
	_main = main
	Settings.set_value("seen_help", not JavaScriptBridge.eval("new URLSearchParams(location.search).has('help')"))
	if not JavaScriptBridge.eval("new URLSearchParams(location.search).has('countdown')"):
		MatchState.countdown_length = 0.0


func _ready() -> void:
	# The tests never look at the picture, and drawing it is most of the work
	# for a browser without a graphics card, as on a CI machine.
	if not JavaScriptBridge.eval("new URLSearchParams(location.search).has('draw')"):
		RenderingServer.render_loop_enabled = false


func _process(delta: float) -> void:
	# Timed by the clock: the engine's own count leaves long frames short.
	var now := Time.get_ticks_usec()
	if _last_frame > 0:
		frame_times.add((now - _last_frame) / 1000000.0)
	_last_frame = now
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
			"mine": player._is_own(),
			"name": _main.world.match_.state.name_of(id),
			"position": [at.x, at.y, at.z],
			"health": record.health if record != null else null,
			"armour": record.armour if record != null else null,
			"alive": record.alive if record != null else null,
			"kills": record.kills if record != null else null,
			"team": record.team if record != null else null,
			"watching": record.watching if record != null else null,
			"holding": record.weapon if record != null else null,
			"overload": record.overload if record != null else null,
			"overloaded": record.overload_left > 0.0 if record != null else null,
			"power": Powers.NAMES.get(record.power, "") if record != null else null,
			"deaths": record.deaths if record != null else null,
			"grenades": record.grenades if record != null else null,
			"look": Looks.unpack(record.look) if record != null else null,
			"aiming": player.is_aiming(),
			"doing": player.body.state(),
			"emote": Emotes.NAMES.get(player.body.emoting(), ""),
			"crouching": player.crouch() >= 0.9,
			"leaning": signf(player.lean()) if absf(player.lean()) >= 0.9 else 0.0,
		}
		if player._is_own():
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
		_main.find_child("BotsButton", true, false),
		_main.find_child("BotSkillButton", true, false),
		_main.find_child("RulesButton", true, false),
		_main.find_child("SkyButton", true, false),
		_main.host_button,
		_main.find_child("PracticeButton", true, false),
		_main.find_child("TutorialButton", true, false),
		_main.find_child("PlayButton", true, false),
		_main.find_child("ProfileButton", true, false),
		_main.find_child("ProfileDone", true, false),
		_main.find_child("Profile_record", true, false),
		_main.find_child("Profile_matches", true, false),
		_main.find_child("CustomiseButton", true, false),
		_main.find_child("TitleSettingsButton", true, false),
		_main.find_child("CreditsButton", true, false),
		_main.find_child("CreditsBackButton", true, false),
		_main.find_child("BackButton", true, false),
		_main.find_child("EditorButton", true, false),
		_main.target_edit,
		_main.join_button,
		_main.find_child("WatchButton", true, false),
		_main.find_child("WatchToggleButton", true, false),
		_main.find_child("RemoveChoiceButton", true, false),
		_main.find_child("RemoveVoteButton", true, false),
		_main.find_child("MuteChoiceButton", true, false),
		_main.find_child("MuteButton", true, false),
		_main.refresh_button,
		_main.play_now_button,
		_main.sort_button,
		_main.mode_filter_button,
		_main.arena_filter_button,
		_main.hide_full_button,
		_main.leave_button,
		_main.disconnected_button,
		_main.rejoin_button,
		_main.resume_button,
		_main.menu_leave_button,
		_main.settings_button,
		_main.pause_settings_button,
		_main.help_button,
		_main.arena_editor.find_child("EditorDone", true, false),
		_main.arena_editor.find_child("EditorCopy", true, false),
		_main.arena_editor.find_child("EditorClear", true, false),
	]:
		if control.is_visible_in_tree():
			controls[control.name] = _centre(control)

	return {
		"screen": _screen(),
		# The group of settings whose tab is chosen, on the settings screen.
		"settings_group": _main.settings_menu.group if _main.settings_menu.visible else "",
		"status": _main.status.text,
		"frame": Engine.get_process_frames(),
		"name_box": _main.name_edit.text,
		"match_buttons": [_main.mode_button.text, _main.max_players_button.text, _main.kill_limit_button.text, _main.minutes_button.text, _main.map_button.text, _main.private_button.text, (_main.find_child("BotsButton", true, false) as Button).text, (_main.find_child("BotSkillButton", true, false) as Button).text, (_main.find_child("RulesButton", true, false) as Button).text, (_main.find_child("SkyButton", true, false) as Button).text],
		"map": _main.world.map_id,
		"mouse_captured": Input.mouse_mode == Input.MOUSE_MODE_CAPTURED,
		"hud": _main.hud.text if _main.hud.visible else "",
		"code": Net.room_code,
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
			"shadows": GraphicsQuality.choice(GraphicsQuality.SHADOWS),
			"music_volume": Settings.get_value("music_volume"),
			"jump_keys": Controls.keys_text("jump"),
			"captions": Settings.get_value("captions"),
		},
		"music": Sounds.is_music_playing(),
		"replays_seen": _main.world.replay_view.played,
		"sky": _main.world.atmosphere.showing,
		"profile": _main.profile_screen.showing() if _main.profile_screen.visible else {},
		"progress": {"xp": Progress.xp, "level": Progress.level(), "matches": Progress.record.matches, "history": Progress.history.size()},
		"photo": {"on": _main.world.photo_mode.on, "saved": _main.world.photo_mode.saved_count},
		"weapon_bar": _main.combat_hud.weapon_bar.shown().map(func(entry: Dictionary) -> String: return "%d %s%s" % [entry.key, entry.weapon, "*" if entry.held else ""]),
		"weapon_wheel": _main.combat_hud.weapon_wheel.choices if _main.combat_hud.weapon_wheel.visible else [],
		"pings": _main.world.pings.shown().map(func(ping: Dictionary) -> Dictionary: return {"by": ping.by, "kind": ping.kind}),
		"chat": {"lines": _main.combat_hud.chat_box.lines(), "typing": _main.combat_hud.chat_box.is_typing(), "wheel": _main.combat_hud.chat_box.wheel.visible},
		"compass": _main.combat_hud.compass.shown().map(func(mark: Dictionary) -> String: return mark.kind),
		"loading": _main.loading_seen,
		"joining": _main.join_screen.showing() if _main.join_screen.visible else {},
		"voice": {"sent": _main.world.voice_chat.sent, "heard": _main.world.voice_chat.heard, "talking": _main.world.voice_chat.talking(), "muted": _main.world.voice_chat.muted.keys(),
			"unheard": _main.world.match_.state.players.keys().filter(func(id: int) -> bool: return _main.world.voice_chat.is_muted(id)) if _main.world.match_.has_state() else []},
		"announcer": _main.world.announcer.spoken.slice(-6),
		"frames": {"typical": frame_times.typical(), "now": frame_times.now(), "seconds": frame_times.seconds},
		"hurt_view": _main.world.hurt_view.showing(),
		"editor": _main.arena_editor.text() if _main.arena_editor.visible else [],
		"editor_link": _main.arena_editor.link() if _main.arena_editor.visible else "",
		"custom_arena": Settings.get_value("custom_arena"),
		"arena_shown": _main.world.custom_code if _main.world.map_id == Maps.CUSTOM else "",
		"doors": _main.world.match_.state.doors,
		"doors_shown": _main.world.doors_shown(),
		"blocks": _main.world.blocks_standing(),
		"drops": _main.world.match_.state.drops.map(func(lying: Dictionary) -> String: return lying.weapon) if _main.world.drops_shown() == _main.world.match_.state.drops.size() else null,
		"pickups": range(_main.world.pickup_pads().size()).map(func(index: int) -> bool: return _main.world.match_.state.pickup_ready(index)),
		"combat": {
			"health": _main.combat_hud.health_text() if _main.combat_hud.visible else "",
			"ammo": _main.combat_hud.ammo_shown() if _main.combat_hud.visible else "",
			"reserve": _main.combat_hud.reserve_shown() if _main.combat_hud.visible else "",
			"tally": _main.combat_hud.tally_shown() if _main.combat_hud.visible else "",
			"weapon": _main.combat_hud.weapon_shown() if _main.combat_hud.visible else "",
			"overload": _main.combat_hud.overload_shown() if _main.combat_hud.visible else "",
			"objective": _main.combat_hud.objective_shown() if _main.combat_hud.visible else "",
			"points": [_main.world.match_.state.team_score(0), _main.world.match_.state.team_score(1)],
			"place": _main.combat_hud.place_shown() if _main.combat_hud.visible else "",
			"vote": _main.combat_hud.vote_shown() if _main.combat_hud.visible else [],
			"replay": _main.combat_hud.replay_shown() if _main.combat_hud.visible else "",
			"replay_ghosts": _main.world.replay_view.ghosts(),
			"watch": _main.combat_hud.watching_shown() if _main.combat_hud.visible else "",
			"tutorial": _main.combat_hud.tutorial_shown() if _main.combat_hud.visible else "",
			"practice": _main.combat_hud.practice_shown() if _main.combat_hud.visible else [],
			"announcement": _main.combat_hud.announcement_shown() if _main.combat_hud.visible else "",
			"power": _main.combat_hud.power_shown() if _main.combat_hud.visible else "",
			"hits_shown": _main.combat_hud.hits_shown,
			"captions": _main.combat_hud.captions.showing(),
			"hurts_shown": _main.combat_hud.hurts_shown,
			"pickups_shown": _main.combat_hud.pickups_shown,
			"knocked_out": _main.combat_hud.knocked_out_shown() if _main.combat_hud.visible else "",
			"match": _main.combat_hud.match_shown() if _main.combat_hud.visible else "",
			"result": _main.combat_hud.result_shown() if _main.combat_hud.visible else "",
			"summary": _main.combat_hud.summary_shown() if _main.combat_hud.visible else [],
			"medal": _main.combat_hud.medal_shown(),
			"medals": _main.combat_hud.medals.earned,
			"kill_feed": _main.combat_hud.kill_feed.showing(),
			"knockouts_announced": _main.combat_hud.kill_feed.announced,
			"latest_knockout": _main.combat_hud.kill_feed.latest,
			"scoreboard": _main.combat_hud.scoreboard.rows_shown() if _main.combat_hud.scoreboard.is_visible_in_tree() else [],
		},
		"sessions": sessions,
		"controls": controls,
	}


func _screen() -> String:
	if _main.loading_cover.visible:
		return "loading"
	if _main.join_screen.visible:
		return "joining"
	if _main.profile_screen.visible:
		return "profile"
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
	if _main.front_end.visible:
		return "credits" if _main.front_end.credits.visible else "title"
	if _main.arena_editor.visible:
		return "editor"
	return "game"


## The middle of `control`, in pixels of the game's canvas.
func _centre(control: Control) -> Array:
	var to_canvas := control.get_viewport().get_final_transform() * control.get_global_transform_with_canvas()
	var at := to_canvas * (control.size / 2.0)
	return [at.x, at.y]
