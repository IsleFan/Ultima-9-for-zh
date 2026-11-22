# World objects: where they live, where they are, which can be taken

2026-11-20..22. Groundwork for a possible "highlight takeable objects" feature (user request 2026-11-20). Static
disassembly of the pinned English `u9.exe` plus two read-only runtime probes. Nothing in the game was changed.

## Probe

`engine.cpp` `probe_objects`, only with the developer marker `object_probe.on` next to the allowlist. Once the
Avatar exists, every 15 s, six times, it logs (local log only):

- `object_probe_avatar`: 0x800 bytes of the Avatar's instance `[[[0x131F668]]+0xE8]`;
- `object_probe_table` / `_rows`: every used handle table entry with its object's first 0x80 bytes;
- `object_probe_pool` / `_rows`: the whole region pool that holds the Avatar's object (non-zero 32-byte records).

Sessions: `front-release/logs/20261122-212631*` (walk and turn), `20261122-213811*` (took a yellow potion, a book
and a lamp). Local only.

## Handle table (CONFIRMED by the code, 0x4C3810 / 0x4C3890)

`[0x8515D0]`: 12-byte entries, capacity `[0x8515CC]` (0x800 at start), free-list head `[0x8515C4]`. Entry `+4` is the
region index (-1 when free), `+8` the object pointer. Only objects something refers to have a handle (about 90 in a
room: torches, buttons, chests, levers, carried items). The yellow potion on the table got a handle only when it was
taken. So the handle table is **not** a list of the world's objects.

## Object record, 32 bytes

| Offset | Meaning | Level |
| --- | --- | --- |
| `+0` | offset of the next record in the same list | STRONG EVIDENCE |
| `+4` | x (low 16 bits), y (high 16 bits) inside the chunk, 0..4095 | CONFIRMED (Avatar record vs instance) |
| `+8` | height (low 16 bits); type id = TYPENAME index (high 16 bits) | CONFIRMED (`0x4BF360` compares `+0xA`) |
| `+0x10` | rotation (values such as 0x5A82 = cos 45° in Q15) | HYPOTHESIS |
| `+0x14` | flags | see below |
| `+0x1C` | owner / list link (changes when an object is taken or the Avatar moves) | HYPOTHESIS |

Type ids below 0x200 are NPCs (`0x4BF290`: record = `[0x98F31C] + type*0x13C`); the Avatar's object has type 0.
A taken object's record is freed: type 0x1FFF, flags `0xFFE0xxxx`.

## Chunks and the region pool (STRONG EVIDENCE)

- Pool base = `[[R+0xD0]+0x34]` with `R = [0x91AAD8 + region*4]` (as `0x4C37BE`, `0x4F1DBC` use it); object
  pointers are base + offset. The Avatar's region was 41; its pool was 32 KB.
- Each 0x2000-byte block of the pool is one chunk of 4096 × 4096 world units. Its first record is a header whose
  `+0xC` / `+0x10` are the chunk's world origin x / y (blocks seen: (8192, 8192), (4096, 8192), (4096, 4096)).
- World position of an object = chunk origin + local x/y; height as stored. Checked on the Avatar: origin (8192,
  8192) + (68, 157) = instance (8260, 8349), height 4155 vs 4157.4.
- When the Avatar walked west over x = 8192 its local x went 70 → 4084.

## Avatar instance (CONFIRMED by the walk)

`[[[0x131F668]]+0xE8]`: world position as floats at `+0x10C` (x), `+0x110` (y), `+0x114` (height); repeated at
`+0x118`, `+0x160`, `+0x16C`. Heading near `+0x1B4` (U9_JUMP_CLIMB.md).

## Which objects can be taken (STRONG EVIDENCE, one room)

Bit `0x2000` of `+0x14`:

- set on everything the player could take in the room: the potions (`…3032`, potions also carry `0x1000`), the books
  that were taken, coins, boots, gauntlets, a staff, the spell book;
- clear on fixed things: bookshelves, chests, buttons, levers, a painting, a crystal ball, goblets and the
  decorative books on the shelves (`…0032`). (Corrected 2026-11-22: wall torches, `…6032`, do have the bit; one
  ceiling torch `…4032` does not.)
- clear also on scrolls and a map lying inside a chest or bag (probably because they are contained).

Still to check in other places: doors and other usable fixed objects (no flag found yet for "can be used"), NPC
corpses, and items on the floor outdoors.

## The camera's draw list (CONFIRMED by the code, 0x4A01B0 = the game's own log_camera_items dump)

- Scene `C = [[0x850228]+0xBC]`; first node `[C+0x11C]`, next node `+4` (iterator `0x5637A0` / `0x5637F0`).
- Node `+0x14` = the object record (null for nodes without one, e.g. level geometry); node `+0x170` flags: the game's
  dump marks a node with any of `0x200C` as **not drawn**.
- Node `+0x2C`..`+0x58`: the instance's 3×4 world matrix, rows of four floats; its translation (the world position)
  is `+0x38`, `+0x48`, `+0x58`. The inverse follows at `+0x5C`. A world bounding box seems to be near `+0xBC`.
- `0x4BE730` (object position) returned (0, 0, 0) for these objects when called from the probe; not used.

Session `front-release/logs/20261122-215837*`: 269–332 nodes with an object in one room.

## The camera (CONFIRMED by the numbers, same session)

- Direct3D: world and view are identity and the projection only maps screen pixels (2/1600, 2/1200). The game
  transforms vertices itself, so the camera is in the scene object `C`.
- `C+0x44`..`+0x70`: world-to-camera 3×4 matrix, rows of four floats: right `(+0x44, +0x48, +0x4C | +0x50)`,
  forward/depth `(+0x54, +0x58, +0x5C | +0x60)`, up `(+0x64, +0x68, +0x6C | +0x70)`. The rotation alone is also at
  `+0x14`..`+0x40`.
- Camera position `+0x80`, `+0x90`, `+0xA0` (also `+0xC4`..`+0xCC`); the matrix maps it to (0, 0, 0).
- `+0x8` = `+0xC` = 1385.64 = 800 / tan 30°: the focal length in pixels for the 1600-pixel-wide screen; `+0x15C` = 60
  (horizontal field of view in degrees); `+0xA4`, `+0xA8` = 800, 600 (meaning unknown).
- Projection used: `v = M·P + t`; screen x = W/2 + f·v.right / v.depth, y = H/2 − f·v.up / v.depth.
- Check: of the 69 nodes the game drew in that frame, 66 project inside the 1600×1200 screen; of the 233 it did not
  draw, 230 project outside.

## Takeable bit, more cases

With `0x2000` of the object's `+0x14` set among drawn nodes: books, the yellow potion, a staff, a short sword, a full
plate armour, and **wall torches** (flags `…6032`; U9 lets the player take torches). Also a "force field" (type 3820)
and an unnamed type 1222: to check in play.

## Not known yet

- Whether the force field and type 1222 are really takeable.
- Where to draw the marker (the instance position is the object's base; the bounding box may give its top).

## Proof of concept: marks over takeable objects (2026-11-22, user approval)

`engine.cpp` `collect_marks` / `update_marks`, only with the developer marker `take_marks.on`. Each frame it walks the
draw list, keeps nodes the game draws whose object passes the filter, projects the box top (`+0x58` + `+0xF8` + 8)
with the camera above and draws a "◆" through the existing overlay (`overlay_text`). Read only, no new hook.

First test (player): the marks sat on the right objects and followed the camera, but also on the Avatar itself, on
wall torches and on objects outside the room (behind walls, on the floor below).

Filters after the test:

- skip type < 0x200 (the Avatar's object has `0x2032`, like an item);
- `(+0x14 & 0x6003) == 0x2002`: drops wall torches (`0x6032`, so `0x4000` = fixed, HYPOTHESIS), decorations (`0x2033`)
  and moss (`0x3030`);
- only within 1200 units horizontally and 220 in height of the Avatar's node (type 0). The game draws objects behind
  walls and on other floors and lets the depth buffer hide them; the drawn flag does not tell.

Second test (player, session `20261122-222847`, local): "looks good". Marked types: books (826, 827, 1292, 1293),
potions (839, 840), staff, short sword, spell book, a loose torch (`0x2032`), a lamp, a chest (1887), the force field
(3820) and the unnamed type 1222 (`0x3032`). `marks_drawn` 39750, `mark_misses` 0, `mark_faults` 0.

Open: whether chest 1887, the force field and type 1222 can really be taken; occlusion by walls inside the range (a
depth test against the game's depth buffer would need the game's z mapping for its pre-transformed vertices).

