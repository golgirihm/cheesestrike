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
@onready var help_button: Button = %HelpButton
@onready var controls_help: Control = %ControlsHelp
@onready var settings_button: Button = %SettingsButton
@onready var pause_settings_button: Button = %PauseSettingsButton
@onready var settings_menu: Control = %SettingsMenu

# Each match setting's button, choices and description, by setting.
var _match_buttons := {}
# The settings of a session of this machine alone, such as the practice
# range's, while one is being started; otherwise null.
var _alone_settings: MatchSettings
## The tutorial the player is being taken through, or null.
var tutorial: Tutorial
var arena_editor: Control
## The title screen.
var front_end: FrontEnd
var _mute_target := 0
## Milliseconds of each frame spent making sounds while the game gets ready.
const SOUND_MAKING_MS := 12
## Covers the view while the game gets ready (see Warmup): until one of
## everything has been drawn, and every sound has been made.
var loading_cover: ColorRect
var _drawn_once := false
## The bar on the cover, and every amount it has shown, for tests.
var loading_bar: ProgressBar
var loading_seen: Array[float] = []
## What is shown while a session is being joined or started.
var join_screen: JoinScreen
## The player's own page.
var profile_screen: ProfileScreen
# The sessions last listed, to know a session's arena before joining it.
var _listed: Array = []
## The button that joins the best session listed, or hosts one, and those
## that sort and filter the list.
var play_now_button: Button
var sort_button: Button
var mode_filter_button: Button
var arena_filter_button: Button
var hide_full_button: CheckBox
var _list_sort := SessionList.FULLEST
# Seconds until a dropped session is tried again, or below 0 for none; how
# many times it has been tried since the last that was joined, and at most.
var _reconnect_left := -1.0
var _auto_rejoins := 0
var _reason_shown := ""
const RECONNECT_WAIT := 3.0
const AUTO_REJOINS := 1
var _list_mode := SessionList.ANY
var _list_arena := SessionList.ANY
# Who the pause menu's vote-to-remove button names.
var _remove_target := 0
# Whether the mouse was captured as of the last frame. Esc while it is captured
# only frees the mouse; the press after that opens the menu.
var _was_captured := false
## The invite link last copied, for the browser tests.
var copied_link := ""


func _ready() -> void:
	# The first of everything is drawn behind a cover, before anyone plays.
	loading_cover = ColorRect.new()
	loading_cover.name = "LoadingCover"
	loading_cover.color = Color(0.09, 0.08, 0.07)
	loading_cover.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var getting_ready := Label.new()
	getting_ready.text = "Getting ready…"
	getting_ready.add_theme_font_size_override("font_size", 28)
	getting_ready.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	getting_ready.grow_horizontal = Control.GROW_DIRECTION_BOTH
	loading_cover.add_child(getting_ready)
	loading_bar = ProgressBar.new()
	loading_bar.name = "LoadingBar"
	loading_bar.theme = Look.theme()
	loading_bar.show_percentage = false
	loading_bar.max_value = 1.0
	loading_bar.custom_minimum_size = Vector2(280, 14)
	loading_bar.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	loading_bar.grow_horizontal = Control.GROW_DIRECTION_BOTH
	loading_bar.position.y += 34
	loading_cover.add_child(loading_bar)
	menu.get_parent().add_child(loading_cover)
	world.warmed.connect(func() -> void: _drawn_once = true)
	world.warm_up.call_deferred()
	Net.session_started.connect(_on_session_started)
	Net.session_failed.connect(_on_session_failed)
	Net.session_ended.connect(_on_session_ended)
	Net.sessions_listed.connect(_show_sessions)
	Net.host_silent_changed.connect(_on_host_silent_changed)
	leave_button.pressed.connect(Net.leave)
	disconnected_button.pressed.connect(_dismiss_disconnected)
	rejoin_button.pressed.connect(_rejoin)
	host_button.pressed.connect(_host)
	_add_session_controls()
	# The whole interface has one look (see Look).
	for screen in menu.get_parent().get_children():
		if screen is Control:
			screen.theme = Look.theme()
	Look.heading(menu.find_child("Title", true, false))
	# The title screen, which leads to everything else.
	front_end = FrontEnd.new()
	menu.get_parent().add_child(front_end)
	menu.get_parent().move_child(front_end, menu.get_index())
	front_end.chosen.connect(_on_way_in_chosen)
	join_screen = JoinScreen.new()
	menu.get_parent().add_child(join_screen)
	profile_screen = ProfileScreen.new()
	menu.get_parent().add_child(profile_screen)
	arena_editor = preload("res://ui/arena_editor.gd").new()
	arena_editor.name = "ArenaEditor"
	arena_editor.theme = Look.theme()
	menu.get_parent().add_child(arena_editor)
	arena_editor.closed.connect(func() -> void:
		_show_match_setting("host_map")
		_show_menu(""))
	# From hosting and joining, the way back to the title screen.
	var back := Button.new()
	back.name = "BackButton"
	back.text = "Back"
	back.icon = Icons.texture("back", 22, Look.CREAM)
	back.custom_minimum_size = host_button.custom_minimum_size
	# Beside the button that hosts, where "Settings" is, which the title
	# screen has now.
	settings_button.add_sibling(back)
	settings_button.hide()
	back.pressed.connect(show_title)
	Look.emphasise(host_button)
	join_button.pressed.connect(func() -> void: _join(target_edit.text))
	# Whether to join to play or to watch, and the same choice once in.
	var watch_button := Button.new()
	watch_button.name = "WatchButton"
	watch_button.custom_minimum_size = join_button.custom_minimum_size
	join_button.add_sibling(watch_button)
	_cycle(watch_button, "join_watching", [false, true], func(watching: bool) -> String: return "Joining to watch" if watching else "Joining to play")
	var watch_toggle := Button.new()
	watch_toggle.name = "WatchToggleButton"
	watch_toggle.custom_minimum_size = resume_button.custom_minimum_size
	resume_button.add_sibling(watch_toggle)
	# Asking for a vote on removing someone: one button names who, stepping
	# through the others each time it is pressed, and one asks.
	var remove_choice := Button.new()
	remove_choice.name = "RemoveChoiceButton"
	var remove_ask := Button.new()
	remove_ask.name = "RemoveVoteButton"
	remove_ask.text = "Ask for the vote"
	for added: Button in [remove_ask, remove_choice]:
		added.custom_minimum_size = resume_button.custom_minimum_size
		watch_toggle.add_sibling(added)
	# Muting someone's talk: the same two buttons again.
	var mute_choice := Button.new()
	mute_choice.name = "MuteChoiceButton"
	var mute_toggle := Button.new()
	mute_toggle.name = "MuteButton"
	# Once muted, the same button blocks them in every session, and then lets
	# them be heard again.
	mute_toggle.tooltip_text = "Muting stops their talk and chat here; blocking stops it in every session"
	for added: Button in [mute_toggle, mute_choice]:
		added.custom_minimum_size = resume_button.custom_minimum_size
		remove_choice.add_sibling(added)
	mute_choice.pressed.connect(func() -> void:
		var others := others_here()
		_mute_target = others[(others.find(_mute_target) + 1) % others.size()] if not others.is_empty() else 0)
	mute_toggle.pressed.connect(func() -> void:
		var record: PlayerRecord = world.match_.state.player(_mute_target)
		var tag := record.tag if record != null else ""
		if Progress.is_blocked(tag):
			Progress.set_blocked(tag, "", false)
			world.voice_chat.set_muted(_mute_target, false)
		elif world.voice_chat.is_muted(_mute_target) and not tag.is_empty():
			Progress.set_blocked(tag, record.name, true)
		else:
			world.voice_chat.set_muted(_mute_target, not world.voice_chat.is_muted(_mute_target)))
	remove_choice.pressed.connect(func() -> void:
		var others := removable()
		_remove_target = others[(others.find(_remove_target) + 1) % others.size()] if not others.is_empty() else 0)
	remove_ask.pressed.connect(func() -> void:
		world.match_.propose_removal(_remove_target)
		close_pause_menu())
	watch_toggle.pressed.connect(func() -> void:
		var own: PlayerRecord = world.match_.own_record()
		if own != null:
			world.match_.watch(not own.watching)
		close_pause_menu())
	target_edit.text_submitted.connect(_join)
	refresh_button.pressed.connect(Net.request_sessions)
	combat_hud.bind(world)
	# With the camera free for a picture, nothing is drawn over the view.
	world.photo_mode.changed.connect(func() -> void:
		var showing: bool = not world.photo_mode.on and world.match_.has_state() and not menu.visible and not front_end.visible
		combat_hud.visible = showing
		hud.visible = showing
		touch_controls.visible = showing)
	_setup_match_buttons()
	Settings.changed.connect(_show_match_setting)
	menu_button.pressed.connect(open_pause_menu)
	resume_button.pressed.connect(close_pause_menu)
	menu_leave_button.pressed.connect(_leave)
	copy_link_button.pressed.connect(_copy_link)
	help_button.pressed.connect(_show_help_from_menu)
	settings_button.pressed.connect(settings_menu.open)
	pause_settings_button.pressed.connect(settings_menu.open)
	Controls.apply(Settings.get_value("bindings"))
	WebPage.set_fullscreen_on_touch(Settings.get_value("touch_fullscreen"))
	Settings.changed.connect(_apply_page_setting)
	FramePace.apply()
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

	# A link to an arena someone made: kept, and offered to host.
	var arena := WebPage.take_arena_code()
	if not arena.is_empty():
		Settings.set_value("custom_arena", arena)
		Settings.set_value("host_map", Maps.CUSTOM)
		status.text = "The arena from the link is ready: host it, or change it in the editor."

	var invited := WebPage.take_invite_code()
	if not invited.is_empty():
		target_edit.text = invited
		_join(invited)

	# Someone who came by a link to an arena or a session is taken straight
	# to it; anyone else starts at the title screen.
	if arena.is_empty() and invited.is_empty():
		show_title()
	else:
		front_end.hide()

	for arg in OS.get_cmdline_user_args():
		if arg == "--host":
			_host()
		elif arg.begins_with("--join="):
			_join(arg.trim_prefix("--join="))


func _process(_delta: float) -> void:
	if _reconnect_left > 0.0:
		_reconnect_left -= _delta
		if not disconnected_notice.visible:
			_reconnect_left = -1.0
		elif _reconnect_left <= 0.0:
			_rejoin()
		else:
			disconnected_detail.text = reconnect_text(_reason_shown, _reconnect_left)
	# A few sounds are made each frame behind the cover, which goes once
	# they all are and one of everything has been drawn.
	if loading_cover.visible:
		var all_made := SoundBank.make_some(SOUND_MAKING_MS)
		loading_bar.value = loading_progress(SoundBank.made_share(), _drawn_once)
		if not loading_seen.has(snappedf(loading_bar.value, 0.01)):
			loading_seen.append(snappedf(loading_bar.value, 0.01))
		if all_made and _drawn_once:
			loading_cover.hide()
	# While joining, the screen that says where to; it goes once the arena
	# is there to stand in.
	if join_screen.visible and hud.visible:
		if world.match_.has_state():
			join_screen.set_arena(world.match_.state.settings.map)
		if world.ready_to_play():
			join_screen.hide()
	# Behind the title screen the arena is looked at from a view that drifts.
	if front_end.visible:
		world.drift_menu_view(Time.get_ticks_msec() / 1000.0)
	if hud.visible:
		hud.text = "Practice range" if Net.alone else "Session %s  ·  %d playing" % [Net.room_code, world.players.get_child_count()]
		# Who is heard talking, by name.
		var talking: Array = world.voice_chat.talking().map(func(id: int) -> String: return world.match_.state.name_of(id))
		if not talking.is_empty():
			hud.text += "\nTalking: %s" % ", ".join(talking)
	if tutorial != null and hud.visible:
		_run_tutorial()
	if pause_menu.visible:
		var own: PlayerRecord = world.match_.own_record()
		var toggle: Button = pause_menu.find_child("WatchToggleButton", true, false)
		toggle.text = "Play" if own != null and own.watching else "Watch instead"
		toggle.visible = not Net.alone
		var others := removable()
		if not others.has(_remove_target):
			_remove_target = others[0] if not others.is_empty() else 0
		var choice: Button = pause_menu.find_child("RemoveChoiceButton", true, false)
		var ask: Button = pause_menu.find_child("RemoveVoteButton", true, false)
		choice.visible = not others.is_empty()
		ask.visible = choice.visible
		choice.text = "Vote to remove: %s" % world.match_.state.name_of(_remove_target)
		var talkers := others_here()
		if not talkers.has(_mute_target):
			_mute_target = talkers[0] if not talkers.is_empty() else 0
		var mute_choice: Button = pause_menu.find_child("MuteChoiceButton", true, false)
		var mute_toggle: Button = pause_menu.find_child("MuteButton", true, false)
		mute_choice.visible = not talkers.is_empty()
		mute_toggle.visible = mute_choice.visible
		mute_choice.text = "Talk from: %s" % world.match_.state.name_of(_mute_target)
		var target: PlayerRecord = world.match_.state.player(_mute_target)
		var tag := target.tag if target != null else ""
		if Progress.is_blocked(tag):
			mute_toggle.text = "Blocked  ·  unblock them"
		elif world.voice_chat.is_muted(_mute_target):
			mute_toggle.text = "Muted  ·  block for good" if not tag.is_empty() else "Unmute them"
		else:
			mute_toggle.text = "Mute them"
	_was_captured = Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
	menu_button.visible = hud.visible and not _was_captured and not pause_menu.visible


func _input(event: InputEvent) -> void:
	if not hud.visible or waiting_notice.visible or settings_menu.visible or controls_help.visible:
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
	_cycle(map_button, "host_map", Maps.ORDER + [Maps.CUSTOM], func(map: String) -> String:
		var none: bool = map == Maps.CUSTOM and ArenaCode.decode(Settings.get_value("custom_arena")).is_empty()
		return "Map: %s%s" % [Maps.NAMES[map], " (none made yet)" if none else ""])
	# Computer players, to fill a session or to practise against.
	var bots_button := Button.new()
	bots_button.name = "BotsButton"
	var skill_button := Button.new()
	skill_button.name = "BotSkillButton"
	# Each goes straight after the map's button, so the last added comes first.
	for added: Button in [skill_button, bots_button]:
		added.custom_minimum_size = map_button.custom_minimum_size
		map_button.add_sibling(added)
	_cycle(bots_button, "host_bots", Bots.COUNTS, func(count: int) -> String: return "No bots" if count == 0 else ("1 bot" if count == 1 else "%d bots" % count))
	_cycle(skill_button, "host_bot_skill", BotBrain.SKILLS, func(skill: String) -> String: return "Bots: %s" % BotBrain.SKILL_NAMES[skill])
	var rules_button := Button.new()
	rules_button.name = "RulesButton"
	rules_button.custom_minimum_size = map_button.custom_minimum_size
	skill_button.add_sibling(rules_button)
	_cycle(rules_button, "host_rules", MatchSettings.RULES, func(rules: String) -> String: return "Rules: %s" % MatchSettings.RULE_NAMES[rules])
	var sky_button := Button.new()
	sky_button.name = "SkyButton"
	sky_button.custom_minimum_size = map_button.custom_minimum_size
	map_button.add_sibling(sky_button)
	_cycle(sky_button, "host_sky", Atmosphere.ORDER + [Atmosphere.RANDOM], func(sky: String) -> String: return "Sky: %s" % Atmosphere.NAMES[sky])
	_cycle(private_button, "host_private", [false, true], func(hidden: bool) -> String: return "Private: invite only" if hidden else "Listed for everyone")


func _cycle(button: Button, key: String, choices: Array, describe: Callable) -> void:
	_match_buttons[key] = {"button": button, "choices": choices, "describe": describe}
	_show_match_setting(key)
	button.pressed.connect(func() -> void:
		Settings.set_value(key, choices[(choices.find(Settings.get_value(key)) + 1) % choices.size()]))


func _apply_page_setting(key: String) -> void:
	if key in [FramePace.CAP, FramePace.BATTERY_SAVER]:
		FramePace.apply()
	if key in [Look.MENU_TEXT, Look.HIGH_CONTRAST]:
		Look.refresh()
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
		"bots": Settings.get_value("host_bots"),
		"bot_skill": Settings.get_value("host_bot_skill"),
		"rules": Settings.get_value("host_rules"),
		# One drawn by lot is drawn here, once, so that everyone gets the same.
		"sky": Atmosphere.settle(Settings.get_value("host_sky")),
	})


# The button for playing at once, beside the one for hosting, and those for
# sorting and filtering the session list, in its heading.
func _add_session_controls() -> void:
	play_now_button = Button.new()
	play_now_button.name = "PlayNowButton"
	play_now_button.text = "Play now"
	play_now_button.tooltip_text = "Join the session with room and the most players, or host one if there is none"
	play_now_button.custom_minimum_size = Vector2(0, 44)
	play_now_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	host_button.get_parent().add_child(play_now_button)
	host_button.get_parent().move_child(play_now_button, 0)
	play_now_button.pressed.connect(play_now)
	# In the list's heading, before Refresh.
	var row := refresh_button.get_parent()
	sort_button = _list_option(row, "SortButton", func() -> void: _list_sort = SessionList.next_of(SessionList.SORTS, _list_sort))
	var modes := [SessionList.ANY]
	modes.append_array(MatchSettings.MODES.filter(func(mode: String) -> bool: return mode != MatchSettings.PRACTICE))
	mode_filter_button = _list_option(row, "ModeFilterButton", func() -> void: _list_mode = SessionList.next_of(modes, _list_mode))
	var arenas := [SessionList.ANY]
	arenas.append_array(Maps.ORDER)
	arena_filter_button = _list_option(row, "ArenaFilterButton", func() -> void: _list_arena = SessionList.next_of(arenas, _list_arena))
	hide_full_button = CheckBox.new()
	hide_full_button.name = "HideFullButton"
	hide_full_button.text = "Hide full"
	hide_full_button.toggled.connect(func(_on: bool) -> void: _draw_sessions())
	row.add_child(hide_full_button)
	row.move_child(refresh_button, -1)
	_draw_sessions()


func _list_option(row: Node, id: String, step: Callable) -> Button:
	var button := Button.new()
	button.name = id
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.clip_text = true
	button.pressed.connect(func() -> void:
		step.call()
		_draw_sessions())
	row.add_child(button)
	return button


## Joins the best session listed for the mode chosen to host, or hosts one
## if none has room (see SessionList.play_now).
func play_now() -> void:
	var best := SessionList.play_now(_listed, host_settings().mode)
	if best.is_empty():
		_host()
	else:
		_join(String(best.code))


func _host() -> void:
	_remember_name()
	_set_busy("Starting session…", host_settings().map)
	Net.host(host_settings(), Settings.get_value("host_private"))


## The other real players in the session, whose talk can be muted.
func others_here() -> Array:
	if multiplayer.multiplayer_peer == null or not world.match_.has_state():
		return []
	var own := multiplayer.get_unique_id()
	var found: Array = world.match_.state.players.keys().filter(func(id: int) -> bool: return id != own and not Bots.is_bot(id))
	found.sort()
	return found


## The players this one could ask for a vote on removing, in order.
func removable() -> Array:
	if multiplayer.multiplayer_peer == null or not world.match_.has_state():
		return []
	var own := multiplayer.get_unique_id()
	var found: Array = world.match_.state.voters().filter(func(id: int) -> bool: return world.match_.can_propose(own, id))
	found.sort()
	return found


# Starts a session alone on the practice range.
func _practise() -> void:
	_remember_name()
	tutorial = null
	_alone_settings = MatchSettings.practice()
	Net.play_alone()


# Starts the tutorial: a session alone on the practice range, with one
# thing to do at a time.
func _teach() -> void:
	_remember_name()
	_alone_settings = MatchSettings.practice()
	# It says how to do everything, a step at a time, so the controls
	# aren't shown first.
	tutorial = Tutorial.new()
	Net.play_alone()


# Tells the tutorial how things stand with the player, and shows what it
# says to do next.
func _run_tutorial() -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null
	var record: PlayerRecord = world.match_.own_record()
	if own == null or record == null:
		return
	var shots := 0
	var reloads := 0
	for carried: WeaponState in own.arsenal.values():
		reloads += carried.reloads
	for weapon: String in Weapons.ids():
		shots += world.match_.tally.of(weapon).shots
	var finished := tutorial.update({"at": own.global_position, "yaw": own.rotation.y, "shots": shots, "aiming": own.is_aiming(),
		"reloading": own.weapon.is_reloading(), "reloads": reloads, "full": own.weapon.ammo >= own.weapon.stats.magazine, "weapons": own.arsenal.size(), "overloaded": record.overload_left > 0.0})
	if finished:
		Sounds.play("objective", -4.0)
	# The step that asks for an Overload gives the player one to set off.
	if tutorial.step_name() == "overload" and record.overload_left <= 0.0:
		record.overload = Overload.METER_FULL
	var keys := {"move": "%s %s %s %s" % [Controls.keys_text("move_forward"), Controls.keys_text("move_left"), Controls.keys_text("move_back"), Controls.keys_text("move_right")],
		"reload": Controls.keys_text("reload"), "overload": Controls.keys_text("overload")}
	var device: String = controls_help.device_for(touch_controls.shown, Input.get_connected_joypads().size())
	combat_hud.show_tutorial(tutorial.text(device, keys, Settings.get_value("touch_left_handed")))


func _join(target: String) -> void:
	if Net.use_webrtc and target.strip_edges().is_empty():
		status.text = "Enter a session code."
		return
	_remember_name()
	# Its arena is known already if it is one of those listed.
	var map := ""
	for session: Dictionary in _listed:
		if String(session.get("code", "")) == target.strip_edges().to_upper():
			map = MatchSettings.from_dict(session.get("settings")).map
	_set_busy("Joining %s…" % target.strip_edges().to_upper() if Net.use_webrtc else "Joining…", map)
	Net.join(target)


## Puts a link to the session on the clipboard, and says so on the button.
func _copy_link() -> void:
	copied_link = Invite.link(WebPage.address(), Net.room_code)
	DisplayServer.clipboard_set(copied_link)
	copy_link_button.text = "Link copied!"
	await get_tree().create_timer(2.0).timeout
	copy_link_button.text = "Copy invite link"


## How full the bar on the cover is, from 0 to 1, when `sounds` is the share
## of the sounds that have been made and one of everything has been drawn
## or not (`drawn`): the sounds are most of the wait.
static func loading_progress(sounds: float, drawn: bool) -> float:
	return clampf(sounds, 0.0, 1.0) * 0.85 + (0.15 if drawn else 0.0)


func _set_busy(message: String, map := "") -> void:
	join_screen.open(message, map)
	status.text = message
	host_button.disabled = true
	join_button.disabled = true


func _on_session_started() -> void:
	_auto_rejoins = 0
	_reconnect_left = -1.0
	Sounds.set_music_quiet(true)
	menu.hide()
	front_end.hide()
	hud.show()
	combat_hud.show()
	touch_controls.show()
	world.start(_alone_settings if _alone_settings != null else host_settings())
	# Someone who joined to watch says so as soon as they are in.
	if _alone_settings == null and not world.match_.is_host() and Settings.get_value("join_watching"):
		world.match_.watch(true)
	_alone_settings = null
	copy_link_button.visible = Net.use_webrtc and OS.has_feature("web") and not Net.alone
	if not Settings.get_value("seen_help") and tutorial == null:
		Settings.set_value("seen_help", true)
		show_help()


## Shows the controls for the device the player seems to be using.
func show_help() -> void:
	controls_help.open(controls_help.device_for(touch_controls.shown, Input.get_connected_joypads().size()))


func _show_help_from_menu() -> void:
	close_pause_menu()
	show_help()
	await controls_help.closed
	if hud.visible:
		open_pause_menu()


func _on_host_silent_changed(silent: bool) -> void:
	waiting_notice.visible = silent
	if silent:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _on_session_failed(reason: String) -> void:
	Sounds.play(UiSounds.ERROR, -6.0)
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
		lines.append("Cheddar %d  –  %d Brie" % [state.team_score(MatchState.CHEDDAR), state.team_score(MatchState.BRIE)])
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
	join_screen.hide()
	final_standings.text = standings(world.match_.state)
	tutorial = null
	combat_hud.show_tutorial("")
	world.stop()
	pause_menu.hide()
	controls_help.hide()
	combat_hud.hide()
	combat_hud.reset()
	touch_controls.hide()
	if reason.is_empty():
		_show_menu("")
		return
	hud.hide()
	disconnected_detail.text = reason
	rejoin_button.visible = can_rejoin(reason, Net.last_joined) and reason != Match.REMOVED
	disconnected_notice.show()
	disconnected_button.grab_focus()
	# A dropped connection is tried again once, after a short wait, unless
	# the player chooses otherwise.
	_reason_shown = reason
	_reconnect_left = RECONNECT_WAIT if rejoin_button.visible and _auto_rejoins < AUTO_REJOINS else -1.0
	if _reconnect_left > 0.0:
		_auto_rejoins += 1
		disconnected_detail.text = reconnect_text(reason, _reconnect_left)


## Whether a player who lost their session for `reason` can try joining it
## again: a joiner whose connection failed, not one whose host closed it.
static func can_rejoin(reason: String, joined: String) -> bool:
	return not joined.is_empty() and not reason.is_empty() and not reason in ["The host closed the session.", Match.HOST_ENDED]


## What the notice of a lost session says while it waits `left` seconds to
## try again.
static func reconnect_text(reason: String, left: float) -> String:
	return "%s\nTrying again in %d…" % [reason, ceili(left)]


func _rejoin() -> void:
	_reconnect_left = -1.0
	disconnected_notice.hide()
	menu.show()
	_join(Net.last_joined)


## Shows the title screen.
func show_title() -> void:
	menu.hide()
	settings_menu.hide()
	front_end.open()


# One of the title screen's ways in was chosen.
func _on_way_in_chosen(what: String) -> void:
	match what:
		"play":
			_show_menu("")
		"practice":
			_practise()
		"learn":
			_teach()
		"profile":
			profile_screen.open()
		"customise":
			settings_menu.open_part(settings_menu.CUSTOMISE)
		"edit":
			menu.hide()
			front_end.hide()
			arena_editor.open(world)
		"settings":
			settings_menu.open_part(settings_menu.SETTINGS)
		"credits":
			front_end.show_credits(true)


func _dismiss_disconnected() -> void:
	disconnected_notice.hide()
	_show_menu("")


func _show_menu(message: String) -> void:
	Sounds.set_music_quiet(false)
	pause_menu.hide()
	settings_menu.hide()
	controls_help.hide()
	hud.hide()
	combat_hud.hide()
	combat_hud.reset()
	touch_controls.hide()
	front_end.hide()
	join_screen.hide()
	Look.arrive(menu)
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
	_listed = list
	_draw_sessions()


# The sessions listed, sorted and filtered as the player has asked.
func _draw_sessions() -> void:
	for child in sessions.get_children():
		child.queue_free()
	sort_button.text = SessionList.SORT_NAMES[_list_sort]
	mode_filter_button.text = MatchSettings.MODE_NAMES[_list_mode] if _list_mode != SessionList.ANY else "Any mode"
	arena_filter_button.text = Maps.NAMES[_list_arena] if _list_arena != SessionList.ANY else "Any arena"
	var shown := SessionList.arranged(_listed, _list_sort, _list_mode, _list_arena, hide_full_button.button_pressed)
	if shown.is_empty():
		var none := Label.new()
		none.text = "None yet. Host one!" if _listed.is_empty() else "None like that. Host one!"
		sessions.add_child(none)
	for session: Dictionary in shown:
		var button := Button.new()
		button.text = session_line(session)
		button.pressed.connect(_join.bind(session.code))
		sessions.add_child(button)
