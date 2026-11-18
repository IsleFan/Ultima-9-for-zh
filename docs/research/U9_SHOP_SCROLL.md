# Scrolling a shop list with the wheel

2026-11-18. Static disassembly of the pinned English `u9.exe`, plus the player's screenshots.

## What the player saw

- The ↑ and ↓ arrows at the left of a shop list scroll it one row per click (original behaviour).
- A wheel turn over a shop (wheel paging on) did not scroll. Every item name and the 物品 heading turned into
  magenta dots, the sign of text whose Chinese binding is lost (`U9_MISCTEXT_UI_PATH.md`).

## How a shop works (CONFIRMED by reading the code)

- A shop is a book: the book class (constructor `0x422580`, vtable `0x742C70`) shows a page built from a BOOKS
  entry by `0x42C830` (item rows `#id"name"$price`). The open book is `[0x83D268]`.
- The page's objects form a list from `[book+0x16C]`, linked through `+4`.
- Each row has a purchase-quantity object of vtable `0x742E60` (constructor `0x42D1F0`). Its mouse slot `0x421E00`
  adds 1 to `+0x30` (the quantity) on the ＋ while it is below `+0x34`, and subtracts 1 on the other area. The
  quantity drawn is not refreshed by a direct write, but the total is.
- The list's two arrows are objects of vtable `0x742E80`; `+0x1B8` is non-zero on the down arrow. Their mouse slot
  `0x422470` (thiscall: event, x, y; `RET 0xC`) handles a button-down (event type 2) inside the arrow's rectangle
  (`+0x10`): it counts the rows above the top edge and, when the list can move, shifts every row's `+0x14`/`+0x1C`
  by one row (33 units), then clears `[[+0xC]+0x6C]` so the book is drawn again. Six rows are visible.
- The wheel feature turns a wheel notch into PgUp/PgDn, bound to `book_next_page` / `book_previous_page`. The
  book's key handler (`0x424210`) sends those to `0x42BEA0` / `0x42C2A0`, which turn the book's leaves and build
  the page again. For a shop this rebuild loses the Chinese names (STRONG EVIDENCE: the screenshot after the turn).

## First try (2026-11-18, withdrawn)

It changed `+0x30` of the `0x742E60` object, taking it for the list. That was a row's purchase quantity: the
player saw the total rise (300, available gold −291) while the quantities still showed 0. Withdrawn before release.

## Change (2026-11-19)

`scroll_shop` (`engine.cpp`) finds the arrow for the direction in the open book and calls its mouse slot with a
button-down at the centre of its rectangle, once per notch or press, so the game's own rules decide.

`input_hooks.cpp` asks the runtime (`Options::wheel_scroll`) before turning a plain wheel notch into a page key.
With a shop list open the notch is used up, also at either end of the list, so it never reaches the shop as a page
key. Without a shop list the wheel pages as before.

Counters: `shop_scrolls` (runtime), `wheel_scrolls` (input hooks).

PgUp and PgDn pressed on the keyboard (no Ctrl, Shift or Alt) over an open shop list scroll it the same way:
PgDn down, PgUp up, one row per press, repeating while held. The press and its release are kept from the game, so
the shop is never paged and its names stay Chinese. This works whether wheel paging is on or off. Counter:
`page_scrolls` (input hooks).

## Names turning into dots after a few scrolls (2026-11-19, runtime bug)

**Seen:** after several scroll steps, fast or slow, wheel or the arrows themselves, the item names and the 物品／
價格／數量 heading turned into dots. 總價 and 可用金幣 stayed Chinese. Turning hover names off did not help, so it
is not the wheel and not the synthetic X.

**Cause (STRONG EVIDENCE, from the code):** marked UI text (misctext and shop names) is stripped of its markers
**in place** the first time it is laid out (`Engine::strip_misc`), and its plan segments are registered in
`journal_buffers` (64 slots). Later layouts of the same, already stripped buffer find no marker and register nothing;
the glyph stage keeps finding the old registration by address. The slots were taken **in turn**
(`journal_next++ % 64`), so once 64 new texts had been registered, the registration of text still on screen was
overwritten and its CJK slots drew as the native 0x7F glyph. Each scroll step lays out new text, so a few dozen
steps were enough.

**Fix:** a new buffer takes an empty slot or the **least recently used** one (`journal_slot`). Every successful
lookup by the glyph stage (`journal_locate`) marks the buffer as used, so text that is drawn stays registered.
Counter: `journal.evictions`. This also protects journal pages, book pages and other UI text from the same loss.

An earlier guess (too many steps between two frames) led to a 60 ms limit per step; it did not help and was removed.
