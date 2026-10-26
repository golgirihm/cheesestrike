import unreal, traceback

# Run in the game's project, with its editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/team_looks.py" -unattended -nullrhi
# (paths absolute). It writes what it did to Saved/team_looks.txt.
#
# Dresses each hero in each side's colours, so that the sides are told apart
# at a glance and at range: for each material slot in LOOKS, a material
# instance of the slot's own material, MI_<slot>_<side> in the hero's
# Materials/Teams folder, with the colour parameters that paint its armour
# (or coat, or metal) set to the side's colour, as the lobby and the HUD show
# it (CheeseStrikeModes::TeamColour): the Gouda Guys orange, the Mozza Radicals
# violet. Only parameters change, so the instances share their parents'
# shaders. CheeseStrikeBodies names the slots (TeamSlots), and
# ACheeseStrikeCharacter puts the instances on.

def linear(r, g, b):
    return [((c / 255.0) / 12.92) if c / 255.0 <= 0.04045 else (((c / 255.0) + 0.055) / 1.055) ** 2.4 for c in (r, g, b)]

# The sides' colours (CheeseStrikeMenu::TeamColour), as linear colours.
SIDES = {"Gouda": linear(230, 159, 0), "Mozza": linear(180, 120, 255)}

# Each hero: its Materials folder, and for each slot, the colour parameters to
# set and how bright (the side's colour times this: a parameter that paints a
# surface keeps it under 1, one that multiplies a texture goes over).
LOOKS = {
    "Belica": ("/Game/ParagonLtBelica/Characters/Heroes/Belica/Materials", "/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica", {
        "M00_upperbody": [("PrimaryArmorColor", 0.45)],
        "M01_lowerbody": [("PrimaryArmorColor", 0.45)],
    }),
    "Murdock": ("/Game/ParagonMurdock/Characters/Heroes/Murdock/Materials", "/Game/ParagonMurdock/Characters/Heroes/Murdock/Meshes/Murdock", {
        "M00_Body": [("BodyMetalColor", 0.7)],
        "M02_Arms": [("BodyMetalColor", 0.7)],
        "M03_Legs": [("BodyMetalColor", 0.7)],
    }),
    "Wraith": ("/Game/ParagonWraith/Characters/Heroes/Wraith/Materials", "/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith", {
        "M00_lowerbody": [("MeatllicCCTint", 1.4)],
        "M01_upperbody": [("MeatllicCCTint", 1.4)],
        "M02_backpackhead": [("MeatllicCCTint", 1.4)],
    }),
    "Twinblast": ("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Materials", "/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast", {
        "M00_Head": [("ArmColor", 0.6)],
        "M01_cloth": [("DetailColor", 0.6), ("CoatColor", 0.35)],
        "M03_Arms": [("ArmColor", 0.6)],
        "M04_LowerBody": [("DetailColor", 0.6)],
    }),
}

# Brighter or darker than the brightness above for a side on a hero, by name: none now. (The Mozza Radicals were mozzarella white and
# needed lifting on three heroes; their violet is put on as the Gouda Guys' orange is.)
WHITER = {}

OUT = unreal.Paths.project_saved_dir() + "team_looks.txt"
lines = []
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
try:
    for hero, (folder, mesh_path, slots) in LOOKS.items():
        mesh = unreal.load_asset(mesh_path)
        materials = {str(slot.material_slot_name): slot.material_interface for slot in mesh.materials}
        teams = folder + "/Teams"
        for slot, parameters in slots.items():
            parent = materials[slot]
            for side, colour in SIDES.items():
                name = f"MI_{slot}_{side}"
                path = f"{teams}/{name}"
                if unreal.EditorAssetLibrary.does_asset_exist(path):
                    unreal.EditorAssetLibrary.delete_asset(path)
                instance = tools.create_asset(name, teams, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                lib.set_material_instance_parent(instance, parent)
                overrides = []
                for parameter, brightness in parameters:
                    brightness *= WHITER.get(side, {}).get(hero, 1.0)
                    value = unreal.LinearColor(colour[0] * brightness, colour[1] * brightness, colour[2] * brightness, 1.0)
                    info = unreal.MaterialParameterInfo(name=parameter, association=unreal.MaterialParameterAssociation.GLOBAL_PARAMETER, index=-1)
                    overrides.append(unreal.VectorParameterValue(parameter_info=info, parameter_value=value))
                    lines.append(f"{hero} {name}: {parameter} = ({value.r:.2f}, {value.g:.2f}, {value.b:.2f})")
                instance.set_editor_property("vector_parameter_values", overrides)
                lib.update_material_instance(instance)
                lines.append("   now " + ", ".join(f"{p}={lib.get_material_instance_vector_parameter_value(instance, p)}" for p, _ in parameters))
                lines.append(f"saved {path} {unreal.EditorAssetLibrary.save_asset(path, False)}")
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
