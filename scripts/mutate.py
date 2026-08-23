#!/usr/bin/env python3
#
# Mutation testing. Change one operator in the library, rebuild, and run the
# specs. A suite that still passes has a mutant it did not kill, which is a
# behaviour nothing asserts on. Coverage says a line ran. This says an assertion
# would have caught it changing.
#
#   ./scripts/mutate.py                   a sample of 100 mutants
#   ./scripts/mutate.py --count 400       a longer run
#   ./scripts/mutate.py --seed 7          a different sample, reproducibly
#   ./scripts/mutate.py --file markdown   only the files whose path matches
#   ./scripts/mutate.py --floor 90        fail under a score of 90
#   ./scripts/mutate.py --at json.cpp:445 one line's mutants, to re-check a fix
#
# Python rather than bash because the mutations are found by walking C++ source
# and skipping what is inside a comment or a string, which is a lexer rather
# than a regular expression.
#
# There is no mull here. Its packages are built per LLVM release and the
# container carries a newer clang than any of them, so this does the same job
# against whatever compiler is present.
#
# One mutant costs a single translation unit's rebuild plus a spec run, about
# five seconds. It is left out of scripts/test.sh for the same reason fuzzing
# is: the result is a number to act on rather than a gate on every commit.

import argparse
import os
import random
import re
import hashlib
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Mutants that change no behaviour, each with the reading behind that claim.
EQUIVALENT = ROOT / "scripts" / "mutants-equivalent.txt"

# Each pair is a change a correct suite should notice. The comparisons and the
# logical operators are where a parser's decisions live, and a boolean literal
# is the switch under a feature.
REPLACEMENTS = {
    "==": "!=",
    "!=": "==",
    "<=": "<",
    ">=": ">",
    "&&": "||",
    "||": "&&",
    "+=": "-=",
    "-=": "+=",
    "true": "false",
    "false": "true",
}

# Longest first, so `<=` is read as one token rather than as `<`.
TOKENS = sorted(REPLACEMENTS, key=len, reverse=True)

WORD = re.compile(r"\w")


def read_equivalent() -> set[tuple[str, str, str, str]]:
    """The mutants listed as equivalent, keyed by file, change, and two lines of source."""
    if not EQUIVALENT.exists():
        return set()

    listed = set()
    entry: dict[str, str] = {}

    def keep(fields: dict[str, str]) -> None:
        if {"path", "change", "before", "line"} <= fields.keys():
            listed.add((fields["path"], fields["change"], fields["before"], fields["line"]))

    for raw in EQUIVALENT.read_text().splitlines():
        if raw.startswith("#"):
            continue

        if raw.strip() == "[mutant]":
            keep(entry)
            entry = {}
            continue

        key, separator, value = raw.partition(" = ")

        if separator and not key.startswith(" "):
            entry[key.strip()] = value

    keep(entry)

    return listed


@dataclass(frozen=True)
class Mutant:
    path: Path
    offset: int
    line: int
    original: str
    replacement: str

    def describe(self) -> str:
        relative = self.path.relative_to(ROOT)
        return f"{relative}:{self.line}  {self.original} -> {self.replacement}"

    def key(self, text: str) -> tuple[str, str, str, str]:
        lines = text.split("\n")
        before = lines[self.line - 2].strip() if self.line >= 2 else ""

        return (str(self.path.relative_to(ROOT)), f"{self.original} -> {self.replacement}",
                before, lines[self.line - 1].strip())


def code_spans(text: str):
    """The half-open ranges of `text` that are code, skipping comments and literals."""
    spans = []
    index = 0
    start = 0
    size = len(text)

    while index < size:
        two = text[index:index + 2]

        if two == "//":
            spans.append((start, index))
            index = text.find("\n", index)
            index = size if index < 0 else index
            start = index
        elif two == "/*":
            spans.append((start, index))
            closing = text.find("*/", index + 2)
            index = size if closing < 0 else closing + 2
            start = index
        elif text[index] in "\"'" or text[index:index + 2] in ('R"', 'u8'):
            spans.append((start, index))
            index = skip_literal(text, index)
            start = index
        else:
            index += 1

    spans.append((start, size))

    return spans


def skip_literal(text: str, index: int) -> int:
    """The offset just past the string or character literal starting at `index`."""
    raw = re.compile(r'(?:u8|u|U|L)?R"([^(]*)\(').match(text, index)

    if raw:
        closing = f'){raw.group(1)}"'
        end = text.find(closing, raw.end())

        return len(text) if end < 0 else end + len(closing)

    quote_at = index

    while quote_at < len(text) and text[quote_at] not in "\"'":
        quote_at += 1

    if quote_at >= len(text):
        return len(text)

    quote = text[quote_at]
    scan = quote_at + 1

    while scan < len(text):
        if text[scan] == "\\":
            scan += 2
            continue

        if text[scan] == quote:
            return scan + 1

        scan += 1

    return len(text)


def find_mutants(path: Path) -> list[Mutant]:
    text = path.read_text()
    found = []

    for start, end in code_spans(text):
        index = start

        while index < end:
            for token in TOKENS:
                if not text.startswith(token, index):
                    continue

                # A word only counts whole: `truest` is not `true`, and the
                # `+=` inside `x +=` is, while a `>=` in `->` is not there at
                # all.
                if token.isalpha():
                    before = text[index - 1] if index else " "
                    after = text[index + len(token):index + len(token) + 1] or " "

                    if WORD.match(before) or WORD.match(after) or before == ".":
                        break

                found.append(Mutant(path, index, text.count("\n", 0, index) + 1,
                                    token, REPLACEMENTS[token]))
                index += len(token) - 1
                break

            index += 1

    return found


def stamp(path: Path) -> None:
    """Mark `path` as changed in a way every build tool notices.

    Make compares whole seconds, so a file written and put back inside one
    second looks untouched and the object built from the mutated text stays in
    the tree. A timestamp a second ahead is newer than any object built now.
    """
    ahead = time.time() + 1
    os.utime(path, (ahead, ahead))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def object_for(build_dir: Path, source: Path) -> Path | None:
    """The object file a source compiles to, which is what a mutation has to change.

    The linked binary is no use for the comparison: Mach-O carries a UUID that
    changes on every link, so two binaries built from the same objects differ.
    A header compiles to no object of its own and is checked by its stamp alone.
    """
    return next(build_dir.rglob(f"{source.name}.o"), None)


def run(command: list[str], timeout: float | None = None) -> int:
    try:
        finished = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        # A mutant that never finishes is one the suite caught by hanging on it,
        # which is a kill rather than a survivor: the behaviour changed.
        return 124

    return finished.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="Mutation testing for the library")
    parser.add_argument("--count", type=int, default=100, help="how many mutants to run")
    parser.add_argument("--seed", type=int, default=0, help="which sample to take")
    parser.add_argument("--file", default="", help="only source files whose path contains this")
    parser.add_argument("--at", action="append", default=[], metavar="PATH:LINE",
                        help="only the mutants on this line, which is how a survivor is re-checked")
    parser.add_argument("--floor", type=float, default=0.0, help="fail below this score")
    parser.add_argument("--build-dir", default="build/mutate", help="where to build")
    parser.add_argument("--jobs", type=int, default=0, help="cores, defaulting to what the machine has")
    parser.add_argument("--timeout", type=float, default=120.0, help="seconds a mutant's specs may take")
    options = parser.parse_args()

    jobs = options.jobs or os.cpu_count() or 4
    build_dir = ROOT / options.build_dir
    specs = build_dir / "blogin_specs"
    backup = build_dir / "mutant-original"

    sources = sorted(p for p in (ROOT / "src" / "blogin").rglob("*.cpp") if options.file in str(p))
    sources += sorted(p for p in (ROOT / "src" / "blogin" / "include").rglob("*.h") if options.file in str(p))

    if not sources:
        print(f"no source files match '{options.file}'", file=sys.stderr)
        return 2

    if backup.exists():
        print(f"a previous run left {backup}. Restore the file it came from before running again",
              file=sys.stderr)
        return 2

    print("building the tree the mutants are built against", flush=True)

    # Warnings stay on and stop being errors. A mutant that writes `a && b || c`
    # is refused for its parentheses otherwise, which counts a live mutation as
    # one that does not compile.
    if run(["cmake", "-S", ".", "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Debug", "-DBLOGIN_WERROR=OFF"]):
        print("configure failed", file=sys.stderr)
        return 2

    build = ["cmake", "--build", str(build_dir), "--target", "blogin_specs", "-j", str(jobs)]

    # The first build is a clean one. A run that was interrupted between
    # mutating a file and putting it back leaves an object compiled from the
    # mutated text, and every mutant after that would be measured against it.
    if run(build + ["--clean-first"]):
        print("the unmutated tree does not build", file=sys.stderr)
        return 2

    if run([str(specs), "--jobs", str(jobs)], timeout=options.timeout):
        print("the specs fail before anything is mutated. Fix that first", file=sys.stderr)
        return 2


    candidates = [mutant for source in sources for mutant in find_mutants(source)]

    if not candidates:
        print("nothing to mutate", file=sys.stderr)
        return 2

    listed = read_equivalent()
    contents = {source: source.read_text() for source in sources}
    equivalent = [mutant for mutant in candidates if mutant.key(contents[mutant.path]) in listed]
    candidates = [mutant for mutant in candidates if mutant.key(contents[mutant.path]) not in listed]

    if equivalent:
        print(f"{len(equivalent)} listed in {EQUIVALENT.name} as changing no behaviour")

    if options.at:
        wanted = set()

        for entry in options.at:
            path, _, line = entry.rpartition(":")
            wanted.add((path, int(line)))

        candidates = [mutant for mutant in candidates
                      if (str(mutant.path.relative_to(ROOT)), mutant.line) in wanted
                      or (mutant.path.name, mutant.line) in wanted]

        if not candidates:
            print(f"no mutants at {', '.join(options.at)}", file=sys.stderr)
            return 2

    random.Random(options.seed).shuffle(candidates)
    chosen = candidates if options.at else candidates[:options.count]

    print(f"{len(candidates)} mutants available, running {len(chosen)} of them\n", flush=True)

    killed = 0
    survived: list[Mutant] = []
    stillborn = 0
    unapplied = 0
    started_at = time.monotonic()

    for index, mutant in enumerate(chosen, start=1):
        original = mutant.path.read_text()
        shutil.copyfile(mutant.path, backup)

        # What this source compiled to before it was touched. A build that
        # leaves it identical did not pick the mutation up, and the specs would
        # then be run against code nobody changed.
        compiled = object_for(build_dir, mutant.path)
        compiled_before = digest(compiled) if compiled else None

        mutated = (original[:mutant.offset]
                   + mutant.replacement
                   + original[mutant.offset + len(mutant.original):])
        mutant.path.write_text(mutated)
        stamp(mutant.path)

        try:
            if run(build):
                # A mutant that does not compile says nothing about the specs.
                # `<` inside a template argument list is the common one.
                stillborn += 1
                outcome = "did not build"
            elif compiled_before is not None and digest(compiled) == compiled_before:
                unapplied += 1
                outcome = "not applied"
            elif run([str(specs), "--jobs", str(jobs)], timeout=options.timeout):
                killed += 1
                outcome = "killed"
            else:
                survived.append(mutant)
                outcome = "SURVIVED"
        finally:
            mutant.path.write_text(original)
            stamp(mutant.path)
            backup.unlink()

        # Flushed, because a run is minutes long and a pipe buffers it into
        # silence otherwise.
        print(f"  [{index}/{len(chosen)}] {outcome:12} {mutant.describe()}", flush=True)

    run(build)

    tested = killed + len(survived)
    score = 100.0 * killed / tested if tested else 0.0

    print()

    if survived:
        print("Survivors, each a change to the library no example objects to:")
        print()

        for mutant in survived:
            print(f"  {mutant.describe()}")

        print()

    print(f"killed:     {killed}")
    print(f"survived:   {len(survived)}")
    print(f"not built:  {stillborn}")
    print(f"not applied:{unapplied}")
    print(f"score:      {score:.1f}% (floor {options.floor:.1f}%)")
    print(f"took:       {time.monotonic() - started_at:.0f}s")

    if tested and score < options.floor:
        print("\nmutation score below the floor", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
