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
| CPU decode, semantics, code emission, scheduling exits, image identity, and invalidation policy | The relevant platform framework | Outside this repository | `psxport`, `x86port`, `x360port`, `gcnport`, `gbaport`, `amigaport`, or `nesport` |
| Embedded-core executable memory and translated-block cache | The embedded core | Lightrec, Xenia, or Dolphin integration | Its maintained fork/integration; never a duplicate `jit-common` wrapper |
| Game identity, native overrides, native subsystems, and title policy | The consuming title | Each game repository | The title's smallest owning module |
| Xbox 360 execution shared by Gears and MUA | `shared/x360port` | Planned platform framework outside this repository | Xenia embedding, XEX/image mapping, Xbox services/devices, raw Xenos/XMA boundaries, typed imports, overrides, original calls, invalidation, and singleton constraints |
| UE3-on-Xbox-360 integration shared beyond a title family | `shared/x360ue3` | Planned clean-code framework outside this repository | Versioned UE3 ABI descriptions, RHI semantic operations, binding schemas, and engine object/resource/thread/frame lifetime over `x360port` |
| Gears-family engine and exact title policy | `GearsUE3` | `x360/gears1` until repository naming is deliberately changed | Gears behavior, exact bindings and pass identities, native subsystems, enhancements, conformance, and app composition |
| Title-neutral Alchemy engine behavior shared by X-Men 2 and MUA | `shared/alchemy` neutral `shared` component | Partial native libraries and tooling outside this repository | Own engine behavior and formats without depending on a CPU framework |
| Alchemy guest-platform ABI/context adaptation | `shared/alchemy` optional `x86` and `x360` components | Intended adapters outside this repository | Consume public `x86port` or `x360port` interfaces respectively; link only the product's platform adapter |
| Exact Alchemy title identity and policy | X-Men 2 or MUA | Consuming title repository | Own executable hashes, addresses, bindings, and title decisions; compose and pin Alchemy plus one platform framework |
| Cross-framework abstraction | No owner until demonstrated twice | — | Extract here only after two concrete framework implementations share the same contract |
