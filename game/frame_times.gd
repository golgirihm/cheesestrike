class_name FrameTimes
## How long frames have lately taken: the longest in each recent second, and
## what is usual. The test hook publishes it, so a test can tell whether the
## game caught at some moment, such as the first shot of a session.

## How many seconds are kept, and how many frames the usual is taken over.
const SECONDS_KEPT := 30
const FRAMES_KEPT := 120

## The longest frame in each recent second, oldest first: [[the second it
## was, counted from the first frame, milliseconds]].
var seconds: Array = []
var _recent: Array[float] = []
var _clock := 0.0


## Counts a frame that took `took` seconds.
func add(took: float) -> void:
	_clock += took
	var second := int(_clock)
	var ms := took * 1000.0
	if seconds.is_empty() or seconds[-1][0] != second:
		seconds.append([second, ms])
		if seconds.size() > SECONDS_KEPT:
			seconds.pop_front()
	else:
		seconds[-1][1] = maxf(seconds[-1][1], ms)
	_recent.append(ms)
	if _recent.size() > FRAMES_KEPT:
		_recent.pop_front()


## What a frame has usually taken lately, in milliseconds: the middle one.
func typical() -> float:
	if _recent.is_empty():
		return 0.0
	var sorted := _recent.duplicate()
	sorted.sort()
	return sorted[sorted.size() / 2]


## The longest frame in the seconds after `second`, in milliseconds.
func longest_since(second: int) -> float:
	var longest := 0.0
	for kept: Array in seconds:
		if kept[0] > second:
			longest = maxf(longest, kept[1])
	return longest


## The latest second counted, for longest_since.
func now() -> int:
	return seconds[-1][0] if not seconds.is_empty() else 0
