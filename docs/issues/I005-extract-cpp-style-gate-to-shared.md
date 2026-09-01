# I005 — Extract psxport's C++ style gate to `shared/re-harness/tools/`

state_items: —
status: open
opened: 2026-09-02

## What

`psx/psxport/tools/check_cpp_style.py` (18,179 bytes) gates clang-format,
clang-tidy, and source structure caps for a C++ repository. It is written to a
`--root`, carries nothing psxport-specific in its logic, and already does the
things a second consumer wants: refusing when either tool is absent, refusing
when `.clang-tidy` is untracked, and reporting per-file counts.

`jit-common` became its second would-be consumer on 2026-09-02, when its own
AGENTS.md convention — *"tracked `.clang-format` and `.clang-tidy`, both enforced
by the test suite"* — was finally implemented. Rather than fork 18 KB, this repo
got `tools/check_style.py`: the same two tools, no structure caps, no tracked-
config validation, ~110 lines.

That is a deliberate stopgap and it is already a DRY violation — two scripts
now answer "is this repository's C++ formatted and lint-clean?".

## The proper fix

Move `check_cpp_style.py` to `shared/re-harness/tools/`, per the global rules
*"Reusable executables live once under `tools/`"* and *"If you write something a
second project will want, put it in `shared/` the first time, not the second."*
Then migrate both consumers atomically:

- `psxport` — update its verifier's invocation to the shared path.
- `jit-common` — delete `tools/check_style.py`, point the `style` ctest at the
  shared script, and keep the structure caps it brings.

## Why it was not done in the change that created it

The change landed during an unattended autonomous tick. Relocating another
project's verification gate — psxport's, whose green result is a landing
precondition there — is not something to do without the user present: a wrong
path or a changed exit code silently disarms that project's whole style gate,
which is exactly the "diagnostic that can print nothing" failure the gate exists
to prevent. Recorded here instead so the second implementation is a known debt
with an owner rather than a fork nobody notices.

## Done when

One script, under `shared/re-harness/tools/`, invoked by both repos, with
`jit-common/tools/check_style.py` deleted rather than left beside it.
