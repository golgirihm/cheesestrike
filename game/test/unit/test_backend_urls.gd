extends GutTest

const PUBLISHED := {"signal": "wss://abc.execute-api.us-east-2.amazonaws.com/prod", "sessions": "https://xyz.lambda-url.us-east-2.on.aws/"}


func test_in_development_the_server_is_on_the_machine_that_served_the_page() -> void:
	assert_eq(
		BackendUrls.choose("", {}, false, "192.168.1.20"),
		{"signal": "ws://192.168.1.20:9080", "http": "http://192.168.1.20:9080"},
	)


func test_a_secure_page_reaches_it_securely() -> void:
	assert_eq(
		BackendUrls.choose("", {}, true, "play.example"),
		{"signal": "wss://play.example:9080", "http": "https://play.example:9080"},
	)


func test_a_published_page_uses_the_addresses_written_into_it() -> void:
	assert_eq(
		BackendUrls.choose("", PUBLISHED, true, "bucket.s3.us-east-2.amazonaws.com"),
		{"signal": "wss://abc.execute-api.us-east-2.amazonaws.com/prod", "http": "https://xyz.lambda-url.us-east-2.on.aws"},
	)


func test_the_pages_signal_parameter_overrides_both() -> void:
	assert_eq(
		BackendUrls.choose("ws://localhost:5123", PUBLISHED, true, "play.example"),
		{"signal": "ws://localhost:5123", "http": "http://localhost:5123"},
	)
	assert_eq(BackendUrls.choose("wss://other.example/", {}, false, "x").http, "https://other.example")


func test_half_written_addresses_are_ignored() -> void:
	var local := BackendUrls.choose("", {}, false, "localhost")
	assert_eq(BackendUrls.choose("", {"signal": "wss://abc.example"}, false, "localhost"), local)
	assert_eq(BackendUrls.choose("", {"signal": "", "sessions": "https://xyz.example"}, false, "localhost"), local)
	assert_eq(BackendUrls.choose("", {"signal": 7, "sessions": null}, false, "localhost"), local)
