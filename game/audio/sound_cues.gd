extends Node
## Decides which sounds the arena makes, from what happens in the match: shots,
## bursts, hits, knock-outs, pickups and Overloads. The world adds this as a
## child; footsteps, jumps and reloads come from the players themselves.

var world: Node3D


func _init(of: Node3D) -> void:
	world = of
	name = "SoundCues"


func _ready() -> void:
	var match_: Match = world.match_
	match_.shot_shown.connect(_on_others_shot)
	match_.projectile_launched.connect(func(_id: int, weapon: String, from: Vector3, _velocity: Vector3) -> void:
		if weapon == Weapons.GRENADE:
			Sounds.play_at("throw", from, world, 0.0, 0.75)
		else:
			Sounds.play_at(weapon, from, world))
	match_.projectile_burst.connect(func(_id: int, weapon: String, at: Vector3) -> void:
		Sounds.play_at(burst_sound(weapon), at, world))
	match_.hit_confirmed.connect(func(_victim: int, _dealt: int, knocked_out: bool, head: bool) -> void:
		Sounds.play(hit_sound(knocked_out, head), -4.0))
	match_.hurt.connect(func(_attacker: int, _from: Vector3, _dealt: int) -> void:
		Sounds.play("hurt"))
	match_.picked_up.connect(func(id: int, _kind: String) -> void:
		_at_player("pickup", id))
	match_.overloaded.connect(func(id: int) -> void:
		_at_player("overload", id))


## The sound this player hears when their shot lands: one for a body hit, a
## sharper one for the head, and another for a knock-out.
static func hit_sound(knocked_out: bool, head: bool) -> String:
	if knocked_out:
		return "knock_out"
	return "hit_head" if head else "hit"


## The sound a projectile from `weapon` ends with: a burst, or for one that
## only strikes, a knock.
static func burst_sound(weapon: String) -> String:
	var stats := Weapons.find(weapon)
	return "burst" if stats == null or stats.splash_radius > 0.0 else "stick"


## This machine's own player fired `weapon` from `muzzle`.
func own_shot(weapon: String, muzzle: Vector3) -> void:
	if not Weapons.find(weapon).fires_projectile():
		Sounds.play_at(weapon, muzzle, world, -2.0, Sounds.varied(0.04))


func _on_others_shot(shooter: int, _ends: Array) -> void:
	var player: Node3D = world.player_node(shooter)
	var record: PlayerRecord = world.match_.state.player(shooter)
	if player != null and record != null:
		Sounds.play_at(record.weapon, player.muzzle.global_position, world, 0.0, Sounds.varied(0.04))


func _at_player(sound: String, id: int) -> void:
	var player: Node3D = world.player_node(id)
	if player != null:
		Sounds.play_at(sound, player.global_position + Vector3.UP, world)
