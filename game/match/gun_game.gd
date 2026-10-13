class_name GunGame
## Gun game: everyone carries one weapon, and each knock-out moves a player
## on to the next. Whoever gets a knock-out with the last of them wins.

## The weapons in the order they are worked through, the hardest to finish
## with last.
const LADDER := ["blaster", "shotgun", "carbine", "grater", "lmg", "rifle", "bow", "launcher", "pistols", "pistol"]


## The weapon a player with `kills` knock-outs carries.
static func weapon_for(kills: int) -> String:
	return LADDER[clampi(kills, 0, LADDER.size() - 1)]


## What a player with `kills` knock-outs is told of where they are, such as
## "Weapon 3 of 10: Colby Carbine".
static func text(kills: int) -> String:
	var at := clampi(kills, 0, LADDER.size() - 1)
	return "Weapon %d of %d: %s" % [at + 1, LADDER.size(), Weapons.find(LADDER[at]).display_name]
