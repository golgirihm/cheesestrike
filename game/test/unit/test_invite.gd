extends GutTest
## Invite links: making them, and reading the code back out of the address.


func test_a_link_adds_the_code_to_the_page_address() -> void:
	assert_eq(Invite.link("https://example.com/cheese/", "ABCD"), "https://example.com/cheese/?join=ABCD")
	assert_eq(Invite.link("https://example.com/cheese/index.html", "ABCD"), "https://example.com/cheese/index.html?join=ABCD")


func test_a_link_keeps_the_other_parameters() -> void:
	assert_eq(
		Invite.link("http://localhost:8060/?signal=ws://pc:9080&test", "WXYZ"),
		"http://localhost:8060/?signal=ws://pc:9080&test&join=WXYZ")


func test_a_link_replaces_an_old_code_and_drops_the_fragment() -> void:
	assert_eq(Invite.link("https://example.com/?join=OLDD&x=1#top", "NEWW"), "https://example.com/?x=1&join=NEWW")


func test_the_code_is_read_from_the_query() -> void:
	assert_eq(Invite.code_in("?join=ABCD"), "ABCD")
	assert_eq(Invite.code_in("?test&signal=ws://a:1&join=abcd"), "ABCD")
	assert_eq(Invite.code_in("join=%20WXYZ%20"), "WXYZ")


func test_a_link_reads_back_as_its_code() -> void:
	var link := Invite.link("http://localhost:8060/?signal=ws://pc:9080", "QRST")
	assert_eq(Invite.code_in(link.get_slice("?", 1)), "QRST")


func test_no_code_or_a_bad_one_gives_nothing() -> void:
	for query: String in ["", "?", "?test", "?join=", "?join", "?join=AB1D", "?join=ABCDEFGHIJ", "?join=<b>", "?rejoin=ABCD"]:
		assert_eq(Invite.code_in(query), "", query)


func test_the_code_is_taken_off_the_query() -> void:
	assert_eq(Invite.without_code("?join=ABCD"), "")
	assert_eq(Invite.without_code("?test&join=ABCD&signal=ws://a:1"), "?test&signal=ws://a:1")
	assert_eq(Invite.without_code("?test"), "?test")
	assert_eq(Invite.without_code(""), "")
