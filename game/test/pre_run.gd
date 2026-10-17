extends GutHookScript
## Runs before the game's tests. A match counts down before it begins (see
## MatchState.COUNTDOWN), when nobody can be hurt; most tests are about what
## happens after, so their matches begin at once. Those about the countdown
## set it back for themselves.


func run() -> void:
	MatchState.countdown_length = 0.0
