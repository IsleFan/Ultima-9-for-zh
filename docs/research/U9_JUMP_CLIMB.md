# Jump and climb on one key (research)

2026-11-16. Static disassembly of `u9.exe`, a read-only probe in the runtime (session 61), then the feature itself, implemented with the user's authorization and tried in game the same day (see "Implemented" at the end).

## Request (human)

Many games put jumping and climbing on one key: with something climbable in front the character climbs, otherwise it jumps. Ultima IX has two commands, `jump` (Space) and `climb` (C). The wish is one key that picks.

Loading the runtime through the proxy DLL does not change what the runtime can do; this would be possible with either loader.

## What the two commands do (CONFIRMED from the code)

| Command | Handler | Action number |
| --- | --- | --- |
| `climb` | `0x5B1F90` | 10 |
| `jump` | `0x5B1FE0` | 11 |

Both handlers act on key down only and test the same things first:
- the movement controller `[0x131F668]` has `+0x20` and `+0x50` clear;
- `0x44FF30` on `[0x13259C4]`→`0x5C3FA0`→`+0x70` returns 0.

`jump` has one more test: `0x51C0F0` on `[[0x850228]+0xC8]` must not return 1.

Then both call `0x5B0D10(action)` on the controller. In the normal case that reaches `0x5B0B60(action, walk)`, which switches on the action through the table at `0x5B0CD8`:

- **Action 10, climb (`0x5B0BFD`).**
  1. Clears the sideways and forward fields (`+0x14`, `+0x18`, `+0x1E`) and stops the current animation.
  2. Calls **vtable `+0x7C`** of the object at `[[controller]+0xE8]`, with no arguments.
  3. The return value is not used.
- **Action 11, jump (`0x5B0C58`).**
  1. Builds a target from the mouse position (`[[0x850228]+0xC0]+0x118`, through `0x5B37D0`).
  2. Clears the same fields.
  3. Calls **vtable `+0x80`** of the same object, with a pointer to that target.

So the game's own "is there something to climb" test is inside the `+0x7C` routine, or below it.

## Session 61 probe and what it led to (2026-11-16)

`movement_probe`: the object at `[[controller]+0xE8]` has vtable `0x741F70`; its `+0x7C` is `0x57BF60` and its `+0x80` is `0x57BFE0`. The human reported: **pressing climb on open ground does nothing visible.**

From there, all CONFIRMED by reading the code:

1. **`0x57BF60` and `0x57BFE0` only queue the action.** The object keeps up to 9 pending actions of 32 bytes each from `+0x4A8`, with the count at `+0x5E8`. Climb stores type 10; jump stores type 11 and its target.
2. **The queue is taken in `0x57E620`** (vtable `+0x154`). It switches on the type through the table at `0x57EC40`:
   - type 10 → sets `+0x148 = 10` and calls **vtable `+0x15C`** (no arguments);
   - type 11 → sets `+0x148 = 11` and calls **vtable `+0x160`** with the target.
3. **For the Avatar's class (vtable `0x741F70`):**
   - `+0x15C` = `0x57EF00`, the climb routine, shared by about 40 actor classes;
   - `+0x160` = `0x4193A0`, the jump routine, the Avatar's own.
4. **The climb routine `0x57EF00` holds the "can I climb here" test.**
   - It gives up at once unless the state `+0x144` is 0, 1, 2 or 5, flag `0x20` of `+0xD8` is set, and flag `0x4000` of `+0xDC` is clear.
   - It then probes up to three points ahead of the actor, along its heading `+0x1B4`, each one farther (distance from `+0x238` × scale `+0x638`). For each it asks vtable `+0x180` for the ground height there, and accepts a ledge whose height above the actor (`+0x168`) lies inside the climbable range (`+0x204`, `+0x208`, `+0x1F8`, all × scale) and that vtable `+0x178` does not reject.
   - **Nothing found:** it calls the callback at `+0x628` when one is set, and returns. No state changes. This matches "nothing visible".
   - **Found:** it starts the climb (vtable `+0x1BC`, then `+0x184`), stores the old state in `+0x146`, and clears flag `0x40` of `+0xD8`.
   - The store to `+0x146` at `0x57F249` happens on the success path only.

## Design (authorized by the user on 2026-11-16)

Make the **jump key** the smart one, for the Avatar only:

1. Hook the Avatar's jump routine `0x4193A0` with the runtime's usual inline hook. Its first bytes are `56 8B F1 8A 86 8E 06 00 00` (`push esi; mov esi,ecx; mov al,[esi+0x68E]`), three whole instructions with nothing position-dependent.
2. In the hook, before jumping:
   - put a sentinel in `+0x146` and set `+0x148 = 10`;
   - call the game's own climb routine `0x57EF00` on the same object.
3. When `+0x146` was overwritten, the climb started: return without jumping.
4. Otherwise put `+0x146` and `+0x148` back and continue into the original jump.

Properties:
- The decision is the game's own test, with the game's own numbers. Nothing is re-implemented.
- The climb key keeps working as before. Only the jump key gains the climb.
- It affects the Avatar only: the hook is on the Avatar's own jump routine.

Open points, to settle in game before it could be called done:
- **The callback at `+0x628`** runs on a failed climb. What it does for the Avatar is UNKNOWN (the human saw nothing on open ground, so it is silent or unset there). If it turns out to do something, the hook would save, clear and restore that field around the call.
- **Jump has one more precondition than climb** in the key handlers (`0x51C0F0`). The smart key goes through the jump handler, so that test still applies; a climb through the smart key is refused in that one case where C would allow it. Which case that is, is UNKNOWN.
- **Jumping at a low wall on purpose** becomes a climb. That is the behaviour the request describes, but it should be a front page switch, off until tried.
- Whether the queue timing matters: the jump is taken from the queue a little after the key press, so the climb test runs at that moment, not at the key press.

## Implemented (2026-11-16)

- Feature bit `FeatureSmartJump` (256), `smart_jump` in `u9cht.ini` `[features]`, **off by default**. The front page shows it as 跳躍鍵自動攀爬.
- `engine.cpp`: `jump_hook` on `0x4193A0` (hook name `avatar_jump`) and `try_climb`, exactly the design above, with two additions:
  - the failure callback at `+0x628` is taken out for the duration of the climb call and put back, so a climb that does not start stays silent;
  - nothing is tried while `+0x68E` is set, because the jump key then does something else (`0x4193A3`).
- The climb call is guarded by SEH; a fault counts in `smart_faults` and the jump goes on.
- Counters in the log: `smart_climbs`, `smart_jumps`, `smart_faults`.

**Front page run, 2026-11-16 (private Steam copy, proxy loading, the box ticked), CONFIRMED in game:**
- the human: the jump key jumps on open ground and climbs where something can be climbed; the climb key still works;
- log: `smart_jump enabled`, 3 climbs, 5 jumps, 0 faults, no exception, exit code 0.

Not looked at separately yet: jumping while moving, the one case where jump is refused but climb is allowed (`0x51C0F0`), and longer play with the feature on.
