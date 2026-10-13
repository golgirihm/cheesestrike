class_name Bots
## Computer players: who they are. Each is a player like any other to the
## match, with an id below zero, which no real player has; the host's
## machine moves them (BotDriver) and decides for them (BotBrain).

## How many the host can ask for.
const COUNTS := [0, 1, 3, 5, 7]
const NAMES := ["Stilton", "Feta", "Edam", "Gruyère", "Manchego", "Halloumi", "Ricotta", "Camembert", "Emmental", "Pecorino", "Provolone", "Taleggio", "Havarti", "Jarlsberg", "Limburger"]
## What marks a name as a bot's.
const TAG := "[bot] "


static func is_bot(id: int) -> bool:
	return id < 0


## The id of bot number `index`, counting from 0.
static func id_for(index: int) -> int:
	return -(index + 1)


static func name_for(id: int) -> String:
	return TAG + NAMES[posmod(-id - 1, NAMES.size())]


## How many bots a session should have: as many as the host asked for, less
## any whose place a real player needs.
static func wanted(settings: MatchSettings, humans: int) -> int:
	return clampi(mini(settings.bots, settings.max_players - humans), 0, MatchSettings.MAX_PLAYERS)


## The loadout bot `id` brings: [main, side], always the same for the same bot.
static func loadout_for(id: int) -> Array:
	var mains: Array = Weapons.MAINS
	return [mains[posmod(-id - 1, mains.size())], Weapons.STARTING]


## The look bot `id` wears, always the same for the same bot.
static func look_for(id: int) -> int:
	var number := -id - 1
	return Looks.pack(Looks.CHEESES[posmod(number, Looks.CHEESES.size())], Looks.HATS[posmod(number * 3 + 1, Looks.HATS.size())], Looks.HELD[0])
