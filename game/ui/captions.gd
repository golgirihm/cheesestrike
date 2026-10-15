extends VBoxContainer
## Captions: important sounds written at the bottom of the screen, with where
## they came from, such as "Footsteps, behind", for players who can't hear
## them. Off unless the "captions" setting is on.
##
## Only sounds made somewhere else in the arena are written; what this player
## does themselves, and what the rest of the screen already shows, is not.

## How long a caption stays after its sound was last heard.
const LIFE := 2.5
## Most captions at once; the oldest goes first.
const MOST := 4
## Sounds closer than this to the player are their own.
const OWN_DISTANCE := 1.5
## What each sound is called; sounds not here get no caption.
const TEXTS := {
	"pistol": "Pistol fire",
	"gnaw": "Mice gnawing",
	"crumble": "Cheese crumbling",
	"blaster": "Blaster fire",
	"shotgun": "Shotgun blast",
	"grater": "Grater fire",
	"launcher": "Launcher shot",
	"pistols": "Pistol fire",
	"carbine": "Carbine burst",
	"rifle": "Rifle shot",
	"lmg": "Machine gun fire",
	"bow": "Bow loosed",
	"burst": "Cheese burst",
	"knife": "Knife swing",
	"throw": "Grenade thrown",
	"slide": "Slide",
	"pad": "Jump pad",
	"emote": "Squeak",
	"step": "Footsteps",
	"step_rock": "Footsteps",
	"step_grass": "Footsteps",
	"step_wood": "Footsteps",
	"step_metal": "Footsteps",
	"step_cheese": "Footsteps",
	"voice_pain": "Cry of pain",
	"voice_out": "Squeal",
	"voice_taunt": "Taunt",
	"voice_reload": "Teammate reloading",
	"pickup": "Pickup taken",
	"overload": "Gouda Overload",
}

var world: Node3D
# Each entry: {"text": String, "left": seconds}, oldest first.
var _entries: Array[Dictionary] = []


## The ring round the crosshair that shows where sounds come from, if any.
var ring: SoundRing


static func listener_far_enough(listener: Vector3, at: Vector3) -> bool:
	return listener.distance_to(at) >= OWN_DISTANCE


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	alignment = BoxContainer.ALIGNMENT_END
	Sounds.played.connect(_on_played)


## The caption for the sound `name` made at `at`, heard by a player at
## `listener` facing `yaw`, or "" for none: what, by `who` if they are
## known, where, and how near or far if it is either.
static func caption(name: String, at: Variant, listener: Vector3, yaw: float, who := "") -> String:
	if not TEXTS.has(name) or not at is Vector3:
		return ""
	if listener.distance_to(at) < OWN_DISTANCE:
		return ""
	var said: String = "%s (%s)" % [TEXTS[name], who] if not who.is_empty() else TEXTS[name]
	said += ", " + direction_word(CombatHud.damage_direction(listener, yaw, at))
	var far := distance_word(listener.distance_to(at))
	return said + (", " + far if not far.is_empty() else "")


## How near a sound `distance` metres away is said to be: "near", "far", or
## nothing in between.
static func distance_word(distance: float) -> String:
	if distance <= NEAR:
		return "near"
	return "far" if distance >= FAR else ""


const NEAR := 6.0
const FAR := 30.0
## A sound this close to a player, in metres, is taken to be theirs.
const MADE_BY := 2.0


## The name of whoever, of the `players` ({name: place}), stands within
## MADE_BY of `at`, the nearest; or "".
static func maker(at: Vector3, players: Dictionary) -> String:
	var found := ""
	var nearest := MADE_BY
	for who: String in players:
		var distance := at.distance_to(players[who])
		if distance <= nearest:
			nearest = distance
			found = who
	return found


## A word for a direction from straight ahead, as damage_direction gives it.
static func direction_word(angle: float) -> String:
	var turned := wrapf(angle, -PI, PI)
	if absf(turned) <= PI / 4.0:
		return "ahead"
	if absf(turned) >= PI * 3.0 / 4.0:
		return "behind"
	return "right" if turned > 0.0 else "left"


## Adds `text`, or keeps it longer if it is already showing.
func add(text: String) -> void:
	for entry in _entries:
		if entry.text == text:
			entry.left = LIFE
			_entries.erase(entry)
			_entries.append(entry)
			_show()
			return
	_entries.append({"text": text, "left": LIFE})
	while _entries.size() > MOST:
		_entries.pop_front()
	_show()


func clear() -> void:
	_entries.clear()
	_show()


## The captions showing, oldest first.
func showing() -> Array[String]:
	var texts: Array[String] = []
	for entry in _entries:
		texts.append(entry.text)
	return texts


func _process(delta: float) -> void:
	if _entries.is_empty():
		return
	var before := _entries.size()
	for entry in _entries:
		entry.left -= delta
	_entries = _entries.filter(func(entry: Dictionary) -> bool: return entry.left > 0.0)
	if _entries.size() != before:
		_show()


func _on_played(name: String, at: Variant) -> void:
	if not Settings.get_value("captions") or world == null or not is_visible_in_tree():
		return
	if multiplayer.multiplayer_peer == null:
		return
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own == null:
		return
	if at is Vector3 and ring != null and listener_far_enough(own.global_position, at):
		ring.heard(name, at)
	var others := {}
	if world.match_.has_state():
		for other: Node3D in world.players.get_children():
			if other != own and String(other.name).is_valid_int():
				others[world.match_.state.name_of(int(String(other.name)))] = other.global_position
	var text := caption(name, at, own.global_position, own.rotation.y, maker(at, others) if at is Vector3 else "")
	if not text.is_empty():
		add(text)


func _show() -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	for entry in _entries:
		var label := Label.new()
		label.text = "[%s]" % entry.text
		label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		label.add_theme_font_size_override("font_size", CombatHud.text_size(15))
		label.add_theme_color_override("font_outline_color", Color.BLACK)
		label.add_theme_constant_override("outline_size", 5)
		label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(label)
