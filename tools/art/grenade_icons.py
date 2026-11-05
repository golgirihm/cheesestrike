"""Renders the loadout page's pictures of the two grenades (Content/UI/Loadout/grenade_cheese.png and
grenade_smoke.png, 256 x 256 on a clear background) from the grenade's own 3D model, lit, as the
heroes' portraits are pictures of the heroes themselves (wave 16; they were flat drawings of the HUD's
icons before): the frag of tools/art/weapon_models.py (an egg of cheese grooved into a pineapple's
blocks, a steel fuse, spoon and ring), seen a little from above and from its spoon's side, in the
game's looks for each kind (tools/art/weapon_materials.py's MI_GrenadeCheese and MI_GrenadeMetal: a
bright yellow Emmental holed all over; MI_GrenadeSmoked: the same frag in a dark smoked rind, its holes
few and small, matte), lit as the countdown's glyphs and the spinner are (a warm key from the upper
left, a cool fill, a rim from behind). Run by Blender, headless:

    blender.exe -b --factory-startup -P tools/art/grenade_icons.py -- [out folder]

It writes grenade_icons.txt into the folder too, saying what it did. The HUD still draws its own small
flat grenade beside the ammunition (UI/CheeseStrikeGrenadeHUD.cpp).
"""

import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import weapon_models  # noqa: E402  (the frag's own mesh, made in plain Python)

SIZE = 256
# The kinds: the cheese's colours and holes (how much of the surface, how big), its roughness.
KINDS = {
    "cheese": {"colour": (1.0, 0.8, 0.12, 1.0), "colour_b": (0.95, 0.62, 0.05, 1.0), "hole": (0.42, 0.22, 0.02, 1.0), "holes": 0.3, "roughness": 0.4},
    "smoke": {"colour": (0.3, 0.16, 0.07, 1.0), "colour_b": (0.17, 0.09, 0.04, 1.0), "hole": (0.08, 0.045, 0.02, 1.0), "holes": 0.16, "roughness": 0.78},
}
STEEL = (0.42, 0.42, 0.4, 1.0)


def clear():
    for collection in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.lights, bpy.data.cameras):
        for block in list(collection):
            collection.remove(block)


def frag_object(materials):
    """The frag as weapon_models.py makes it (centimetres, Z up, the spoon on +X), `materials` (by name) in its slots."""
    made = weapon_models.Mesh()
    nu, nv = 64, 48
    weapon_models.grid(made, "Cheese", lambda i, j: weapon_models.frag_body_at(i, j, nu, nv), nu, nv, weapon_models.radial, uv_scale=2.0)
    weapon_models.cylinder(made, "Metal", *weapon_models.FRAG_COLLAR[:3], 24)
    weapon_models.cylinder(made, "Metal", *weapon_models.FRAG_FUSE[:3], 20)
    weapon_models.cylinder(made, "Metal", *weapon_models.FRAG_CAP[:3], 20)
    weapon_models.spoon(made)
    weapon_models.ring(made)
    mesh = bpy.data.meshes.new("Frag")
    faces = []
    slots = []
    order = list(made.faces.keys())
    for material in order:
        for a, b, c in made.faces[material]:
            faces.append((a - 1, b - 1, c - 1))
            slots.append(order.index(material))
    mesh.from_pydata([tuple(p) for p in made.positions], [], faces)
    mesh.update()
    for material in order:
        mesh.materials.append(materials[material])
    mesh.polygons.foreach_set("material_index", slots)
    mesh.update()
    mesh.shade_smooth()
    body = bpy.data.objects.new("Frag", mesh)
    bpy.context.scene.collection.objects.link(body)
    return body, order


def cheese_material(name, kind):
    """The cheese of `kind`: mottled between its two colours, holed (round pits, darker, cut in by the bump), fine pores."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    shader = nodes["Principled BSDF"]
    shader.inputs["Roughness"].default_value = kind["roughness"]
    shader.inputs["Subsurface Weight"].default_value = 0.15
    shader.inputs["Subsurface Radius"].default_value = (0.3, 0.2, 0.08)
    shader.inputs["Subsurface Scale"].default_value = 0.3
    coords = nodes.new("ShaderNodeTexCoord")
    mottle = nodes.new("ShaderNodeTexNoise")
    mottle.inputs["Scale"].default_value = 0.35
    mottle.inputs["Detail"].default_value = 3.0
    links.new(coords.outputs["Object"], mottle.inputs["Vector"])
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = kind["colour"]
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = kind["colour_b"]
    links.new(mottle.outputs["Fac"], ramp.inputs["Fac"])
    # The holes: the nearest of points scattered through the cheese, a pit where it is near.
    cells = nodes.new("ShaderNodeTexVoronoi")
    cells.inputs["Scale"].default_value = 0.55
    links.new(coords.outputs["Object"], cells.inputs["Vector"])
    pit = nodes.new("ShaderNodeMapRange")
    pit.inputs["From Min"].default_value = kind["holes"]
    pit.inputs["From Max"].default_value = kind["holes"] * 0.7
    pit.inputs["To Min"].default_value = 0.0
    pit.inputs["To Max"].default_value = 1.0
    links.new(cells.outputs["Distance"], pit.inputs["Value"])
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    colour_a = next(i for i in mix.inputs if i.identifier == "A_Color")
    colour_b = next(i for i in mix.inputs if i.identifier == "B_Color")
    factor = next(i for i in mix.inputs if i.identifier == "Factor_Float")
    result = next(o for o in mix.outputs if o.identifier == "Result_Color")
    links.new(ramp.outputs["Color"], colour_a)
    colour_b.default_value = kind["hole"]
    links.new(pit.outputs["Result"], factor)
    links.new(result, shader.inputs["Base Color"])
    # The pits sink in, with fine pores over all of it.
    invert = nodes.new("ShaderNodeMath")
    invert.operation = "SUBTRACT"
    invert.inputs[0].default_value = 1.0
    links.new(pit.outputs["Result"], invert.inputs[1])
    pores = nodes.new("ShaderNodeTexNoise")
    pores.inputs["Scale"].default_value = 4.0
    pores.inputs["Detail"].default_value = 2.0
    links.new(coords.outputs["Object"], pores.inputs["Vector"])
    height = nodes.new("ShaderNodeMath")
    height.operation = "MULTIPLY_ADD"
    links.new(pores.outputs["Fac"], height.inputs[0])
    height.inputs[1].default_value = 0.05
    links.new(invert.outputs["Value"], height.inputs[2])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.8
    bump.inputs["Distance"].default_value = 0.25
    links.new(height.outputs["Value"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    return mat


def steel_material():
    mat = bpy.data.materials.new("Metal")
    mat.use_nodes = True
    shader = mat.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = STEEL
    shader.inputs["Metallic"].default_value = 1.0
    shader.inputs["Roughness"].default_value = 0.42
    return mat


def stage(body):
    """A camera a little above and to the spoon's side, fitting the frag with a little room; the lights."""
    scene = bpy.context.scene
    corners = [body.matrix_world @ Vector(c) for c in body.bound_box]
    low = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    high = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    middle = (low + high) / 2.0
    reach = max(max(high.x - low.x, high.y - low.y, high.z - low.z) / 2.0, 1.0)
    camera_data = bpy.data.cameras.new("Camera")
    camera_data.lens = 85.0
    camera = bpy.data.objects.new("Camera", camera_data)
    scene.collection.objects.link(camera)
    # From in front (-Y), a little to the spoon's side (+X) and above, at a distance that fits the frag in the frame.
    way = Vector((0.45, -1.0, 0.42)).normalized()
    sensor_half = math.atan(camera_data.sensor_width / 2.0 / camera_data.lens)
    distance = reach / math.tan(sensor_half) * 0.98
    camera.location = middle + way * distance
    camera.rotation_euler = (middle - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera
    scene.render.resolution_x = SIZE
    scene.render.resolution_y = SIZE

    def light(name, energy, rotation, size):
        data = bpy.data.lights.new(name, "SUN")
        data.energy = energy
        data.angle = math.radians(size)
        lamp = bpy.data.objects.new(name, data)
        lamp.rotation_euler = [math.radians(a) for a in rotation]
        scene.collection.objects.link(lamp)

    light("Key", 6.5, (-40.0, -25.0, -20.0), 8.0)
    light("Fill", 1.4, (-10.0, 45.0, 30.0), 25.0)
    light("Rim", 3.0, (-125.0, 0.0, 10.0), 5.0)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else os.path.join(HERE, "..", "..", "Content", "UI", "Loadout"))
    os.makedirs(out, exist_ok=True)
    report = []
    for name, kind in KINDS.items():
        clear()
        scene = bpy.context.scene
        scene.render.engine = "CYCLES"
        scene.cycles.samples = 160
        scene.cycles.use_denoising = True
        scene.cycles.device = "CPU"
        scene.render.film_transparent = True
        scene.render.image_settings.file_format = "PNG"
        scene.render.image_settings.color_mode = "RGBA"
        scene.view_settings.view_transform = "Standard"
        scene.view_settings.look = "None"
        world = bpy.data.worlds.new("World") if scene.world is None else scene.world
        scene.world = world
        world.use_nodes = True
        world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.26, 0.24, 0.22, 1.0)
        world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.9
        body, order = frag_object({"Cheese": cheese_material("Cheese", kind), "Metal": steel_material()})
        # Stood a little tilted, its fuse up and toward the right, so that the spoon and the ring both show.
        body.rotation_euler = (math.radians(-8.0), math.radians(10.0), math.radians(-28.0))
        bpy.context.view_layer.update()
        stage(body)
        target = os.path.join(out, f"grenade_{name}.png")
        scene.render.filepath = target
        bpy.ops.render.render(write_still=True)
        report.append(f"{name}: {target}, {SIZE} x {SIZE}, {len(body.data.polygons)} faces")
    with open(os.path.join(out, "grenade_icons.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


main()
