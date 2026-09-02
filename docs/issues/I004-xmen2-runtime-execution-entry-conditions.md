# I004 — `pc/xmen2`: what runtime execution needs, measured

state_items: S040, S041, S043, S047
goals: G001
status: open
opened: 2026-09-01

Ground truth for the x86 side of the migration, established by inspecting the
running port rather than by assuming psxport's shape transfers.

## The corpus being replaced

| fact | measurement |
|---|---|
| translated functions | **116,500** unique `fn_<module>_<ep>` symbols |
| module translation units | 89 `.c` files in `src/recomp/` |
| generated C on disk | **307 MB**, gitignored, regenerated at build time |
| instruction-level holes | **8,234** `x86_unsupported_insn()` call sites |

The last row is the correctness argument, and it is the x86 analogue of
psxport's "the interpreter runs overlay code the recompiler misses" (S010). The
mnemonics the translator gives up on are not exotic:

```
/* NOT TRANSLATED: CMOVZ EAX,EDX -- mnemonic CMOVZ */
x86_unsupported_insn(0x10012fa0U, 0x10012fddU, "FUN_10012fa0", "mnemonic CMOVZ");
/* NOT TRANSLATED: FCOMIP ST0,ST1 -- mnemonic FCOMIP */
x86_unsupported_insn(0x10013380U, 0x100133abU, "FUN_10013380", "mnemonic FCOMIP");
```

Each is a loud abort if reached. Ranking the reasons turns "8,234 holes" into a
much smaller job than the number suggests:

| reason | count | share |
|---|---|---|
| **3DNow! (`PFMUL` 3823, `PFADD` 2156, `PFSUB` 423, `PFACC` 300, + 16 more)** | **7,410** | **90.0%** |
| everything else — 100 distinct reasons, none above 114 | 824 | 10.0% |

**Ninety percent of the corpus's holes are one instruction family, and it is
twenty opcodes.** 3DNow! is AMD's K6-2/Athlon SIMD extension: pairs of packed
floats in the MMX registers, semantically simple, and an interpreter closes the
whole family once. They arrive as 2,440 sites each in `libIGGfx_003.c`,
`cgD3D8_000.c` and `XMen2_011.c` — the identical count says this is one
3DNow!-compiled Alchemy math library statically linked into three modules, not
three problems — plus 90 in `libIGMath_008.c`.

The 10% tail is the second argument. It is led by `operand 'extended double ptr
[ESP+…]'` (x87 80-bit spills, 258 sites) and then by `INT`, `STD`, `LODSD`,
`INSB`, `OUTSB`, `ARPL`, `BOUND`, `DAA`, `AAS`, `IN`, and bare segment registers
— 16-bit and privileged opcodes that a 2005 Windows game does not execute. The
translator's own note says so: *"GS segment override (the shipped occurrences are
embedded data decoded as code)"*. These are **data that static analysis had to
guess was code**. An interpreter never guesses: it decodes only what execution
actually reaches, so this entire class of hole stops existing rather than being
fixed.

**Read this finding honestly.** It does not by itself argue for runtime
execution: the static translator could implement those same twenty 3DNow!
opcodes and close 90% of its own holes. What it argues is that the *decoder*
work is small, wherever it lands — so the interpreter is cheap to reach a useful
coverage level with, and the cost of S043 is much lower than "8,234 holes"
implies. The reasons to migrate remain G001 (no build-time code generation), the
307 MB regeneration cost, and the misdecoded-data class above, which only an
interpreter removes structurally.

## Four entry conditions, and they are already met

This project is better positioned for runtime execution than psxport was.

1. **The guest machine code is already in guest memory.** `src/native/pe_map.c`
   `memcpy`s each PE section's raw bytes to its virtual address
   (`memcpy(guest_memory_pointer(base + va), f + raw, n)`), so `.text` is
   readable at its mapped address at run time. An engine needs no new loader.
2. **There is exactly one dispatch owner, and its miss is already explicit.**
   `x86_native_call_at(addr, CPU *)` returns 0 when no translated body exists,
   and `x86_dispatch_one()` reports and fails on that. "No body at this address"
   is therefore already a named, reached condition — which is precisely where a
   runtime engine plugs in, with no fallback that could hide it.
3. **Overrides are already engine-independent.** They are keyed by *mapped*
   address in `g_override[]` and consulted before the body, not installed into a
   generated dispatch table. This is the defect psxport has to fix first (I003)
   and this project does not have.
4. **`CPU` is instance-owned** — a struct passed by pointer, never a global
   register file. That satisfies the in-process-comparison constraint for
   registers.

## The three things that are not met

- **Guest memory is a single process-wide arena.** `guest_memory_pointer(addr)`
  resolves against one mapping, so two engines cannot run in one process against
  independent memory. Registers are per-`CPU`; memory is not. This is what
  in-process A/B comparison needs, and it is a real change, not a detail.
- **85 direct `fn_<module>_<ep>` calls from hand-written code**, across ~30
  files, led by `save_trace_runtime.c` (26), `movie.c` (16), `script_trace.c`
  (12). Same shape as psxport's I003 but an order of magnitude smaller. Each
  must become a call by guest address so it routes through the engine.
  `oracle_trace.c` already documents the hazard: such a call "never passes
  through `x86_native_call_at`".
- **No second engine exists**, so nothing can be measured yet. Unlike psxport,
  where the interpreter was already in the tree and the frame-budget question
  could be answered in an afternoon, here the interpreter must be built before
  §5.1's "measure before escalating" can even be applied.

## Ordered work

1. **S043 first, not S041.** The x86-32 interpreter is the semantics authority
   and also the thing that makes a measurement possible. The initial coverage
   target is measured, not guessed: **3DNow!'s 20 opcodes clear 90% of the
   corpus's holes**, and much of the tail is misdecoded data an interpreter
   never has to decode at all.

   **`shared/recomp-x86` has NO decoder to seed from** — corrected 2026-09-01
   after reading it rather than assuming. Its front end is Ghidra
   (`tools/ghidra_export.sh` → `functions.json`) and `recomp.py` lifts
   disassembled *mnemonic text*, which is exactly why its failures read
   `mnemonic PFMUL` rather than an opcode byte. Its per-instruction semantics
   are a real seed; its decode does not exist. And Ghidra is a maintainer-only
   tool that can never be a player prerequisite, so the interpreter needs its
   own decoder. **Settled 2026-09-01: Zydis v4.1.1, decode only**, pinned in
   `shared/x86port/vendor/zydis` — a decode-only library brings no memory-model
   or threading opinions to fight `jit-common`, which is what disqualifies
   embedding a whole core.

   Validated rather than argued, using an oracle this project already had and
   had not noticed: the Ghidra export records the **raw bytes** of every
   instruction beside Ghidra's own reading. Decoding all 2,168,629 of them
   through the shipping `x86p_decode()` gives 0 failures and 99.9983% length
   agreement, with all 37 disagreements the 0x9B FWAIT-folding convention (both
   readings correct; the literal one is what an interpreter wants). It also
   caught zydis 4.1 spelling PFRSQRT as "PFSQRT" and PFRCPIT1 as "PFCPIT1"
   before that could become two opcodes the engine could not name.

   Started 2026-09-01: `shared/x86port` exists, with 3DNow! semantics as its
   first module — 19 opcodes implemented, the 5 approximation instructions
   refused by name rather than approximated, 427 checks. Deliberately chosen as
   the first piece because it is the largest coverage win that does NOT depend
   on the decoder decision above.
2. Engine selector at `x86_native_call_at`, following psxport's `engine_select.h`
   (I001): total, refusing, one owner. The miss path at `x86_dispatch_one`
   becomes the interpreter route rather than an abort, which makes the first
   engine incremental — the translated corpus keeps running while the
   interpreter takes only what it can.

   **The selector itself landed 2026-09-01** as `shared/x86port`'s
   `src/x86port/engine.{h,c}` (74 checks). It needed more than a copy of
   psxport's: there, all three arms are compiled into every build, so selection
   only has to catch a misspelling. Here they are not — the substrate is
   generated C living in the *title*, and the interpreter does not exist yet —
   and an engine spelled correctly but never linked runs something else just as
   silently as a misspelled one. So `x86p_engine_resolve()` takes an
   availability mask from the consumer and refuses either way, with distinct
   reasons, since a typo is fixed on a command line and a missing arm is fixed
   by building it.

   **Wiring it into `pc/xmen2` is deliberately deferred, for two reasons found
   while doing it.**

   - *It would be dead code today.* Routing the `x86_dispatch_one` miss to an
     interpreter that does not exist adds an arm nothing can select. The seam is
     worth landing when there is something on the other side of it, so step 2's
     xmen2 half now follows the interpreter rather than preceding it.
   - *~~`shared/x86port` has no remote~~ — RESOLVED 2026-09-01.* The port
     resolves shared checkouts through `tools/shared_dir.py` and provisions
     them from a pinned URL + revision in `bootstrap.py`'s `SHARED_REPOS`, so a
     remote-less repo could not be consumed without breaking the fresh-clone
     launcher contract everywhere but this machine. Both repos are now
     published — `SomeoneIsWorking/x86port` and `SomeoneIsWorking/jit-common`,
     public, matching their siblings. What remains for the wiring is a
     `SharedRepo` entry pinned to an exact revision, never a branch.

     USER 2026-09-01: "You can create remotes via gh, note this so it will
     stick" — creating a remote for a new shared repo is pre-authorized and
     does not need a round trip. The pre-push audit for game assets and
     machine-specific paths is not waived by it.

   **LANDED 2026-09-02** (`pc/xmen2` 39b1990). `src/native/x86_engine.c` owns
   the seam; `x86_dispatch_one`'s miss asks it before reporting, so the
   substrate keeps every address it has a body for. `X2_ENGINE` selects, and
   the JIT arm is left out of the availability mask on purpose — a translated
   block emits its direct CALLs inline, and one calling a statically
   recompiled body would jump into host code with a guest EIP, so `jit` is
   refused by name rather than downgraded.

   Four things the wiring found that reading did not:

   - **The bridge is only exact at a call boundary, so it says so.** The
     substrate's CPU has no EIP, no AF and no DF. Flags travel as a
     materialised EFLAGS word rather than as a lazy tuple neither model can
     express in the other's terms; EIP comes from a page of INT3 the returning
     function lands on. AF and DF are dead at a Win32 call boundary, which is
     why the loss is exact rather than approximate.
   - **The trampoline page collided twice before it landed**, at 0x000B0000
     (the data arena) and 0x00090000 (the import poison page). Neither shared
     silently, which turned each into a one-line fix rather than a corruption
     hunt. It sits at 0x00080000, with the low map written down beside it.
   - **A NULL host is an identity mapping.** `X86pMem` asked "is this
     configured" as "is host null", and `pc/xmen2` maps the guest at the
     host's own addresses, so the first interpreted instruction reported a
     fetch fault at a page it had just written. Fixed in x86port (4e40796)
     with both answers under test.
   - **The seam does not fire in a 60-frame run.** Every address the game
     dispatched to had a body, so the report is zeros — and zeros from a
     working engine and from a broken bridge are the same two lines. The
     engine therefore runs a program of its own through the real
     `x2_engine_call` before the game starts, and asks the call-out predicate
     about both a resolved override and plain guest data. Measured: selftest
     passed, 6 guest instructions, 10 checks, 0 engine calls, run identical to
     the substrate.

   What this does NOT yet give is a measurement of the game through the
   engine, because nothing routes a REACHED body to it. Step 3 needs a way to
   make the substrate decline a chosen set of entry points, so the same
   function can be run both ways and compared — the differential of S041,
   applied to real game code.

3. **The take set, and the frame budget (§5.1).**

   **LANDED 2026-09-02** (`pc/xmen2` 4bcfafb). `X2_ENGINE_TAKE` names entry
   points the substrate must hand over even though it HAS them — a list, an
   `@file`, or `all`. Host code is never taken in any mode: an import thunk
   and a native override are C functions with no guest bytes at their address.
   A named address that is one of those, or that has no body at all, stops the
   run rather than being dropped, because a take set that quietly loses half
   its entries reports a measurement of something else.

   **Taking bodies found two defects the miss path had carried since it
   landed, and neither was reachable without it.** This is the argument for
   building the take set before chasing coverage: the seam had a selftest, a
   report with both halves of every ratio, and two bugs that only a function
   the GAME calls could expose.

   - **The engine pushed a return address.** Every caller has already pushed
     the word the callee's RET pops — `x86_guest_call_args` writes 0xDEADBEEF
     there explicitly — so the guest returned to the engine's trampoline
     correctly, the engine's own stack check passed because its frame
     balanced, and the caller's return address was left behind. Four bytes of
     guest stack per taken call, caught by `x86_guest_call` on the first one.
     The engine now leaves on the caller's own return address with the stack
     unwound past it; the address alone is not enough, because it would also
     match a CALL to it from deeper in.
   - **The call-out dropped tail jumps.** It used `x86_native_call_at`, which
     does not drain `C->tail_target`; a recompiled body ending in a tail JUMP
     reports it by leaving that set and returning, and only `x86_dispatch`'s
     loop drains it. It calls `x86_dispatch` now.

   Measured, X-Men Legends II, 60 frames, offscreen: four hot bodies taken,
   **4 calls entered, 18 guest instructions executed, 2 handed back**, run
   clean, and the default substrate run unchanged.

   **THERE IS STILL NO FRAME-BUDGET NUMBER, and the reason is a real
   divergence.** `X2_ENGINE_TAKE=all` does not survive startup: it diverges
   inside `msdia80.dll`'s CRT initialisation, at a RET in `__mtinit` that pops
   0x00000a28, and then faults at 0x3. Eighteen instructions is not a
   measurement of anything, and `all` is the configuration that would produce
   one, so §5.1 cannot be applied until that divergence is understood. It is
   the next thing to chase, and it is now a bounded, reproducible question
   rather than one about whether the engine works at all.

4. The 85 direct symbol calls, then per-module removal of the emitted corpus.
5. `pc/xmen2/docs/strategy.md` argues for static recompilation and is rewritten
   in the change that lands the frame-budget number above, not before.
