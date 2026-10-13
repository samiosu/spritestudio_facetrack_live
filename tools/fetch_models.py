#!/usr/bin/env python3
"""Fetch the Apache-2.0 Face Landmarker v1 bundle, verifying an immutable hash."""
import argparse
import hashlib
from pathlib import Path
import shutil
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
URL = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"
SHA256 = "64184e229b263107bc2b804c6625db1341ff2bb731874b0bcc2fe6544e0bc9ff"
DESTINATIONS = {
    "android": ROOT / "mobile/android/app/src/main/assets/face_landmarker.task",
    "ios": ROOT / "mobile/ios/Resources/face_landmarker.task",
}

def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("android", "ios", "all"), default="all")
    parser.add_argument("--source", type=Path, help="Use an already downloaded bundle; its hash is still verified")
    args = parser.parse_args()
    targets = list(DESTINATIONS.values()) if args.target == "all" else [DESTINATIONS[args.target]]
    if all(path.exists() and digest(path) == SHA256 for path in targets):
        print("Pinned model already present and verified.")
        return
    with tempfile.TemporaryDirectory(prefix="spriteface-model-") as temporary:
        bundle = Path(temporary) / "face_landmarker.task"
        if args.source:
            shutil.copyfile(args.source, bundle)
        else:
            request = urllib.request.Request(URL, headers={"User-Agent": "SpriteFace-model-fetch/0.1"})
            with urllib.request.urlopen(request, timeout=60) as response, bundle.open("wb") as output:
                total = 0
                while block := response.read(65536):
                    total += len(block)
                    if total > 16 * 1024 * 1024:
                        raise RuntimeError("Model exceeded the expected download size")
                    output.write(block)
        if digest(bundle) != SHA256:
            raise RuntimeError("Model SHA-256 mismatch; existing assets were not replaced")
        for path in targets:
            path.parent.mkdir(parents=True, exist_ok=True)
            # Replace atomically on each target's own filesystem.
            with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as output:
                staged = Path(output.name)
                with bundle.open("rb") as source:
                    shutil.copyfileobj(source, output)
            try:
                staged.replace(path)
            finally:
                staged.unlink(missing_ok=True)
            print(f"Verified model installed: {path.relative_to(ROOT)}")

if __name__ == "__main__":
    main()
