# Project goals

This document owns the epic-level intent of the migration off static
recompilation onto runtime execution. The architecture that realises it is `docs/migration.md`; the factual
progress ledger is `docs/project-state.md`.

## G001 — No static code generation; guest code runs at runtime, fast enough

USER 2026-09-01: "I said JIT but what I meant is no static code generation and
something performant so it doesn't have to be JIT."

Each console/PC port executes guest machine code by interpreting or translating
it at runtime, rather than by translating the whole binary to C at build time.
The engine — switch interpreter, threaded interpreter, JIT, or a hybrid — is an
implementation choice measured against each title's frame budget, not a
requirement in itself.

Native overrides remain the second runtime and the long-term direction: the
emulated runtime executes whatever is not yet overridden, and an override's
super-call runs the original guest function through it.

Success conditions:

- No project retains a static translator, generated C corpus, or regeneration
  build step.
- Every priority port (`psx/*`, `sunbright`, `x360/gears1`, `pc/lf2`,
  `pc/xmen2`) reaches its existing conformance milestone on its runtime engine,
  at an acceptable frame rate.
- Adding or removing an override requires no regeneration of anything.
- Each framework's engine choice is backed by a measurement against its title's
  frame budget, not by assumption. A JIT is built only where something simpler
  was measured and missed.

Non-goals: changing what the ports do, their renderers, their HLE behaviour, or
their faithful-first-then-enhance phasing.

## G002 — One framework per platform, with a deliberately thin shared layer

Each console or host platform gets its own framework owning its CPU execution,
guest APIs, HLE, renderer integration, and harness — the shape `psxport`
already has. Shared libraries hold only what is genuinely platform-independent,
and shared shape is extracted after two consumers demonstrably need it rather
than predicted in advance.

Success conditions:

- `psxport`, `gcnport`, `xenonport`, and `x86port` exist as independent
  frameworks; no framework depends on another.
- `jit-common` contains no guest-CPU knowledge; `render-common` contains no
  guest graphics API.
- Every title is a consumer of exactly one framework and reaches shared
  infrastructure only through it.

## G003 — Verification is re-anchored without losing what the parity work proved

The differential harnesses currently diff generated C against a reference
emulator. That anchor disappears with static recompilation, and the machinery
that replaces it must survive the migration — not because a difference count
gates anything (it does not; the bar is a working game that looks right) but
because it is how a visible defect gets root-caused instead of guessed at.

Success conditions:

- Every framework can run its new engine in lockstep against another and pause
  at the first divergence. During a migration that pairing is
  new-engine-vs-substrate, which needs no third engine.
- Each project's residual/known-divergence list has been re-derived under the
  new engine rather than assumed to carry over.
- Where we wrote the translator ourselves (`x86port`), a reference interpreter
  exists as the authority on the semantics it must match. Where a proven core is
  embedded, no interpreter is built for validation — an existing one is kept as a
  free diagnostic only.

## G004 — Ports stay deliverable on every host they support today

The ports ship on desktop x86-64, Apple Silicon, and Android ARM64. Core
selection, executable-memory handling, and the translation cache must respect
that; a core that only emits x86-64 is a stated, recorded limitation rather than
a silent one.

Success conditions:

- Every framework runs on every host it must ship to. Where a chosen core's JIT
  backend does not cover a host architecture, the fallback is a portable
  interpreter and the question becomes whether it holds frame rate — measured,
  not assumed.
- Executable-memory handling works where anonymous RWX is refused (Android) — for
  frameworks that use a JIT at all.
- Each framework's host-architecture support is recorded honestly in project
  state, including where it is degraded.
- The persistent translation cache is correct across host architectures or
  refuses to load.

## G005 — Guidance and vocabulary match the architecture

"Recomp" meant the emulated runtime. Every port has two runtimes: native
overrides and the emulated runtime (now executed at runtime rather than
statically generated). Skills, global instructions, and per-project docs must say
so.

Success conditions:

- The `recomp-*` skill family is replaced by `jit-*`.
- Global instructions no longer carry static-recompilation-only rules
  ("generated code is sacrosanct", instruction-coverage build gates).
- Each migrated project's own docs use the two-runtimes vocabulary.
