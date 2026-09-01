# Project state

Factual progress ledger for the static-recompilation → JIT migration. Epic
intent is `docs/project-goals.md`; the architecture is `docs/migration.md`;
atomic work is `docs/issues/`.

**A new session starts here.** Read the current focus, then the row for the item
it names, then that item's detail section. Update this file in the same change
that advances an item.

## Comparison baseline

The baseline is the current static-recompilation pipeline shared by all these
ports: a per-project translator converts the whole guest binary to C at build
time (`psxport/tools/recomp`, `shared/recomp-x86`, `XenonRecomp`, sunbright's
`sms-recomp`), the generated corpus is compiled into the port, and hand-written
native overrides replace individual generated bodies. Changing the translator
regenerates and recompiles tens to hundreds of megabytes of C; an unimplemented
instruction fails the build; and the differential harness diffs generated C
against a reference emulator per function.

Every difference this migration introduces — runtime translation, per-framework
ownership, per-framework verification, host-architecture reach, and the shared
translation cache — is a separate row below.

## Current focus

S011 is the current focus.

## Capability inventory

| ID | Capability or outcome | State | Factual dependency | Goals |
| --- | --- | --- | --- | --- |
| S001 | `jit-common` provides W^X executable code memory, including dual-mapping where anonymous RWX is refused | missing | — | G002, G004 |
| S002 | `jit-common` provides a block cache with chaining, eviction, and invalidation on self-modifying code and bank/overlay switches | missing | S001 | G002 |
| S003 | `jit-common` provides a persistent translation cache that is key-bound, verifiable, and safe to discard | missing | S002 | G002, G004 |
| S004 | `jit-common` provides a reusable override table with gameplay scoping, an A/B disable, and the evidence gate | missing | — | G001, G002 |
| S005 | `jit-common` provides harness helpers: register-file differ, trace ring, deterministic-replay scaffolding, first-divergence reporting | missing | — | G002, G003 |
| S006 | `render-common` provides a neutral render IR and host backends shared by every framework | missing | — | G002 |
| S007 | `android-runtime` owns the title-neutral APK runtime, extracted out of Lucent | missing | — | G002, G004 |
| S008 | Lucent is reduced to logging, configuration, HTTP control, and ZIP, with no Android or SDL surface | missing | S007 | G002 |
| S010 | `psxport` has a reference MIPS interpreter available as a divergence-diagnosis engine | partial | — | G003 |
| S011 | `psxport` executes guest MIPS code through a lightrec-based JIT | missing | S001, S002 | G001 |
| S012 | `psxport` runs its JIT in lockstep against the substrate Core and pauses at the first divergence | partial | S011 | G003 |
| S013 | `psxport`'s static translator, generated corpus, and regeneration step are removed | missing | S011, S014 | G001 |
| S014 | The PSX titles reach their existing conformance milestones through the JIT | missing | S011, S012 | G001, G003 |
| S020 | `xenonport` exists as a platform framework, absorbing `shared/xenon-host` and gears1's title-neutral 360 layer | missing | — | G002 |
| S021 | `xenonport` executes guest Xenon code through Xenia's CPU backend | missing | S001, S002, S020 | G001 |
| S022 | `xenonport` can lockstep its JIT against another engine and pause at the first divergence | missing | S005, S020 | G003 |
| S023 | `x360/gears1` reaches boot and in-game conformance through the JIT, with XenonRecomp removed | missing | S021, S022 | G001, G003 |
| S024 | `xenonport` runs on ARM64 hosts (Apple Silicon, Android) | missing | S021 | G004 |
| S030 | `gcnport` exists as a platform framework, extracted from `sunbright`'s title-neutral GameCube/Wii layer | missing | — | G002 |
| S031 | `gcnport` executes guest Gekko/Broadway code through Dolphin's JIT | missing | S001, S002, S030 | G001 |
| S032 | `gcnport` can lockstep its JIT against another engine and pause at the first divergence | missing | S005, S030 | G003 |
| S033 | `sunbright` reaches its file-select conformance milestone through the JIT, with its static translator removed | missing | S031, S032 | G001, G003 |
| S040 | `x86port` exists as a platform framework, replacing `shared/recomp-x86` | missing | — | G002 |
| S041 | `x86port` translates x86-32 to x86-64 hosts, including lazy-EFLAGS evaluation | missing | S001, S002, S040 | G001 |
| S042 | `x86port` translates x86-32 to ARM64 hosts | missing | S041 | G004 |
| S043 | `x86port` has a reference x86-32 interpreter as the authority on the semantics its translator must match | missing | S005, S040 | G003 |
| S044 | `x86port` implements a DirectDraw guest graphics frontend lowering to `render-common` | missing | S006, S040 | G002 |
| S045 | `x86port` implements a Direct3D 8 guest graphics frontend lowering to `render-common` | missing | S006, S040 | G002 |
| S046 | `pc/lf2` reaches its existing conformance milestone through the JIT | missing | S041, S043, S044 | G001, G003 |
| S047 | `pc/xmen2` reaches its existing conformance milestone through the JIT | missing | S041, S043, S045 | G001, G003 |
| S050 | The `recomp-*` skill family is replaced by `jit-*` describing the JIT architecture | missing | — | G005 |
| S051 | Global instructions carry no static-recompilation-only rules and record the revised shared-repo and Android ownership tables | missing | S007, S050 | G005 |
| S060 | The second-wave ports (`kirbh`, `benefactor`, `mimp`) run on platform frameworks of their own | missing | S001, S002 | G001 |

## Capability details

### S001 — Executable code memory

Required capability: reserve/commit/protect code regions, flip RW↔RX, dual-map a
`memfd` where anonymous RWX is refused (Android SELinux `execmem`, hardened
Linux), flush the instruction cache at correct granularity for ARM64 hosts, and
allocate blocks within branch-displacement range of one another. Nothing exists
yet; every framework needs this before it can emit a single instruction.

### S002 — Block cache

Required capability: a guest-address → translated-block container with block
chaining, eviction, and invalidation driven by the framework on self-modifying
code, bank switches, overlay loads, and DMA into code memory. Design the negative
first: a cache that silently misses must be distinguishable from one that works.

### S003 — Persistent translation cache

Required capability: an on-disk format keyed on guest image hash, framework
version, core revision, codegen configuration, host architecture, and the
override-set digest; a load path that discards wholly rather than partially
trusting; refusal to persist blocks from writable or unidentified overlay
regions; position independence or a recorded code-region base; and a
verification mode that re-translates in parallel and asserts agreement. Build it
after the first framework runs a real title, so the format is designed against a
real block layout.

### S004 — Override table

Required capability: `(guest address → native function)` registration, gameplay
scoping, a wholesale A/B disable, and the evidence gate that refuses an override
without a proven divergence or a named reason. The super-call is
framework-supplied via callback, because "run the guest function" means
something different per core. Frameworks may use this or keep their own; it is a
library, not a mandate.

### S005 — Harness helpers

Required capability: a register-file differ over a framework-supplied layout, a
trace ring buffer, deterministic-replay scaffolding, and first-divergence
reporting. These are what make S012/S022/S032/S043 possible without each
framework rewriting the comparison logic.

### S006 — `render-common`

Required capability: a neutral render command stream (resources, state, draws,
passes, resolves) and the host backends behind it (Vulkan, SDL_GPU, headless
capture), so device/queue/swapchain/pipeline-cache management and the frame graph
are written once. gears1's `runtime/native_rhi.*` already proves the shape and is
the extraction candidate. Open question in `migration.md` §13.5: extract now, or
let `psxport`'s `gpu_vk` be the second consumer that shapes it.

### S007 — `android-runtime`

Required capability: a shared library owning the title-neutral APK runtime — SDL
Activity lifecycle, app-private user-data handoff, persisted SAF grants and
bounded staging, raw touch-contact capture and cancellation, window insets and
lifecycle — extracted from Lucent with its tests. Touch *meaning* stays with the
framework/title input policy. Every Android consumer must be repointed atomically
and the global Android ownership table rewritten in the same change.

### S008 — Lucent reduced

Required capability: Lucent contains logging, configuration, the loopback HTTP
control channel, and ZIP safety, with `touch.h`, `content.h`, `platform.h`, and
`platform_c.h` gone to S007. Today a consumer that wants a logger inherits an
SDL/JNI/Android-SDK surface, which is the cohesion defect this fixes.

### S010 — `psxport` reference MIPS interpreter

Established by inspection 2026-09-01; this corrects an earlier assumption that
the interpreter was merely a component with no oracle role.

What exists and works:

- `runtime/recomp/interp.cpp` — a flat MIPS R3000 interpreter operating on
  per-Core memory, whose instruction semantics match the emitter bit-for-bit.
- It is already **the oracle engine**: `Core::use_interp` routes
  `rec_super_call` / `rec_interp` / `rec_coro_run` / `stub_dispatch` to it
  (`dispatch.cpp`), and `guest_call.h` carries the same ternary at every guest
  call site.
- `runtime/recomp/sbs.cpp` already runs two Cores in lockstep with per-frame
  RAM+scratchpad diff, pause-at-first-divergence, and debug-server inspection.
- `PSXPORT_SELFTEST=oracle` (`selftest.cpp`) boots the interpreter Core with a
  software rasteriser and asserts it reaches GAME and plays the intro cutscene.
  Its own documentation records that the interpreter runs **scenes the recomp
  substrate cannot**: the un-recompiled overlay code the recompiler misses, and
  the mis-emitted in-function `jr` that froze the recomp GAME loop (later-272).

That last point is the strongest in-tree argument for this migration: on this
project the interpreter is already more correct than the static substrate.

Ground truth on which substrate ships: `Core::use_interp` defaults to `0`
(`core.h:146`), so the native port Core runs `rec_dispatch` → generated C. Every
consuming title still generates and compiles its shards (`tekken3`, `crash`,
`spyro`, `toystory2`, `crashbash`, and others reference the recomp pipeline in
their `CMakeLists.txt`). `interp.cpp`'s header previously claimed the recompiler
had been dropped from the build; that claim contradicted `dispatch.cpp`, the
`use_interp` default, and every title build, and has been corrected in place.

The engine selection this depended on **landed 2026-09-01** (I001): `Core::engine`
is now an `Engine {Substrate, Interpreter, Jit}` enum over a total routing policy
in `runtime/recomp/engine_select.h`, and the JIT arm refuses loudly rather than
falling back. psxport's full gate passes 133/133, and `PSXPORT_SELFTEST=oracle` passes on
Tomba2Engine built against this tree (GAME loop +4174/4200 frames, SOP scene id=7)
— so the interpreter's ability to run the cutscene the substrate cannot is
confirmed by a run, not just by the selftest's documentation.

Gap: two things remain before the interpreter is a usable comparison partner for
a JIT. (1) There is no JIT to compare against — S011. (2) `interp.cpp` holds
per-Core guest register tags in a process-global array (I002), which breaks any
pairing where both Cores interpret. Neither blocks S011.

### S011 — `psxport` lightrec JIT

Required capability: guest MIPS R3000 execution through lightrec, bound to
`psxport`'s own memory map, GTE, and MMIO rather than Beetle's bus. lightrec is
chosen over Beetle's dynarec because it is standalone, embeds with host-supplied
memory callbacks, and has both x86-64 and ARM64 backends; PSX Android is a named
goal, so host portability decides this. Beetle remains vendored as an interpreter
oracle only.

The seam already exists and is narrow: every guest call goes through
`guest_call.h`'s `c->use_interp ? rec_interp(c, fn) : rec_dispatch(c, fn)`, and
`dispatch.cpp` holds the same branch for the four entry points. Replacing that
boolean with an execution-engine selector adds the JIT route in one place rather
than threading a new path through the runtime — so this is far less invasive on
`psxport` than the equivalent step will be elsewhere.

### S012 — `psxport` JIT lockstep divergence diff

`runtime/recomp/sbs.cpp` already runs two Cores in lockstep with identical input,
a per-frame RAM+scratchpad diff, pause-at-first-divergence, and debug-server
inspection (`sbs diff` / `bt` / `watch` / `show` / `step`). Nothing new has to be
built to compare engines.

Gap: it can pair an interpreter Core against a substrate Core, but there is no
JIT Core to select yet, so the pairing that matters for this migration —
JIT-vs-substrate — is not reachable. Closing it is a consequence of S011 plus
I001's engine selector, not separate work.

Deliberately NOT in scope: standing up a reference interpreter to validate
lightrec. Per USER 2026-09-01 the core is already proven; the risk is our binding
of it, which JIT-vs-substrate covers.

### S013 — `psxport` static translator removed

Required capability: `tools/recomp/` (`decode.py`, `emit.py`, `psexe.py` and
their tests) and the generated `generated/port/` trees are deleted, the
regeneration build step is gone, and every doc reference is updated. Blocked
behind S011 and S014 by sequencing, not by a defect: the static path stays until
the JIT has reached conformance.

### S014 — PSX titles through the JIT

Required capability: the PSX titles (`Tomba2Engine`, `spyro`, `tekken3`,
`crash`, `crashbash`, `ctr`, `megamanx4`, `toystory2`, `spider1`, `vagrant`,
`coord`, `c12`) reach the conformance milestones they hold today, running on the
JIT, with each project's residual/known-divergence list re-derived rather than
carried over. One framework migration covers all of them.

### S020 — `xenonport` framework

Required capability: a platform framework owning the title-neutral Xbox 360
layer — Xenon CPU execution, the Xenos PM4 frontend and Xenia's shader
translator, kernel/XAM HLE, XMA audio, and disc/XEX identity. `shared/xenon-host`
moves inside it as the image/binding layer; it has one consumer family, so a
separate shared repo buys nothing. `gears1` and `mua` become titles over it.

### S021 — `xenonport` Xenia CPU execution

Required capability: guest PPC Xenon + VMX128 execution through Xenia's
`xe::cpu::Processor`, which is already designed to be embedded with a memory
object and an export resolver. This replaces XenonRecomp entirely.

### S022 — `xenonport` lockstep divergence diff

Required capability: run the Xenon JIT in lockstep against another engine over
identical input and pause at the first divergence, so a visible defect can be
root-caused. During the migration the other engine is the existing XenonRecomp
substrate. Xenia's own PPC interpreter is available and free to keep as a
stepping engine, but building interpreter-oracle capability to validate Xenia's
JIT is explicitly not the goal (`migration.md` §5).

### S023 — `gears1` through the JIT

Required capability: Gears of War 1 boots and reaches in-game through the JIT,
`extern/XenonRecomp` and `config/gears.toml` are removed, and the project's
residual list is re-derived under the new oracle.

### S024 — `xenonport` on ARM64 hosts

Required capability: the Xenon guest runs on Apple Silicon and Android at an
acceptable frame rate.

Reclassified from `blocked` to `missing` on 2026-09-01, after USER clarified the
requirement is "no static code generation and something performant — it doesn't
have to be JIT". Xenia's *JIT* backend emits x86-64 only
(`src/xenia/cpu/backend/x64/`), so the JIT route is genuinely unavailable on
ARM64. But Xenia also carries a PPC **interpreter**, which is portable, and the
requirement is performance rather than a JIT specifically. So the honest state is
"not built and possibly slow", not "impossible".

The open question (`migration.md` §13.4) becomes a measurement rather than a
commitment: does an interpreter — switch or threaded — hold frame rate for a
Xenon-era UE3 title on an ARM64 device? Gears of War 1 is the most demanding
target in the whole migration, so this is the row most likely to end up needing a
real ARM64 JIT backend in the Xenia fork. Measure before deciding.

### S030 — `gcnport` framework

Required capability: a platform framework owning the title-neutral GameCube and
Wii layer — Gekko/Broadway execution, Aurora for GX, DVD/DI, OS/DOL HLE, and the
Wii-specific additions when a Wii title arrives. This requires splitting
`sunbright` into framework plus title, which is the largest single extraction in
the migration. `migration.md` §13.3 asks whether it happens before or after the
in-flight file-select milestone.

### S031 — `gcnport` Dolphin JIT execution

Required capability: guest Gekko/Broadway execution through Dolphin's JIT, which
already handles paired singles, `psq_l`/`psq_st`, the graphics-quantisation
registers, and `dcbz`/`dcbz_l` cache-line behaviour, and which has both Jit64 and
JitArm64 backends. `sunbright`'s existing `force_jit` hook is the seed.

### S032 — `gcnport` lockstep divergence diff

Required capability: run the Gekko JIT in lockstep against another engine over
identical input and pause at the first divergence. During the migration the other
engine is sunbright's existing recomp substrate; `sunbright`'s `parity_sweep`
already performs value-level comparison and is the machinery to re-anchor.
Dolphin's PowerPC interpreter is free to keep as a stepping engine; building
interpreter-oracle capability to validate Dolphin's JIT is not the goal.

### S033 — `sunbright` through the JIT

Required capability: Super Mario Sunshine reaches its file-select conformance
milestone through the JIT, with `build-recomp`, `build-sms-recomp`,
`sms-recomp/`, and `run-recomp.sh` removed and the parity findings re-derived.

### S040 — `x86port` framework

Required capability: a platform framework owning x86-32 execution, the Win32
HLE, and the guest graphics frontends. It replaces `shared/recomp-x86`, whose
x86-32 decode and per-instruction semantics are the seed for the translator.
`pc/lf2` and `pc/xmen2` become titles over it, and the original Xbox path later.

### S041 — `x86port` x86-64 emission

Required capability: x86-32 guest code executes fast enough on an x86-64 host.

**Do not start this before measuring** (`migration.md` §5.1). `pc/lf2` is a 1999
2D fighting game and `pc/xmen2` a 2005 action-RPG; a threaded interpreter may hold
frame rate for both, in which case this row and S042 are unnecessary work. If
measurement says a translator is needed, the design is an asmjit-based one with
guest registers in a context struct and lazy-EFLAGS evaluation — chosen over
embedding qemu-TCG because TCG brings its own memory model, cache, and threading
assumptions that fight `jit-common`, whereas an owned translator integrates with
the shared caches and is traceable by the project's own harness.

### S042 — `x86port` ARM64 emission

Required capability: a second asmjit emitter targeting ARM64 so the x86 titles
remain deliverable on Apple Silicon and Android. Separate from S041 because it is
a distinct backend that can regress independently.

### S043 — `x86port` reference x86-32 interpreter

Required capability: an x86-32 interpreter stating the semantics S041 and S042
must match. This is the one framework where an interpreter is genuinely
**required** rather than optional: the other frameworks embed a proven core whose
semantics are already established by a catalogue of shipped titles, whereas here
we write the translator, so nothing else says what correct means
(`migration.md` §5, "Interpreters: required only where we wrote the translator").

### S044 — DirectDraw guest frontend

Required capability: a title-independent DirectDraw implementation (surfaces,
blits, palettes, page flipping) lowering to `render-common`, used by `pc/lf2`.
It implements the API the game actually calls, verified against guest behaviour —
not a per-title fast path that happens to make one screen appear.

### S045 — Direct3D 8 guest frontend

Required capability: a title-independent Direct3D 8 implementation lowering to
`render-common`, used by `pc/xmen2`. Most of what an original Xbox port needs
later, since the Xbox API is D3D8-derived.

### S046 — `pc/lf2` through the JIT

Required capability: Little Fighter 2 reaches its existing conformance milestone
through the JIT with the DirectDraw frontend, and its claims/instruments registry
is re-validated against the new oracle.

### S047 — `pc/xmen2` through the JIT

Required capability: X-Men Legends II reaches its existing conformance milestone
through the JIT with the Direct3D 8 frontend. `docs/strategy.md` currently argues
for static recompilation and must be rewritten in the same change.

### S050 — `jit-*` skills

Required capability: `shared/re-harness/skills/recomp/` becomes `skills/jit/` —
`jit-port`, `jit-init`, `jit-core`, `jit-overrides`, `jit-harness` — describing
the interpreter-first → JIT → override → verify loop, plus guidance on where a
new responsibility belongs in the L1–L4 layering. Rename early; refine the bodies
as each framework lands so they describe what actually worked.

### S051 — Global instructions

Required capability: the global instruction set drops static-recompilation-only
rules ("generated code is sacrosanct", the instruction-coverage build gate),
rewords the no-CI rule to "guest-binary JIT ports" while keeping its reason,
updates the shared-repo table (gains `jit-common`, `render-common`,
`android-runtime`, the frameworks; loses `recomp-x86` and `xenon-host`), and
rewrites the Android ownership table per S007.

### S060 — Second-wave ports

Required capability: `kirbh` (GBA, over mGBA's JIT), `benefactor` (Amiga M68K),
and `mimp` (NES 6502) run on platform frameworks of their own. Deliberately after
the priority targets; listed so the inventory is complete rather than only
covering current work.
