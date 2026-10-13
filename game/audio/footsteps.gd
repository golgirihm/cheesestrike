class_name Footsteps
## What a footstep sounds like: by what is trodden on, by whether the walker
## runs or creeps, and by who they are to the listener. Opponents' steps are
## the ones worth hearing, so they are the clearest.

## The step sound for each thing an arena is made of (see Textures).
const CUES := {
	Textures.ASPHALT: "step", Textures.TILE: "step", Textures.COBBLE: "step", Textures.PLASTER: "step",
	Textures.ROCK: "step_rock", Textures.DIRT: "step_grass", Textures.WOOD: "step_wood", Textures.METAL: "step_metal",
	Textures.CHEESE: "step_cheese", Textures.BREAD: "step_cheese",
}
## Who a walker is to the listener.
const OWN := "own"
const TEAMMATE := "teammate"
const OPPONENT := "opponent"
## How loud each one's steps are, in decibels.
const LEVELS := {OWN: -9.0, TEAMMATE: -10.0, OPPONENT: -2.0}
## Added for one who runs, and for one fully crouched.
const RUNNING_DB := 2.5
const CROUCHED_DB := -10.0
const WORLD_MASK := 1


## The step sound on `surface`, one of Textures' kinds.
static func cue(surface: String) -> String:
	return CUES.get(surface, "step")


## What is under feet at `feet` in the arena `map`: one of Textures' kinds,
## or "" in the air.
static func surface_under(map: Node3D, feet: Vector3) -> String:
	if map == null or not map.is_inside_tree():
		return ""
	var space := map.get_world_3d().direct_space_state
	var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(feet + Vector3.UP * 0.4, feet + Vector3.DOWN * 0.6, WORLD_MASK))
	if hit.is_empty():
		return ""
	return Textures.kind_for(hit.collider, String(map.get_meta("scenery", Scenery.MEADOW)))


## Who the walker with record `walker` is to the listener with record
## `listener`, in a match with `teams` or without.
static func relation(listener: PlayerRecord, walker: PlayerRecord, teams: bool) -> String:
	if listener == null or walker == null:
		return OPPONENT
	if listener == walker:
		return OWN
	if teams and listener.team != MatchState.NO_TEAM and listener.team == walker.team:
		return TEAMMATE
	return OPPONENT


## How loud a step is, in decibels, for a walker who is `who` to the
## listener, `running` or not, and crouched by `crouch` (0 to 1).
static func level(who: String, running: bool, crouch: float) -> float:
	return LEVELS.get(who, LEVELS[OPPONENT]) + (RUNNING_DB if running else 0.0) + CROUCHED_DB * clampf(crouch, 0.0, 1.0)
