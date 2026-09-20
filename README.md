# Battlegrounds — The Local Tavern

A Windows desktop game rendered in **C++ / raylib 6**, with a Hearthstone-inspired carved wooden tavern board, actual Hearthstone minion artwork, oval battlefield portraits, dragging, recruitment flights, attack lunges, damage numbers, shield fragments, summon particles, and sound effects.

## Play

Open `release/native/Battlegrounds.exe`. Keep the entire `release/native` folder together: it contains the game, cached art, and its private rules runtime. It works offline and requires no separately installed Node runtime. Windows x64 and an OpenGL 3.3 capable graphics driver are required.

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

The app has no browser or webview. Raylib owns the native window, input, graphics and audio. A bundled, hidden Node process runs the deterministic TypeScript rules through private stdin/stdout pipes. No HTTP server is started.

## Workshop and debugging

**Card Workshop** edits attack, health, tavern tier, pool count, keywords, enabled state, effect handler, description, and buff multipliers. Save, reset, duplicate, export a JSON pack, or drop a pack JSON file onto the workshop. The source definitions are `src/data/cards.json`; token art is in `src/data/token-art.json`. Existing matches embed their original pack, so edits take effect in new matches and newly created lab scenarios. Description text is descriptive: new mechanics require a handler in `src/game/engine.ts` or `src/game/combat.ts`.

**Combat Lab** builds two warbands, adds normal/golden minions, changes stats, plays frame-by-frame combat, estimates outcomes over 100 simulations, and imports/exports scenarios. Drop a scenario JSON on the lab to restore it.

**F2 console** shows state, RNG, pack hash, action log and AI reasoning. It can advance AI, rewind up to 30 in-session snapshots, and run `gold 10`, `health 40`, `tier 6`, `spawn CFM_315`, `give CFM_315` (add to hand), `clear`, or `seed 42`. Choose the affected player in the panel. Debug mutations mark the save as modified.

Autosaves, card overrides, settings, match history and exports live in `%LOCALAPPDATA%\BobsBattlegrounds`. Writes use a temporary file and retain a `.bak` copy. `BOBS_DATA_DIR` overrides the directory for tests. Export a match from the Escape menu; drop it into the game window to restore it. Finished games appear in the Matchbook, with previous/next-round replay controls.

## Build and verify

For a source upload, include the repository files, `package-lock.json`, `cards-source.json`, `src/data`, `public/art`, and `native/assets`. The cached art makes the built game work offline; the source card snapshot is required by the import scripts. Asset ownership and provenance are documented in `native/ASSETS.md`.

Dependencies, `native/vendor`, build outputs, screenshots, test saves, and logs are excluded by `.gitignore`. Keep the entire `release/native` folder together when sharing the playable build separately (for example, as a release ZIP); it is not committed as source.

```powershell
npm ci
npm run setup:native
npm run build
npm start
```

`setup:native` downloads pinned raylib, nlohmann JSON and a portable w64devkit compiler into `native/vendor`. Nothing is installed system-wide. Set `CXX` to use another compatible MinGW compiler. The prepared workspace already has these dependencies and cached art.

```powershell
npm test
npm run test:native
npm run soak -- 1000
```

The native tests cover rules-process communication, card changes/reset, undo, import rejection, save/restart, a complete match, replays, and native buy/play/reorder/cancel/sell drag handlers. Twenty-six captured screens include cards mid-drag, entry before Battlecry summons, combat impact, normal/golden card frames, Warleader aura stats, hover enchantments, outward aura pulses, shield gain/break transitions, star gathering, hero strikes, and the four-panel phase roll. Checks verify that original drag slots stay empty and Battlecry tokens appear after their parent lands. Screenshots are written to `release/native/screenshots`. Test saves use isolated directories under `test-results`.

Aura sources continuously emit expanding glow pulses; hovering an affected minion highlights its source and lists aura bonuses and recorded buffs beneath the enlarged card. Use the mouse wheel to scroll longer lists. Buffs from older saves without recorded provenance appear as Other stat changes. Divine Shield forms and shatters over a short animated transition.

The tavern shows active aura bonuses on affected minions, including Amalgams, with green boosted stats. Bonuses update on sale and repositioning and remain separate from permanent saved stats.

Minions land before Battlecries resolve; repeated Battlecries have separate presentation steps. Summons pop into place with staggered timing. Combat attacks pull back, accelerate into contact, pause briefly on impact, and recoil; deaths and summons have separate beats. Original normal/golden Hearthstone frame layers surround editable artwork, text and stats.

Text fields support cursor movement, Home/End, Shift selection, Ctrl+A, and clipboard shortcuts. Hand cards lift smoothly with stable hover areas; drag targets highlight on the table, and canceled cards return to their slots. Combat centers the enemy hero above the board, with health at the portrait bottom right. Circular hero powers show their cost and a hover description. Four board panels rotate between recruitment and combat. At combat end, surviving minions release tier stars in sequence; these gather into the winning hero before it lunges at the opponent, with damage shown at impact. Divine Shield has a bright gold bubble and label; Reborn has a cyan border and badge, an explicit grant cue, and a longer resurrection beat. Combat has clear win/loss feedback, pauses beneath menus, and settings restore fullscreen on launch.

Art refresh commands (network needed only when importing):

```powershell
npm run import:cards -- --art --tiles --renders
node tools/import-token-art.mjs
node tools/import-frames.mjs
node tools/import-power-art.mjs
```

The UI is C++/raylib. TypeScript supplies the shared game rules, AI and save validation; Node runs the hidden rules host. `npm start`, `npm run build`, and `npm run package` all target the native edition.

## Fidelity and remaining work

This is a playable offline application with the full match lifecycle and editing/debugging tools. It is **not yet an exact historical Hearthstone implementation**. Akazamzarak's Secrets are excluded, Adapt choices are automated, random summons use the supported local pool, and historical edge-case timing has not been exhaustively certified. The 15.6 tier manifest is curated because the archived JSON does not supply those tiers.

Animations, the board, and sound effects are original approximations of the game's presentation. Nine hero portraits currently use a fallback emblem; some archived minion art is low-resolution. English UI, portable Windows packaging, and local heuristic AI are implemented. Localization, a signed installer, exact original animation timing/audio, and full patch parity remain follow-up work.

Asset provenance and the generated board prompt are recorded in [native/ASSETS.md](native/ASSETS.md). The current implementation roadmap is [NATIVE_PLAN.md](NATIVE_PLAN.md).
