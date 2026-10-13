class_name Maps
## The arenas, by id. Each is a scene with its Geometry, SpawnPoints (at least
## MatchSettings.MAX_PLAYERS), Pickups (markers with a `kind`) and a
## MenuCamera for the view behind the menu. The first two spawn points face
## each other with a clear view between them, and a Gouda Launcher lies in
## the middle, which the browser tests rely on.
##
## A scene lays its Geometry out as plain boxes; Scenery draws them as they
## are seen, and adds the fence at the edge and the land beyond. The four
## Wall bodies are the edge: solid, far taller than anyone can be thrown,
## and not drawn. A Block is cheese, a Building a tower, and a Car or a Bus
## is only its collision shapes, drawn by Scenery. An arena may also have
## JumpPads and Ladders (see Fixtures). Its Places are markers with a
## `label`: the names of its parts, each the name of everywhere nearer to
## it than to any other.

const DEFAULT := "canyon"
const ORDER := ["canyon", "box", "city", "dairy"]
## The arenas the session list's server knows of. A session on any other is
## listed as being on the first of these (see MatchSettings.advertised).
const LISTED := ["canyon", "box", "city"]
## The practice range: an arena for one, not among those a host chooses
## from. Besides what every arena has, it has Targets, markers with a
## `distance`, where a target stands to be shot at; and its pads hold every
## weapon, laid out as a rack.
const RANGE := "range"
const NAMES := {"canyon": "Cheddar Canyon", "box": "Brie Box", "city": "Curd City", "dairy": "Double Cream Dairy", RANGE: "Practice range"}

const _SCENES := {
	"canyon": preload("res://world/maps/cheddar_canyon.tscn"),
	"box": preload("res://world/maps/brie_box.tscn"),
	"city": preload("res://world/maps/curd_city.tscn"),
	"dairy": preload("res://world/maps/dairy.tscn"),
	RANGE: preload("res://world/maps/practice_range.tscn"),
}


static func scene(id: String) -> PackedScene:
	return _SCENES.get(id, _SCENES[DEFAULT])


static func exists(id: Variant) -> bool:
	return id is String and _SCENES.has(id)
