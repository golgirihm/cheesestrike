class_name BackendUrls
extends RefCounted
## Works out where the session server is: the address for WebSocket
## connections (hosting and joining) and the one for plain HTTP requests (the
## session list, and whether a session is still open).
##
## The published game is told both by the page it is on: deploying writes them
## into the page as `window.cheeseStrikeConfig`, since on AWS they are two
## unrelated addresses. In development nothing is written, and the server is
## one process answering both on port 9080 of whatever machine served the
## page, so a phone on the LAN reaches the dev machine. Either way
## `?signal=ws://...` in the page's address overrides it, with HTTP taken to be
## at the same place.

const LOCAL_PORT := 9080


## The addresses to use, as `{"signal": ..., "http": ...}`, neither ending in
## a slash. `override` is the page's `signal` parameter or "", `configured`
## what deploying wrote into the page or {}, and the rest describe the page.
static func choose(override: String, configured: Dictionary, page_is_https: bool, page_host: String) -> Dictionary:
	if not override.is_empty():
		return {"signal": override.trim_suffix("/"), "http": http_for(override)}
	var signal_url: Variant = configured.get("signal")
	var sessions_url: Variant = configured.get("sessions")
	if signal_url is String and sessions_url is String and not signal_url.is_empty() and not sessions_url.is_empty():
		return {"signal": signal_url.trim_suffix("/"), "http": sessions_url.trim_suffix("/")}
	var local := "%s://%s:%d" % ["wss" if page_is_https else "ws", page_host, LOCAL_PORT]
	return {"signal": local, "http": http_for(local)}


## The HTTP address at the same place as a WebSocket address.
static func http_for(signal_url: String) -> String:
	var address := signal_url.trim_suffix("/")
	if address.begins_with("wss://"):
		return "https://" + address.trim_prefix("wss://")
	if address.begins_with("ws://"):
		return "http://" + address.trim_prefix("ws://")
	return address
