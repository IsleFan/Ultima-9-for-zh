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
