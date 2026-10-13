class_name WeaponState
extends RefCounted
## One weapon in a player's hands: how much is in the magazine and whether it is
## ready to fire. The player holding it uses this to decide when to shoot and
## what to show; the host keeps its own count (see ShotRules).

var stats: WeaponStats
var ammo: int
## Rounds carried besides those in the magazine. Not counted for a weapon
## that never runs out.
var reserve: int
## Seconds until the next shot is allowed.
var cooldown := 0.0
## Seconds until the reload in progress finishes, or 0 when not reloading.
var reload_left := 0.0
## Reloads finished since this weapon was picked up.
var reloads := 0
## Shots of the burst in progress still to come, for a weapon that fires in
## bursts: they follow the first without the trigger being asked.
var burst_left := 0


func _init(weapon: WeaponStats) -> void:
	stats = weapon
	ammo = weapon.magazine
	reserve = maxi(weapon.reserve, 0)


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
	if stats.burst > 1:
		burst_left = burst_left - 1 if burst_left > 0 else stats.burst - 1
		if ammo == 0:
			burst_left = 0
		cooldown += stats.burst_interval if burst_left > 0 else stats.burst_rest()
	else:
		# Added rather than set, so a frame that runs late doesn't slow the rate.
		cooldown += stats.fire_interval
	if ammo == 0:
		start_reload()
	return true


## Fires the weapon's other way, with `other`'s stats, if there are the
## rounds for it and the weapon is ready. Reports whether it did.
func fire_other(other: WeaponStats) -> bool:
	if other == null or ammo < other.rounds or cooldown > 0.0 or is_reloading():
		return false
	ammo -= other.rounds
	burst_left = 0
	cooldown += other.fire_interval
	if ammo == 0:
		start_reload()
	return true


## Whether a burst is under way, with shots still to come.
func in_burst() -> bool:
	return burst_left > 0


## Whether there is nothing left to fire or to reload with.
func is_dry() -> bool:
	return ammo <= 0 and not stats.unlimited() and reserve <= 0


## Adds `rounds` to the reserve, up to the most that can be carried. Reports
## whether any were taken.
func add_reserve(rounds: int) -> bool:
	if stats.unlimited() or reserve >= stats.reserve_max:
		return false
	reserve = mini(reserve + rounds, stats.reserve_max)
	return true


## Starts reloading unless already reloading or full, or with nothing to
## reload with. Reports whether it did.
func start_reload() -> bool:
	if is_reloading() or ammo >= stats.magazine:
		return false
	if not stats.unlimited() and reserve <= 0:
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
			var wanted := stats.magazine - ammo
			var taken := wanted if stats.unlimited() else mini(wanted, reserve)
			ammo += taken
			if not stats.unlimited():
				reserve -= taken
			reloads += 1


## Fills the magazine at once and gives back the reserve, as on respawning.
func refill() -> void:
	ammo = stats.magazine
	reserve = maxi(stats.reserve, 0)
	reload_left = 0.0
	cooldown = 0.0
	burst_left = 0
