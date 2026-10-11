class_name WebPage
## Browser-page behavior that web builds need and the engine doesn't provide.
## Every call is a no-op outside a web build.
##
## Keep-alive: browsers stop delivering animation frames to a page that is
## hidden or covered, which freezes the game loop. A frozen host freezes the
## session for everyone, and any frozen player stops reading the network while
## messages keep arriving, until the buffer overflows and drops them. While
## keep-alive is on, a Web Worker timer (which browsers leave running) drives
## the loop whenever frames stop arriving. It can't help once a phone suspends
## the page outright.
##
## Wake lock: stops the screen dimming and locking while the game is visible,
## for players who aren't touching the screen. Browsers only offer it on HTTPS
## and localhost, and drop it whenever the page is hidden, so it is re-requested
## each time the page comes back.
##
## Mouse look: with the pointer locked, Chrome sometimes reports a movement the
## mouse never made, as the pointer locks and with clicks after that: exactly
## the size of the pointer's position on the page, as if measured from the
## corner, sometimes along with the position reset to the corner. Taken as
## real, it throws the view to a random angle. The game takes the mouse's
## movement from here, gathered between frames, with those left out, rather
## than from the engine, which passes them on. A real movement matching the
## pointer's position to the pixel in both directions is next to impossible.
##
## Raw mouse movement: with the mouse captured, Chrome on Windows now and then
## reports one movement hundreds of pixels long that the mouse never made, which
## throws the view to a different angle. That comes from how it re-centres the
## hidden pointer, and asking for movement straight from the mouse avoids it. It
## also takes the operating system's pointer acceleration out of aiming.

const _SCRIPT := """
(() => {
	if (window.cheeseStrike) return;

	const FRAME_MS = 16;
	// How long frames must be missing before the worker takes over.
	const STALL_MS = 100;

	const nativeRequest = window.requestAnimationFrame.bind(window);
	const pending = new Map();
	let nextId = 1;
	let keepAlive = false;
	let stalled = false;
	let lastFrame = performance.now();
	// At most one real frame request is outstanding, however long the page
	// stays hidden; it doubles as the signal that frames are arriving again.
	let awaitingNative = false;

	const runPending = (time) => {
		lastFrame = performance.now();
		for (const id of [...pending.keys()]) {
			const callback = pending.get(id);
			if (!callback) continue;
			pending.delete(id);
			callback(time);
		}
	};

	window.requestAnimationFrame = (callback) => {
		const id = nextId++;
		pending.set(id, callback);
		if (!awaitingNative) {
			awaitingNative = true;
			nativeRequest((time) => {
				awaitingNative = false;
				stalled = false;
				runPending(time);
			});
		}
		return id;
	};
	window.cancelAnimationFrame = (id) => { pending.delete(id); };

	const source = 'let timer = null; onmessage = (e) => { clearInterval(timer); timer = e.data ? setInterval(() => postMessage(0), e.data) : null; };';
	const worker = new Worker(URL.createObjectURL(new Blob([source], { type: 'text/javascript' })));
	worker.onmessage = () => {
		if (!keepAlive) return;
		if (!stalled && performance.now() - lastFrame < STALL_MS) return;
		stalled = true;
		runPending(performance.now());
	};

	let wantWakeLock = false;
	let wakeLock = null;
	const acquireWakeLock = async () => {
		if (!wantWakeLock || wakeLock || document.hidden || !navigator.wakeLock) return;
		try {
			const lock = await navigator.wakeLock.request('screen');
			if (!wantWakeLock) { lock.release(); return; }
			wakeLock = lock;
			lock.addEventListener('release', () => { if (wakeLock === lock) wakeLock = null; });
		} catch (error) {
			// Refused, e.g. by battery saver. Nothing to do but carry on.
		}
	};
	document.addEventListener('visibilitychange', acquireWakeLock);

	// The engine captures the mouse with a plain requestPointerLock(). Ask for
	// raw movement instead, falling back where the browser doesn't offer it.
	const nativeLock = Element.prototype.requestPointerLock;
	if (nativeLock) {
		Element.prototype.requestPointerLock = function (options) {
			let result;
			try {
				result = nativeLock.call(this, { unadjustedMovement: true, ...options });
			} catch (error) {
				return nativeLock.call(this, options);
			}
			if (!result || !result.catch) return result;
			return result.catch((error) => {
				if (error.name !== 'NotSupportedError') throw error;
				return nativeLock.call(this, options);
			});
		};
	}

	let lookX = 0;
	let lookY = 0;
	// Where the pointer last was on the page, by any mouse event.
	let pointerX = 0;
	let pointerY = 0;
	const sameSize = (movement, position) => Math.abs(movement) === Math.abs(position);
	document.addEventListener('mousemove', (event) => {
		const lastX = pointerX;
		const lastY = pointerY;
		pointerX = event.clientX;
		pointerY = event.clientY;
		if (!document.pointerLockElement) return;
		const moved = event.movementX !== 0 || event.movementY !== 0;
		const phantom = moved && (
			(sameSize(event.movementX, event.clientX) && sameSize(event.movementY, event.clientY))
			|| (sameSize(event.movementX, lastX) && sameSize(event.movementY, lastY)));
		if (phantom) return;
		lookX += event.movementX;
		lookY += event.movementY;
	}, true);
	for (const type of ['mousedown', 'mouseup']) {
		document.addEventListener(type, (event) => {
			pointerX = event.clientX;
			pointerY = event.clientY;
		}, true);
	}

	window.cheeseStrike = {
		takeMouseMovement() {
			const moved = lookX + ' ' + lookY;
			lookX = 0;
			lookY = 0;
			return moved;
		},
		setKeepAlive(on) {
			keepAlive = on;
			stalled = false;
			lastFrame = performance.now();
			worker.postMessage(on ? FRAME_MS : 0);
		},
		setWakeLock(on) {
			wantWakeLock = on;
			if (on) acquireWakeLock();
			else if (wakeLock) wakeLock.release();
		},
	};
})();
"""


static func install() -> void:
	if OS.has_feature("web"):
		JavaScriptBridge.eval(_SCRIPT, true)


static func set_keep_alive(on: bool) -> void:
	if OS.has_feature("web"):
		JavaScriptBridge.eval("window.cheeseStrike.setKeepAlive(%s)" % ("true" if on else "false"), true)


static func set_wake_lock(on: bool) -> void:
	if OS.has_feature("web"):
		JavaScriptBridge.eval("window.cheeseStrike.setWakeLock(%s)" % ("true" if on else "false"), true)


## How far the mouse has moved with the pointer locked since the last call, in
## the page's own pixels. Always zero outside a web build.
static func take_mouse_movement() -> Vector2:
	if not OS.has_feature("web"):
		return Vector2.ZERO
	var moved: Variant = JavaScriptBridge.eval("window.cheeseStrike.takeMouseMovement()", true)
	if not moved is String:
		return Vector2.ZERO
	var parts: PackedStringArray = moved.split(" ")
	return Vector2(parts[0].to_float(), parts[1].to_float()) if parts.size() == 2 else Vector2.ZERO


static var _native_key := ""


## A key that stays the same for this tab, through reloads, but differs between
## tabs: in a browser, kept in the tab's session storage; natively, made once
## per run. The host uses it to know a player who comes back after losing
## their connection.
static func player_key() -> String:
	if OS.has_feature("web"):
		var key: Variant = JavaScriptBridge.eval("""(() => {
			try {
				let key = sessionStorage.getItem('cheeseStrikeKey');
				if (!key) {
					key = Math.random().toString(36).slice(2) + Date.now().toString(36);
					sessionStorage.setItem('cheeseStrikeKey', key);
				}
				return key;
			} catch (error) {
				return '';
			}
		})()""", true)
		if key is String and not key.is_empty():
			return key
	if _native_key.is_empty():
		var rng := RandomNumberGenerator.new()
		rng.randomize()
		_native_key = "%x%x" % [rng.randi(), rng.randi()]
	return _native_key
