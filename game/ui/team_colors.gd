class_name TeamColors
## The teams' colours, from a palette that people with the common kinds of
## colour blindness can tell apart: Cheddar a red orange and Brie blue.
## Cheddar's is not the yellow orange of cheese itself, which is kept for
## cheese that can be eaten. Teams also
## differ by the shape of the badge over each player, so colour is never the
## only clue.

const CHEDDAR := Color(0.835, 0.369, 0.0)
const BRIE := Color(0.337, 0.706, 0.914)


static func of(team: int) -> Color:
	match team:
		MatchState.CHEDDAR:
			return CHEDDAR
		MatchState.BRIE:
			return BRIE
	return Color.WHITE
