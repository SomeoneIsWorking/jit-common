# I001 — Generalise psxport's `use_interp` boolean into an execution-engine selector

state_items: S010, S011
status: resolved
opened: 2026-09-01
resolved: 2026-09-01

## What

`psxport` chooses its execution substrate with a single boolean, `Core::use_interp`
(`runtime/recomp/core.h:146`, default `0`). It is branched on in two places:

- `runtime/recomp/dispatch.cpp` — `rec_super_call`, `rec_interp`, `rec_coro_run`,
  `stub_dispatch`, each `if (c->use_interp) { … interp } else { rec_dispatch }`.
- `runtime/recomp/guest_call.h` — five call sites of
  `c->use_interp ? rec_interp(c, fn) : rec_dispatch(c, fn)`.

`runtime/recomp/overlay_router.cpp:414` carries the same branch for overlay
routing.

A JIT is a **third** engine, so the boolean cannot express it. Replace it with an
enum-valued engine selection on `Core` (`Substrate`, `Interpreter`, `Jit`) and one
routing helper the branch sites call, so adding the JIT touches the selector and
not every guest-call site.

## Why this first

It is the whole insertion point for S011 — the JIT arrives as a third engine
value rather than as a new branch threaded through every guest-call site. It also
unblocks S012: `sbs.cpp` can already lockstep two Cores and pause at the first
divergence, but it can only select between interpreter and substrate, so
JIT-vs-substrate — the pairing that matters during the migration — is not
expressible until the selector exists.

## Constraints

- **No behaviour change in this issue.** `Substrate` and `Interpreter` must route
  exactly where they route today; the JIT arm does not exist yet. This is a
  refactor whose success condition is that the existing gates and
  `PSXPORT_SELFTEST=oracle` behave identically.
- Follow psxport's "never duplicate code" rule — one owner for the routing
  decision, not a copy of the ternary per call site.
- Framework change starts with a red hermetic test in `psxport/tests/test_*.cpp`
  (psxport `CLAUDE.md`, "Build and test").
- Do not hand-edit `generated/`.

## Done when

- `Core` carries an engine enum; no `use_interp` boolean remains.
- One routing owner; `dispatch.cpp`, `guest_call.h`, and `overlay_router.cpp`
  consult it rather than re-implementing the branch.
- A hermetic test covers all currently-reachable engine values, including the
  negative (an unset/invalid engine refuses rather than silently picking one).
- `PSXPORT_SELFTEST=oracle` and the existing ctest gate pass unchanged.

## Resolution (2026-09-01)

- `runtime/recomp/engine_select.h` — new. Owns `Engine {Substrate, Interpreter,
  Jit}`, `kEngineDefault`, and the total `engine_route()` policy, which REFUSES an
  unknown value instead of defaulting.
- `core.h` — `int use_interp` replaced by `Engine engine = kEngineDefault`.
  `Engine::Substrate == 0`, so the shipping default is byte-identical.
- `dispatch.cpp` — the four entry points' four copies of the branch collapse into
  one `route_guest_call()`. The `Jit` arm aborts with a named message rather than
  falling back to the substrate.
- `guest_call.h` — the five ternaries were not only duplicated policy but
  **redundant**: `rec_interp()` already made the same decision, so
  `c->use_interp ? rec_interp(c,fn) : rec_dispatch(c,fn)` was exactly equivalent
  to `rec_interp(c,fn)`. Now one call each; marshalling is all that is left.
- `overlay_router.cpp`, `interp.cpp`, `selftest.cpp`, `sbs.cpp` — consult the
  enum.
- `tests/test_engine_select.cpp` — new; 23 checks over totality, injectivity, the
  named mapping, distinct names, the preserved default, and the negative
  (out-of-range values refuse, with a denominator).

**Correction to the stated goal.** "One routing owner" turned out to be wrong in
detail: `rec_dispatch()` in `overlay_router.cpp` is a *genuine second entry point*
— generated code calls it directly — so it must keep its own guard. The honest
invariant is **one routing POLICY** (`engine_select.h`), consulted by every entry
point, with no entry point re-deriving the decision from a flag.

## Verification

- `ctest --test-dir build` — **133/133 pass**, including `cpp_style` and the new
  `test_engine_select`.
- The new test was negative-tested before being trusted: making unknown engines
  fall through to `Substrate` (the old boolean's exact defect) fails
  `unknown_engine_refuses` with `got 0 want 8`.
- **`PSXPORT_SELFTEST=oracle` — PASS**, run against `Tomba2Engine` built on this
  psxport tree: "interpreter core RAN the cutscene without freeze/MISS (GAME loop
  +4174 over 4200 frames; furthest SOP scene id=7)", ending at the free-roam
  field, with `0 UNKNOWN` knobs in the exit audit. The interpreter path is
  therefore unchanged by a run, not merely by inspection. (Discs are provisioned
  via each repo's `.env`; an earlier note here wrongly said this was unrunnable.)
- Building the consuming game is what caught two defects the hermetic suite
  structurally cannot — see "Collisions the framework suite cannot catch".

## Collisions the framework suite cannot catch

Two failures appeared only when a real game was built against this tree, and
neither could have been found by `ctest` in psxport:

1. **Global-name collision.** The first version of this header declared a bare
   `Engine` in the global namespace. Tomba2Engine has `class Engine`
   (`game/core/engine.h`); the build failed instantly. Fixed by namespacing to
   `psx::exec` (house convention: `psx::config`, `psx::ui`). `psxport_smoke`
   proves the framework exports no GAME symbols — it cannot prove the framework
   avoids names a game wants.
2. **A pre-existing broken link, not from this change.** Uncommitted GpuState
   de-globalization work in this tree had deleted `gpu_seen3d_this_frame(Core*)`
   as having "no external callers", while Tomba2Engine called it through a
   *local forward declaration* at its call site — invisible to a header-side
   caller audit. Restored beside its surviving twin `gpu_had3d_last_frame`,
   declared properly in `gpu_vk.h`, and the game now includes the header. A
   redundant local re-declaration in `gpu_vk.cpp` was removed too.

Rule added to `psxport/AGENTS.md` from (2), at the user's instruction: never
forward-declare across an ownership boundary; always include the header. Removing
a public function requires auditing the GAME trees, not just psxport.
