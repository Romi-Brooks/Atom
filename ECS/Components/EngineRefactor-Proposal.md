# Engine Refactoring Proposals

> Archived planning entry point. Active and unfinished architecture work is
> maintained in [`../../Docs/Remaining-Issues.md`](../../Docs/Remaining-Issues.md).

This file is retained so existing links do not break. The earlier proposal mixed
unfinished design ideas with dependency and backend migrations that have since
been completed, which made it unreliable as a current plan.

Current directions include:

- Split `Entity` data and behavior into components and systems without committing
  immediately to a complex archetype ECS. See `ARCH-108`.
- Keep `../../Atom.cpp` as a link/compile smoke executable until a formal application
  runner is introduced. See `ARCH-101`.
- Keep third-party source dependencies as pinned Git submodules, built through
  `../../ThirdParty/CMakeLists.txt`. See [`../../ThirdParty/README.md`](../../ThirdParty/README.md).
- Continue separating window management, rendering, audio playback, and decoding
  through backend contracts and runtime composition. See `RENDER-001` and the
  audio items in the unified issue list.

New architecture work should be added only to the unified issue list so status,
priority, and implementation order have a single source of truth.
