extends GutTest
## Loadouts: the weapons a player chooses to come into play with.

const WORLD := preload("res://world/world.tscn")
const MENU := preload("res://ui/settings_menu.gd")

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	match_ = world.match_
	match_.start()


func after_each() -> void:
	Settings.set_value("loadout_main", Weapons.DEFAULT_MAIN)
	Settings.set_value("loadout_side", Weapons.STARTING)


func test_every_weapon_has_a_slot_and_the_lists_agree_with_them() -> void:
	for id: String in Weapons.ids():
		assert_has([Weapons.MAIN, Weapons.SIDE, Weapons.POWER], Weapons.find(id).slot, id)
	assert_eq(Weapons.MAINS, Weapons.in_slot(Weapons.MAIN))
	assert_eq(Weapons.SIDES, Weapons.in_slot(Weapons.SIDE))
	assert_has(Weapons.MAINS, Weapons.DEFAULT_MAIN)
	assert_has(Weapons.SIDES, Weapons.STARTING)
	assert_has(Weapons.in_slot(Weapons.POWER), "launcher")
	assert_eq(Settings.CHOICES["loadout_main"], Weapons.MAINS)
	assert_eq(Settings.CHOICES["loadout_side"], Weapons.SIDES)


func test_every_sidearm_never_runs_out() -> void:
	# A weapon run dry gives way to the sidearm, so there must always be one to give way to.
	for id: String in Weapons.SIDES:
		assert_true(Weapons.find(id).unlimited(), id)


func test_a_loadout_from_another_machine_is_put_right() -> void:
	assert_eq(Weapons.clean_loadout("shotgun", "pistol").slice(0, 2), ["shotgun", "pistol"])
	assert_eq(Weapons.clean_loadout("launcher", "pistol").slice(0, 2), ["", "pistol"], "a power weapon is found, not chosen")
	assert_eq(Weapons.clean_loadout("pistol", "blaster").slice(0, 2), ["", "pistol"], "each in its own slot")
	assert_eq(Weapons.clean_loadout(7, null).slice(0, 2), ["", Weapons.STARTING])
	assert_eq(Weapons.clean_loadout("banana", "spoon").slice(0, 2), ["", Weapons.STARTING])


func test_the_chosen_loadout_comes_from_the_settings() -> void:
	assert_eq(Weapons.chosen_loadout().slice(0, 2), [Weapons.DEFAULT_MAIN, Weapons.STARTING], "to begin with")
	Settings.set_value("loadout_main", "grater")
	assert_eq(Weapons.chosen_loadout().slice(0, 2), ["grater", Weapons.STARTING])


func test_a_record_with_no_loadout_has_only_the_sidearm() -> void:
	var record := PlayerRecord.new(1)
	assert_eq(record.weapons, [Weapons.STARTING])
	assert_true(record.has_only_loadout())
	record.disarm()
	assert_eq(record.weapons, [Weapons.STARTING])
	assert_eq(record.weapon, Weapons.STARTING)


func test_disarming_leaves_a_player_their_loadout_with_the_main_weapon_in_hand() -> void:
	var record := PlayerRecord.new(1)
	record.main = "shotgun"
	record.disarm()
	assert_eq(record.weapons, [Weapons.STARTING, "shotgun"])
	assert_eq(record.weapon, "shotgun")
	assert_true(record.has_only_loadout())
	record.weapons.append("launcher")
	assert_false(record.has_only_loadout(), "they have found something")
	record.disarm()
	assert_eq(record.weapons, [Weapons.STARTING, "shotgun"], "which a knock-out loses them")


func test_a_loadout_survives_the_trip_and_nonsense_does_not() -> void:
	var state := MatchState.new()
	var record := state.add_player(1)
	record.main = "grater"
	record.disarm()
	var copy := MatchState.from_dict(state.to_dict()).player(1)
	assert_eq(copy.main, "grater")
	assert_eq(copy.side, Weapons.STARTING)
	assert_eq(copy.weapons, [Weapons.STARTING, "grater"])
	var numbers := PackedInt32Array()
	numbers.resize(PlayerRecord._COUNT)
	numbers.fill(0)
	numbers[PlayerRecord._MAIN] = Weapons.ORDER.find("launcher")
	numbers[PlayerRecord._SIDE] = 999
	var odd := PlayerRecord.from_packed(["X", numbers])
	assert_eq(odd.main, "")
	assert_eq(odd.side, Weapons.STARTING)


func test_a_loadout_chosen_before_joining_is_there_on_arrival() -> void:
	match_._arm(5, "shotgun", "pistol")
	assert_null(match_.state.player(5))
	match_.add_player(5)
	var record := match_.state.player(5)
	assert_eq(record.weapons, [Weapons.STARTING, "shotgun"])
	assert_eq(record.weapon, "shotgun", "in hand")


func test_a_loadout_chosen_just_after_joining_takes_effect_at_once() -> void:
	match_.add_player(5)
	assert_eq(match_.state.player(5).weapons, [Weapons.STARTING], "until they say otherwise")
	watch_signals(match_)
	match_._arm(5, "grater", "pistol")
	assert_eq(match_.state.player(5).weapons, [Weapons.STARTING, "grater"])
	assert_signal_emitted(match_, "state_changed")


func test_a_loadout_changed_in_the_middle_of_a_fight_waits_until_they_come_back() -> void:
	match_.add_player(5)
	match_.add_player(6)
	match_._arm(5, "blaster", "pistol")
	match_.state.damage(6, 5, 30.0)
	match_._arm(5, "shotgun", "pistol")
	var record := match_.state.player(5)
	assert_eq(record.weapons, [Weapons.STARTING, "blaster"], "still what they came in with")
	assert_eq(record.main, "shotgun", "but noted")
	match_.state.damage(6, 5, 500.0)
	assert_eq(record.weapons, [Weapons.STARTING, "shotgun"], "theirs when they are knocked out and come back")
	assert_eq(record.weapon, "shotgun")


func test_a_loadout_is_not_swapped_under_a_player_who_has_found_a_weapon() -> void:
	match_.add_player(5)
	match_._arm(5, "blaster", "pistol")
	match_.state.player(5).weapons.append("launcher")
	match_._arm(5, "grater", "pistol")
	assert_eq(match_.state.player(5).weapons, [Weapons.STARTING, "blaster", "launcher"])


func test_the_host_refuses_a_loadout_that_is_not_allowed() -> void:
	match_.add_player(5)
	match_._arm(5, "launcher", "blaster")
	var record := match_.state.player(5)
	assert_eq(record.main, "")
	assert_eq(record.side, Weapons.STARTING)
	assert_eq(record.weapons, [Weapons.STARTING])


func test_this_machines_own_player_comes_in_with_what_the_settings_say() -> void:
	Settings.set_value("loadout_main", "shotgun")
	match_.add_player(1)
	assert_eq(match_.state.player(1).weapons, [Weapons.STARTING, "shotgun"])
	var own: Node3D = world._create_player([1, Vector3.ZERO])
	world.players.add_child(own)
	assert_eq(own.weapon.stats.id, "shotgun", "in hand")
	assert_eq(own.arsenal.keys(), [Weapons.STARTING, "shotgun"])
	# Changed in the settings in the middle of a session: the host hears.
	match_.state.player(1).health = 50
	Settings.set_value("loadout_main", "grater")
	assert_eq(match_.state.player(1).main, "grater")
	assert_eq(own.weapon.stats.id, "shotgun", "theirs next time")


func test_a_weapon_run_dry_gives_way_to_the_sidearm_of_the_loadout() -> void:
	match_.add_player(1)
	var own: Node3D = world._create_player([1, Vector3.ZERO])
	world.players.add_child(own)
	assert_eq(own._fallback_weapon(), Weapons.STARTING)


func test_the_settings_say_what_is_chosen_and_step_through_the_weapons() -> void:
	assert_eq(MENU.loadout_text("loadout_main"), "Main weapon: Cheddar Blaster")
	assert_eq(MENU.loadout_text("loadout_side"), "Sidearm: Curd Pistol")
	var menu: Control = MENU.new()
	add_child_autofree(menu)
	menu.refresh()
	var button: Button = menu.find_child("Setting_loadout_main", true, false)
	assert_eq(button.text, "Main weapon: Cheddar Blaster")
	button.pressed.emit()
	assert_eq(Settings.get_value("loadout_main"), "shotgun")
	for press in Weapons.MAINS.size() - 1:
		button.pressed.emit()
	assert_eq(Settings.get_value("loadout_main"), Weapons.DEFAULT_MAIN, "and round again")
