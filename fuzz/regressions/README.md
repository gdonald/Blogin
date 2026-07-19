# Regression inputs

One file per input a fuzz target once failed on, named for what went wrong.
`scripts/fuzz.sh replay` runs these alongside the corpus, and `minimize` never
touches them: an input that no longer adds coverage still has to keep passing.

Adding one is the last step of fixing a fuzz finding. The crash artifact goes
here, the fix goes in `src/`, and a spec covering the same case goes in
`specs/`, so the behaviour is pinned by something readable as well as by bytes.
