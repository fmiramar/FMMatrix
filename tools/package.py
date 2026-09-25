#!/usr/bin/env python3
"""Package a CMake-staged Extensions tree without machine-derived names."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("--stage", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--owner", required=True)
parser.add_argument("--version", required=True)
parser.add_argument("--platform", choices=["macos", "linux", "windows"], required=True)
parser.add_argument("--architecture", choices=["x64", "arm64"], required=True)
args = parser.parse_args()
for label in (args.owner, args.version):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", label):
        parser.error("owner and version must be filename-safe identifiers")
extension = args.stage / "Extensions" / "FMMatrixUGens"
if not (extension / "Classes" / "FMMatrixUGens.sc").is_file():
    parser.error("stage must contain Extensions/FMMatrixUGens/Classes/FMMatrixUGens.sc")
if not any(p.suffix in {".scx", ".so", ".dll"} for p in extension.rglob("*")):
    parser.error("staged server binary is missing")
args.output.mkdir(parents=True, exist_ok=True)
name = f"{args.owner}-FMMatrixUGens-{args.version}-{args.platform}-{args.architecture}.zip"
archive_path = args.output / name
metadata = {"project": "FMMatrixUGens", "version": args.version, "owner": args.owner,
            "platform": args.platform, "architecture": args.architecture}
with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(extension.rglob("*")):
        if file.is_symlink():
            parser.error(f"unexpected staged symlink: {file.name}")
        if file.is_file() and file.name != ".DS_Store":
            archive.write(file, file.relative_to(args.stage).as_posix())
    archive.writestr("Extensions/FMMatrixUGens/build-info.json", json.dumps(metadata, indent=2) + "\n")
digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
(args.output / f"{name}.sha256").write_text(f"{digest}  {name}\n")
print(json.dumps({"archive": name, "sha256": digest}))
