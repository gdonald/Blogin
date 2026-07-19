# Seed inputs

Well-formed examples of what each target parses, written by hand: one file per
construct, named for the construct. They give the fuzzer somewhere to start, so
it spends its budget on the edges of a real frame or a real document rather than
on rediscovering that a WebSocket frame begins with an opcode byte.

`minimize` never touches these. It keeps whichever inputs cover the same edges
most cheaply, which is the right answer for a machine-grown corpus and the wrong
one for a set that also documents the format: a hand-written ping frame and a
70,000 byte payload say something a smaller blob covering the same branches does
not.

`fuzz/corpus/` is the machine-grown counterpart, and `fuzz/regressions/` holds
inputs that once failed. Replay runs all three, explore seeds from all three and
writes new finds into the corpus only.
