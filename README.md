# jit-common

Shared JIT infrastructure for the console→PC game-port frameworks. It is
deliberately thin: **no CPU translator, no decoder, no per-architecture
semantics, and no interface a framework is obliged to implement.** Each platform
framework (`psxport`, `gcnport`, `xenonport`, `x86port`, …) owns its own CPU core
integration and calls into this library for the parts that do not depend on the
guest CPU.

What lives here:

| Area | Owns |
|---|---|
| **Executable memory** | W^X code buffers, dual-mapping for platforms that refuse anonymous RWX (Android), icache coherence, code-region allocation within branch range |
| **Block cache** | guest address → translated block, chaining, eviction, invalidation on SMC / bank switch / overlay load |
| **Persistent translation cache** | on-disk format, keying, versioning, load/store, verification mode |
| **Override table** | `(guest address → native function)`, gameplay scoping, A/B disable, the evidence gate |
| **Harness helpers** | register-file differ, trace ring, deterministic-replay scaffolding, first-divergence reporting |

Commonality is **extracted here after two frameworks actually share it**, never
predicted up front. An earlier draft mandated a single `CpuCore` interface across
all frameworks; that was the wrong shape and has been dropped.

## The migration program lives here

This repository is also the home of the static-recompilation → JIT migration
that spans every port project. Four authorities, in the order a new session
should read them:

| Document | Answers |
|---|---|
| [`docs/project-state.md`](docs/project-state.md) | **Start here.** What is done, partial, blocked, missing — and the one current focus |
| [`docs/migration.md`](docs/migration.md) | The architecture: L1–L4 layering, platform frameworks, CPU core selection and the host-arch constraint, the graphics frontend/backend split, verification |
| [`docs/project-goals.md`](docs/project-goals.md) | Why, and what "done" means for the program |
| [`docs/issues/`](docs/issues/) | Atomic work points, linked to state items |

Validate the ledger after every edit:

```sh
python3 ../re-harness/project_state.py --root .
```

## Status

Design and tracker complete; no code yet. Current focus is **S010** — promoting
`psxport`'s MIPS interpreter to a validated correctness anchor, the first
executable step of the migration.
