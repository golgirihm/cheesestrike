"""Renders the game's spinner: a 3D wedge of cheese with rounded corners
turning about its vertical axis, from left to right, as a loop of 48 frames
(one turn), 64 x 64 pixels each on a clear background, lit as the countdown's
glyphs are (tools/art/countdown_glyphs.py: a warm key from the upper left, a
cool fill, a rim from behind). Cheese gold, with the wheel's rind along the
wedge's outer edge, a few holes in its cut faces. Run by Blender, headless:

    blender.exe -b --factory-startup -P tools/art/spinner_frames.py -- <out folder>

It writes spinner_00.png to spinner_47.png into the folder (Content/UI/Spinner
by default, next to this repository's root), where the game reads them as
files (UI/CheeseStrikeLoader.cpp: SCheeseStrikeLoader shows them at 24 a
second, the menu's spinners, the loading screen and the post-game wait
alike), and spinner_frames.txt saying what it did.
"""

import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

FRAMES = 48
SIZE = 64
# The wedge: a sector of a wheel this wide (degrees), of this radius and height (Blender units), its corners rounded this much.
ANGLE = 58.0
RADIUS = 1.0
HEIGHT = 0.5
ROUNDING = 0.075
# The cheese and the rind: a warm yellow, a little translucent like wax; the rind a deeper orange-brown.
CHEESE = (1.0, 0.7, 0.12, 1.0)
CHEESE_DEEP = (0.85, 0.42, 0.03, 1.0)
RIND = (0.62, 0.3, 0.05, 1.0)


def clear():
    for collection in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.lights, bpy.data.cameras):
        for block in list(collection):
            collection.remove(block)


def material(name, colour, deep, roughness):
    """A waxy cheese: mottled between `colour` and a deeper orange, darker in its holes, fine pores in the bump."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    shader = nodes["Principled BSDF"]
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Subsurface Weight"].default_value = 0.2
    shader.inputs["Subsurface Radius"].default_value = (0.08, 0.05, 0.02)
    shader.inputs["Subsurface Scale"].default_value = 0.05
    texture = nodes.new("ShaderNodeTexCoord")
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 5.0
    noise.inputs["Detail"].default_value = 3.0
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = colour
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = deep
    links.new(texture.outputs["Object"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    occlusion = nodes.new("ShaderNodeAmbientOcclusion")
    occlusion.inputs["Distance"].default_value = 0.1
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
    pores.inputs["Scale"].default_value = 40.0
    pores.inputs["Detail"].default_value = 2.0
    links.new(texture.outputs["Object"], pores.inputs["Vector"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.1
    bump.inputs["Distance"].default_value = 0.004
    links.new(pores.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    return mat


def wedge():
    """The wedge as a mesh: a sector of a cylinder, its point at the origin and its arc toward +Y, standing on Z = 0, corners rounded."""
    bm = bmesh.new()
    half = math.radians(ANGLE / 2.0)
    steps = 16
    bottom = [bm.verts.new((0.0, 0.0, 0.0))]
    top = [bm.verts.new((0.0, 0.0, HEIGHT))]
    for step in range(steps + 1):
        angle = -half + 2.0 * half * step / steps
        x = RADIUS * math.sin(angle)
        y = RADIUS * math.cos(angle)
        bottom.append(bm.verts.new((x, y, 0.0)))
        top.append(bm.verts.new((x, y, HEIGHT)))
    # The top and the bottom as fans from the point; the arc and the two cut sides as quads.
    for step in range(steps):
        bm.faces.new((bottom[0], bottom[step + 2], bottom[step + 1]))
        bm.faces.new((top[0], top[step + 1], top[step + 2]))
        bm.faces.new((bottom[step + 1], bottom[step + 2], top[step + 2], top[step + 1]))
    bm.faces.new((bottom[0], bottom[1], top[1], top[0]))
    bm.faces.new((bottom[0], top[0], top[steps + 1], bottom[steps + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new("Wedge")
    bm.to_mesh(mesh)
    bm.free()
    body = bpy.data.objects.new("Wedge", mesh)
    bpy.context.scene.collection.objects.link(body)
    bevel = body.modifiers.new("Rounding", "BEVEL")
    bevel.width = ROUNDING
    bevel.segments = 5
    bevel.limit_method = "ANGLE"
    bevel.angle_limit = math.radians(30.0)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    rounded = bpy.data.meshes.new_from_object(body.evaluated_get(depsgraph))
    body.modifiers.remove(bevel)
    old = body.data
    body.data = rounded
    bpy.data.meshes.remove(old)
    return body


def holes(body, rng):
    """A few spheres cut out of the top and the cut sides, as a cheese's holes; not the rind. Returns how many."""
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.normal_update()
    spheres = bmesh.new()
    placed = []

    def add(centre, radius):
        for other, r in placed:
            if (other - centre).length < (r + radius) * 1.2:
                return False
        placed.append((centre, radius))
        bmesh.ops.create_uvsphere(spheres, u_segments=16, v_segments=10, radius=radius, matrix=Matrix.Translation(centre))
        return True

    half = math.radians(ANGLE / 2.0)
    # On the top: a few, well inside the wedge.
    tries = 0
    while len(placed) < 4 and tries < 200:
        tries += 1
        r = rng.uniform(0.28, 0.8) * RADIUS
        a = rng.uniform(-half * 0.6, half * 0.6)
        radius = rng.uniform(0.06, 0.11)
        add(Vector((r * math.sin(a), r * math.cos(a), HEIGHT + radius * rng.uniform(-0.4, 0.3))), radius)
    # On each cut side: a couple.
    for side in (-1.0, 1.0):
        made = 0
        tries = 0
        while made < 2 and tries < 200:
            tries += 1
            r = rng.uniform(0.3, 0.8) * RADIUS
            z = rng.uniform(0.15, 0.35)
            radius = rng.uniform(0.05, 0.09)
            # The side's outward normal.
            normal = Vector((side * math.cos(half), -math.sin(half), 0.0))
            on_side = Vector((side * r * math.sin(half), r * math.cos(half), z))
            made += add(on_side + normal * radius * rng.uniform(-0.3, 0.2), radius)
    bm.free()
    if not placed:
        return 0
    cutter_mesh = bpy.data.meshes.new("Holes")
    spheres.to_mesh(cutter_mesh)
    spheres.free()
    cutter = bpy.data.objects.new("Holes", cutter_mesh)
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
    return len(placed)


def dress(body):
    """The cheese on every face but the arc, which is the rind."""
    body.data.materials.clear()
    body.data.materials.append(material("Cheese", CHEESE, (0.97, 0.6, 0.08, 1.0), 0.42))
    body.data.materials.append(material("Rind", RIND, (0.45, 0.2, 0.03, 1.0), 0.6))
    for polygon in body.data.polygons:
        normal = polygon.normal
        flat = Vector((normal.x, normal.y, 0.0))
        centre = polygon.center
        # The rind: faces on the arc, facing outward along the radius, near the full radius.
        radial = Vector((centre.x, centre.y, 0.0))
        on_arc = radial.length > RADIUS * 0.8 and flat.length > 0.6 and radial.length > 0.001 and flat.normalized().dot(radial.normalized()) > 0.85
        polygon.material_index = 1 if on_arc else 0
    body.data.shade_smooth()
    body.data.set_sharp_from_angle(angle=math.radians(40.0))


def stage(body):
    """Lit as the countdown's glyphs, seen from a little above by an orthographic camera that fits the turning wedge."""
    scene = bpy.context.scene
    camera_data = bpy.data.cameras.new("Camera")
    camera_data.type = "ORTHO"
    # Wide enough for the wedge whichever way it faces (turning about its middle, its longest reach is about 0.6 of the
    # radius, to its point or its corners), with a little clear round it, so that it fills the frame.
    camera_data.ortho_scale = RADIUS * 1.4
    camera = bpy.data.objects.new("Camera", camera_data)
    camera.location = (0.0, -10.0, 10.0 * math.tan(math.radians(22.0)) + HEIGHT / 2.0)
    camera.rotation_euler = (math.radians(90.0 - 22.0), 0.0, 0.0)
    scene.collection.objects.link(camera)
    scene.camera = camera
    scene.render.resolution_x = SIZE
    scene.render.resolution_y = SIZE

    def light(name, kind, energy, rotation, size=0.0):
        data = bpy.data.lights.new(name, kind)
        data.energy = energy
        if kind == "SUN":
            data.angle = math.radians(size)
        lamp = bpy.data.objects.new(name, data)
        lamp.rotation_euler = [math.radians(a) for a in rotation]
        scene.collection.objects.link(lamp)

    # A warm key from the upper left, a cool fill from the right, and a rim from behind above (countdown_glyphs.py's).
    light("Key", "SUN", 4.2, (-38.0, -30.0, 0.0), 6.0)
    light("Fill", "SUN", 1.1, (-10.0, 40.0, 0.0), 20.0)
    light("Rim", "SUN", 2.0, (-120.0, 0.0, 0.0), 4.0)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.abspath(argv[0] if argv else os.path.join(here, "..", "..", "Content", "UI", "Spinner"))
    os.makedirs(out, exist_ok=True)
    clear()
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 128
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

    body = wedge()
    count = holes(body, random.Random(15))
    dress(body)
    # Turning about its own middle, not its point: the wedge's middle is about 0.6 of the radius along its bisector.
    for v in body.data.vertices:
        v.co.y -= RADIUS * 0.58
    stage(body)
    report = [f"{count} holes, {SIZE} x {SIZE}, {FRAMES} frames"]
    for frame in range(FRAMES):
        # +Z turns the near face from left to right as seen from the camera.
        body.rotation_euler = (0.0, 0.0, 2.0 * math.pi * frame / FRAMES)
        scene.render.filepath = os.path.join(out, f"spinner_{frame:02d}.png")
        bpy.ops.render.render(write_still=True)
    with open(os.path.join(out, "spinner_frames.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


main()
