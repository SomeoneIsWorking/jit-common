# jit-common

Shared JIT infrastructure for the console→PC game-port frameworks. It is
deliberately thin: **no CPU translator, no decoder, no per-architecture
semantics, and no interface a framework is obliged to implement.** Each platform
framework (`psxport`, `gcnport`, `x360port`, `x86port`, …) owns its own CPU core
integration and calls into this library for the parts that do not depend on the
guest CPU.

What lives here:

| Area | Owns |
|---|---|
| **Executable memory** | W^X code buffers, dual-mapping for platforms that refuse anonymous RWX (Android), icache coherence, code-region allocation within branch range |
| **Block cache** | guest address → translated block, chaining, eviction, invalidation on SMC / bank switch / overlay load |
| **Potential shared runtime contracts** | only mechanisms demonstrated by two framework consumers; embedded cores keep their own caches and executable memory |

Commonality is **extracted here after two frameworks actually share it**, never
predicted up front. An earlier draft mandated a single `CpuCore` interface across
all frameworks; that was the wrong shape and has been dropped.

## The migration program lives here

This repository is also the home of the runtime guest-execution migration that
spans every port project. Four authorities, in the order a new session
should read them:

| Document | Answers |
|---|---|
| [`docs/project-state.md`](docs/project-state.md) | **Start here.** What is done, partial, blocked, missing — and the one current focus |
| [`docs/migration.md`](docs/migration.md) | The product contract, runtime boundaries, evidence gate, platform framework choices, title order, and first milestones |
| [`docs/project-goals.md`](docs/project-goals.md) | Why, and what "done" means for the program |
| [`docs/issues/`](docs/issues/) | Atomic work points, linked to state items |

Validate the ledger after every edit:

```sh
python3 ../re-harness/project_state.py --root .
```

## Status

The migration is underway. `jit-common` provides executable-memory and
block-cache primitives, but neither is mandatory for embedded cores that
already own those mechanisms. Gameplay products contain native overrides plus a
dynarec/JIT; interpreters are test-only and must not be linked or selectable in
gameplay builds. The current focus in
[`docs/project-state.md`](docs/project-state.md) names the next unfinished boundary.
