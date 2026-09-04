# Native/dynarec hybrid migration

This is the portfolio execution plan. It replaces the retired static-recompiler
plan rather than preserving it as an alternative.

## Product contract

Every affected game becomes one product with cooperating runtime execution paths:

1. **Native path:** verified host implementations replace selected guest
   functions or subsystems.
2. **Guest CPU path:** PSX, x86, GameCube, and Xbox 360 translate ordinary
   guest blocks on demand. NES, GBA, and Amiga may use a maintained interpreter.

Dynarec-class products follow DuckStation's shape: compile ordinary cold blocks
before execution, but permit a bounded interpreter fallback when compilation or
safe fetch is explicitly refused. Every fallback reports a typed reason, guest
PC, and block/instruction counts before returning to JIT dispatch. Interpretation
is never the default mode, a profiling first pass, an asynchronous compilation
bridge, or proof of a missing host backend. NES/GBA/Amiga may deliberately use
an interpreter as their default CPU only after representative gameplay—not boot,
menus, logos, or video—meets the correctness/performance budget on each host.

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

## Break-first migration and evidence gate

Boot, logos, menus, attract loops, and FMV are checkpoints, not representative
gameplay. In particular, a headless/no-audio/no-present FMV timing run with no
native overrides active cannot qualify a product engine.

Before dynarec implementation starts in a title, preserve only independently
useful binary/behavior evidence, native subsystem contracts, and oracle
scenarios. Then delete its generator, generated corpus, static dispatcher,
generation-only seeds, static-only tests/config/selectors, and stale methodology.
The build may—and should—fail at one explicit missing runtime-executor boundary;
the old product is not kept runnable as a bridge or comparison arm.

After that break-first change, one bounded dynamic milestone must prove:

- a fresh checkout can provision from the user's game asset and build without a
  translator or generated guest corpus;
- the authenticated title reaches at least its current verified frontier;
- a dynarec-class product executes nonzero translated blocks, reports fallback
  blocks/instructions by reason, and reaches the discriminator primarily through
  JIT execution; or a low-power interpreter-class product meets its declared
  representative-gameplay correctness and performance budget;
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
analysis, or a separately built test oracle. Representative-gameplay
conformance completes the dynamic migration; static execution is already absent
and none may be restored as a compatibility mode or oracle.

## Platform architecture and order

### 1. x86-32: finish the existing dynamic path

`shared/x86port` already contains an x86-32 interpreter and an x64 JIT.
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

X-Men 2 also establishes the first product use of the intended shared Alchemy
engine. `shared/alchemy` already contains partial `alchemy`, `alchemy_input`,
and optional `alchemy_input_sdl` libraries plus XMLB/ARK tooling, but `x2native`
currently links and calls none of those runtime targets. Integrate the narrow
`alchemy_input`/guest `igControllerManager` adapter first and A/B it against the
retained DirectInput path. Move further engine ownership only after a concrete
shipping path proves the interface.

Keep Alchemy as one repository with three component families rather than
creating `x86alchemy` and `x360alchemy` repositories:

```text
               alchemy/shared
                /           \
       alchemy/x86       alchemy/x360
             |                 |
          x86port           x360port
```

The arrows are dependency direction: each optional adapter consumes its
platform framework's public interfaces, while `alchemy/shared` depends on
neither. Exact addresses, executable identities, and title policy stay in
X-Men 2 and MUA. The title repository composes and pins Alchemy plus the one
relevant platform framework; Alchemy itself must not carry both frameworks as
submodules or link an unused adapter into a product.

### 2. PSX: integrate the proven dynarec, then migrate one title at a time

Integrate a maintained, pinned Lightrec fork directly into a per-`Core` executor.
Ordinary cold blocks compile synchronously; Lightrec's interpreter is retained
only for reason-coded refused-block fallback, not its upstream first-pass or
interpret-while-compiling behavior. Lightrec owns its
cache and executable memory. `psxport` owns machine-state synchronization,
HLE/device callbacks, image-aware overrides, original calls, bounded exits, and
invalidation plus translated/fallback telemetry.

Prove the shared contract first with one resident override and two overlay images
that reuse an address. Then finish one title before starting another. Tomba! 2
is first because it exercises resident code, overlays, many native calls, and an
existing boot-to-gameplay frontier. Tomba! 1 follows in the same repository;
the other PSX titles all depend only on the shared executor, and any execution
order is coordination policy here rather than a factual dependency.

### 3. Xbox 360: build `x360port`, then layer UE3 and title engines

Use Xenia's existing x64 and A64 dynarec backends and their bounded fallback;
do not write a second PPC interpreter or put Xenia behind `jit-common` caches. `x360port` owns a
narrow executor around Xenia `Memory`, `Processor`, `ThreadState`, `RawModule`,
typed imports, device-memory callbacks, runtime overrides, and original calls.
Account explicitly for Xenia's process-global memory/MMIO/clock assumptions.

The former shared Xbox host has been migrated in place into `x360port`, carrying
forward only its useful authenticated-image and import-validation contracts. Do
not reintroduce its precomputed generated function map or title-owned concrete
PPC ABI; no separate host owner remains.

Gears and MUA are both first-class `x360port` consumers. Break both static paths
first, preserving only independent evidence and native subsystem contracts,
then drive the shared executor from each title's first discriminator so the
framework cannot silently acquire Gears-only policy.

Gears has two additional owners above the platform layer:

```text
Gears title/revision adapters + GearsUE3
                    |
                    v
        shared/x360ue3
                    |
                    v
        shared/x360port -> Xenia dynarec
```

`x360ue3` is an independently authored clean-code Xbox 360 UE3 integration. It
owns reusable, versioned UE3 ABI descriptions, UE3 RHI semantic operations,
and object/resource/thread/frame lifetime contracts over public `x360port`
interfaces. It does not own exact title addresses, shader hashes, pass rosters,
navigation, save policy, gameplay rules, or application composition; those stay
in `GearsUE3`. The existing local `shared/ue3` checkout is reference material,
not a source, build, runtime, or distribution dependency.

MUA is an Alchemy title, not a UE3 title. Its dependency is
`MUA -> x360port -> Xenia`, plus `MUA -> shared/alchemy` for native engine
services. MUA's dynarec migration is active now. Only its Alchemy adoption waits
for X-Men 2 to prove the corresponding shared contracts; MUA does not build a
second title-local Alchemy engine and never depends on `x360ue3` or `GearsUE3`.
When that gate opens, MUA links Alchemy's x360 adapter; it does not make the
neutral Alchemy core depend directly on Xenia or `x360port`.

### 4. GameCube: build `gcnport` around Dolphin

Use Dolphin's dynarec and runtime image ownership. Add a guest-address native
hook that remains correct across block shapes, chaining, invalidation, and host
architectures; do not revive the previously removed generated path. Sunbright
is the first and only title until its declared gate is complete.

### 5. Amiga and NES

`amigaport` may use a maintained 68000 interpreter with complete
PC/SR/exception state and override keys that include the active loaded image.
Its first consumer is Benefactor; representative gameplay must qualify the
interpreter on every released host.

`nesport` may use a maintained 6502 interpreter with complete
PC/status/interrupt/cycle state and override identity based on the physical MMC3
ROM mapping rather than CPU address alone. Its first consumer is Mimp;
representative gameplay must qualify the interpreter on every released host.

### Deferred: Kirbh

Kirbh remains in scope but is not a near-term implementation stream. Its durable
goals are one clean `gbaport` native/emulator runtime, drop-in split-screen
multiplayer, and a wider gameplay camera implemented at projection/viewport/
scissor and proven culling boundaries rather than by stretching. Its competing
WIP product paths are not promoted into the new foundation.

## Audited first implementation discriminators

These checkpoints establish that the replacement executor is wired to real
title code after the static product has already been deleted. They do not
complete a migration by themselves; each title still needs the representative-
gameplay evidence gate above.

| Project | First implementation discriminator |
| --- | --- |
| X-Men 2 | Reconfirm the no-generated JIT default through representative interactive gameplay and native overrides; report bounded fallback with denominators; sync the canonical framework; link and call the first `shared/alchemy` runtime contract through the `igControllerManager` adapter. |
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
| Gears of War | After deleting the static executor/corpus, execute real leaf `0x8222E868`, one typed import, and disabled/enabled/super override paths through `x360port`; put reusable UE3/Xbox semantics in `x360ue3` and exact Gears behavior in `GearsUE3`. |
| Marvel: Ultimate Alliance | After deleting its static product surfaces, execute the exact Gold XEX entry `0x824806D8` through `x360port` until the first named missing service with nonzero Xenia JIT blocks. Adopt `shared/alchemy` only after X-Men 2 proves each shared engine contract. |
| Sunbright | Boot exact `GMSE01` and prove the `J3DShape::draw` hook at `0x802e0390` through Dolphin's dynarec without generated guest code. |
| Benefactor | Reach current title synchronization through a maintained 68000 core across its four address-reusing images, then qualify representative interactive gameplay performance. |
| Mimp | Execute RESET through title/attract using a maintained 6502 core, physical MMC3 block identity, and no host-stack/generated-function coroutine model; then qualify representative gameplay performance. |

## Planning-before-implementation gate

Before implementation resumes, each affected repository must have truthful
goals, project state, and ownership mapping. Replace its static migration plan;
do not append a contradictory “dynamic” section. Preserve only independently
verified binary addresses, behavior, test scenarios, and native subsystem
contracts. The central state item S005 remains current until those local plans
are coherent and their validators pass.
