# I002 — psxport test interpreter depth tags are process-global

state_items: S020
goals: G003
status: open
opened: 2026-09-01

`runtime/recomp/interp.cpp` stores per-GPR depth provenance in file-static
`s_pz_kind[32]` even though the registers belong to one `Core`. Two test-oracle
cores can therefore corrupt each other's diagnostic provenance.

Move the tags into test-owned per-`Core` state and add a two-core discriminator.
This interpreter remains outside every gameplay target; fixing its oracle state
does not make it a product engine or fallback.
