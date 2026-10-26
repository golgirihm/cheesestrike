import array, math, os, random, traceback, wave
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wave8_sounds.py" -unattended -nullrhi
# (both paths absolute). It writes what it did to Saved/wave8_sounds.txt.
#
# Two sets of sounds, made from files kept outside the repository (Content/LICENSES.md has their sources):
#
# The Swiss Shotgun's shots (/Game/OpenGameArt/Shotgun/SW_ShotgunShot1..3): three real shotgun blasts from
#   "The Free Firearm Sound Library" (CC0; OpenGameArt.org), its Mossberg and Nova recordings, 96 kHz 24-bit
#   stereo, each cut to start a moment before its blast, taken down to 48 kHz mono and faded out.
#
# The voice of the Unmanned Aerial Cheese streak (/Game/CheeseStrike/Voice/SW_UacOnline, SW_UacEnemy): the lines
#   "UAC is online" and "Enemy UAC online" spoken by Windows' own offline voice (System.Speech, Microsoft David,
#   slow), made with this PowerShell, run before the script:
#     Add-Type -AssemblyName System.Speech; $s = New-Object System.Speech.Synthesis.SpeechSynthesizer
#     $s.SelectVoice("Microsoft David Desktop"); $s.Rate = -2
#     $f = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(22050, 16, 1)   # bits and channels as enums
#     $s.SetOutputToWaveFile("<VOICE>\raw_online.wav", $f); $s.Speak("U A C is online."); (and raw_enemy.wav, "Enemy U A C online.")
#   and here lowered in pitch, filtered to a radio's band, driven a little, and given a squelch at each end and a
#   breath of static, so that it sounds like a serious voice on a command net.
FIREARMS = os.environ.get("CHEESESTRIKE_FIREARMS", r"C:\Hamid\code\FabDownloads\firearms\Prepared SFX Library")
VOICE = os.environ.get("CHEESESTRIKE_VOICE", r"C:\Hamid\code\FabDownloads\uac_voice")
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


def biquad(samples, kind, rate, freq, q=0.707):
    """An RBJ biquad, 'low' or 'high' pass."""
    w0 = 2 * math.pi * freq / rate
    alpha = math.sin(w0) / (2 * q)
    cs = math.cos(w0)
    if kind == "low":
        b0, b1, b2 = (1 - cs) / 2, 1 - cs, (1 - cs) / 2
    else:
        b0, b1, b2 = (1 + cs) / 2, -(1 + cs), (1 + cs) / 2
    a0, a1, a2 = 1 + alpha, -2 * cs, 1 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for x in samples:
        y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, x, y1, y
        out.append(y)
    return out


def resampled(samples, ratio):
    """Played at `ratio` of the speed (below 1: slower and deeper), by linear interpolation."""
    count = int(len(samples) / ratio)
    out = []
    for i in range(count):
        p = i * ratio
        j = int(p)
        f = p - j
        a = samples[j]
        b = samples[j + 1] if j + 1 < len(samples) else a
        out.append(a + (b - a) * f)
    return out


def trimmed(samples, rate, margin=0.04):
    """Without the silence round the speech."""
    peak = max(abs(s) for s in samples)
    first = onset(samples, 0.04)
    last = len(samples) - 1
    while last > first and abs(samples[last]) < peak * 0.04:
        last -= 1
    return samples[max(0, first - int(margin * rate)):min(len(samples), last + int(margin * rate))]


def radio(samples, rate, seed):
    """A voice on a command net: deeper, band-limited, driven, with squelch clicks and static under it."""
    rng = random.Random(seed)
    voice = trimmed(resampled(samples, 0.92), rate)
    for freq in (260.0, 260.0):
        voice = biquad(voice, "high", rate, freq)
    for freq in (3300.0, 3300.0):
        voice = biquad(voice, "low", rate, freq)
    loudest = max(abs(s) for s in voice) or 1.0
    voice = [math.tanh(2.2 * s / loudest) for s in voice]
    # The static, only while the carrier is open, and a click of the squelch at each end.
    lead = int(0.09 * rate)
    tail = int(0.12 * rate)
    total = lead + len(voice) + tail
    out = [0.0] * total
    hiss = biquad([rng.uniform(-1, 1) for _ in range(total)], "high", rate, 1200.0)
    hiss = biquad(hiss, "low", rate, 3600.0)
    for i in range(total):
        envelope = 1.0 if lead <= i < lead + len(voice) else 0.0
        out[i] = hiss[i] * 0.05 * envelope
    for i, s in enumerate(voice):
        out[lead + i] += s * 0.8
    for start, size in ((int(0.01 * rate), int(0.03 * rate)), (lead + len(voice) + int(0.02 * rate), int(0.05 * rate))):
        for i in range(size):
            if start + i < total:
                out[start + i] += rng.uniform(-1, 1) * 0.22 * (1.0 - i / size) ** 2
    return out


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

    # The voice.
    for raw, name, seed in (("raw_online.wav", "SW_UacOnline", 1), ("raw_enemy.wav", "SW_UacEnemy", 2)):
        samples, rate = read(os.path.join(VOICE, raw))
        make(faded(radio(samples, rate, seed), rate, 0.002, 0.05, 0.9), rate, "/Game/CheeseStrike/Voice", name, work)
except Exception:
    lines.append(traceback.format_exc())
with open(OUT, "w") as o:
    o.write("\n".join(lines) + "\n")
unreal.SystemLibrary.quit_editor()
