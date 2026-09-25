#!/usr/bin/env python3
"""Run installed-extension checks with bounded subprocesses and saved logs."""
import argparse
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--sclang", default="sclang")
parser.add_argument("--live", action="store_true")
parser.add_argument("--supernova", action="store_true")
parser.add_argument("--fm7", action="store_true", help="Include optional comparison requiring sc3-plugins FM7")
parser.add_argument("--only", nargs="+", help="Run only these selected test names (retain --live/--supernova/--fm7 when needed)")
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
logs = project / "build" / "test-logs"
logs.mkdir(parents=True, exist_ok=True)
tests = [
    ("compile_check", "FMMATRIX_COMPILE_OK"),
    ("nrt_reference", "FMMATRIX_NRT_OK"),
    ("pm_reference", "PMMATRIX_NRT_OK"),
    ("dynamic_reference", "FMMATRIX_DYNAMIC_OK"),
    ("oversampling_reference", "FMMATRIX_OVERSAMPLING_OK"),
    ("zdf_reference", "FMMATRIX_ZDF_OK"),
    ("graph_reference", "FMMATRIX_GRAPH_OK"),
    ("extension_reference", "FMMATRIX_EXTENSIONS_OK"),
    ("verify_scdoc", "FMMATRIX_SCDOC_OK"),
]
if args.live:
    tests.append(("server_smoke", "FMMATRIX_SERVER_OK"))
    tests.append(("examples_smoke", "FMMATRIX_EXAMPLES_OK"))
if args.supernova:
    tests.append(("supernova_smoke", "FMMATRIX_SUPERNOVA_OK"))
if args.fm7:
    tests.append(("fm7_comparison", "FMMATRIX_FM7_OK"))
if args.only:
    unknown = set(args.only) - {name for name, _ in tests}
    if unknown:
        parser.error(f"Unavailable tests: {', '.join(sorted(unknown))}; enable their optional test group")
    tests = [(name, marker) for name, marker in tests if name in args.only]
for name, marker in tests:
    command = [args.sclang, "-D", str(project / "tests" / "sc" / f"{name}.scd")]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired as error:
        print(f"FAIL {name}: timed out", file=sys.stderr)
        sys.exit(1)
    output = result.stdout + result.stderr
    (logs / f"{name}.log").write_text(output)
    # Expected API errors are caught by the test without being reported.
    failed = result.returncode != 0 or marker not in output or "ERROR:" in output
    if name == "verify_scdoc":
        failed = failed or "WARNING:" in output or "Warning:" in output
    print(f"{'FAIL' if failed else 'PASS'} {name}", flush=True)
    if failed:
        print(output, file=sys.stderr)
        sys.exit(1)
