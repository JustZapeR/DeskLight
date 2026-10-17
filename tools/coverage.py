#!/usr/bin/env python3
"""Aggregate host tests into one coverage run, using the compiler's gcov reader."""
import json
import os
import pathlib
import platform
import re
import shutil
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
build = root / "tests/coverage" / (platform.system() + "-" + platform.machine())
shutil.rmtree(build, ignore_errors=True)
build.mkdir(parents=True)
cc = os.environ.get("CC", "cc")
flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "--coverage"]
tests = sorted((root / "tests").glob("test_*.c"))
modules = [root / "firmware/Sensors/imu.c", root / "firmware/Desk/desk.c", root / "firmware/Desk/console.c"]
objects = []
entrypoints = []
for path in tests + modules:
    target = build / (path.stem + ".o")
    extra = []
    if path in tests:
        entry = path.stem + "_entry"
        entrypoints.append(entry)
        extra = ["-Dmain=" + entry]
    subprocess.run([cc, *flags, *extra, "-c", str(path), "-o", str(target)], check=True)
    objects.append(str(target))
runner = build / "runner.c"
runner.write_text("\n".join("int " + e + "(void);" for e in entrypoints) +
                  "\nint main(void) {\n" +
                  "\n".join("if (" + e + "()) return 1;" for e in entrypoints) + "\nreturn 0;\n}\n")
subprocess.run([cc, *flags, str(runner), *objects, "-lm", "-o", str(build / "all")], check=True)
subprocess.run([str(build / "all")], check=True)
reader = ["xcrun", "llvm-cov", "gcov"] if platform.system() == "Darwin" else ["gcov"]
report = {}
logs = []
for path in modules:
    result = subprocess.run([*reader, "-b", "-c", str(build / (path.stem + ".gcno"))],
                            cwd=build, capture_output=True, text=True, check=True)
    logs.append(result.stdout + result.stderr)
    match = re.search(r"Lines executed:([\d.]+)% of (\d+).*?Taken at least once:([\d.]+)% of (\d+)",
                      result.stdout, re.S)
    assert match, result.stdout
    lines, line_count, branches, branch_count = match.groups()
    report[str(path.relative_to(root))] = {
        "lines_percent": float(lines), "lines": int(line_count),
        "branch_taken_percent": float(branches), "branches": int(branch_count)}
    assert float(lines) >= 90 and float(branches) >= 80, report
out = root / "docs/validation" / ("coverage-" + platform.system() + "-" + platform.machine() + ".json")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2) + "\n")
(build / "gcov.txt").write_text("\n".join(logs))
print(json.dumps(report, indent=2))
print("PASS coverage: portable modules >=90% lines and >=80% branch directions")
