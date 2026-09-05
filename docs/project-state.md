# Project state

Factual capability ledger for the runtime-execution migration. Epic intent is
`docs/project-goals.md`; architecture and order are `docs/migration.md`; atomic
work is `docs/issues/`.

## Comparison baseline

The baseline is the current portfolio of native-enhanced ports whose remaining
guest code is emitted offline as large generated C/C++ corpora and compiled into
each product. The intended products instead consume the user's original binary
at runtime: dynarec-first for stronger platforms, and a maintained interpreter
where explicitly allowed for low-power consoles.

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
| S010 | `x86port` owns x86-64 dynamic translation plus bounded counted fallback, with no static engine | partial | S005 | G001, G002, G004 |
| S011 | X-Men 2 ships from `x86port` with no generated guest corpus and passes representative gameplay conformance | partial | S010 | G001, G003 |
| S012 | Little Fighter 2 ships from `x86port` with no generated guest corpus and passes representative gameplay conformance | missing | S010 | G001, G003 |
| S013 | Obsolete `shared/recomp-x86` is removed after all consumers leave it | missing | S011, S012 | G005 |
| S014 | `shared/alchemy` is one product-ready neutral engine with separately linked x86/x360 platform adapters and typed configuration/logging boundaries | partial | — | G007 |
| S015 | X-Men 2 links and calls a proven `shared/alchemy` contract through its x86 adapter in representative gameplay | missing | S014 | G007 |
| S020 | `psxport` owns a per-`Core` PSX dynarec, counted refused-block fallback, runtime overrides, scoped original calls, bounded exits, and code invalidation | missing | S005 | G001, G002, G004 |
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
| S040 | `x360port` executes Xbox 360 guest code through Xenia's x64/A64 dynarecs and owns runtime native-call boundaries | missing | S005 | G001, G002, G004, G008 |
| S041 | Gears of War reaches its current frontier through `x360port` and `x360ue3`, with the static PPC pipeline absent | missing | S040, S045 | G001, G003, G008 |
| S042 | Marvel: Ultimate Alliance reaches its current frontier through `x360port` with the static PPC pipeline absent | missing | S040 | G001, G003, G008 |
| S043 | The former shared Xbox host is replaced in-place by `x360port` without a generated-function-map contract | verified | — | G002, G005, G008 |
| S044 | MUA consumes and extends the Alchemy contracts proved by X-Men 2 without creating a parallel title-local engine | missing | S015, X-Men 2 complete goals | G007 |
| S045 | `x360ue3` provides a clean title-neutral UE3-on-Xbox-360 layer between `x360port` and `GearsUE3` | missing | S040 | G008 |
| S050 | `gcnport` executes GameCube guest code through Dolphin's dynarec and owns robust runtime hooks | missing | S005 | G001, G002, G004 |
| S051 | Sunbright reaches its current frontier through `gcnport` with its generated corpus removed | missing | S050 | G001, G003 |
| S060 | Kirbh has one clean `gbaport` native/emulator product architecture | missing | S005 | G001, G002, G006 |
| S061 | Kirbh provides drop-in split-screen multiplayer | missing | S060 | G003, G006 |
| S062 | Kirbh provides a wider gameplay camera without final-image stretching | missing | S060 | G003, G006 |
| S070 | `amigaport` owns a maintained 68000 interpreter with image-scoped identity and complete CPU state | missing | S005 | G001, G002 |
| S071 | Benefactor reaches its current frontier through `amigaport` with its generated corpus removed | missing | S070 | G001, G003 |
| S080 | `nesport` owns a maintained 6502 interpreter with physical MMC3 mapping identity and complete execution state | missing | S005 | G001, G002 |
| S081 | Mimp reaches its current frontier through `nesport` with its generated corpus removed | missing | S080 | G001, G003 |

Every title-completion item above includes the representative interactive
gameplay gate in G003. The boot/menu/leaf checkpoints in `docs/migration.md` are
first implementation discriminators, not completion evidence. Static execution
is deleted before those discriminators and is never restored as a bridge.

## Evidence and exact gaps

### S001 — global skill replacement

Evidence: commit `75249b3` replaced the `recomp-*` skill family with
`dynarec-*`, removed stale installed links, and passed all skill validators.

### S002 — deliberately thin shared code

`jit-common` has tested W^X code-memory and block-cache primitives. Embedded
cores are not required to use them. Gap: no shared abstraction has yet been
demonstrated by two framework consumers.

The code-memory owner now provides
`jc_code_publish_range(region, offset, bytes_written)`: it flushes the changed
executable-address range while retaining the existing whole-region protection
transition. `jc_code_publish(region, bytes_written)` remains the offset-zero
compatibility entry point. Range checks reject overflow and out-of-region bytes;
a valid empty range closes the write window without publishing new instructions.
This lets append-only emitters avoid repeatedly flushing all preceding blocks.
The API does not introduce a persistent cache or change block-cache policy.

On Apple Silicon, `jc_code_begin_write` always reopens the calling thread's
MAP_JIT write window. A different region's publication may have closed it even
when the requested region's local writable flag remains set. Callers must begin
writing again after any publication on that thread.

Verification on 2026-09-05, Apple Silicon macOS: `test_code_memory` passes
606 checks with zero failures. The execution battery runs through the default
MAP_JIT mechanism and forced mprotect (one of two forced mechanisms available;
Linux dual-mapped memfd is unavailable here). Range tests execute an initial
function and 64 appended/rewritten values at offset 256, preserve the initial
function, reject invalid and overflowing bounds without closing a write window,
and check empty-range publication. Two simultaneous regions exercise the
thread-wide protection transition. All three CTest entries pass, including the
clang-format/clang-tidy gate. These are code-memory correctness observations;
Windows, Linux dual mapping, concurrent patching, and application performance
are not established by this host's tests.

### S003 — global instruction replacement

Evidence: commits `75249b3`, `bcb1150`, and `859c47b` remove the static
methodology, define dynarec-first bounded fallback, and permit measured
interpreter products for explicitly low-power consoles; shared validators pass.

### S004 — portfolio audit

Evidence: the 2026-09-04 audit covered `shared/x86port`, `pc/xmen2`, `pc/lf2`,
`psx/psxport` and every listed PSX title, `x360/gears1`, `x360/mua`,
the former shared Xbox host, `sunbright`, `kirbh`, `benefactor`, and `mimp`, including
each first implementation discriminator, break-first removal boundary, and full
dynamic gameplay completion gate.
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

`x86port` commit `e18bf6e` provides a verified x64 JIT-only product target, a
separately linked test interpreter, expanded reached instruction coverage, and
mechanical product-link/source guards. Gap: Apple Silicon macOS and Android
arm64-v8a product translation are both absent and must be qualified separately.

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

### S014 — shared Alchemy engine readiness

Existing `shared/alchemy` targets provide partial format/render-data/input
libraries and XMLB/ARK tooling. Gap: its shipping library still carries
X2-specific viewer vocabulary, environment reads, and direct stderr diagnostics,
and no gameplay consumer proves a stable runtime API. Introduce typed config and
logger boundaries, one dependency pin/resolver authority, and focused stateful
engine owners without rewriting proven C parsers for style alone. Keep one
repository: the neutral `shared` component must depend on neither execution
framework, while optional `x86` and `x360` adapters consume `x86port` and
`x360port` respectively and are never linked together merely because they
coexist in the repository.

### S015 — X-Men 2 shared Alchemy consumption

Missing capability: X-Men 2 currently provisions `shared/alchemy` and invokes
some XMLB/ARK tooling, while `x2native` links, includes, and calls none of its
runtime libraries. First integrate `alchemy_input` through the x86 adapter's
guest `igControllerManager` contract and A/B-verify it against the retained
DirectInput path during representative gameplay.

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

### S040 — x360port executor

Missing capability: wrap Xenia's existing x64/A64 dynarecs in a title-neutral
executor owning authenticated images, contexts, threads, imports, device writes,
runtime overrides, original calls, and singleton constraints.

### S041 — Gears of War

The generated PPC corpus, static translator dependency, function maps, and
static-only tooling are absent. Missing capability: execute the audited guest
leaf/import/override round-trip through `x360port`, route reusable UE3/Xbox
semantics through `x360ue3`, and reach the current Gears frontier through
`GearsUE3`.

### S042 — Marvel: Ultimate Alliance

Missing capability: delete MUA's unfinished static product surfaces first,
then execute exact Gold XEX entry `0x824806D8` through `x360port`/Xenia to a
named service boundary while preserving its exact provisioning evidence. MUA
does not use `x360ue3`.

### S043 — x360port replacement

Evidence: the predecessor repository was migrated in place to `shared/x360port`
at commit `538c51f`, its GitHub repository and local checkout were renamed, and
its old host runner, standalone guest memory, static dispatch ABI, title adapter,
and generated-function-map ownership were deleted. The remaining verified
library is explicitly `x360port_validation`; it cannot be mistaken for an Xenia
executor, which remains missing under S040.

### S044 — MUA shared Alchemy consumption

Missing capability: MUA has no build, source, launcher, tool, or runtime
dependency on `shared/alchemy`. After every X-Men 2 goal passes, MUA must reuse
the proven shared engine contracts through Alchemy's x360 adapter over
`x360port`, keeping title addresses, executable identity, and policy local.
The neutral Alchemy component remains independent of `x360port`, and MUA links
no x86 adapter.

### S045 — x360ue3 platform-engine layer

Missing capability: create an independently authored `shared/x360ue3` consumer
of `x360port` for reusable UE3 Xbox platform contracts. It owns versioned UE3
ABI descriptions, RHI semantic operations, and object/resource/thread/frame
lifetime interfaces, while all exact Gears addresses, shader hashes, pass
rosters, navigation, save policy, gameplay, and application composition remain
in `GearsUE3`. The local `shared/ue3` reference checkout is never a build,
runtime, or distribution dependency.

### S050 — gcnport executor

Missing capability: integrate Dolphin's dynarec behind a title-neutral GameCube
executor with robust address hooks, runtime invalidation, and host qualification.

### S051 — Sunbright

Missing capability: boot exact `GMSE01` and prove the J3D draw hook through
Dolphin's dynarec, then remove roughly 2.79 million generated source lines.

### S060 — Kirbh deferred runtime

Missing capability: replace the competing WIP execution paths with one clean
`gbaport` native/emulator architecture around a maintained GBA core, then prove
representative gameplay correctness and performance on each released host. This
project is explicitly deferred and is not a near-term stream.

### S061 — Kirbh split-screen

Missing capability: provide drop-in split-screen multiplayer on Kirbh's single
authoritative runtime, with explicit join/leave, camera, input, and shared-state
ownership.

### S062 — Kirbh wider camera

Missing capability: widen the gameplay camera through deterministic projection,
viewport, scissor, and proven culling ownership without stretching the final
image or using frame-aware heuristics.

### S070 — amigaport executor

Missing capability: integrate a maintained 68000 interpreter with complete
PC/SR/exception state, bounded execution, active-image identity, and scoped
native/original calls, then qualify representative gameplay on each host.

### S071 — Benefactor

Missing capability: reach current title synchronization through `amigaport`
across four address-reusing images, then remove generated C and generated-body
native calls.

### S080 — nesport executor

Missing capability: integrate a maintained 6502 interpreter with complete
PC/status/interrupt/cycle state and override identity based on physical MMC3
ROM mapping, then qualify representative gameplay on each host.

### S081 — Mimp

Missing capability: first execute RESET through title/attract through `nesport`,
after first deleting 16,664 generated functions and generated-call coroutine
ownership; then prove representative gameplay. A maintained Nestopia execution
core may be the product CPU, while an independently composed oracle remains the
comparison owner.
