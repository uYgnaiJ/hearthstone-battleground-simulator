# Native Battlegrounds implementation status

The requested product direction is a local game with a Hearthstone-style wooden box/table, animated card interactions, local computer opponents, editable card definitions and a debug console. This supersedes the earlier Electron/React presentation proposal.

## Implemented

1. **Native game window:** C++20/raylib 6, resizable 1600×900 logical canvas, fullscreen, physical tavern background, oval portraits, hover detail cards, dragging and contextual controls.
2. **Game presentation:** recruitment flights, summon particles, attack lunges, damage text, shield fragments, death smoke, camera shake, synthesized sound, combat pause/step/speed controls.
3. **Offline match lifecycle:** hero selection, seven AI opponents, shop economy, warband/hand management, tiers, triples/discovery, powers, combat, elimination, final placement, save/resume and match history.
4. **Pinned content:** HearthstoneJSON 35747 / Origins 15.6, curated 80-minion/23-hero ruleset, local card renders/tiles, artwork for 18 summon types, provenance manifests.
5. **Editing:** native workshop for card stats/tier/pool/keywords/text/effect handler, duplication/reset, JSON pack import/export, saved matches retain their own content pack.
6. **Tools:** native combat lab, 100-sample odds, F2 state/log/AI panel, debug mutations, per-session rewind, all-round replay navigation.
7. **Delivery:** portable Windows folder with a single native executable and JSON/art assets, local build/setup scripts, isolated automated native smoke tests and native host integration tests.

## Architecture

`native/main.cpp`, `render.hpp`, `motion.hpp`, and `audio.hpp` own the game UI and effects. `native/bridge.hpp` sends commands to a serial worker thread in the same process. `native/rules/host.hpp` manages commands, editable content, atomic saves, undo, history and combat lab. `engine.hpp`, `combat.inc`, and `ai.inc` contain deterministic C++ rules and computer opponents. JSON is the data and snapshot format; no JavaScript engine, child process, browser, or HTTP server is used.

Build, setup, launch and tests use PowerShell scripts and the C++ compiler. Optional Node scripts remain only for refreshing source artwork. Existing version-1 saves and card packs remain compatible.

## Release validation

- C++20 compilation and 812 deterministic legacy parity cases, including all minions in normal/golden form and 30 complete seeded matches.
- Gameplay tests including frozen-shop refill and Magnetic pool accounting through triples.
- Native host integration through restart, backup recovery, finished match and replay.
- Native drag-handler test for buy/play and rendered screenshot review across game, combat, editor, collection, lab and debugger.
- Seeded full-game soak following rules changes.

## Remaining fidelity work

The current app is playable throughout its supported ruleset. Full historical parity is a separate acceptance milestone: implement Akazamzarak's Secrets, player-selectable Adapt, exact historical random summon pools and audited edge-case timing. Replace the ten missing hero portraits and low-resolution tile fallbacks, refine original-game motion timing, add localization/accessibility, and produce a signed installer. These items are not represented as completed.
