# Asset provenance

Hearthstone card names, descriptions and artwork belong to Blizzard Entertainment. This local fan application is not affiliated with Blizzard.

- Pinned card data: [HearthstoneJSON build 35747](https://api.hearthstonejson.com/v1/35747/enUS/cards.json), patch 15.6. The original is cached as `cards-source.json`.
- Archived card renders: [schmich/hearthstone-card-images](https://github.com/schmich/hearthstone-card-images), version 5.0.1, fetched through jsDelivr and cached locally.
- Archived tile fallback images: [HearthSim/hs-card-tiles](https://github.com/HearthSim/hs-card-tiles).
- Full portrait, minion, Bob and available hero-power artwork is cached from the [HearthstoneJSON image API](https://hearthstonejson.com/docs/images.html) by `node tools/import-presentation-art.mjs`. `public/art/presentation-manifest.json` records source URLs, card IDs, SHA-256 digests and unavailable images. These images override archived renders at presentation time without changing rules, card data, or saved matches. Missing power artwork retains a procedural rune fallback. Pyramad uses the same character’s `ULDA_BOSS_12h` adventure portrait because the original Battlegrounds art URL is unavailable.
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
- [w64devkit](https://github.com/skeeto/w64devkit): local build toolchain. Compiler sources/binaries are not part of the game distribution. MinGW runtime attribution is included.


## Combat corner faces

`native/assets/combat-board.png` was created with the built-in imagegen tool as an edit of `tavern-board.png`. Only the four corner regions are sampled at runtime, so the centre and UI recesses stay unchanged. This is original matching artwork, not extracted Hearthstone scenery.

Prompt:

> Use case: precise-object-edit. Asset type: alternate combat face of an existing fantasy card-game board. Edit target: attached tavern-board.png. Preserve EXACT overall composition, overhead camera, board outline, pale central playing surface, empty top plaque, left portrait strip, right circular button recess, bottom hero arch, bottom card tray, and painterly rendering. Change ONLY the four corner scenery clusters: top-left candle and scroll become a small iron weapons rack with crossed axes and a red cloth; top-right tankard/coins become a battered shield and small blue pennant; bottom-left mug/crates become a pile of helmets and a sword; bottom-right runestone/chest clutter become a small anvil and armored gauntlets. These props belong on matching chunky wooden rotating panels. Keep all replacement objects confined to corner areas; do not cover centre, side UI recesses or hero arch. Match original wood and brass colors and illumination, with subtle cooler steel on new props. No text, no numbers, no cards, no characters, no logos. Output full matching 16:9 board texture. This is original matching game art, not a screenshot.

Reference material: Blizzard's [2019 Battlegrounds introduction](https://hearthstone.blizzard.com/en-us/news/23156373), [Tips with Kripp](https://www.youtube.com/watch?v=nF2QKEjFwrc), and the corner transition at [1:09 in continuous 2019 gameplay](https://www.youtube.com/watch?v=DkAgDThYsXE&t=69s). Reference footage is not distributed with the app.
