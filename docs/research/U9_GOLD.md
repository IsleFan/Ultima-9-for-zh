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

1. ~~Does the stats page show `金幣: N` in the Chinese build?~~ Yes: the player confirmed 金幣 and 箭 under the
   mana line (2026-11-18).
2. ~~The item name of type 908 is 黃金.~~ Renamed 金幣 (user decision 2026-11-18, commit `c71300c`).
3. ~~For a cheat: the setter for a property is not identified yet.~~ Found, see below.

## Setting the amount (CONFIRMED by reading the code)

`0x4C56F0` is the property setter: thiscall on a handle holder, arguments (property, value), `RET 8`. It is the
counterpart of the getter `0x4C5640` and calls `0x4C9470`. The gold pick-up code `0x4AD550` uses it with
property `0x11` on the type 908 object, after capping the total with `0x4AD430` (`9999 - gold`).

## Developer cheat add_gold (user authorization 2026-11-18)

- Bound like a game cheat: `add_gold` under `[CHEAT COMMANDS]` (the front page's key bindings page lists it as
  「作弊：金幣增加一千（中文化）」). The game files the unknown name as a command with no handler and category
  `0xFFFF` (`0x4D25E0` → `0x4D28A0`), so it never runs anything in the game.
- On the key, the runtime does at the next frame flip (outside its lock) the steps of `0x4AD2F0` to find the type
  908 object, reads property 17 and writes it back 1000 higher, at most 9999 (`cheat_notice.h` `gold_step`).
- Notices (digits do not warm for notices, so Chinese numerals): 金幣增加一千, 金幣已補到上限九千九百九十九,
  金幣已達上限九千九百九十九, 身上要先有金幣才能增加, 金幣增加失敗.
- **The Avatar must already carry some gold.** Creating the type 908 object is not attempted.
- Counters: `gold_requests`, `gold_added`, `gold_full`, `gold_no_purse`, `gold_faults`, `gold_before`,
  `gold_after`.

To check in game: the stats page and the shop show the new amount; save, reload and the amount stays.
