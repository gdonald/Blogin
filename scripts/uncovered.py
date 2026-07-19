#!/usr/bin/env python3
"""List the lines the spec suite never ran, per file.

`scripts/coverage.sh` reports the percentage. This says which lines are behind
it, which is what tells you whether one is a missing spec or a path no test can
reach.

Run `scripts/coverage.sh` first: this reads the profile it leaves behind, from
the same `BLOGIN_COVERAGE_BUILD_DIR` that script writes to.

usage: scripts/uncovered.py [name fragment]
"""

import os
import re
import subprocess
import sys

COUNTED = re.compile(r"^\s*(\d+)\|\s*(\d+)\|")


def ranges(numbers):
    for index, number in enumerate(numbers):
        if index == 0 or number != numbers[index - 1] + 1:
            start = number

        if index + 1 == len(numbers) or numbers[index + 1] != number + 1:
            yield (start, number)


def main(argv):
    pattern = argv[1] if len(argv) > 1 else ""

    sources = sorted(subprocess.run(
        ["find", "src", "-name", "*.cpp", "-o", "-name", "*.h"],
        capture_output=True, text=True, check=True,
    ).stdout.split())

    tool = ["xcrun", "llvm-cov"] if sys.platform == "darwin" else ["llvm-cov"]

    build = os.environ.get("BLOGIN_COVERAGE_BUILD_DIR", "build/coverage")

    shown = subprocess.run(
        tool + ["show", f"{build}/blogin_specs",
                f"-instr-profile={build}/blogin.profdata"] + sources,
        capture_output=True, text=True, check=True,
    ).stdout

    name = ""
    lines = []
    total = 0

    def report():
        if not lines:
            return

        spans = ", ".join(f"{start}" if start == end else f"{start}-{end}"
                          for start, end in ranges(lines))

        print(f"{name}: {len(lines)} lines: {spans}")

    for line in shown.splitlines():
        if line.endswith(":") and "/" in line:
            report()

            name = line.rstrip(":").split("/")[-1]
            lines = []

            continue

        counted = COUNTED.match(line)

        if counted and counted.group(2) == "0" and pattern in name:
            lines.append(int(counted.group(1)))
            total += 1

    report()

    print(f"\n{total} lines uncovered")


if __name__ == "__main__":
    sys.exit(main(sys.argv))
