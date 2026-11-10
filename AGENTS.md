# jit-common — agent guidance

Shared JIT infrastructure for the port frameworks. This repo also owns the
cross-project migration to runtime guest execution: `docs/migration.md` is the
whole architecture and migration plan, `docs/project-state.md` the state ledger
and current focus. Execution policy (JIT default, bounded fallback, no offline
translation): the `dynarec-port` and `dynarec-runtime` skills.

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
  `shared/android-port`; Lucent supplies only optional helper APIs.
- **Runtime JIT code generation is allowed.** Say "offline/static guest
  translation" for the removed design; a dynarec is also a recompiler and a JIT
  necessarily generates host code, so banning those words would describe the
  target architecture incorrectly.

## Conventions

- C++20; the test suite enforces the tracked `.clang-format` and `.clang-tidy`.
- A diagnostic that can print nothing is lying — design the negative case first.
  This matters here: a block cache that silently misses and a persistent cache
  that silently discards look identical to a working one.
