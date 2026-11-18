# Release patch format (U9CP)

2026-11-18. A release carries no game file. The four localized FLX files still hold the game's own bytes: the
English entries the build leaves unchanged, the container, and the Gargish passages quoted in books. So the
release carries, for each FLX, a patch that turns the player's original file into the Chinese one.

- Made by `u9cht/l10n/patch.py` (`make`), called by `scripts/package_release.py`.
- Applied by the front page (`runtime/common/flx_patch.h`, `front::prepare_static`) the first time the player
  starts the game. The input must match `source_game_sha256` and the result `resource_output_sha256` of the
  shipped `build-manifest.json`. The result is written to `data/static/` beside the release, never to the game.
- GOG and Steam ship the same four FLX files (CONFIRMED, HANDOFF), so one patch per file serves both.

## Layout (little-endian)

| Field | Size |
| --- | --- |
| `U9CP` | 4 |
| version (1) | u32 |
| source size | u32 |
| target size | u32 |
| op count | u32 |
| ops | … |

| Op | Bytes | Meaning |
| --- | --- | --- |
| 0 copy | `00`, u32 offset, u32 length | bytes from the original |
| 1 add | `01`, u32 length, data | bytes written by this project |
| 2 widen | `02`, u32 offset, u32 length | original bytes, each turned into a UTF-16LE unit `(byte, 00)` |

The ops must give exactly the target size; every copy must stay inside the original; nothing may follow the last op.

## What a patch may contain

The generator copies every run of 8 or more bytes that the original holds, directly or widened. The packaging
script refuses a patch whose added bytes hold a run of 16 or more Latin letters (UTF-16LE or ASCII). The added
bytes are the Chinese text, the markers and offsets of the rebuilt container, and short Latin pieces of our own
translations such as `(Britannia)`.

Sizes for the first internal build (`out-m28`): text.flx 449,575 bytes, misctext.flx 6,349, TYPENAME.FLX
113,252, BOOKS-EN.FLX 119,145 (the widen op removed 27 KB of Gargish from the books patch).
