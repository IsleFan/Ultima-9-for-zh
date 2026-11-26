# bitmap16.flx notes (signpost textures)

Evidence date: 2026-11-26. File: `static/bitmap16.flx`, 320,877,694 bytes, SHA-256
`3ec4ece8540c64ac7717d431267d2ab85a8612ccf119b78c36a299021ad1adcb` (GOG/Steam 1.19F). Code: `u9cht/bitmap_flx.py`,
tests `tests/test_bitmap_flx.py`.

## Which file the game reads

`Options.ini` has `Use16BitTextures=1` and `UseCompressedTextures=0`, so the game reads `bitmap16.flx` (STRONG
EVIDENCE: the setting names, and the patched entries showed in game). `bitmapC.flx` (compressed, 1,548 bytes per
signpost) and `bitmapsh.flx` (2,892 bytes per signpost) hold the same entries in other formats; a player who changes
those settings would see the English signs. Not handled yet.

## Container

The same FLX header as text.flx (`U9-FLX-NOTES.md`): slot count at `0x50` (8,000), table at `0x80` of
`(u32 offset, u32 length)`. 6,599 slots are used. The terrain files `texture16.9` (16,384 slots, 3,365 used, all
64x64) and `texture16.14` share the container.

## Texture entry (the layout `bitmap_flx.py` accepts)

| Offset | Field |
| --- | --- |
| `0x00` | u32: width in the low 16 bits, number of mipmaps in the high 16 (`0x30040`: 64, 3 mipmaps) |
| `0x04` | u32 height |
| `0x08` | u32 frame count (1) |
| `0x0C` | u32, meaning UNKNOWN (`0x56000` on the signposts); kept as read |
| `0x10` | frame table: `(u32 offset 0x18, u32 length)` |
| `0x18` | frame: u32 flags, u32 width, u32 height, 8 bytes kept as read (zero here) |
| `0x2C` | `height` u32 row offsets, relative to the frame: `0x14 + 4*height + 2*width*row` |
| then | the image, RGB565, row by row; then each mipmap at half the size (32x16, 16x8, 8x4) |

- Frame flags `0x60000400`: RGB565 (CONFIRMED by decoding; the 1555 reading gives wrong colours). Bits 8..9 = 1
  (`0x500` family) is ARGB1555, as in tgwUltima9GL's `GoldLetters.cs`. Other flag values exist (`0x2000051F`,
  `0x220A4500`, ...) and are refused.
- Mipmaps are 2x2 averages; `bitmap_flx.downsample` (round to nearest) matches the game's within one step per
  channel on every pixel of entries 4472 and 4569.
- Sizes in use: 128x128 (940 entries), 64x64 (868), 32x32, 64x128, 256x256, ... Most are single-frame.

## Signposts

Ten place-name boards, 64x32, 5,612 bytes each (CONFIRMED by a similarity search over every 64x32 single-frame entry;
the next match is far off):

| Entry | Text | Entry | Text |
| --- | --- | --- | --- |
| 4471 | Asylum | 4476 | Trinsic |
| 4472 | Britain | 4477 | Valoria |
| 4473 | Cove | 4478 | Yew |
| 4474 | Moonglow | 4568 | Column |
| 4475 | Paws | 4569 | Stonegate |

Entry 3381 is the same board with no name: it differs from 4472 only inside `(14,9)-(51,19)` and from 4569 only
inside `(6,11)-(57,22)` (CONFIRMED). Entries 3379, 3382 and 3383 are blank arrow boards in other colours.

## Round trip and first change

- All ten signposts: `build_texture(parse_texture(entry)) == entry` (BYTE-PERFECT PASS, local-only test).
- 2026-11-26: 4472 → 不列顛城 and 4569 → 石門, drawn on entry 3381 with Noto Sans TC Bold at 12 px (white, dark
  outline), mipmaps made again, written in place into the private copy (same length, table untouched). In game, at
  the Britain/Stonegate crossroads, both boards read clearly (RUNTIME PASS, screenshot by the player).

## Delivery: file redirect (authorized 2026-11-26)

The runtime hooks u9.exe's `CreateFileA` import (IAT slot `0x7400CC`, `runtime/U9ChtRuntime/file_redirect.h`). When the
game opens `static\bitmap16.flx` for reading, the runtime opens `<data>\redirect\bitmap16.flx` instead. The game
folder is never changed.

- The game opens all its resources by full path with `GENERIC_READ` and `OPEN_ALWAYS` (CONFIRMED with a temporary
  file probe that logged the opened names). It opens `bitmap16.flx`, `bitmapC.flx` and `bitmapsh.flx` twice each
  at start.
- Redirected only for read access with `OPEN_EXISTING` or `OPEN_ALWAYS`, and only when the copy has the size of
  the game's file (event `file_redirect_setup`). When opening the copy fails, the game's own file is opened.
- 2026-11-26, private copy back to the original hash, copy in the test folder's `data\redirect`: both opens were
  redirected (`file_redirect` `ok:true`), the boards showed in Chinese, the game ended normally (RUNTIME PASS).

## Not done

- Drawing the text is a local script that uses Pillow (`.local/research-signs/`), not part of the repository.
- No release path yet: the front page has to make `data\redirect\bitmap16.flx` from the player's own file and a
  patch of the ten entries (the copy holds the game's art and must never be distributed). `bitmapC.flx` and
  `bitmapsh.flx` need their own formats.
- 4471 Asylum has no approved Chinese name.
