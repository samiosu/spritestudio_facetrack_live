#!/usr/bin/env python3
"""Check that every recorded license text exists and retains its audited hash."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    manifest = json.loads((ROOT / "licenses/manifest.json").read_text())
    errors = []
    materials = manifest["licensed_materials"]
    for item in materials:
        path = ROOT / "licenses" / item["file"]
        if not path.resolve().is_relative_to((ROOT / "licenses").resolve()) or not path.is_file():
            errors.append(f"Missing/invalid license path: {item['file']}")
            continue
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != item["license_file_sha256"]:
            errors.append(f"License content changed: {item['file']}")
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"Verified {len(materials)} retained license/notice files. This is not a complete binary SBOM audit.")

if __name__ == "__main__":
    main()
