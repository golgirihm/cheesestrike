"""Renders the start countdown's words (3, 2, 1, FIGHT) as 3D cheese for the
HUD: each a block of extruded lettering in the engine's Roboto Black,
bevelled, its face and sides pocked with holes, in a waxy cheese yellow, lit
from the upper left and turned a little so that its depth shows, on a clear
background. Run by Blender, headless:

    blender.exe -b --factory-startup -P tools/art/countdown_glyphs.py -- <out folder>

It writes T_Podium_1.png to T_Podium_3.png (the podium's places, straight on) and T_Count_1.png to T_Count_9.png and T_Count_Fight.png
(512 pixels high, as wide as a power of two) into the folder; tools/art/import_countdown.py imports them
into /Game/CheeseStrike/UI/Countdown/, where the HUD finds them
(UI/CheeseStrikeHUDCountdown.cpp). Roboto is Apache 2.0, from the engine's
Slate fonts.
"""

import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Vector, Matrix

FONT = r"C:\Program Files\Epic Games\UE_5.8\Engine\Content\Slate\Fonts\Roboto-Black.ttf"
# The usual count is 3 (2 before a later round); ?Countdown= may ask for up to 9.
WORDS = {**{str(n): f"T_Count_{n}" for n in range(1, 10)}, "FIGHT": "T_Count_Fight"}
# The podium's places (wave 9): the same cheese, but straight on, not turned, and all three framed alike (the same scale, the same baseline
# in the picture, each centred across), so that the HUD can stand them on one line, upright. (The turned countdown ones sat in their
# pictures by their own turned outlines, so no two stood alike.)
PODIUM = {str(n): f"T_Podium_{n}" for n in range(1, 4)}
HEIGHT = 512
EXTRUDE = 0.09
BEVEL = 0.022
# The cheese: a warm yellow, a little translucent like wax, and darker deep in its holes.
CHEESE = (1.0, 0.7, 0.12, 1.0)
CHEESE_DEEP = (0.85, 0.42, 0.03, 1.0)


def clear():
    for collection in (bpy.data.objects, bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.lights, bpy.data.cameras):
        for block in list(collection):
            collection.remove(block)


def cheese_material():
    material = bpy.data.materials.new("Cheese")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    shader = nodes["Principled BSDF"]
    shader.inputs["Roughness"].default_value = 0.42
    shader.inputs["Subsurface Weight"].default_value = 0.25
    shader.inputs["Subsurface Radius"].default_value = (0.08, 0.05, 0.02)
    shader.inputs["Subsurface Scale"].default_value = 0.05
    # Mottled: a soft noise between the yellow and a deeper orange, and fine pores in the bump.
    texture = nodes.new("ShaderNodeTexCoord")
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 6.0
    noise.inputs["Detail"].default_value = 3.0
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = CHEESE
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = (0.97, 0.6, 0.08, 1.0)
    links.new(texture.outputs["Object"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    # Deeper in the holes (the ambient occlusion), the colour darkens towards orange.
    occlusion = nodes.new("ShaderNodeAmbientOcclusion")
    occlusion.inputs["Distance"].default_value = 0.08
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    colour_a = next(i for i in mix.inputs if i.identifier == "A_Color")
    colour_b = next(i for i in mix.inputs if i.identifier == "B_Color")
    factor = next(i for i in mix.inputs if i.identifier == "Factor_Float")
    result = next(o for o in mix.outputs if o.identifier == "Result_Color")
    colour_b.default_value = CHEESE_DEEP
    links.new(ramp.outputs["Color"], colour_a)
    invert = nodes.new("ShaderNodeMath")
    invert.operation = "SUBTRACT"
    invert.inputs[0].default_value = 1.0
    links.new(occlusion.outputs["AO"], invert.inputs[1])
    links.new(invert.outputs["Value"], factor)
    links.new(result, shader.inputs["Base Color"])
    pores = nodes.new("ShaderNodeTexNoise")
    pores.inputs["Scale"].default_value = 60.0
    pores.inputs["Detail"].default_value = 2.0
    links.new(texture.outputs["Object"], pores.inputs["Vector"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.12
    bump.inputs["Distance"].default_value = 0.004
    links.new(pores.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    return material


def lettering(word):
    """The word as a mesh: extruded and bevelled lettering, centred on the origin, its face towards +Z."""
    curve = bpy.data.curves.new(word, type="FONT")
    curve.body = word
    curve.font = bpy.data.fonts.load(FONT)
    curve.size = 1.0
    curve.space_character = 1.06
    curve.align_x = "CENTER"
    curve.align_y = "CENTER"
    curve.extrude = EXTRUDE
    curve.bevel_depth = BEVEL
    curve.bevel_resolution = 4
    curve.resolution_u = 10
    holder = bpy.data.objects.new(word + "_curve", curve)
    bpy.context.scene.collection.objects.link(holder)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    mesh = bpy.data.meshes.new_from_object(holder.evaluated_get(depsgraph))
    bpy.data.objects.remove(holder)
    body = bpy.data.objects.new(word, mesh)
    bpy.context.scene.collection.objects.link(body)
    return body


def sample(faces, count, rng):
    """`count` random points on `faces` (bmesh faces), by area, with each face's normal."""
    areas = [f.calc_area() for f in faces]
    total = sum(areas)
    points = []
    if total <= 0.0:
        return points
    for _ in range(count):
        pick = rng.random() * total
        for face, area in zip(faces, areas):
            pick -= area
            if pick <= 0.0:
                break
        loops = [l.vert.co for l in face.loops]
        # A random point in a random fan triangle of the face, by area.
        tris = [(loops[0], loops[i], loops[i + 1]) for i in range(1, len(loops) - 1)]
        weights = [((b - a).cross(c - a)).length for a, b, c in tris]
        t = rng.choices(tris, weights=weights)[0] if sum(weights) > 0 else tris[0]
        u, v = rng.random(), rng.random()
        if u + v > 1.0:
            u, v = 1.0 - u, 1.0 - v
        points.append((t[0] + (t[1] - t[0]) * u + (t[2] - t[0]) * v, face.normal.copy()))
    return points


def holes(body, rng):
    """Spheres cut out of the face and the sides, as a cheese's holes: craters of every depth, a few right through."""
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.normal_update()
    front = [f for f in bm.faces if f.normal.z > 0.9]
    sides = [f for f in bm.faces if abs(f.normal.z) < 0.3]
    front_area = sum(f.calc_area() for f in front)
    spheres = bmesh.new()
    placed = []

    def add(centre, radius):
        # Apart from the others, so that the holes stay round.
        for other, r in placed:
            if (other - centre).length < (r + radius) * 1.1:
                return False
        placed.append((centre, radius))
        bmesh.ops.create_uvsphere(spheres, u_segments=20, v_segments=12, radius=radius, matrix=Matrix.Translation(centre))
        return True

    # As many as the face's size calls for (a few at least), trying more places than that until they fit.
    wanted = max(7, round(front_area * 70))
    made = 0
    for point, normal in sample(front, wanted * 5, rng):
        radius = rng.uniform(0.03, 0.075)
        depth = rng.uniform(-0.2, 0.55)
        made += add(point + Vector((0.0, 0.0, radius * depth)), radius)
        if made >= wanted:
            break
    wanted = max(3, round(front_area * 14))
    made = 0
    for point, normal in sample(sides, wanted * 5, rng):
        radius = rng.uniform(0.025, 0.05)
        made += add(point + normal * radius * rng.uniform(-0.2, 0.3), radius)
        if made >= wanted:
            break
    bm.free()
    cutter_mesh = bpy.data.meshes.new(body.name + "_holes")
    spheres.to_mesh(cutter_mesh)
    spheres.free()
    cutter = bpy.data.objects.new(body.name + "_holes", cutter_mesh)
    bpy.context.scene.collection.objects.link(cutter)
    cut = body.modifiers.new("Holes", "BOOLEAN")
    cut.operation = "DIFFERENCE"
    cut.solver = "EXACT"
    cut.object = cutter
    cut.use_self = True
    cut.use_hole_tolerant = True
    depsgraph = bpy.context.evaluated_depsgraph_get()
    result = bpy.data.meshes.new_from_object(body.evaluated_get(depsgraph))
    body.modifiers.remove(cut)
    old = body.data
    body.data = result
    bpy.data.meshes.remove(old)
    bpy.data.objects.remove(cutter)
    body.data.shade_smooth()
    body.data.set_sharp_from_angle(angle=math.radians(40.0))
    return len(placed)


def straight_rows():
    """The lowest and highest the podium's digits reach together (they share a baseline, and a framing)."""
    low, high = 1e9, -1e9
    for word in PODIUM:
        body = lettering(word)
        for v in body.data.vertices:
            low = min(low, v.co.y)
            high = max(high, v.co.y)
        mesh = body.data
        bpy.data.objects.remove(body)
        bpy.data.meshes.remove(mesh)
    return low, high


def stage(body, rows=None):
    """Turned a little to show its depth (unless `rows`, the straight digits' shared vertical extent, is given: then straight on), lit, and framed by an orthographic camera that fits it."""
    body.rotation_euler = (0.0, 0.0, 0.0) if rows else (math.radians(14.0), math.radians(-20.0), 0.0)
    bpy.context.view_layer.update()
    corners = [body.matrix_world @ v.co for v in body.data.vertices]
    low = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    high = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    if rows:
        low.y, high.y = rows
    middle = (low + high) / 2.0
    width = (high.x - low.x) * 1.08
    height = (high.y - low.y) * 1.08

    camera_data = bpy.data.cameras.new("Camera")
    camera_data.type = "ORTHO"
    camera = bpy.data.objects.new("Camera", camera_data)
    camera.location = (middle.x, middle.y, 10.0)
    bpy.context.scene.collection.objects.link(camera)
    scene = bpy.context.scene
    scene.camera = camera
    scene.render.resolution_y = HEIGHT
    # As wide as the power of two it fits in (so that the engine makes its mipmaps), clear either side; the scale is the wider side's.
    resolution_x = 1 << max(0, math.ceil(math.log2(max(1.0, HEIGHT * width / height))))
    scene.render.resolution_x = resolution_x
    camera_data.ortho_scale = height * max(resolution_x, HEIGHT) / HEIGHT

    def light(name, kind, energy, rotation, size=0.0):
        data = bpy.data.lights.new(name, kind)
        data.energy = energy
        if kind == "SUN":
            data.angle = math.radians(size)
        lamp = bpy.data.objects.new(name, data)
        lamp.rotation_euler = [math.radians(a) for a in rotation]
        scene.collection.objects.link(lamp)

    # A warm key from the upper left, a cool fill from the right, and a rim from behind above.
    light("Key", "SUN", 4.2, (-38.0, -30.0, 0.0), 6.0)
    light("Fill", "SUN", 1.1, (-10.0, 40.0, 0.0), 20.0)
    light("Rim", "SUN", 2.0, (-120.0, 0.0, 0.0), 4.0)


def render(word, name, out, rng, rows=None):
    clear()
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 96
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
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.25, 0.22, 0.2, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6

    body = lettering(word)
    count = holes(body, rng)
    # One material for the face, the sides and the holes alike (the cut leaves a slot of its own).
    body.data.materials.clear()
    body.data.materials.append(cheese_material())
    for polygon in body.data.polygons:
        polygon.material_index = 0
    stage(body, rows)
    scene.render.filepath = os.path.join(out, name + ".png")
    bpy.ops.render.render(write_still=True)
    return count


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else os.path.join(os.getcwd(), "countdown"))
    os.makedirs(out, exist_ok=True)
    report = []
    # CHEESESTRIKE_GLYPH_SETS: which to render, "count" (the countdown's) and "podium" (the places'); both by default.
    sets = os.environ.get("CHEESESTRIKE_GLYPH_SETS", "count,podium").split(",")
    if "count" in sets:
        for index, (word, name) in enumerate(WORDS.items()):
            count = render(word, name, out, random.Random(1000 + index))
            report.append(f"{name}: {count} holes, {bpy.context.scene.render.resolution_x} x {HEIGHT}")
    if "podium" in sets:
        clear()
        rows = straight_rows()
        for index, (word, name) in enumerate(PODIUM.items()):
            count = render(word, name, out, random.Random(1000 + int(word) - 1), rows)
            report.append(f"{name}: {count} holes, {bpy.context.scene.render.resolution_x} x {HEIGHT}")
    with open(os.path.join(out, "countdown_glyphs.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


main()
