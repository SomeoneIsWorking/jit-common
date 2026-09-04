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
  intrinsics) only. Android build/device mechanics belong in
  `shared/android-port`; title-neutral APK runtime behavior belongs in Lucent.
- **One gameplay product:** PSX, x86, GameCube, and Xbox 360 use native overrides
  plus a dynarec/JIT by default, with only reason-coded and measured
  DuckStation-style interpreter fallback. NES, GBA, and Amiga may instead use a
  maintained interpreter when representative gameplay proves the host budget.
  Interpreter-only diagnostic modes never masquerade as dynarec evidence.
- **Offline guest translation is retired.** No build, install, or provisioning
  pipeline may emit guest code as source, object code, or a precompiled title
  substrate. The product dynamically translates the user's original binary
  while it runs. A runtime-populated cache may persist as
  disposable user data, but a fresh install cannot require it.
- **Runtime JIT code generation is allowed.** Say "offline/static guest
  translation" for the removed design; a dynarec is also a recompiler and a JIT
  necessarily generates host code, so banning those words would describe the
  target architecture incorrectly.

## Conventions

- C++20, Clang for agent verification builds, tracked `.clang-format` and
  `.clang-tidy`, both enforced by the test suite.
- A diagnostic that can print nothing is lying — design the negative case first
  (global rule). This matters here: a block cache that silently misses and a
  persistent cache that silently discards look identical to a working one.
- Subagents are globally authorized up to the active service limit. Keep
  ownership non-overlapping and serialize shared builds or singleton runtimes.

## Continuing the migration

This repo owns the cross-project migration to runtime guest execution, not just
this library. To pick the work up in a new session:

1. Read `docs/project-state.md` — the **Current focus** line names the one state
   item in play; read its row and its detail section.
2. Read `docs/migration.md` for the architecture that item sits in.
3. Check `docs/issues/` for open atomic work against that state item.
4. Update `docs/project-state.md` in the same change that advances an item, and
   run `python3 ../re-harness/tools/project_state.py --root .` afterwards.

Do not infer progress from code presence, commit history, or a project README —
the state ledger is the authority, and a stale one is the failure mode this
tracker exists to prevent.

## Status

Current focus **S005**: replace the audited projects' stale local plans before
runtime implementation resumes. `docs/project-state.md` is the factual
authority. Old interpreter timing from headless FMV-oriented runs is explicitly
not product or performance evidence.
