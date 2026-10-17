"""Tests for what deploying does to the game's files, against moto's imitation
of S3 in this process. Nothing reaches AWS.

    python -m unittest discover -s infra
"""

import gzip
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

import boto3
from moto import mock_aws

import deploy

PAGE = "<html><head><title>CheeseStrike</title></head><body><canvas></canvas></body></html>"
SIGNAL = "wss://abc123.execute-api.us-east-2.amazonaws.com/prod"
SESSIONS = "https://xyz789.lambda-url.us-east-2.on.aws/"


class ConfigTest(unittest.TestCase):
    def test_the_addresses_are_written_into_the_head_as_the_game_reads_them(self):
        page = deploy.with_config(PAGE, SIGNAL, SESSIONS)
        written = re.search(r"<script>window\.cheeseStrikeConfig = (.*?);</script>\n</head>", page)
        self.assertIsNotNone(written, page)
        self.assertEqual(json.loads(written.group(1)), {"signal": SIGNAL, "sessions": SESSIONS})
        self.assertEqual(page.replace(written.group(0), "</head>"), PAGE, "nothing else changed")

    def test_an_address_cant_end_the_script_early(self):
        page = deploy.with_config(PAGE, "wss://x/</script><script>alert(1)</script>", SESSIONS)
        self.assertEqual(page.count("</script>"), 1)
        written = re.search(r"cheeseStrikeConfig = (.*?);</script>", page).group(1)
        self.assertEqual(json.loads(written)["signal"], "wss://x/</script><script>alert(1)</script>")

    def test_a_page_with_no_head_is_refused(self):
        with self.assertRaises(ValueError):
            deploy.with_config("<html><body></body></html>", SIGNAL, SESSIONS)


class FunctionTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.folder = pathlib.Path(folder.name) / "lambda"

    def test_only_the_functions_own_code_is_uploaded(self):
        names = deploy.build_function(self.folder)
        self.assertEqual(names, ["aws_lambda.py", "core.py", "dynamo_store.py"])

    def test_building_again_leaves_nothing_behind(self):
        self.folder.mkdir()
        (self.folder / "server.py").write_text("stale")
        self.assertNotIn("server.py", deploy.build_function(self.folder))

    def test_the_function_imports_with_nothing_but_its_own_code(self):
        deploy.build_function(self.folder)
        # Isolated from the repository, as on AWS; boto3 is in Lambda's runtime.
        script = f"import sys; sys.path.insert(0, {str(self.folder)!r}); import aws_lambda; print(aws_lambda.handler.__module__)"
        ran = subprocess.run([sys.executable, "-I", "-c", script], capture_output=True, text=True, cwd=self.folder)
        self.assertEqual(ran.returncode, 0, ran.stderr)
        self.assertEqual(ran.stdout.strip(), "aws_lambda")


class UploadTest(unittest.TestCase):
    def setUp(self):
        # Made-up credentials, so that nothing can reach a real account even
        # if the imitation were somehow bypassed.
        for name in ("AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY", "AWS_SESSION_TOKEN"):
            os.environ[name] = "testing"
        mock = mock_aws()
        mock.start()
        self.addCleanup(mock.stop)
        self.s3 = boto3.client("s3", region_name="us-east-2")
        self.s3.create_bucket(Bucket="game", CreateBucketConfiguration={"LocationConstraint": "us-east-2"})
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.folder = pathlib.Path(folder.name)
        self.files = {
            "index.html": PAGE.encode(),
            "index.js": b"console.log('engine');" * 100,
            "index.wasm": b"\0asm" + b"engine" * 5000,
            "index.pck": b"GDPC" + b"game data" * 5000,
            "index.icon.png": b"\x89PNG\r\n\x1a\n" + bytes(range(256)),
            "sounds/music.ogg": b"OggS" + bytes(range(256)),
        }
        for name, content in self.files.items():
            path = self.folder / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)

    def stored(self, key):
        return self.s3.get_object(Bucket="game", Key=key)

    def content(self, key):
        """What a browser ends up with for `key`."""
        stored = self.stored(key)
        body = stored["Body"].read()
        return gzip.decompress(body) if stored.get("ContentEncoding") == "gzip" else body

    def test_every_file_is_uploaded_under_its_own_path(self):
        sent = deploy.upload(self.s3, "game", self.folder, SIGNAL, SESSIONS)
        self.assertEqual(sorted(sent), sorted(self.files))
        for name, content in self.files.items():
            if name != "index.html":
                self.assertEqual(self.content(name), content, name)

    def test_the_page_is_uploaded_with_the_backends_addresses(self):
        deploy.upload(self.s3, "game", self.folder, SIGNAL, SESSIONS)
        self.assertEqual(self.content("index.html").decode(), deploy.with_config(PAGE, SIGNAL, SESSIONS))
        self.assertEqual((self.folder / "index.html").read_bytes(), PAGE.encode(), "the build itself is left alone")

    def test_large_text_and_engine_files_are_stored_compressed_and_labelled_so(self):
        deploy.upload(self.s3, "game", self.folder, SIGNAL, SESSIONS)
        for name in ("index.html", "index.js", "index.wasm", "index.pck"):
            stored = self.stored(name)
            self.assertEqual(stored.get("ContentEncoding"), "gzip", name)
        for name in ("index.js", "index.wasm", "index.pck"):
            self.assertLess(self.stored(name)["ContentLength"], len(self.files[name]) / 2, name)
        for name in ("index.icon.png", "sounds/music.ogg"):
            self.assertNotIn("ContentEncoding", self.stored(name), name)

    def test_each_file_says_what_it_is(self):
        deploy.upload(self.s3, "game", self.folder, SIGNAL, SESSIONS)
        kinds = {name: self.stored(name)["ContentType"] for name in self.files}
        self.assertEqual(kinds["index.html"], "text/html")
        self.assertEqual(kinds["index.js"], "text/javascript")
        # Browsers only compile the engine as it downloads if it is labelled this.
        self.assertEqual(kinds["index.wasm"], "application/wasm")
        self.assertEqual(kinds["index.pck"], "application/octet-stream")
        self.assertEqual(kinds["index.icon.png"], "image/png")

    def test_browsers_are_told_to_check_for_a_newer_version(self):
        deploy.upload(self.s3, "game", self.folder, SIGNAL, SESSIONS)
        for name in self.files:
            self.assertEqual(self.stored(name)["CacheControl"], "no-cache", name)

    def test_a_folder_with_no_page_is_refused(self):
        (self.folder / "index.html").unlink()
        with self.assertRaises(ValueError):
            deploy.upload(self.s3, "game", self.folder, SIGNAL, SESSIONS)

    def test_the_backends_addresses_are_read_from_the_stack(self):
        class Stacks:
            def describe_stacks(self, StackName):
                assert StackName == "cheesestrike"
                return {"Stacks": [{"Outputs": [{"OutputKey": "SignalUrl", "OutputValue": SIGNAL}, {"OutputKey": "GameBucket", "OutputValue": "game"}]}]}

        self.assertEqual(deploy.stack_outputs(Stacks(), "cheesestrike"), {"SignalUrl": SIGNAL, "GameBucket": "game"})


if __name__ == "__main__":
    unittest.main()
