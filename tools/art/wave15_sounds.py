import array, math, os, sys, traceback, wave

# The sounds of wave 15, made with numpy from files kept outside the repository and from nothing (Content/LICENSES.md
# has the sources; CheeseStrikeWeaponLooks and CheeseStrikeSounds load them by path):
#
# The Cheese Grater's shot (/Game/CheeseStrike/Sounds/SW_GraterShot1..3): a rasping "zzzip", made from nothing: a
#   short burst of noise through a comb filter whose pitch sweeps down over 80 ms, with a faint metallic ring.
# The Cheese Stick's swing (/Game/CheeseStrike/Sounds/SW_StickSwing1..3): a heavy, slow whoosh, about 350 ms of body
#   and a tail (0.55 s in all), noise through a band whose centre falls as the stroke goes; and its blow landing
#   (SW_StickHit): a deep thump, a low tone falling with a knock of low noise.
# The Swiss Shotgun's shot (/Game/OpenGameArt/Shotgun/SW_ShotgunBoom1..3): the three blasts of wave 8 cut again from
#   "The Free Firearm Sound Library" (CC0, OpenGameArt.org; its Mossberg and Nova recordings, 96 kHz 24-bit, in
#   C:\Hamid\code\FabDownloads\firearms) with a thump of 60-120 Hz lasting 150 ms laid under the crack and a rumble
#   that tails longer than the recording's.
# Five more hurt grunts (/Game/OpenGameArt/Hurt/SW_Oof6..10), to make ten with wave 14's: hurt_05 of "Hurt Sound
#   Effects" by EZduzziteh (CC0, in C:\Hamid\code\FabDownloads\oga_hurt_ezduzziteh), and pain1, pain3, pain4 and
#   pain6 of "11 male human pain/death sounds" by Michel Baradari (CC BY 3.0, in
#   C:\Hamid\code\FabDownloads\oga_pain_baradari\player), made as wave 14's were: mono, a little deeper, the highs
#   softened, cut short, faded and made alike in loudness.
#
# Two steps, from the repository's root:
#   1. C:\Hamid\code\FabDownloads\piper\venv\Scripts\python.exe -I tools/art/wave15_sounds.py prepare
#      (any Python with numpy and soundfile) writes the WAVs to C:\Hamid\code\FabDownloads\wave15_prepared and prints
#      each one's length, peak and RMS.
#   2. With the editor closed:
#      UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wave15_sounds.py" -unattended -nullrhi
#      imports them; it writes what it did to Saved/wave15_sounds.txt.

DOWNLOADS = os.environ.get("CHEESESTRIKE_DOWNLOADS", r"C:\Hamid\code\FabDownloads")
PREPARED = os.path.join(DOWNLOADS, "wave15_prepared")
FIREARMS = os.environ.get("CHEESESTRIKE_FIREARMS", os.path.join(DOWNLOADS, "firearms", "Prepared SFX Library"))
RATE = 48000
OWN = "/Game/CheeseStrike/Sounds"
# (prepared file, destination folder, asset name)
SOUNDS = [(f"SW_GraterShot{i}.wav", OWN, f"SW_GraterShot{i}") for i in range(1, 4)] \
    + [(f"SW_StickSwing{i}.wav", OWN, f"SW_StickSwing{i}") for i in range(1, 4)] \
    + [("SW_StickHit.wav", OWN, "SW_StickHit")] \
    + [(f"SW_ShotgunBoom{i}.wav", "/Game/OpenGameArt/Shotgun", f"SW_ShotgunBoom{i}") for i in range(1, 4)] \
    + [(f"SW_Oof{i}.wav", "/Game/OpenGameArt/Hurt", f"SW_Oof{i}") for i in range(6, 11)]
# The grunts: (file, how much faster it is played; the pain pack's are slower, deeper takes than EZduzziteh's).
OOFS = [
    (os.path.join("oga_hurt_ezduzziteh", "hurt_05.mp3"), 0.87),
    (os.path.join("oga_pain_baradari", "player", "pain1.wav"), 1.0),
    (os.path.join("oga_pain_baradari", "player", "pain3.wav"), 1.0),
    (os.path.join("oga_pain_baradari", "player", "pain4.wav"), 1.0),
    (os.path.join("oga_pain_baradari", "player", "pain6.wav"), 1.0),
]
# The shotgun's blasts: (file, the blast's number counting from the first), as wave 8 cut them.
BLASTS = [("Mossberg\\N_30P.wav", 0), ("Nova\\O_21P.wav", 0), ("Nova\\O_21P.wav", 1)]


def write(samples, rate, path):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(array.array("h", [max(-32768, min(32767, int(round(s * 32767)))) for s in samples]).tobytes())


def prepare():
    import numpy as np
    import soundfile as sf

    rng = np.random.default_rng(1515)

    def mono(path):
        x, rate = sf.read(path, dtype="float64")
        return (x.mean(axis=1) if x.ndim > 1 else x), rate

    def biquad(x, kind, rate, freq, q=0.707):
        """An RBJ biquad: 'low', 'high' or 'band' (constant peak gain) pass."""
        w0 = 2 * math.pi * freq / rate
        alpha = math.sin(w0) / (2 * q)
        cs = math.cos(w0)
        if kind == "low":
            b = [(1 - cs) / 2, 1 - cs, (1 - cs) / 2]
        elif kind == "high":
            b = [(1 + cs) / 2, -(1 + cs), (1 + cs) / 2]
        else:
            b = [alpha, 0.0, -alpha]
        a = [1 + alpha, -2 * cs, 1 - alpha]
        b = [v / a[0] for v in b]
        a1, a2 = a[1] / a[0], a[2] / a[0]
        y = np.zeros_like(x)
        x1 = x2 = y1 = y2 = 0.0
        for i, v in enumerate(x):
            out = b[0] * v + b[1] * x1 + b[2] * x2 - a1 * y1 - a2 * y2
            x2, x1, y2, y1 = x1, v, y1, out
            y[i] = out
        return y

    def sweeping_band(x, rate, centres, q):
        """Noise through a band-pass whose centre follows `centres` (one per sample): a state-variable filter, run sample by sample."""
        y = np.zeros_like(x)
        low = band = 0.0
        for i, v in enumerate(x):
            f = 2.0 * math.sin(math.pi * min(centres[i], rate * 0.22) / rate)
            high = v - low - band / q
            band += f * high
            low += f * band
            y[i] = band
        return y

    def resampled(x, ratio):
        """Played at `ratio` of the speed (below 1: slower and deeper)."""
        n = int(len(x) / ratio)
        return np.interp(np.arange(n) * ratio, np.arange(len(x)), x)

    def to_rate(x, rate, wanted):
        n = int(len(x) * wanted / rate)
        return np.interp(np.arange(n) * rate / wanted, np.arange(len(x)), x)

    def from_onset(x, rate, share=0.25, lead=0.004):
        peak = np.abs(x).max()
        first = int(np.argmax(np.abs(x) >= peak * share))
        return x[max(0, first - int(lead * rate)):]

    def shaped(x, rate, keep, fade_in, fade_out, peak):
        x = x[:int(keep * rate)].copy()
        n_in = max(1, int(fade_in * rate))
        x[:n_in] *= np.linspace(0, 1, n_in)
        n_out = min(len(x), int(fade_out * rate))
        x[len(x) - n_out:] *= np.linspace(1, 0, n_out) ** 2
        return x * peak / (np.abs(x).max() or 1.0)

    def measured(name, x, rate):
        rms = math.sqrt(float(np.mean(x * x))) if len(x) else 0.0
        peak = float(np.abs(x).max()) if len(x) else 0.0
        print(f"{name}: {len(x) / rate:.3f} s, peak {20 * math.log10(peak or 1e-9):.1f} dBFS ({peak:.2f}), RMS {20 * math.log10(rms or 1e-9):.1f} dBFS")

    def save(name, x, rate=RATE):
        write(x, rate, os.path.join(PREPARED, name + ".wav"))
        measured(name, x, rate)

    os.makedirs(PREPARED, exist_ok=True)

    # The Cheese Grater's "zzzip": 80 ms of noise, in at once, through a comb filter (a fed-back delay) whose pitch falls from
    # about 2.4 kHz to 900 Hz over the burst, so that it rasps; a faint ring of three inharmonic metallic partials over it,
    # and a short tail, 120 ms in all.
    for number in range(1, 4):
        count = int(0.12 * RATE)
        t = np.arange(count) / RATE
        burst = rng.uniform(-1, 1, count) * np.minimum(1.0, t / 0.0015) * np.exp(-t / 0.028)
        burst = biquad(biquad(burst, "high", RATE, 500.0), "low", RATE, 5000.0)
        pitch_from, pitch_to = 2400.0 * (1.0 + 0.08 * (number - 2)), 900.0
        y = np.zeros(count)
        for i in range(count):
            along = min(1.0, t[i] / 0.08)
            hertz = pitch_from * (pitch_to / pitch_from) ** along
            delay = RATE / hertz
            k = i - delay
            j = int(math.floor(k))
            frac = k - j
            back = (y[j] * (1 - frac) + y[j + 1] * frac) if j >= 0 and j + 1 < i else 0.0
            # Fed back hard, so that the comb's pitch is heard over the noise; the feedback's highs damped a little.
            y[i] = burst[i] * 0.3 + 0.93 * back
        ring = sum(0.08 * share * np.sin(2 * math.pi * hertz * t) * np.exp(-t / 0.045)
                   for hertz, share in ((3150.0, 1.0), (4710.0, 0.6), (6890.0, 0.35)))
        zip_ = y + ring * np.minimum(1.0, t / 0.002)
        save(f"SW_GraterShot{number}", shaped(zip_, RATE, 0.12, 0.0005, 0.03, 0.85))

    # The Cheese Stick's swing: noise through a band whose centre falls from about 1400 Hz to 250 Hz as the stroke goes,
    # swelling over the first 150 ms to its loudest at 200 ms and falling away over the rest; a low rumble under it for the
    # weight. About 350 ms of body (above -20 dB of its peak), 0.55 s with the tail.
    for number in range(1, 4):
        count = int(0.55 * RATE)
        t = np.arange(count) / RATE
        along = np.minimum(1.0, t / 0.45)
        centres = 1400.0 * (1.0 + 0.1 * (number - 2)) * (250.0 / 1400.0) ** along
        whoosh = sweeping_band(rng.uniform(-1, 1, count), RATE, centres, 1.6)
        rumble = biquad(biquad(rng.uniform(-1, 1, count), "low", RATE, 160.0), "low", RATE, 160.0) * 6.0
        envelope = np.where(t < 0.2, (t / 0.2) ** 1.6, np.exp(-(t - 0.2) / 0.11))
        swing = (whoosh * 1.0 + rumble * 0.9 * np.where(t < 0.25, t / 0.25, 1.0)) * envelope
        save(f"SW_StickSwing{number}", shaped(swing, RATE, 0.55, 0.005, 0.08, 0.9))

    # The Cheese Stick's blow: a tone falling from 95 Hz to 45 over a quarter of a second, with a knock of noise low-passed
    # at 220 Hz at its front and a soft click of mid noise at the very start.
    count = int(0.3 * RATE)
    t = np.arange(count) / RATE
    phase = np.cumsum(2 * math.pi * (45.0 + 50.0 * np.exp(-t / 0.05)) / RATE)
    tone = np.sin(phase) * np.minimum(1.0, t / 0.004) * np.exp(-t / 0.09)
    knock = biquad(biquad(rng.uniform(-1, 1, count), "low", RATE, 220.0), "low", RATE, 220.0) * 6.0 * np.exp(-t / 0.03)
    click = biquad(rng.uniform(-1, 1, count), "low", RATE, 1200.0) * 0.35 * np.exp(-t / 0.004)
    save("SW_StickHit", shaped(np.tanh(1.4 * (tone + knock + click)), RATE, 0.3, 0.001, 0.06, 0.9))

    # The Swiss Shotgun's boom: each blast cut as wave 8 cut it (a moment before the blast, taken to 48 kHz mono), with a
    # thump under its crack (a tone from 120 Hz down to 60 over 150 ms, in over 5 ms) and a rumble of low noise that tails
    # longer than the recording's, 2.2 s in all, the whole held just under full scale.
    cache = {}
    for number, (name, nth) in enumerate(BLASTS, 1):
        if name not in cache:
            cache[name] = mono(os.path.join(FIREARMS, name))
        x, rate = cache[name]
        peak = np.abs(x).max()
        start = int(np.argmax(np.abs(x) >= peak * 0.5))
        if nth > 0:
            later = start + int(2.0 * rate)
            start = later + int(np.argmax(np.abs(x[later:]) >= np.abs(x[later:]).max() * 0.5))
        start = max(0, start - int(0.02 * rate))
        keep = 2.2
        blast = to_rate(x[start:start + int(keep * rate)], rate, RATE)
        blast = blast / (np.abs(blast).max() or 1.0)
        count = len(blast)
        t = np.arange(count) / RATE
        at = 0.02
        since = np.maximum(0.0, t - at)
        phase = np.cumsum(2 * math.pi * (60.0 + 60.0 * np.exp(-since / 0.05)) / RATE)
        thump = np.sin(phase) * np.where(t < at, 0.0, np.minimum(1.0, since / 0.005)) * np.exp(-since / 0.07)
        rumble = biquad(biquad(rng.uniform(-1, 1, count), "low", RATE, 110.0), "low", RATE, 110.0) * 9.0
        rumble *= np.where(t < at, 0.0, np.minimum(1.0, since / 0.03)) * np.exp(-since / 0.45)
        # The thump and the rumble under the crack, not over it: the crack keeps about a third of the energy, where
        # the recording alone has four fifths, and the band of 60-120 Hz about two fifths, where it alone has a hundredth.
        boom = np.tanh(1.15 * (blast * 1.0 + thump * 0.32 + rumble * 0.2))
        save(f"SW_ShotgunBoom{number}", shaped(boom, RATE, keep, 0.001, 0.6, 0.95))

    # The grunts, as wave 14 made them.
    for number, (name, faster) in enumerate(OOFS, 6):
        x, rate = mono(os.path.join(DOWNLOADS, name))
        x = resampled(from_onset(x, rate, 0.1, 0.01), faster)
        x = biquad(biquad(x, "low", rate, 3200.0), "high", rate, 90.0)
        save(f"SW_Oof{number}", shaped(x, rate, 0.45, 0.002, 0.12, 0.8), rate)


def import_sounds():
    import unreal
    out = unreal.Paths.project_saved_dir() + "wave15_sounds.txt"
    lines = []
    try:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        for source, destination, name in SOUNDS:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(PREPARED, source))
            task.set_editor_property("destination_path", destination)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            made = [a for a in (unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")) if isinstance(a, unreal.SoundWave)]
            if not made:
                raise RuntimeError(f"nothing imported from {source}")
            unreal.EditorAssetLibrary.save_asset(made[0].get_path_name(), False)
            lines.append(f"{destination}/{name}: {made[0].get_editor_property('duration'):.2f} s")
    except Exception:
        lines.append(traceback.format_exc())
    with open(out, "w") as o:
        o.write("\n".join(lines) + "\n")
    unreal.SystemLibrary.quit_editor()


if len(sys.argv) > 1 and sys.argv[1] == "prepare":
    prepare()
else:
    import_sounds()
