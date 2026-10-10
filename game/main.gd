extends Node
## Entry scene: the arena with the host/join menu over it.
##
## Native builds also take `-- --host` or `-- --join=<address>` on the command
## line to skip the menu.

@onready var world: Node3D = $World
@onready var menu: Control = $UI/Menu
@onready var host_button: Button = %HostButton
@onready var target_edit: LineEdit = %TargetEdit
@onready var join_button: Button = %JoinButton
@onready var sessions_panel: Control = %SessionsPanel
@onready var sessions: VBoxContainer = %Sessions
@onready var refresh_button: Button = %RefreshButton
@onready var status: Label = %Status
@onready var hud: Label = %Hud
@onready var paused_notice: Control = %PausedNotice
@onready var disconnected_notice: Control = %DisconnectedNotice
@onready var disconnected_detail: Label = %DisconnectedDetail
@onready var disconnected_button: Button = %DisconnectedButton


func _ready() -> void:
	Net.session_started.connect(_on_session_started)
	Net.session_failed.connect(_on_session_failed)
	Net.session_ended.connect(_on_session_ended)
	Net.sessions_listed.connect(_show_sessions)
	Net.host_paused_changed.connect(paused_notice.set_visible)
	disconnected_button.pressed.connect(_dismiss_disconnected)
	host_button.pressed.connect(_host)
	join_button.pressed.connect(func() -> void: _join(target_edit.text))
	target_edit.text_submitted.connect(_join)
	refresh_button.pressed.connect(Net.request_sessions)

	# Session codes and the session list only exist with the signaling server.
	sessions_panel.visible = Net.use_webrtc
	if not Net.use_webrtc:
		target_edit.placeholder_text = "Host address (blank for this PC)"
	Net.request_sessions()

	for arg in OS.get_cmdline_user_args():
		if arg == "--host":
			_host()
		elif arg.begins_with("--join="):
			_join(arg.trim_prefix("--join="))


func _process(_delta: float) -> void:
	if hud.visible:
		hud.text = "Session %s  ·  %d playing" % [Net.room_code, world.players.get_child_count()]


func _host() -> void:
	_set_busy("Starting session…")
	Net.host()


func _join(target: String) -> void:
	if Net.use_webrtc and target.strip_edges().is_empty():
		status.text = "Enter a session code."
		return
	_set_busy("Joining…")
	Net.join(target)


func _set_busy(message: String) -> void:
	status.text = message
	host_button.disabled = true
	join_button.disabled = true


func _on_session_started() -> void:
	menu.hide()
	hud.show()
	world.start()


func _on_session_failed(reason: String) -> void:
	_show_menu(reason)


func _on_session_ended(reason: String) -> void:
	world.stop()
	if reason.is_empty():
		_show_menu("")
		return
	hud.hide()
	disconnected_detail.text = reason
	disconnected_notice.show()
	disconnected_button.grab_focus()


func _dismiss_disconnected() -> void:
	disconnected_notice.hide()
	_show_menu("")


func _show_menu(message: String) -> void:
	hud.hide()
	menu.show()
	status.text = message
	host_button.disabled = false
	join_button.disabled = false
	Net.request_sessions()


func _show_sessions(list: Array) -> void:
	for child in sessions.get_children():
		child.queue_free()
	if list.is_empty():
		var none := Label.new()
		none.text = "None yet. Host one!"
		sessions.add_child(none)
	for session: Dictionary in list:
		var button := Button.new()
		button.text = "%s  ·  %d playing" % [session.code, int(session.players)]
		button.pressed.connect(_join.bind(session.code))
		sessions.add_child(button)
