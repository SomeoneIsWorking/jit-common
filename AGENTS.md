# jit-common — agent guidance

Shared JIT infrastructure for the port frameworks. Read `docs/migration.md`
first: it is the whole architecture and the migration plan, not just this
library's scope.

## Boundaries

- **No guest CPU knowledge in this repo.** No decoder, no instruction semantics,
  no arch-specific register layout. If a change needs to know it is MIPS, it
  belongs in the framework.
- **No mandated cross-framework interface.** Each platform framework integrates
  its CPU core the way that core wants to be integrated. Extract shared shape
  here only after two frameworks demonstrably share it.
- **No Android SDK / SDL / JNI dependency.** The executable-memory code uses
  plain OS primitives (`memfd_create`, `mmap`, `mprotect`, cache-flush
  intrinsics) only. Android APK runtime behaviour belongs in
  `shared/android-runtime` (see migration doc §10).
- **Two runtimes per port:** native overrides (WIP, the direction) + the
  emulated (JIT) runtime. The game is always the guest game. Do not reintroduce
  "recomp" wording.

## Conventions

- C++20, Clang for agent verification builds, tracked `.clang-format` and
  `.clang-tidy`, both enforced by the test suite.
- A diagnostic that can print nothing is lying — design the negative case first
  (global rule). This matters here: a block cache that silently misses and a
  persistent cache that silently discards look identical to a working one.
- Subagent allowance: **0** until the user assigns a count.

## Continuing the migration

This repo owns the cross-project static-recompilation → JIT migration, not just
this library. To pick the work up in a new session:

1. Read `docs/project-state.md` — the **Current focus** line names the one state
   item in play; read its row and its detail section.
2. Read `docs/migration.md` for the architecture that item sits in.
3. Check `docs/issues/` for open atomic work against that state item.
4. Update `docs/project-state.md` in the same change that advances an item, and
   run `python3 ../re-harness/project_state.py --root .` afterwards.

Do not infer progress from code presence, commit history, or a project README —
the state ledger is the authority, and a stale one is the failure mode this
tracker exists to prevent.

## Status

Current focus **S011**. First measurement in: on `psxport`/Tomba! 2, the
interpreter already runs the game at a median 6.10 ms/frame against a 16.67 ms
budget (the substrate is 2.27), so desktop PSX needs no JIT to drop static code
generation. `docs/project-state.md` S011 carries the numbers and what they do
not cover. No `jit-common` code yet — S001 onward are still untouched.
