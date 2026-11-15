# Ground rules

These apply to every phase. Read them before touching code.

## 1. Do not ask; decide

The user wants this implemented without questions. When something is unclear:

1. Look in the phase file, then `03_content_tables.md`, then `01_vision.md`.
2. If still open, pick the option that (a) serves the hook, the town that
   remembers; (b) makes the player decide something instead of adding a
   number; (c) is smaller to build. In that order.
3. Write one line in `DECISIONS.md`: date, phase, what was open, what you
   chose, why.

Stop and report to the user only for these: a destructive action outside the
repository; anything outward-facing (push, pull request, publishing); a build
toolchain that cannot be made to work; or a contradiction in this plan that
makes two phases impossible together.

## 2. Branch and commits

- Work on a new branch `overhaul`, cut from `art-and-feel`.
- One commit at the end of each finished phase. A second, small commit is fine
  if a later phase has to fix an earlier one.
- Message: a short subject in the project's style (see `git log`), for example
  `The home base: furniture as material, windows, stations`, then a body
  listing what changed. **No `Co-Authored-By` trailer. No AI attribution.**
- Never push. Never open a pull request. Never amend a commit of an earlier
  phase.
- Stage files by name or by folder. Do not `git add -A`: a stray
  `build-local/` folder with build output and saves sits in the tree. Phase 1
  adds it to `.gitignore`.

## 3. Build, test, harness

Toolchain: MSYS2 UCRT64 (g++, CMake, Ninja), C++20, SDL2, OpenGL 3.3.

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 3
./build/dead_pixel_tests.exe
```

- `CMakeLists.txt` lists every source and test file by name. A new `.cpp`
  must be added to `dead_pixel_core`; a new test file to `dead_pixel_tests`.
- Tests use `tests/TestFramework.h`: `DP_TEST(name)`, `DP_CHECK`,
  `DP_CHECK_EQ`, `DP_CHECK_NEAR`. Test names are sentences in snake case
  (`taps_run_until_the_small_hours_of_day_3`).
- The harness plays the real game from a script and takes screenshots
  off-screen. **Run every script in a fresh directory** with the PATH above
  exported, or it exits 127 (missing DLLs) or 2 (leftover saves):

```bash
rm -rf /tmp/run && mkdir -p /tmp/run && cp -r assets tools /tmp/run/ && cp build/dead_pixel_survival.exe /tmp/run/
cd /tmp/run && ./dead_pixel_survival.exe --script tools/harness/NAME.txt --hidden --world-seed 1234 --shots shots
```

  A script prints `FAIL` lines when an `expect` does not hold. Exit code 0
  and no `FAIL` line means it passed. On Windows use any scratch directory in
  place of `/tmp/run`.
- New harness commands go in `src/dev/DevHarness.cpp` and are listed in the
  README's command list.

## 4. Definition of done, for every phase

A phase is done when all of these hold:

1. The build has no new warnings (`-Wall -Wextra -Wpedantic`).
2. All unit tests pass, including the new ones the phase asks for.
3. The phase's harness script passes, and **you have opened its screenshots
   and looked at them**. Text that overflows, sprites that sort wrong and
   panels that cover the player are failures even when the script passes.
4. The older harness scripts still pass (run them all; each in its own fresh
   directory).
5. A save made before the phase still loads, or the save version was raised
   on purpose with a migration (section 6).
6. Docs: the status table in `README.md` here, a section and a log line in
   `docs/revival_plan.md`, the controls and command lists in the top-level
   `README.md`, and the Controls panel in the game if keys changed.
7. The commit is made.

Report results honestly in the commit body and the revival plan: if something
was not verified in a screenshot, say so.

## 5. Code rules

- `Game.cpp` only shrinks. New behaviour goes into a system
  (`src/systems`), a rules module (`src/world`, `src/legacy`) or a new
  `src/core/GameXxx.cpp` slice of the `Game` class.
- **Rules live apart from the game.** Put every rule that can be stated
  without the ECS (who rises, what a night sends, what a skill level gives)
  in a plain function in a small module, and test it there. `FirstDays`,
  `LegacyRules`, `Doors` and `ToneTuning` are the pattern.
- No new magic item ids in C++. After phase 1, code names items through
  `ItemIds` (resolved by key at load). Numbers that are tuning live in
  `assets/data/tuning.json` or as named `constexpr` values next to the rule.
- Time: the game runs a fixed step of 1/60 s (`kFixedTimeStepSeconds`).
  One real second is one game minute; 60 s is a game hour; 1440 s a game day.
  State which unit a constant uses in its name or comment (`...PerGameHour`,
  `...Seconds`).
- Tiles are 32 px in world space, drawn as 64x32 isometric diamonds. Use
  `mTileMap.tileWidth()`; do not hard-code 32 in new code.
- Randomness: anything that decides gameplay must come from an RNG seeded from
  the run seed (`mSaveManager.currentRunSeed() ^ salt`) or from
  `deterministic01`. Never `std::random_device` in the simulation. A harness
  run with `--world-seed` must replay the same way twice.
- Performance budget: 300 zombies and 20 survivors at 60 ticks/s in Release
  with under 4 ms per tick. Pathfinding is the known risk; phase 4 fixes it.
- Comments say why, in plain words, like the existing code.

## 6. Saves

- `SaveManager::kCurrentSaveVersion` is 1 today. Phase 1 raises it to 2 and
  adds a migration path: a loader reads version 1 files and fills new fields
  with defaults. From then on:
  - New optional fields: read with a default, do not raise the version.
  - A change in meaning, or renumbered data: raise the version and write a
    migration in `SaveManager` with a round-trip test in `tests/test_save.cpp`.
- Item ids already in saves (0 to 29) never change meaning. New items take the
  ids given in `03_content_tables.md`.
- Tile GIDs are saved in layouts and must not be renumbered.
- Every new piece of state says where it lives: **world memory** (survives
  death, one file per kind under `save/`), **checkpoint** (this run only,
  `RunCheckpoint`), or **not saved** (derived or cosmetic).

## 7. Art, interface and sound

- All art is drawn by code: `tools/generate_sprites.py` (atlas, tiles, items,
  furniture, structures), `tools/charrig.py` (people and the dead),
  `tools/scenery.py`. Same pixel scale as today: 64 px character frames with
  a figure about 36 px tall, 32x32 item icons with a 1 px dark outline and
  light from the top-left, 64x80 furniture. Regenerate the atlas with
  `python tools/generate_sprites.py` and commit the new `spritesheet.png` and
  `.json`. Do not hand-edit the PNG. Do not use image generation or bought
  packs.
- The atlas is 4096x2048. Before adding a block of sprites, check free space
  in the generator; if it runs out, grow the atlas to 4096x4096 and keep
  every existing rectangle where it is.
- Do not copy the look of any existing game. Original shapes and palettes.
- Interface: the "bare hands" style. HUD text sits straight on the world with
  its own shadow, no plates. Panels dock to the right edge (pack 540 px,
  make 700 px) and use the palette in `GameShared.h` (`kPanelBack`,
  `kPanelInk`, `kPanelDim`, `kPanelAmber`, `kPanelGood`, `kPanelBad`,
  `kPanelRule`, `kPanelTrack`, `kPanelOnInk`). A chosen row is inverted
  (ink background, `kPanelOnInk` text). Amber means "needs attention" and
  nothing else. No gold edges. Text uses whole-number scales of the 5x7 font.
  Shapes are drawn in the sprite pass, text in the layer's text pass
  (`uiLayer` in `GameRender.cpp`).
- Every new screen or panel gets a harness screenshot at 1280x720, and the
  text must fit at that size.
- Sound: every effect is synthesised by `tools/generate_sounds.py` and
  written to `assets/sounds/`. Phase 12 owns the full sound pass; earlier
  phases may add a placeholder effect when a new action would otherwise be
  silent.

## 8. Writing in the game

Short, plain, concrete, past the point of comfort. The game speaks like the
existing lines: "A door gives way." "A board splinters. 2 boards hold." "The
taps are dry." No exclamation marks, no jokes, no second-person pep talk. A
message says what happened and, when it matters, what is left.

## 9. What not to do

- Do not change the camera, the tile size, or the town generator's layout
  output for an existing seed (saved towns must stay the same town). Phase 6
  changes what buildings *are*, not where they stand.
- Do not add multiplayer, a mod system, a scripting language or a new
  dependency. (`nlohmann-json`, `stb`, SDL2, SDL2_mixer, GLEW and GLM are what
  there is.)
- Do not remove a system because it is shallow; deepen it or leave it.
- Do not add features that are not in this plan. Put ideas in `DECISIONS.md`
  under "Ideas, not built".
