# Opening the journal at its table of contents

2026-11-18. Static disassembly of the pinned English `u9.exe` (capstone 5.0.7, local helper). The player asked for
a key that goes straight to the journal's table of contents (the page with 狀態, 歷史, 日誌筆記, 生物總誌 and the
options, save/load, credits and quit buttons). No game command does this.

## The journal commands (CONFIRMED by reading the code)

Registered at `0x4D2BB0` and following:

| Command | Handler |
| --- | --- |
| `open_journal` | `0x42DDE0` |
| `journal_stats`, `journal_history`, `journal_diary_first_page`, `journal_diary_last_page`, `journal_bestiary`, `journal_options`, `journal_save`, `journal_load` | `0x42DE80` (shared) |

The key dispatcher `0x4D3430` calls a handler with ECX = the command's name and the 32-byte key event by value
(`RET 0x20`).

`0x42DE80`:

1. Acts only when event `+0x14` has bit `0x20` and not bit `0x10000`.
2. Compares the name (case-insensitive, `0x711C10`) with the table at `0x782D78`: `journal_stats` …
   `journal_load`. The matching index selects a page from `0x782DA0` (1, 2, 3, 3, 4, 5, 6, 6) and a sub-page from
   `0x782DC0`.
3. **A name that is not in the table leaves page 0 and sub-page 0.**
4. Returns without doing anything while a book is open (`[0x83D268]` non-zero) or while `0x5AE8D0(8)` refuses.
5. Builds the book (`0x422580`, 0x1CC bytes), stores it in `[0x83D268]` and calls `0x42A570(page, sub)`.

`open_journal` (`0x42DDE0`) does the same without the page call. The book's constructor itself opens the page
last shown for that book type (`[0x838FA8 + type*4]`, sub-page `[0x839260 + type*4]`). This is why **J goes back
to the last page, not to the contents**.

STRONG EVIDENCE that page 0 is the table of contents: the named pages are 1–6, and the contents page links to
all of them. Not yet confirmed in game.

## What Esc does now (CONFIRMED by reading the code)

The world key handler (`0x49A9CA` onward) handles Esc before the keymap. It:

- closes the modal input entry of type `0xE` that `0x5EFFB0` finds in the list at `[[0x84F810]+0xC0]` (a book or
  similar window), through `0x5EC8B0` and its vtable `+0x14`;
- when `[[[0x850228]+0xC0]+0x78] == 1`, leaves that mode instead (what the mode is: UNKNOWN);
- closes the type `0xE` entries of its own list;
- then passes the key to the keymap. Esc and Escape cannot be bound in a keymap (reserved, `0x4D2030`).

When nothing is open, Esc does nothing visible.

## Implementation (user request 2026-11-18; player switch FeatureEscContents, on by default)

`runtime/U9ChtRuntime/engine.cpp`: `on_escape`, `esc_blocker`, `open_contents`.

1. When Esc (without Ctrl, Shift or Alt) arrives, before the game sees it, the runtime checks whether Esc would
   close something. Esc is left alone when:
   - a book is open (`[0x83D268]`), reason 1;
   - any entry of type `0xE` is in the list at `[[0x84F810]+0xC0]`, reason 2. This is wider than the game's own
     test, which also matches the entry's owner; the list is walked by its links, not through its cursor;
   - the mode above is 1, reason 3;
   - a conversation line or choice menu is alive (only known when FeatureClickSkip is on), reason 4;
   - something could not be read, reason 5.
2. Otherwise, at the next frame flip (outside the runtime lock, since building the book allocates through the
   runtime's hooks), it calls `0x42DE80` with the name `journal_contents` and an event with `+0x14 = 0x20`. The
   name is not in the table, so the handler opens page 0. The game's own refusals still apply.
3. A request older than 250 ms is dropped. While a movie plays the game does not flip, and an Esc that skipped the
   movie must not open the journal afterwards.

Esc still closes the journal when it is open: the runtime sees the open book and does nothing.

No code in the game is patched. Counters: `esc_presses`, `contents_requested`, `contents_opened`,
`contents_refused`, `contents_faults`, `contents_stale`, and `esc_reasons` (the last 8, newest last).

## To check in game

1. Esc with nothing open shows the table of contents.
2. Esc in the journal, the spellbook, a book, the backpack, the map, a conversation and a shop closes it as before
   and does not open the journal.
3. Esc that skips a movie does not open the journal.
