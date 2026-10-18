extends GutTest
## Keeps how players connect inside game/net, so that another way of
## connecting (Steam's, say) can take its place there without the rest of the
## game noticing. The rest of the game goes through Net's own members, listed
## here, and the multiplayer API that every kind of connection provides.

const NET := "res://net/"
const LEFT_OUT := ["res://addons/", "res://test/", "res://licensed/"]

## Names that belong to one way of connecting: WebRTC and the session server's
## protocol (signaling/core.py), or ENet.
const TRANSPORT := [
	"WebRTC", "WebSocket", "ENet", "BackendUrls", "JoiningPeers", "ICE_SERVERS",
	"\"offer\"", "\"answer\"", "\"candidate\"", "\"hosted\"", "\"joined\"", "\"resume\"",
]

## What the rest of the game may use of Net. A new one is a change to what
## every way of connecting has to provide, so it is added here on purpose.
const NET_MEMBERS := [
	"alone", "host", "host_silent", "host_silent_changed", "in_session", "join", "last_joined",
	"leave", "play_alone", "request_sessions", "room_code",
	"session_ended", "session_failed", "session_started", "sessions_listed", "sessions_unreachable",
]


func test_nothing_outside_game_net_names_a_way_of_connecting() -> void:
	var found := []
	for path in _scripts_outside_net():
		var lines := FileAccess.get_file_as_string(path).split("\n")
		for index in lines.size():
			for name: String in TRANSPORT:
				if lines[index].contains(name):
					found.append("%s:%d %s" % [path, index + 1, name])
	assert_eq(found, [], "belongs in game/net")


func test_the_rest_of_the_game_uses_only_nets_own_members() -> void:
	var member := RegEx.create_from_string("\\bNet\\.([A-Za-z_]+)")
	var found := []
	for path in _scripts_outside_net():
		var lines := FileAccess.get_file_as_string(path).split("\n")
		for index in lines.size():
			for result in member.search_all(lines[index]):
				if not NET_MEMBERS.has(result.get_string(1)):
					found.append("%s:%d Net.%s" % [path, index + 1, result.get_string(1)])
	assert_eq(found, [], "not one of NET_MEMBERS")


func test_the_scripts_are_found() -> void:
	var paths := _scripts_outside_net()
	assert_has(paths, "res://main.gd")
	assert_does_not_have(paths, "res://net/net.gd")


func _scripts_outside_net() -> Array:
	var paths := []
	var folders := ["res://"]
	while not folders.is_empty():
		var folder: String = folders.pop_back()
		if folder == NET or LEFT_OUT.has(folder):
			continue
		for sub in DirAccess.get_directories_at(folder):
			if not sub.begins_with("."):
				folders.append(folder.path_join(sub) + "/")
		for file in DirAccess.get_files_at(folder):
			if file.ends_with(".gd"):
				paths.append(folder.path_join(file))
	return paths
