import array, math, os, sys, traceback, wave

# The weapons' sounds of wave 17, made with numpy from files kept outside the repository and from nothing (Content/LICENSES.md
# has the sources; CheeseStrikeWeaponLooks loads them by path):
#
# The Swiss Shotgun's blast (/Game/OpenGameArt/Shotgun/SW_ShotgunBlast1..3): a big, boomy blast with a low punch and a long
#   tail, layered as the Roquefort's report is: three pump shotguns of "The Free Firearm Sound Library" (CC0,
#   OpenGameArt.org; the Mossberg's N_30P, the Nova's O_21P and the Model 12's K_22P, in
#   C:\Hamid\code\FabDownloads\firearms\Prepared SFX Library), each cut from just before its blast, with a sub punch
#   (a tone from 70 Hz down to 38 over 60 ms), a thump of 120 Hz falling to 55 under the crack, a short body of noise at
#   150-500 Hz, a rumble of low noise rolling on for over a second and two far, dull echoes after the recording's own, 3 s.
# The Cheese Stick's blow landing on someone (/Game/CheeseStrike/Sounds/SW_StickThock), made from nothing: a loud, intense
#   "thock": a hard click at its front, the ring of a struck wooden block (four modes of 380-1700 Hz dying in 10-45 ms), a
#   slap of mid noise, and under them a body thump (a tone from 130 Hz down to 55) and a knock of low noise, driven into
#   saturation for its intensity, 0.45 s.
#
# Two steps, from the repository's root:
#   1. C:\Hamid\code\FabDownloads\piper\venv\Scripts\python.exe -I tools/art/wave17_sounds.py prepare
#      (any Python with numpy and soundfile) writes the WAVs to C:\Hamid\code\FabDownloads\wave17_prepared and prints
#      each one's length, peak and RMS.
#   2. With the editor closed:
#      UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wave17_sounds.py" -unattended -nullrhi
#      imports them; it writes what it did to Saved/wave17_sounds.txt.

DOWNLOADS = os.environ.get("CHEESESTRIKE_DOWNLOADS", r"C:\Hamid\code\FabDownloads")
PREPARED = os.path.join(DOWNLOADS, "wave17_prepared")
FIREARMS = os.environ.get("CHEESESTRIKE_FIREARMS", os.path.join(DOWNLOADS, "firearms", "Prepared SFX Library"))
# The shotgun's three blasts: the Mossberg's, the Nova's and the Model 12's.
BLASTS = [os.path.join("Mossberg", "N_30P.wav"), os.path.join("Nova", "O_21P.wav"), os.path.join("Model 12", "K_22P.wav")]
RATE = 48000
SOUNDS = [(f"SW_ShotgunBlast{i}.wav", "/Game/OpenGameArt/Shotgun", f"SW_ShotgunBlast{i}") for i in range(1, 4)] \
    + [("SW_StickThock.wav", "/Game/CheeseStrike/Sounds", "SW_StickThock")]
# The editor's step imports only the sounds whose names hold one of these words, if any are given in the environment
# (e.g. CHEESESTRIKE_ONLY="Thock"); all of them otherwise.
ONLY = os.environ.get("CHEESESTRIKE_ONLY", "").split()


def write(samples, rate, path):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(array.array("h", [max(-32768, min(32767, int(round(s * 32767)))) for s in samples]).tobytes())


def prepare():
    import numpy as np
    import soundfile as sf

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

    def to_rate(x, rate, wanted):
        n = int(len(x) * wanted / rate)
        return np.interp(np.arange(n) * rate / wanted, np.arange(len(x)), x)

    def shaped(x, rate, keep, fade_in, fade_out, peak):
        x = x[:int(keep * rate)].copy()
        n_in = max(1, int(fade_in * rate))
        x[:n_in] *= np.linspace(0, 1, n_in)
        n_out = min(len(x), int(fade_out * rate))
        x[len(x) - n_out:] *= np.linspace(1, 0, n_out) ** 2
        return x * peak / (np.abs(x).max() or 1.0)

    def save(name, x, rate):
        write(x, rate, os.path.join(PREPARED, name + ".wav"))
        rms = math.sqrt(float(np.mean(x * x))) if len(x) else 0.0
        peak = float(np.abs(x).max()) if len(x) else 0.0
        # The loudness of its first 100 ms, where a blow or a blast is heard.
        head = x[:int(0.1 * rate)]
        head_rms = math.sqrt(float(np.mean(head * head))) if len(head) else 0.0
        spectrum = np.abs(np.fft.rfft(x)) ** 2
        hertz = np.fft.rfftfreq(len(x), 1.0 / rate)
        low = spectrum[hertz < 150].sum() / spectrum.sum()
        high = spectrum[hertz > 2000].sum() / spectrum.sum()
        print(f"{name}: {len(x) / rate:.3f} s, peak {20 * math.log10(peak or 1e-9):.1f} dBFS ({peak:.2f}), RMS {20 * math.log10(rms or 1e-9):.1f} dBFS, "
              f"first 100 ms {20 * math.log10(head_rms or 1e-9):.1f} dBFS; {low:.2f} of its energy under 150 Hz, {high:.2f} over 2 kHz")

    os.makedirs(PREPARED, exist_ok=True)
    rng = np.random.default_rng(1717)

    # The Swiss Shotgun's blast.
    for number, name in enumerate(BLASTS, 1):
        x, rate = mono(os.path.join(FIREARMS, name))
        peak = np.abs(x).max()
        start = max(0, int(np.argmax(np.abs(x) >= peak * 0.4)) - int(0.015 * rate))
        keep = 3.0
        blast = to_rate(x[start:start + int(keep * rate)], rate, RATE)
        blast = blast / (np.abs(blast).max() or 1.0)
        count = len(blast)
        if count < int(keep * RATE):
            blast = np.concatenate([blast, np.zeros(int(keep * RATE) - count)])
            count = len(blast)
        t = np.arange(count) / RATE
        at = 0.015
        since = np.maximum(0.0, t - at)
        on = np.where(t < at, 0.0, 1.0)
        # The punch: a sub tone from 70 Hz to 38, in over 2 ms, gone in about 200 ms.
        punch = np.sin(np.cumsum(2 * math.pi * (38.0 + 32.0 * np.exp(-since / 0.06)) / RATE)) * on * np.minimum(1.0, since / 0.002) * np.exp(-since / 0.11)
        # The thump under the crack: 120 Hz to 55 over 80 ms.
        thump = np.sin(np.cumsum(2 * math.pi * (55.0 + 65.0 * np.exp(-since / 0.04)) / RATE)) * on * np.minimum(1.0, since / 0.003) * np.exp(-since / 0.08)
        # Its body: noise of 150-500 Hz for the first 60 ms.
        body = biquad(biquad(rng.uniform(-1, 1, count), "high", RATE, 150.0), "low", RATE, 500.0) * 3.0 * on * np.exp(-since / 0.03)
        rumble = biquad(biquad(rng.uniform(-1, 1, count), "low", RATE, 110.0), "low", RATE, 110.0) * 9.0
        rumble *= on * np.minimum(1.0, since / 0.02) * np.exp(-since / 0.75)
        dull = biquad(biquad(blast, "low", RATE, 800.0), "low", RATE, 800.0)
        echoes = np.zeros(count)
        for delay, share in ((0.28, 0.34), (0.62, 0.22)):
            k = int(delay * RATE)
            echoes[k:] += dull[:count - k] * share
        report = np.tanh(1.35 * (blast * 1.2 + punch * 0.4 + thump * 0.35 + body * 0.3 + rumble * 0.16 + echoes))
        save(f"SW_ShotgunBlast{number}", shaped(report, RATE, keep, 0.001, 1.0, 0.97), RATE)

    # The Cheese Stick's thock.
    count = int(0.45 * RATE)
    t = np.arange(count) / RATE
    attack = np.minimum(1.0, t / 0.0004)
    click = biquad(biquad(rng.uniform(-1, 1, count), "high", RATE, 2500.0), "low", RATE, 9000.0) * 2.5 * np.exp(-t / 0.0025)
    # A struck block of hard wood: its modes, each falling a little in pitch as it dies.
    block = np.zeros(count)
    for hertz, decay, share in ((380.0, 0.045, 0.9), (610.0, 0.035, 1.0), (1050.0, 0.02, 0.6), (1700.0, 0.01, 0.4)):
        phase = np.cumsum(2 * math.pi * hertz * (1.0 + 0.06 * np.exp(-t / 0.01)) / RATE)
        block += np.sin(phase) * np.exp(-t / decay) * share
    slap = biquad(biquad(rng.uniform(-1, 1, count), "high", RATE, 600.0), "low", RATE, 3000.0) * 2.0 * np.exp(-t / 0.018)
    thump = np.sin(np.cumsum(2 * math.pi * (55.0 + 75.0 * np.exp(-t / 0.035)) / RATE)) * np.minimum(1.0, t / 0.0015) * np.exp(-t / 0.11)
    knock = biquad(biquad(rng.uniform(-1, 1, count), "low", RATE, 250.0), "low", RATE, 250.0) * 7.0 * np.exp(-t / 0.04)
    thock = np.tanh(1.8 * attack * (click * 1.0 + block * 1.4 + slap * 0.8 + thump * 0.6 + knock * 0.35))
    save("SW_StickThock", shaped(thock, RATE, 0.45, 0.0002, 0.15, 0.97), RATE)


def import_sounds():
    import unreal
    out = unreal.Paths.project_saved_dir() + "wave17_sounds.txt"
    lines = []
    try:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        for source, destination, name in SOUNDS:
            if ONLY and not any(word.lower() in name.lower() for word in ONLY):
                continue
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
