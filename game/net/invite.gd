class_name Invite
extends RefCounted
## Invite links: a page address that, opened, joins a session straight away,
## such as `https://example.com/cheesestrike/?join=ABCD`.

## The query parameter that holds the session code.
const PARAM := "join"
## Longest code read from a link; real codes are four letters.
const MAX_CODE := 8


## The link to the session `code` from the page at `page_url`. Keeps the
## page's other parameters, such as `signal`, so whoever opens the link reaches
## the same session server; drops any old code and the part after `#`.
static func link(page_url: String, code: String) -> String:
	var address := page_url.get_slice("#", 0)
	var base := address.get_slice("?", 0)
	var kept: Array[String] = []
	if address.contains("?"):
		for part in address.get_slice("?", 1).split("&", false):
			if part.get_slice("=", 0) != PARAM:
				kept.append(part)
	kept.append("%s=%s" % [PARAM, code.uri_encode()])
	return "%s?%s" % [base, "&".join(kept)]


## The session code in a page's query string (`location.search`), upper case,
## or "" if there is none or it can't be a code.
static func code_in(query: String) -> String:
	for part in query.trim_prefix("?").split("&", false):
		if part.get_slice("=", 0) != PARAM or not part.contains("="):
			continue
		var code := part.get_slice("=", 1).uri_decode().strip_edges().to_upper()
		if code.is_empty() or code.length() > MAX_CODE:
			return ""
		for character in code:
			if not (character >= "A" and character <= "Z"):
				return ""
		return code
	return ""


## The query string `query` without its session code, so that once read, a
## reload doesn't join again.
static func without_code(query: String) -> String:
	var kept: Array[String] = []
	for part in query.trim_prefix("?").split("&", false):
		if part.get_slice("=", 0) != PARAM:
			kept.append(part)
	return "" if kept.is_empty() else "?" + "&".join(kept)
