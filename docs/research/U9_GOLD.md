# Where the game keeps and shows the Avatar's gold

2026-11-18. Static disassembly of the pinned English `u9.exe` (capstone 5.0.7, local helper). Nothing was run
and nothing was changed. Asked for because the player could not find a gold total anywhere, and as groundwork for
a possible "add 1000 gold" developer cheat (not authorized yet).

## How the game counts gold (CONFIRMED by reading the code)

`0x4AD2F0` returns the Avatar's gold (no arguments):

1. `[0x98F31C]` (the Avatar's record) → `0x4F1D90` gives the Avatar's inventory container.
2. `0x4BF360(0x38C)` searches that container for an object of type `0x38C` = 908. TYPENAME 908 is the item named
   `Gold` (translated 黃金).
3. If found, `0x4C5640(0x11)` reads that object's property 17. That value is the gold count. If not found, the
   result is 0.

So gold is **one object of type 908 in the backpack whose property 17 holds the amount**. The world coin piles
(TYPENAME 1279–1282) are separate types. How they turn into type 908 on pick-up is UNKNOWN.

`0x4AD430` returns `9999 - gold`. STRONG EVIDENCE that 9999 is the most gold the Avatar can carry.

## Where the game shows it (CONFIRMED by reading the code)

- **Stats page** (the builder whose text flow is reported with caller `0x425DC0`). After health and mana it adds
  a divider line (`\t\t` + three native glyph bytes `BC BD BE`), then:
  - misctext 274 (金幣) through `"%s: %d\n"` with the value of `0x4AD2F0`, at `0x425983`–`0x4259C2`;
  - misctext 275 (箭) with `0x4AD710` (arrows), at `0x4259DE`–`0x425A1A`;
  - misctext 49 (技能) and the skill list.
  These lines are added every time, with no condition.
- **Shop**: misctext 253 (可用金幣) at `0x422144` and 93 (總價) at `0x4220F8`.
- **Item label** when the cursor rests on the gold in the backpack: `"(%d) %s"` (HANDOFF, session 25).

## Runtime evidence

The stats page flow logged in session `20261117-123743` was 118 bytes long. The log keeps only the first 48 bytes,
which end after the mana line. So the gold line was in the text the game laid out. Whether it was visible on the
page (and not cut off, or on a page the player did not turn to) is UNKNOWN.

## Open questions

1. Does the stats page show `金幣: N` in the Chinese build? If not, where does the text stop?
2. The item name of type 908 is 黃金, while the stats page and the shop say 金幣. Should they be the same?
3. For a cheat: write property 17 of the existing type 908 object (clamped to 9999), or create the object when the
   Avatar has none. The setter for a property is not identified yet.
