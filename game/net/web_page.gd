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

	window.cheeseStrike = {
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
