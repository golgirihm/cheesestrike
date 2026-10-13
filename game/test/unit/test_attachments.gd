extends GutTest
## Attachments: one thing fitted to the main weapon of a loadout, each a
## small gain for a small cost.

const WORLD := preload("res://world/world.tscn")
const MENU := preload("res://ui/settings_menu.gd")
## Every number a weapon has that an attachment might change.
const NUMBERS := ["damage", "headshot_multiplier", "fire_interval", "magazine", "reserve", "reserve_max", "reload_time", "max_range",
	"falloff_start", "falloff_end", "falloff_floor", "pellets", "spread_degrees", "hip_spread_degrees", "aim_zoom", "bloom_shots",
	"recoil_up", "recoil_cap", "recoil_side", "recoil_reset", "burst", "burst_interval", "draw_time", "carry_speed",
	"projectile_speed", "projectile_gravity", "fuse", "impact_damage", "splash_damage", "splash_radius", "knockback", "rounds"]

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	match_ = world.match_
	match_.start()


func after_each() -> void:
	Settings.set_value("loadout_main", Weapons.DEFAULT_MAIN)
	Settings.set_value("loadout_attachment", Attachments.IDS[0])


# The numbers of `fitted` that differ from `weapon`'s.
func _changed(weapon: WeaponStats, fitted: WeaponStats) -> Array:
	return NUMBERS.filter(func(number: String) -> bool: return weapon.get(number) != fitted.get(number))


func test_each_changes_only_what_it_says() -> void:
	for id: String in Weapons.MAINS:
		var weapon := Weapons.find(id)
		assert_same(Attachments.fitted(weapon, Attachments.NONE), weapon, "%s with nothing is itself" % id)
		var sight := Attachments.fitted(weapon, Attachments.SIGHT)
		for number: String in _changed(weapon, sight):
			assert_has(["aim_zoom", "spread_degrees", "hip_spread_degrees"], number, "%s sight" % id)
		assert_lt(sight.aim_zoom, weapon.aim_zoom, "%s: closer" % id)
		assert_lte(sight.spread_degrees, weapon.spread_degrees, id)
		assert_gt(sight.hip_spread_degrees, weapon.hip_spread_degrees, "%s: at a cost" % id)
		var grip := Attachments.fitted(weapon, Attachments.GRIP)
		for number: String in _changed(weapon, grip):
			assert_has(["recoil_up", "recoil_side", "carry_speed"], number, "%s grip" % id)
		assert_lte(grip.recoil_up, weapon.recoil_up, id)
		assert_lt(grip.carry_speed, weapon.carry_speed, "%s: at a cost" % id)
		var magazine := Attachments.fitted(weapon, Attachments.MAGAZINE)
		assert_eq(_changed(weapon, magazine), ["magazine", "reload_time"], "%s magazine" % id)
		assert_gt(magazine.magazine, weapon.magazine, id)
		assert_gt(magazine.reload_time, weapon.reload_time, "%s: at a cost" % id)
		for fitted: WeaponStats in [sight, grip, magazine]:
			assert_eq(fitted.id, id)
			assert_eq(fitted.display_name, weapon.display_name)
			assert_eq(fitted.alt, weapon.alt)
			assert_eq(fitted.scoped, weapon.scoped)
			assert_eq(fitted.automatic, weapon.automatic)


func test_the_weapon_itself_is_left_as_it_was() -> void:
	var blaster := Weapons.find("blaster")
	var rounds := blaster.magazine
	var longer := Attachments.fitted(blaster, Attachments.MAGAZINE)
	assert_eq(blaster.magazine, rounds)
	assert_ne(longer.magazine, rounds)
	assert_same(Attachments.fitted(blaster, Attachments.MAGAZINE), longer, "the same one each time")
	assert_same(Attachments.fitted(blaster, 99), blaster)
	assert_null(Attachments.fitted(null, Attachments.GRIP))


func test_every_attachment_has_a_name_and_a_place_in_the_settings() -> void:
	assert_eq(Attachments.IDS.size(), Attachments.KINDS.size())
	assert_eq(Settings.CHOICES["loadout_attachment"], Attachments.IDS)
	for kind: int in Attachments.KINDS:
		assert_true(Attachments.NAMES.has(kind))
		assert_eq(Attachments.from_id(Attachments.IDS[kind]), kind)
		assert_false(Attachments.text(kind).is_empty())
	assert_eq(Attachments.from_id("banana"), Attachments.NONE)
	assert_eq(Attachments.from_id(7), Attachments.NONE)
	assert_eq(Attachments.text(Attachments.GRIP), "Grip (less kick, a little slower)")
	assert_eq(Attachments.text(Attachments.NONE), "None")
	assert_false(Attachments.is_kind("grip"))
	assert_false(Attachments.is_kind(-1))


func test_a_loadout_carries_its_attachment_and_nonsense_is_none() -> void:
	assert_eq(Weapons.clean_loadout("shotgun", "pistol", Attachments.GRIP), ["shotgun", "pistol", Attachments.GRIP])
	assert_eq(Weapons.clean_loadout("shotgun", "pistol"), ["shotgun", "pistol", Attachments.NONE])
	assert_eq(Weapons.clean_loadout("shotgun", "pistol", 42)[2], Attachments.NONE)
	assert_eq(Weapons.clean_loadout("shotgun", "pistol", "grip")[2], Attachments.NONE)
	Settings.set_value("loadout_attachment", "magazine")
	assert_eq(Weapons.chosen_loadout()[2], Attachments.MAGAZINE)


func test_an_attachment_is_on_the_main_weapon_only() -> void:
	var record := PlayerRecord.new(1)
	record.main = "blaster"
	record.attachment = Attachments.MAGAZINE
	assert_same(record.stats("blaster"), Weapons.find("blaster"), "not until they come into play with it")
	record.disarm()
	assert_eq(record.fitted, Attachments.MAGAZINE)
	assert_eq(record.fitted_to, "blaster")
	assert_gt(record.stats("blaster").magazine, Weapons.find("blaster").magazine)
	assert_same(record.stats("pistol"), Weapons.find("pistol"))
	assert_same(record.stats("launcher"), Weapons.find("launcher"))
	assert_null(record.stats("banana"))
	assert_null(record.stats("shotgun_both"), "another way of firing is not a weapon to carry")


func test_a_loadout_with_no_main_weapon_has_nothing_fitted() -> void:
	var record := PlayerRecord.new(1)
	record.attachment = Attachments.GRIP
	record.disarm()
	assert_eq(record.fitted, Attachments.NONE)


func test_a_fitted_weapon_survives_the_trip_and_nonsense_does_not() -> void:
	var state := MatchState.new()
	var record := state.add_player(1)
	record.main = "grater"
	record.attachment = Attachments.SIGHT
	record.disarm()
	record.attachment = Attachments.GRIP
	var copy := MatchState.from_dict(state.to_dict()).player(1)
	assert_eq(copy.attachment, Attachments.GRIP, "chosen for next time")
	assert_eq(copy.fitted, Attachments.SIGHT, "and what is on it now")
	assert_eq(copy.fitted_to, "grater")
	var numbers := PackedInt32Array()
	numbers.resize(PlayerRecord._COUNT)
	numbers.fill(0)
	numbers[PlayerRecord._FITTED] = 13 | (9 << 4) | (200 << 8)
	var odd := PlayerRecord.from_packed(["X", numbers])
	assert_eq(odd.attachment, Attachments.NONE)
	assert_eq(odd.fitted, Attachments.NONE)
	assert_eq(odd.fitted_to, "")


func test_an_attachment_chosen_on_arrival_is_fitted_at_once() -> void:
	match_.add_player(5)
	match_._arm(5, "blaster", "pistol", Attachments.MAGAZINE)
	var record := match_.state.player(5)
	assert_eq(record.fitted, Attachments.MAGAZINE)
	assert_eq(record.fitted_to, "blaster")


func test_one_changed_in_the_middle_of_a_fight_waits_until_they_come_back() -> void:
	match_.add_player(5)
	match_.add_player(6)
	match_._arm(5, "blaster", "pistol", Attachments.GRIP)
	match_.state.damage(6, 5, 30.0)
	match_._arm(5, "blaster", "pistol", Attachments.MAGAZINE)
	var record := match_.state.player(5)
	assert_eq(record.fitted, Attachments.GRIP, "still what they came in with")
	assert_eq(record.attachment, Attachments.MAGAZINE, "but noted")
	match_.state.damage(6, 5, 500.0)
	assert_eq(record.fitted, Attachments.MAGAZINE)


func test_the_host_counts_a_longer_magazine_for_the_player_who_has_one_only() -> void:
	match_.add_player(1)
	match_.add_player(2)
	match_._arm(1, "blaster", "pistol", Attachments.MAGAZINE)
	match_._arm(2, "blaster", "pistol")
	var plain := Weapons.find("blaster")
	var longer := Attachments.fitted(plain, Attachments.MAGAZINE)
	var allowed := {1: 0, 2: 0}
	for id: int in [1, 2]:
		var weapon := match_.state.player(id).stats("blaster")
		for shot in longer.magazine + 5:
			if match_._rules_for(id).allow_shot(weapon, 10.0 + shot * plain.fire_interval):
				allowed[id] += 1
	assert_eq(allowed[1], longer.magazine)
	assert_eq(allowed[2], plain.magazine)


func test_the_owner_carries_their_weapon_as_fitted() -> void:
	Settings.set_value("loadout_attachment", "magazine")
	match_.add_player(1)
	var own: Node3D = world._create_player([1, Vector3.ZERO])
	world.players.add_child(own)
	var plain := Weapons.find("blaster")
	assert_gt(own.arsenal["blaster"].stats.magazine, plain.magazine)
	assert_eq(own.weapon.ammo, own.weapon.stats.magazine, "and it comes full")
	assert_same(own.arsenal["pistol"].stats, Weapons.find("pistol"))
	# Changed in the settings, it is theirs when they next come back.
	match_.state.player(1).health = 50
	Settings.set_value("loadout_attachment", "grip")
	assert_gt(own.arsenal["blaster"].stats.magazine, plain.magazine, "not yet")
	match_.state.damage(1, 1, 500.0, true)
	match_.state_changed.emit()
	assert_eq(own.arsenal["blaster"].stats.magazine, plain.magazine)
	assert_lt(own.arsenal["blaster"].stats.recoil_up, plain.recoil_up)
	assert_same(own.weapon, own.arsenal["blaster"], "in hand, as fitted")


func test_the_settings_say_what_is_fitted_and_step_through_them() -> void:
	assert_eq(MENU.loadout_text("loadout_attachment"), "Fitted to it: None")
	var menu: Control = MENU.new()
	add_child_autofree(menu)
	menu.refresh()
	var button: Button = menu.find_child("Setting_loadout_attachment", true, false)
	assert_not_null(button)
	button.pressed.emit()
	assert_eq(Settings.get_value("loadout_attachment"), "sight")
	assert_eq(button.text, "Fitted to it: Sight (closer and tighter aimed, wilder from the hip)")
	for press in Attachments.IDS.size() - 1:
		button.pressed.emit()
	assert_eq(Settings.get_value("loadout_attachment"), "none", "and round again")
