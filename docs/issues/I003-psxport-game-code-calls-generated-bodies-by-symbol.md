# I003 — PSX native code still calls generated guest bodies by C symbol

state_items: S020, S021
goals: G001, G002
status: open
opened: 2026-09-01

A no-generated Tomba! 2 link experiment found 816 distinct unresolved symbols
across 109 game files: 405 raw generated bodies, 300 override-aware wrappers,
roughly 106 overlay bodies/wrappers, and four generated override setters. The
measurement proves that framework linkage is not the only migration boundary;
title-native code also names the old execution implementation directly.

Replace those references with two runtime operations owned by the per-`Core`
PSX dynarec executor:

1. normal address dispatch, which honors an image-scoped native override; and
2. one-call original dispatch, which suppresses only the current override and
   executes the guest body through the dynarec.

Overlay identity must be part of the key because distinct modules reuse guest
addresses. Installing or removing an override must invalidate any translated
path that captured the prior decision. Completion requires the Tomba! 2 product
to link with the generated corpus physically absent and the gameplay binary to
contain no test interpreter.
