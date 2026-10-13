extends RefCounted
## An arena as a player might lay one out in the editor, for the tests that
## check arenas: every kind of piece, a stack of blocks, and the launcher in
## the middle where the first spot looks.


static func arena() -> Dictionary:
	var made := ArenaCode.blank(44)
	for piece: Array in [
			[ArenaCode.LAUNCHER, 0, 0], [ArenaCode.BLOCK, -8, 6], [ArenaCode.BLOCK, -8, 6, 0, 1], [ArenaCode.BLOCK, -6, 6],
			[ArenaCode.BLOCK, 9, -5], [ArenaCode.WALL, 6, -12], [ArenaCode.WALL, -12, 0, 1], [ArenaCode.CRATE, 10, 3],
			[ArenaCode.CRATE, -3, -11], [ArenaCode.HEALTH, 14, -14], [ArenaCode.HEALTH, -14, 14], [ArenaCode.AMMO, 0, 12],
			[ArenaCode.AMMO, 0, -15], [ArenaCode.ARMOUR, 15, 0], [ArenaCode.POWER, -5, 12], [ArenaCode.JUMP, 5, 9, 1]]:
		var went := ArenaCode.place(made, {"kind": piece[0], "x": piece[1], "z": piece[2], "turn": piece[3] if piece.size() > 3 else 0, "level": piece[4] if piece.size() > 4 else 0})
		assert(went, "the sample arena's pieces fit")
	return made


static func code() -> String:
	return ArenaCode.encode(arena())
