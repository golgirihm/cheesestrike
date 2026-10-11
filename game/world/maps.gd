class_name Maps
## The arenas, by id. Each is a scene with its Geometry, SpawnPoints (at least
## MatchSettings.MAX_PLAYERS), Pickups (markers with a `kind`) and a
## MenuCamera for the view behind the menu. The first two spawn points face
## each other with a clear view between them, and a Gouda Launcher lies in
## the middle, which the browser tests rely on.

const DEFAULT := "canyon"
const ORDER := ["canyon", "box"]
const NAMES := {"canyon": "Cheddar Canyon", "box": "Brie Box"}

const _SCENES := {
	"canyon": preload("res://world/maps/cheddar_canyon.tscn"),
	"box": preload("res://world/maps/brie_box.tscn"),
}


static func scene(id: String) -> PackedScene:
	return _SCENES.get(id, _SCENES[DEFAULT])


static func exists(id: Variant) -> bool:
	return id is String and _SCENES.has(id)
