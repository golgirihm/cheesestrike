## Curd City: a town to fight across. Cheddar's yard is at the west end and
## Brie's at the east, 580 m apart, a minute's run and more. Between them run
## three streets, the Main Avenue down the middle and North and South Street
## either side, crossed by five more; and on the blocks between stand houses
## of cheese two and three floors high, warehouses of cheese with ladders up
## them, and a few towers of stone to find one's way by.
##
## The town is the same turned half way round, so that neither side has the
## better of it: the west half is laid out here and the east half is the
## same, turned.

const SIZE := Vector2(640, 400)
## Where the streets run: east to west at these Z, north to south at these X.
const ACROSS := {-110.0: 16.0, 0.0: 22.0, 110.0: 16.0}
const DOWN := [-180.0, -90.0, 0.0, 90.0, 180.0]
## What the streets running north to south are called.
const LANES := {-180.0: "Rind Lane", -90.0: "Whey Street", 0.0: "Market Street", 90.0: "Curd Street", 180.0: "Wax Lane"}
const DOWN_WIDTH := 14.0
## Where each side's yard begins, and the middle of it.
const YARD := 262.0
const YARD_MIDDLE := 292.0
const STONE_TOWERS := [Color(0.5, 0.6, 0.72), Color(0.66, 0.4, 0.33)]


static func build() -> Node3D:
	var town := BigMap.new("CurdCity", Scenery.CITY, SIZE)
	var streets := _streets(town)
	var houses: Array[Dictionary] = []
	for side: float in [1.0, -1.0]:
		houses.append_array(_half(town, side))
	_middle(town)
	# Once everything stands, each house's doors are joined to the streets.
	for house: Dictionary in houses:
		town.link_nearest(house.front_point, streets)
		town.link_nearest(house.back_point, streets)
	# Starting spots, the west's and the east's in turn, so that the first two
	# to join a free-for-all start at either end.
	for row in 2:
		for column: float in [-28.0, -9.0, 9.0, 28.0]:
			var at := Vector2(-YARD_MIDDLE - 8.0 + row * 16.0, column)
			town.spawn(at)
			town.spawn(-at)
	town.objective("BaseCheddar", Vector3(-YARD_MIDDLE, 0, 0))
	town.objective("BaseBrie", Vector3(YARD_MIDDLE, 0, 0))
	for index in 5:
		var at: Vector2 = [Vector2.ZERO, Vector2(-90, -110), Vector2(90, 110), Vector2(-90, 110), Vector2(90, -110)][index]
		town.objective("Hill%d" % (index + 1), Vector3(at.x, 0, at.y))
	for index in 3:
		town.objective("Point%s" % "ABC"[index], Vector3((index - 1) * 180.0, 0, 0))
	var eye := Vector3(-380, 120, 190)
	return town.finish(Transform3D(Basis.looking_at(Vector3(-120, 0, 0) - eye), eye))


# The roads, the points bots go between on them, and the paving under the
# blocks between them. Returns how many points there are on the streets.
static func _streets(town: BigMap) -> int:
	var half := SIZE / 2.0
	for z: float in ACROSS:
		town.road("Avenue" if z == 0.0 else "Street", Vector2(-YARD, z), Vector2(YARD, z), ACROSS[z])
	for x: float in DOWN:
		town.road("Street", Vector2(x, -half.y), Vector2(x, half.y), DOWN_WIDTH)
	for yard: float in [-1.0, 1.0]:
		town.paving(Rect2(Vector2(minf(yard * half.x, yard * YARD), -half.y), Vector2(half.x - YARD, SIZE.y)))
	# The corners: every street crossing, the yards' ends of the streets, and
	# the ends of the streets that run out north and south.
	var columns: Array[float] = [-YARD_MIDDLE]
	columns.append_array(DOWN)
	columns.append(YARD_MIDDLE)
	var grid := {}
	for x in columns:
		for z: float in ACROSS:
			grid[Vector2(x, z)] = town.point(Vector3(x, 0, z))
	for x in columns:
		for z: float in [-165.0, 165.0]:
			grid[Vector2(x, z)] = town.point(Vector3(x, 0, z))
	var rows: Array[float] = [-165.0]
	rows.append_array(ACROSS.keys())
	rows.append(165.0)
	for x in columns:
		for index in rows.size() - 1:
			town.link(grid[Vector2(x, rows[index])], grid[Vector2(x, rows[index + 1])])
	for z: float in ACROSS:
		for index in columns.size() - 1:
			town.link(grid[Vector2(columns[index], z)], grid[Vector2(columns[index + 1], z)])
	return grid.size()


# Half the town: the west if `side` is 1, or the same turned half way round
# for the east if it is -1. Returns its houses (see BigMap.house).
static func _half(town: BigMap, side: float) -> Array[Dictionary]:
	var at := func(x: float, z: float) -> Vector2: return Vector2(x, z) * side
	var turn := func(quarters: int) -> int: return quarters + (0 if side > 0.0 else 2)
	var team := "Cheddar" if side > 0.0 else "Brie"
	for block: Rect2 in _blocks():
		town.paving(Rect2(block.position * side if side > 0.0 else -block.end, block.size))
	var houses: Array[Dictionary] = []
	# By the yard: a house on North Street, the hotel on the Avenue, and a
	# cottage on South Street.
	houses.append(town.house(at.call(-225, -140), Vector2(32, 22), 2, turn.call(0), {"label": "%s Dairy" % team, "inside": [Pickups.AMMO]}))
	houses.append(town.house(at.call(-225, -42), Vector2(36, 24), 3, turn.call(0), {"label": "%s Hotel" % team, "inside": [Pickups.HEALTH, Pickups.AMMO]}))
	houses.append(town.house(at.call(-225, 142), Vector2(28, 20), 1, turn.call(2), {"label": "%s Cottage" % team, "inside": [Pickups.HEALTH]}))
	# Further in: the bank, the tall house over the Avenue, and the shops.
	houses.append(town.house(at.call(-145, -45), Vector2(30, 22), 2, turn.call(0), {"label": "%s Bank" % team, "inside": [Pickups.AMMO]}))
	houses.append(town.house(at.call(-135, 50), Vector2(28, 24), 3, turn.call(2), {"label": "%s Lookout" % team, "inside": [Pickups.ARMOUR]}))
	houses.append(town.house(at.call(-45, -145), Vector2(26, 20), 1, turn.call(0), {"label": "%s Bakery" % team, "inside": [Pickups.HEALTH]}))
	houses.append(town.house(at.call(-45, -48), Vector2(32, 22), 2, turn.call(0), {"label": "%s Inn" % team, "inside": [Pickups.HEALTH]}))
	houses.append(town.house(at.call(-48, 82), Vector2(26, 20), 1, turn.call(2), {"label": "%s Shop" % team, "inside": [Pickups.AMMO]}))
	houses.append(town.house(at.call(-45, 148), Vector2(30, 22), 2, turn.call(2), {"label": "%s Mill" % team, "inside": [Pickups.HEALTH]}))
	# Warehouses of cheese, most with a ladder up them.
	var ladder_way := func(way: Vector3) -> Vector3: return way * side
	town.warehouse(at.call(-243, -178), Vector3(24, 10, 14), ladder_way.call(Vector3.RIGHT))
	town.warehouse(at.call(-235, 45), Vector3(22, 12, 16), ladder_way.call(Vector3.BACK))
	town.tower(at.call(-212, 82), Vector2(16, 16), 52.0, STONE_TOWERS[1 if side > 0.0 else 0])
	town.warehouse(at.call(-118, -150), Vector3(26, 11, 16), ladder_way.call(Vector3.FORWARD))
	town.warehouse(at.call(-112, -85), Vector3(14, 8, 12))
	town.warehouse(at.call(-115, 155), Vector3(20, 10, 22), ladder_way.call(Vector3.LEFT))
	town.warehouse(at.call(-62, -178), Vector3(20, 8, 12))
	town.warehouse(at.call(-25, 40), Vector3(14, 9, 14), ladder_way.call(Vector3.RIGHT))
	# A tower of stone on each half, seen from anywhere.
	var tower_at: Vector2 = at.call(-160, 160)
	town.tower(tower_at, Vector2(20, 20), 64.0, STONE_TOWERS[0 if side > 0.0 else 1])
	# A pad at the yard's mouth that throws whoever steps on it off down the
	# Avenue, toward the fighting.
	town.jump_pad(Vector3(-270.0 * side, 0, 0), Vector3(14.0 * side, 13.0, 0))
	# Cover in the yard, low walls of cheese across it.
	for z: float in [-80.0, -45.0, 45.0, 80.0]:
		town.cheese(Vector3(-268.0 * side, 2.0, z * side), Vector3(1.2, 4.0, 12.0))
	for z: float in [-140.0, 140.0]:
		town.cheese(Vector3(-306.0 * side, 2.0, z * side), Vector3(14.0, 4.0, 1.2))
	# Cover along the streets: blocks of cheese, and parked cars and a bus.
	for spot: Vector3 in [Vector3(-235, 1.5, -5), Vector3(-160, 1.5, 6), Vector3(-118, 1.5, -6), Vector3(-62, 1.5, 5),
			Vector3(-210, 1.5, -112), Vector3(-130, 1.5, -106), Vector3(-60, 1.5, -114), Vector3(-200, 1.5, 108), Vector3(-120, 1.5, 114), Vector3(-30, 1.5, 106),
			Vector3(-176, 1.5, -60), Vector3(-94, 1.5, 55), Vector3(-184, 1.5, 150), Vector3(-86, 1.5, -150), Vector3(4, 1.5, -70)]:
		town.cheese(Vector3(spot.x * side, spot.y, spot.z * side), Vector3(3, 3, 3))
	for parked: Vector3 in [Vector3(-200, -8, PI / 2.0), Vector3(-75, 8, PI / 2.0), Vector3(-150, -104, PI / 2.0), Vector3(-30, 116, PI / 2.0),
			Vector3(-185, 30, 0.0), Vector3(-95, -30, 0.0), Vector3(-5, -140, 0.0)]:
		var spot: Vector2 = at.call(parked.x, parked.y)
		town.car(spot, parked.z + (0.0 if side > 0.0 else PI))
	var bus_at: Vector2 = at.call(-140, -7)
	town.bus(bus_at, PI / 2.0 + (0.0 if side > 0.0 else PI))
	# Pickups lying in the streets and the yard.
	for lying: Array in [[Pickups.HEALTH, Vector3(-278, 0, -16)], [Pickups.AMMO, Vector3(-278, 0, 16)], [Pickups.HEALTH, Vector3(-180, 0, -110)],
			[Pickups.AMMO, Vector3(-90, 0, 0)], [Pickups.HEALTH, Vector3(-90, 0, 110)], [Pickups.AMMO, Vector3(-180, 0, 110)]]:
		var spot: Vector3 = lying[1]
		town.pickup(lying[0], Vector3(spot.x * side, 0, spot.z * side))
	town.place("%s Yard" % team, Vector3(-YARD_MIDDLE * side, 0, -14 * side))
	town.place("%s Gate" % team, Vector3(-YARD_MIDDLE * side, 0, 14 * side))
	return houses


# The middle of the town, where the Avenue crosses: the launcher and the
# power-up either side of it, and cover at each corner; and the names of
# every crossing.
static func _middle(town: BigMap) -> void:
	town.pickup("launcher", Vector3(0, 0, 14))
	town.pickup(Pickups.POWER, Vector3(0, 0, -14))
	for corner: Vector2 in [Vector2(-16, -16), Vector2(16, 16), Vector2(16, -16), Vector2(-16, 16)]:
		town.cheese(Vector3(corner.x, 2.0, corner.y), Vector3(4, 4, 4))
	for x: float in LANES:
		for z: float in ACROSS:
			var name := "The Crossroads" if x == 0.0 and z == 0.0 else "%s %s" % [LANES[x], {-110.0: "North", 0.0: "Crossing", 110.0: "South"}[z]]
			town.place(name, Vector3(x, 0, z))


# The blocks of the west half, between the yard, the streets and the middle.
static func _blocks() -> Array[Rect2]:
	var found: Array[Rect2] = []
	var half := SIZE / 2.0
	var edges_x: Array[float] = [-YARD, -180.0 - DOWN_WIDTH / 2.0, -180.0 + DOWN_WIDTH / 2.0, -90.0 - DOWN_WIDTH / 2.0, -90.0 + DOWN_WIDTH / 2.0, -DOWN_WIDTH / 2.0]
	var edges_z: Array[float] = [-half.y, -118.0, -102.0, -11.0, 11.0, 102.0, 118.0, half.y]
	for column in range(0, edges_x.size(), 2):
		for row in range(0, edges_z.size(), 2):
			found.append(Rect2(edges_x[column], edges_z[row], edges_x[column + 1] - edges_x[column], edges_z[row + 1] - edges_z[row]))
	return found
