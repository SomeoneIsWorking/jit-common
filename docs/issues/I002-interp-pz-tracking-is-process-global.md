# I002 — psxport's interpreter holds per-Core guest register tags in a process-global array

state_items: S010, S012
status: open
opened: 2026-09-01

## What

`runtime/recomp/interp.cpp:66`:

```c
static uint8_t s_pz_kind[32];   // one tag per GPR
```

This is the native-depth tap's per-GPR tracking: one tag per guest register saying
whether it currently carries a projected vertex depth (`PZ_VERTEX`, set on `mfc2`)
or a load source address (`PZ_SRC`, set on `lw`), cleared by any other write to
that register. The clear is the tap's whole safety property — a stale tag attaches
a wrong depth to an unrelated address, which sorts a 2D element into the 3D scene.

The array is **file-static, so it is per-process**, while the state it describes is
**per-Core** (`c->r[]`). Two Cores executing interpreted code in one process share
one set of tags and clear each other's.

This violates the instance-owned-state rule (`migration.md` §2, "Instance-owned
state is what makes in-process comparison possible"): a `static` holding
guest-derived state breaks the two-cores-in-one-process comparison that the whole
verification design rests on, and the corruption presents as a divergence in the
thing being measured.

## Reachability today — stated honestly

Not currently reachable, for two reasons that are both incidental:

- The tap is off by default (`PSXPORT_INTERP_DEPTH=1` to enable), and its own
  comment records that it is not yet precise enough to earn being on.
- Today's lockstep pairing is interpreter Core vs **substrate** Core, and the
  substrate does not execute `interp.cpp`, so only one Core touches the array.

Both protections disappear under the migration. Comparing interpreter against JIT,
or any two interpreter Cores, puts two writers on one array.

## Fix

Move the tags onto `Core` beside the other per-Core interpreter diagnostic state
(`Core::idiag`, `InterpDiag`) — that struct already exists for exactly this reason
("per-Core so SBS profiles never interleave"). It is the right owner and the fix is
mechanical.

## Done when

- No `static`/global in `interp.cpp` holds guest-derived state.
- A hermetic test runs two Cores through the tagging path interleaved and asserts
  their tags are independent — with a denominator, not "no corruption seen".
- `PSXPORT_SELFTEST=oracle` unchanged.

## Wider action

Sweep every framework for the same shape before its migration: any `static` or
global holding guest machine state, including in diagnostics. This is a
prerequisite for the comparison design, not cleanup.
