# Battlegrounds — The Local Tavern

A Windows and macOS desktop game rendered in **C++ / raylib 6**, with a **2.5D perspective renderer**, a Hearthstone-inspired carved wooden tavern board, actual Hearthstone minion artwork, oval battlefield portraits, dragging, recruitment flights, attack lunges, damage numbers, shield fragments, summon particles, and sound effects.

## Play

On **macOS (Apple Silicon)**, open `release/macos/Battlegrounds.app`, or extract `release/macos/Battlegrounds-macos-arm64.zip` and drag the app into Applications. The app includes its artwork and data and needs no Node runtime. The build targets macOS 11 or later; it is locally ad-hoc signed, not Apple notarized.


Open `release/native/Battlegrounds.exe`. Keep the entire `release/native` folder together: it contains the C++ executable, cached art, and editable JSON data. It works offline with no Node or JavaScript runtime. Windows x64 and an OpenGL 3.3 capable graphics driver are required.

New matches automatically generate a fresh random seed, including when choosing **Play again**. You can edit **Match seed** on the hero-selection screen to reproduce a particular setup. Resuming a saved game keeps its original seed. Combat Lab has its own seed.

The game offers seven local computer opponents, three difficulty levels, hero selection, recruitment, triples, discoveries, hero powers, automatic combat, elimination, final placement, autosaves, and finished-match replays. Matches use a curated Origins pool: 80 minions and 23 heroes sourced from HearthstoneJSON **15.6 / build 35747**.

| Action | Control |
|---|---|
| Buy a minion | Drag from the shop toward your hand, or click it and Recruit |
| Play a minion | Drag from your hand onto the lower battlefield |
| Target a battlecry | Drop the hand card over the friendly target |
| Magnetic merge | Hold Shift while dropping onto a friendly Mech |
| Reorder | Drag a friendly minion along the lower row |
| Cancel a drag | Right-click, Escape, or release outside a valid area |
| Sell | Drag a friendly minion into the shop area |
| Target a hero power | Select a friendly minion, then activate the power |
| Refresh / freeze | R / F |
| End turn / pause combat | Space |
| Debug panel | F2 |
| Fullscreen / menu | F11 / Escape |

The app has no browser or webview. Raylib owns the native window, input, graphics and audio. The rules, combat, local AI, saves, editor and debugging commands are all C++20. A worker thread inside the executable processes game commands while the render thread keeps drawing. No child process or HTTP server is started.

The tabletop is a perspective-projected textured plane. Minions have raised rims and depth-dependent scale; cards tilt and cast broader shadows during drags, recruitment flights, and attacks. Hand cards fan out and lift on hover. Controls, menus, and tooltips remain in screen space for readability. Board and hand drag targets use inverse projection to match their tilted planes. The renderer reuses the existing painted artwork; it does not include fully modeled scenery or dynamic 3D lighting.

## Workshop and debugging

**Card Workshop** edits attack, health, tavern tier, pool count, keywords, enabled state, effect handler, description, and buff multipliers. Save, reset, duplicate, export a JSON pack, or drop a pack JSON file onto the workshop. The source definitions are `src/data/cards.json`; token art is in `src/data/token-art.json`. Existing matches embed their original pack, so edits take effect in new matches and newly created lab scenarios. Description text is descriptive: new mechanics require a handler in `native/rules/engine.hpp` or `native/rules/combat.inc`.

**Combat Lab** builds two warbands, adds normal/golden minions, changes stats, plays frame-by-frame combat, estimates outcomes over 100 simulations, and imports/exports scenarios. Drop a scenario JSON on the lab to restore it.

**F2 console** shows state, RNG, pack hash, action log and AI reasoning. It can advance AI, rewind up to 30 in-session snapshots, and run `gold 10`, `health 40`, `tier 6`, `spawn CFM_315`, `give CFM_315` (add to hand), `clear`, or `seed 42`. Choose the affected player in the panel. Debug mutations mark the save as modified.

On macOS, saves and settings live in `~/Library/Application Support/BobsBattlegrounds`. On Windows, autosaves, card overrides, settings, match history and exports live in `%LOCALAPPDATA%\BobsBattlegrounds`. Writes use a temporary file and retain a `.bak` copy. `BOBS_DATA_DIR` overrides the directory for tests. Export a match from the Escape menu; drop it into the game window to restore it. Finished games appear in the Matchbook, with previous/next-round replay controls.

## Build and verify

For macOS, install Xcode Command Line Tools (`xcode-select --install`) if needed, then run:

```sh
./tools/setup-macos.sh
./tools/build-macos.sh --tests
./tools/test-macos.sh --skip-build --smoke
open release/macos/Battlegrounds.app
```

Setup downloads checksum-verified raylib 6.0 source and nlohmann JSON 3.12.0 into `native/vendor`. Building uses Apple's compiler and system frameworks, with raylib statically linked. No Homebrew dependencies are required. The build produces an app and ZIP for the host architecture (Apple Silicon on an arm64 Mac, Intel on an Intel Mac). Only Apple Silicon has been verified here. Smoke reports and screenshots go into isolated `test-results/native-smoke-*` folders, outside the signed app. Use `./tools/test-macos.sh --skip-build --soak 1000` for extended match checks.

The npm shortcuts select the scripts for your operating system: `npm run setup:native`, `npm run build`, `npm start`, and `npm run test:native`.

### Windows


For a source upload, include the repository files, `package-lock.json`, `cards-source.json`, `src/data`, `public/art`, and `native/assets`. The cached art makes the built game work offline; the source card snapshot is required by the import scripts. Asset ownership and provenance are documented in `native/ASSETS.md`.

Dependencies, `native/vendor`, build outputs, screenshots, test saves, and logs are excluded by `.gitignore`. Keep the entire `release/native` folder together when sharing the playable build separately (for example, as a release ZIP); it is not committed as source.

```powershell
./tools/setup.ps1
./tools/build.ps1
./tools/run.ps1
```

`tools/setup.ps1` downloads pinned raylib, nlohmann JSON and a portable w64devkit compiler into `native/vendor`. Nothing is installed system-wide. Set `CXX` to use another compatible MinGW compiler. The prepared workspace already has these dependencies and cached art.

```powershell
./tools/test.ps1 -Smoke
./tools/test.ps1 -SkipBuild -Soak 1000
```

The macOS checks also verify perspective depth, card lift anchors, and inverse-projected picking. The native suite includes 812 reference cases captured from the previous engine, plus gameplay assertions and integration checks covering card changes/reset, undo, import rejection, save/restart, a complete match, replays, and native buy/play/reorder/cancel/sell drag handlers. Thirty-one captured screens include cards mid-drag, entry before Battlecry summons, combat impact, normal/golden card frames, Warleader aura stats, hover enchantments, outward aura pulses, shield gain/break transitions, star gathering, hero strikes, and the corner-panel phase flip. Checks verify that original drag slots stay empty and Battlecry tokens appear after their parent lands. Screenshots are written to `release/native/screenshots`. Test saves use isolated directories under `test-results`.

Aura sources continuously emit expanding glow pulses; hovering an affected minion highlights its source and lists aura bonuses and recorded buffs beneath the enlarged card. Use the mouse wheel to scroll longer lists. Buffs from older saves without recorded provenance appear as Other stat changes. Divine Shield forms and shatters over a short animated transition.

The tavern shows active aura bonuses on affected minions, including Amalgams, with green boosted stats. Bonuses update on sale and repositioning and remain separate from permanent saved stats.

Minions land before Battlecries resolve; repeated Battlecries have separate presentation steps. Summons pop into place with staggered timing. Combat attacks pull back, accelerate into contact, pause briefly on impact, and recoil; deaths and summons have separate beats. Original normal/golden Hearthstone frame layers surround editable artwork, text and stats.

Text fields support cursor movement, Home/End, Shift selection, Ctrl+A, and clipboard shortcuts. Hand cards lift smoothly with stable hover areas; drag targets highlight on the table, and canceled cards return to their slots. Combat centers the enemy hero above the board, with health at the portrait bottom right. Circular hero powers show their cost and a hover description. Four corner scenery panels flip around horizontal axles between recruitment and combat, revealing tavern props or battlefield weapons. The centre stays fixed; the shop/opponent display switches during the flip, and combat waits for the reveal. Returning to recruitment reverses the motion. At combat end, surviving minions release tier stars in sequence; these gather into the winning hero before it lunges at the opponent, with damage shown at impact. Divine Shield has a bright gold bubble and label; Reborn has a cyan border and badge, an explicit grant cue, and a longer resurrection beat. Combat has clear win/loss feedback, pauses beneath menus, and settings restore fullscreen on launch.

Optional artwork import tools still use Node as a development tool. Node and npm are not needed to build, test, launch, or distribute the game. Install those optional dependencies only to refresh artwork (network required):

```powershell
npm ci
npm run import:cards -- --art --tiles --renders
node tools/import-token-art.mjs
node tools/import-frames.mjs
node tools/import-power-art.mjs
```

Source layout: `native/main.cpp`, `render.hpp`, `perspective.hpp`, `motion.hpp`, and `audio.hpp` handle presentation; `native/rules` contains recruitment, combat, AI, persistence and validation; `native/bridge.hpp` owns the worker queue. Card definitions remain plain JSON. The optional npm build/start shortcuts select the platform-specific scripts.

## Fidelity and remaining work

This is a playable offline application with the full match lifecycle and editing/debugging tools. It is **not yet an exact historical Hearthstone implementation**. Akazamzarak's Secrets are excluded, Adapt choices are automated, random summons use the supported local pool, and historical edge-case timing has not been exhaustively certified. The 15.6 tier manifest is curated because the archived JSON does not supply those tiers.

Animations, the board, and sound effects are original approximations of the game's presentation. Nine hero portraits currently use a fallback emblem; some archived minion art is low-resolution. English UI, Windows and macOS app packaging, and local heuristic AI are implemented. Localization, a signed installer, exact original animation timing/audio, and full patch parity remain follow-up work.

Asset provenance and the generated board prompt are recorded in [native/ASSETS.md](native/ASSETS.md). The current implementation roadmap is [NATIVE_PLAN.md](NATIVE_PLAN.md).

The recruitment screen uses Bob’s portrait with grouped upgrade, refresh and freeze controls, a recessed turn button, a coin counter and contextual drag guidance. Full artwork overrides older thumbnail crops when available; refresh the local cache with `node tools/import-presentation-art.mjs`. The board uses a restrained perspective and desaturated wood palette.
