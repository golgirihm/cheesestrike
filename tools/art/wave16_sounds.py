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
#
# Two steps, from the repository's root:
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
SOUNDS = [("SW_PickupClack.wav", "/Game/OpenGameArt/Clicks", "SW_PickupClack")] \
    + [(f"SW_OofFemale{i}.wav", "/Game/OpenGameArt/HurtFemale", f"SW_OofFemale{i}") for i in range(1, 11)]


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
