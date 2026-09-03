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

   **Bisecting `all` (2026-09-02, `pc/xmen2` fa194bd) found the segment
   bases were never bridged.** FS is per-THREAD, so it lives in `g_fsbase` on
   the substrate side and in the CPU on x86port's, and the state bridge stepped
   over it. `mov eax, fs:[0]` is the opening of every /GS-compiled function's
   SEH prologue — seven bytes into `FUN_004874b0`, the first function taken —
   and with a zero base it reads guest address 0. It faulted at 0x3, which
   looks exactly like a null dereference and says nothing about segments.

   Two things were needed to find that and are worth keeping. The take set
   grew **range and module forms**, because `all` is either clean or broken
   somewhere and halving the set is the only way to turn that into an address;
   four bisect steps reached one function. And the engine can now say **where
   it is** on any stop path — a host backtrace ends at `x2_engine_call`, so the
   first run reported a SIGSEGV whose only named body was an import thunk that
   had nothing to do with it.

   **Measured, 60 frames, offscreen, 3243 bodies taken: 794 calls entered,
   6091 guest instructions, 289 handed back, deepest nesting 2**, run clean,
   default substrate run unchanged. The take report's `routed` matches the
   engine's own call count exactly, which it did not before: counting the
   dispatcher's "route this?" and the engine's "interpret into this?" as one
   number reported 1149 dispatches for 794 calls.

   **The setjmp boundary is owned, and THE FRAME BUDGET IS MEASURED**
   (2026-09-02, `pc/xmen2` 3b45f5e). The engine's run loop is a live host
   frame, so it takes the guest's `setjmp` itself rather than letting the
   import stub record a `jmp_buf` with no frame behind it — same table, same
   `x86_setjmp_buf`/`x86_setjmp_done` pair, same reclaim rules. A guest setjmp
   is a guest control transfer: the register file, ESP included, is restored
   from the snapshot, so the resumed guest has the stack it had at the setjmp
   and not the deeper one it unwound from. The engine's nesting count comes
   back with it, because every engine frame between the jump and the landing
   died with the host frames they lived in.

   Measured, X-Men Legends II, offscreen, unbounded, whole `XMen2.exe` taken:
   **161,742,175 guest instructions over 50,430 engine calls**, 1,243,573
   hand-backs, deepest nesting 4, 69 setjmps taken and 23 longjmps resumed, to
   600 frames.

   | frames | substrate | engine |
   |---|---|---|
   | 60 | 2.46 s | 30.40 s |
   | 300 | 8.20 s | 33.68 s |
   | 600 | 8.43 s | 36.95 s |

   Steady state from the 300→600 window: **substrate 0.77 ms/frame, engine
   10.9 ms/frame — ~14× slower, and inside the 16.67 ms budget.** This is the
   x86 analogue of psxport's 6.10 ms figure (S011), and it says the same
   thing: an interpreter alone is enough to drop static code generation on the
   desktop, and the JIT is an optimisation rather than a prerequisite.

   **Read it with its limits.** Offscreen driver, the attract loop rather than
   gameplay, and only `XMen2.exe` taken — the Alchemy DLLs are still on the
   substrate, so this is not yet the whole game interpreted. It is a bound on
   the exe's share, not a final number.

   **That defect was NOT the setjmp path, and the suspicion above was wrong.**
   With the whole module taken the run printed every shutdown report and then
   died in the host allocator (`sysmalloc: assertion failed`, exit 134). The
   reasoning that tied it to setjmp was circumstantial — the halves that were
   clean also took no setjmp — and a longjmp unwinding a live D3D8 lock was a
   story, not a measurement. AddressSanitizer named it in one line once it
   could run at all: the heartbeat's import probe sizes its snapshot by
   `x86_thunk_count()` at first use and then walks to the CURRENT count, and
   the thunk table grows for the life of the process, so the probe wrote past
   its own allocation from a second thread. Fixed by sizing it with
   `x86_thunk_capacity()` (`pc/xmen2` 73687cb).

   ASan could not run before this: its shadow is at low addresses and collides
   with the identity-mapped guest at 0x00400000. `X2_GUEST_ARENA_RESERVED`
   forces the rebased arena a desktop build otherwise skips — the same path
   Apple Silicon and Android take in production. That is the reusable part of
   this: the port now has a sanitizer build.

4. **DONE 2026-09-02 — the corpus is gone, not shrunk.** The generator, the
   generated bodies, and their inputs were deleted in one change (`pc/xmen2`
   27f0a7b), on the user's instruction to delete first and let the build break
   rather than validate the replacement alongside the old path.

5. **DONE 2026-09-03 — the whole game runs on the engine** (`pc/xmen2`
   8270f4a, 73687cb). Not a take set: there is nothing to take from. Every
   guest instruction in all 20 modules and `XMen2.exe` is decoded and executed
   by the interpreter, the game creates its D3D8 device, presents, reaches
   `X2_MAX_FRAMES` and exits 0. Measured, offscreen, 5 frames: **374,155,039
   guest instructions over 22,608 engine calls**, 836,547 hand-backs, deepest
   nesting 5, 69 setjmps and 23 longjmps resumed.

   What the deletion turned up, and what each needed:

   - **Nothing could answer "does this host implement KERNEL32!X".** The
     generated per-module IAT listings were that answer. It is a property of
     the host, not of the player's images, so it is now written in the host:
     one table per DLL surface, 398 entries across 12, each built from a
     single macro list that also declares the stubs.
   - **By-ordinal imports were never asked.** The binder only ever consulted
     the registry by name, so WS2_32 — imported entirely by ordinal — went
     straight to poison and `WSAStartup` could not answer.
   - **Three refusals were about the corpus, not about correctness.**
     `_initterm` listed every constructor target with no recompiled body and
     aborted (that list existed to be fed back to the lifter as seeds); the
     exe's entry point was refused when it had no body, which is now every
     run; `engine_file_path` asked whether libIGCore's setter had been lifted
     instead of whether the address is inside the mapped image.
   - **The engine's step cap killed the game.** `main` does not return until
     the process exits, so counting its instructions against a "this call is
     not finishing" cap measured how long the game had been played. The
     program's entry point is exempt; every call it makes is still capped.

   **Read it with its limits.** Offscreen, attract loop, 5 frames — a
   correctness result, not a performance one. The 5 frames took ~70 s of wall
   time, most of it before the first present, and the per-frame steady state
   under the whole-game interpreter has NOT been measured yet. The 10.9
   ms/frame figure above covers `XMen2.exe` only, with the Alchemy DLLs still
   on the substrate that no longer exists.

6. `pc/xmen2/docs/strategy.md` argues for static recompilation and is now
   describing a thing that has been deleted. Still to rewrite.
