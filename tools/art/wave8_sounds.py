import array, os, traceback, wave
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wave8_sounds.py" -unattended -nullrhi
# (both paths absolute). It writes what it did to Saved/wave8_sounds.txt.
#
# The Swiss Shotgun's shots, made from files kept outside the repository (Content/LICENSES.md has their sources).
# (The voice's lines, once made here too, are tools/art/voice_lines.py's.)
#
# The Swiss Shotgun's shots (/Game/OpenGameArt/Shotgun/SW_ShotgunShot1..3): three real shotgun blasts from
#   "The Free Firearm Sound Library" (CC0; OpenGameArt.org), its Mossberg and Nova recordings, 96 kHz 24-bit
#   stereo, each cut to start a moment before its blast, taken down to 48 kHz mono and faded out.
FIREARMS = os.environ.get("CHEESESTRIKE_FIREARMS", r"C:\Hamid\code\FabDownloads\firearms\Prepared SFX Library")
OUT = unreal.Paths.project_saved_dir() + "wave8_sounds.txt"

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


def read(path):
    """A WAV (any width, any channels) as mono floats from -1 to 1, and its rate."""
    with wave.open(path, "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    scale = float(1 << (8 * width - 1))
    out = []
    for i in range(frames):
        total = 0
        for c in range(channels):
            o = (i * channels + c) * width
            total += int.from_bytes(raw[o:o + width], "little", signed=True)
        out.append(total / channels / scale)
    return out, rate


def write(samples, rate, path):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(array.array("h", [max(-32768, min(32767, int(round(s * 32767)))) for s in samples]).tobytes())


def onset(samples, share, start=0):
    """The first sample from `start` as loud as `share` of the loudest from there."""
    peak = max(abs(s) for s in samples[start:])
    for i in range(start, len(samples)):
        if abs(samples[i]) >= peak * share:
            return i
    return start


def halved(samples):
    """Half the rate: pairs averaged."""
    return [(samples[i] + samples[i + 1]) * 0.5 for i in range(0, len(samples) - 1, 2)]


def faded(samples, rate, fade_in, fade_out, peak):
    out = list(samples)
    n_in = max(1, int(fade_in * rate))
    n_out = max(1, int(fade_out * rate))
    for i in range(min(n_in, len(out))):
        out[i] *= i / n_in
    for i in range(min(n_out, len(out))):
        out[len(out) - 1 - i] *= i / n_out
    loudest = max(abs(s) for s in out) or 1.0
    return [s * peak / loudest for s in out]


def make(samples, rate, destination, name, work):
    path = os.path.join(work, name + ".wav")
    write(samples, rate, path)
    made = [a for a in import_file(path, destination, name) if isinstance(a, unreal.SoundWave)]
    if not made:
        raise RuntimeError(f"nothing imported from {path}")
    lib.save_asset(made[0].get_path_name(), False)
    lines.append(f"{name}: {made[0].get_editor_property('duration'):.2f} s")


try:
    work = os.path.join(unreal.Paths.project_saved_dir(), "wave8_sounds")
    os.makedirs(work, exist_ok=True)

    # The shotgun: (file, the blast's number counting from the first, how many seconds to keep).
    blasts = [("Mossberg\\N_30P.wav", 0, 1.8), ("Nova\\O_21P.wav", 0, 1.8), ("Nova\\O_21P.wav", 1, 1.8)]
    cache = {}
    for number, (name, nth, keep) in enumerate(blasts, 1):
        if name not in cache:
            cache[name] = read(os.path.join(FIREARMS, name))
        samples, rate = cache[name]
        start = onset(samples, 0.5)
        if nth > 0:
            # The next blast after this one's tail has died down: the first loud sample 2 s on.
            start = onset(samples, 0.5, start + int(2.0 * rate))
        start = max(0, start - int(0.02 * rate))
        piece = halved(samples[start:start + int(keep * rate)])
        make(faded(piece, rate // 2, 0.001, 0.5, 0.95), rate // 2, "/Game/OpenGameArt/Shotgun", f"SW_ShotgunShot{number}", work)
        lines.append(f"shot {number}: {name} from {start / rate:.3f} s")
except Exception:
    lines.append(traceback.format_exc())
with open(OUT, "w") as o:
    o.write("\n".join(lines) + "\n")
unreal.SystemLibrary.quit_editor()
