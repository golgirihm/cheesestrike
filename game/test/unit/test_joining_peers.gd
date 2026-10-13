extends GutTest

var joining: JoiningPeers


func before_each() -> void:
	joining = JoiningPeers.new()


func test_a_player_who_never_connects_is_given_up_on_in_time() -> void:
	joining.started(7)
	assert_eq(joining.tick(JoiningPeers.TIMEOUT - 0.1), [] as Array[int])
	assert_eq(joining.tick(0.2), [7] as Array[int])
	assert_eq(joining.tick(100.0), [] as Array[int], "and only once")


func test_a_player_who_connects_is_never_given_up_on() -> void:
	joining.started(7)
	joining.tick(1.0)
	joining.connected(7)
	assert_eq(joining.tick(JoiningPeers.TIMEOUT * 2.0), [] as Array[int])


func test_leaving_the_session_server_shortens_the_wait() -> void:
	joining.started(7)
	joining.tick(1.0)
	joining.left_server(7)
	assert_eq(joining.tick(JoiningPeers.TIMEOUT_AFTER_LEAVING - 0.1), [] as Array[int])
	assert_eq(joining.tick(0.2), [7] as Array[int])


func test_leaving_the_session_server_just_before_connecting_drops_nobody() -> void:
	# Every player leaves the session server once connected, and the host can
	# hear of that first.
	joining.started(7)
	joining.left_server(7)
	joining.tick(0.1)
	joining.connected(7)
	assert_eq(joining.tick(JoiningPeers.TIMEOUT), [] as Array[int])


func test_leaving_the_session_server_never_lengthens_the_wait() -> void:
	joining.started(7)
	joining.tick(JoiningPeers.TIMEOUT - 1.0)
	joining.left_server(7)
	assert_eq(joining.tick(1.1), [7] as Array[int])


func test_players_are_timed_separately() -> void:
	joining.started(1)
	joining.tick(10.0)
	joining.started(2)
	assert_eq(joining.tick(JoiningPeers.TIMEOUT - 10.0), [1] as Array[int])
	assert_eq(joining.tick(10.0), [2] as Array[int])


func test_a_report_about_someone_not_joining_is_ignored() -> void:
	joining.left_server(99)
	assert_eq(joining.tick(JoiningPeers.TIMEOUT * 2.0), [] as Array[int])
