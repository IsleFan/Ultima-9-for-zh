# Ultima IX: Ascension Extended Setup (third-party tool) and the keymap file

2026-11-15. Static inspection only. The tool was **not run**. No game file was changed.

## Subject

| Item | Value |
| --- | --- |
| File | `Ultima IX Ascension Extended Setup b6 b2a.exe` (local copy, untracked, not redistributable by us) |
| SHA-256 | `CBED2E7AF3D7BBA4B2C53DCCA0E23A0B598D1A407D877EC00C5524E6E0FEDD0A` |
| Size | 851,968 bytes |
| Version | "v1.0 beta 6 Beta Test 2", 2001-07-22, by Xenerkes Dragon (UDIC), "All rights reserved" |
| Build | 32-bit, MSVC 6 linker, MFC statically linked (`CPropertySheet`), ANSI only |
| Help | `.chm` covers beta 5 only: Performance, Sound, Lighting, Game. It has no page on the key tabs. |

The copy the user pointed to in the original GOG install has the same SHA-256 as the local copy. Only the local copy was read.

## What the tool does (CONFIRMED from resources and strings)

It is an editor for two text files in the game folder: `options.ini` and the keymap named there.

Menus:
- File: Open, Save, Save As, Save & Exit, Exit.
- Configs: three presets (Low End, High End, Geforce), Tooltips, Splash Screen.
- Keyboard: Qwerty or Azerty layout for the key picker.
- Tray icon: Restore, Shutdown.

Eight property pages:

| Dialog | Page | Edits |
| --- | --- | --- |
| 1001 | Performance Settings | clipping planes, camera, culling, textures, particles, dead pool, mesh cache |
| 1002 | Sound Settings | speakers, 3D, 2D, volumes, SFX cache, conversation audio/text |
| 1003 | Lighting Settings | flicker, skip frames, gamma, shading |
| 1004 | Game Settings | tooltips, video mode, rasterizer, LOD, walking speed, language, movie path, **keymap file** |
| 107 | Move Keys | movement, combat, mouse, screenshot, first person, flush music/spells |
| 1005 | Journal Keys | journal, spellbook circles, book pages, hand, inventory, examine, pause, exit |
| 1006 | Engine Keys | camera, clipping planes, main/equipment UI |
| 1007 | Cheat Keys | time, weather, Avatar cheats, spell cheats |

Other facts:
- The keymap file is chosen through `Current_Keymap` in `options.ini` `[Interface]`; the file filter is `*.kmp`.
- Its own settings live in the registry under `Software\Xenerkes\Ultima IX Extended Setup` (splash, start on task bar, keyboard layout).
- It imports `EnumDisplaySettingsA` and `ChangeDisplaySettingsA` to list video modes.
- The tooltip texts for the INI pages are the comments of the game's own `OPTIONS.TXT`.

## The key pages

Each command is one row: a label, an edit box (52 dialog units wide) and a `...` button.

The `...` button opens dialog 154, the key picker:
- title `Key Definition (Qwerty Keyboard)` or `(Azerty Keyboard)`;
- a caption "Key Definiton for <command>" and an edit box "Keys Combinaison";
- about 105 push buttons drawn as a keyboard: function row, main block, navigation block, arrows, keypad, plus Ctrl, Alt, Shift;
- OK and Cancel.

The player builds a combination by clicking the on-screen keys. The tool does not capture a real key press for this (STRONG EVIDENCE: the dialog is only buttons, and there are two fixed layout tables instead of a scan-code lookup).

Key names the tool writes: `escape`, `F1`–`F12`, `PrintScrn`, `ScrollLock`, `pause`, `ctrl`, `alt`, `shift`, `space`, `left`, `right`, `up`, `down`, `CapsLock`, `tab`, `insert`, `pagedown`, `pageup`, `delete`, `end`, `home`, `Keypad0`–`Keypad9`, `Keypad/ . * - +`, `NumLock`, `LeftBracket`, `RightBracket`, letters, digits and punctuation.

Commands the tool knows, by class:

| Class | Count | Commands |
| --- | --- | --- |
| `CKeymap_Movement` | 25 | sidestep_left/right, forward, backward, toggle_movement_speed, jump, climb, enter_combat, defend, target_lock, get_target_lock, abort_targeting, mouse_button1/2, mouse_left/right/up/down, invert_mouse, mouseSensitivity_increase/decrease, take_screenshot, momentary_first_person, flush_music, flush_spells |
| `CKeymap_Interaction` | 30 | activate_hand, activate_inventory, examine, pause, exit_game, open_journal, open_spellbook, open_world_map, journal_load/save/options/stats/history/bestiary, journal_diary_first_page/last_page, book_next_page/previous_page, spellbook_circle_0–8, spellbook_rituals, spellbook_reagents, spellbook_glossary |
| `CKeymap_Engine` | 19 | camera_mode, camera_in/out, camera_way_out, zoom_in/out/reset, yaw_left/right, pitch_up/down, clipping_plane_in/out/reset, middle_plane_in/out/reset, toggle_mainUI, toggle_equipmentUI |
| `CKeymap_Cheat` | 17 | pass/unpass_one_hour, pass/unpass_one_minute, sunrise_sunset, toggle_storms, toggle_sun, toggle_wind, toggle_avatar_fly/fast/invulnerable, avatar_pathfind, recharge_mana, fill_spellbook, spells_0-2, spells_3-5, spells_6-8 |

Sections it reads and writes: `[MOVEMENT COMMANDS]`, `[INTERACTION COMMANDS]`, `[COMBAT COMMANDS]`, `[GENERAL OPTIONS COMMANDS]`, `[CAMERA CONTROL COMMANDS]`, `[ENGINE TUNING COMMANDS]`, `[CHEAT COMMANDS]`, `[CODE TEST COMMANDS]`, `[DEVELOPMENT CONTROL COMMANDS]`, `[COMMENTS]`.

### Gaps in the tool

- **Dead rows (STRONG EVIDENCE).** The Engine page has rows labelled Camera Way In, Exit to New Camera, Exit to Old Camera, Pitch Lock and Toggle Screen Center. The executable holds no command string for them (`camera_way_in`, `exit_to_new_camera`, `exit_to_old_camera`, `pitchlocked`, `toggle_screen_center` are absent), so these rows cannot be saved.
- **One key per command (HYPOTHESIS).** One edit box per command. The game's own `Default.kmp` binds several keys to one command (`q` and `` ` `` to `activate_hand`; arrows and keypad arrows to movement; `tab` and `delete` to `enter_combat`). What the tool does with the extra lines on save was not tested.
- **Keypad arrows.** The tool has no `KeyPadLeft`-style names, which `Default.kmp` uses.
- **Comments and layout on save (UNKNOWN).** Whether the header comment and inline comments survive a save was not tested.
- The cheat page warns that `spells_0-2`, `spells_3-5` and `spells_6-8` reset the inventory to the start of the game and add quest items (the tool's claim; not verified by us).

## What the game itself accepts (from `u9.exe` strings; the parser trace is further down)

The game is the authority, not the tool. Addresses are file offsets in the GOG/Steam `u9.exe`.

Line form, from the shipped `Default.kmp`:

    [SECTION NAME]
    key            = command    ; comment
    mod+mod+key    = command

- `;` starts a comment. The file header warns that a `;` followed by `=` is read as an assignment of the `;` key.
- The header also says: each command can have only one key combination, ESC and F1–F12 are reserved, and Shift or Alt should not modify momentary commands. The first statement conflicts with the shipped file itself, which binds two keys to several commands; how the game resolves that is UNKNOWN.
- Right click (move) and left click (use/attack) are hard coded and not in the file.

Key name table at `0x387400`:
`Backspace`, `Tab`, `Clear`, `Enter`, `KeypadEnter`, `Shift`, `Ctrl`, `Alt`, `Pause`, `CapsLock`, `ESC`/`ESCAPE`, `Space`, `PageUp`, `PageDown`, `End`, `Home`, `Left`, `Up`, `Right`, `Down`, `PrintScrn`, `Insert`, `Delete`, the `KeyPad` forms of the last ten (`KeyPadPageUp` … `KeyPadDelete`), `0`–`9`, `a`–`z`, `Keypad0`–`Keypad9`, `Keypad* + - . /`, `F1`–`F12`, `NumLock`, `ScrollLock`, `; = , - . / `` ` `` \ '`, `LeftBracket`, `RightBracket`.

Modifier prefixes at `0x388xxx`: `CTRL+`, `ALT+`, `SHIFT+`. The shipped file writes them in lower case, so matching is case-insensitive (STRONG EVIDENCE).

Error text: `There is an error in your keymap file.` … `is not a recognized key.`

Command table at `0x387e30`, 98 commands (counted from the registration code, see the parser trace) in 11 categories (Movement, Interaction, Combat, Magic, General Options, Camera Control, H3D Control, Engine Tuning, Cheat, Development Control, Code Test). Commands the game has and the tool lacks:

`exit_to_new_camera`, `exit_to_old_camera`, `pitchlocked`, `toggle_screen_center`, `spin_camera`, `camera_way_in`, `momentary_camera_mode`, `next_mono_screen`, `previous_mono_screen`.

Two bindings are built into the executable next to the reserved keys: `shift+escape = exit_to_old_camera` and `PrintScrn = take_screenshot`.

**A built-in binding screen may exist (HYPOTHESIS).** At `0x3a0100` the game holds `Defined Commands`, the category names, ` = `, `OK`, `Cancel` and `Enter key for `. This looks like an in-game key assignment screen. Whether it can be reached in the retail game is UNKNOWN.

`KEYMAP.NEW` in the game folder is a second shipped keymap ("Audball's preferences" for the keypad). It differs from `Default.kmp` in the movement section.

## Relation to this project

- `scripts/run_p8_0_session.py --keymap` already installs a test keymap as a separate file and points `Current_Keymap` at it for one session, then restores `Options.ini`. `Default.kmp` is never modified.
- The cheat hints in the runtime are enabled only when the active keymap has entries under `[CHEAT COMMANDS]`.
- `U9Cht.exe` already edits `Options.ini` in place while keeping every other byte (`GameScreenWidth`, `GameScreenHeight`). The same rule would fit `Current_Keymap`.

## Parser trace (2026-11-15, static disassembly of `u9.exe`, nothing run)

Addresses below are virtual addresses (image base `0x400000`). All points are CONFIRMED by reading the code unless marked.

### Functions

| Address | Role |
| --- | --- |
| `0x4D23C0` | Load: register commands, add the two built-in bindings, copy the keymap name from `0x98F580`, call the reader |
| `0x4D2A10` | Registers 98 commands, each with the key string `undefined` |
| `0x4D28A0` | Find a command by name or create it; keeps the list sorted by category, then by name |
| `0x4D25E0` | Reads the keymap file |
| `0x4D2030` | Reserved key test |
| `0x4D3C30` | Key string → 16-bit key code |
| `0x4D3E80` | Key code → key string |
| `0x4D3820` | Add a key (by string) to a command's key list |
| `0x4D3430` | Dispatch a key event to a command (called from `0x49AA91`) |
| `0x4D3390` | Find the first command that owns a key code |

Objects: a command holds `+0x0C` name, `+0x10` handler, `+0x14` a list of keys, `+0x24` category. A key holds `+0x0C` its string and `+0x10` its code.

### Reading the file (`0x4D25E0`)

- Path = game folder (`0x98F434`) + keymap name. The default name is `default.kmp`.
- It calls `GetPrivateProfileSectionA` once for each of 11 section names: Movement, Interaction, Combat, Magic, General Options, Camera Control, H3D Control, Engine Tuning, Cheat, Development Control, Code test (each followed by ` Commands`). Any other section, such as `[COMMENTS]`, is never read.
- **The section buffer is 4,096 bytes.** A longer section is cut off by Windows.
- The section a line sits in does not matter. A command is looked up by name in one list for all sections.
- Per line:
  - every space and tab is dropped, wherever it is;
  - `;` starts a comment, except in `;=x` (not `;==`), which binds the `;` key;
  - the key is the text before the first `=`; a line starting with `==` binds the `=` key;
  - the command is the rest.
- **No length checks.** The stack buffers are 64 bytes for the stripped line, 32 for the key and 32 for the command. A line with no `=` makes the scan run past the line. A writer must keep every line inside these limits and must never write a non-comment line without `=`.
- Command names are compared without regard to case (`0x711C10` is a case-insensitive compare).
- A command name the game does not know is added as a new command with **no handler**. There is no message; the key simply does nothing.

### Key strings (`0x4D3C30`)

1. The string `undefined` gives code 0.
2. The string is upper-cased in place (`0x713900`), so key names and modifiers are case-insensitive.
3. `SHIFT+`, `ALT+` and `CTRL+` are found anywhere in the string and removed. They set `0x100`, `0x200` and `0x400`.
4. A remaining single shifted symbol is turned into Shift plus its base key. There are 21 such pairs, for example `!`→`1`, `:`→`;`, `+`→`=`, `~`→`` ` ``, `{`→`[`.
5. The name is looked up in a table of 112 entries at `0x788E00` (22 bytes each: name, virtual key at `+0x14`, extended flag at `+0x15`). The extended flag sets `0x800`.
6. **An unknown name shows a Windows message box** ("There is an error in your keymap file. X is not a recognized key."), gives code 0, and loading goes on. The game is not stopped.

The extended flag is real: `Left`, `Up`, `PageUp`, `Insert`, `Delete` and the like are the grey keys, and `KeyPadLeft` … `KeyPadDelete` are the keypad keys with NumLock off. They are different codes. `Keypad0`–`Keypad9` are the keypad with NumLock on. `KeypadEnter` and `NumLock` are also extended.

### Reserved keys (`0x4D2030`)

- The whole key string is compared with `ESC`, `ESCAPE`, `F1` … `F12`. Such a line is skipped.
- The message for it ("The key combination … is reserved") goes through `0x556480`, which is an empty function in this build. **Nothing is shown.**
- The compare is on the whole string, so `ctrl+f1` passes the test. Whether a modified function key then works in game is UNKNOWN.
- The list also holds `PrintScrn`, but the compare is against the upper-cased string, so that entry never matches.

### Answers to the three questions

1. **Several keys for one command: supported.** Each command keeps a list of keys, and every matching line adds one.
2. **One key for several commands: only one fires.** Dispatch walks the commands in list order and calls the first one that owns the code. The order is category (0 Movement, 1 Interaction, 2 Combat, 4 General Options, 5 Camera, 7 Engine Tuning, 8 Cheat, 10 the four spell cheats), then name. The file order plays no part.
3. **Case does not matter**, for sections, key names, modifiers and command names.
4. **An unknown key name does not stop the game.** It shows a message box and the line has no effect.

### Other findings

- Two bindings are added before the file is read and cannot be removed through it: `shift+escape = exit_to_old_camera` and `PrintScrn = take_screenshot`.
- Dispatch ignores Shift, Ctrl and Alt pressed alone, so a modifier cannot be a binding by itself.
- **`abort_targeting` does not exist in `u9.exe`.** The string is absent and no command of that name is registered. The line `a = abort_targeting` in the shipped `Default.kmp` therefore creates a command with no handler (STRONG EVIDENCE that the A key does nothing; not checked in game). The tool lists this command too.
- The 98 commands registered at `0x4D2A10` are the list above without duplicates; `spin_camera`, `momentary_camera_mode`, `next_mono_screen` and `previous_mono_screen` are among them.
- **More commands are registered elsewhere**, through the same `0x4D28A0`:
  - `0x5B21A8`: `toggle_movement_speed`, `shift_sidestep_left`, `shift_sidestep_right`, `shift_forward`, `shift_backward` (category 0);
  - `0x497FD0`: `shortcut_beltF1` … `shortcut_beltF12` (category 1);
  - `0x49D830`: `log_camera_items` (category 10).
  That is 116 in all. The front page offers 97 of them: the 98, minus the two mono screen commands, plus `toggle_movement_speed`. Bindings of any other name found in a file are written back unchanged.
- Several commands share one handler (`journal_*`, `spellbook_*`, `pass/unpass_*`, `spells_*`), so the handler must receive which command fired.
- An in-game binding screen exists in code: `0x5CF543` adds the pressed key to the selected command and `0x5CF2AE` calls the file reader. Whether the retail game can open it is still UNKNOWN.
- The game imports `WritePrivateProfileStringA`. Whether it ever writes the keymap was not traced.

## Re-implementation (2026-11-15)

The key pages are rebuilt as the 按鍵設定 page of `U9Cht.exe`: `runtime/U9ChtFront/keymap.h` and `keymap.cpp`, tested by `runtime/tests/keymap_tests.cpp`. See `runtime/U9ChtFront/README.md`. The `Options.ini` pages of the tool are not rebuilt (the user's decision).

LOCAL-ONLY check with `U9_GAME_DIR` set: the shipped `Default.kmp` reads as 69 bindings, 1 unknown command (`abort_targeting`), 0 skipped lines and 0 shared keys, and survives write and read unchanged.

## Keyboard movement: one direction at a time (2026-11-15, static only)

The four movement handlers (`sidestep_left` `0x5B1610`, `sidestep_right` `0x5B1870`, `forward` `0x5B1AD0`, `backward` `0x5B1D30`) share one shape. They write to the movement state at `[0x131F668]`:

- `+0x14` is the sideways step (−1 left, +1 right); `+0x18` is forward/back (+1 forward, −1 back); `+0x1D` is "a movement key is held".
- The direction comes from modifier bits, the same scheme as the hard-coded right-click move (Ctrl = left, Alt = right, Ctrl+Alt = back). `sidestep_left` forces the Ctrl bit, and the others work the same way.
- **On key down**, nothing happens when `+0x1D` is already set. A second movement key pressed while the first is held is ignored.
- **On key up**, `+0x1D` is cleared and both `+0x14` and `+0x18` are reset. Releasing either key stops the Avatar.
- Movement keys do nothing while `+0x1C` or `+0x50` is set. What sets them is UNKNOWN. A stuck value there would block movement in a way that looks like "the Avatar cannot move" (HYPOTHESIS; compare the unexplained no-move reports in `docs/HANDOFF.md`).

Result: the keyboard moves in one of four directions only. Diagonal movement (forward plus a side step) is not possible in the original game (CONFIRMED from the code; not tried in game). Adding it would need a new runtime hook (AGENTS §13).

Session 57 (2026-11-15, session tool, the user's keymap from the front page with WASD added): arrows, WASD, right-click move, B, J and Alt+S all worked. The keymap written by the front page is therefore read correctly by the game. The no-move seen in the front-page run the same day is not explained by the keymap.

## Changing bindings while the game runs (2026-11-15, static only)

- The game reads the keymap once, at start: `0x4966FC` calls the loader `0x4D23C0`. The keymap object is kept at `[0x84F80C]`; its file name is at `+0x14`, copied from `0x98F580`.
- A handler-shaped function at `0x49C140` (`ret 0x20`, acts on key down) calls the reader `0x4D25E0` again. Nothing refers to it: no command is registered with it in this build (CONFIRMED: no reference to its address). It looks like a developer "reload keymap" command that was left unregistered.
- The reader only adds keys (`0x4D3820`). It never removes one. Calling it again would keep every old binding (CONFIRMED from the code).
- So a live change needs, on the game's own thread: empty each command's key list, add the two built-in bindings again, set the file name, then call `0x4D25E0`. That is a new runtime intervention calling game functions, and needs explicit authorization (AGENTS §13). Not implemented.

## Still open

1. Whether the in-game binding screen is reachable, and whether the game writes the keymap file.
2. Whether a modified function key (`ctrl+f1`) works in game.
3. Which cheat commands work in the retail build. `fill_spellbook`, `recharge_mana`, `toggle_avatar_fast/fly/invulnerable` and `pass_one_hour` are CONFIRMED working (sessions 30–33); the others are untested.
4. Whether `GetPrivateProfileSectionA` counts comment lines toward the 4,096 bytes. A writer should keep each section well below that either way.
