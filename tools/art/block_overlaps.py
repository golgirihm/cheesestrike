"""Lists, for every map, the places where two surfaces lie in the same plane and overlap: what z-fights, and so
flickers, as the camera moves (the depth buffer can't tell which of two coplanar faces is in front, and the answer
changes from pixel to pixel and frame to frame). Also lists blocks of cheese whose boxes overlap (one inside another)
and actors standing exactly on another of the same mesh (duplicates).

Each static mesh is taken as the box it fills, along the world's axes (turned by a multiple of 90 degrees about z;
others are counted and left out):
  - a rounded block of cheese (SM_CheeseBlock_*, block_mesh.py) stands block_mesh.OVERLAP proud of its size on every
    face, and only the middle of each face is flat (block_mesh.ROUNDING in from its edges);
  - the engine's cube is flat to its edges;
  - a wheel (the engine's cylinder or SM_CheeseWheel) is flat only on its top and bottom, inside its rim;
  - anything else (Fab's props) only for duplicates.
Two faces facing the same way, in the same plane (within PLANE cm), whose flat parts overlap by more than AREA cm^2,
z-fight wherever both are seen.

    UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/block_overlaps.py" -unattended -nullrhi

(the editor closed) writes Saved/block_overlaps.txt. Nothing is saved.
"""

import os
import sys
import traceback
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import block_mesh

MAPS = ("Lvl_Creamery", "Lvl_FondueFloor", "Lvl_Grotto", "Lvl_Kitchen", "Lvl_Labyrinth", "Lvl_ProvingGround", "Lvl_Range", "Lvl_RindMarket")
PLANE = 0.6         # cm: faces nearer than this are in one plane to the depth buffer at play distances
AREA = 25.0         # cm^2: less than a 5 cm square of overlap isn't worth listing
INSIDE = 1.0        # cm: blocks' boxes overlapping by more than this along every axis overlap


def shape_of(component):
    """(kind, centre, half extents, rounding, proud) of a static mesh component, or None if it isn't along the axes."""
    mesh = component.get_editor_property("static_mesh")
    if mesh is None:
        return None
    name = mesh.get_name()
    rotation = component.get_world_rotation()
    if abs(rotation.pitch) > 0.5 or abs(rotation.roll) > 0.5:
        return "turned"
    quarter = round(rotation.yaw / 90.0)
    if abs(rotation.yaw - quarter * 90.0) > 0.5:
        return "turned"
    scale = component.get_world_scale()
    size = block_mesh.size_from(name)
    if size is not None:
        half = [size[0] / 2.0 * scale.x, size[1] / 2.0 * scale.y, size[2] / 2.0 * scale.z]
        kind = "block"
        rounding = min(block_mesh.ROUNDING, min(size) / 3.0)
        proud = block_mesh.OVERLAP
    else:
        box = mesh.get_bounding_box()
        local = [(box.max.x - box.min.x) / 2.0, (box.max.y - box.min.y) / 2.0, (box.max.z - box.min.z) / 2.0]
        half = [local[0] * abs(scale.x), local[1] * abs(scale.y), local[2] * abs(scale.z)]
        kind = {"Cube": "cube", "Cylinder": "wheel", "SM_CheeseWheel": "wheel"}.get(name, "other")
        rounding = 0.0 if kind == "cube" else 0.15 * 2.0 * half[0] if kind == "wheel" else 0.0
        proud = 0.0
    if quarter % 2:
        half[0], half[1] = half[1], half[0]
    # The middle of the mesh's box, in the world (a mesh's pivot need not be its middle).
    low, high = component.get_local_bounds()
    middle_local = unreal.Vector((low.x + high.x) / 2.0, (low.y + high.y) / 2.0, (low.z + high.z) / 2.0)
    centre = component.get_world_transform().transform_location(middle_local)
    return kind, (centre.x, centre.y, centre.z), half, rounding, proud, name


def faces(entry):
    """The flat faces of an entry: (axis, sign, plane, (u0, u1), (v0, v1))."""
    kind, centre, half, rounding, proud = entry["kind"], entry["centre"], entry["half"], entry["rounding"], entry["proud"]
    out = []
    if kind == "other":
        return out
    for axis in range(3):
        if kind == "wheel" and axis != 2:
            continue
        for sign in (-1, 1):
            plane = centre[axis] + sign * (half[axis] + proud)
            others = [a for a in range(3) if a != axis]
            spans = []
            for o in others:
                inset = rounding - proud
                spans.append((centre[o] - half[o] + inset, centre[o] + half[o] - inset))
            if all(s[1] > s[0] for s in spans):
                out.append((axis, sign, plane, spans[0], spans[1]))
    return out


def label(entry):
    return f"{entry['label']} ({entry['mesh']}{', wax' if entry['wax'] else ''})"


def check_map(level, lines):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    path = "/Game/CheeseStrike/Maps/" + level
    levels.load_level(path)
    if unreal.EditorLevelLibrary.get_editor_world().get_outermost().get_name() != path:
        lines.append(f"{level}: could not open")
        return
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    entries = []
    turned = 0
    for actor in eas.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not component.is_visible() or isinstance(component, unreal.InstancedStaticMeshComponent):
                continue
            shape = shape_of(component)
            if shape is None:
                continue
            if shape == "turned":
                turned += 1
                continue
            kind, centre, half, rounding, proud, mesh = shape
            material = component.get_material(0)
            wax = material is not None and "Wax" in material.get_name()
            entries.append({"label": actor.get_actor_label(), "kind": kind, "centre": centre, "half": half, "rounding": rounding,
                            "proud": proud, "mesh": mesh, "wax": wax, "cheese": isinstance(actor, unreal.CheeseStrikeCheese)})
    lines.append(f"{level}: {len(entries)} meshes along the axes ({sum(1 for e in entries if e['cheese'])} cheese), {turned} turned (left out)")

    # Duplicates: the same mesh at the same place and size.
    seen = {}
    duplicates = 0
    for e in entries:
        key = (e["mesh"],) + tuple(round(c) for c in e["centre"]) + tuple(round(h) for h in e["half"])
        if key in seen:
            duplicates += 1
            lines.append(f"  DUPLICATE {label(e)} on {label(seen[key])} at {tuple(round(c) for c in e['centre'])}")
        else:
            seen[key] = e

    # Coplanar faces facing the same way whose flat parts overlap.
    planes = {}
    for index, e in enumerate(entries):
        for face in faces(e):
            planes.setdefault((face[0], face[1], round(face[2])), []).append((index, face))
    pairs = {}
    for (axis, sign, key), here in planes.items():
        candidates = here + planes.get((axis, sign, key + 1), [])
        for i, (a, fa) in enumerate(here):
            for (b, fb) in candidates[i + 1:]:
                if a == b or abs(fa[2] - fb[2]) > PLANE:
                    continue
                du = min(fa[3][1], fb[3][1]) - max(fa[3][0], fb[3][0])
                dv = min(fa[4][1], fb[4][1]) - max(fa[4][0], fb[4][0])
                if du > 0 and dv > 0 and du * dv > AREA:
                    pair = (min(a, b), max(a, b))
                    pairs.setdefault(pair, []).append(("xyz"[axis] + ("+" if sign > 0 else "-"), du * dv, fa[2]))
    cheese_pairs = 0
    for (a, b), found in sorted(pairs.items(), key=lambda p: -max(f[1] for f in p[1])):
        ea, eb = entries[a], entries[b]
        if ea["cheese"] or eb["cheese"]:
            cheese_pairs += 1
        sides = ", ".join(f"{s} at {p:.1f}: {area:.0f} cm2" for s, area, p in found)
        lines.append(f"  COPLANAR {label(ea)} and {label(eb)}: {sides}")

    # Blocks of cheese inside one another.
    blocks = [e for e in entries if e["cheese"] and e["kind"] in ("block", "cube")]
    inside = 0
    for i, a in enumerate(blocks):
        for b in blocks[i + 1:]:
            depth = [min(a["centre"][k] + a["half"][k], b["centre"][k] + b["half"][k]) - max(a["centre"][k] - a["half"][k], b["centre"][k] - b["half"][k]) for k in range(3)]
            if all(d > INSIDE for d in depth):
                inside += 1
                lines.append(f"  OVERLAPPING {label(a)} and {label(b)}: {depth[0]:.0f} x {depth[1]:.0f} x {depth[2]:.0f} cm")
    lines.append(f"{level}: {duplicates} duplicates, {len(pairs)} coplanar pairs ({cheese_pairs} with cheese), {inside} overlapping blocks")


def main():
    out = unreal.Paths.project_saved_dir() + "block_overlaps.txt"
    lines = []
    try:
        for level in (sys.argv[1:] or MAPS):
            check_map(level, lines)
    except Exception:
        lines.append(traceback.format_exc())
    finally:
        open(out, "w").write("\n".join(lines))
        unreal.SystemLibrary.quit_editor()


main()
