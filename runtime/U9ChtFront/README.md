# The release's front page (U9Cht.exe)

`U9Cht.exe` is the page a player opens instead of `u9.exe`. It finds the game, holds the player's settings,
applies the Chinese resources for one play session, and starts `U9ChtLauncher.exe`. When the game ends it
puts the original files back.

## What the page holds

| Section | Contents | Stored in |
| --- | --- | --- |
| 遊戲位置 | Folder holding `u9.exe`. Found automatically from the GOG registry entries, the Steam libraries (`libraryfolders.vdf`), uninstall entries and common GOG folders, or chosen by hand. When more than one install is found, 切換版本 buttons let the player choose. Checks the `u9.exe` hash (GOG or Steam release), `ddraw.dll` and the four static files. | `front.ini` `[game] dir` |
| 中文化資料 | State of `data/` (the `build_localization.py` output) and its plan count. | — |
| 強化功能 | Hover names (with the delay), wheel paging, Ctrl+wheel zoom, movie subtitles, status numbers, floating numbers, enemy bars, and 跳躍鍵自動攀爬 (the jump key climbs when the game finds something to climb; off by default; `docs/research/U9_JUMP_CLIMB.md`). | `u9cht.ini` `[features]` |
| 畫面與字型 | Chinese font, journal letter and line spacing, resolution (keep, 800×600 or 1600×1200). | `u9cht.ini` `[font]` and `[journal]`; `front.ini` `[display]` |
| Buttons | 開始遊戲, 按鍵設定, 說明 (`說明.txt`), 回報問題 (GitHub Issues), 結束. | — |
| 按鍵設定 (its own page) | Key bindings for 97 game commands and 3 runtime commands in six tabs, cheats included. | `<game>\U9CHT.kmp`; `Options.ini` `Current_Keymap` |

How the files are handled:
- Both INI files are written as UTF-16LE with a BOM, so font paths may hold Chinese.
- `U9ChtLauncher.exe` validates `u9cht.ini` strictly and passes `[features]` to the runtime in `Config`.
  This is protocol 3: `features`, `hover_delay_ms`.

**Resolution.** The runtime draws Chinese at any 4:3 size from 480 to 1536 lines, with glyphs of height/30
pixels (`common/screen.h`, `docs/research/U9_RESOLUTIONS.md`). Other sizes make it end the game safely with
error 18.

The page therefore:
- offers 800×600, 1024×768, 1280×960 and 1600×1200;
- refuses to start while the game itself is set to another size and the page is set to keep it.

Choosing a size changes only these values; every other byte of both files is kept:
- `GameScreenWidth` and `GameScreenHeight` in `Options.ini`;
- `display=WxH` in `dxcfg.ini`.

The change is persistent, like changing it in the game's own options.

The Steam release ships `dxcfg.ini` with `display=desktop`, which scales the game to the desktop. That line names no size, so it is left as it is and only `Options.ini` changes.

## Releases: data built from patches

A release ships `data/patches/<name>.u9cp` instead of `data/static/<name>` (the built FLX hold the game's English
text). `prepare_static` (called by `install`) builds each missing or wrong file from the player's original, taken
from the game folder or, while an install is in place, from `backup/`, and checks it against the manifest. See
`docs/release/PATCH_FORMAT.md`.

## Key bindings page

`keymap.h` and `keymap.cpp` read and write a keymap the way `u9.exe` reads it. The rules were traced in the game's code; see `docs/research/U9_EXTENDED_SETUP_TOOL.md`.

On the page:
- one row per command, each bound key shown as a small button; clicking it removes the key;
- ＋ waits for a real key press (with Ctrl, Alt, Shift) and adds it; Esc cancels;
- a command may hold up to four keys;
- a key shared by two commands is drawn red on the command the game will not run;
- 載入原版預設 loads the bindings of `Default.kmp` into the page.

What it refuses:
- Esc and unmodified F1–F12 (the game skips them);
- Shift, Ctrl or Alt alone;
- keys the game's table does not name, such as the keypad `/`, which Windows reports as an extended key.

Saving:
- writes `U9CHT.kmp` beside `u9.exe`;
- sets `Current_Keymap=U9CHT.kmp` in `Options.ini` and keeps every other byte of that file;
- never modifies `Default.kmp`;
- keeps every line inside the game's unchecked buffers (64, 32 and 32 bytes) and every section under 3,800 bytes;
- writes back, unchanged, bindings of command names the page does not offer.

The change is persistent, like the resolution.

Binding a cheat puts a line under `[CHEAT COMMANDS]`, which also turns on the runtime's on-screen cheat notices.

**While the game runs**, the page stays usable. Bindings saved then take effect in the running game about a second after the player returns to it, once every key is released. The runtime reloads the keymap with the game's own routines; see `docs/research/U9_EXTENDED_SETUP_TOOL.md`, "Changing bindings while the game runs".

When that game ends, the page points `Current_Keymap` at `U9CHT.kmp` again, in case the game wrote its own value back on the way out.

**In game, the key of 叫出按鍵設定頁** (tab 一般; default Ctrl+Shift+K) brings this page up:
1. The runtime posts `OpenKeyBindingsMessage` (`protocol.h`) to the window of class `U9ChtFront`, with the game's window in `wParam`. First it calls `AllowSetForegroundWindow` for the page's process.
2. The page restores itself, opens the key bindings page and comes to the front.
3. 返回 becomes 回到遊戲, which hands the foreground back to the game and minimizes the page again.

Details:
- The page ignores the message unless it started the running game.
- 叫出按鍵設定頁 is the runtime command `open_key_bindings` (2026-11-26; before that Ctrl+Shift+K was fixed and could not be bound). The game files it with no handler, like `show_takeable`.
- With no key of its own it uses Ctrl+Shift+K, unless another command binds that key (`keymap.h` `page_keys`). The page shows that default in the row, so saving writes it to `U9CHT.kmp`.
- The runtime reads its keys at start and on the live reload; the extended bit is not compared.
- Without a front page (session tool), the runtime logs `key_bindings_hotkey` with `"front_page":false` once.
- In game (2026-11-15, front page, full screen): the hotkey brought the page up every time. 回到遊戲 did nothing at first, because the full-screen game had minimized itself. It now restores the game window first and falls back to the Alt+Tab switch; with that it works (CONFIRMED). Saving during that run was not tried, so the live reload through the page itself is still untested (session 60 replaced the file directly).

In game:
- sessions 57 and 58: a keymap saved by this page works;
- session 60: the live reload works (CONFIRMED). That test replaced the file directly, not through the page.

## Steam release

The Steam `u9.exe` (app 3067560) is the GOG executable with an Electronic Arts Authenticode signature appended. This was CONFIRMED on 2026-11-12:
- after removing the signature, the bytes are identical;
- `ddraw.dll` and the four static files are identical.

Every hooked address is therefore the same. `version_manifest.h` accepts both hashes (`known_exe`, `exe_edition`). The Steam copy was played in game through this page on 2026-11-12.

## Install model

`front_core.cpp` uses the same model as `scripts/run_p8_0_session.py`.

**Before the game starts:**
1. Each `data/static/*.FLX` must match `resource_output_sha256` in its build manifest.
2. Each original in `<game>/static` must match `source_game_sha256`.
3. The originals are copied to `backup/` and verified.
4. `backup/installed.txt` is written before the first original is replaced.
5. The Chinese files are copied in and verified.

**When the game ends:** `recover()` puts each original back and verifies it. It runs twice:
1. `U9ChtLauncher.exe` runs it as soon as the game exits, because the page passes `--restore-backup <backup>`. It also runs on the launcher's failure paths, except while a resumed game may still be running.
2. The page runs it again when it sees the launcher exit. With the receipt already gone, this does nothing.

The launcher's run covers a page that is gone by then: on 2026-11-15 Microsoft Defender quarantined `U9Cht.exe` while the game was running, and the player's files stayed replaced. The session tool does not pass `--restore-backup` and restores on its own.

`recover()` also runs when the page starts, which resolves a session cut short by a crash or a power loss.

A file that matches neither the original nor the installed hash has been changed by something else:
- it is never overwritten;
- its backup is kept;
- the page reports it.

## How the runtime is loaded

The page starts the launcher with `--load proxy` (`U9Cht.exe --load inject` selects the old injection path):
- the launcher copies `U9ChtProxy.dll` into the game folder as `dsound.dll` for the session;
- it starts `u9.exe` normally;
- the proxy loads the runtime at the game's entry point;
- when the game ends the launcher removes `dsound.dll` again.

There is no suspended process, no remote memory and no remote thread. See `docs/research/U9_PROXY_DLL_LOADING.md`.

A `dsound.dll` the player already has in the game folder is never overwritten: the page reports it and does not start the game. A copy of ours left by a session that was cut short does nothing when the game is started any other way, and is removed when the page next starts.

## Folder layout

Everything sits beside `U9Cht.exe` unless overridden on the command line:

    U9Cht.exe, U9ChtLauncher.exe, U9ChtRuntime.dll, U9ChtProxy.dll   (--runtime DIR)
    data\        build output: static\*.FLX, runtime\allowlist.u9ct, movies.tsv, build-manifest.json   (--data DIR)
    fonts\       default font: the first .ttf/.otf/.ttc
    u9cht.ini, front.ini, backup\, logs\              (--base DIR)

## Tests

- `U9ChtFrontCoreTests` (ctest) runs only on synthetic files. It covers:
  - manifest parsing;
  - Steam library parsing and editions;
  - settings round trip and clamping;
  - resolution edits;
  - install, recover, a foreign change, version mismatch and damaged data.
- `U9ChtKeymapTests` (ctest) covers the key bindings core on synthetic text: key names, the game's line rules, write and read back, shared keys, saving beside a synthetic `Options.ini`. With `U9_GAME_DIR` set it also checks the game's own `Default.kmp` (LOCAL-ONLY, skipped otherwise).
- `U9Cht.exe --screenshot FILE.bmp` draws the page once and exits. Add `--page keys:N` for tab N of the key bindings page.
- The page draws with Microsoft JhengHei UI. Where it is missing (Wine, CrossOver on Mac), it registers the first font in `fonts\` for itself only (`FR_PRIVATE`) and uses that. `--ui-font bundled` forces the fallback on Windows, for checking it with `--screenshot`.

## Icon

`u9cht.ico` (a gold ankh, resource 1 in `front.rc`) is drawn by `scripts/make_icon.py` from plain geometry. It
holds no game art.

## Not yet done

Building `data/` on the player's computer. It needs `build_localization.py`, the player's own game files and a
bundled Python. Today `data/` is a build made beforehand on the developer's machine. Built FLX files hold the
player's English text and must never be distributed.
