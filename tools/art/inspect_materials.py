import unreal, sys, traceback

# Run in the game's project, with its editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/inspect_materials.py" -unattended -nullrhi
# Writes to Saved/inspect_materials.txt each hero's material slots: the
# material, its parent chain, and its vector and scalar parameters with their
# values, for choosing what tools/art/team_looks.py tints.
MESHES = [
    "/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica",
    "/Game/ParagonMurdock/Characters/Heroes/Murdock/Meshes/Murdock",
    "/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith",
    "/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast",
]
OUT = unreal.Paths.project_saved_dir() + "inspect_materials.txt"
lines = []
lib = unreal.MaterialEditingLibrary
try:
    for path in MESHES:
        mesh = unreal.load_asset(path)
        lines.append("== " + path)
        for slot in mesh.materials:
            material = slot.material_interface
            lines.append(f"slot {slot.material_slot_name}: {material.get_path_name() if material else None} ({material.get_class().get_name() if material else ''})")
            if material is None:
                continue
            chain = []
            parent = material
            while isinstance(parent, unreal.MaterialInstance):
                parent = parent.get_editor_property("parent")
                chain.append(parent.get_path_name() if parent else "None")
            if chain:
                lines.append("   parents: " + " <- ".join(chain))
            try:
                vectors = lib.get_vector_parameter_names(material)
                for name in vectors:
                    value = lib.get_material_instance_vector_parameter_value(material, name) if isinstance(material, unreal.MaterialInstance) else lib.get_material_default_vector_parameter_value(material, name)
                    lines.append(f"   vector {name} = ({value.r:.2f}, {value.g:.2f}, {value.b:.2f}, {value.a:.2f})")
                scalars = lib.get_scalar_parameter_names(material)
                lines.append("   scalars: " + ", ".join(str(n) for n in scalars))
                textures = lib.get_texture_parameter_names(material)
                lines.append("   textures: " + ", ".join(str(n) for n in textures))
            except Exception as e:
                lines.append("   params: " + str(e))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
