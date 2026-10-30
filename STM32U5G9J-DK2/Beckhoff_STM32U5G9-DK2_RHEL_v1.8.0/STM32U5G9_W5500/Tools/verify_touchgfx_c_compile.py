#!/usr/bin/env python3
"""Dry-run the TouchGFX simulator C compile rule and verify language-safe flags.

@author Prathamesh Chipkar
@version 2.0.0-alpha.9.0
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]).resolve()
tg = root / "TouchGFX"
make = shutil.which("make")
if not make:
    print("FAIL: GNU make was not found in PATH", file=sys.stderr)
    raise SystemExit(1)

env = os.environ.copy()
env["OS"] = "Windows_NT"
proc = subprocess.run(
    [make, "-f", "simulator/gcc/Makefile", "-n", "all", "TOUCHGFX_VERIFY_ONLY_SKIP_ASSET_CHECK=1"],
    cwd=tg,
    env=env,
    text=True,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    check=False,
)
out = proc.stdout
# Extract the command immediately following the io_config.c compile announcement.
m = re.search(r"Compiling \.\./Application/Config/Src/io_config\.c\s+mkdir[^\n]*\n(?P<cmd>gcc \\\n(?:.*\\\n)*?.*-c \.\./Application/Config/Src/io_config\.c[^\n]*)", out)
if not m:
    print(out[-5000:], file=sys.stderr)
    print("FAIL: could not resolve io_config.c simulator compile command", file=sys.stderr)
    raise SystemExit(1)
cmd = m.group("cmd")
cpp = re.search(r"Compiling gui/src/screen1_screen/Screen1View\.cpp\s+mkdir[^\n]*\n(?P<cmd>g\+\+ \\\n(?:.*\\\n)*?.*-c gui/src/screen1_screen/Screen1View\.cpp[^\n]*)", out)
cpp_cmd = cpp.group("cmd") if cpp else ""
checks = [
    ("C compiler is gcc", cmd.lstrip().startswith("gcc ")),
    ("C++ overloaded-virtual warning absent from C command", "overloaded-virtual" not in cmd),
    ("C warnings-as-errors retained", "-Werror" in cmd),
    ("C pedantic checking retained", "-pedantic" in cmd),
    ("io_config.c is compiled as C source", "-c ../Application/Config/Src/io_config.c" in cmd),
    ("TouchGFX C++ compiler remains g++", bool(cpp_cmd) and cpp_cmd.lstrip().startswith("g++ ")),
    ("TouchGFX C++ warning policy retains overloaded-virtual", "overloaded-virtual" in cpp_cmd),
]
failed = False
for label, ok in checks:
    print(("PASS" if ok else "FAIL") + ": " + label)
    failed |= not ok
if failed:
    print("Resolved command:\n" + cmd, file=sys.stderr)
    raise SystemExit(1)
print("TOUCHGFX C COMPILE RULE: PASS")
