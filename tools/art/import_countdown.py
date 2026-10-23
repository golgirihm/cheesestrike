import os, subprocess, traceback
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/import_countdown.py" -unattended -nullrhi
# (both paths absolute: the editor runs from its own folder).
# It writes what it did to Saved/import_countdown.txt.
#
# The start countdown's words in 3D cheese (3, 2, 1, FIGHT): renders them with
# Blender (tools/art/countdown_glyphs.py; Blender from CHEESESTRIKE_BLENDER, or
# where it is installed on the development PC) into Saved/Countdown, then
# imports them into /Game/CheeseStrike/UI/Countdown, where the HUD loads them
# by path (UI/CheeseStrikeHUDCountdown.cpp; the folder is in
# DirectoriesToAlwaysCook).
OUT = unreal.Paths.project_saved_dir() + "import_countdown.txt"
DESTINATION = "/Game/CheeseStrike/UI/Countdown"
BLENDER = os.environ.get("CHEESESTRIKE_BLENDER", os.path.expandvars(r"%LOCALAPPDATA%\Programs\blender-5.2.2-windows-x64\blender.exe"))
lines = []
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def render(folder):
    script = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "tools", "art", "countdown_glyphs.py")
    done = subprocess.run([BLENDER, "-b", "--factory-startup", "-P", script, "--", folder], capture_output=True, text=True)
    lines.append(f"Blender exited {done.returncode}")
    if done.returncode != 0:
        lines.append(done.stdout[-2000:])
        lines.append(done.stderr[-2000:])
    report = os.path.join(folder, "countdown_glyphs.txt")
    if os.path.exists(report):
        lines.append(open(report).read())


def glyph(path, name):
    """A picture for the HUD: kept whole (never streamed out), mipmapped, as it is drawn smaller than it was rendered."""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    imported = [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]
    imported = [a for a in imported if isinstance(a, unreal.Texture2D)]
    if not imported:
        lines.append(f"  {name}: nothing imported from {path}")
        return
    t = imported[0]
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    t.set_editor_property("never_stream", True)
    t.set_editor_property("srgb", True)
    lib.save_asset(t.get_path_name(), False)
    lines.append(f"  {name}: {t.blueprint_get_size_x()} x {t.blueprint_get_size_y()}")


try:
    folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Countdown")
    os.makedirs(folder, exist_ok=True)
    render(folder)
    for file in sorted(os.listdir(folder)):
        if file.startswith("T_Count_") and file.endswith(".png"):
            glyph(os.path.join(folder, file), file[:-4])
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
