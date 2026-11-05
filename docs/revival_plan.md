# Dead Pixel Survival — Revival Plan

Started: 2026-11-05 · Branch: `revival` · Owner: Claude (autonomous until handoff)

This is the living plan for reviving the project after a seven-month pause. It
replaces the old milestone backlog as the source of truth for what happens next.
The GDD (`dead_pixel_survival_gdd_v0.3.md`) is still the destination.

## Direction (decided)

- **Goal:** ship-quality. Build as if it will be released (itch.io demo first).
- **Hook:** the fixed world that remembers past runs. Graves, decayed bases and
  districts that have turned since last time, on top of Zomboid-style simulation.
  Every change is tested against this.
- **Scope:** keep every existing system and deepen them in waves, ordered by
  closeness to the hook:
  1. Core loop: scavenging, needs, noise and zombies, combat, readability.
  2. Legacy layer: graves, persistent bases, world memory, bloodlines.
  3. Extras: NPCs, vehicles, fire, crafting, modes, tone presets.
- **Camera:** isometric stays. Readability work covers wall/roof occlusion and
  diagonal-facing characters.
- **Tech:** no engine switch. Clean-up happens alongside features.

## Working rules

1. Nothing counts as done until it is seen working: a harness screenshot or
   metrics run, not just a successful build.
2. Every phase ends with a build, the tests, a harness smoke run and a commit.
3. `Game.cpp` only shrinks. New behaviour goes into systems or UI modules.
4. Saves stay compatible within the revival, or bump `saveVersion` on purpose.

## Phases

| # | Phase | Status |
|---|---|---|
| 0 | Branch, plan, CI fix | done |
| 1 | Dev harness: scripted autoplay, offscreen screenshots, metrics, input-latch fix | in progress |
| 2 | Baseline screenshots and assessment | |
| 3 | Test target: save round-trip and core logic tests | |
| 4 | Split `Game.cpp` into systems and UI modules | |
| 5 | Wave 1: core loop and first 15 minutes | |
| 6 | Isometric wall/roof occlusion | |
| 7 | Placeholder art generator upgrade | |
| 8 | Wave 2: make the legacy hook visible | |
| 9 | Wave 3: extras polish, tone presets with real effects | |
| 10 | Docs, README and audit sync | |
| 11 | Final verification and handoff | |

## Known issues found during revival

- Input transitions (`wasKeyPressed`) are cleared per render frame, not per
  simulation tick. Presses can be dropped (0 ticks in a frame) or fire twice
  (2 ticks in a frame).
- Tone presets are stored and shown but have no gameplay effect.
- `beginDeathState` and `beginRetirementState` duplicate ~50 lines of
  persistence code verbatim.
- No wall/roof occlusion: indoor zombies and loot can hide behind walls.
- Character sprites face only up/down/right; isometric movement is diagonal.

## Log

- 2026-11-05: Reviewed project, committed shelved M8–M10 work (`11926fb`),
  fixed CI dependencies (`5183a9c`), opened `revival` branch.
