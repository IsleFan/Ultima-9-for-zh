# Crash: loading a save after dying with a target locked

Status: **CONFIRMED native bug** (original English `u9.exe`, nothing injected). Recorded only; not fixed.

## Steps

1. Lock an enemy with `T` (`target_lock`).
2. Die while the target is locked.
3. Load a save.

The game crashes with an access violation (0xC0000005) during or right after the load.

## Evidence

| Run | Setup | Target locked | Result |
| --- | --- | --- | --- |
| Original exe, 2026-11-15 (1st) | private Steam copy, no injection | no | no crash, exit 0 |
| Original exe, 2026-11-15 (2nd) | private Steam copy, no injection | yes | **crash** (bash: segmentation fault) |
| session 51 | localization, new overlay features off | no | no crash |
| sessions 38, 40, 41, 45, 52, 53 | localization, overlay features on | yes (testing the target bar) | crash |
| session 54 | localization, overlays compute but draw nothing | not locked / locked | no crash / **crash** |

Executable hash on the original runs: `339a5ba8…` (the Steam `u9.exe`); `static/text.flx` was the original (`378d0bf9…`).

The runtime crash log (vectored handler, log only) recorded these faulting points:

- `u9.exe+0x1E0C15` (`0x5E0C15`) most often: a doubly linked list tail insert writes through a corrupt tail node (`[eax+4]`).
  - The list belongs to an object at `+0x118` (`0x55FA00`, called from `0x4CBB29`).
  - Stack: `0x55FA40`, `0x4CBB2E`, `0x4C9B6C`, `0x574EF1`.
- Also `u9.exe+0x15FBEB` and `u9.exe+0x1A7263`.

The varying addresses suggest memory that was freed during the load is used afterwards.

## Notes (HYPOTHESIS)

- The combat manager `[0x98F324]` keeps the lock: `+0x100C` is the lock flag and `+0x1008` the entry index.
  - Each entry's `+0xC` points into the stats record table `[0x98F31C]`.
- Loading frees and re-allocates that table (`0x4F2270`).
- A lock that survives the load would then point into freed memory. This fits the evidence but has not been traced.

## Consequences

- The localization is not the cause. The earlier "no heap use in overlays" change (`e6dc102`) stays: it is harmless.
- Help text for players should say: **release the target lock (`T` / `A`) before loading a save, or restart the game after dying with a target locked.**
- A possible later fix would be a minimal hook that clears the lock before a load. That needs explicit authorization (AGENTS §13).
