# Static recompilation → JIT: architecture and migration

Status: **design, not started.** Author it, get sign-off, then execute.

USER 2026-09-01: "I want to convert all of them from recomp with overrides to JIT
with overrides. Nothing should be recomped."

USER 2026-09-01: "interpreter can be used to check if JIT is wrong but … we know
for example Xenia runs gears1 well and Dolphin's JIT also works well so does
beetle."

USER 2026-09-01: "Licensing is not important, you can use any license … change
all recomp wording … there are two runtimes there, one native (WIP), one
emulator, the game itself is still guest game so the wording 'recomp' just means
the emulated runtime."

USER 2026-09-01: "we can make a separate framework per platform such as Aurora+
JIT for GameCube+Wii for example, like psxport is its own framework, I think I'd
prefer that. I thought about shared framework only for common JIT helpers and
such and common caching mechanism"

USER 2026-09-01: "Android doesn't have to be in Lucent also, it can be separated,
whichever makes the best design. prioritize only design quality, ignore how much
effort it would take"

USER 2026-09-01: "graphical backends, xmen2 is d3d8 and lf2 is ddraw for example
so each framework may need multiple backend support"

---

## 1. Goal and framing

USER 2026-09-01: "I said JIT but what I meant is no static code generation and
something performant so it doesn't have to be JIT."

**The requirement is two things: no static code generation, and adequate
performance.** A JIT is one way to satisfy both, not the goal. The engine each
framework picks is an implementation choice measured against that title's frame
budget (§5.1).

Every port executes its guest binary by **interpreting or translating it at
runtime**. **No guest code is statically recompiled to C.**

Each port has **two runtimes for the guest title**:

- **native overrides** — hand-written host code, a growing set (WIP), the
  long-term direction; and
- **the emulated runtime** — the JIT, which executes every guest function not yet
  overridden.

The game is always the guest game. "recomp" in the old docs meant *the emulated
runtime*, which was static recompilation to C. Wherever old docs say "recomp /
recompiled / generated body", read "the emulated (JIT) runtime".

Priority targets: `pc/xmen2`, `pc/lf2`, `psx` (all titles), `x360/gears1`,
`sunbright`. Second wave: `kirbh` (GBA), `benefactor` (Amiga), `mimp` (NES).

### Why move off static recompilation

- **Iteration cost.** Static recomp regenerates and recompiles tens to hundreds
  of MB of C on every translator change (gears1: ~49k functions / ~176 MB C++).
  A JIT translates only what runs, only once per run.
- **Build-gate friction.** "Instruction not implemented → translator exits
  non-zero and seals the module" turns every long-tail opcode into a build stop.
  A JIT falls back to its interpreter and keeps running.
- **Proven cores exist.** Xenia runs Gears 1. Dolphin's JIT runs Sunshine.
  lightrec/Beetle run the PSX catalogue. Years of per-title correctness work the
  static translators are re-deriving by hand.
- **Overrides get simpler.** "Keep the recomp body alive so it stays diffable"
  becomes "the JIT can always run the guest body" — with zero regeneration.

### What does NOT change

Overrides and their evidence gate; each project's platform HLE, renderer, audio,
input and saves; faithful-first-then-enhance phasing; the differential-harness
discipline; ROM/disc provisioning; single-title conformance discipline.

---

## 2. Layered architecture

The user's preference: **one framework per platform**, like `psxport` today —
each owning its CPU, its guest APIs, its HLE, its renderer, and its harness. The
shared layer is deliberately thin: JIT helpers and the translation cache.

```
┌──────────────────────────────────────────────────────────────────────┐
│ L4  titles                                                           │
│   psx/spyro · psx/tekken3 · … │ sunbright │ gears1 · mua │ lf2 · xmen2│
└───────┬───────────────────────┴─────┬─────┴──────┬───────┴─────┬─────┘
        │                             │            │             │
┌───────┴─────────┬───────────────────┴──┬─────────┴─────┬───────┴─────┐
│ L3  platform frameworks (§4)                                          │
│  psxport        │  gcnport             │  xenonport    │  x86port     │
│  PSX            │  GameCube + Wii      │  Xbox 360     │  Win32 / OG  │
│  MIPS R3000     │  PPC Gekko/Broadway  │  PPC Xenon    │  x86-32      │
│  PSX GPU/SPU/   │  Aurora GX · DVD/DI  │  Xenos PM4 ·  │  DDraw · D3D8│
│  GTE/MDEC/CD    │  · OS/DOL HLE        │  kernel/XAM   │  · D3D9 · GDI│
│  · BIOS HLE     │                      │  (xenon-host) │  · Win32 HLE │
└───────┬─────────┴──────────┬───────────┴──────┬────────┴──────┬──────┘
        │                    │                  │               │
┌───────┴────────────────────┴──────────────────┴───────────────┴──────┐
│ L2  shared infrastructure                                            │
│  jit-common (§3)          render-common (§6)      android-runtime    │
│  exec memory · block      neutral RHI · Vulkan/   (§10)              │
│  cache · persistent       SDL_GPU backends ·      Activity · SAF ·   │
│  translation cache ·      frame graph             touch · insets     │
│  override table ·                                                    │
│  harness helpers          android-port  (build/package/device)       │
└───────┬──────────────────────────────────────────────────────────────┘
        │
┌───────┴──────────────────────────────────────────────────────────────┐
│ L1  lucent — logging · configuration · loopback HTTP control · zip    │
│      dependency-free, host-agnostic                                  │
└──────────────────────────────────────────────────────────────────────┘
```

### Instance-owned state is what makes in-process comparison possible

USER 2026-09-01: "C++ OOP is not extremely essential but it helps being able to do
in-process comparison such as if it's truly OOP, we can run an oracle core and a
native core in the same runtime and compare the states directly."

This is the architectural reason the whole verification story works, and it is a
hard constraint rather than a style preference:

- **All guest machine state belongs to an instance, never to a global or a file
  static.** Registers, memory, COP0/exception state, the GTE, timers, scheduler
  state, and any diagnostic tracking must hang off the core object. `psxport`
  already earned this — `Core` is a class, and its comments record the
  de-globalization work and that "exception state must never be shared between
  two Cores".
- **The payoff:** two engines instantiate in one process, run the same input, and
  their states are compared directly — no second process, no screenshots, no
  serialisation boundary. That is what `sbs.cpp` does, and it is why a divergence
  can be pinned to the first frame it appears in.
- **One file static silently breaks it.** Two cores interleaving through a
  `static` scratch array corrupt each other's tracking, and the corruption looks
  like a divergence in the thing being measured. Any `static`/global holding
  guest-derived state is a defect under this rule, including in diagnostics.

**Therefore it is a core-selection criterion (§5):** a candidate CPU core must
support two live instances in one process. lightrec is instance-based
(`lightrec_state *`). Xenia is object-oriented (`Emulator`, `Processor`,
`Memory`). Dolphin is the risk — it is historically globals-heavy and its
migration to a `Core::System` instance is incomplete, so `gcnport` must verify
two-instance coexistence before committing, and treat fixing it in the fork as
part of the work rather than a surprise.

The same rule binds `jit-common`: its block cache, code-memory allocator, override
table, and harness helpers are all instance-owned. A process-global block cache
would make two engines share translations and quietly destroy the comparison.

Rules that keep the layering honest:

- **A framework never depends on another framework.** Shared behaviour moves down
  to L2, never sideways.
- **L2 libraries know nothing about a guest CPU or a guest API.** `jit-common`
  has no MIPS in it; `render-common` has no D3D8 in it.
- **A title never reaches past its framework** into L2 for platform behaviour. It
  may use `lucent` for logging/config directly, as today.
- **There is no mandated `CpuCore` interface across frameworks.** An earlier draft
  proposed one; it was the wrong shape. Each framework integrates its core the
  way that core wants to be integrated. Commonality that turns out to be real
  gets *extracted* into `jit-common` afterwards, not predicted up front.

---

## 3. `shared/jit-common` — what is actually shared

Only what is genuinely guest-CPU-independent. No translator, no decoder, no
per-arch semantics, no interface every framework must implement.

### 3.1 Executable code memory

The one piece every JIT needs and every JIT gets subtly wrong:

- W^X code buffers: reserve, commit, flip RW↔RX, release.
- **Dual-mapping** for platforms that refuse anonymous RWX — one RW view and one
  RX view of the same `memfd`/shared mapping. Required on Android (SELinux
  `execmem`), useful on hardened Linux, mandatory on any future iOS/console path.
- Instruction-cache coherence: `__builtin___clear_cache` / `sys_icache_invalidate`
  at the right granularity for ARM64 hosts.
- Large-page hints, guard pages, and a code-region allocator that keeps
  translated blocks within branch-displacement range of each other.

This is JIT-domain policy, so it lives here rather than in a platform library —
but it uses only plain OS primitives, so it does not drag Android SDK code into
`jit-common`.

### 3.2 Block cache

Guest address → translated block; block chaining/linking; eviction; and
**invalidation** on self-modifying code, bank switching, overlay loads, and DMA
into code memory. Frameworks call `invalidate(addr, len)`; the container handles
the bookkeeping.

### 3.3 Persistent translation cache

The "common caching mechanism" the user asked for. On-disk format, keying,
versioning, and load/store. See §9.

### 3.4 Override table

A reusable implementation of the override pattern: `(guest address → native
function)`, gameplay-scoped registration, the A/B disable toggle, and the
evidence gate (an override needs a proven divergence or a named reason — port
kirbh's prerequisite check). The **super-call** is framework-specific (it means
"run the guest function through this framework's core"), so the table takes a
framework-supplied callback rather than owning it. A framework may use this or
keep its own; it is a library, not a mandate.

### 3.5 Harness helpers

Register-file differ, trace ring buffer, deterministic-replay scaffolding,
first-divergence reporting. Frameworks supply the register layout.

---

## 4. Platform frameworks

Naming follows `psxport`. Names are provisional; `gcnport` is the user's
"Aurora + JIT" for GameCube/Wii.

### 4.1 `psxport` — PlayStation (exists)

Owns: MIPS R3000 JIT, GTE, GPU (native + faithful paths), SPU, MDEC, CD/disc,
BIOS/SDK HLE, the SDL_GPU renderer, the SBS differential harness.

Migration: `tools/recomp/` (`decode.py`, `emit.py`) and the generated
`generated/port/` trees are deleted. The existing `runtime/recomp/interp.cpp`
(1171 lines) is **promoted**, not deleted — it becomes the reference interpreter
the JIT is validated against (§8). One migration covers ~12 titles.

CPU core: **lightrec** (see §5) with Beetle's interpreter as a second oracle.

### 4.2 `gcnport` — GameCube + Wii (new)

Owns: PPC Gekko/Broadway JIT (via Dolphin), Aurora for GX, DVD/DI, the OS/DOL
HLE, and the Wii-specific additions (Broadway extensions, Wii memory map,
Bluetooth/WiiMote when a Wii title arrives).

`sunbright` is extracted into this framework: what is title-neutral (Gekko
execution, GX, DVD, OS HLE) moves to `gcnport`; what is Super Mario Sunshine
(addresses, actor behaviour, scene knowledge, the file-select work) stays in
`sunbright` as the title. This is the same framework/title split `psxport`
already has, applied to the GC/Wii side. Its `build-recomp`, `build-sms-recomp`,
`sms-recomp/`, and `run-recomp.sh` trees go.

Because Dolphin already handles paired singles, `psq_l/psq_st`, the
graphics-quantisation registers, and `dcbz`/`dcbz_l` cache-line behaviour, its
JIT is worth far more than a hand-rolled Gekko backend.

### 4.3 `xenonport` — Xbox 360 (new)

Owns: PPC Xenon + VMX128 JIT (via Xenia's CPU), the Xenos PM4 command-stream
frontend and Xenia's shader translator, the kernel/XAM HLE, XMA audio, and disc/
XEX identity. `shared/xenon-host` (the title-neutral guest-image contract, memory
window, and import-manifest validation) sits **inside** `xenonport` as its image/
binding layer rather than remaining a separate shared repo — it has exactly one
consumer family and splitting it buys nothing.

`gears1` and `mua` become titles over it. `extern/XenonRecomp` and
`config/gears.toml` are removed.

### 4.4 `x86port` — Win32 and original Xbox (new; replaces `shared/recomp-x86`)

Owns: x86-32 JIT, the Win32 HLE, and **several guest graphics API frontends** —
this is the framework where §6 matters most:

| Title | Guest graphics API |
|---|---|
| `pc/lf2` | DirectDraw (2D blit/surface) |
| `pc/xmen2` | Direct3D 8 |
| OG Xbox path (later) | Xbox D3D8-like + pushbuffer |

Each frontend is a title-independent implementation of a real API, so it belongs
in `x86port` as a selectable module, never in a title. A D3D8 frontend written
for xmen2 is most of what an OG Xbox port later needs.

`shared/recomp-x86` is retired; its x86-32 decode and per-instruction semantics
are the seed for `x86port`'s translator (§5.4).

### 4.5 Second wave

`gbaport` (mGBA JIT — `~/repo/mgba` is already here) for `kirbh`; `amigaport`
(M68K) for `benefactor`; `nesport` (6502) for `mimp`.

---

## 5. Execution engines

### 5.1 Pick the cheapest engine that meets the measured frame budget

The requirement is no static codegen plus adequate performance, so a JIT is only
warranted where something simpler misses the budget. In rough order of cost:

| Engine | Cost to build | Portability | When it is the right answer |
|---|---|---|---|
| **Switch interpreter** | already exists in most of these trees | perfect — plain C, no code memory | Low-demand titles; the correctness reference regardless |
| **Threaded interpreter** (computed-goto / direct threading) | small — a dispatch rewrite of an existing interpreter, no code generation at all | perfect: no W^X, no Android `execmem` problem, identical on x86-64 and ARM64, no code cache, no SMC invalidation | Often enough. Typically several times faster than a switch loop |
| **JIT** | large — host backends, code memory, cache invalidation | constrained: needs W^X and a backend per host arch | Only where measurement says the interpreter misses the budget |
| **Threaded interpreter + JIT for hot blocks** | largest | as JIT | A title that is mostly cheap with a few hot loops |

**Measure before choosing.** These are 1990s–2000s console titles running on
modern hardware; several will hit frame rate on a threaded interpreter, and
building a JIT for them would be work spent for nothing. The rule is: stand up the
interpreter, measure against the title's frame budget, and escalate only on
evidence.

Two consequences worth stating up front:

- `pc/lf2` is a 1999 2D fighting game. A threaded interpreter is very likely
  sufficient, which would remove the need for a bespoke x86-32 JIT with two host
  emitters (S041/S042) entirely. Measure before committing to that work.
- `shared/jit-common`'s code-memory, block-cache and translation-cache pieces are
  needed **only by frameworks that choose a JIT engine**. A threaded-interpreter
  framework uses none of them. The library is scoped correctly; it is just not
  universally required.

### 5.2 CPU cores and the host-architecture constraint

Effort is not the deciding factor; **portability of the host codegen is**, because
these ports ship on Android (ARM64) and Apple Silicon as well as x86-64. This
rules some otherwise-obvious cores out.

| Framework | Guest CPU | Core | host x86-64 | host ARM64 |
|---|---|---|---|---|
| `psxport` | MIPS R3000 | **lightrec** | ✅ | ✅ |
| | | Beetle's own dynarec | ✅ | ⚠️ weak |
| `gcnport` | PPC Gekko/Broadway | **Dolphin** (Jit64 + JitArm64) | ✅ | ✅ |
| `xenonport` | PPC Xenon + VMX128 | **Xenia** (x64 backend only) | ✅ | ❌ |
| `x86port` | x86-32 | **asmjit** translator (ours) | ✅ | ✅ (two emitters) |
| | | qemu-TCG / Unicorn (GPL-2.0) | ✅ | ✅ (awkward embed) |
| | | FEX-Emu / Box86 (MIT) — CPU only, still needs our HLE | n/a (host is the CPU) | ✅ |

Consequences, stated plainly:

- **`psxport` uses lightrec, not Beetle's dynarec.** lightrec is a standalone
  MIPS→host JIT designed to be embedded with host-supplied memory callbacks —
  exactly our shape — and it has x86-64 and ARM64 backends. Beetle's CPU is
  welded to its own bus/GPU/SPU, all of which `psxport` already reimplements
  natively. Beetle stays vendored as an **interpreter oracle**, not as the engine.
  PSX Android is a named goal, so this is a correctness-of-design call, not a
  convenience one.
- **`xenonport` is x86-64-only until Xenia's IR gets an ARM64 backend.** That is
  the honest state: Gears on Android/Apple Silicon is blocked on real work in the
  Xenia fork. Since effort is not a constraint, writing that ARM64 backend is the
  right long-term answer (and is upstreamable); until then, record the limitation
  rather than papering over it.
- **`x86port`'s core is an OPEN QUESTION, and this row was written on a false
  premise.** It claimed `shared/recomp-x86` already held the x86-32 decode and
  semantics, so writing our own translator was "retargeting emission rather than
  deriving semantics". **Measured false 2026-09-01**: recomp-x86 has no decoder
  at all — its front end is Ghidra, consuming disassembled mnemonic TEXT, which
  is why its failures read `mnemonic PFMUL`. The cheapest-looking reason for
  writing our own does not exist, and everything downstream of it (S043's
  interpreter as a required oracle) inherits that.

  The reasons that survive are real but narrower: native integration with
  `jit-common`'s caches, debuggability through our own harness, and a TCG embed
  bringing its own memory model, cache and threading assumptions. qemu-TCG and
  Unicorn are also GPL-2.0, which is a shipping problem this table never stated.

  **Two things this row got structurally wrong, both worth carrying:**

  - **On an x86-64 host an x86-32 guest needs NO core.** The host is the CPU.
    The table lists x86port's host targets as though a core were required for
    both, which silently doubled the scope. A core is needed for ARM64 only —
    which is where Android is, so it is still needed, but the question is
    "an ARM64 backend" and not "an x86 CPU".
  - **The CPU was never the hard part of this port.** Running a 2005 Windows
    game without Wine is a Win32 + D3D8 HLE problem (S044, S045), and that is
    the piece nothing off-the-shelf supplies embeddably. FEX-Emu and Box86 are
    NOT alternatives to it: they emulate the CPU for *Linux* x86 binaries and
    are run underneath Wine, so they leave the actual work untouched. They are
    candidates for the ARM64 CPU backend alone, under our own HLE.

  Lazy-EFLAGS evaluation is the one genuinely hard part of writing our own and
  is well-understood; `src/x86port/flags.c` now implements it, hardware-verified.

### Interpreters: required only where we wrote the translator

USER 2026-09-01: "I don't think PSX needs interpreter oracle because JIT is known
to work well."

That is the right call, and it generalises by who wrote the translator:

- **Embedding a proven core** (lightrec, Dolphin, Xenia): do **not** invest in
  building interpreter-oracle capability. The core's instruction semantics are
  already proven across a catalogue of titles; re-proving them is wasted effort.
  Where an interpreter already exists in-tree it stays as a free diagnostic — it
  costs nothing and it is occasionally the only engine that reaches a given scene
  — but it is not a prerequisite for the JIT and not a gate on anything.
- **Writing the translator ourselves** (`x86port`, §4.4): a reference interpreter
  is **required**, because nothing else states the semantics the translator must
  match. There it is the authority, not a diagnostic.

The real integration risk with a borrowed core is not its instruction semantics
but **our binding of it** — the memory map, MMIO, GTE, overlay routing,
coroutine/threading model, and override interposition are all new code. That is
covered by comparing the JIT against the engine the project already ships
(§8.2), not by standing up a new oracle.

---

## 6. Graphics: guest API frontends × host backends

The guest graphics API varies *within* a platform framework, and the host
renderer varies *across* deployments. These are two independent axes and the
design must keep them independent.

```
  guest API frontend            neutral render IR           host backend
  ─────────────────────         ─────────────────           ────────────
  DDraw ─┐                                                ┌─ Vulkan
  D3D8  ─┼─→ lower to ──→   render-common command    ──→  ┼─ SDL_GPU
  D3D9  ─┤                  stream (resources, state,     └─ (headless capture)
  GX    ─┤                  draws, passes, resolves)
  PSX GPU┤
  PM4   ─┘
```

- **Guest API frontends live in the framework that owns that platform.** DDraw
  and D3D8 are `x86port` modules; GX is `gcnport`; the PSX GPU is `psxport`;
  Xenos PM4 is `xenonport`. A title selects a frontend; it never implements one.
- **`shared/render-common` (L2) owns the neutral render IR and the host
  backends.** gears1 already proved this shape with `runtime/native_rhi.*` — a
  "title-neutral, PM4-independent semantic frame". That concept is promoted out
  of gears1 into `render-common` and reused by every framework, so Vulkan
  device/queue/swapchain/pipeline-cache management, resource lifetime, and the
  frame graph are written once.
- **Faithful vs native paths stay a framework concern.** `psxport` already has
  both (`gpu_beetle` faithful, `gpu_native` + `gpu_vk` native). That distinction
  is about how a *guest* API is interpreted, so it belongs in the frontend, not in
  `render-common`.
- **A frontend is not an HLE shortcut.** DDraw and D3D8 frontends implement the
  real API surface the game calls, verified against the guest's own behaviour —
  not a per-title fast path that happens to make one game's menu appear.

The same split applies to audio (guest API: DirectSound / SPU / AX / XAudio →
neutral mixer → host backend) and input. Those are smaller; fold them into
`render-common`'s sibling libraries only when a second consumer actually exists.

---

## 7. Overrides

Unchanged in semantics; the plumbing gets cleaner.

1. The executor is about to enter a guest function at `pc`.
2. The override table is consulted. Hit → the native function is invoked with
   arguments marshalled from the guest register file per that ABI (MIPS o32, PPC,
   cdecl/stdcall).
3. The override may **super-call**: run the original guest function through the
   JIT to its natural return, then continue.
4. On return, out-registers are written back and the guest resumes.

"Keep the recomp body alive for A/B" is now free — the guest code is always
there to run. The A/B toggle disables the table wholesale. Dynamic-dispatch-only
targets still need interposition at the indirect-call site, which each
framework's core must expose.

---

## 8. Verification

**The bar is a working game that looks right — not a difference count.**

USER 2026-08-30 (recorded in `psxport/CLAUDE.md`, and stated there to apply to
every PSX project): "pixel matching doesn't matter. I just want working game that
looks correct." … "It's pretty frustrating that all the previous work went to
pixel matching instead of just verifying it works fine and looks fine wide/60."

So everything in this section is **diagnostic machinery for finding the cause of
a visible defect**. A divergence count is never a completion condition and never
blocks a migration step. What gates a step is: the game runs, reaches its
conformance milestone, and looks right when driven.

### Where a pixel comparison is still valid, and where it is meaningless

USER 2026-09-01: "it's not that it doesn't matter, it matters in some cases like
FMVs but PSX for example has wobble that a native depth renderer wouldn't."

The distinction is **whether the port is deliberately diverging at that surface**:

| Surface | Pixel/sample compare | Why |
|---|---|---|
| FMV / MDEC decode output | **Valid** — should match | The port decodes the same stream; a difference is a decode bug |
| 2D blits, UI, palette/CLUT | **Valid** | No projection change; the port reproduces these |
| Audio PCM | **Valid** | Same reasoning |
| 3D geometry through a native-depth / native-projection renderer | **Meaningless** | The port deliberately removes the console's fixed-point vertex snapping ("wobble") and affine texture warping. A non-zero diff is the enhancement working, not a defect |
| Anything under widescreen or interpolation | **Meaningless** | The frame is intentionally not the original frame |

So a harness must know which surface it is diffing. A whole-frame diff over a 3D
scene reports the enhancement as a divergence and drowns the real signal — that is
how a difference count became a time sink.

### An agent must never use its own reading of a frame as the oracle

USER 2026-09-01: "it matters in cases where the output should be 1-1 identical
because the agent has no way of knowing what is a bug and what isn't. Often I show
it an image for example, something is flickering in and out, and I send a shot
where it is visible, no flickering is visible on that shot but the agent thinks
the shot has a bug."

This is the strongest argument for keeping exact comparison, and it is about the
agent's competence, not the user's. An agent looking at one frame cannot tell
"this is a defect" from "this is how the game looks" — so it invents defects in
correct output and misses them in broken output. A reference is what makes the
question answerable at all.

USER 2026-09-01: "'eyeballing' is not strictly banned, it does help when comparing
two shots that should visually match but may or may not have 100% pixel matching
such as again PSX wobble or like widescreen view or 60fps interpolation."

So the rule is not "never look" — it is **always against a reference**. What is
banned is judging a *single* image in isolation. Three cases:

| Situation | Method |
|---|---|
| Output should be bit-identical (FMV, 2D/UI, palette, audio) | **Exact compare.** The diff is authoritative |
| Output deliberately diverges but should show the same scene (wobble removed, widescreen, 60 fps interpolation) | **Visual comparison of two shots.** A pixel diff is meaningless here; looking at both is the right tool and often the only one |
| One image, no reference | **Not a method.** This is where an agent invents defects and misses real ones |

- **When the user supplies a screenshot of a defect, the defect is what they say
  it is.** Do not re-read the image and diagnose something else, and do not
  conclude the shot is fine because the reported artefact is not visible in that
  frame — an intermittent artefact is absent from most frames by definition. Ask
  for the frame range or a capture, or reproduce it; never overrule the report
  with an inspection of the still.
- A harness therefore has to produce **pairs**, not verdicts: the two shots side
  by side for the diverging surfaces, the exact diff for the faithful ones.

The completion condition is still a working game that looks right, and a
difference count is still not the goal. But a reference — exact or visual — is
how an agent earns the right to make any claim about the picture.

The old harnesses diffed generated C against a reference emulator, per function.
There is no generated C now. Replacement diagnostics, in order of use:

1. **JIT Core vs the engine the project already ships**, in lockstep, comparing
   guest state at frame boundaries and pausing at the first divergence. During a
   migration that pairing is JIT-vs-substrate, which needs no new engine — both
   already exist. `psxport`'s `sbs.cpp` is this, already built.
2. **Whole system vs the stock reference emulator** (Xenia / Dolphin / Beetle /
   the real thing) at frame granularity: frame hashes, audio PCM, deterministic
   input replay. Unchanged in spirit from today's SBS / `parity_sweep`.
3. **Single-step from the last known-good point** to localise a divergence found
   by (1) or (2). Where an interpreter exists it is the most useful engine to
   step, because it reaches code the other engines may not.

Note what this is *not*: standing up a reference interpreter purely to validate a
borrowed, already-proven core. See §5, "Interpreters: required only where we
wrote the translator".

Determinism requirements are unchanged: fixed input replay, no wall-clock in the
sim path, seeded RNG, deterministically-scheduled guest threads for harness runs.

**This is the highest-risk part of the migration** — not because a diff must go
to zero, but because these harnesses are how a visible defect gets root-caused at
all. Losing them means going back to guessing, which is what motivated building
them. Re-anchor them on the reference interpreter *before* deleting the static
path in each project, and re-derive the existing residual/known-divergence lists
rather than assuming they carry over.

---

## 9. Persistent translation cache

USER 2026-09-01: "JIT caching could be used although I'm not sure what benefit it
would have."

It is worth building properly, and the benefit is clearest where the earlier
draft under-rated it: **Android and other weak CPUs**, where cold translation of
a large title is visible at startup and again after every scene that pulls in new
code. On a desktop x86-64 machine the win is small; on a phone it is the
difference between an instant launch and a stutter-filled first minute.

Design, owned by `jit-common` (§3.3):

- **Key** binds everything that can change emitted code: guest image SHA-256,
  framework version, core revision, codegen configuration digest, host
  architecture, and the override-set digest. Any mismatch discards the cache
  silently and re-translates — never partially trusts it.
- **Never persist blocks from writable/self-modifying regions.** Overlay-loaded
  and bank-switched code is cached only when the framework can name the overlay
  identity; otherwise it is memory-only.
- **Position independence.** Persisted blocks must be relocatable or the cache
  must record the code region's base and refuse a load that cannot reproduce it.
- **Verification mode.** A harness flag that loads the cache and re-translates in
  parallel, asserting the two agree — otherwise a stale cache becomes an
  undebuggable heisenbug.

Build it after the first framework runs a real title, so the format is designed
against a real block layout rather than a guess.

---

## 10. Lucent, Android, and the platform layer

USER 2026-09-01: "Android doesn't have to be in Lucent also, it can be separated,
whichever makes the best design."

It should be separated. Lucent's stated identity is "logging, configuration, and
a loopback HTTP control channel — no third-party dependencies". The Android
runtime pieces currently living there (`touch.h`, `content.h`, `platform.h`,
`platform_c.h`) are a different concern with different dependencies: they need
SDL, the Android SDK/NDK, JNI, and the Storage Access Framework. Keeping them in
Lucent means every consumer of a logger inherits an Android surface.

Proposed factoring:

- **`lucent` (L1)** — logging, configuration, loopback HTTP control channel, ZIP
  safety. Dependency-free and host-agnostic. `touch`/`content`/`platform` move
  out.
- **`shared/android-runtime` (L2, new)** — everything title-neutral that executes
  *inside the APK*: SDL Activity lifecycle, app-private user-data handoff,
  persisted SAF grants and bounded staging, raw touch-contact capture and
  cancellation, window insets and lifecycle. This is what Lucent's Android
  headers are today, extracted with their tests.
- **`shared/android-port` (L2, exists)** — unchanged: deterministic build and
  device mechanics (pinned Gradle/AGP/NDK, the native dependency prefix, APK
  inspection and signing, the shared emulator lock/AVD contract).
- **Touch *meaning* stays in the title/framework.** `android-runtime` reports
  contacts; what a contact means — a button, a stick, a gesture — is input policy
  and belongs where the rest of the input policy lives.

The global `AGENTS.md`/`CLAUDE.md` Android ownership table currently assigns the
APK runtime to Lucent; it must be updated in the same change that performs this
extraction, and every Android consumer (`lf2`, `xmen2`, the PSX Android work)
repointed atomically.

The JIT's executable-memory requirements (§3.1) are **not** part of this. They
need only `memfd_create`/`mmap`/`mprotect` and cache-flush intrinsics, so they
stay in `jit-common` where the W^X policy lives, and no Android SDK dependency
follows them.

---

## 11. Per-project migration checklist

For each project, in order:

1. **Freeze.** Record the current conformance-target state in
   `docs/project-state.md`; note the executor swap as the active focus.
2. **Stand up the framework** (or extend it, for `psxport`) and wire the build.
3. **Reference interpreter first.** Promote or write the guest-CPU interpreter
   and get a boot trace green *before* touching the static path.
4. **JIT to first frame** with the memory map and platform HLE bound. No
   overrides yet.
5. **Re-validate the residual list** against JIT-vs-interpreter and against the
   stock reference emulator. Port real divergences; discard stale ones.
6. **Re-point overrides**; run the A/B toggle both ways.
7. **Reach the project's existing conformance milestone** through the JIT
   (psxport: Tomba!2 gameplay; sunbright: file-select; gears1: boot + in-game).
8. **Delete** the static translator, generated C, regen build step, and every
   doc reference. Update `codemap.md`, `project-state.md`, `re-frontier.md`,
   `README.md`, and `run.sh` status text — this is where the per-project "recomp"
   wording sweep happens.
9. **One landing gate**, then commit and push.

Sequencing proposal (the user has no preference): `psxport` first — it is already
a clean framework/title split, lightrec is the easiest embed, and one migration
covers ~12 titles. Then `xenonport` (Xenia is designed to be embedded), then
`gcnport` (largest extraction, since `sunbright` must be split into
framework+title), then `x86port` on `lf2` (smallest x86 surface, DDraw frontend),
then `xmen2` (adds the D3D8 frontend).

---

## 12. Skills and global instructions

USER 2026-09-01: "Migrate recomp-* family to jit-*" / "change all recomp wording."

- `shared/re-harness/skills/recomp/` → `skills/jit/`:
  - `jit-port` (was `recomp-port`) — umbrella: the interpreter-first →
    JIT → override → verify loop, faithful-first-then-enhance, provisioning.
  - `jit-init` (was `recomp-init`) — starting a new platform framework or a new
    title over an existing one.
  - `jit-core` (was `recomp-recompiler`) — selecting and embedding a CPU core,
    host-arch portability, the reference interpreter, indirect-call interposition.
  - `jit-overrides` (was `recomp-overrides`) — largely survives verbatim.
  - `jit-harness` (was `recomp-harness`) — JIT-vs-interpreter inner loop plus
    whole-system-vs-emulator; residual re-validation.
- A new `port-frameworks` skill documents the L1–L4 layering and where a new
  responsibility belongs — or this is folded into `codemap` guidance.
- Global instruction edits:
  - "Static-recompiler projects never use CI" — keep the rule, reword to
    "guest-binary JIT ports"; the reason (the user's game files must never reach
    a hosted builder) is unchanged.
  - "Generated code is sacrosanct / never hand-edit generated C" — delete. There
    is no generated C. Replace with the vendored-core fork rule.
  - Instruction-coverage-table-as-build-gate — delete.
  - The shared-repo table gains `jit-common`, `render-common`,
    `android-runtime`, and the platform frameworks; loses `recomp-x86` and
    `xenon-host` (absorbed into `xenonport`).
  - The Android ownership table is rewritten per §10.
- Re-run `tools/install_skills.py` after the rename.

Do the rename and terminology sweep early; refine the skill bodies as each
framework lands so they describe what actually worked.
`x360/gears1/AGENTS.md` Product-target is already updated (2026-09-01).

---

## 13. Open questions

1. **Framework names.** `gcnport` vs the user's "Aurora"; `xenonport`;
   `x86port`. Cosmetic but they end up everywhere, so settle before the first
   repo is created.
2. **Repo granularity.** Each framework its own git repo under `~/repo` (like
   `psxport`) or a directory inside a monorepo? Current tree is repo-per-project,
   so repo-per-framework matches.
3. **`sunbright` extraction boundary.** Splitting a live project into
   `gcnport` + title is the largest single piece of this migration and the one
   most likely to disturb in-flight work. Decide whether it happens before or
   after the file-select milestone lands.
4. **Xenia ARM64 backend** (§5) — write one, or accept `xenonport` as
   x86-64-only and record it as a stated limitation?
5. **`render-common` scope.** Promote gears1's `native_rhi` immediately, or let
   the second consumer (`psxport`'s `gpu_vk`) drive the extraction so the
   abstraction is shaped by two real users rather than one?
6. **Threading determinism.** Xenon and Gekko titles are multi-threaded; confirm
   Xenia's and Dolphin's scheduling can be driven deterministically for replay
   before committing the harness design to it.
