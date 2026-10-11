extends Node
## Entry scene: the arena with the host/join menu over it.
##
## Native builds also take `-- --host` or `-- --join=<address>` on the command
## line to skip the menu.

@onready var world: Node3D = $World
@onready var menu: Control = $UI/Menu
@onready var name_edit: LineEdit = %NameEdit
@onready var mode_button: Button = %ModeButton
@onready var max_players_button: Button = %MaxPlayersButton
@onready var kill_limit_button: Button = %KillLimitButton
@onready var minutes_button: Button = %MinutesButton
@onready var map_button: Button = %MapButton
@onready var private_button: Button = %PrivateButton
@onready var host_button: Button = %HostButton
@onready var target_edit: LineEdit = %TargetEdit
@onready var join_button: Button = %JoinButton
@onready var sessions_panel: Control = %SessionsPanel
@onready var sessions: VBoxContainer = %Sessions
@onready var refresh_button: Button = %RefreshButton
@onready var status: Label = %Status
@onready var hud: Label = %Hud
@onready var touch_controls: Control = %TouchControls
@onready var combat_hud: Control = %CombatHud
@onready var waiting_notice: Control = %WaitingNotice
@onready var leave_button: Button = %LeaveButton
@onready var disconnected_notice: Control = %DisconnectedNotice
@onready var disconnected_detail: Label = %DisconnectedDetail
@onready var disconnected_button: Button = %DisconnectedButton
@onready var rejoin_button: Button = %RejoinButton
@onready var final_standings: Label = %FinalStandings
@onready var menu_button: Button = %MenuButton
@onready var pause_menu: Control = %PauseMenu
@onready var resume_button: Button = %ResumeButton
@onready var menu_leave_button: Button = %MenuLeaveButton
@onready var copy_link_button: Button = %CopyLinkButton
@onready var settings_button: Button = %SettingsButton
@onready var pause_settings_button: Button = %PauseSettingsButton
@onready var settings_menu: Control = %SettingsMenu

# Each match setting's button, choices and description, by setting.
var _match_buttons := {}
# Whether the mouse was captured as of the last frame. Esc while it is captured
# only frees the mouse; the press after that opens the menu.
var _was_captured := false
## The invite link last copied, for the browser tests.
var copied_link := ""


func _ready() -> void:
	Net.session_started.connect(_on_session_started)
	Net.session_failed.connect(_on_session_failed)
	Net.session_ended.connect(_on_session_ended)
	Net.sessions_listed.connect(_show_sessions)
	Net.host_silent_changed.connect(_on_host_silent_changed)
	leave_button.pressed.connect(Net.leave)
	disconnected_button.pressed.connect(_dismiss_disconnected)
	rejoin_button.pressed.connect(_rejoin)
	host_button.pressed.connect(_host)
	join_button.pressed.connect(func() -> void: _join(target_edit.text))
	target_edit.text_submitted.connect(_join)
	refresh_button.pressed.connect(Net.request_sessions)
	combat_hud.bind(world)
	_setup_match_buttons()
	Settings.changed.connect(_show_match_setting)
	menu_button.pressed.connect(open_pause_menu)
	resume_button.pressed.connect(close_pause_menu)
	menu_leave_button.pressed.connect(_leave)
	copy_link_button.pressed.connect(_copy_link)
	settings_button.pressed.connect(settings_menu.open)
	pause_settings_button.pressed.connect(settings_menu.open)
	Controls.apply(Settings.get_value("bindings"))
	WebPage.set_fullscreen_on_touch(Settings.get_value("touch_fullscreen"))
	Settings.changed.connect(_apply_page_setting)
	name_edit.text = Settings.get_value("name")
	name_edit.text_submitted.connect(func(_text: String) -> void: _remember_name())
	name_edit.focus_exited.connect(_remember_name)

	# Session codes and the session list only exist with the signaling server,
	# and links only in a browser.
	sessions_panel.visible = Net.use_webrtc
	private_button.visible = Net.use_webrtc
	copy_link_button.visible = Net.use_webrtc and OS.has_feature("web")
	if not Net.use_webrtc:
		target_edit.placeholder_text = "Host address (blank for this PC)"
	Net.request_sessions()
	Sounds.play_music()
	if TestHook.wanted():
		add_child(TestHook.new(self))

	var invited := WebPage.take_invite_code()
	if not invited.is_empty():
		target_edit.text = invited
		_join(invited)

	for arg in OS.get_cmdline_user_args():
		if arg == "--host":
			_host()
		elif arg.begins_with("--join="):
			_join(arg.trim_prefix("--join="))


func _process(_delta: float) -> void:
	if hud.visible:
		hud.text = "Session %s  ·  %d playing" % [Net.room_code, world.players.get_child_count()]
	_was_captured = Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
	menu_button.visible = hud.visible and not _was_captured and not pause_menu.visible


func _input(event: InputEvent) -> void:
	if not hud.visible or waiting_notice.visible or settings_menu.visible:
		return
	var toggle := event.is_action_pressed("pause_menu")
	if event.is_action_pressed("ui_cancel"):
		toggle = toggle or (not _was_captured and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED)
	if toggle:
		get_viewport().set_input_as_handled()
		if pause_menu.visible:
			close_pause_menu()
		else:
			open_pause_menu()


## Opens the in-game menu. The game carries on, but this player's character
## stands still while it is open.
func open_pause_menu() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	pause_menu.show()
	resume_button.grab_focus()


func close_pause_menu() -> void:
	pause_menu.hide()


func is_pause_menu_open() -> bool:
	return pause_menu.visible


## Keeps the name typed in the menu, cleaned, or puts back the last one if
## nothing usable was typed.
func _remember_name() -> void:
	var cleaned := PlayerName.clean(name_edit.text)
	if not cleaned.is_empty():
		Settings.set_value("name", cleaned)
	name_edit.text = Settings.get_value("name")


# Each match setting is a button that steps through its choices when pressed,
# which suits a phone as well as a mouse.
func _setup_match_buttons() -> void:
	var player_counts := range(MatchSettings.MIN_PLAYERS, MatchSettings.MAX_PLAYERS + 1)
	_cycle(mode_button, "host_mode", MatchSettings.MODES, func(mode: String) -> String: return MatchSettings.MODE_NAMES[mode])
	_cycle(max_players_button, "host_max_players", player_counts, func(count: int) -> String: return "Up to %d players" % count)
	_cycle(kill_limit_button, "host_kill_limit", MatchSettings.KILL_LIMITS, func(limit: int) -> String: return "First to %d" % limit)
	_cycle(minutes_button, "host_minutes", MatchSettings.MINUTES, func(minutes: int) -> String: return "%d minutes" % minutes)
	_cycle(map_button, "host_map", Maps.ORDER, func(map: String) -> String: return "Map: %s" % Maps.NAMES[map])
	_cycle(private_button, "host_private", [false, true], func(hidden: bool) -> String: return "Private: invite only" if hidden else "Listed for everyone")


func _cycle(button: Button, key: String, choices: Array, describe: Callable) -> void:
	_match_buttons[key] = {"button": button, "choices": choices, "describe": describe}
	_show_match_setting(key)
	button.pressed.connect(func() -> void:
		Settings.set_value(key, choices[(choices.find(Settings.get_value(key)) + 1) % choices.size()]))


func _apply_page_setting(key: String) -> void:
	if key == "touch_fullscreen":
		WebPage.set_fullscreen_on_touch(Settings.get_value("touch_fullscreen"))


func _show_match_setting(key: String) -> void:
	if not _match_buttons.has(key):
		return
	var cycle: Dictionary = _match_buttons[key]
	if not cycle.choices.has(Settings.get_value(key)):
		Settings.set_value(key, Settings.DEFAULTS[key])
	cycle.button.text = cycle.describe.call(Settings.get_value(key))


## The match settings chosen on the menu.
func host_settings() -> MatchSettings:
	return MatchSettings.from_dict({
		"mode": Settings.get_value("host_mode"),
		"kill_limit": Settings.get_value("host_kill_limit"),
		"minutes": Settings.get_value("host_minutes"),
		"max_players": Settings.get_value("host_max_players"),
		"map": Settings.get_value("host_map"),
	})


func _host() -> void:
	_remember_name()
	_set_busy("Starting session…")
	Net.host(host_settings(), Settings.get_value("host_private"))


func _join(target: String) -> void:
	if Net.use_webrtc and target.strip_edges().is_empty():
		status.text = "Enter a session code."
		return
	_remember_name()
	_set_busy("Joining…")
	Net.join(target)


## Puts a link to the session on the clipboard, and says so on the button.
func _copy_link() -> void:
	copied_link = Invite.link(WebPage.address(), Net.room_code)
	DisplayServer.clipboard_set(copied_link)
	copy_link_button.text = "Link copied!"
	await get_tree().create_timer(2.0).timeout
	copy_link_button.text = "Copy invite link"


func _set_busy(message: String) -> void:
	status.text = message
	host_button.disabled = true
	join_button.disabled = true


func _on_session_started() -> void:
	Sounds.set_music_quiet(true)
	menu.hide()
	hud.show()
	combat_hud.show()
	touch_controls.show()
	world.start(host_settings())


func _on_host_silent_changed(silent: bool) -> void:
	waiting_notice.visible = silent
	if silent:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _on_session_failed(reason: String) -> void:
	_show_menu(reason)


## How a session that has ended stood: the result if a match had just been
## won, then the best few players, or the teams' scores.
static func standings(state: MatchState) -> String:
	if state.players.is_empty():
		return ""
	var lines: Array[String] = []
	if not state.is_playing():
		lines.append(state.result_text())
	if state.settings.is_teams():
		lines.append("Cheddar %d  –  %d Brie" % [state.team_kills[MatchState.CHEDDAR], state.team_kills[MatchState.BRIE]])
	var rows := preload("res://ui/scoreboard.gd").rows(state)
	if state.settings.is_teams():
		rows.sort_custom(func(a: PlayerRecord, b: PlayerRecord) -> bool: return a.kills > b.kills)
	for place in mini(rows.size(), 5):
		lines.append("%d. %s  ·  %d" % [place + 1, rows[place].name, rows[place].kills])
	return "\n".join(lines)


## Leaves the session. A host first tells everyone the session is over, so
## they see why, and gives that a moment to arrive.
func _leave() -> void:
	if world.match_.is_host() and not multiplayer.get_peers().is_empty():
		world.match_.end_session()
		await get_tree().create_timer(0.3).timeout
	Net.leave()


func _on_session_ended(reason: String) -> void:
	final_standings.text = standings(world.match_.state)
	world.stop()
	pause_menu.hide()
	combat_hud.hide()
	combat_hud.reset()
	touch_controls.hide()
	if reason.is_empty():
		_show_menu("")
		return
	hud.hide()
	disconnected_detail.text = reason
	rejoin_button.visible = can_rejoin(reason, Net.last_joined)
	disconnected_notice.show()
	disconnected_button.grab_focus()


## Whether a player who lost their session for `reason` can try joining it
## again: a joiner whose connection failed, not one whose host closed it.
static func can_rejoin(reason: String, joined: String) -> bool:
	return not joined.is_empty() and not reason.is_empty() and not reason in ["The host closed the session.", Match.HOST_ENDED]


func _rejoin() -> void:
	disconnected_notice.hide()
	menu.show()
	_join(Net.last_joined)


func _dismiss_disconnected() -> void:
	disconnected_notice.hide()
	_show_menu("")


func _show_menu(message: String) -> void:
	Sounds.set_music_quiet(false)
	pause_menu.hide()
	settings_menu.hide()
	hud.hide()
	combat_hud.hide()
	combat_hud.reset()
	touch_controls.hide()
	menu.show()
	status.text = message
	host_button.disabled = false
	join_button.disabled = false
	Net.request_sessions()


## How a session reads in the session list.
static func session_line(session: Dictionary) -> String:
	var settings := MatchSettings.from_dict(session.get("settings"))
	return "%s  ·  %d/%d  ·  %s  ·  %s" % [
		session.get("code", "?"), int(session.get("players", 0)), settings.max_players, MatchSettings.MODE_NAMES[settings.mode], Maps.NAMES[settings.map]]


func _show_sessions(list: Array) -> void:
	for child in sessions.get_children():
		child.queue_free()
	if list.is_empty():
		var none := Label.new()
		none.text = "None yet. Host one!"
		sessions.add_child(none)
	for session: Dictionary in list:
		var button := Button.new()
		button.text = session_line(session)
		button.pressed.connect(_join.bind(session.code))
		sessions.add_child(button)
