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
   and also the thing that makes a measurement possible. Seed its decode and
   per-instruction semantics from `shared/recomp-x86`'s translator, which
   already encodes them. The initial coverage target is measured, not guessed:
   **3DNow!'s 20 opcodes clear 90% of the corpus's holes**, and much of the tail
   is misdecoded data an interpreter never has to decode at all.
2. Engine selector at `x86_native_call_at`, following psxport's `engine_select.h`
   (I001): total, refusing, one owner. The miss path at `x86_dispatch_one`
   becomes the interpreter route rather than an abort, which makes the first
   engine incremental — the translated corpus keeps running while the
   interpreter takes only what it can.
3. Measure against the frame budget (§5.1) before considering S041/S042.
4. The 85 direct symbol calls, then per-module removal of the emitted corpus.
5. `pc/xmen2/docs/strategy.md` argues for static recompilation and is rewritten
   in the change that lands step 3's result, not before.
