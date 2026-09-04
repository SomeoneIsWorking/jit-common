# Codemap

This map records ownership and placement only. Progress belongs in
`docs/project-state.md`; intent belongs in `docs/project-goals.md`; ordering and
acceptance gates belong in `docs/migration.md`.

| Responsibility | Owner | Current location | New work belongs |
| --- | --- | --- | --- |
| Portfolio native/dynarec architecture and migration order | Migration authority | `docs/migration.md` | `docs/migration.md` |
| Portfolio epic outcomes | Goals authority | `docs/project-goals.md` | `docs/project-goals.md` |
| Portfolio capability status and current focus | State authority | `docs/project-state.md` | `docs/project-state.md` |
| Atomic migration work and findings | Issue catalog | `docs/issues/` | One issue under `docs/issues/` |
| Guest-neutral executable-memory primitives | `jit-common` code-memory module | `src/jitcommon/code_memory.{h,cpp}` | Same cohesive module and its tests |
| Guest-neutral address-to-block container | `jit-common` block-cache module | `src/jitcommon/block_cache.{h,cpp}` | Same cohesive module and its tests |
| Public library source tree and build composition | `jit-common` library | `src/`, `CMakeLists.txt` | The smallest cohesive module under `src/jitcommon/` |
| Product-facing library verification | Test suite | `tests/` | The test file for the production module under test |
| Repository maintenance and verification entry points | Tooling | `tools/` | A cohesive Python tool; broadly reusable tools move to the canonical shared harness repository |
| CPU decode, semantics, code emission, scheduling exits, image identity, and invalidation policy | The relevant platform framework | Outside this repository | `psxport`, `x86port`, `xenonport`, `gcnport`, `gbaport`, `amigaport`, or `nesport` |
| Embedded-core executable memory and translated-block cache | The embedded core | Lightrec, Xenia, or Dolphin integration | Its maintained fork/integration; never a duplicate `jit-common` wrapper |
| Game identity, native overrides, native subsystems, and title policy | The consuming title | Each game repository | The title's smallest owning module |
| Title-neutral Alchemy engine behavior shared by X-Men 2 and MUA | `shared/alchemy` | Partial native libraries and tooling outside this repository | Prove the interface through X-Men 2 first; MUA later consumes it through a title-owned ABI adapter |
| Cross-framework abstraction | No owner until demonstrated twice | — | Extract here only after two concrete framework implementations share the same contract |
