# I004 — X-Men 2 dynamic product needs representative conformance

state_items: S010, S011
goals: G001, G003, G004
status: open
opened: 2026-09-01

X-Men 2 has removed its roughly 307 MiB generated guest corpus and its x64 JIT
executes the title at runtime. Earlier entry, selftest, and level-load evidence
established real translated execution but did not qualify representative
interactive gameplay or every declared host.

Required completion:

- synchronize canonical `shared/x86port` with the consumer and remove the old
  static engine from its public API and documentation;
- make the interpreter a separately built test oracle that is absent from the
  gameplay product's link and selector surfaces;
- run a bounded representative gameplay scenario through the shipping JIT with
  native overrides active, nonzero translated blocks, and relevant CPU/memory/
  timing/device evidence against an independent oracle;
- qualify required host backends, reporting a missing ARM64 JIT as a missing
  capability rather than falling back to interpretation.
