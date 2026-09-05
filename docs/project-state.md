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
| S013 | Obsolete `shared/recomp-x86` is removed after all consumers leave it | verified | — | G005 |
| S014 | `shared/alchemy` is one product-ready neutral engine with separately linked x86/x360 platform adapters and typed configuration/logging boundaries | partial | — | G007 |
| S015 | X-Men 2 links and calls a proven `shared/alchemy` contract through its x86 adapter in representative gameplay | missing | S014 | G007 |
| S020 | `psxport` owns a per-`Core` PSX dynarec, counted refused-block fallback, runtime overrides, scoped original calls, bounded exits, and code invalidation | partial | S005 | G001, G002, G004 |
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
| S040 | `x360port` executes Xbox 360 guest code through Xenia's x64/A64 dynarecs and owns runtime native-call boundaries | partial | S005 | G001, G002, G004, G008 |
| S041 | Gears of War reaches its current frontier through `x360port` and `x360ue3`, with the static PPC pipeline absent | missing | S040, S045 | G001, G003, G008 |
| S042 | Marvel: Ultimate Alliance reaches its current frontier through `x360port` with the static PPC pipeline absent | missing | S040 | G001, G003, G008 |
| S043 | The former shared Xbox host is replaced in-place by `x360port` without a generated-function-map contract | verified | — | G002, G005, G008 |
| S044 | MUA consumes and extends the Alchemy contracts proved by X-Men 2 without creating a parallel title-local engine | missing | S015, X-Men 2 complete goals | G007 |
| S045 | `x360ue3` provides a clean title-neutral UE3-on-Xbox-360 layer between `x360port` and `GearsUE3` | missing | S040 | G008 |
| S050 | `gcnport` executes GameCube guest code through Dolphin's dynarec and owns robust runtime hooks | partial | S005 | G001, G002, G004 |
| S051 | Sunbright reaches its current frontier through `gcnport` with its generated corpus removed | missing | S050 | G001, G003 |
| S060 | Kirbh has one clean `gbaport` native/emulator product architecture | missing | S005 | G001, G002, G006 |
| S061 | Kirbh provides drop-in split-screen multiplayer | missing | S060 | G003, G006 |
| S062 | Kirbh provides a wider gameplay camera without final-image stretching | missing | S060 | G003, G006 |
| S070 | `amigaport` owns a maintained 68000 interpreter with image-scoped identity and complete CPU state | partial | S005 | G001, G002 |
| S071 | Benefactor reaches its current frontier through `amigaport` with its generated corpus removed | missing | S070 | G001, G003 |
| S080 | `nesport` owns a maintained 6502 interpreter with physical MMC3 mapping identity and complete execution state | partial | S005 | G001, G002 |
| S081 | Mimp reaches its current frontier through `nesport` with its generated corpus removed | missing | S080 | G001, G003 |

Every title-completion item above includes the representative interactive
gameplay gate in G003. The boot/menu/leaf checkpoints in `docs/migration.md` are
first implementation discriminators, not completion evidence. Static execution
is deleted before those discriminators and is never restored as a bridge.

## Host CI support

The host CI workflow exercises the checked-in code-memory and block-cache tests
with a complete checkout history and no game assets:

| Host | State | Evidence or exact gap |
| --- | --- | --- |
| Linux x86-64 | supported | `.github/workflows/ci.yml` configures and runs the CMake tests with Clang. |
| Windows x86-64 | supported | Hosted run `33890983817` at `4512a2054b0ecf737b6dd0b03f23713d23550b3c` passed clang-cl compilation plus the native Windows executable-memory and block-cache tests. |
| macOS arm64 | supported | `.github/workflows/ci.yml` configures and runs the CMake tests with Apple Silicon Clang and the MAP_JIT path. |
| macOS x86-64 | supported | `.github/workflows/ci.yml` configures and runs the CMake tests with Intel Apple Clang and the MAP_JIT path. |
| Android arm64-v8a | missing | The code-memory layer is relevant to Android dynarecs, but there is not yet an NDK build plus native device/emulator runtime discriminator for executable publication, write protection, and instruction-cache coherence. `shared/android-port` should supply that device boundary; configure-only evidence is intentionally absent. |

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

Integration with the Windows/host-CI mainline is verified locally on Linux x86-64
with Clang: all three CTests pass, including format/tidy. Code-memory execution
passes 905 checks through the default mapping and both available forced mechanisms
(dual-mapped memfd and mprotect), including bounded range publication and unchanged
earlier code. The subsequent build performs no compilations. This combined-tree
result does not replace native macOS/Windows hosted verification or prove concurrent
patching or title performance.

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

Evidence: a repository-wide live-source scan found no consumer dependency;
only this retirement record and consumer source-policy refusals remained. The
GitHub repository was deleted and the clean local checkout at commit `980038c`
was moved to the desktop trash on 2026-09-04 rather than retained as legacy.

### S014 — shared Alchemy engine readiness

At committed `shared/alchemy` revision `123566b` ([canonical state](../../alchemy/docs/project-state.md)),
format/render-data/input libraries and XMLB/ARK tooling remain partial, while
the shipping configuration, diagnostics, language, and dependency boundaries
are verified by its S013 gate. Gap: no gameplay consumer proves a stable
runtime API, and each consumer still needs one immutable runtime/tooling pin.
Keep one
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

Partial capability at committed `psx/psxport` revision `992dbfb2` ([canonical state](../../../psx/psxport/docs/project-state.md))
with its Lightrec fork at `b1457137`: the synthetic shipping executor runs nonzero
translated blocks, transfers architectural state, counts reason-coded bounded
fallback, routes scoped original calls, and invalidates changed executable
ranges. The product boundary excludes offline guest generation and a selectable
interpreter. Gap: no real title has yet passed the integrated native-service,
representative-gameplay, or AArch64 gates. This entry intentionally cites only
committed HEAD evidence.

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

Missing capability: `psx/spider1` revision `84a3ac25` ([canonical state](../../../psx/spider1/docs/project-state.md)) has a partial native/Lightrec
composition and a title-local route that reaches `dem1`, but it has not reached
that checkpoint through Lightrec. Representative gameplay and resumable
runtime guest execution remain unverified.

### S027 — Spyro

Missing capability: reach the stage-13 800/900 route through the PSX dynarec and
replace the generated world body with runtime guest execution.

### S028 — Mega Man X4

Missing capability: `psx/megamanx4` revision `75630ba` ([canonical state](../../../psx/megamanx4/docs/project-state.md)) records the native/Lightrec
target and measured native field-service seams, but no committed product run
reaches the 4,000-field frontier through Lightrec. Authenticated runtime VSync
interception and representative gameplay remain unverified.

### S029 — Tekken 3

Missing capability: `psx/tekken3` revision `7d8b0e5` ([canonical state](../../../psx/tekken3/docs/project-state.md)) retains deterministic
entry/CD evidence and the native/Lightrec target, but no committed product run
reaches `NAMCO PRESENTS` through the dynarec with all 14 original calls. The
representative gameplay gate remains unverified.

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

Partial capability at committed `shared/x360port` revision `84bb697` ([canonical state](../../x360port/docs/project-state.md)): the
title-neutral context authenticates and registers a synthetic Xenia `RawModule`,
executes authenticated PPC through Xenia's x64 dynarec, and crosses typed
function/variable imports. Gap: runtime device callbacks, overrides/original
calls, executable-write invalidation, A64 host qualification, and real-title
gameplay remain missing. This is synthetic framework evidence, not Xbox title
evidence.

### S041 — Gears of War

The generated PPC corpus, static translator dependency, function maps, and
static-only tooling are absent. Missing capability: execute the audited guest
leaf/import/override round-trip through `x360port`, route reusable UE3/Xbox
semantics through `x360ue3`, and reach the current Gears frontier through
`GearsUE3`.

### S042 — Marvel: Ultimate Alliance

Missing capability: MUA's static product surfaces are already removed at
committed revision `c976a92` ([canonical state](../../../x360/mua/docs/project-state.md));
the remaining title gameplay migration must execute exact Gold XEX entry
`0x824806D8` through `x360port`/Xenia to a named service boundary while
preserving its provisioning evidence. MUA title gameplay is deferred until
every X-Men 2 goal passes; cleanup and shared-foundation work may continue.
MUA does not use `x360ue3`.

### S043 — x360port replacement

Evidence: the predecessor repository was migrated in place to `shared/x360port`
at committed revision `84bb697` (following the predecessor migration), its
GitHub repository and local checkout were renamed, and
its old host runner, standalone guest memory, static dispatch ABI, title adapter,
and generated-function-map ownership were deleted. S040 now records the partial
Xenia executor: synthetic x64 execution and typed imports are present, while the
remaining device, override, invalidation, A64, and title gates are open.

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

Partial capability at committed `shared/gcnport` revision `70ec0b0` ([canonical state](../../gcnport/docs/project-state.md)): the
framework policy, authenticated hook/original state machine, and synthetic
Dolphin JIT cold/cache/hook/invalidation execution are verified on hosted
desktop targets. Gap: six of eleven embedding-contract operations, typed
fallback, and a complete shipping adapter remain missing; this does not qualify
Sunbright gameplay.

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

Partial capability at committed `shared/amigaport` revision `3233479` ([canonical state](../../amigaport/docs/project-state.md)): the
maintained PUAE owner exposes the callback-driven CPU boundary, image-scoped
native/original dispatch, and bounded telemetry, with focused synthetic tests
covering the legal opcode table and core state. Gap: exact bus/address-error
frame construction and device-reset conformance remain open; no Benefactor
gameplay evidence exists.

### S071 — Benefactor

Missing capability: reach current title synchronization through `amigaport`
across four address-reusing images, then remove generated C and generated-body
native calls.

### S080 — nesport executor

Partial capability at committed `shared/nesport` revision `4f6c00c` ([canonical state](../../nesport/docs/project-state.md)): the
maintained MesenCE dependency, complete machine-state boundary, physical
MMC3-scoped dispatch, and synthetic runtime tests are present. Gap: host
portability and a real user-supplied ROM's representative gameplay remain
unverified.

### S081 — Mimp

Missing capability: first execute RESET through title/attract through `nesport`,
after first deleting 16,664 generated functions and generated-call coroutine
ownership; then prove representative gameplay. A maintained Nestopia execution
core may be the product CPU, while an independently composed oracle remains the
comparison owner.
