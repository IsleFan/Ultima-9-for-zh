# Loading the runtime without process injection (proxy DLL)

2026-11-15. Static research only. Nothing was built or run, and no game file was changed.

## Why

On 2026-11-15 Microsoft Defender quarantined `U9Cht.exe` as `Behavior:Win32/DefenseEvasion.A!ml`, 3 s after it started the game (see `docs/HANDOFF.md`).

Today `U9ChtLauncher.exe` loads the runtime by classic process injection:
1. create `u9.exe` suspended;
2. `VirtualAllocEx`, `WriteProcessMemory`;
3. `CreateRemoteThread` calling `LoadLibraryW`;
4. a second remote call to `U9ChtInitialize`.

That is the pattern behaviour detection looks for (MITRE ATT&CK T1055, a Defense Evasion technique).

For comparison, the Ultima VIII project is never flagged because it ships ScummVM, an engine of its own. It touches no other process.

Common game mods avoid cross-process injection with a **proxy DLL**: a DLL in the game folder that has the name of a system DLL the game imports. Windows loads it from the game folder first, and it forwards the real calls to the system copy. The GOG `ddraw.dll` in this game folder is itself such a wrapper.

## Facts (CONFIRMED from the files, 2026-11-15)

Static imports of `u9.exe`:

| DLL | Functions | Where it comes from |
| --- | --- | --- |
| `dshow.dll` | CoCreateInstance, CoUninitialize, CoInitialize | **GOG file in the game folder** |
| `DDRAW.dll` | DirectDrawEnumerateExA, DirectDrawCreate, DirectDrawEnumerateA | **GOG wrapper in the game folder** (hash checked by the launcher and the runtime; the runtime hooks into it) |
| `DSOUND.dll` | ordinals 1 and 2 | system (`SysWOW64`) |
| `WINMM.dll` | timeGetTime | system; also imported by ddraw, dshow and glide2x |
| `Quartz.dll` | AMGetErrorTextA | system |
| `KERNEL32`, `USER32`, `GDI32`, `ADVAPI32`, `comdlg32` | — | system, all in **KnownDLLs** (always loaded from System32; a proxy is impossible) |

Loaded at run time with `LoadLibrary`: `DINPUT.DLL` (from `0x4A6940` and `0x4A6ABF`, input set-up), `glide2x.dll` (`0x5A19DF`, 3dfx path only) and `CTL3D32.DLL` (`0x5D7141`).

KnownDLLs on this machine (registry `Session Manager\KnownDLLs`) do not include `dsound`, `dinput`, `winmm` or `quartz`, so a copy in the game folder is loaded first.

System `dsound.dll` (`SysWOW64`, 458,752 bytes) exports 12 functions:
- by name;
- ordinal 1 = `DirectSoundCreate`, ordinal 2 = `DirectSoundEnumerateA`, which are the two `u9.exe` imports;
- 3 `DirectSoundEnumerateW`, 4 `DllCanUnloadNow`, 5 `DllGetClassObject`, 6 `DirectSoundCaptureCreate`, 7/8 `DirectSoundCaptureEnumerateA/W`, 9 `GetDeviceID`, 10 `DirectSoundFullDuplexCreate`, 11 `DirectSoundCreate8`, 12 `DirectSoundCaptureCreate8`.

`u9.exe` entry point: RVA `0x314920` (VA `0x714920`). It starts with `55 8B EC 6A FF` (`push ebp; mov ebp,esp; push -1`), five whole bytes, enough for a jump.

`U9ChtInitialize(Config*)` (`runtime.cpp`) makes no assumption about being called from another process. It checks:
- the image base;
- the hashes of `u9.exe` and `ddraw.dll`;
- the running path;
- that no debugger is attached;
- that `ddraw.dll` is loaded at its expected base.

All of these hold at the entry point when a static import has been loaded, because `ddraw.dll` is a static import too.

## Candidates

| Candidate | For | Against |
| --- | --- | --- |
| **`dsound.dll`** | Static import, so it loads at process start, before the entry point. Only 12 exports, and the game uses 2 by ordinal. Not a KnownDLL. No other module in the process imports it. | All 12 exports must be forwarded with the same ordinals. Sound runs through it, so a forwarding mistake would show as silence (easy to see). |
| `dinput.dll` | 7 exports | Loaded late, by `LoadLibrary` during input set-up. Game code has already run by then, so the hooks would arrive late. |
| `winmm.dll` | Static import | About 180 exports, and also imported by three other DLLs in the folder. Large surface. |
| `quartz.dll` | Static import, 1 function | A large COM DirectShow DLL, which the GOG `dshow.dll` replacement may also need. Risky. |
| `ddraw.dll`, `dshow.dll` | — | GOG's own files. Replacing them changes the wrapper the runtime depends on. |

**Recommendation: `dsound.dll` (STRONG EVIDENCE that it is the best fit; not yet built).**

## Proposed design (needs authorization: AGENTS §13)

1. **`dsound.dll` (new, small).**
   - Exports the 12 names with the same ordinals. Each one jumps to the system `SysWOW64\dsound.dll`, which is loaded with a full path on first use.
   - In `DllMain(PROCESS_ATTACH)` it does almost nothing, because the loader lock is held. It reads one environment variable, `U9CHT_LAUNCH` (the path of a launch file).
   - **Without the variable it is inert.** The game started from Steam, GOG or by hand runs exactly as the original.
   - With the variable, it patches the five entry bytes of `u9.exe` with a jump to its own stub.
2. **Entry stub.** It runs after every DLL is loaded and before any game code, the same moment the launcher uses today.
   - It restores the five bytes.
   - It reads the launch file: game dir, runtime path, log path, mode, allowlist, settings.
   - It builds `Config` with the same code the launcher uses (moved to `common`).
   - It calls `LoadLibraryW(runtime)` and `U9ChtInitialize`.
   - On failure it logs and continues as the plain English game. It never stops the game halfway.
   - Then it jumps to the original entry.
3. **Front page and launcher.**
   - `dsound.dll` is installed into the game folder for the session, with the same receipt, backup and verify as the FLX files, and removed afterwards.
   - `u9.exe` is started with a plain `CreateProcess`, with `U9CHT_LAUNCH` in its environment.
   - No suspended process, no remote memory, no remote thread.
   - `U9ChtLauncher.exe` can stay as the waiting parent that restores the files, or the front page can do it alone.
4. **Session tool.** The same path, with an option to keep the old injection for bisecting.

What does not change: the runtime and every hook inside it, the handshake fields, and the logs.

Risks and open points:
- **Defender may still flag it (UNKNOWN).** The patch of the game's own entry point happens inside the game's process. That is common for mods, but nothing guarantees a verdict. It has to be tried on this machine.
- **A leftover `dsound.dll`** after a crash is inert without the variable, and `recover()` removes it on the next start.
- **The Steam release** starts `u9.exe` through Steam only when the player uses Steam. The front page starts it directly, as today. No change.
- The `Config` handshake fields that the launcher reads back (`state`, `process_id`, `image_base`, `wrapper_base`) would come from the log instead.

## Effort (rough)

- The proxy DLL: about 150 lines, plus a `.def` file with ordinals.
- Moving the settings-to-`Config` code to `common`: small.
- Install and receipt changes in `front_core` and the session tool: small to medium.
- Tests: synthetic tests for the launch file and the receipt, then one session to compare the logs with the injection path.
