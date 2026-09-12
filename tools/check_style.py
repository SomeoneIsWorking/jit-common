#!/usr/bin/env python3
"""Gate clang-format and clang-tidy over this repository's own C++ sources.

DESIGN THE NEGATIVE FIRST. The failure this guards against is not "a file is
misformatted" -- that one announces itself. It is the gate that checks nothing
and reports success: clang-tidy absent from PATH, a compile database that was
never generated, a glob that matched no files after a directory moved. Each of
those makes a green result mean "I looked at nothing", which is indistinguishable
from "everything passed" unless the count is printed. So this prints the
denominator on every run and REFUSES when it is zero or when either tool is
missing, rather than passing quietly.

The shared C++ policy tool audits the tracked tool configurations and checks
ownership syntax in Clang's parsed AST using this build's compile database.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CPP_POLICY_CANDIDATES = (
    REPO.parent / "re-harness" / "tools" / "cpp_policy.py",
    REPO / "build" / "deps" / "re-harness" / "tools" / "cpp_policy.py",
)


def shared_cpp_policy() -> Path:
    for candidate in CPP_POLICY_CANDIDATES:
        if candidate.is_file():
            return candidate
    tried = ", ".join(str(path) for path in CPP_POLICY_CANDIDATES)
    sys.exit(f"REFUSED: shared C++ policy tool is missing; tried {tried}")


def tracked_sources() -> list[Path]:
    out = subprocess.run(
        ["git", "ls-files", "*.cpp", "*.h"],
        cwd=REPO,
        capture_output=True,
        text=True,
        check=True,
    )
    return [REPO / line for line in out.stdout.split() if line]


def require(tool: str) -> str:
    path = shutil.which(tool)
    if path is None:
        sys.exit(
            f"REFUSED: {tool} is not on PATH. This gate cannot report a pass it "
            f"did not check. Install it (sudo dnf install clang-tools-extra) and "
            f"re-run."
        )
    return path


def check_format(files: list[Path]) -> int:
    tool = require("clang-format")
    bad = 0
    for f in files:
        r = subprocess.run(
            [tool, "--dry-run", "-Werror", str(f)], capture_output=True, text=True
        )
        if r.returncode != 0:
            bad += 1
            sys.stderr.write(r.stderr)
    print(f"clang-format: {len(files) - bad}/{len(files)} file(s) conformant")
    return bad


def check_tidy(build: Path) -> int:
    tool = require("clang-tidy")
    db = build / "compile_commands.json"
    if not db.is_file():
        sys.exit(
            f"REFUSED: no compile database at {db}. clang-tidy would silently "
            f"check nothing. Configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON."
        )
    units = sorted((REPO / "src").rglob("*.cpp")) + sorted(
        (REPO / "tests").rglob("*.cpp")
    )
    if not units:
        sys.exit("REFUSED: no translation units found to lint.")
    bad = 0
    for u in units:
        r = subprocess.run(
            [tool, "-p", str(build), "--header-filter=.*jitcommon.*", str(u)],
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            bad += 1
            sys.stderr.write(r.stdout + r.stderr)
    print(f"clang-tidy: {len(units) - bad}/{len(units)} translation unit(s) clean")
    return bad


def check_cpp_policy(build: Path) -> int:
    cpp_policy = shared_cpp_policy()
    commands = (
        [sys.executable, str(cpp_policy), "--audit-config", str(REPO)],
        [
            sys.executable,
            str(cpp_policy),
            "--compile-commands",
            str(build / "compile_commands.json"),
            "--root",
            str(REPO),
        ],
    )
    failures = 0
    for command in commands:
        result = subprocess.run(command, capture_output=True, text=True)
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        if result.returncode:
            failures += 1
    return failures


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=str(REPO / "build"), help="CMake build dir")
    args = ap.parse_args()

    files = tracked_sources()
    if not files:
        sys.exit(
            "REFUSED: git ls-files matched no C++ sources. A gate over zero "
            "files is not a passing gate."
        )

    build = Path(args.build).resolve()
    failures = check_format(files) + check_tidy(build) + check_cpp_policy(build)
    if failures:
        print(f"STYLE GATE FAILED: {failures} finding(s)", file=sys.stderr)
        return 1
    print("style gate passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
