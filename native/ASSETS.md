# Asset provenance

Hearthstone card names, descriptions and artwork belong to Blizzard Entertainment. This local fan application is not affiliated with Blizzard.

- Pinned card data: [HearthstoneJSON build 35747](https://api.hearthstonejson.com/v1/35747/enUS/cards.json), patch 15.6. The original is cached as `cards-source.json`.
- Archived card renders: [schmich/hearthstone-card-images](https://github.com/schmich/hearthstone-card-images), version 5.0.1, fetched through jsDelivr and cached locally.
- Archived tile fallback images: [HearthSim/hs-card-tiles](https://github.com/HearthSim/hs-card-tiles).
- Circular hero powers currently use procedural rune icons. `node tools/import-power-art.mjs` attempts to cache original power artwork from that tile archive and records successful downloads in `public/art/power-manifest.json`; this archive returned no matching power images during the current build.
- Blank normal/golden minion frames, name and tribe ribbons, and attack/health badges: [HearthSim/Sunwell assets](https://github.com/HearthSim/Sunwell/tree/master/assets). These are original Hearthstone asset layers, copyright Blizzard, composed with editable card art, text and numbers at runtime. `native/assets/frames/manifest.json` records URLs and SHA-256 digests; the upstream license is included alongside them. Refresh with `node tools/import-frames.mjs`.
- `public/art/manifest.json` and `public/art/token-manifest.json` record each downloaded URL, original card ID, local path and SHA-256 digest. Some hero renders depict the same named character from another card in the archive.
- Windows Georgia Bold and Segoe UI fonts are loaded from the operating system; their font files are not redistributed.
- Sound effects are synthesized by `native/audio.hpp`; no original game audio is included.

## Original board

`native/assets/tavern-board.png` was generated using the built-in image-generation tool in this session. It is the source background copied into `release/native/assets` by the build. The game adds all interactive minions, text, controls and animation at runtime.

Generation prompt:

> Use case stylized-concept, asset background for native Hearthstone Battlegrounds style game, landscape 16:9. Polished painterly 3D fantasy tavern board directly above, chunky whimsical hand-painted Warcraft/Hearthstone direction. Elaborate physical open wooden box, richly carved walnut/honey oak perimeter, brass rivets, sculpted bevels, deep shadows, blue corner runestones. Wide quiet pale tan parchment/sandstone central 75% width 65% height for two minion rows. Top empty plaque, candle/scroll left, tankard/coins right. Left recessed portrait strip. Right circular turn button recess. Bottom arched hero portrait recess/card-hand tray. Orthographic overhead, no words/numbers/cards/minions/logo/watermark. Warm amber hearth light and purple shadows.

## Runtime dependencies

- [raylib 6.0](https://github.com/raysan5/raylib/releases/tag/6.0): native window, OpenGL rendering, input and audio. zlib/libpng license.
- [nlohmann JSON 3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0): C++ JSON protocol parsing. MIT license.
- [Node.js](https://nodejs.org/): bundled headless rules runtime; its license file is included under `release/native/licenses`.
- [w64devkit](https://github.com/skeeto/w64devkit): local build toolchain. Compiler sources/binaries are not part of the game distribution. MinGW runtime attribution is included.
