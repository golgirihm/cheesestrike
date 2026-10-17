extends GutTest
## The weapons beyond the first few: the rifle and its scope, the carbine's
## bursts, the machine gun's weight, the pair of pistols, and the bow.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")
const Effects := preload("res://effects/effects.gd")
const CHEST := BodyShape.CHEST.y * BodyShape.SIZE

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3, weapon: String) -> Node3D:
	match_.add_player(id)
	var record := match_.state.player(id)
	if not record.weapons.has(weapon):
		record.weapons.append(weapon)
	record.weapon = weapon
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	match_.state_changed.emit()
	return player


func _eye_of(id: int) -> Vector3:
	var target: Node3D = world.player_node(id)
	var eye: Vector3 = BodyShape.eye_centres()[0]
	return target.global_position + Basis(Vector3.UP, target.rotation.y) * eye * BodyShape.SIZE


func _fly(seconds: float) -> void:
	for i in int(seconds * 60.0):
		match_._physics_process(1.0 / 60.0)


# --- Choosing among what is carried ---------------------------------------

func test_number_keys_count_off_what_is_carried() -> void:
	var carried := ["rifle", "pistol", "launcher"]
	assert_eq(Weapons.at_key(carried, 0), "pistol", "the sidearm first")
	assert_eq(Weapons.at_key(carried, 1), "launcher")
	assert_eq(Weapons.at_key(carried, 2), "rifle")
	assert_eq(Weapons.at_key(carried, 3), "", "no fourth")
	assert_eq(Weapons.at_key(carried, -1), "")
	for key in Weapons.KEYS:
		assert_true(InputMap.has_action("weapon_%d" % (key + 1)))
	# A loadout and everything that can be found fit on the keys there are.
	assert_lte(2 + Weapons.in_slot(Weapons.POWER).size(), Weapons.KEYS)


func test_every_weapon_fits_in_what_is_sent() -> void:
	assert_lt(Weapons.ORDER.size(), 31, "one bit each")
	assert_eq(Weapons.ORDER.slice(0, 5), ["pistol", "blaster", "shotgun", "grater", "launcher"], "added to at the end only")
	var record := PlayerRecord.new(1)
	record.weapons = Weapons.ids()
	record.weapon = "bow"
	var copy := PlayerRecord.from_packed(record.to_packed())
	assert_eq(copy.weapons, Weapons.ids())
	assert_eq(copy.weapon, "bow")


# --- The rifle -------------------------------------------------------------

func test_the_rifle_rewards_a_head_shot() -> void:
	var rifle := Weapons.find("rifle")
	assert_lt(rifle.damage, float(PlayerRecord.MAX_HEALTH), "a body shot leaves them standing")
	assert_gt(rifle.damage * 2.0, float(PlayerRecord.MAX_HEALTH), "two do not")
	assert_gte(rifle.damage * rifle.headshot_multiplier, float(PlayerRecord.MAX_HEALTH), "a head shot knocks out")
	assert_eq(rifle.damage_at(200.0), rifle.damage, "as hard at any distance")
	assert_gte(rifle.fire_interval, 1.0, "slow")
	assert_eq(rifle.spread(true), 0.0, "exact through the scope")
	assert_gte(rifle.spread(false), 8.0, "and hopeless without it")
	assert_true(rifle.scoped)
	for id: String in Weapons.ids():
		if id != "rifle":
			assert_false(Weapons.find(id).scoped, id)
			assert_gt(Weapons.find(id).aim_zoom, rifle.aim_zoom, "%s zooms less" % id)


func test_a_rifle_shot_to_the_head_knocks_out_and_one_to_the_body_does_not() -> void:
	var shooter := _add(1, Vector3(0, 0, 9), "rifle")
	_add(2, Vector3(0, 0, -9), "blaster")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, CHEST, -9) - origin).normalized(), 1, "rifle", WeaponStats.AIMED)
	assert_eq(match_.state.player(2).health, 25)
	match_.state.player(2).health = 100
	match_._clock += 5.0
	match_._shot(1, origin, (_eye_of(2) - origin).normalized(), 1, "rifle", WeaponStats.AIMED)
	assert_false(match_.state.player(2).alive)


func test_the_scope_narrows_the_view_and_takes_the_weapon_out_of_it() -> void:
	var own := _add(1, Vector3(0, 0, 9), "rifle")
	own.equip("rifle")
	own._switch_left = 0.0
	assert_false(own.is_scoped())
	assert_true(own.gun.visible)
	own._aim = 1.0
	own._animate(0.0)
	assert_true(own.is_scoped())
	assert_false(own.gun.visible, "the scope is looked through, not at")
	own.equip("pistol")
	own._animate(0.0)
	assert_false(own.is_scoped(), "only a scoped weapon")
	assert_true(own.gun.visible)


func test_the_scope_blacks_out_all_but_a_circle() -> void:
	assert_almost_eq(HUD.scope_radius(Vector2(1280, 720)), 720 * 0.44, 0.01)
	assert_almost_eq(HUD.scope_radius(Vector2(400, 900)), 400 * 0.44, 0.01, "within the screen whichever way it is held")


func test_others_see_a_scope_glint_only_while_it_is_looked_through() -> void:
	_add(1, Vector3(0, 0, 9), "pistol")
	var other := _add(2, Vector3(0, 0, -9), "rifle")
	other._show_weapon("rifle")
	var glint: Node3D = other.gun.find_child("Glint", true, false)
	assert_not_null(glint)
	other._animate(0.0)
	assert_false(glint.visible)
	other._pose = other.POSE_AIMING
	other._animate(1.0)
	assert_true(glint.visible)
	other._pose = 0
	other._animate(1.0)
	assert_false(glint.visible)


func test_the_owner_never_sees_their_own_glint() -> void:
	var own := _add(1, Vector3(0, 0, 9), "rifle")
	own.equip("rifle")
	own._aim = 1.0
	own._animate(0.0)
	assert_false(own._glint.visible)


# --- Breath ----------------------------------------------------------------

func test_a_scope_sways_and_a_held_breath_steadies_it() -> void:
	var breath := Breath.new()
	var loose := 0.0
	for step in 300:
		breath.tick(1.0 / 60.0, false)
		loose = maxf(loose, breath.sway().length())
	assert_gt(loose, Breath.SWAY * 0.5, "it moves")
	assert_lt(loose, Breath.SWAY * 1.5, "but not wildly")
	var held := 0.0
	for step in 120:
		breath.tick(1.0 / 60.0, true)
		if step > 30:
			held = maxf(held, breath.sway().length())
	assert_true(breath.holding)
	assert_lt(held, loose * 0.2, "nearly still")
	assert_lt(breath.air, 1.0)


func test_a_breath_runs_out_and_has_to_be_got_back() -> void:
	var breath := Breath.new()
	breath.tick(Breath.LUNGS + 0.1, true)
	assert_eq(breath.air, 0.0)
	assert_false(breath.holding)
	assert_true(breath.winded)
	breath.tick(0.5, true)
	assert_false(breath.holding, "not again until some is back")
	breath.tick(Breath.RECOVERY * Breath.WINDED_UNTIL, false)
	assert_false(breath.winded)
	breath.tick(0.1, true)
	assert_true(breath.holding)
	breath.tick(Breath.RECOVERY, false)
	assert_eq(breath.air, 1.0, "and all of it in time")


func test_the_sway_is_the_same_whenever_it_is_asked() -> void:
	assert_eq(Breath.sway_at(3.2), Breath.sway_at(3.2))
	assert_ne(Breath.sway_at(3.2), Breath.sway_at(4.0))


func test_only_a_scoped_weapon_sways_the_view() -> void:
	var own := _add(1, Vector3(0, 0, 9), "rifle")
	own.equip("rifle")
	own._switch_left = 0.0
	own._aim = 1.0
	own.breath._time = 1.0
	own._process(0.0)
	assert_ne(own._scope_sway, Vector2.ZERO)
	own.equip("pistol")
	own._process(0.0)
	assert_eq(own._scope_sway, Vector2.ZERO)


# --- The carbine -----------------------------------------------------------

func test_one_pull_of_the_carbines_trigger_fires_three() -> void:
	var carbine := WeaponState.new(Weapons.find("carbine"))
	var stats := carbine.stats
	assert_eq(stats.burst, 3)
	assert_false(stats.automatic)
	assert_true(carbine.fire())
	assert_true(carbine.in_burst())
	assert_false(carbine.fire(), "not all at once")
	carbine.tick(stats.burst_interval)
	assert_true(carbine.fire())
	assert_true(carbine.in_burst())
	carbine.tick(stats.burst_interval)
	assert_true(carbine.fire())
	assert_false(carbine.in_burst(), "three, and it waits for the trigger")
	carbine.tick(stats.burst_interval)
	assert_false(carbine.fire(), "a rest before the next burst")
	carbine.tick(stats.burst_rest())
	assert_true(carbine.fire())
	assert_eq(carbine.ammo, stats.magazine - 4)
	assert_almost_eq(stats.burst_interval * 2.0 + stats.burst_rest(), stats.fire_interval, 0.0001, "a burst and its rest are the weapon's interval")


func test_a_burst_ends_with_the_magazine() -> void:
	var carbine := WeaponState.new(Weapons.find("carbine"))
	carbine.ammo = 2
	assert_true(carbine.fire())
	carbine.tick(carbine.stats.burst_interval)
	assert_true(carbine.fire())
	assert_false(carbine.in_burst(), "nothing left to follow")
	assert_true(carbine.is_reloading())


func test_changing_weapon_ends_a_burst() -> void:
	var own := _add(1, Vector3(0, 0, 9), "carbine")
	own.equip("carbine")
	own.weapon.cooldown = 0.0
	own.weapon.fire()
	var carbine: WeaponState = own.weapon
	assert_true(carbine.in_burst())
	own.equip("pistol")
	assert_false(carbine.in_burst())


func test_the_host_lets_a_burst_through_but_not_bursts_too_close_together() -> void:
	var rules := ShotRules.new()
	var carbine := Weapons.find("carbine")
	var allowed := 0
	# A changed game firing a burst's worth every burst interval for a second.
	var time := 10.0
	while time < 11.0:
		if rules.allow_shot(carbine, time):
			allowed += 1
		time += carbine.burst_interval
	var honest := int(1.0 / carbine.fire_interval + 1) * carbine.burst
	assert_gte(allowed, 3, "a burst")
	assert_lte(allowed, honest + int(ShotRules.BURST), "about what the weapon can do")


func test_a_burst_of_the_carbine_outdoes_the_sidearm() -> void:
	var carbine := Weapons.find("carbine")
	assert_gt(carbine.damage * carbine.burst / carbine.fire_interval, 2.0 * Weapons.find("pistol").damage / Weapons.find("pistol").fire_interval)


# --- The machine gun -------------------------------------------------------

func test_the_machine_gun_is_slow_to_bring_up_and_heavy_to_carry() -> void:
	var lmg := Weapons.find("lmg")
	for id: String in Weapons.ids():
		if id != "lmg":
			var other := Weapons.find(id)
			assert_gt(lmg.draw_time, other.draw_time, id)
			assert_gt(lmg.magazine, other.magazine, id)
			assert_gt(lmg.reload_time, other.reload_time, id)
			assert_lt(lmg.carry_speed, other.carry_speed, id)
	var own := _add(1, Vector3(0, 0, 9), "lmg")
	own.equip("pistol")
	own.equip("lmg")
	assert_almost_eq(own._switch_left, lmg.draw_time, 0.0001)
	assert_almost_eq(own.weapon.cooldown, lmg.draw_time, 0.0001, "and can't fire until it is up")
	assert_almost_eq(own.switch_lowering(lmg.draw_time, lmg.draw_time), 1.0, 0.0001)
	assert_almost_eq(own.switch_lowering(0.0, lmg.draw_time), 0.0, 0.0001)


# --- The pair of pistols ---------------------------------------------------

func test_the_pair_of_pistols_trades_aim_for_fire() -> void:
	var pair := Weapons.find("pistols")
	var single := Weapons.find("pistol")
	assert_eq(pair.slot, Weapons.SIDE)
	assert_true(pair.unlimited())
	assert_gt(pair.damage / pair.fire_interval, single.damage / single.fire_interval, "more fire")
	assert_gt(pair.spread(true), single.spread(true) * 2.0, "less aim")
	assert_gt(pair.aim_zoom, single.aim_zoom, "with no sights to bring up")
	assert_lt(pair.damage_at(50.0), single.damage_at(50.0), "and less reach")
	var model := WeaponModels.build("pistols")
	assert_almost_eq((model.get_node("LeftPistol") as Node3D).position.x, -WeaponModels.PAIR_GAP, 0.0001, "the other hand has its own")
	model.free()


func test_the_dual_burratas_fire_a_shot_from_each_for_each_pull() -> void:
	var burratas := Weapons.find("burratas")
	var curds := Weapons.find("pistols")
	assert_eq(burratas.display_name, "Dual Burratas")
	assert_eq(burratas.slot, Weapons.SIDE)
	assert_has(Weapons.SIDES, "burratas", "offered as a sidearm")
	assert_eq(Weapons.ORDER.back(), "burratas", "added at the end, so no other weapon is sent differently")
	assert_true(burratas.unlimited())
	assert_false(burratas.automatic, "a pull at a time")
	assert_eq(burratas.burst, 2, "a shot from each")
	assert_gt(burratas.damage, curds.damage, "each hitting harder than the twin curds'")
	assert_lt(burratas.damage * burratas.burst / burratas.fire_interval, curds.damage / curds.fire_interval, "though doing less in a second than the twin curds held down")
	assert_lt(burratas.spread(true), curds.spread(true), "aimed more closely")
	assert_gt(burratas.damage_at(40.0), curds.damage_at(40.0), "and reaching further")
	var model := WeaponModels.build("burratas")
	var second := model.get_node("LeftPistol") as Node3D
	assert_almost_eq(second.position.x, -WeaponModels.PAIR_GAP, 0.0001, "the second in the left hand")
	assert_gt(second.get_child_count(), 0, "drawn there")
	model.free()


# --- The bow ---------------------------------------------------------------

func test_a_breadstick_hurts_only_whoever_it_strikes() -> void:
	var shooter := _add(1, Vector3(0, 0, 5), "bow")
	_add(2, Vector3(0, 0, -3), "blaster")
	_add(3, Vector3(1.2, 0, -3), "blaster")
	watch_signals(match_)
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, CHEST + 0.1, -3) - origin).normalized(), 1, "bow", WeaponStats.AIMED)
	assert_signal_emitted(match_, "projectile_launched")
	_fly(1.0)
	assert_signal_emitted(match_, "projectile_burst")
	assert_eq(match_.state.player(2).health, 100 - int(Weapons.find("bow").impact_damage))
	assert_eq(match_.state.player(3).health, 100, "standing right beside them")
	assert_eq(match_.state.player(1).health, 100)


func test_a_breadstick_to_the_head_knocks_out() -> void:
	var bow := Weapons.find("bow")
	assert_gte(bow.impact_damage * bow.headshot_multiplier, float(PlayerRecord.MAX_HEALTH))
	var stick := Projectile.new(1, 1, bow, Vector3(0, 1.5, 5), Vector3.FORWARD)
	var none := func(_from: Vector3, _to: Vector3) -> float: return INF
	var eye: Vector3 = BodyShape.eye_centres()[0] * BodyShape.SIZE
	stick.position = Vector3(eye.x, eye.y, 3.0)
	var targets := {2: HitScan.pose(Vector3.ZERO, 0.0)}
	var burst: Variant = null
	for step in 60:
		burst = stick.advance(1.0 / 240.0, none, targets)
		if burst != null:
			break
	assert_not_null(burst)
	assert_eq(stick.struck, 2)
	assert_true(stick.struck_head)


func test_a_breadstick_that_misses_hurts_nobody_and_falls() -> void:
	var bow := Weapons.find("bow")
	assert_gt(bow.projectile_gravity, 0.0)
	var stick := Projectile.new(1, 1, bow, Vector3(0, 1.5, 0), Vector3.FORWARD)
	var none := func(_from: Vector3, _to: Vector3) -> float: return INF
	for step in 30:
		stick.advance(1.0 / 60.0, none, {})
	assert_lt(stick.position.y, 1.5)
	assert_eq(stick.struck, 0)
	var shooter := _add(1, Vector3(0, 0, 5), "bow")
	_add(2, Vector3(0, 0, -3), "blaster")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(3, CHEST, -3) - origin).normalized(), 1, "bow", WeaponStats.AIMED)
	_fly(1.5)
	assert_eq(match_.state.player(2).health, 100)


func test_a_breadstick_makes_a_knock_and_a_wheel_a_burst() -> void:
	const CUES := preload("res://audio/sound_cues.gd")
	assert_eq(CUES.burst_sound("bow"), "stick")
	assert_eq(CUES.burst_sound("launcher"), "burst")
	assert_eq(CUES.burst_sound(Weapons.GRENADE), "burst")
	assert_eq(CUES.burst_sound("banana"), "burst")


func test_a_breadstick_is_drawn_flying_point_first_and_leaves_no_burst() -> void:
	var along: Transform3D = Effects.stick_transform(Vector3(1, 2, 3), Vector3(0, 0, -20))
	assert_almost_eq(along.basis.y, Vector3(0, 0, -1), Vector3.ONE * 0.0001)
	assert_eq(along.origin, Vector3(1, 2, 3))
	var before: int = world.effects.get_child_count()
	world.effects.launch_projectile(7, "bow", Vector3(0, 2, 0), Vector3(0, 0, -30))
	assert_eq(world.effects.projectiles_in_flight(), 1)
	world.effects._process(0.1)
	world.effects.burst_projectile(7, "bow", Vector3(0, 2, -3))
	assert_eq(world.effects.projectiles_in_flight(), 0)
	await wait_process_frames(2)
	assert_eq(world.effects.get_child_count(), before, "nothing left behind")
