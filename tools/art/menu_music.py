import array, os, traceback, wave
import unreal

# Run with the editor closed:
#   UnrealEditor-Cmd.exe <repo>/CheeseStrike.uproject -ExecCmds="py <repo>/tools/art/menu_music.py" -unattended -nullrhi
# (both paths absolute). It writes what it did to Saved/menu_music.txt.
#
# The music of the front end and the lobby: "Bravery Beats" by MintoDog (OpenGameArt.org, CC0), an
# electric guitar track at 172 beats a minute, downloaded as its FLAC into MUSIC (outside the
# repository; Content/LICENSES.md has its page). The editor reads the FLAC (Python can't), and its
# sound is written out as a WAV; a whole number of bars of it (FROM_BAR, BARS) is cut out as the
# loop and imported, looping, as /Game/OpenGameArt/Music/SW_MenuMusic. The track is made to loop,
# its 65 bars exactly (90.7 s), its end leading back into its start, which begins from silence: the
# whole of it is the loop, its last milliseconds faded down to that silence so that the seam doesn't
# click. A loop that starts later blends its end into what comes just before its first bar instead.
#
# With CHEESESTRIKE_MUSIC_SURVEY=1 it only lists how loud each bar is, to choose the loop by.
MUSIC = os.environ.get("CHEESESTRIKE_MUSIC", r"C:\Hamid\code\FabDownloads\Music\bravery_beats")
SOURCE = os.path.join(MUSIC, "bravery_beats_bpm172.flac")
WHOLE = os.path.join(MUSIC, "bravery_beats_whole.wav")
LOOP = os.path.join(MUSIC, "bravery_beats_loop.wav")
SURVEY = os.environ.get("CHEESESTRIKE_MUSIC_SURVEY") == "1"
OUT = unreal.Paths.project_saved_dir() + "menu_music.txt"
DESTINATION = "/Game/OpenGameArt/Music"
NAME = "SW_MenuMusic"
BPM = 172.0
# The loop: these bars of the track (the first is bar 0), and how long the seam's blend is.
FROM_BAR = int(os.environ.get("CHEESESTRIKE_MUSIC_FROM", "0"))
BARS = int(os.environ.get("CHEESESTRIKE_MUSIC_BARS", "65"))
SEAM_SECONDS = 0.03
FADE_SECONDS = 0.003

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


def read(path):
    with wave.open(path, "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        data = array.array("h", w.readframes(frames))
    if width != 2:
        raise RuntimeError(f"{path}: {width * 8}-bit, not 16")
    return channels, rate, data


def bar_frames(rate):
    return 4.0 * 60.0 / BPM * rate


def survey(channels, rate, data):
    per_bar = bar_frames(rate)
    bars = int(len(data) / channels / per_bar + 0.5)
    for bar in range(bars):
        a = int(bar * per_bar) * channels
        b = min(len(data), int((bar + 1) * per_bar) * channels)
        chunk = data[a:b]
        rms = (sum(s * s for s in chunk[::16]) / max(1, len(chunk[::16]))) ** 0.5
        lines.append(f"  bar {bar:2d} at {a / channels / rate:6.2f} s: rms {rms:7.0f}")


def cut(channels, rate, data):
    per_bar = bar_frames(rate)
    start = int(round(FROM_BAR * per_bar))
    total = len(data) // channels
    end = min(total, int(round((FROM_BAR + BARS) * per_bar)))
    if (FROM_BAR + BARS) * per_bar > total + 1:
        raise RuntimeError(f"the loop ends at frame {end}, after the track's {total}")
    loop = array.array("h", data[start * channels:end * channels])
    frames = end - start
    if start == 0:
        fade = int(FADE_SECONDS * rate)
        for i in range(fade):
            at = frames - fade + i
            for c in range(channels):
                loop[at * channels + c] = int(round(loop[at * channels + c] * (fade - 1 - i) / fade))
        seam = 0
    else:
        seam = int(SEAM_SECONDS * rate)
        if start < seam:
            raise RuntimeError("the loop starts too early for the seam's blend")
    # The last frames fade from the track's own into what came just before the loop's start, which is what plays
    # into its start: so the end leads into the beginning as the track itself would have.
    for i in range(seam):
        t = (i + 1) / seam
        at = frames - seam + i
        before = start - seam + i
        for c in range(channels):
            mixed = loop[at * channels + c] * (1.0 - t) + data[before * channels + c] * t
            loop[at * channels + c] = max(-32768, min(32767, int(round(mixed))))
    with wave.open(LOOP, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(loop.tobytes())
    lines.append(f"loop: bars {FROM_BAR} to {FROM_BAR + BARS}, {frames / rate:.2f} s, from {start / rate:.2f} s")


try:
    source = [a for a in import_file(SOURCE, "/Game/Temp", "SW_MusicSource") if isinstance(a, unreal.SoundWave)]
    if not source:
        raise RuntimeError(f"nothing imported from {SOURCE}")
    lines.append(f"source: {source[0].get_editor_property('duration'):.2f} s; exported {export_wav(source[0], WHOLE)}")
    lib.delete_asset("/Game/Temp/SW_MusicSource")
    channels, rate, data = read(WHOLE)
    lines.append(f"{channels} channels at {rate} Hz, {len(data) // channels} frames")
    if SURVEY:
        survey(channels, rate, data)
    else:
        cut(channels, rate, data)
        music = [a for a in import_file(LOOP, DESTINATION, NAME) if isinstance(a, unreal.SoundWave)]
        if not music:
            raise RuntimeError(f"nothing imported from {LOOP}")
        music[0].set_editor_property("looping", True)
        lib.save_asset(music[0].get_path_name(), False)
        lines.append(f"{NAME}: {music[0].get_editor_property('duration'):.2f} s, looping")
except Exception:
    lines.append(traceback.format_exc())
finally:
    open(OUT, "w").write("\n".join(lines))
    unreal.SystemLibrary.quit_editor()
