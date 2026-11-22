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
- clear on fixed things: bookshelves, torches, chests, buttons, levers, a painting, a crystal ball, goblets and the
  decorative books on the shelves (`…0032`);
- clear also on scrolls and a map lying inside a chest or bag (probably because they are contained).

Still to check in other places: doors and other usable fixed objects (no flag found yet for "can be used"), NPC
corpses, and items on the floor outdoors.

## Not known yet

- How to list only the chunks around the Avatar when several regions are loaded.
- The camera (position, orientation, field of view) needed to place a marker on screen. The game may submit
  pre-transformed vertices, so Direct3D's transform state may not hold it.
