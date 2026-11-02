#!/usr/bin/env python3
"""Refresh checksums after deliberately updating the archived audit evidence."""

import csv
import hashlib
import json
from pathlib import Path


root = Path(__file__).resolve().parent.parent
origins = json.loads((root / "origins.json").read_text())
excluded = {"inventory.tsv", "SHA256SUMS"}
files = sorted(p for p in root.rglob("*") if p.is_file() and p.name not in excluded)
with (root / "inventory.tsv").open("w", newline="") as stream:
    writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
    writer.writerow(["path", "bytes", "sha256", "category", "preservation", "origin"])
    for path in files:
        relative = path.relative_to(root).as_posix()
        with path.open("rb") as content:
            checksum = hashlib.file_digest(content, "sha256").hexdigest()
        preservation, origin = origins.get(
            relative, ["authored-or-captured", "Evidence preservation/reproduction"]
        )
        writer.writerow([relative, path.stat().st_size, checksum,
                         relative.split("/")[0], preservation, origin])
files.append(root / "inventory.tsv")
with (root / "SHA256SUMS").open("w") as stream:
    for path in sorted(files):
        with path.open("rb") as content:
            checksum = hashlib.file_digest(content, "sha256").hexdigest()
        stream.write(f"{checksum}  {path.relative_to(root).as_posix()}\n")
print(f"Indexed {len(files)} files; SHA256SUMS covers every other evidence file.")
