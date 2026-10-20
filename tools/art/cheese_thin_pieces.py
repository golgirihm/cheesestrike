import unreal, traceback

# Puts the shooter level's thin cheese on MI_CheeseThin, which has no holes:
# a piece under 30 cm thick, or standing less than 30 cm above the floor (the
# stripes on the floor are blocks sunk into it, 2 cm proud). Holes carved
# into what looks like a slab that thin would look like holes in paper. Run after cheese_material.py, with the
# editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/cheese_thin_pieces.py" -unattended -nullrhi
# It writes what it did to Saved/cheese_thin_pieces.txt.
OUT = unreal.Paths.project_saved_dir() + "cheese_thin_pieces.txt"
CHEESE = "/Game/FirstPerson/MI_FirstPersonColorway.MI_FirstPersonColorway"
THIN = "/Game/CheeseStrike/Materials/MI_CheeseThin"
THIN_BELOW = 30.0
FLOOR = 0.0
lines = []
try:
    thin = unreal.load_asset(THIN)
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Variant_Shooter/Lvl_Shooter")
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            slots = [i for i in range(comp.get_num_materials()) if comp.get_material(i) is not None and comp.get_material(i).get_path_name() in (CHEESE, thin.get_path_name())]
            if not slots:
                continue
            origin, extent, radius = unreal.SystemLibrary.get_component_bounds(comp)
            thickness = 2.0 * min(extent.x, extent.y, extent.z)
            standing = (origin.z + extent.z) - max(origin.z - extent.z, FLOOR)
            if thickness >= THIN_BELOW and standing >= THIN_BELOW:
                continue
            actor.modify()
            comp.modify()
            for i in slots:
                comp.set_material(i, thin)
            lines.append(f"{actor.get_actor_label()}: {thickness:.0f} cm thick, {standing:.0f} cm above the floor, no holes")
    lines.append("saved " + str(unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
