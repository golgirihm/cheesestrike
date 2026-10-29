import array, math, os, sys, traceback, wave

# Recorded sounds in place of synthesised ones (wave 14), from files kept outside the repository
# (Content/LICENSES.md has their sources; both CC0, from OpenGameArt.org):
#
# Footsteps (/Game/OpenGameArt/Footsteps/SW_FootstepWalk1..6 and SW_FootstepSprint1..6): "Footsteps" by
#   GboxMikeFozzy, six steps of shoes on a subway's concrete, 48 kHz mono Ogg Vorbis, in
#   C:\Hamid\code\FabDownloads\oga_footsteps_subway. Each is cut from a moment before its heel, the rumble under
#   60 Hz taken off, faded out and made as loud as the others; the sprint's are the same steps quicker and harder
#   (played a little faster, cut shorter, pushed), heard louder by the game (CheeseStrikeFootsteps::Volume).
#
# The hurt "oof" (/Game/OpenGameArt/Hurt/SW_Oof1..5): "Hurt Sound Effects" by EZduzziteh, short breathy grunts,
#   44.1 kHz stereo MP3, in C:\Hamid\code\FabDownloads\oga_hurt_ezduzziteh; taken to mono, a little deeper, the
#   highs softened, faded and made alike in loudness.
#
# Two steps, from the repository's root:
#   1. C:\Hamid\code\FabDownloads\piper\venv\Scripts\python.exe -I tools/art/wave14_sounds.py prepare
#      (any Python with numpy and soundfile, which read Ogg and MP3) writes the WAVs to C:\Hamid\code\FabDownloads\wave14_prepared.
#   2. With the editor closed:
#      UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/wave14_sounds.py" -unattended -nullrhi
#      imports them; it writes what it did to Saved/wave14_sounds.txt.

DOWNLOADS = os.environ.get("CHEESESTRIKE_DOWNLOADS", r"C:\Hamid\code\FabDownloads")
PREPARED = os.path.join(DOWNLOADS, "wave14_prepared")
STEPS = ["01-footstep_0.ogg", "02-footstep.ogg", "03-footstep.ogg", "04-footstep.ogg", "05-footstep.ogg", "06-footstep.ogg"]
OOFS = ["hurt_01_0.mp3", "hurt_02.mp3", "hurt_03.mp3", "hurt_04.mp3", "hurt_06.mp3"]
# (prepared file, destination folder, asset name)
SOUNDS = [(f"SW_FootstepWalk{i}.wav", "/Game/OpenGameArt/Footsteps", f"SW_FootstepWalk{i}") for i in range(1, 7)] \
    + [(f"SW_FootstepSprint{i}.wav", "/Game/OpenGameArt/Footsteps", f"SW_FootstepSprint{i}") for i in range(1, 7)] \
    + [(f"SW_Oof{i}.wav", "/Game/OpenGameArt/Hurt", f"SW_Oof{i}") for i in range(1, 6)]


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
        w0 = 2 * math.pi * freq / rate
        alpha = math.sin(w0) / (2 * q)
        cs = math.cos(w0)
        if kind == "low":
            b = [(1 - cs) / 2, 1 - cs, (1 - cs) / 2]
        else:
            b = [(1 + cs) / 2, -(1 + cs), (1 + cs) / 2]
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

    def faster(x, ratio):
        n = int(len(x) / ratio)
        return np.interp(np.arange(n) * ratio, np.arange(len(x)), x)

    def from_onset(x, rate, share=0.25, lead=0.004):
        peak = np.abs(x).max()
        first = int(np.argmax(np.abs(x) >= peak * share))
        return x[max(0, first - int(lead * rate)):]

    def shaped(x, rate, keep, fade_out, peak):
        x = x[:int(keep * rate)].copy()
        n_in = max(1, int(0.002 * rate))
        x[:n_in] *= np.linspace(0, 1, n_in)
        n_out = min(len(x), int(fade_out * rate))
        x[len(x) - n_out:] *= np.linspace(1, 0, n_out) ** 2
        return x * peak / (np.abs(x).max() or 1.0)

    os.makedirs(PREPARED, exist_ok=True)
    for i, name in enumerate(STEPS, 1):
        x, rate = mono(os.path.join(DOWNLOADS, "oga_footsteps_subway", name))
        x = biquad(from_onset(x, rate), "high", rate, 60.0)
        walk = shaped(x, rate, 0.26, 0.12, 0.9)
        write(walk, rate, os.path.join(PREPARED, f"SW_FootstepWalk{i}.wav"))
        # The sprint's: the same heel landing quicker and harder.
        hard = np.tanh(1.8 * faster(x, 1.08) / (np.abs(x).max() or 1.0))
        sprint = shaped(hard, rate, 0.17, 0.07, 0.95)
        write(sprint, rate, os.path.join(PREPARED, f"SW_FootstepSprint{i}.wav"))
        print(name, f"walk {len(walk) / rate:.2f} s, sprint {len(sprint) / rate:.2f} s")
    for i, name in enumerate(OOFS, 1):
        x, rate = mono(os.path.join(DOWNLOADS, "oga_hurt_ezduzziteh", name))
        x = faster(from_onset(x, rate, 0.1, 0.01), 0.87)
        x = biquad(biquad(x, "low", rate, 3200.0), "high", rate, 90.0)
        oof = shaped(x, rate, 0.45, 0.12, 0.8)
        write(oof, rate, os.path.join(PREPARED, f"SW_Oof{i}.wav"))
        print(name, f"oof {len(oof) / rate:.2f} s")


def import_sounds():
    import unreal
    out = unreal.Paths.project_saved_dir() + "wave14_sounds.txt"
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
