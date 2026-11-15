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

## Workaround in the runtime (authorized by the user 2026-11-15; verified in session 56)

When the Avatar's HP reaches 0 with the lock on, the runtime calls the game's own clear routine `0x4F90C0` once. It
is a thiscall on `[0x98F324]` with argument 0 (no sound). No new hook is involved: the call is made from the existing
flip hook, outside the runtime lock (calling it under the lock deadlocked the game in session 55: the routine frees the marker through the hooked free).

- The routine unmarks the entry: `0x4F91E0` removes the red target marker object `[P+0x1024]` from its scene list
  through `0x55FA60` and deletes it. The crashes write into that same list family (`0x55FA00` / `0x5E0C00`).
- It then sets `+0x100C = 0` and `+0x1008 = -1`, the same as releasing the lock with `T`.
- Counters: `death_unlocks`, `death_unlock_faults`.

This supports the HYPOTHESIS above: the target marker survives the load and points into the freed scene.

Session 56 (localization, all features on): the user locked a target, died, and loaded twice. There was no crash and
no hang: `death_unlocks` 2, `death_unlock_faults` 0, no fault logged, exit code 0. CONFIRMED.
