import array, os, traceback, wave
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/stab_sound.py" -unattended -nullrhi
# (both paths absolute). It writes what it did to Saved/stab_sound.txt.
#
# The execution's knife (UCheeseStrikeLastStand): the sound of the blade going into the chest, and of it being pulled out,
# made from "8 Wet Squish, Slurp Impacts" by Independent.nu (OpenGameArt.org, CC0), downloaded into SOURCE (outside the
# repository; Content/LICENSES.md has its page) and its archive unpacked there. The editor reads the FLACs (Python can't),
# and their sound is written out as WAVs and mixed here:
#   SW_KnifeStabIn: the sharp, wet bite of impactsplat06 with the dull thud of impactsplat03 under it, their onsets
#     together, the silence before cut off, faded out over its last tenth of a second;
#   SW_KnifeStabOut: the rising slurp of impactsplat08, its quiet lead-in cut short.
# Both are imported, mono, into /Game/OpenGameArt/Stab.
SOURCE = os.environ.get("CHEESESTRIKE_STAB", r"C:\Hamid\code\FabDownloads\stab_sounds\wet\impsplat")
WORK = os.environ.get("CHEESESTRIKE_STAB_WORK", r"C:\Hamid\code\FabDownloads\stab_sounds\made")
OUT = unreal.Paths.project_saved_dir() + "stab_sound.txt"
DESTINATION = "/Game/OpenGameArt/Stab"

lines = []
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def import_file(path, destination, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]


def export_wav(asset, path):
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", path)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("prompt", False)
    return unreal.Exporter.run_asset_export_task(task)


def decoded(name):
    """The splat `name` (impactsplatNN) as mono floats from -1 to 1, and its rate."""
    source = os.path.join(SOURCE, name + ".mp3.flac")
    wav = os.path.join(WORK, name + ".wav")
    asset = [a for a in import_file(source, "/Game/Temp", "SW_Stab_" + name) if isinstance(a, unreal.SoundWave)]
    if not asset:
        raise RuntimeError(f"nothing imported from {source}")
    export_wav(asset[0], wav)
    lib.delete_asset("/Game/Temp/SW_Stab_" + name)
    with wave.open(wav, "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        data = array.array("h", w.readframes(frames))
    if width != 2:
        raise RuntimeError(f"{wav}: {width * 8}-bit, not 16")
    mono = [sum(data[i:i + channels]) / channels / 32768.0 for i in range(0, len(data), channels)]
    return mono, rate


def onset(samples, share=0.12):
    """The first sample as loud as `share` of the loudest."""
    peak = max(abs(s) for s in samples)
    for i, s in enumerate(samples):
        if abs(s) >= peak * share:
            return i
    return 0


def shaped(samples, rate, fade_in, fade_out, peak=0.89):
    """Faded in and out (seconds), and made `peak` loud at most."""
    out = list(samples)
    n_in = max(1, int(fade_in * rate))
    n_out = max(1, int(fade_out * rate))
    for i in range(min(n_in, len(out))):
        out[i] *= i / n_in
    for i in range(min(n_out, len(out))):
        out[len(out) - 1 - i] *= i / n_out
    loudest = max(abs(s) for s in out) or 1.0
    return [s * peak / loudest for s in out]


def write(samples, rate, path):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(array.array("h", [max(-32768, min(32767, int(round(s * 32767)))) for s in samples]).tobytes())


def make(name, samples, rate):
    path = os.path.join(WORK, name + ".wav")
    write(samples, rate, path)
    made = [a for a in import_file(path, DESTINATION, name) if isinstance(a, unreal.SoundWave)]
    if not made:
        raise RuntimeError(f"nothing imported from {path}")
    lib.save_asset(made[0].get_path_name(), False)
    lines.append(f"{name}: {made[0].get_editor_property('duration'):.2f} s")


try:
    os.makedirs(WORK, exist_ok=True)
    bite, rate = decoded("impactsplat06")
    thud, thud_rate = decoded("impactsplat03")
    slurp, slurp_rate = decoded("impactsplat08")
    if not (rate == thud_rate == slurp_rate):
        raise RuntimeError("the splats differ in rate")
    # In: the bite and the thud together from their onsets (a few milliseconds before them, so the attack is whole).
    lead = int(0.004 * rate)
    a = max(0, onset(bite) - lead)
    b = max(0, onset(thud) - lead)
    length = int(0.55 * rate)
    mixed = []
    for i in range(length):
        s = (bite[a + i] if a + i < len(bite) else 0.0) * 0.8
        s += (thud[b + i + int(0.008 * rate)] if b + i + int(0.008 * rate) < len(thud) else 0.0) * 0.75
        mixed.append(s)
    make("SW_KnifeStabIn", shaped(mixed, rate, 0.002, 0.1), rate)
    # Out: the slurp from where it begins to rise, to its end.
    start = max(0, onset(slurp, 0.35) - int(0.1 * rate))
    make("SW_KnifeStabOut", shaped(slurp[start:], rate, 0.03, 0.03, 0.8), rate)
    lines.append(f"in: bite from {a / rate:.3f} s, thud from {b / rate:.3f} s; out: slurp from {start / rate:.3f} s")
except Exception:
    lines.append(traceback.format_exc())
with open(OUT, "w") as o:
    o.write("\n".join(lines) + "\n")
unreal.SystemLibrary.quit_editor()
