class_name Warmup
## Getting the first fight ready before it happens. The first time anything
## is drawn, the graphics hardware has work to do for it, and in a browser
## that is long enough to be felt: the first shot, the first burst and the
## first pickup each made the game catch. So when the game starts, one of
## everything a fight draws is put in front of the view for a few frames,
## behind a screen that says the game is getting ready, and cleared away.

## How many frames the things are drawn for.
const FRAMES := 4
## How far in front of the view they are put, in metres.
const AHEAD := 9.0


## One of every model a fight shows, in a row: a character, each weapon and
## each pickup.
static func props() -> Node3D:
	var holder := Node3D.new()
	holder.name = "Warmup"
	var models: Array[Node3D] = []
	var character: Node3D = preload("res://player/soldier_character.gd").new()
	character.name = "Character"
	models.append(character)
	for weapon: String in Weapons.ids():
		models.append(WeaponModels.build(weapon))
	for kind: String in [Pickups.HEALTH, Pickups.AMMO, Pickups.ARMOUR, Pickups.POWER]:
		models.append(PickupModels.build(kind))
	models.append(PickupModels.base())
	for index in models.size():
		holder.add_child(models[index])
		models[index].position = Vector3((index - models.size() / 2.0) * 0.6, 0, 0)
	return holder


## Sets off one of every effect a fight shows, at `at`, in `effects`.
static func fire(effects: Node3D, at: Vector3) -> void:
	for weapon: String in ["", effects.TRAILED[0]]:
		effects.show_shot(at, [at + Vector3(0.5, -AHEAD, -1.0), at + Vector3(2, 0, -3)], Vector3.RIGHT, weapon)
	effects.launch_projectile(-1, "launcher", at, Vector3.ZERO)
	effects.burst_projectile(-1, "launcher", at)
	effects.launch_projectile(-2, Weapons.GRENADE, at, Vector3.ZERO)
	effects.launch_projectile(-3, "bow", at, Vector3.FORWARD)
	effects.landing(at, 1.0)
	effects.sparkle(at, Pickups.HEALTH)
	effects.overload_flare(at, Color.ORANGE)
	effects.crumbs(at, Color.YELLOW, 12)
