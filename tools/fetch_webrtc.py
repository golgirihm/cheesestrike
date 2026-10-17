"""Installs WebRTC for native builds: the webrtc-native GDExtension, into
game/addons/webrtc_native.

A browser gives the web build WebRTC, but Godot's desktop builds have none of
their own, so without this a native instance can't host or join. The release
holds libraries for every platform, about 37 megabytes; this keeps only the
desktop ones (Windows and Linux x86_64, and macOS), and checks the archive
against the version and SHA-256 pinned in tools/versions.env. The folder is
left out of git, so run this once after cloning and again when the pin
changes.

    python tools/fetch_webrtc.py

The extension leaves itself out of web exports, so it changes nothing there.
"""

import hashlib
import io
import pathlib
import shutil
import sys
import urllib.request
import zipfile

from fetch_export_template import pinned_versions

ROOT = pathlib.Path(__file__).resolve().parent.parent
ADDON = "addons/webrtc_native/"
FOLDER = ROOT / "game" / ADDON
DESKTOP = (".windows.template_debug.x86_64.", ".windows.template_release.x86_64.", ".linux.template_debug.x86_64.", ".linux.template_release.x86_64.", ".macos.")


def wanted(name):
    """Whether an entry of the release's archive is one to install."""
    if not name.startswith(ADDON) or name.endswith("/"):
        return False
    if "/lib/" not in name:
        return True  # the .gdextension file and the licenses
    return any(platform in name for platform in DESKTOP)


def main():
    pins = pinned_versions()
    version = pins["WEBRTC_NATIVE_VERSION"]
    url = f"https://github.com/godotengine/webrtc-native/releases/download/{version}-stable/godot-extension-webrtc_native.zip"
    print(f"downloading webrtc-native {version}", flush=True)
    with urllib.request.urlopen(url) as response:
        data = response.read()
    found = hashlib.sha256(data).hexdigest()
    if found != pins["WEBRTC_NATIVE_SHA256"]:
        print(f"error: the archive has SHA-256 {found}, not the pinned {pins['WEBRTC_NATIVE_SHA256']}")
        return 1

    shutil.rmtree(FOLDER, ignore_errors=True)
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        names = [name for name in archive.namelist() if wanted(name)]
        for name in names:
            target = ROOT / "game" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.read(name))
    print(f"installed {len(names)} files to {FOLDER.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
