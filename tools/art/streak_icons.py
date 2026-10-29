"""Renders the three kill streaks' icons for the HUD's streak panel and its
streak-ready prompt, as small 3D cheese pictures in the countdown glyphs' look
(tools/art/countdown_glyphs.py): 128 pixels square, on a clear background.

  - T_Streak_AerialCheese: the Unmanned Aerial Cheese, a wheel of cheese with
    four arms and a small rotor on each (a quadcopter of cheese), from above
    and to the side;
  - T_Streak_CheeseStrike: the Cheese Strike, a wheel falling, tilted, with
    streaks of its fall over it and a red target ring on the floor under it;
  - T_Streak_GoudaOverload: the Gouda Overload, a wheel bursting and melting,
    chunks flying off and drips running down, before a radiating glow.

Run by Blender, headless:

    blender.exe -b --factory-startup -P tools/art/streak_icons.py -- <out folder>

tools/art/import_streak_icons.py runs it and imports the pictures into
/Game/CheeseStrike/UI/Streaks, where the HUD finds them
(ACheeseStrikeHUD::StreakIcon). The HUD draws them in their own colours when
the streak is made and dimmed when not, so they are rendered lit and bright.
"""

import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Vector, Matrix

# CHEESESTRIKE_ICON_SIZE renders them bigger, for looking at them.
SIZE = int(os.environ.get("CHEESESTRIKE_ICON_SIZE", "128"))
ICONS = ("T_Streak_AerialCheese", "T_Streak_CheeseStrike", "T_Streak_GoudaOverload")
# The cheese: the countdown's warm yellow, a paler cut face, and the gouda's wax rind.
CHEESE = (1.0, 0.7, 0.12, 1.0)
CHEESE_CUT = (1.0, 0.83, 0.36, 1.0)
RIND = (0.95, 0.5, 0.06, 1.0)


def clear():
    for collection in (bpy.data.objects, bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.lights, bpy.data.cameras):
        for block in list(collection):
            collection.remove(block)


def principled(name, colour, roughness=0.45, emission=None, strength=0.0, subsurface=0.2):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = colour
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Subsurface Weight"].default_value = subsurface
    shader.inputs["Subsurface Radius"].default_value = (0.08, 0.05, 0.02)
    shader.inputs["Subsurface Scale"].default_value = 0.05
    if emission is not None:
        shader.inputs["Emission Color"].default_value = emission
        shader.inputs["Emission Strength"].default_value = strength
    return material


def glow_material(name, colour, strength):
    """Light that fades out from the middle of a plane to nothing at its edge: a glow on a clear background."""
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    for node in list(nodes):
        nodes.remove(node)
    output = nodes.new("ShaderNodeOutputMaterial")
    coords = nodes.new("ShaderNodeTexCoord")
    gradient = nodes.new("ShaderNodeTexGradient")
    gradient.gradient_type = "SPHERICAL"
    links.new(coords.outputs["Object"], gradient.inputs["Vector"])
    # Softer towards the edge: the fall-off squared.
    power = nodes.new("ShaderNodeMath")
    power.operation = "POWER"
    power.inputs[1].default_value = 1.6
    links.new(gradient.outputs["Fac"], power.inputs[0])
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = colour
    emission.inputs["Strength"].default_value = strength
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    mix = nodes.new("ShaderNodeMixShader")
    links.new(power.outputs["Value"], mix.inputs["Fac"])
    links.new(transparent.outputs["BSDF"], mix.inputs[1])
    links.new(emission.outputs["Emission"], mix.inputs[2])
    links.new(mix.outputs["Shader"], output.inputs["Surface"])
    return material


def link(obj):
    bpy.context.scene.collection.objects.link(obj)
    return obj


def mesh_object(name, bm, material):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = link(bpy.data.objects.new(name, mesh))
    obj.data.materials.append(material)
    return obj


def wheel(name, radius=1.0, height=0.55, cut=True, rng=None):
    """A wheel of cheese with its rind, a wedge cut out of it to show the paler cheese inside with its holes."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=48, radius1=radius, radius2=radius, depth=height)
    # The rind's edges rounded a little.
    edges = [e for e in bm.edges if not e.is_boundary and abs(e.verts[0].co.z - e.verts[1].co.z) < 1e-4 and abs(abs(e.verts[0].co.z) - height / 2) < 1e-4]
    bmesh.ops.bevel(bm, geom=edges, offset=height * 0.14, segments=3, affect="EDGES")
    rind = principled(name + "_rind", RIND, 0.35)
    body = mesh_object(name, bm, rind)
    # The faces top and bottom in the cheese's own yellow, the rind round the side, so that it reads as a wheel from any side.
    body.data.materials.append(principled(name + "_face", CHEESE, 0.42))
    for polygon in body.data.polygons:
        polygon.material_index = 1 if abs(polygon.normal.z) > 0.9 else 0
    body.data.shade_smooth()
    if not cut:
        return body
    # The wedge: a box rotated about the wheel's axis, taking a quarter out towards the viewer.
    cutter_bm = bmesh.new()
    bmesh.ops.create_cube(cutter_bm, size=1.0)
    for v in cutter_bm.verts:
        v.co = Vector((v.co.x * radius * 1.4 + radius * 0.7, v.co.y * radius * 1.4 + radius * 0.7, v.co.z * height * 1.5))
    cutter = mesh_object(name + "_wedge", cutter_bm, rind)
    cutter.rotation_euler = (0.0, 0.0, math.radians(-80.0))
    # Holes in the cut faces: spheres near the wedge's two planes.
    rng = rng or random.Random(7)
    holes_bm = bmesh.new()
    for _ in range(9):
        r = rng.uniform(0.07, 0.13) * radius
        along = rng.uniform(0.25, 0.85) * radius
        z = rng.uniform(-0.25, 0.25) * height
        side = rng.choice((-80.0, 10.0))
        angle = math.radians(side)
        centre = Vector((math.cos(angle) * along, math.sin(angle) * along, z))
        bmesh.ops.create_uvsphere(holes_bm, u_segments=14, v_segments=8, radius=r, matrix=Matrix.Translation(centre))
    holes = mesh_object(name + "_holes", holes_bm, rind)
    bpy.context.view_layer.update()
    for tool in (cutter, holes):
        cut_mod = body.modifiers.new("Cut", "BOOLEAN")
        cut_mod.operation = "DIFFERENCE"
        cut_mod.solver = "EXACT"
        cut_mod.object = tool
        cut_mod.use_self = True
        cut_mod.use_hole_tolerant = True
        # The faces the cut leaves take the cutter's material: the paler cheese.
        tool.data.materials.clear()
        tool.data.materials.append(principled(name + "_cut", CHEESE_CUT, 0.5, subsurface=0.3))
        cut_mod.material_mode = "TRANSFER"
        depsgraph = bpy.context.evaluated_depsgraph_get()
        result = bpy.data.meshes.new_from_object(body.evaluated_get(depsgraph))
        body.modifiers.remove(cut_mod)
        old = body.data
        body.data = result
        bpy.data.meshes.remove(old)
        bpy.data.objects.remove(tool)
    body.data.shade_smooth()
    body.data.set_sharp_from_angle(angle=math.radians(35.0))
    return body


def box(name, size, at, material, rotation=(0.0, 0.0, 0.0), bevel=0.0):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    if bevel > 0.0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=bevel, segments=2, affect="EDGES")
    obj = mesh_object(name, bm, material)
    obj.location = at
    obj.rotation_euler = rotation
    return obj


def disc(name, radius, depth, at, material, segments=32):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=depth)
    obj = mesh_object(name, bm, material)
    obj.location = at
    return obj


def torus(name, major, minor, at, material):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, major_segments=64, minor_segments=12, location=at)
    obj = bpy.context.active_object
    obj.name = name
    obj.data.materials.append(material)
    obj.data.shade_smooth()
    return obj


def glow(name, radius, at, colour, strength, facing=(0.0, 0.0, 0.0)):
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=radius)
    obj = mesh_object(name, bm, glow_material(name, colour, strength))
    obj.location = at
    obj.rotation_euler = facing
    return obj


def aerial_cheese():
    """A quadcopter of cheese: the wheel, four arms out to its corners, a motor and a rotor's blur on each."""
    body = wheel("Drone", 0.72, 0.4)
    metal = principled("Metal", (0.12, 0.12, 0.14, 1.0), 0.3, subsurface=0.0)
    rotor = principled("Rotor", (0.75, 0.78, 0.82, 1.0), 0.25, subsurface=0.0)
    rotor.node_tree.nodes["Principled BSDF"].inputs["Alpha"].default_value = 0.55
    for i, angle in enumerate((45.0, 135.0, 225.0, 315.0)):
        a = math.radians(angle)
        way = Vector((math.cos(a), math.sin(a), 0.0))
        arm_at = way * 0.85
        box(f"Arm{i}", (0.95, 0.09, 0.07), arm_at + Vector((0.0, 0.0, 0.12)), metal, (0.0, 0.0, a), 0.02)
        tip = way * 1.2
        disc(f"Motor{i}", 0.09, 0.16, tip + Vector((0.0, 0.0, 0.2)), metal)
        disc(f"Rotor{i}", 0.36, 0.015, tip + Vector((0.0, 0.0, 0.3)), rotor)
        # A blade across each blur, so that it reads as a rotor rather than a plate.
        box(f"Blade{i}", (0.68, 0.06, 0.012), tip + Vector((0.0, 0.0, 0.31)), metal, (0.0, 0.0, a + math.radians(30.0 * i)))
    view((math.radians(52.0), 0.0, math.radians(20.0)))


def cheese_strike():
    """A wheel falling, tilted, with the streaks of its fall over it, and a red target ring on the floor under it."""
    body = wheel("Bomb", 0.78, 0.42)
    body.location = (0.05, 0.0, 0.45)
    body.rotation_euler = (math.radians(-30.0), math.radians(-20.0), math.radians(200.0))
    red = principled("Target", (0.95, 0.08, 0.05, 1.0), 0.4, emission=(1.0, 0.1, 0.05, 1.0), strength=1.2, subsurface=0.0)
    torus("Ring", 1.05, 0.1, (0.0, 0.0, -0.75), red)
    torus("Inner", 0.52, 0.09, (0.0, 0.0, -0.75), red)
    disc("Bull", 0.16, 0.05, (0.0, 0.0, -0.75), red)
    # The fall's streaks: thin pale bars over the wheel, fading upwards in size.
    streak = principled("Streak", (1.0, 0.92, 0.7, 1.0), 0.6, emission=(1.0, 0.85, 0.5, 1.0), strength=0.8, subsurface=0.0)
    for i, (x, y, length) in enumerate(((-0.45, 0.1, 0.4), (0.1, -0.2, 0.55), (0.6, 0.15, 0.35))):
        box(f"Streak{i}", (0.075, 0.075, length), (x, y, 1.15 + length / 2.0), streak, bevel=0.03)
    view((math.radians(62.0), 0.0, math.radians(15.0)))


def gouda_overload():
    """A wheel bursting and melting: cracked open, chunks flying off, drips running down, before a radiating glow."""
    rng = random.Random(11)
    body = wheel("Overload", 0.7, 0.42, rng=rng)
    body.rotation_euler = (math.radians(12.0), math.radians(-8.0), 0.0)
    cheese = principled("Chunks", CHEESE, 0.42)
    melt = principled("Melt", (1.0, 0.62, 0.05, 1.0), 0.15, emission=(1.0, 0.55, 0.05, 1.0), strength=0.6)
    # Chunks flying out round it.
    for i in range(9):
        a = math.radians(i * 40.0 + rng.uniform(-10, 10))
        out = rng.uniform(1.0, 1.35)
        at = Vector((math.cos(a) * out, math.sin(a) * out * 0.75, rng.uniform(-0.2, 0.55)))
        s = rng.uniform(0.13, 0.2)
        box(f"Chunk{i}", (s, s * 0.8, s * 0.7), at, cheese, (rng.uniform(0, 3), rng.uniform(0, 3), rng.uniform(0, 3)), s * 0.25)
    # Drips running down its side and hanging off it.
    for i in range(7):
        a = math.radians(-160.0 + i * 24.0 + rng.uniform(-6, 6))
        at = Vector((math.cos(a) * 0.68, math.sin(a) * 0.68, -0.2))
        length = rng.uniform(0.18, 0.42)
        drip = box(f"Drip{i}", (0.07, 0.07, length), at - Vector((0.0, 0.0, length / 2.0)), melt, bevel=0.03)
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.065)
        bead = mesh_object(f"Bead{i}", bm, melt)
        bead.location = at - Vector((0.0, 0.0, length))
        bead.data.shade_smooth()
    # The glow behind it, and its rays.
    glow("Glow", 1.9, (0.0, 0.6, -0.4), (1.0, 0.45, 0.05, 1.0), 2.2, (math.radians(60.0), 0.0, 0.0))
    rays = glow_material("Rays", (1.0, 0.62, 0.12, 1.0), 3.6)
    for i in range(12):
        a = math.radians(i * 30.0 + 15.0)
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=1.0)
        for v in bm.verts:
            v.co = Vector((v.co.x * 2.0, v.co.y * 0.09, 0.0))
        ray = mesh_object(f"Ray{i}", bm, rays)
        ray.location = (0.0, 0.5, -0.3)
        ray.rotation_euler = (math.radians(60.0), 0.0, 0.0)
        ray.rotation_mode = "XZY"
        ray.rotation_euler = (math.radians(60.0), a, 0.0)
    view((math.radians(60.0), 0.0, 0.0), glow_share=0.55)


def view(rotation, glow_share=0.0):
    """An orthographic camera looking from `rotation`, framing everything solid square and filling the picture (the glows,
    which fade to nothing at their edges, by `glow_share` of their size), and the countdown's three lights."""
    scene = bpy.context.scene
    camera_data = bpy.data.cameras.new("Camera")
    camera_data.type = "ORTHO"
    camera = link(bpy.data.objects.new("Camera", camera_data))
    camera.rotation_euler = rotation
    turn = camera.rotation_euler.to_matrix()
    inverse = turn.inverted()
    bpy.context.view_layer.update()
    low = Vector((1e9, 1e9))
    high = Vector((-1e9, -1e9))
    for obj in scene.objects:
        if obj.type != "MESH":
            continue
        glowing = obj.name.startswith("Glow") or obj.name.startswith("Ray")
        if glowing and glow_share <= 0.0:
            continue
        centre = obj.matrix_world.translation
        for v in obj.data.vertices:
            world = obj.matrix_world @ v.co
            if glowing:
                world = centre + (world - centre) * glow_share
            local = inverse @ world
            low.x, low.y = min(low.x, local.x), min(low.y, local.y)
            high.x, high.y = max(high.x, local.x), max(high.y, local.y)
    middle = (low + high) / 2.0
    camera_data.ortho_scale = max(high.x - low.x, high.y - low.y) * 1.06
    forward = turn @ Vector((0.0, 0.0, -1.0))
    camera.location = turn @ Vector((middle.x, middle.y, 0.0)) - forward * 20.0
    scene.camera = camera

    def light(name, energy, rot, size):
        data = bpy.data.lights.new(name, "SUN")
        data.energy = energy
        data.angle = math.radians(size)
        lamp = link(bpy.data.objects.new(name, data))
        lamp.rotation_euler = [math.radians(a) for a in rot]

    light("Key", 4.2, (-38.0, -30.0, 0.0), 6.0)
    light("Fill", 1.3, (-10.0, 40.0, 0.0), 20.0)
    light("Rim", 2.0, (-120.0, 0.0, 0.0), 4.0)


def render(build, name, out):
    clear()
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 128
    scene.cycles.use_denoising = True
    scene.cycles.device = "CPU"
    scene.render.film_transparent = True
    scene.render.resolution_x = SIZE
    scene.render.resolution_y = SIZE
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    world = bpy.data.worlds.new("World") if scene.world is None else scene.world
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.25, 0.22, 0.2, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    build()
    scene.render.filepath = os.path.join(out, name + ".png")
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else os.path.join(os.getcwd(), "streaks"))
    os.makedirs(out, exist_ok=True)
    report = []
    for build, name in zip((aerial_cheese, cheese_strike, gouda_overload), ICONS):
        render(build, name, out)
        report.append(f"{name}: {SIZE} x {SIZE}")
    with open(os.path.join(out, "streak_icons.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


main()
