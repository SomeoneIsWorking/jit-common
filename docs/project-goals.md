# Project goals

This document owns the epic-level intent of the portfolio migration. The
architecture and ordering live in `docs/migration.md`; factual progress lives in
`docs/project-state.md`; atomic work lives in `docs/issues/`.

USER 2026-09-04: "I'm not going to do any more code generation style static
recomps anymore, you can remove everything about that methodology"

USER 2026-09-04: "I need native/dynarec hybrid projects"

USER 2026-09-04: "Okay then I'll allow you doing what DuckStation does"

USER 2026-09-04: "For all projects, not just PSX"

USER 2026-09-04: "And uh weaker consoles like NES/GBA/Amiga can be interpreter, sorry I forgot about them"

## G001 — Every port has one runtime-executed native hybrid

The product executes native overrides where they exist and consumes the user's
original binary at runtime. PSX, x86, GameCube, and Xbox 360 default to dynamic
translation; their interpreter may run only as a reason-coded, measured fallback
after the JIT refuses a block. NES, GBA, and Amiga may use a maintained
interpreter as the shipping CPU when representative gameplay meets each host's
correctness and performance budget.

Success conditions:

- No build, install, provisioning, or release path emits guest code as source,
  object files, or a precompiled title substrate.
- Every dynarec-class product offers ordinary cold blocks to the JIT first and
  reports translated and fallback blocks/instructions with denominators. A
  fallback-dominated run or zero translated blocks is not dynarec evidence.
- Every low-power interpreter-class product proves representative interactive
  gameplay on each released host; boot, menus, logos, or video are insufficient.
- Native overrides are selected by runtime guest identity and address, can be
  disabled for A/B diagnosis, and can call the original guest body through the
  dynarec without recursion.
- Adding or removing an override never regenerates guest code.
- Before dynarec implementation resumes, each title's generator, generated
  corpus, static dispatcher, generation-only seeds, static-only tests/config,
  and static methodology are deleted. The build fails explicitly at the missing
  runtime-executor boundary until the replacement lands.

Contributing state items: S010–S015, S020–S032, S040–S045, S050–S051,
S060–S062, S070–S081.

## G002 — Each guest platform has one execution owner

`psxport`, `x86port`, `x360port`, `gcnport`, `gbaport`, `amigaport`, and
`nesport` own their respective CPU integration, executable-image identity,
native-call boundary, invalidation, scheduling exits, and diagnostics. Titles
provide game identity, native overrides, and title policy.

Success conditions:

- Every title consumes exactly one platform framework.
- A proven embedded core retains ownership of its own code cache and executable
  memory; `jit-common` does not wrap or duplicate it.
- `jit-common` contains no guest-CPU knowledge and gains an abstraction only
  after at least two frameworks demonstrate the same contract.
- Platform frameworks contain no title-specific addresses or behavior.

Contributing state items: S002, S010, S020, S040, S043, S050, S060, S070,
S080.

## G003 — Conformance covers representative gameplay

Boot logos, menus, attract modes, and FMV playback are useful checkpoints but
cannot establish gameplay correctness or performance. Migration evidence must
exercise the native and dynarec sides of the actual hybrid product during
representative interactive gameplay.

Success conditions:

- After the static product is deleted, each title re-establishes its current
  verified capability frontier through the dynarec.
- Each completed title adds a bounded representative-gameplay scenario with
  guest-PC/register, memory, interrupt/timing, and relevant device evidence.
- Diagnostics prove both their positive and negative answers and report
  denominators; absence of a symptom is not evidence without reachability.
- Independent emulator, hardware, binary, or test-only interpreter evidence
  remains available for root-causing the first divergence. The retired static
  product is not retained as a permanent oracle.

Contributing state items: every title capability in S011–S012, S021–S032,
S041–S042, S051, S061–S062, S071, and S081.

## G004 — Every declared dynarec host has a real backend

USER 2026-09-04: "Try to also make arm64 work for both arm64 macs and Android"

Host support is a verified backend property, not an assumption and not an
interpreter escape hatch for a dynarec-class platform.

Success conditions:

- Every released desktop and mobile target has a dynarec backend for its host
  architecture and passes the title's representative-gameplay gate there.
- ARM64 support covers both Apple Silicon macOS and Android arm64-v8a as
  separately qualified host targets; success on one does not imply the other.
- Executable-memory publication, instruction-cache coherence, block
  invalidation, and ABI transitions are exercised on each supported host class.
- A missing backend is reported as a missing capability; bounded fallback may
  cover individual refused blocks but never replace the backend or qualify it.
- Runtime-populated caches are disposable user data, bound to the exact guest,
  core, host, and configuration, and never required by a fresh install.

Contributing state items: S002, S010, S020, S040, S050, S060, S070, S080.

## G005 — Plans and guidance describe only the live methodology

The old static-recompiler methodology is not preserved as an alternative,
legacy section, or compatibility path.

Success conditions:

- Global skills and instructions describe native/dynarec hybrids and explicitly
  reject offline guest translation.
- Every affected project's goals, state, codemap, plan, launcher documentation,
  and tests use the dynamic ownership model.
- Historical evidence is retained only when it states a still-useful binary or
  behavioral fact; generated-symbol workflows and static-process instructions
  are removed.

Contributing state items: S001, S003–S006, S013, S043.

## G006 — Kirbh preserves its deferred product intent

USER 2026-09-04: "My goal with it was drop-in splitscreen multiplayer and wider camera angle etc but eh"

USER 2026-09-04: "you can put kirbh back in scope I guess but I won't work on it soon, just note the project goals"

When Kirbh resumes, it starts from one clean native/emulator architecture rather
than reconciling its competing WIP product paths. Its intended product provides
drop-in split-screen multiplayer and a wider gameplay camera.

Success conditions:

- One `gbaport` built around a maintained GBA core executes every non-native
  gameplay path; a shipping interpreter is allowed after representative
  gameplay qualifies it on each released host.
- Players can join and leave split-screen play through an explicit input and
  shared-state ownership model.
- The wider camera renders additional world coverage through deterministic
  projection, viewport, scissor, and proven culling ownership without stretching
  or frame-aware sampling.

Constraint: this goal is recorded but explicitly deferred; it does not enter the
near-term migration order until the user changes its priority.

Contributing state items: S060–S062.

## G007 — X-Men 2 and MUA share one Alchemy engine layer

`shared/alchemy` is the common owner for title-neutral Alchemy engine behavior.
Its existing libraries and tools are a partial foundation, not proof that either
gameplay product consumes a shared runtime.

It remains one repository with three cohesive component families: a neutral
`shared` engine core, a separately linked `x86` adapter over `x86port`, and a
separately linked `x360` adapter over `x360port`. The neutral core has no CPU
framework dependency. The title composes and pins the relevant sibling
repositories and links exactly one adapter; Alchemy does not vendor or embed
both platform frameworks as submodules.

Success conditions:

- X-Men 2 links and calls a narrow shared runtime contract in representative
  gameplay through the x86 adapter, beginning with the `alchemy_input` guest
  `igControllerManager` contract and an A/B comparison against its retained
  path.
- New shared stateful owners use focused C++ RAII/composition; proven stateless C
  parsers remain C behind narrow interfaces.
- Shipping library code receives typed configuration and a configurable logger;
  it contains no title-specific `X2VIEW_*`, direct `getenv`, or direct stderr
  policy.
- MUA's Xbox 360 dynarec migration proceeds through `x360port`; its Alchemy
  adoption remains gated until every X-Men 2 goal passes, then consumes and
  extends the proven shared contracts through the x360 adapter while keeping
  MUA addresses and policy local.
- Both gameplay build/link/call-path audits prove actual shared-library use; a
  checkout, provisioner pin, or offline XMLB/ARK tool invocation is insufficient.

Contributing state items: S014, S015, S044.

## G008 — Xbox 360 UE3 ownership has three explicit layers

Gears uses a platform runtime, an Xbox 360 UE3 integration layer, and a
Gears-family engine layer. These are distinct because Xbox execution is useful
to non-UE3 titles, UE3/Xbox contracts are useful beyond Gears, and Gears title
behavior must not leak into either shared framework.

Success conditions:

- `x360port` owns authenticated XEX mapping, Xenia CPU/thread/dynarec embedding,
  Xbox kernel/device services, raw Xenos/XMA boundaries, typed imports, runtime
  overrides, original calls, invalidation, and explicit singleton constraints.
- `x360ue3` depends only on public `x360port` interfaces and owns reusable UE3
  Xbox platform contracts: versioned engine ABI descriptions, UE3 RHI semantic
  operations, object/resource/thread/frame lifetime, and title-supplied binding
  schemas. It contains no Gears address, shader hash, pass roster, navigation,
  save policy, or gameplay rule.
- `GearsUE3` consumes `x360ue3` and owns Gears-family behavior, exact
  title/revision bindings, pass identities, native subsystems, enhancements,
  conformance, and application composition.
- MUA consumes `x360port` and `shared/alchemy`; it does not depend on
  `x360ue3` or `GearsUE3` because MUA is an Alchemy title, not a UE3 title.
- The existing `shared/ue3` checkout is reference material only. No gameplay
  product builds, links, packages, or copies it, and `x360ue3` remains an
  independently authored clean-code framework.

Contributing state items: S040–S045.
