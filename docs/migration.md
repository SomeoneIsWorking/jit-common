# Native/dynarec hybrid migration

This is the portfolio execution plan. It replaces the retired static-recompiler
plan rather than preserving it as an alternative.

## Product contract

Every affected game becomes one product with two cooperating execution paths:

1. **Native path:** verified host implementations replace selected guest
   functions or subsystems.
2. **Dynarec path:** every remaining guest instruction is translated on demand
   from the user's original binary and executed from a runtime code cache.

An interpreter can bootstrap a backend or diagnose a divergence only in a
separately built test target, including diagnostic tests. A gameplay build must
not link it, expose an engine selector for it, or contain a fallback into it.
Product verification therefore includes a link/selector audit as well as
nonzero dynarec execution.

“No static recompilation” means no offline, build-time, install-time, or
provisioning-time emission of guest code as C/C++, object code, or a precompiled
title substrate. Runtime JIT code generation is the required mechanism, not the
forbidden one. A runtime-populated persistent cache is optional disposable user
data and may never be a fresh-install prerequisite.

## Non-negotiable runtime boundaries

- The executor consumes an authenticated runtime image, not generated function
  tables or seed lists.
- Native overrides are keyed by complete guest identity: image/module generation
  plus address wherever overlays, banks, or reloads can reuse an address.
- A normal call honors the override table. A native override's `superCall`
  suppresses only its current override for one call and executes the original
  guest body through the dynarec.
- Installing, removing, or changing an override invalidates any translated call
  path that captured the old decision.
- Guest writes, DMA, overlay loads, bank switches, savestate restore, and other
  executable-memory changes invalidate every affected translated block.
- Host work, VSync/frame suspension, interrupts, exceptions, and thread exit use
  explicit bounded executor exits. Do not assume a C++ exception can unwind
  through generated host frames.
- Each live game instance owns its CPU and override state. Process-global core,
  tag, MMIO, clock, or cache state is a defect unless the embedded core proves
  that singleton contract unavoidable and the product constrains instances.
- Embedded dynarecs own their code memory and block cache. `jit-common` is used
  only for a missing contract that at least two framework integrations actually
  share.

## Evidence gate

Boot, logos, menus, attract loops, and FMV are checkpoints, not representative
gameplay. In particular, a headless/no-audio/no-present FMV timing run with no
native overrides active cannot qualify a product engine.

A title's static path is deleted only when one bounded dynamic milestone proves:

- a fresh checkout can provision from the user's game asset and build without a
  translator or generated guest corpus;
- the authenticated title reaches at least its current verified frontier;
- the dynarec executes nonzero blocks and build/link inspection proves that no
  interpreter is present in the gameplay product;
- native overrides and an override-bypassing original call both run through the
  shipping dispatcher;
- relevant overlay/bank/self-modifying-code invalidation is exercised with a
  positive and controlled-negative case;
- timing, interrupts, memory, and relevant device state are compared against an
  independent oracle at the boundary under test;
- a representative interactive gameplay scenario meets its declared frame-time
  and correctness budget on each released host architecture.

Do not regenerate, build, or run the static product during the migration.
Already-recorded evidence may identify the capability frontier, but new
comparison evidence comes from an independent emulator, hardware, binary
analysis, or a separately built test oracle. Delete the generator, corpus,
static dispatcher, generated-symbol tests, seeds, and methodology when the
replacement reaches representative-gameplay conformance; none remains as a
compatibility mode or oracle.

## Platform architecture and order

### 1. x86-32: finish the existing dynamic path

`shared/x86port` already contains an x86-32 test interpreter and an x64 JIT.
Synchronize its canonical repository with the proven X-Men 2 consumer,
remove `Substrate` from the public engine and documentation, and establish its
project authorities. Then:

1. Re-verify X-Men 2's no-generated default on representative gameplay, not
   merely entry or level-load timing.
2. Migrate Little Fighter 2's existing Win32/DirectDraw host seams onto the same
   JIT without making graphics extraction a CPU prerequisite.
3. Implement and qualify the ARM64 backend required by declared Apple/Android
   hosts.
4. Remove `shared/recomp-x86` after its last real consumer is gone.

### 2. PSX: integrate the proven dynarec, then migrate one title at a time

PSX does not need an interpreter-versus-JIT decision. Integrate a maintained,
pinned Lightrec revision directly into a per-`Core` executor. Lightrec owns its
cache and executable memory. `psxport` owns machine-state synchronization,
HLE/device callbacks, image-aware overrides, original calls, bounded exits, and
invalidation.

Prove the shared contract first with one resident override and two overlay images
that reuse an address. Then finish one title before starting another. Tomba! 2
is first because it exercises resident code, overlays, many native calls, and an
existing boot-to-gameplay frontier. Tomba! 1 follows in the same repository;
the other PSX titles all depend only on the shared executor, and any execution
order is coordination policy here rather than a factual dependency.

### 3. Xbox 360: build `xenonport` around Xenia

Use Xenia's existing x64 and A64 dynarec backends. Do not write a PPC
interpreter and do not put Xenia behind `jit-common` caches. `xenonport` owns a
narrow executor around Xenia `Memory`, `Processor`, `ThreadState`, `RawModule`,
typed imports, device-memory callbacks, runtime overrides, and original calls.
Account explicitly for Xenia's process-global memory/MMIO/clock assumptions.

Absorb the useful authenticated-image and import-validation contracts from
`shared/xenon-host` into `xenonport`; do not retain its precomputed generated
function map or title-owned concrete PPC ABI. Remove the separate owner once no
independent responsibility remains.

Migrate Gears first, then MUA. Preserve their exact title provisioning and
native host behavior while deleting XenonRecomp, generated PPC modules,
precomputed function maps, and switch-target generation.

### 4. GameCube: build `gcnport` around Dolphin

Use Dolphin's dynarec and runtime image ownership. Add a guest-address native
hook that remains correct across block shapes, chaining, invalidation, and host
architectures; do not revive the previously removed generated path. Sunbright
is the first and only title until its declared gate is complete.

### 5. Amiga and NES

`amigaport` must dynamically translate 68000 with complete PC/SR/exception state
and cache/override keys that include the active loaded image. Its first consumer
is Benefactor.

`nesport` must dynamically translate 6502 with complete PC/status/interrupt/cycle
state and cache/override identity based on the physical MMC3 ROM mapping rather
than CPU address alone. Its first consumer is Mimp.

### Deferred: Kirbh

Kirbh remains in scope but is not a near-term implementation stream. Its durable
goals are one clean `gbaport` native/dynarec runtime, drop-in split-screen
multiplayer, and a wider gameplay camera implemented at projection/viewport/
scissor and proven culling boundaries rather than by stretching. Its competing
WIP product paths are not promoted into the new foundation.

## Audited first implementation discriminators

These checkpoints establish that the replacement executor is wired to real
title code. They do not authorize static-corpus deletion by themselves. Each
title still needs the representative-gameplay evidence gate above before the
old path is removed.

| Project | First implementation discriminator |
| --- | --- |
| X-Men 2 | Reconfirm the no-generated JIT default through representative interactive gameplay and native overrides; prove the product neither links nor selects the test interpreter; sync the canonical framework. |
| Little Fighter 2 | Execute its existing product frontier through `x86port`'s JIT with the current HLE/graphics adapter and no generator or generated C. |
| Tomba! 2 | Prove one resident and one colliding-overlay override/original call, then reach the current boot-to-gameplay frontier with all native owners active and no generated files. |
| Tomba! 1 | Reproduce the existing 35-field CRT0 boundary and continue to its current CD/title frontier through the PSX dynarec with six engine-neutral overrides. |
| Crash Bandicoot | Preserve the dirty PadRead/menu work and reach its current product frontier through the PSX dynarec without generated C. |
| Crash Team Racing | Replace generated dispatch and convert frame completion from C++ unwinding to an explicit executor exit, then reach its current frontier. |
| Crash Bash | Preserve 27 runtime override installs and replace 15 generated-body calls with scoped original calls before reaching its current path. |
| Spider-Man | Reach the dem1 checkpoint and replace the generated movie-fiber body with resumable runtime guest execution. |
| Spyro | Reach the stage-13 800/900 route and replace the generated world body with runtime guest execution. |
| Mega Man X4 | Reach the existing 4,000-field front-end/movie frontier; replace movie-body rewriting with authenticated runtime VSync interception. |
| Tekken 3 | Reach `NAMCO PRESENTS` within 1,200 frames with all 14 original calls routed by address. |
| Toy Story 2 | Reach Andy's Room within 120 frames while invalidating all 22 streamed modules across their two reused code slots. |
| Vagrant Story | Reach the 1,000/1,000 TITLE checkpoint with its required native CD override and no guest-VSync violation. |
| C-12 | Execute the authenticated whole program beyond its first VSync/former static miss through the PSX dynarec. |
| Gears of War | Execute real leaf `0x8222E868`, one typed import, and disabled/enabled/super override paths through Xenia before entry-point boot. |
| Marvel: Ultimate Alliance | Execute the exact Gold XEX entry `0x824806D8` until the first named missing service with nonzero Xenia JIT blocks and no generated PPC. |
| Sunbright | Boot exact `GMSE01` and prove the `J3DShape::draw` hook at `0x802e0390` through Dolphin's dynarec without generated guest code. |
| Benefactor | Reach current title synchronization through runtime-translated 68000 blocks across its four address-reusing images. |
| Mimp | Execute RESET through title/attract using physical MMC3 block identity and no host-stack/generated-function coroutine model. |

## Planning-before-implementation gate

Before implementation resumes, each affected repository must have truthful
goals, project state, and ownership mapping. Replace its static migration plan;
do not append a contradictory “dynamic” section. Preserve only independently
verified binary addresses, behavior, test scenarios, and native subsystem
contracts. The central state item S005 remains current until those local plans
are coherent and their validators pass.
