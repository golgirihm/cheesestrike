import array, math, os, sys, traceback, wave

# The sounds of wave 16, made with numpy from files kept outside the repository (Content/LICENSES.md has the sources;
# CheeseStrikeSounds loads them by path):
#
# The weapon pickup's clack (/Game/OpenGameArt/Clicks/SW_PickupClack): one syllable of a reload, a click and its clack
#   66 ms later, cut from "equipment clicks III" by LFA (CC0, OpenGameArt.org: a real bolt-action rifle cocked, a
#   stapler and a tape measure snapped; in C:\Hamid\code\FabDownloads\oga_equipment_clicks3), its body of 1-3 kHz kept
#   and lifted, the hiss above 7 kHz softened, a low knock of about 160 Hz laid under the clack, 110 ms in all.
# The female hurt grunts (/Game/OpenGameArt/HurtFemale/SW_OofFemale1..10), for Lt. Belica (CheeseStrikeSounds::VoiceOf):
#   the seven of "Female Hurt Grunts & Groans" by Nocturnal_Vanguard (AuraVoice; CC0, in
#   C:\Hamid\code\FabDownloads\oga_female_hurt) and the three "damaged" takes of the mature voice (Type 3) of
#   "RPG Voice Starter Pack" by cicifyre (CC0, in C:\Hamid\code\FabDownloads\oga_rpg_voice_starter), made as the male
#   ones were: mono, the highs softened, cut to 0.45 s at most, faded, at the same peak.
# The Roquefort Rifle's shot (/Game/OpenGameArt/Rifle/SW_RoquefortShot1..3): a heavy, powerful report with a long tail,
#   three bolt-action rifles of "The Free Firearm Sound Library" (CC0, OpenGameArt.org; the Tikka T3, the Arisaka and the
#   M1917, W_24P, E_18P and B_16P, in C:\Hamid\code\FabDownloads\firearms\Prepared SFX Library, out of its lib.7z),
#   each cut from just before its blast, with a deep thump of 50-110 Hz under the crack, a rumble of low noise and two
#   far, dull echoes rolling on after the recording's own, 2.8 s in all, just under full scale.
# The Cheese Stick's blow landing on someone (/Game/CheeseStrike/Sounds/SW_StickPound), made from nothing: a deep,
#   heavy pound, a body tone falling from 75 Hz to 36, a knock of low noise, a dull slap of mid noise at its front and a
#   smaller second knock 70 ms on, driven a little into saturation for its weight, with a short crunch over it, 0.6 s.
#
# Two steps, from the repository's root:
#   0. The Roquefort's rifles out of the library's archive, once (7-Zip):
#      7z x C:\Hamid\code\FabDownloads\firearms\lib.7z "Prepared SFX Library/Tikka/*.wav" "Prepared SFX Library/Arisaka/*.wav" "Prepared SFX Library/1917/*.wav"
#   1. C:\Hamid\code\FabDownloads\piper\venv\Scripts\python.exe -I tools/art/wave16_sounds.py prepare
#      (any Python with numpy and soundfile) writes the WAVs to C:\Hamid\code\FabDownloads\wave16_prepared and prints
#      each one's length, peak and RMS.
#   2. With the editor closed:
#      UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wave16_sounds.py" -unattended -nullrhi
#      imports them; it writes what it did to Saved/wave16_sounds.txt.

DOWNLOADS = os.environ.get("CHEESESTRIKE_DOWNLOADS", r"C:\Hamid\code\FabDownloads")
PREPARED = os.path.join(DOWNLOADS, "wave16_prepared")
CLICKS = os.path.join(DOWNLOADS, "oga_equipment_clicks3", "equipment_clicks3.wav")
# The click-clack in the recording: from (seconds), and how long the clack runs.
CLACK_FROM = 3.725
CLACK_KEEP = 0.110
# Where the second, louder transient (the clack) falls in the cut, for the knock under it.
CLACK_AT = 0.070
FEMALE_GROANS = os.path.join(DOWNLOADS, "oga_female_hurt", "female_hurt_grunts_groans_1.ogg")
# The seven grunts in the groans' recording (seconds), as an envelope over -35 dB of its peak finds them.
GROANS = [(0.34, 1.14), (2.10, 2.70), (3.72, 4.12), (4.92, 6.09), (7.34, 8.09), (9.33, 10.30), (11.70, 12.56)]
STARTER = os.path.join(DOWNLOADS, "oga_rpg_voice_starter", "RPG Voice Starter Pack", "Type 3")
FIREARMS = os.environ.get("CHEESESTRIKE_FIREARMS", os.path.join(DOWNLOADS, "firearms", "Prepared SFX Library"))
# The Roquefort's three reports: the Tikka T3's, the Arisaka's and the M1917's.
RIFLES = [os.path.join("Tikka", "W_24P.wav"), os.path.join("Arisaka", "E_18P.wav"), os.path.join("1917", "B_16P.wav")]
RATE = 48000
SOUNDS = [("SW_PickupClack.wav", "/Game/OpenGameArt/Clicks", "SW_PickupClack")] \
    + [(f"SW_OofFemale{i}.wav", "/Game/OpenGameArt/HurtFemale", f"SW_OofFemale{i}") for i in range(1, 11)] \
    + [(f"SW_RoquefortShot{i}.wav", "/Game/OpenGameArt/Rifle", f"SW_RoquefortShot{i}") for i in range(1, 4)] \
    + [("SW_StickPound.wav", "/Game/CheeseStrike/Sounds", "SW_StickPound")]
# The editor's step imports only the sounds whose names hold one of these words, if any are given in the environment
# (e.g. CHEESESTRIKE_ONLY="Roquefort Stick"); all of them otherwise.
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

    def from_onset(x, rate, share=0.1, lead=0.01):
        peak = np.abs(x).max()
        first = int(np.argmax(np.abs(x) >= peak * share))
        return x[max(0, first - int(lead * rate)):]

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
        spectrum = np.abs(np.fft.rfft(x)) ** 2
        hertz = np.fft.rfftfreq(len(x), 1.0 / rate)
        body = spectrum[(hertz >= 1000) & (hertz <= 3000)].sum() / spectrum.sum()
        low = spectrum[hertz < 300].sum() / spectrum.sum()
        print(f"{name}: {len(x) / rate:.3f} s, peak {20 * math.log10(peak or 1e-9):.1f} dBFS ({peak:.2f}), RMS {20 * math.log10(rms or 1e-9):.1f} dBFS, "
              f"{body:.2f} of its energy at 1-3 kHz, {low:.2f} under 300 Hz")

    os.makedirs(PREPARED, exist_ok=True)

    # The clack: the rifle's click-clack, its body lifted, its hiss softened, a low knock under the clack.
    x, rate = mono(CLICKS)
    cut = x[int(CLACK_FROM * rate):int((CLACK_FROM + CLACK_KEEP + 0.01) * rate)]
    body = biquad(cut, "band", rate, 1800.0, 0.8)
    cut = biquad(biquad(cut + body * 1.2, "low", rate, 7000.0), "high", rate, 120.0)
    cut = cut / (np.abs(cut).max() or 1.0)
    t = np.arange(len(cut)) / rate
    since = np.maximum(0.0, t - CLACK_AT)
    knock = np.sin(2 * math.pi * np.cumsum(150.0 + 60.0 * np.exp(-since / 0.01)) / rate) * np.where(t < CLACK_AT, 0.0, np.minimum(1.0, since / 0.0015)) * np.exp(-since / 0.018)
    rng = np.random.default_rng(1616)
    thud = biquad(biquad(rng.uniform(-1, 1, len(cut)), "low", rate, 400.0), "low", rate, 400.0) * 6.0
    thud *= np.where(t < CLACK_AT, 0.0, np.exp(-since / 0.012))
    clack = cut + knock * 0.45 + thud * 0.25
    save("SW_PickupClack", shaped(clack, rate, CLACK_KEEP, 0.0005, 0.03, 0.7), rate)

    rng16 = np.random.default_rng(1616)
    # The Roquefort's report: each rifle's blast from just before it, taken to 48 kHz mono; under its crack a thump (a tone
    # from 110 Hz down to 50 over 120 ms, in over 4 ms) and a rumble of low noise that rolls on for over a second; after
    # the recording's own echoes, two more, far and dull (low-passed copies 0.31 and 0.67 s on); 2.8 s, faded over 0.9 s.
    for number, name in enumerate(RIFLES, 1):
        x, rate = mono(os.path.join(FIREARMS, name))
        peak = np.abs(x).max()
        start = max(0, int(np.argmax(np.abs(x) >= peak * 0.4)) - int(0.015 * rate))
        keep = 2.8
        blast = to_rate(x[start:start + int(keep * rate)], rate, RATE)
        blast = blast / (np.abs(blast).max() or 1.0)
        count = len(blast)
        t = np.arange(count) / RATE
        at = 0.015
        since = np.maximum(0.0, t - at)
        on = np.where(t < at, 0.0, 1.0)
        phase = np.cumsum(2 * math.pi * (50.0 + 60.0 * np.exp(-since / 0.04)) / RATE)
        thump = np.sin(phase) * on * np.minimum(1.0, since / 0.004) * np.exp(-since / 0.09)
        rumble = biquad(biquad(rng16.uniform(-1, 1, count), "low", RATE, 120.0), "low", RATE, 120.0) * 9.0
        rumble *= on * np.minimum(1.0, since / 0.02) * np.exp(-since / 0.7)
        dull = biquad(biquad(blast, "low", RATE, 900.0), "low", RATE, 900.0)
        echoes = np.zeros(count)
        for delay, share in ((0.31, 0.32), (0.67, 0.2)):
            k = int(delay * RATE)
            echoes[k:] += dull[:count - k] * share
        report = np.tanh(1.2 * (blast + thump * 0.42 + rumble * 0.14 + echoes))
        save(f"SW_RoquefortShot{number}", shaped(report, RATE, keep, 0.001, 0.9, 0.95), RATE)

    # The Cheese Stick's pound: a body tone falling from 75 Hz to 36 over a fifth of a second, a knock of noise under 260 Hz
    # at its front, a dull slap of mid noise (400 Hz to 2 kHz) for its first 15 ms, and a smaller second knock 70 ms on
    # (the blow's bounce), driven into a little saturation; 0.6 s.
    count = int(0.6 * RATE)
    t = np.arange(count) / RATE
    phase = np.cumsum(2 * math.pi * (36.0 + 39.0 * np.exp(-t / 0.07)) / RATE)
    tone = np.sin(phase) * np.minimum(1.0, t / 0.003) * np.exp(-t / 0.16)
    knock = biquad(biquad(rng16.uniform(-1, 1, count), "low", RATE, 260.0), "low", RATE, 260.0) * 7.0 * np.exp(-t / 0.045)
    slap = biquad(biquad(rng16.uniform(-1, 1, count), "high", RATE, 400.0), "low", RATE, 2000.0) * 1.5 * np.exp(-t / 0.008)
    later = np.maximum(0.0, t - 0.07)
    bounce = biquad(biquad(rng16.uniform(-1, 1, count), "low", RATE, 200.0), "low", RATE, 200.0) * 3.0 * np.where(t < 0.07, 0.0, np.exp(-later / 0.03))
    pound = np.tanh(1.8 * (tone * 1.0 + knock * 0.8 + slap + bounce * 0.5))
    # Over the saturated low end, the blow's crunch, heard on small speakers too: noise of 250 Hz to 1.2 kHz for 25 ms.
    crunch = biquad(biquad(rng16.uniform(-1, 1, count), "high", RATE, 250.0), "low", RATE, 1200.0) * np.minimum(1.0, t / 0.001) * np.exp(-t / 0.025)
    pound = pound + crunch * 1.1
    save("SW_StickPound", shaped(pound, RATE, 0.6, 0.0005, 0.15, 0.95), RATE)

    # The female grunts: AuraVoice's seven, then the starter pack's three.
    groans, rate = mono(FEMALE_GROANS)
    takes = [(groans[int(a * rate):int(b * rate)], rate) for a, b in GROANS]
    for number in range(1, 4):
        takes.append(mono(os.path.join(STARTER, f"damaged{number}.wav")))
    for number, (x, rate) in enumerate(takes, 1):
        x = from_onset(x, rate)
        x = biquad(biquad(x, "low", rate, 5000.0), "high", rate, 120.0)
        save(f"SW_OofFemale{number}", shaped(x, rate, 0.45, 0.002, 0.12, 0.8), rate)


def import_sounds():
    import unreal
    out = unreal.Paths.project_saved_dir() + "wave16_sounds.txt"
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
