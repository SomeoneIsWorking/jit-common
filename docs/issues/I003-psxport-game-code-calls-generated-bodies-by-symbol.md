# I003 — psxport game code calls generated bodies by C symbol, not by guest address

state_items: S013, S011, S014
goals: G001
status: open
opened: 2026-09-01

## What is in the way of deleting the static translator

Measured, not estimated. `cmake/tomba2_port.cmake` was configured with
`GEN_REC_SRCS` emptied and `game/core/recomp_register.cpp` replaced by an
all-null `RecompRegistry`, then `tomba2_port` was built into a separate tree.

**Everything compiled. Only the link failed**, with **816 distinct undefined
symbols across 109 files**:

| symbol shape | count | what it is |
|---|---|---|
| `gen_func_<addr>` | 405 | the RAW generated body — a native override super-calling the original |
| `func_<addr>` | 300 | the override-aware generated wrapper |
| `ov_<mod>_gen_<addr>` | 78 | the same two shapes inside an overlay module |
| `ov_<mod>_func_<addr>` | ~28 | " |
| `shard_set_override`, `ov_{a00,sop,crd}_set_override` | 4 | installing a native override INTO the generated dispatch table |

Concentration: `game/core/field_owned_leaves.cpp` alone accounts for 126 of the
references; the remaining 108 files carry 1–15 each. `guest_abi.h`'s
`guest_call(c, ra_const, fn)` takes a **function pointer**, which is how a game
call site names a generated body without the framework doing so.

So the framework↔substrate seam (`recomp_iface.h`) is genuinely clean —
`libpsxport.a` has no undefined generated symbol, and `psxport_smoke` proves it.
The coupling that blocks S013 is entirely **game→generated**, which that seam
deliberately permits.

## The design question this exposes

`rec_dispatch(c, addr)` already honours `Core::engine` (both in
`dispatch.cpp`'s `route_guest_call` and in `overlay_router.cpp`'s own check), so
every address-routed call is engine-agnostic today. The 816 symbol calls are
not, and two of them cannot be mechanically rewritten to an address:

- `func_<addr>(c)` → `rec_dispatch(c, addr)` is a straight substitution.
- `gen_func_<addr>(c)` means *run the original body, ignoring my override*.
  There is no engine-level operation for that yet. `rec_super_call()` is not it:
  its Interpreter arm calls `overrides::dispatchOracle(c, addr)` first, so an
  override that super-called itself would re-enter its own native handler.

`overrides::install()` already records the generated body pointer per override
(`gen`), which is where the substrate gets its answer. The engine-neutral
replacement is a super-call that suppresses the override for that one address
and then executes the guest code — cheap for an interpreter, and the same
operation a JIT would need.

## Why nothing has exploded yet

`overrides::install()`'s `oracleAllowed` defaults to `false`, and **Tomba2Engine
opts zero of its 334 override installs in**. So on the Interpreter engine today
none of the native overrides run at all; the re-entrancy above is unreachable,
and an interpreter run is pure guest emulation rather than the port.

That is also the correction to the S011 measurement: the two engines were not
running the same work. See S011.

## Work

1. An engine-neutral super-call that suppresses the override for one address.
2. Rewrite the 816 call sites: `func_<addr>(c)` → `rec_dispatch(c, addr)`,
   `gen_func_<addr>(c)` → the new super-call. Mechanical, and verifiable — the
   addresses are in the symbol names, and the no-substrate link is the gate: it
   passes only when the last one is gone.
3. Move the 4 `*_set_override` call sites onto `overrides::` so an override is
   installed once, engine-independently, rather than into a generated table.
4. `guest_call(c, ra_const, fn)` loses its function-pointer form.
5. Then `GEN_REC_SRCS` can be emptied for real, and S013 can delete the
   translator.
