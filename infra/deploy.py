"""Deploys CheeseStrike to AWS: the backend in infra/template.yaml, and the
game's web build to the bucket that template makes.

    python infra/deploy.py            both, backend first
    python infra/deploy.py backend    only the backend
    python infra/deploy.py game       only the game

It acts on whatever AWS account the AWS CLI is signed in to (`aws configure`),
in REGION, under the stack name STACK. Deploying again updates what is there.

The backend needs the SAM CLI (`sam`). The game needs Godot with the Web
export templates; it is exported fresh each time, to build/deploy.

Uploading the game does three things a plain copy wouldn't:

  - It writes the backend's addresses into index.html, which is how the game
    finds its session server (game/net/backend_urls.gd).
  - It compresses the large files before uploading. The engine is tens of
    megabytes and S3 serves files exactly as stored, so this is what makes
    the game quick to load and cheap to serve.
  - It tells browsers to check for a newer version on every visit, so players
    are never left on an old build after a deploy.
"""

import argparse
import gzip
import json
import mimetypes
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "infra" / "template.yaml"
BUILD = ROOT / "build" / "deploy"
STACK = "cheesestrike"
REGION = "us-east-2"

# File types worth compressing. Images and sounds are compressed already.
COMPRESSED = {".html", ".js", ".wasm", ".pck", ".json", ".svg", ".css", ".txt"}
CONTENT_TYPES = {".wasm": "application/wasm", ".pck": "application/octet-stream", ".js": "text/javascript"}


def with_config(html, signal_url, sessions_url):
    """`html` with the backend's addresses written into its head, where the
    game reads them as `window.cheeseStrikeConfig`."""
    if "</head>" not in html:
        raise ValueError("the page has no </head> to write the backend's addresses before")
    config = json.dumps({"signal": signal_url, "sessions": sessions_url})
    # Kept from ending the script early, whatever the addresses hold.
    config = config.replace("<", "\\u003c")
    return html.replace("</head>", f"<script>window.cheeseStrikeConfig = {config};</script>\n</head>", 1)


def content_type(path):
    return CONTENT_TYPES.get(path.suffix) or mimetypes.guess_type(path.name)[0] or "application/octet-stream"


def upload(s3, bucket, folder, signal_url, sessions_url):
    """Uploads the web build in `folder` to `bucket`. Returns the keys sent."""
    sent = []
    for path in sorted(folder.rglob("*")):
        if not path.is_file():
            continue
        key = path.relative_to(folder).as_posix()
        body = path.read_bytes()
        if key == "index.html":
            body = with_config(body.decode("utf-8"), signal_url, sessions_url).encode("utf-8")
        extra = {"ContentType": content_type(path), "CacheControl": "no-cache"}
        if path.suffix in COMPRESSED:
            body = gzip.compress(body, mtime=0)
            extra["ContentEncoding"] = "gzip"
        s3.put_object(Bucket=bucket, Key=key, Body=body, **extra)
        sent.append(key)
    if "index.html" not in sent:
        raise ValueError(f"no index.html in {folder}; nothing playable was uploaded")
    return sent


def stack_outputs(cloudformation, stack):
    """What the deployed backend reports about itself: its addresses and bucket."""
    described = cloudformation.describe_stacks(StackName=stack)["Stacks"][0]
    return {output["OutputKey"]: output["OutputValue"] for output in described.get("Outputs", [])}


def run(*command):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def find(*names):
    for name in names:
        if shutil.which(name):
            return shutil.which(name)
    sys.exit(f"{names[-1]} not found on the PATH.")


def deploy_backend(stack, region):
    sam = find("sam")
    run(sam, "deploy", "--template-file", TEMPLATE, "--stack-name", stack, "--region", region, "--resolve-s3", "--capabilities", "CAPABILITY_IAM", "--no-fail-on-empty-changeset")


def deploy_game(stack, region):
    import boto3

    outputs = stack_outputs(boto3.client("cloudformation", region_name=region), stack)
    godot = find("godot_console", "godot")
    shutil.rmtree(BUILD, ignore_errors=True)
    BUILD.mkdir(parents=True)
    run(godot, "--headless", "--path", ROOT / "game", "--import")
    run(godot, "--headless", "--path", ROOT / "game", "--export-release", "Web", BUILD / "index.html")
    sent = upload(boto3.client("s3", region_name=region), outputs["GameBucket"], BUILD, outputs["SignalUrl"], outputs["SessionsUrl"])
    print(f"uploaded {len(sent)} files")
    print(f"play at {outputs['GameUrl']}")


def main():
    parser = argparse.ArgumentParser(description="Deploy CheeseStrike to AWS.")
    parser.add_argument("what", nargs="*", choices=["backend", "game"], help="default: both")
    parser.add_argument("--stack", default=STACK)
    parser.add_argument("--region", default=REGION)
    args = parser.parse_args()
    what = args.what or ["backend", "game"]
    if "backend" in what:
        deploy_backend(args.stack, args.region)
    if "game" in what:
        deploy_game(args.stack, args.region)


if __name__ == "__main__":
    main()
