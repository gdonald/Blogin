#!/usr/bin/env bash
#
# Generate a large synthetic site, so scaling problems show up here rather than
# in production.
#
# Generated, never committed: five thousand posts is megabytes of noise that
# would bloat every clone to say something a script can say on demand. The
# output is deterministic for a given seed and count, so a measurement taken
# today is comparable with one taken later.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

count="${1:-5000}"
target="${2:-$root/build/synth-corpus}"

rm -rf "$target"
mkdir -p "$target/content/posts" "$target/layouts"

python3 - "$count" "$target" <<'PY'
import json
import pathlib
import random
import sys

count = int(sys.argv[1])
target = pathlib.Path(sys.argv[2])

# Fixed seed: the same count always produces the same site.
rng = random.Random(0x5eed)

words = ("alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu "
         "nu xi omicron pi rho sigma tau upsilon phi chi psi omega").split()

tag_pool = [f"tag-{index}" for index in range(40)]

def sentence():
    return " ".join(rng.choice(words) for _ in range(rng.randint(6, 18))).capitalize() + "."

def paragraph():
    return " ".join(sentence() for _ in range(rng.randint(3, 7)))

posts = target / "content" / "posts"

for index in range(count):
    year = 2015 + index % 10
    month = 1 + index % 12
    day = 1 + index % 28

    tags = rng.sample(tag_pool, rng.randint(1, 4))

    body = [f"---",
            f"title: Post number {index}",
            f"date: {year:04d}-{month:02d}-{day:02d}",
            f"tags: [{', '.join(tags)}]",
            f"description: A generated post, number {index}.",
            f"---",
            ""]

    for heading in range(rng.randint(1, 4)):
        body.append(f"## Section {heading}")
        body.append("")
        body.append(paragraph())
        body.append("")
        body.append(f"Some *emphasis* and **weight** in paragraph {heading}.")
        body.append("")

    (posts / f"{year:04d}-{month:02d}-{day:02d}-post-{index}.md").write_text("\n".join(body))

# Close enough in shape to a real site that a build exercises listings,
# pagination, and partials rather than only the post pages.
(target / "layouts" / "base.haml").write_text(
    "%html\n  %head\n    %title= meta-title\n  %body\n    != yield\n")

(target / "layouts" / "show.haml").write_text(
    "%article\n  %h1= title\n  != body\n")

(target / "layouts" / "index.haml").write_text(
    "%section\n"
    "  %h1= heading\n"
    "  != render(:partial<entry>, :collection(posts), :as<entry>)\n"
    "  != pagination-html\n")

(target / "layouts" / "_entry.haml").write_text(
    "%article\n"
    "  %h2\n"
    "    %a{href: \"#{$entry<url>}\"}= $entry<title>\n"
    "  %p= $entry<description>\n")

(target / "blogin.json").write_text(json.dumps({
    "title": "Synthetic",
    "base-url": "https://example.com",
    "home-section": "posts",
    "page-size": 10,
    "taxonomies": ["tags"],
}, indent=2) + "\n")

print(f"{count} posts written to {target}")
PY

printf 'total: %s files, %s KB\n' \
  "$(find "$target" -type f | wc -l | tr -d ' ')" \
  "$(find "$target" -type f -exec du -ck {} + | tail -1 | cut -f1)"
