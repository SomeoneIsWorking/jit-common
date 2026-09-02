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
| S001 | `jit-common` provides W^X executable code memory, including dual-mapping where anonymous RWX is refused | partial | — | G002, G004 |
| S002 | `jit-common` provides a block cache with chaining, eviction, and invalidation on self-modifying code and bank/overlay switches | partial | S001 | G002 |
| S003 | `jit-common` provides a persistent translation cache that is key-bound, verifiable, and safe to discard | missing | S002 | G002, G004 |
| S004 | `jit-common` provides a reusable override table with gameplay scoping, an A/B disable, and the evidence gate | missing | — | G001, G002 |
| S005 | `jit-common` provides harness helpers: register-file differ, trace ring, deterministic-replay scaffolding, first-divergence reporting | missing | — | G002, G003 |
| S006 | `render-common` provides a neutral render IR and host backends shared by every framework | missing | — | G002 |
| S007 | `android-runtime` owns the title-neutral APK runtime, extracted out of Lucent | missing | — | G002, G004 |
| S008 | Lucent is reduced to logging, configuration, HTTP control, and ZIP, with no Android or SDL surface | missing | S007 | G002 |
| S010 | `psxport` has a reference MIPS interpreter available as a divergence-diagnosis engine | partial | — | G003 |
| S011 | `psxport` runs a shipping title with no statically generated code, inside the frame budget | partial | S001, S002 | G001 |
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
| S041 | `x86port` translates x86-32 to x86-64 hosts, including lazy-EFLAGS evaluation | partial | S001, S002, S040 | G001 |
| S042 | `x86port` translates x86-32 to ARM64 hosts | missing | S041 | G004 |
| S043 | `x86port` has a reference x86-32 interpreter as the authority on the semantics its translator must match | partial | S005, S040 | G003 |
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
allocate blocks within branch-displacement range of one another.

**Landed 2026-09-01** as `src/jitcommon/code_memory.{h,cpp}` — the first code in
this repo. 68 checks.

The interface hands back TWO pointers, `write` and `exec`, equal under mprotect
and MAP_JIT and different under dual mapping. A single-pointer interface is
correct on a Linux desktop and wrong on a phone, so the split is structural
rather than documented. Mechanism is resolved by attempting it once at run time,
not from the target triple: whether anonymous memory may become executable is
kernel policy, so a compile-time answer is right on the build machine and wrong
on the shipping one. A host where nothing works refuses by name instead of
returning writable memory, which would defer a policy refusal into a jump to a
non-executable page. The icache flush is unconditional and inside `publish()`,
because on x86-64 it is a no-op and therefore the easiest thing in the subsystem
to omit and never notice until ARM64.

Verified by writing real host machine code into a region and calling it, then
patching and calling again — the second half is the icache test. The battery runs
through every mechanism the host can provide rather than the one it would pick,
so **Android's dual-mapping path is exercised on an ordinary Linux box** instead
of first running on a user's phone; the covered count is printed as its own
denominator (2 of 2 here).

Mutation testing found a real design defect: `publish()` inferred the mechanism
from `exec == write` rather than knowing it, so making the pointers equal by
accident made it mprotect a shared mapping — plausible on Linux, refused on
Android. The region now records its mechanism. A second survivor exposed a
missing leak check, which had to run per mechanism because the leak only exists
under the one with two mappings.

Gap: MAP_JIT (Apple Silicon) and VirtualProtect (Windows) are written but have
run on no host — stated as unverified, not assumed. Allocation within
branch-displacement range is not implemented; it belongs with the block cache
(S002), which is what will need it.

### S002 — Block cache

Required capability: a guest-address → translated-block container with block
chaining, eviction, and invalidation driven by the framework on self-modifying
code, bank switches, overlay loads, and DMA into code memory. Design the negative
first: a cache that silently misses must be distinguishable from one that works.

**Landed 2026-09-01** as `src/jitcommon/block_cache.{h,cpp}`. 668 checks.

This is what replaces static recompilation's dispatch table, and the framing
matters: runtime translation deletes the code-discovery problem by construction,
but moves the LOOKUP onto the hot path. That same lookup, done linearly, was
xmen2's worst measured hotspot (C210: 4,592 ms of level load → 500 ms as a
binary search), and a JIT asks it far more often — so the layout is chosen to be
**emittable**, not merely fast in C. Open addressing over a power-of-two table
makes the inline sequence multiply/shift/scale/load/compare;
`jc_block_table_layout()` publishes the offsets from the real struct so an
emitter cannot hold a stale copy, and a test reproduces the emitted address
computation and requires agreement. Deletion shifts backward rather than using
tombstones, because the emitted fast path examines one slot and every step of
displacement silently converts an emitted hit into a slow-path call.
Invalidation is by overlap, not containment.

Mutation testing found two real defects. Both probe loops were unbounded,
terminating only because the insert limit guarantees a free slot — removing that
limit did not fail the suite, it HUNG, which is the worst failure mode available.
And the displacement logic had **zero coverage**: Fibonacci hashing sends
sequential guest addresses to distinct slots, so no probe chain formed across
153,600 randomised lookups. Chains are now constructed deliberately, including
the arrangement that discriminates a wrong displacement rule from a right one and
the same case across the table wrap. A third defect was in the harness itself —
two mutations reported as surviving had failed to compile, so a stale binary ran.

Gap: **block chaining is not implemented.** Patching a block's exit to jump
straight to its successor needs the emitter's patch sites, so it belongs with the
first backend rather than being half-built here; the cache-side half of it — that
invalidating a block must unchain everything pointing at it — is designed for but
unwritten. Eviction is whole-cache flush only: a full table refuses inserts
rather than evicting a block a chained branch may still reference.

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
  call site. (Both replaced by `Core::engine` in I001; see below.)
- `runtime/recomp/sbs.cpp` already runs two Cores in lockstep with per-frame
  RAM+scratchpad diff, pause-at-first-divergence, and debug-server inspection.
- `PSXPORT_SELFTEST=oracle` (`selftest.cpp`) boots the interpreter Core with a
  software rasteriser and asserts it reaches GAME and plays the intro cutscene.
  Its own documentation records that the interpreter runs **scenes the recomp
  substrate cannot**: the un-recompiled overlay code the recompiler misses, and
  the mis-emitted in-function `jr` that froze the recomp GAME loop (later-272).

That last point is the strongest in-tree argument for this migration: on this
project the interpreter is already more correct than the static substrate.

Ground truth on which substrate ships: `Core::engine` defaults to
`Engine::Substrate` (`core.h:151`), so the native port Core runs `rec_dispatch`
→ generated C. Every
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

### S011 — `psxport` runs with no statically generated code

Required capability: a shipping PSX title executes guest MIPS with nothing
generated at build time, holding the 60 Hz frame budget. **Not** "has a JIT" —
USER 2026-09-01: *"I said JIT but what I meant is no static code generation and
something performant so it doesn't have to be JIT"*. The engine is whichever
rung of the ladder in `migration.md` §5.1 the measurement justifies.

Landed 2026-09-01:

- `Core::engine` is a total, refusing selector (I001), so an engine that is
  selected but never runs cannot look like a working run.
- `PSXPORT_ENGINE=substrate|interpreter|jit` selects it for the real game, not
  only inside the SBS/selftest harnesses, which is what made the measurement
  below possible at all. It aborts by name on an unrecognised value; verified
  against the built game (`PSXPORT_ENGINE=lightrec` → "names none of the 3
  engines this build has").

**MEASURED 2026-09-01 — the existing interpreter already clears the frame
budget on this title and host.** `PSXPORT_SELFTEST=startgame` on Tomba2Engine
(boot → mash Start → GAME stage → 600 field frames), headless, `NOPACE`,
`NOAUDIO`, one x86-64 desktop, both engines PASSING the same assertion:

| engine | whole run (wall) | steady-state field, 21 windows of 30 frames |
|---|---|---|
| `substrate` (statically recompiled C) | 5.32 s | median **2.27** ms/frame, p90 3.20, max 3.67 |
| `interpreter` (`interp.cpp`) | 12.48 s | median **6.10** ms/frame, p90 7.77, max 10.47 |

The interpreter is 2.35× the substrate's cost and spends **37% of a 16.67 ms
frame**, worst window 63%. So on desktop PSX, dropping static recompilation
does not require a JIT at all — the engine that is already in the tree, and is
already *more* correct than the substrate (S010), is fast enough.

**The two runs were not doing the same work, and the difference favours the
interpreter.** `overrides::install()`'s `oracleAllowed` defaults to `false` and
Tomba2Engine opts **zero of its 334 override installs** in, so on the
Interpreter engine none of the native overrides run: that column is pure guest
emulation with no native help at all, while the substrate column is recompiled
C *plus* 334 natives. 6.10 ms/frame is therefore an upper bound on the emulated
runtime's cost, not a like-for-like engine comparison — the target state
(runtime execution + overrides) has the natives back and lands below it.
Establishing that as a measurement rather than an inference needs I003 first,
because 816 of the overrides' own guest calls are still direct calls into
recompiled C.

Gaps: that reframes the remaining work. What is still open is not "write a JIT",
it is:

1. Whether that headroom survives the parts this measurement excludes: a real
   present path, audio, and the heaviest scenes rather than a field walk.
2. Whether it survives an ARM64 handheld/Android host, where the same work
   costs several times more. This is the only place a JIT (lightrec — standalone,
   host-supplied memory callbacks, x86-64 *and* ARM64 backends) is currently
   justified, and it stays a measured decision, not a default.
3. S013 — removing the translator and the generated corpus, which is what
   actually delivers G001.

Beetle remains vendored as a hardware backend and interpreter oracle only.

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
their tests) and the generated shard trees are deleted, the regeneration build
step is gone, and every doc reference is updated. Until then Tomba! 2's
`run.sh` still runs the recompiler (`tools/ensure_recomp.py` →
`generated/shard_*.c`), which is the user-visible fact this item owns.

**MEASURED 2026-09-01 — what actually blocks it.** `GEN_REC_SRCS` was emptied
and the `RecompRegistry` nulled, then `tomba2_port` was built in a separate
tree. Everything COMPILED; only the link failed, with **816 distinct undefined
symbols across 109 files** — all of them `game/` code naming a generated body
directly (`gen_func_<addr>`, `func_<addr>`, the overlay equivalents) plus 4
`*_set_override` calls.

So the framework↔substrate seam is genuinely clean and is not the problem: the
coupling is entirely game→generated, which `recomp_iface.h` deliberately allows.
The work is mechanical rewriting of those call sites to guest addresses, with
one real design gap first — there is no engine-neutral way to super-call the
original body past your own override. Inventory, the design question, and the
ordered work: **I003**.

Gap: I003 (816 symbol call sites and the missing engine-neutral super-call).
S014 conformance through the new engine is the other precondition; the static
path stays until then.

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

Entry conditions measured against the running `pc/xmen2` port 2026-09-01 — see
**I004** for the numbers. Three of them are already met and are what makes this
the tractable side of the migration: the PE images' real `.text` bytes are
already mapped into guest memory, `x86_native_call_at()` is a single dispatch
owner whose "no body here" miss is already explicit and fail-loud, and native
overrides are keyed by mapped address rather than installed into a generated
table (the defect psxport must fix first, I003).

Gap: guest memory is a single process-wide arena, so two engines cannot run in
one process against independent memory even though `CPU` registers are
instance-owned; and 85 hand-written call sites still name generated bodies by
C symbol.

**The framework now executes inside `pc/xmen2` (2026-09-02, 39b1990).** The
port consumes `x86port` and `jit-common` as pinned shared repos, selects an
engine with `X2_ENGINE`, and routes `x86_dispatch_one`'s "no body here" to the
interpreter. It is verified by a selftest that runs a real guest program
through the real entry point before the game starts, because the seam is a
MISS and a 60-frame run reaches none of them — see I004 for what the wiring
found, and what a measurement still needs.

**And it now runs real game code (2026-09-02, `pc/xmen2` 4bcfafb).**
`X2_ENGINE_TAKE` makes the substrate DECLINE named entry points, so a body the
corpus translated perfectly well is executed by the engine instead and the two
can be compared. Making it decline immediately found two defects the miss path
had carried since it landed and that no selftest could reach: the engine pushed
a return address its caller had already pushed, and its call-out dropped a
recompiled body's tail jump. Four hot bodies taken now run 60 frames clean.
Bisecting `all` then found the **segment bases were never bridged** — FS is
per-thread, so it sat outside both CPU structs, and every /GS prologue's
`mov eax, fs:[0]` read guest address 0 and faulted as a null dereference. With
that fixed, **3243 taken bodies run 60 frames clean: 794 calls, 6091 guest
instructions, 289 handed back.**

**The frame budget is measured (2026-09-02, `pc/xmen2` 3b45f5e).** Taking the
whole exe needed the engine to own the guest's `setjmp`: its run loop is a live
host frame, so it takes the host setjmp itself rather than letting the import
stub record a `jmp_buf` with nothing behind it. With that, **161,742,175 guest
instructions over 50,430 engine calls** run to 600 frames — 69 setjmps taken,
23 longjmps resumed, deepest nesting 4. Steady state over the 300→600 window,
offscreen and unbounded: **substrate 0.77 ms/frame, engine 10.9 ms/frame,
~14× slower and inside the 16.67 ms budget.** That is the x86 analogue of
S011's 6.10 ms on psxport, and it carries the same conclusion: on the desktop
the interpreter alone is enough to drop static code generation.

Its limits are part of the measurement: offscreen driver, the attract loop
rather than gameplay, and only `XMen2.exe` taken with the Alchemy DLLs still on
the substrate. And one defect is open — with the whole module taken the run
prints every report and then dies in the host allocator at teardown
(`sysmalloc: assertion failed`). It is the setjmp path (either half taken alone
is clean and takes no setjmp), and I004 step 3 carries what is known.

A prerequisite found while attempting the `pc/xmen2` wiring, and **resolved the
same day**: the port provisions shared checkouts from a pinned URL + revision in
`bootstrap.py`'s `SHARED_REPOS`, so a repo with no remote could not be consumed
without breaking the fresh-clone launcher contract everywhere but this machine.
`shared/x86port` and `shared/jit-common` are now published alongside their
siblings (`SomeoneIsWorking/x86port`, `SomeoneIsWorking/jit-common`, public).
The wiring needs a `SharedRepo` entry pinned to an exact revision, never a
branch.

### S041 — `x86port` x86-64 emission

Required capability: x86-32 guest code executes fast enough on an x86-64 host.

**Verified as of 2026-09-02: a first backend translates and executes, and its
output matches the interpreter on every program tested.**

- `src/x86port/emit_x64.{h,c}` — the host encoder. Verified by DECODING its own
  output with Zydis rather than against hand-written byte strings: 6,513 checks
  over an exhaustive 16-register sweep with displacements at the encoding's
  decision boundaries, and all sixteen CMOVcc conditions decoded individually
  (the condition number is added to a base opcode, so an off-by-one emits a
  different, perfectly valid conditional move). Six mutations
  covering the RSP/R12 SIB trap, the RBP/R13 disp8 trap, REX.R/REX.B swapping,
  the ALU opcode arithmetic, byte-store width and the disp8 boundary were all
  killed.
- `src/x86port/jit_x64.{h,c}` — guest block to host code. Data movement is
  inlined; arithmetic CALLS `x86p_alu`, the same function the interpreter calls,
  so the two engines are identical by construction rather than by agreement.
  That is what keeps the differential from being circular: an inlined
  reimplementation of the lazy-flag derivation would be a second authority on
  S043's semantics.
- Relative branches (`JMP`/`Jcc`, rel8 and rel32) end a block, which is what
  makes it a basic block. Emitted WITHOUT a forward jump: `x86p_cond` — again
  the interpreter's own evaluator — then both candidate addresses materialised
  and `CMOVcc` selecting between them. No fixup list, so no fixup list to
  forget to apply. Indirect branches stay excluded on purpose: their target is
  not known until the block runs, so they need the block cache rather than a
  constant folded in at translation time.
- `tests/test_jit_x64.c` — the differential. 1,500 generated programs, 17,666
  guest instructions translated and executed, whole-machine comparison against
  the interpreter (eight registers, EIP, the raw lazy tuple including
  `carry_in`, and all six derived flags), plus an independent re-walk verifying
  `guest_len`. The suite reports how many blocks ended in a branch (1,320 of
  1,500) and REFUSES if that is zero, so a generator that stopped producing
  branches cannot keep claiming that coverage.
- Fifteen mutations killed across both files, including CMP and TEST storing a
  result, swapped ALU operands, a caller-saved CPU register, a misaligned stack
  at the helper call, an unscaled register offset, an inverted CMOV condition,
  a branch target taken relative to the wrong instruction, and a JMP going to
  its fall-through.

  One mutation SURVIVED before being fixed and is worth recording: setting
  `guest_len` from the branch target instead of the fall-through passed every
  state comparison, because nothing read that field. It is the field range
  invalidation uses to decide whether a write to guest memory stales a block,
  so the cache would have been unable to invalidate correctly while every test
  stayed green. The independent span re-walk now covers it.

- `src/x86port/jit_engine.{h,c}` — the dispatch loop, which is what makes the
  translator an ENGINE: look up, translate on a miss, publish, enter, and step
  the interpreter for anything the backend refuses. It is the first caller
  `jit-common`'s code region and block cache have had. The interpreter is the
  FALLBACK, not a second engine, so coverage is a performance property rather
  than a correctness one — which is what lets this proceed one emitter at a
  time. `tests/test_jit_engine.c` runs whole programs against the interpreter:
  loops, cache hits on re-entry, the fallback, arena exhaustion and flush,
  invalidation of self-modifying code, a guest memory fault, and the same run
  forced through DUAL MAPPING — the mechanism Android selects and a Linux box
  never does. That test says loudly that it tested nothing if the host cannot
  provide dual mapping; it does not pass.

  Eight of nine engine mutations killed. The survivor — publishing fewer bytes
  than were written — is an ARM64 instruction-cache defect and genuinely
  equivalent code on x86-64; it is recorded as untested rather than counted.
  Three of the killed ones were real defects that every state comparison
  passed: the block cache holding the WRITE address (identical to the exec
  address on this host), a guest memory fault reported as budget exhaustion
  (nothing compared the stopping REASON), and deleting the fast mid-block
  fallback (correct, and quietly quadratic in any hot loop).

Gap: the translatable set is 32-bit MOV and ALU with a MEMORY OPERAND on either
side, PUSH/POP, LEA, direct CALL and RET, NOP, and PC-relative JMP/Jcc. Not yet
emitted, each ending the block by name: 8- and 16-bit widths with their
partial-write rules; shifts and rotates; INC/DEC; indirect branches and indirect
calls; x87; PUSH imm8, whose sign-extension rule lives in the interpreter and is
refused rather than reimplemented here. Blocks do not chain, and there is no
persistent translation cache across runs.

**COVERAGE ON REAL GAME CODE: 10.27%** (`tools/jit_coverage.c`, 2026-09-02).
Over X-Men Legends II's full Ghidra export — 58,248 functions, 2,168,666
instructions — 49,675 functions (85.3%) yield a block and 222,731 instructions
translate, at a mean block length of 4.48. The differential runs on every one of
those blocks against the interpreter: 0 divergences.

The ranked stopper list is the work queue: indirect CALL 6,997, PUSH 5,351
(the imm8 form), MOV 4,030 (widths below 32 bits), FLD 1,924, indirect JMP
1,832, DEC 460, TEST 439. The head has changed shape: what remains is
dominated by indirect control flow, which needs the block cache to resolve a
target at run time rather than another emitter, and by x87.

Speed: `jit_bench` reports 0.97 ns/insn, 1.44x a like-for-like
static-recompilation baseline and 145x the interpreter. That kernel is
register-only and now known to be unrepresentative of real code, so 1.44x must
not be quoted as a whole-game figure. Coverage is STATIC and unweighted by
execution frequency; a profile-weighted number needs the game running.

**The measurement caveat still stands** (`migration.md` §5.1): S011 measured the
PSX interpreter comfortably inside frame budget, and no equivalent guest/host
split has been measured for `pc/lf2` or `pc/xmen2`. This backend does not assert
that a JIT is needed for either title; it establishes the path and the
verification shape. Escalation past the current set should follow a measurement,
not this row's existence.

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

**This is the first x86 work to do, ahead of S041**, and I004 records why. It is
not only the semantics authority: it is the only way to obtain a measurement at
all, because unlike `psxport` — where the interpreter was already in the tree and
the frame-budget question was answerable in an afternoon (S011) — there is no
second engine here to measure.

It also has a measured initial coverage target rather than "the ISA". The static
translator left **8,234 `x86_unsupported_insn()` holes** in the emitted corpus,
each a loud abort if reached — and **7,410 of them (90.0%) are 3DNow!**, a
twenty-opcode family from one 3DNow!-compiled math library linked into three
modules. Twenty opcodes in a decoder close nine tenths of the corpus's holes.

The 10% tail argues the same way from the other side: it is led by x87 80-bit
spills and then by `INT`, `STD`, `LODSD`, `INSB`, `ARPL`, `BOUND`, `DAA`, `IN`
and bare segment registers — opcodes a 2005 Windows game does not execute,
which the translator itself annotates as "embedded data decoded as code". Static
analysis must guess what is code; an interpreter decodes only what execution
reaches, so that class of hole stops existing rather than being fixed.

Started 2026-09-01: `shared/x86port` exists and owns 3DNow! —
`src/x86port/three_dnow.{h,c}`, 19 opcodes implemented and the 5 approximation
instructions (PFRCP, PFRSQRT, PFRCPIT1, PFRCPIT2, PFRSQIT1) REFUSED by name
rather than approximated, since `1.0f/x` for PFRCP agrees to six digits and
disagrees in the low mantissa bits. 427 checks pass; negative-tested by making
PFRCP return `1.0f/x`, which fails three assertions across two tests.

**The decoder question is settled, and by measurement.** `shared/recomp-x86`
could not seed it — corrected by reading it: its front end is Ghidra and it
lifts disassembled mnemonic TEXT, which is why its failures read `mnemonic
PFMUL`. It has semantics to seed from and no decode at all, and Ghidra can never
be a player prerequisite. x86port therefore embeds **Zydis v4.1.1** for decode
only and keeps semantics its own — decode is mechanical and brings no memory
model, threading, or cache assumptions to fight `jit-common`, which is exactly
what disqualifies embedding a whole core.

Validated over **2,168,629 real instructions** from `pc/xmen2`'s 20-module
Ghidra export, whose records carry the raw bytes beside Ghidra's own reading:
`0` failed to decode, `2,168,592` length agreements (99.9983%), and all 37
disagreements the same 0x9B FWAIT-folding convention, where both readings are
correct and the literal one is what an interpreter wants. The instrument is
`tools/{corpus_extract.py, decode_diff.c}`; it refuses an empty corpus rather
than reporting agreement, and does not special-case the FWAIT category.

**The engine selector landed 2026-09-01** — `src/x86port/engine.{h,c}`, 74
checks — so "which engine ran?" is answerable before there is a second engine to
answer it about. It follows psxport's `engine_select.h` (I001) for the enum and
the total, refusing route, and shares its vocabulary deliberately: the
frameworks are separate, but a report from one has to be readable against the
other. It needed more than a copy, because psxport compiles all three arms into
every build while here the substrate is generated C living in the *title* and
the interpreter is unwritten — so selection takes an availability mask from the
consumer and refuses a correctly-spelled-but-unlinked engine as loudly as a
misspelled one, with a different reason for each. Mutation-tested: making the
route fall through to the substrate fails 5 checks, and making resolution
quietly fall back on an unlinked engine fails 9.

**The flag model landed 2026-09-01** — `src/x86port/flags.{h,c}` — and it is
the piece of this row where being wrong is quietest, so it got the strongest
verification: the test executes each instruction on the host CPU with a
controlled incoming EFLAGS and compares the real word. **2,187,776 comparisons,
0 mismatches**, all six flags, exhaustive at byte width (256×256 operand pairs ×
four incoming CF/AF combinations, for ADD SUB CMP AND OR XOR ADC SBB INC DEC SHL
SHR SAR) plus deterministic boundary-crossed sweeps at 16 and 32 bits. On a
non-x86 host it reports that no oracle ran rather than passing on the hermetic
cases.

The oracle found four defects that reading the manual had not: logic ops CLEAR
AF and shifts SET it (the model preserved both); SHR's OF is `msb(a)` only at a
count of 1 and 0 beyond; and a shift by zero writes no flags at all, which is
the *instruction's* rule, so `x86p_flags_set` refuses to record one rather than
growing a preserving branch for four flags with no correct value. It also
exposed a hole in its own first version, which varied only the incoming CF and
so reported "0 differ" on the logic ops' AF with no power to see otherwise —
which is why the sweep now compares the ISA-undefined flags too.

The model is seeded from `pc/xmen2`'s substrate (`x86rt.h` FK_*/FLAG_*) and
keeps its lazy shape deliberately, so the two are comparable instruction by
instruction. It departs from it in three stated places: AF exists at all, ADC
and SBB compute eagerly (a carry-in is not expressible in the lazy triple, and
modelling it wrongly once cost xmen2 issue #16), and shifts are split by
direction rather than sharing one CF formula that works only because a caller
pre-arranges its input.

**The integer ALU landed 2026-09-01** — `src/x86port/alu.{h,c}` — covering 24
operations that are **400,614 of the corpus's 2,168,629 instructions (18.47%)**,
a scope ranked from the real export rather than chosen by intuition. Result and
flags are one call, which is a deliberate departure: the substrate computes the
result at the call site and records the flags with a separate `SETFLAGS` macro,
two facts about one instruction kept in two places at 400,614 sites with nothing
checking that the flag kind matches the arithmetic done.

**2,342,080 hardware comparisons of result AND flags, 0 mismatches.** This
closes the gap `test_flags` necessarily left: that suite fed hardware's own
result back into the model, so it proved the derivation given a result and not
the results. Two more defects fell out — SAR's carry-out past the operand width
(SHL and SHR run out of operand bits, SAR never does; wrong on 11,776 of 34,816
cases) and MUL/IMUL, which write CF and OF and nothing else, this CPU preserving
all four undefined flags across 65,536 of 65,536 cases in both directions where
the model had derived them (wrong on 49,280). The divides report `#DE` by return
value, including quotient overflow, and the sweep runs one on hardware only
where the model claims success — which is itself the test of that predicate.

**The interpreter executes, as of 2026-09-01.** `src/x86port/{cond,cpu,exec}.{h,c}`
plus an operand model in `decode.h` complete the loop: decode → operands →
ALU → flags → conditions → next instruction. Measured against the shipped
corpus by the same instrument that validated decode, now reporting semantic
coverage beside decode agreement: **2,036,997 of 2,168,629 instructions (93.93%)
have semantics in this build**, and it prints the unmodelled remainder ranked by
mnemonic, so the work list stays current rather than being re-derived.

The three supporting pieces, each verified in the way its failure mode demands:

- **Conditions** — all 16, against hardware `SETcc`, over the entire 64-state
  flag space. 1024/1024 exact. Enumerated rather than sampled because JB and JL
  read identically in English and test different flags, and choosing wrong is
  right for small positive values and wrong across the sign boundary.
- **Machine state** — registers as an array indexed the way the *encoding*
  indexes them, since a translator resolves a register at build time and an
  interpreter resolves it from three ModRM bits at run time. The two traps are
  byte registers 4–7 being the high bytes of EAX–EBX rather than ESP–EDI, and
  partial writes preserving the rest; both mutation-tested. Memory is a struct,
  not a global — the first move toward the independent-memory requirement below.
- **The step loop** — one instruction, with all five outcomes named (decode
  failure, unsupported, fetch fault, memory fault, `#DE`), EIP left pointing at
  the failing instruction, and the mnemonic carried even on a refusal.

Verified end to end on real machine code assembled with `clang -m32`: a sum
loop, a call with a stack frame (checking ESP balance, not just the return
value), memory access with MOVZX/MOVSX/LEA at three widths, and
DIV/SETcc/CMOVcc. Each program asserts what its bytes decode to before running
them.

**x87 landed 2026-09-01**, in `src/x86port/x87.{h,c}` (the FPU) and
`src/x86port/x87_exec.{h,c}` (the instructions), taking semantic coverage from
93.93% to **98.18% — 2,129,063 of 2,168,629 instructions**, measured by the
same instrument.

The stack is modelled as a stack: `ST(i)` is a POSITION under a rotating TOP,
not register `i`, which is right for exactly as long as TOP is zero and then
reads from the wrong place with no crash and no obviously wrong number. TOP is
observable through FNSTSW bits 11–13, so it is explicit rather than normalised
away by shuffling an array. The pop suffix is carried as a COUNT, because
FCOMPP pops twice.

**Rounding is the finding worth keeping.** x87 rounds ONCE, to the precision
the control word selects; computing in extended and rounding afterwards is two
roundings, and they disagree — measured against this host's FPU over 2,359,296
operations, **27,930 (1.18%) differed**, none of them at PC=extended where
there is no second rounding to do. So on an x86 host `x86p_x87_arith` loads the
guest's control word into the real FPU and executes the real instruction. Two
plainer defects fell out of the same sweep: precision control was narrowing via
a `(float)` cast, which clamps the EXPONENT as well as the significand (results
near 1e300 came back as infinity), and it ignored rounding control entirely.

That makes the arithmetic sweep tautological on x86, which the suite states
rather than reporting as parity: what it still checks is the routing — operand
order, the reverse flag, which ST(i), whether the control word arrives — and
one deliberate defect demonstrates it can fail. `x86p_x87_arith_portable`
exposes the two-step path so the fidelity gap on a host with no x87 unit stays
MEASURED (0.4% at PC=double/nearest, 3.8% at PC=double/up) rather than assumed
small; that is the number the ARM64 host of S042 inherits, and closing it means
an 80-bit softfloat.

Two process notes, because both changed what got written:

- **The first sweep had no power.** 196 cases per operation found nothing; the
  widened one — 256 values including denormals, infinities and generated
  full-mantissa values, crossed with six control words — found every defect
  above. This is the same failure the flag oracle had, in a new place.
- **The execution tests passed first try and were wrong to be trusted.**
  Mutation testing found three uncovered claims the suite had comments about
  but no cases for: FXCH hardcoded to ST(1) survived (every FXCH tested named
  ST(1), where the index is indistinguishable), the FI forms reading their
  operand as a float survived (no program used FIADD), and FLD ST(i) reading
  after the push instead of before survived (every FLD took a memory operand).
  All three now fail when reintroduced.

Gap: MMX and SSE — MOVQ alone is 6,936 of the remaining 39,566 — the REP string
forms, the x87 transcendentals (FSIN, FCOS and friends, refused by name rather
than approximated), and the five refused 3DNow! approximations once their
AMD-specified forms are in hand. Decode, engine selection, the flag model, the
integer ALU, the conditions, the machine state, the execution loop and x87 are
no longer among them.

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
for static recompilation and must be rewritten in the same change — in the change
that lands the frame-budget measurement, not before it.

What is being replaced, measured 2026-09-01: **116,500** translated functions
across 89 module translation units, **307 MB** of generated C, gitignored and
regenerated at build time. Ordered work and the four entry conditions: I004.

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
