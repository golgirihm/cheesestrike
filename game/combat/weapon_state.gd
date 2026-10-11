class_name WeaponState
extends RefCounted
## One weapon in a player's hands: how much is in the magazine and whether it is
## ready to fire. The player holding it uses this to decide when to shoot and
## what to show; the host keeps its own count (see ShotRules).

var stats: WeaponStats
var ammo: int
## Seconds until the next shot is allowed.
var cooldown := 0.0
## Seconds until the reload in progress finishes, or 0 when not reloading.
var reload_left := 0.0
## Reloads finished since this weapon was picked up.
var reloads := 0


func _init(weapon: WeaponStats) -> void:
	stats = weapon
	ammo = weapon.magazine


func is_reloading() -> bool:
	return reload_left > 0.0


func can_fire() -> bool:
	return ammo > 0 and cooldown <= 0.0 and not is_reloading()


## Uses one round if the weapon can fire, and reports whether it did. Firing the
## last round starts a reload.
func fire() -> bool:
	if not can_fire():
		return false
	ammo -= 1
	# Added rather than set, so a frame that runs late doesn't slow the rate.
	cooldown += stats.fire_interval
	if ammo == 0:
		start_reload()
	return true


## Starts reloading unless already reloading or full. Reports whether it did.
func start_reload() -> bool:
	if is_reloading() or ammo >= stats.magazine:
		return false
	reload_left = stats.reload_time
	return true


func tick(delta: float) -> void:
	# The cooldown can't bank time while idle: a held trigger fires at the
	# weapon's rate, never in a burst.
	cooldown = maxf(cooldown - delta, 0.0)
	if is_reloading():
		reload_left -= delta
		if reload_left <= 0.0:
			reload_left = 0.0
			ammo = stats.magazine
			reloads += 1


## Fills the magazine at once, as on respawning.
func refill() -> void:
	ammo = stats.magazine
	reload_left = 0.0
	cooldown = 0.0
