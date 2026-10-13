extends Node3D
## How an arena is lit, beyond its sun: a cool light from the other side of
## the sky that fills the shade, so that shade is never flat grey; a soft
## dark edge on the ground round the foot of each solid thing, as if the
## light had been worked out in advance; and lamps, in the arenas that have
## them, which come on when the sky is dark.
##
## The sun, which casts the real shadows, is the world's, and is set by the
## Atmosphere. The world adds this as a child of the arena.
##
## This is not baked lighting: nothing is worked out ahead of time and
## stored. It is the same few cheap things on every machine.

## The cool of shade, and how strong beside the sun.
const FILL := Color(0.62, 0.74, 1.0)
const FILL_ENERGY := 0.28
const LAMP := Color(1.0, 0.82, 0.55)
const LAMP_ENERGY := 2.6
const LAMP_RANGE := 18.0
const LAMP_HEIGHT := 7.5
## Where each arena's lamps stand, by its id; most have none.
const LAMPS := {
	"city": [Vector3(-12.5, 0, -12.5), Vector3(12.5, 0, -12.5), Vector3(-12.5, 0, 12.5), Vector3(12.5, 0, 12.5), Vector3(0, 0, -30), Vector3(0, 0, 30)],
	"market": [Vector3(-9, 0, -4), Vector3(9, 0, 4), Vector3(-4, 0, 9), Vector3(13, 0, -13), Vector3(-14, 0, 14)],
	"cave": [Vector3(0, 0, -9), Vector3(0, 0, 10), Vector3(-17, 0, -9), Vector3(17, 0, 13), Vector3(-9, 0, 4), Vector3(9, 0, -4)],
}
## The arenas whose lamps are always lit, whatever the sky.
const ALWAYS_LIT := ["cave"]
## The skies under which lamps come on.
const DARK_SKIES := [Atmosphere.DUSK, Atmosphere.NIGHT, Atmosphere.RAIN, Atmosphere.DUST]

var _fill: DirectionalLight3D
var _lamps: Array[Dictionary] = []
var _shades := 0
var _always := false
static var _shade_texture: ImageTexture


## Builds the lighting of arena `id`, whose solid pieces are under
## `geometry`.
func build(id: String, geometry: Node) -> void:
	name = "Lighting"
	_always = ALWAYS_LIT.has(id)
	_fill = DirectionalLight3D.new()
	_fill.name = "Fill"
	_fill.light_color = FILL
	_fill.light_energy = FILL_ENERGY
	_fill.shadow_enabled = false
	add_child(_fill)
	for body: Node in geometry.get_children():
		var shape := body.get_node_or_null("CollisionShape3D") as CollisionShape3D
		if shape == null or not shape.shape is BoxShape3D or String(body.name).begins_with("Wall") or body.name == "Floor":
			continue
		var size: Vector3 = (shape.shape as BoxShape3D).size
		var at: Vector3 = (body as Node3D).position
		# Only what stands on the ground darkens it.
		if at.y - size.y / 2.0 > 0.3 or absf((body as Node3D).rotation.x) > 0.01 or absf((body as Node3D).rotation.z) > 0.01:
			continue
		add_child(_contact_shade(Vector3(at.x, 0.03, at.z), Vector2(size.x, size.z)))
		_shades += 1
	for at: Vector3 in LAMPS.get(id, []):
		_lamps.append(_lamp(at))


## Sets the lighting for the sky `sky` (see Atmosphere), whose sun shines
## along `sun_way`. With `lights` off, as on low graphics, the lamps glow
## but light nothing.
func show_for(sky: String, sun_way: Vector3, lights := true) -> void:
	# From the other side of the sky, and higher, so it reaches into shade.
	var from := Vector3(-sun_way.x, -0.6, -sun_way.z).normalized()
	_fill.look_at_from_position(Vector3.ZERO, from, Vector3.UP if absf(from.y) < 0.99 else Vector3.RIGHT)
	var lit := _always or DARK_SKIES.has(sky)
	for lamp: Dictionary in _lamps:
		(lamp.light as OmniLight3D).visible = lit and lights
		(lamp.glow as StandardMaterial3D).emission_energy_multiplier = 2.2 if lit else 0.0
		(lamp.glow as StandardMaterial3D).albedo_color = LAMP if lit else Color(0.82, 0.82, 0.8)


## What the arena's lighting comes to, for tests: {"fill", "shades", "lamps",
## "lit": how many lamps are lighting anything, "glowing": how many are on}.
func counts() -> Dictionary:
	return {"fill": 1 if _fill != null else 0, "shades": _shades, "lamps": _lamps.size(),
		"lit": _lamps.filter(func(lamp: Dictionary) -> bool: return lamp.light.visible).size(),
		"glowing": _lamps.filter(func(lamp: Dictionary) -> bool: return lamp.glow.emission_energy_multiplier > 0.0).size()}


# A soft dark edge on the ground round something `footprint` across.
func _contact_shade(at: Vector3, footprint: Vector2) -> MeshInstance3D:
	var reach := 1.6
	var plane := PlaneMesh.new()
	plane.size = footprint + Vector2.ONE * reach * 2.0
	var dark := StandardMaterial3D.new()
	dark.albedo_color = Color(0, 0, 0.04, 0.34)
	dark.albedo_texture = _shade()
	dark.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	dark.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	plane.material = dark
	var shade := MeshInstance3D.new()
	shade.mesh = plane
	shade.position = at
	shade.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return shade


# A picture that is dark in the middle and fades to nothing at its edge.
static func _shade() -> ImageTexture:
	if _shade_texture == null:
		var image := Image.create(32, 32, false, Image.FORMAT_LA8)
		for y in 32:
			for x in 32:
				var out := maxf(absf(x - 15.5), absf(y - 15.5)) / 15.5
				image.set_pixel(x, y, Color(1, 1, 1, 1.0 - smoothstep(0.45, 1.0, out)))
		_shade_texture = ImageTexture.create_from_image(image)
	return _shade_texture


# A lamp on a post: {"light", "glow"}.
func _lamp(at: Vector3) -> Dictionary:
	var post := Node3D.new()
	post.position = at
	add_child(post)
	var iron := StandardMaterial3D.new()
	iron.albedo_color = Color(0.16, 0.17, 0.2)
	post.add_child(Rounded.instance(Rounded.cylinder(0.14, LAMP_HEIGHT, 0.05, 8), iron, Vector3(0, LAMP_HEIGHT / 2.0, 0)))
	var glow := StandardMaterial3D.new()
	glow.emission_enabled = true
	glow.emission = LAMP
	var bulb := SphereMesh.new()
	bulb.radius = 0.5
	bulb.height = 1.0
	bulb.radial_segments = 12
	bulb.rings = 6
	var head := Rounded.instance(bulb, glow, Vector3(0, LAMP_HEIGHT + 0.3, 0))
	head.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	post.add_child(head)
	var light := OmniLight3D.new()
	light.light_color = LAMP
	light.light_energy = LAMP_ENERGY
	light.omni_range = LAMP_RANGE
	light.shadow_enabled = false
	light.position = Vector3(0, LAMP_HEIGHT, 0)
	post.add_child(light)
	return {"light": light, "glow": glow}
