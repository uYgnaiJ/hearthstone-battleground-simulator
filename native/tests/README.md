# Native regression tests

`legacy-parity.json` contains 812 deterministic reference results captured from the previous TypeScript rules engine before its removal. It covers each of the 80 minions in normal and golden form during play and combat, every hero's initial state, active hero powers, and every round of 30 complete seeded games across all three AI difficulties.

`native/tests.cpp` reconstructs the scenarios and compares FNV-1a hashes of the entire canonical result, including combat animation frames, RNG state, pool accounting, AI decisions, and permanent buffs. Object keys are sorted and wall-clock creation timestamps are excluded. These fingerprints preserve migration parity; they are not a certification of historical Hearthstone rules.

Additional readable assertions cover auras, frozen shop refill, magnetic merges through triples, Reborn, poison versus Divine Shield, validation, save recovery, editor changes, undo, lab simulations, full-match persistence, and replay. `--soak N` runs N complete matches and checks termination, one winner, board/hand limits, and combat event limits.

From the repository root:

```powershell
./tools/test.ps1 -Smoke
./tools/test.ps1 -SkipBuild -Soak 1000
```

All test saves and failure artifacts are isolated under ignored `test-results/`.
