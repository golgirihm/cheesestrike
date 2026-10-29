# -*- coding: utf-8 -*-
import array, math, os, random, subprocess, sys, traceback, wave

# The game's spoken lines: the streaks called in, the vermin coming, a teammate reloading, the rounds and the match.
#
# Two steps, each run from the repository's root:
#
# 1. Speak them, outside the editor, with Piper (https://github.com/OHF-Voice/piper1-gpl, the `piper-tts` package)
#    installed in a venv of its own, and its voice "norman" (en_US-norman-medium, from the rhasspy/piper-voices
#    repository on Hugging Face: trained from scratch on public-domain LibriVox recordings, so the voice is public
#    domain too):
#      python -m venv C:\Hamid\code\FabDownloads\piper\venv
#      C:\Hamid\code\FabDownloads\piper\venv\Scripts\python.exe -m pip install piper-tts
#      (en_US-norman-medium.onnx and .onnx.json into C:\Hamid\code\FabDownloads\piper\voices)
#      C:\Hamid\code\FabDownloads\piper\venv\Scripts\python.exe -I tools/art/voice_lines.py speak
#    which writes each line, spoken and then put through the radio below, to C:\Hamid\code\FabDownloads\piper\lines.
#    Piper says a line a little differently each time, and now and then garbles a short one: each line's take in
#    use (<name>.raw.wav there) was picked from several, every word checked by a transcription (Whisper; not yet by ear),
#    and `speak` keeps a .raw.wav already there, only filtering it again; delete one to have it spoken afresh.
#    A line recorded by a person instead goes in as <name>.raw.wav too (48 kHz mono, dry, no effects: this script
#    adds them).
#
# 2. Import them, with the editor closed:
#      UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/voice_lines.py" -unattended -nullrhi
#    into /Game/CheeseStrike/Voice (the game loads them by path: Rules/CheeseStrikeVoice.cpp's table). It writes
#    what it did to Saved/voice_lines.txt.

PIPER = os.environ.get("CHEESESTRIKE_PIPER", r"C:\Hamid\code\FabDownloads\piper")
VOICE = "en_US-norman-medium"
LINES_DIR = os.path.join(PIPER, "lines")

# (asset name, what is said, how it is heard): "radio" is a voice on the command net; "shout" a teammate near by.
# What is said is Piper's input: a word it says wrongly is given as phonemes, [[ ... ]] (UAC spelt out) or spelt as
# said (Gouda as "Gowda"). Piper garbles a sentence of one word, so such a line is said with more after a comma and
# cut at the pause before the last word as it is spoken (a fourth field, "first"; the take of "Victory" in use
# was cut where the transcription put "sir", since its own "vic-tory" has a pause in it).
LINES = [
    ("SW_UacOnline", "[[ jˈuː ˈeɪ sˈiː ]] online.", "radio"),
    ("SW_UacEnemy", "Enemy [[ jˈuː ˈeɪ sˈiː ]] online.", "radio"),
    ("SW_StrikeInbound", "Cheese Strike inbound.", "radio"),
    ("SW_StrikeEnemy", "Enemy Cheese Strike inbound.", "radio"),
    ("SW_OverloadIncoming", "Gowda overload incoming.", "radio"),
    ("SW_OverloadEnemy", "Enemy Gowda overload incoming.", "radio"),
    ("SW_VerminInbound", "Vermin inbound.", "radio"),
    ("SW_Reloading", "Reloading!", "shout"),
    ("SW_EnemyDown", "Enemy down.", "radio"),
    ("SW_RoundWon", "Round won.", "radio"),
    ("SW_RoundLost", "Round lost.", "radio"),
    ("SW_Victory", "Victory, sir.", "radio", "first"),
    ("SW_Defeat", "Defeat, sir.", "radio", "first"),
]


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


def shout(samples, rate):
    """A teammate calling out near by, heard in the room: a little deeper, the lows and the hiss off, pushed a little."""
    voice = trimmed(resampled(samples, 0.94), rate)
    voice = biquad(voice, "high", rate, 140.0)
    voice = biquad(voice, "low", rate, 6000.0)
    loudest = max(abs(s) for s in voice) or 1.0
    return [math.tanh(1.6 * s / loudest) for s in voice]


def before_pause(samples, rate, gap=0.05):
    """All that is said before the last pause (a stretch of `gap` seconds near silence between words): the line
    without the word said after it ("sir"), which is only there so that Piper doesn't garble a line of one word."""
    window = int(0.01 * rate)
    peak = max(abs(s) for s in samples) or 1.0
    loud = [max(abs(s) for s in samples[i:i + window]) > 0.04 * peak for i in range(0, len(samples) - window, window)]
    last = None
    quiet = 0
    begun = False
    for k, on in enumerate(loud):
        if on:
            if begun and quiet * window >= gap * rate:
                last = k - quiet
            begun = True
            quiet = 0
        else:
            quiet += 1
    return samples[:last * window + int(0.03 * rate)] if last is not None else samples


def speak():
    """Step 1, outside the editor: each line spoken by Piper (unless a take is there already) and put through its filter."""
    os.makedirs(LINES_DIR, exist_ok=True)
    model = os.path.join(PIPER, "voices", VOICE + ".onnx")
    for seed, (name, text, how, *cut) in enumerate(LINES, 1):
        raw = os.path.join(LINES_DIR, name + ".raw.wav")
        if not os.path.exists(raw):
            # A little slower than its own pace: measured, as a voice on a net is.
            subprocess.run([sys.executable, "-m", "piper", "-m", model, "-f", raw, "--length-scale", "1.05"],
                           input=text.encode("utf-8"), check=True, capture_output=True)
            if cut:
                said, rate = read(raw)
                write(before_pause(said, rate), rate, raw)
        samples, rate = read(raw)
        heard = radio(samples, rate, seed) if how == "radio" else shout(samples, rate)
        write(faded(heard, rate, 0.002, 0.05, 0.9), rate, os.path.join(LINES_DIR, name + ".wav"))
        print(f"{name}: {len(heard) / rate:.2f} s")


def import_lines():
    """Step 2, in the editor: the filtered lines into /Game/CheeseStrike/Voice."""
    import unreal
    out = unreal.Paths.project_saved_dir() + "voice_lines.txt"
    lines = []
    try:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        for name, text, how, *cut in LINES:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(LINES_DIR, name + ".wav"))
            task.set_editor_property("destination_path", "/Game/CheeseStrike/Voice")
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            made = [a for a in (unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")) if isinstance(a, unreal.SoundWave)]
            if not made:
                raise RuntimeError(f"nothing imported for {name}")
            unreal.EditorAssetLibrary.save_asset(made[0].get_path_name(), False)
            lines.append(f"{name}: \"{text}\" ({how}) {made[0].get_editor_property('duration'):.2f} s")
    except Exception:
        lines.append(traceback.format_exc())
    with open(out, "w") as o:
        o.write("\n".join(lines) + "\n")
    unreal.SystemLibrary.quit_editor()


if len(sys.argv) > 1 and sys.argv[1] == "speak":
    speak()
elif __name__ == "__main__" or "unreal" in sys.modules:
    import_lines()
