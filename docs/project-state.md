# Project state

Factual capability ledger for the native/dynarec migration. Epic intent is
`docs/project-goals.md`; architecture and order are `docs/migration.md`; atomic
work is `docs/issues/`.

## Comparison baseline

The baseline is the current portfolio of native-enhanced ports whose remaining
guest code is emitted offline as large generated C/C++ corpora and compiled into
each product. The intended product instead combines native overrides with
on-demand dynamic translation of the user's original binary. Emulator-based
projects without an offline guest-code pipeline are outside this migration.

## Current focus

S005 is the current focus: finish the audited portfolio plan and replace stale
project-local plans before any further runtime implementation.

## Capability inventory

| ID | Capability or outcome | State | Factual dependency | Goals |
| --- | --- | --- | --- | --- |
| S001 | Global static-recompiler skills are replaced by native/dynarec guidance | verified | — | G005 |
| S002 | `jit-common` supplies only demonstrated cross-framework primitives | partial | — | G002, G004 |
| S003 | Static-recompiler-only global instructions are removed | verified | S001 | G005 |
| S004 | Every affected repository has been audited against the native/dynarec requirement | verified | — | G005 |
| S005 | Central and project-local goals, state, ownership maps, and migration plans reflect the audited architecture | partial | S004 | G005 |
| S006 | The public project catalogue reflects each repository's revised architecture and capability state | missing | S005 | G005 |
| S010 | `x86port` owns x86-64 dynamic translation for products and a separately built x86-32 interpreter for tests, with no static engine | partial | S005 | G001, G002, G004 |
| S011 | X-Men 2 ships from `x86port` with no generated guest corpus and passes representative gameplay conformance | partial | S010 | G001, G003 |
| S012 | Little Fighter 2 ships from `x86port` with no generated guest corpus and passes representative gameplay conformance | missing | S010 | G001, G003 |
| S013 | Obsolete `shared/recomp-x86` is removed after all consumers leave it | missing | S011, S012 | G005 |
| S020 | `psxport` owns a per-`Core` PSX dynarec, runtime overrides, scoped original calls, bounded exits, and code invalidation | missing | S005 | G001, G002, G004 |
| S021 | Tomba! 2 reaches its current gameplay frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S022 | Tomba! 1 reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S023 | Crash Bandicoot reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S024 | Crash Team Racing reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S025 | Crash Bash reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S026 | Spider-Man reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S027 | Spyro reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S028 | Mega Man X4 reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S029 | Tekken 3 reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S030 | Toy Story 2 reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S031 | Vagrant Story reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S032 | C-12 reaches its current frontier through `psxport` with no generated corpus | missing | S020 | G001, G003 |
| S040 | `xenonport` executes Xbox 360 guest code through Xenia's x64/A64 dynarecs and owns runtime native-call boundaries | missing | S005 | G001, G002, G004 |
| S041 | Gears of War reaches its current frontier through `xenonport` with XenonRecomp removed | missing | S040 | G001, G003 |
| S042 | Marvel: Ultimate Alliance reaches its current frontier through `xenonport` with XenonRecomp removed | missing | S040 | G001, G003 |
| S043 | `shared/xenon-host` is absorbed into `xenonport` or removed without retaining a generated-function-map contract | missing | S040 | G002, G005 |
| S050 | `gcnport` executes GameCube guest code through Dolphin's dynarec and owns robust runtime hooks | missing | S005 | G001, G002, G004 |
| S051 | Sunbright reaches its current frontier through `gcnport` with its generated corpus removed | missing | S050 | G001, G003 |
| S060 | Kirbh has one clean `gbaport` native/dynarec product architecture | missing | S005 | G001, G002, G004, G006 |
| S061 | Kirbh provides drop-in split-screen multiplayer | missing | S060 | G003, G006 |
| S062 | Kirbh provides a wider gameplay camera without final-image stretching | missing | S060 | G003, G006 |
| S070 | `amigaport` dynamically translates 68000 code with image-scoped identity and complete CPU state | missing | S005 | G001, G002, G004 |
| S071 | Benefactor reaches its current frontier through `amigaport` with its generated corpus removed | missing | S070 | G001, G003 |
| S080 | `nesport` dynamically translates 6502 code with physical MMC3 mapping identity and complete execution state | missing | S005 | G001, G002, G004 |
| S081 | Mimp reaches its current frontier through `nesport` with its generated corpus removed | missing | S080 | G001, G003 |

Every title-completion item above includes the representative interactive
gameplay gate in G003. The boot/menu/leaf checkpoints in `docs/migration.md` are
first implementation discriminators, not completion evidence and not authority
to delete the current path.

## Evidence and exact gaps

### S001 — global skill replacement

Evidence: commit `75249b3` replaced the `recomp-*` skill family with
`dynarec-*`, removed stale installed links, and passed all skill validators.

### S002 — deliberately thin shared code

`jit-common` has tested W^X code-memory and block-cache primitives. Embedded
cores are not required to use them. Gap: no shared abstraction has yet been
demonstrated by two framework consumers.

### S003 — global instruction replacement

Evidence: commits `75249b3` and `bcb1150` remove the static methodology, require
native/dynarec hybrids, and forbid interpreters from gameplay build, link,
selector, and fallback surfaces; shared validators pass.

### S004 — portfolio audit

Evidence: the 2026-09-04 audit covered `shared/x86port`, `pc/xmen2`, `pc/lf2`,
`psx/psxport` and every listed PSX title, `x360/gears1`, `x360/mua`,
`shared/xenon-host`, `sunbright`, `kirbh`, `benefactor`, and `mimp`, including
each first implementation discriminator and full gameplay retirement gate.
Emulator-only AVPE is outside the migration.

### S005 — truthful local plans

The central authorities now reflect the audit. Gap: affected project-local
goals, state ledgers, codemaps, and plans still contain static or contradictory
methodology and must be replaced before implementation resumes.

### S006 — public catalogue refresh

Missing capability: refresh the `pages` project from each revised project-state
authority so it no longer publishes static-recompiler descriptions as the
intended architecture and continues to show every intended feature's actual
state.

### S010 — x86port product JIT

The X-Men 2 consumer contains an x64 JIT and test interpreter. Canonical
`x86port` has been synchronized and edited to remove `Substrate`. Gap: the edit
is unverified, local authorities are missing, gameplay must not link the test
interpreter, and ARM64 product translation is absent.

### S011 — X-Men 2 dynamic product

X-Men 2 has removed roughly 307 MiB of generated guest C and defaults to the
x64 JIT. Gap: representative gameplay conformance, the no-interpreter product
link gate, canonical framework synchronization, and required host coverage are
not yet verified together.

### S012 — Little Fighter 2 dynamic product

Missing capability: execute the current LF2 product through `x86port`'s JIT,
preserve its Win32/DirectDraw/native seams, and remove its generator and roughly
87,940 lines of generated guest C.

### S013 — retire recomp-x86

Missing capability: prove X-Men 2 and LF2 have no live dependency, then remove
the obsolete shared static translator rather than retain it as legacy.

### S020 — psxport dynarec executor

Missing capability: integrate the already-proven PSX dynarec as a per-`Core`
product executor with image-scoped overrides, original calls, bounded exits,
state synchronization, and executable-memory invalidation. The gameplay target
must not link the existing test interpreter.

### S021 — Tomba! 2

Missing capability: prove resident and colliding-overlay native/original calls,
then reach the current boot-to-gameplay frontier through the PSX dynarec with
the generated corpus absent.

### S022 — Tomba! 1

Missing capability: reproduce the 35-field CRT0 boundary and current CD/title
frontier through the PSX dynarec with its six overrides and no generated C.

### S023 — Crash Bandicoot

Missing capability: preserve the in-flight PadRead/menu behavior while reaching
the current product frontier through the PSX dynarec without generated C.

### S024 — Crash Team Racing

Missing capability: replace generated dispatch, express frame completion as a
bounded executor exit, and reach the current frontier through the PSX dynarec.

### S025 — Crash Bash

Missing capability: migrate 27 override installs and 15 generated-body calls to
the runtime executor and reach the current Crash Bash path without generated C.

### S026 — Spider-Man

Missing capability: reach the dem1 checkpoint through the PSX dynarec and
replace the generated movie-fiber body with resumable runtime guest execution.

### S027 — Spyro

Missing capability: reach the stage-13 800/900 route through the PSX dynarec and
replace the generated world body with runtime guest execution.

### S028 — Mega Man X4

Missing capability: reach the 4,000-field frontier through the PSX dynarec and
replace generated movie-body rewriting with authenticated runtime VSync exits.

### S029 — Tekken 3

Missing capability: reach `NAMCO PRESENTS` within 1,200 frames through the PSX
dynarec with all 14 original calls routed by address.

### S030 — Toy Story 2

Missing capability: reach Andy's Room within 120 frames through the PSX dynarec
while invalidating all 22 streamed modules across two reused code slots.

### S031 — Vagrant Story

Missing capability: reach the 1,000/1,000 TITLE checkpoint through the PSX
dynarec with its required native CD override and valid VSync ownership.

### S032 — C-12

Missing capability: execute the authenticated whole program beyond its first
VSync/former static miss through the PSX dynarec with no generated corpus.

### S040 — xenonport executor

Missing capability: wrap Xenia's existing x64/A64 dynarecs in a title-neutral
executor owning authenticated images, contexts, threads, imports, device writes,
runtime overrides, original calls, and singleton constraints.

### S041 — Gears of War

Missing capability: execute the first audited guest leaf/import/override
round-trip, then reach the current Gears frontier and remove its roughly 185 MiB
generated PPC corpus and XenonRecomp dependency.

### S042 — Marvel: Ultimate Alliance

Missing capability: execute exact Gold XEX entry `0x824806D8` through Xenia to a
named service boundary, then preserve current native/provisioning behavior while
removing its unfinished static path.

### S043 — xenon-host disposition

Missing capability: preserve its authenticated-image and import-validation
facts in `xenonport`, remove the precomputed generated-function-map contract,
update consumers, and delete the separate owner if no independent responsibility
remains.

### S050 — gcnport executor

Missing capability: integrate Dolphin's dynarec behind a title-neutral GameCube
executor with robust address hooks, runtime invalidation, and host qualification.

### S051 — Sunbright

Missing capability: boot exact `GMSE01` and prove the J3D draw hook through
Dolphin's dynarec, then remove roughly 2.79 million generated source lines.

### S060 — Kirbh deferred runtime

Missing capability: replace the competing WIP execution paths with one clean
`gbaport` native/dynarec architecture whose gameplay binary contains no
interpreter. This project is explicitly deferred and is not a near-term stream.

### S061 — Kirbh split-screen

Missing capability: provide drop-in split-screen multiplayer on Kirbh's single
authoritative runtime, with explicit join/leave, camera, input, and shared-state
ownership.

### S062 — Kirbh wider camera

Missing capability: widen the gameplay camera through deterministic projection,
viewport, scissor, and proven culling ownership without stretching the final
image or using frame-aware heuristics.

### S070 — amigaport executor

Missing capability: dynamically translate 68000 with complete PC/SR/exception
state, bounded execution, image-generation cache keys, invalidation, and scoped
native/original calls.

### S071 — Benefactor

Missing capability: reach current title synchronization through `amigaport`
across four address-reusing images, then remove generated C and generated-body
native calls.

### S080 — nesport executor

Missing capability: dynamically translate 6502 with complete PC/status/
interrupt/cycle state and cache/override identity based on physical MMC3 ROM
mapping.

### S081 — Mimp

Missing capability: first execute RESET through title/attract through `nesport`,
then prove representative gameplay before removing 16,664 generated functions
and generated-call coroutine ownership. Retain Nestopia only as an independent
test oracle.
