# Known issues seen in play

Problems the player hit that are not fixed yet. Each entry says what was seen, what the logs show and what is
still unknown. Evidence levels follow AGENTS.md §7.

## 1. "Unable to create Primary Surface in Video Memory!" while taking a screenshot (2026-11-18)

**Seen:** while playing full screen at 1600x1200, the player tried to take a screenshot. The game showed a
message box titled `Ultima IX:Ascension Error!` with the text `Unable to create Primary Surface in Video
Memory!` and then quit.

**Session:** `front-release/logs/20261118-214142*` (local only).

- `screen_size`: 1600x1200, em 40. The Direct3D overlay (`gpu_overlay`) was on.
- The game ran for about 17 minutes (87,632 frames) before the error.
- The last `runtime_counters` show `errors 0`, `gpu_fallbacks 0`, `gpu_resets 0`, `gpu_restore_failures 0`,
  `gpu_last_hresult 0`. The Chinese runtime recorded no failure of its own.
- The launcher log reports the exit code `3221226525` = `0xC000041D` (`STATUS_FATAL_USER_CALLBACK_EXCEPTION`):
  the process ended while it was inside a window-message callback, which fits a crash after the message box.

**Classification:**

- CONFIRMED: the message comes from the game. It is not text from the Chinese runtime.
- STRONG EVIDENCE: the counters above show the runtime did not fail before the error.
- HYPOTHESIS: the screenshot tool (or the key it uses) took focus away from the full-screen DirectDraw game, or
  switched the display mode. When the game came back it tried to create its primary surface again and failed. The
  original game is known to handle Alt+Tab badly in full screen. The overlay keeps D3D resources (an atlas
  texture) in video memory. Whether that adds to the problem is UNKNOWN.
- UNKNOWN: which screenshot tool or key was used; whether the same thing happens with the overlay turned off
  (`gpu_overlay.off`), in windowed mode, or in the original game without the Chinese runtime.

**Workaround for now:** take screenshots in windowed mode (front page option 視窗化).

**Next steps (not started):** find out the tool and key; try to reproduce it with and without the overlay, and in
the plain original game. Only if the overlay is at fault: release and rebuild its texture when focus is lost.

## 2. The Avatar cannot sit in the gypsy's chair at the start of a new game (2026-11-18, fixed)

**Seen:** in a new game, a left click on the chair in front of the gypsy makes the Avatar walk towards it, but the
walk stops and the Avatar never sits, so the conversation that continues the story cannot start. The player used
the release build through GOG Galaxy on the GOG install.

**Isolated by the player:**

- The English game (`u9.exe` started directly) sits normally.
- With 點擊任意處跳過對白 (FeatureClickSkip) off: still fails.
- With 游標停留顯示名稱 (FeatureHoverNames) off: **sits normally** (CONFIRMED).

**Cause (STRONG EVIDENCE):** the hover feature presses a synthetic X once the cursor has rested on something for
the hover delay (200 ms here). After the click the cursor usually rests on the chair while the Avatar walks, so the
X arrives during the walk and the game drops the walk-and-sit action.

**Fix:** `input_hooks.cpp`. After a real mouse button press, no hover starts until the cursor has moved more than
8 pixels from where the button was pressed (`click_hold`, counter `click_holds`). Moving the cursor brings the names
back. Not covered: a player who moves the cursor away and lets it rest again while the Avatar is still walking.

**Verified (2026-11-18, player, front page):** hover names on, a click on the chair with the mouse left still: the
Avatar sits; moving the cursor afterwards shows names again.

## 3. Hover names type X into a text prompt (2026-11-19, fixed)

**Seen:** with 游標停留顯示名稱 on, at a shrine's mantra prompt (「現在說出同情的祈禱詞(Mantra)。」) the input field
filled with X whenever the cursor rested, more after each mouse move.

**Cause (CONFIRMED by the code and the fix):** the hover presses and repeats a synthetic X key-down. The mantra is
read by `CheckInputProcessor` (vtable `0x754B70`, slot `0x61A1D0`): it builds an input line (constructor `0x41CA30`,
vtable `0x742560`) holding an edit field (constructor `0x5C83B0`, vtable `0x7533E8`). The field's key slot `0x5C8B10`
takes key-down events (flag `0x20`) while the global focus `[0x1325A68]` is the field and types the key, so each
synthetic X became a letter. The focus is set by `0x5C8E40` and cleared by `0x5C8EDC` and by the field's destructor
`0x5C8500`.

**Fix:** `engine.cpp` `hover_allowed()`: no hover starts, and an active one is released, while `[0x1325A68]` is not
zero (counter `hover_text_blocks`). Read only; no new hook. The hover also pauses while a conversation line or a
choice menu lives (`hover_talk_blocks`; the talk line and menu hooks now install with either FeatureClickSkip or
FeatureHoverNames).

**Wrong leads (kept so they are not tried again):**
- The prompt is not a conversation line while the field is open: the line from `0x617A02` is created only after the
  answer (the result text).
- The text box of vtable `0x742C30` (constructor `0x420590`, book page builder `0x426A60`) is a different box, most
  likely the save name; no book is open at the mantra prompt.
- The line array `0x838F20` (count `[0x838F98]`) stayed empty during the prompt.

**Verified (2026-11-19, player, front page):** hover names on, mantra prompt with the mouse moved and at rest: no X
typed; `hover_text_blocks` 2374, runtime `errors` 0 (`front-release/logs/20261119-235004*`, local only).

Not checked: the save name box with hover names on.

## 4. Random crashes in long Windows sessions (2026-11-26, open)

**Seen:** during the Windows check of the 0.1.2 release (private Steam copy, front page, windowed 1600x1200), three of
five localized sessions ended in a crash of `u9.exe`. The player feels that the longer the session, the likelier it is.

| Session (`release-test/U9CHT-0.1.2-internal/logs/`, local only) | Length | Features | Result |
| --- | --- | --- | --- |
| `20261126-202708` | ~16 min | all on | crash `u9.exe+0x19CEAC`, 12 s after the 3rd Ctrl+Shift+K |
| `20261126-214607` | ~4 min | all on | crash `u9.exe+0x4E516` in combat |
| `20261126-215503` | ~2.5 min | combat features off | no crash |
| `20261126-220003` | ~8.5 min | combat features off | crash `u9.exe+0x94A19` |
| `20261126-223311` | ~18 min | all on | no crash |

"Combat features" are status numbers, floating numbers and the enemy bar. The original game (`u9.exe` started
directly, same copy and save) ran 18 minutes without a crash in one session.

**What the crashes show (CONFIRMED by the runtime's `exception` events and disassembly):**

- `0x59CEAC`: a COM call through `[esi+0x3FC3B4]`, which is null. The same address and stack crashed the
  2026-11-18 session of issue 1 ("Unable to create Primary Surface"), before the Mac fixes and the icon existed.
  Both followed a change of focus.
- `0x44E516`: reads `[[0xAE9100]+0xA4]` with `[0xAE9100]` null. The global is written only at `0x4358DA` and
  `0x435A0F`, from `[obj+0x114]`. The runtime never writes it; it only reads other fields under `[0x850228]`.
- `0x494A19`: reads a garbage address (`0x8658A9A8`).

**Memory (sampled every 10 s from outside, `p10-misc/vanilla-mem/`, local only):** `u9.exe` is not large address
aware (2 GB). Highest virtual size: original 1.26 GB, localized 1.31 GB; highest private bytes 0.73 / 0.79 GB.
Neither grows steadily.

**Classification:**

- STRONG EVIDENCE: running out of address space is not the cause.
- UNKNOWN: whether the localization causes or adds to the crashes. Three crashes in five localized sessions
  against one clean original session is too little to tell; the original game is not known to be stable either.
- HYPOTHESIS: different crash sites with null or garbage pointers fit memory written wrongly earlier, or the
  game's own fragile state handling (focus changes, issue 1).
- Ruled out: the three combat features alone (a crash with them off).

**Next steps (not started):** more original sessions of the same length to get its crash rate; if the original
stays clean, turn features off in halves, or compare with the session tool and the older injection path.

## 5. The game window cannot be moved in windowed mode (2026-11-26, not fixed)

**Seen:** in windowed mode the cursor turns into the game's cursor as soon as it enters the window and cannot reach
the title bar, so the window cannot be dragged. Win+arrow keys and Alt+Space (window menu) did nothing.

**Measured (read-only probe from outside, `GetClipCursor`/`GetCursorInfo` every 250 ms, local log
`p10-misc/vanilla-mem/cursor-20261126-230741.csv`):** while the game is in front, the system cursor is clipped to
exactly the game's client area and hidden; when another window is in front the clip is released, and it returns
when the game is in front again (CONFIRMED).

**Who clips:** the game's own clip routine `0x49E110` (it calls `ClipCursor` with the client rectangle at `0x49E1F5`)
starts with `ret` (`C3`) in the GOG/Steam executable, so it never runs (CONFIRMED). The bundled GOG ddraw wrapper
imports `ClipCursor`, `ShowCursor` and `SetCursorPos`, so it most likely does the clipping in windowed mode
(STRONG EVIDENCE). The original game uses the same wrapper; the localization is not involved.

**Possible fixes (not started; the user chose to only record it):**
- The front page moves the game window from outside while it runs (centre / left / right / nudge) and remembers
  the place for the next start. No hook.
- Holding a key releases the clip so the title bar can be dragged. Needs a hook on the wrapper's `ClipCursor`
  (new hook: explicit authorization required), and may fight a wrapper that clips again every frame.
