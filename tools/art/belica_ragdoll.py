import unreal, traceback

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/belica_ragdoll.py" -unattended -nullrhi
# (both paths absolute). It writes what it did to Saved/belica_ragdoll.txt.
#
# Gives Lt. Belica a ragdoll. Her mesh came from Paragon with Belica_Extents
# as its physics asset, a few rough bodies for its bounds, which as a ragdoll
# sags and floats (limbs drifting as if in the wind); this makes one as the
# editor's Create Physics Asset does, a capsule for each bone of any length
# and limited joints between them, none under the head (with bodies of
# their own, her face and jaw swing loose as she falls, and the face melts),
# and gives it to her mesh.
OUT = unreal.Paths.project_saved_dir() + "belica_ragdoll.txt"
MESH = "/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica"
RAGDOLL = "/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/PA_Belica_Ragdoll"
lines = []
try:
    mesh = unreal.load_asset(MESH)
    if unreal.EditorAssetLibrary.does_asset_exist(RAGDOLL):
        unreal.EditorAssetLibrary.delete_asset(RAGDOLL)
    ragdoll = unreal.CheeseStrikeArtTools.make_ragdoll(mesh, RAGDOLL)
    lines.append(f"made {ragdoll.get_path_name() if ragdoll else None}; the mesh's is now {mesh.get_editor_property('physics_asset').get_path_name()}")
    lines.append("bodies: " + ", ".join(str(b) for b in unreal.CheeseStrikeArtTools.ragdoll_bodies(ragdoll)))
    lines.append("saved ragdoll " + str(unreal.EditorAssetLibrary.save_asset(RAGDOLL, False)))
    lines.append("saved mesh " + str(unreal.EditorAssetLibrary.save_asset(MESH, False)))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
