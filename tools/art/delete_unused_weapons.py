"""Deletes the weapon assets nothing uses any more (wave 15): the Fab guns the older looks borrowed (Max3d's PPSh-41 and
DP-28, SpatialNeglect's SV-98 and Classic M4, Xeradev's M1911) with their textures, and the material instances made
for them and for Lyra's guns in /Game/CheeseStrike/Weapons/Materials. The maps' stands were saved holding the look's
assets of the day they were built (a pickup's components are set again at play, ACheeseStrikePickup::ApplyLook, so
what the map keeps is stale): each map is opened first and any mesh component of any actor that still holds one of
these assets or materials has it cleared, and the map saved. Then each asset is deleted only if nothing else references
it (unreal.EditorAssetLibrary.find_package_referencers_for_asset, after the others on the list); what was cleared,
deleted and kept, and why, is written to Saved/delete_unused_weapons.txt. A map is saved through map_kit.save_level,
which leaves the editor's unbuilt navigation mesh out of it (saved once, in wave 15, it left every map's bots and mice
with nowhere to walk: the game keeps a saved mesh as built and rebuilds only where something changes). Run headless,
the editor closed:

    UnrealEditor-Cmd.exe CheeseStrike.uproject -ExecCmds="py tools/art/delete_unused_weapons.py" -unattended -nullrhi
"""

import os
import sys
import traceback
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art"))
import map_kit as kit

FOLDERS = ["/Game/Fab/Max3d", "/Game/Fab/SpatialNeglect", "/Game/Fab/Xeradev"]
INSTANCES = ["MI_Blaster", "MI_Carbine", "MI_CarbineSling", "MI_CarbineScope", "MI_Grater", "MI_Munster", "MI_Shotgun", "MI_Roquefort",
             "MI_RoquefortScope", "MI_Pistol", "MI_BurratasSteel"]
MATERIALS = "/Game/CheeseStrike/Weapons/Materials/"
MAPS = "/Game/CheeseStrike/Maps"

lib = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lines = []


def package_of(asset):
    return asset.get_outermost().get_name() if asset is not None else None


def clear_references(level, doomed):
    """Opens `level` and takes the doomed assets off every mesh component of every actor in it; saves it if any was."""
    if not levels.load_level(level):
        lines.append(f"could not open {level}")
        return
    current = unreal.EditorLevelLibrary.get_editor_world().get_outermost().get_name()
    if current != level:
        lines.append(f"the level open is {current}, not {level}; left alone")
        return
    cleared = 0
    for actor in eas.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.MeshComponent):
            # The materials first: a component whose mesh has gone has no slots to look through.
            overrides = list(component.get_editor_property("override_materials"))
            if any(package_of(m) in doomed for m in overrides):
                component.set_editor_property("override_materials", [None if package_of(m) in doomed else m for m in overrides])
                cleared += 1
            if isinstance(component, unreal.StaticMeshComponent) and package_of(component.get_editor_property("static_mesh")) in doomed:
                component.set_static_mesh(None)
                cleared += 1
            elif isinstance(component, unreal.SkeletalMeshComponent) and package_of(component.get_editor_property("skeletal_mesh_asset")) in doomed:
                component.set_skeletal_mesh_asset(None)
                cleared += 1
    if cleared:
        lines.append(f"{level}: cleared {cleared} stale meshes and materials off its actors; saved {kit.save_level()}")
    else:
        lines.append(f"{level}: nothing stale")


try:
    assets = [MATERIALS + name for name in INSTANCES]
    for folder in FOLDERS:
        assets += [path.split(".")[0] for path in lib.list_assets(folder, recursive=True, include_folder=False)]
    assets = [a for a in assets if lib.does_asset_exist(a)]
    doomed = set(assets)
    for path in lib.list_assets(MAPS, recursive=True, include_folder=False):
        level = path.split(".")[0]
        if unreal.load_asset(level).__class__.__name__ == "World":
            clear_references(level, doomed)
    # Meshes before their textures, instances before anything: a referencer that is itself on the list doesn't count.
    kept = {}
    for attempt in range(3):
        kept = {}
        for path in list(assets):
            referencers = [r for r in lib.find_package_referencers_for_asset(path, load_assets_to_confirm=True) if r.split(".")[0] not in doomed]
            if referencers:
                kept[path] = referencers
                continue
            deleted = lib.delete_asset(path)
            lines.append(f"{'deleted' if deleted else 'could not delete'} {path}")
            if deleted:
                assets.remove(path)
        if not kept:
            break
    for path, referencers in kept.items():
        lines.append(f"kept {path}: referenced by {', '.join(referencers)}")
    for folder in FOLDERS:
        if lib.does_directory_exist(folder) and not lib.list_assets(folder, recursive=True, include_folder=False):
            lib.delete_directory(folder)
            lines.append(f"removed the empty folder {folder}")
except Exception:
    lines.append(traceback.format_exc())
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "delete_unused_weapons.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    unreal.SystemLibrary.quit_editor()
