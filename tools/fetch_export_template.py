"""Installs the one Godot export template the browser tests need: the Web
debug template, which CI uses to export the game for them.

Godot publishes its export templates as a single archive of about a gigabyte
covering every platform. This reads only the Web template's ten megabytes out
of it, by asking the server for just those parts of the file, and checks the
result against the SHA-256 pinned in tools/versions.env.

    python tools/fetch_export_template.py

A machine with the export templates installed through the Godot editor has no
need of this.
"""

import hashlib
import io
import os
import pathlib
import sys
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE = "web_nothreads_debug.zip"


def pinned_versions():
    pins = {}
    for line in (ROOT / "tools" / "versions.env").read_text(encoding="utf-8-sig").splitlines():
        name, separator, value = line.strip().partition("=")
        if separator and not name.startswith("#"):
            pins[name] = value
    return pins


def templates_folder(version):
    """Where Godot looks for the export templates of `version`."""
    if sys.platform == "win32":
        base = pathlib.Path(os.environ["APPDATA"]) / "Godot"
    elif sys.platform == "darwin":
        base = pathlib.Path.home() / "Library" / "Application Support" / "Godot"
    else:
        base = pathlib.Path(os.environ.get("XDG_DATA_HOME", pathlib.Path.home() / ".local" / "share")) / "godot"
    return base / "export_templates" / f"{version}.stable"


class RemoteFile(io.RawIOBase):
    """A file on a web server, read a requested range at a time, so that
    zipfile can pick one entry out of an archive without downloading it all."""

    def __init__(self, url):
        self._url = url
        self._position = 0
        with self._request("bytes=0-0") as response:
            self._size = int(response.headers["Content-Range"].rpartition("/")[2])

    def _request(self, byte_range):
        return urllib.request.urlopen(urllib.request.Request(self._url, headers={"Range": byte_range}))

    def readable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self._position

    def seek(self, offset, whence=io.SEEK_SET):
        start = {io.SEEK_SET: 0, io.SEEK_CUR: self._position, io.SEEK_END: self._size}[whence]
        self._position = start + offset
        return self._position

    def readinto(self, buffer):
        last = min(self._position + len(buffer), self._size) - 1
        if last < self._position:
            return 0
        with self._request(f"bytes={self._position}-{last}") as response:
            data = response.read()
        buffer[: len(data)] = data
        self._position += len(data)
        return len(data)


def main():
    pins = pinned_versions()
    version = pins["GODOT_VERSION"]
    url = f"https://github.com/godotengine/godot/releases/download/{version}-stable/Godot_v{version}-stable_export_templates.tpz"
    # Buffered in large pieces so that the template arrives in a few requests.
    with zipfile.ZipFile(io.BufferedReader(RemoteFile(url), buffer_size=4 * 1024 * 1024)) as archive:
        data = archive.read(f"templates/{TEMPLATE}")

    found = hashlib.sha256(data).hexdigest()
    if found != pins["GODOT_WEB_TEMPLATE_SHA256"]:
        print(f"error: {TEMPLATE} has SHA-256 {found}, not the pinned {pins['GODOT_WEB_TEMPLATE_SHA256']}")
        return 1

    folder = templates_folder(version)
    folder.mkdir(parents=True, exist_ok=True)
    (folder / TEMPLATE).write_bytes(data)
    print(f"installed {TEMPLATE} ({len(data) // 1024} KiB) to {folder}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
