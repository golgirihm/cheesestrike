import unreal, sys, traceback

# Run in the game's project, with its editor closed, after
# tools/art/copy_from_lyra.py has copied a hero in:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/fit_hero.py <Hero> [animations]" -unattended -nullrhi
# (paths absolute). It writes what it did to Saved/fit_hero_<Hero>.txt. Fits
# a Paragon hero from the Lyra project to the game, as Lt. Belica was:
#
# - Lyra's animations carry editor-only animation modifiers whose Blueprints
#   need Lyra's own C++; here they only make errors on load, so they go.
# - The retargeter writes every bone at scale 1, but a hero's reference pose
#   may scale some (Belica's face hangs from faceAttach at 0.79; at 1 her
#   eyes and lips swell), so the retargeted animations drop those bones'
#   tracks and keep the hero's pose.
# - Paragon's textures are 4K sources; a soldier seen across an arena needs
#   far less. Every texture's stored source is cut to 1024 pixels (2048 for
#   the body's own maps, if MAIN_TEXTURE_SIZE says so), in 8 bits; with
#   "animations", which fits only the animations (after new sets come in
#   for weapons), the textures are left as they are.
HEROES = {
    "Belica": "/Game/ParagonLtBelica",
    "Murdock": "/Game/ParagonMurdock",
    "Wraith": "/Game/ParagonWraith",
    "Twinblast": "/Game/ParagonTwinblast",
}
FOLDERS = {
    "Belica": "/Game/ParagonLtBelica/Characters/Heroes/Belica",
    "Murdock": "/Game/ParagonMurdock/Characters/Heroes/Murdock",
    "Wraith": "/Game/ParagonWraith/Characters/Heroes/Wraith",
    "Twinblast": "/Game/ParagonTwinblast/Characters/Heroes/TwinBlast",
}
MESHES = {"Belica": "Belica", "Murdock": "Murdock", "Wraith": "Wraith", "Twinblast": "TwinBlast"}
TEXTURE_SIZE = 1024

args = [a for a in sys.argv[1:] if a]
hero = args[0]
only_animations = "animations" in args[1:]
OUT = unreal.Paths.project_saved_dir() + f"fit_hero_{hero}.txt"
lines = []
try:
    pack = HEROES[hero]
    folder = FOLDERS[hero]
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([pack], True)
    mesh = unreal.load_asset(f"{folder}/Meshes/{MESHES[hero]}")
    ref = mesh.skeleton.get_reference_pose()
    scaled = []
    for bone in unreal.AnimPoseExtensions.get_bone_names(ref):
        s = unreal.AnimPoseExtensions.get_bone_pose(ref, bone, unreal.AnimPoseSpaces.LOCAL).scale3d
        if abs(s.x - 1) > 1e-3 or abs(s.y - 1) > 1e-3 or abs(s.z - 1) > 1e-3:
            scaled.append(str(bone))
    lines.append("bones the reference pose scales: " + ", ".join(scaled))

    stripped = fixed = 0
    for data in registry.get_assets_by_path(folder + "/Animations", True):
        kind = str(data.asset_class_path.asset_name)
        if kind not in ("AnimSequence", "BlendSpace1D", "BlendSpace", "AimOffsetBlendSpace"):
            continue
        asset = data.get_asset()
        changed = False
        user_data = list(asset.get_editor_property("asset_user_data"))
        kept = [u for u in user_data if u is not None and "Modifier" not in u.get_class().get_name()]
        if len(kept) != len(user_data):
            asset.set_editor_property("asset_user_data", kept)
            stripped += 1
            changed = True
        if kind == "AnimSequence":
            tracks = [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(asset)]
            drop = [b for b in scaled if b in tracks]
            for bone in drop:
                unreal.AnimationLibrary.remove_bone_animation(asset, bone, False, True)
            if drop:
                fixed += 1
                changed = True
        if changed:
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    lines.append(f"animations: modifiers stripped from {stripped}, scaled bones' tracks dropped from {fixed}")

    fns = unreal.TextureAssetActionsBlueprintFns
    counts = {}
    for data in [] if only_animations else registry.get_assets_by_path(pack, True):
        kind = str(data.asset_class_path.asset_name)
        if kind not in ("Texture2D", "TextureCube"):
            continue
        texture = data.get_asset()
        limit = 512 if "Panorama" in str(data.asset_name) or kind == "TextureCube" else TEXTURE_SIZE
        results = []
        try:
            results.append(str(fns.convert_to8bit_texture_source(texture, False)))
        except Exception as e:
            results.append("8bit: " + str(e))
        try:
            results.append(str(fns.resize_texture_source(texture, limit)))
        except Exception as e:
            results.append("resize: " + str(e))
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
        key = " ".join(results)
        counts[key] = counts.get(key, 0) + 1
    lines.append("textures: " + str(counts))
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
