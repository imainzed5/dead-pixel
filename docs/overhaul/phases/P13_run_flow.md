# Phase 13: run flow, settings and the demo

**Goal.** Wrap the systems into a game someone else can pick up: a run that
has shape after day 3, teaching without a tutorial, settings, key bindings,
and a build that runs on a clean machine.

**The player notices:** always knowing one sensible thing to do next; a
first run that explains itself; changing the keys; a zip that just runs.

**Feel target.** A stranger plays for 40 minutes without asking a question.

## Core

### 1. Life after day 3: standing aims

The day plans end on day 3 (`FirstDays`). From day 4 the objective panel
shows **one aim** with up to three steps, chosen each dawn by
`src/world/Aims.h/.cpp` (pure, tested) from the world's facts. Urgent needs
still take the title line, as today.

Aims, in the order tried; the first whose condition holds is shown:

| Aim | Shown when | Steps (each with a count where it has one) |
|---|---|---|
| Water that refills | no working rain collector at the base and fewer than 4 clean bottles | Build a rain collector. Or: boil water (n/3) |
| A house that holds | fewer than all doors and windows of the base boarded or blocked | Close up the house (n/m). Hang an alarm |
| Tonight (horde day) | a horde is due tonight (P04) | Lights out before dark. Stay quiet. Have a way out |
| The power is going | day 4 until it fails | Eat what is in the fridges. Find batteries (n/2) and candles (n/2) |
| Food for a week | fewer than 7 days of food at the base (pack + crates) | Find food (n). Set a snare. Plant a bed |
| Somewhere to work | no workbench or crate at the base | Build a crate. Build a workbench |
| Before the cold | season is Autumn after day 36, no Winter Coat or fewer than 20 planks stored | Find a winter coat. Store wood (n/20) |
| The way out (P09) | lead 1 known, in "The way out" mode | The next unmet step known |
| Someone out there | a lead of kind pack, grave, risen or base within 60 tiles, not yet visited this life | Go to it (direction and distance) |
| Hold on | none of the above | "Day 23." and the season; no steps |

- Completing an aim's steps gives the one-off line "Done." in good colour,
  depression -4 (once per aim per life), and the next aim appears at the
  next dawn (not at once: leave the player a quiet evening).
- Day cards at dawn (exist for days 2 and 3): add fixed cards for day 4
  ("Day 4. Nobody is coming."), the first day without power, day 7 ("A
  week."), each season's first day, and every tenth day. One line each,
  in `assets/data/day_cards.json`; otherwise the card shows only "Day N".
- The bot follows aims after day 3 (extend `Autoplayer`).

### 2. Teaching through play

No tutorial screens. Three devices that already exist do the work:

- **The prompt line** names the verb when the player is first able to use
  it, once per save (a set of seen prompts in the settings file): "Hold E
  and walk to push." "Space to shove." "Crouch near a door to listen."
  "Hold G to throw." After the first successful use it is never shown
  again. Maximum one teaching prompt on screen; they queue.
- **The day-1 plan** is the tutorial. Add one step at the very start, before
  "Search the house": "Look around (WASD to move, E to use)" which checks
  itself after 10 tiles walked and one E press. First life of a world only.
- **The first night** teaches fighting: night 1's first caller is always a
  lone Shambler at reduced health (30), same in every mode (it is one at a
  time already). If the player has not shoved yet, the prompt line offers
  it when the caller gets within 2 tiles.
- **The Controls panel** (F2) is reorganised by situation: Moving, Hands,
  Fighting, Home, Looking (map, notebook, body), with only the keys that
  exist after this overhaul. It fits one screen at 1280x720 in two columns.
- **Death teaches.** The run-end screen gains one plain line under the
  cause, picked from the cause and the run's facts, never more than one:
  thirst ("Bottles can be filled at any tap until day 3."), a night-1 death
  at home with doors unboarded ("Boards on the doors would have held."),
  infection ("A dirty bite needs disinfecting within three hours."), a grab
  death with zero shoves made ("Space shoves them off."). Lines live in
  `assets/data/death_hints.json`, each shown at most twice per save.

### 3. Title flow

- Title menu order: Continue, New Life (same world), New World, The Town
  (this world's record: lives, graves, got out, days; the cemetery list;
  Enter on a name reads the journal), Settings, Controls, Credits, Quit.
- New World asks, on one screen: mode (The way out / Endless), threat mode
  (with the rewritten lines of P04), tone, Living World / Fresh Start
  (P10), and an optional seed (digits; blank is random). Three save slots
  stay.
- New Life goes to the character screen (P05).
- The town behind the menu stays as the live preview.

### 4. Settings

A settings file `settings.json` next to the saves (not per slot), loaded at
start; a Settings panel reachable from the title and the pause menu, in the
bare-hands style (rows with the value at the right; Left/Right changes;
the chosen row inverted).

| Group | Settings |
|---|---|
| Sound | Master, Effects, Ambience, Music (0 to 10 each) |
| Screen | Fullscreen (off, borderless, exclusive); Resolution (windowed sizes: 1280x720, 1600x900, 1920x1080, native); VSync; Interface size (1x, 1.5x, 2x: scales all UI text and panels by whole or half steps; default by window height: 1x up to 899, 1.5x up to 1299, 2x above); Screen effects (full, reduced, off); Screen shake (0 to 10) |
| Game | Auto-pause when the window loses focus; Hold or toggle for crouch and for run; Show teaching prompts; Day length (24, 36 or 48 real minutes per game day, set per world at creation and shown here read-only) |
| Keys | Every action listed with its key; Enter then a key rebinds; conflicts are swapped and said so; "Reset keys". Mouse buttons can be bound. Stored in the settings file |

- `Input` gets an action layer: code asks `input.pressed(Action::Shove)`,
  never a raw scancode. Replace every raw key check outside `Input` (grep
  for `SDL_SCANCODE_`). Prompts and the Controls panel print the bound key's
  name, so "E" in every message becomes the binding.
- Day length: all rates are "per real second" today with 60 s per game hour.
  Add `kGameHourSeconds` as a per-world value (60, 90, 120) and make the
  clock advance use it; needs and other per-game-hour rules must be driven
  from game time, not real time. Audit: anything written "per second" that
  means "per game minute" (needs decay, wound timers, spoilage, depression)
  scales with it; anything that is real-time feel (combat, movement, noise
  durations, stamina regen) does not. List both groups in a comment block in
  `Tuning.h`. Default stays 24 minutes.
- Gamepad: not in this plan.

### 5. Pause, saving and quitting

- Saving: the game checkpoints on sleep (exists), and now also at each dawn,
  on entering your base, and on quitting to the title. "Save" stays in the
  pause menu. One checkpoint per world; death deletes it (exists).
- Pause menu: Resume, Settings, Controls, Stay here for good (P10, when
  allowed), Save and quit to title, Quit to desktop (asks once).
- The pause panel's right side keeps the run's facts (mode, tone, day,
  traits, kills) as today.
- A crash-safe write is already used for most files; make the checkpoint,
  memory and survivors files use `writeJsonAtomic` like the rest.

### 6. Accessibility and comfort

- Interface size (above). The 5x7 font at 1x is small on large screens.
- Colour: amber and red are never the only signal: critical conditions
  also pulse and carry a mark ("!") before the word.
- Screen shake and effects settings (above).
- Hold-to-act actions (search, board, treat) accept "press once" mode: one
  press starts, any move cancels.
- Text speed is not a thing; messages stay 3 s or until replaced; the last
  10 messages can be read in the notebook's Journal page footer.

### 7. The demo build

- `tools/package.py`: builds Release, copies `dead_pixel_survival.exe`,
  `assets/`, the needed UCRT64 DLLs (found with `ldd` on the exe, filtered
  to those under `/ucrt64/bin`), `README.txt` (controls and one paragraph)
  and `LICENSE` into `dist/DeadPixelSurvival-<version>/`, and zips it.
  Saves go to `%APPDATA%/DeadPixelSurvival/` in packaged builds (a
  `portable.txt` file next to the exe keeps them local; the dev build keeps
  `save/` in the working directory so the harness is unchanged).
- Version string in one header (`src/core/Version.h`), shown small on the
  title screen's bottom-right, written into saves.
- Window icon and exe icon (generated 32 and 256 px from the item-style
  generator: a boarded door).
- A demo flag (`--demo`, or a `demo.txt` beside the exe): a life is capped
  at day 7 at dawn with a card "This is the end of the demo's week. The town
  will remember <Name>." then the normal retirement path, so demo lives
  still feed the legacy. Everything else is the full game.
- CI: add the packaging step as an artifact on the existing workflow (no
  release, no upload elsewhere).
- Test on a clean Windows user profile if one is available; otherwise check
  the zip runs from a fresh folder with PATH stripped to System32
  (`cmd /c "set PATH=C:\Windows\System32 && dead_pixel_survival.exe --max-ticks 600 --hidden"`).

## Later

- Gamepad.
- Languages (all strings are already short; they would need moving to a
  table first).
- Steam.

## Files

New: `src/world/Aims.*`, `src/core/Settings.*`, `src/core/GameSettings.cpp`
(panel), `src/core/Version.h`, `assets/data/day_cards.json`,
`assets/data/death_hints.json`, `tools/package.py`, `tests/test_aims.cpp`,
`tests/test_settings.cpp`, `tests/test_bindings.cpp`,
`tools/harness/aims.txt`, `settings.txt`, `first_run.txt`.
Changed: `Input.*`, `InputSystem`, every raw key check, `FirstDays`,
`GameFirstDays.cpp`, `GameMenus.cpp`, `GameUi.cpp` (title, pause, controls,
run-end hint, UI scale through one `ui()` scale factor), `Hud` (scale),
`SaveManager` (atomic writes, per-world day length, version), `Autoplayer`,
`CMakeLists.txt`, CI workflow, README.

## Acceptance

- `aims.txt`: on day 4 with no collector the aim is "Water that refills";
  building one changes the aim at the next dawn; a horde day shows
  "Tonight"; screenshots of four aims fit the top-right at 1x and 2x.
- `first_run.txt`: a fresh world: the look-around step, the push and shove
  prompts appear once and never again after use (second life: none).
- `settings.txt`: rebinding Shove to F and reading the Controls panel shows
  F; the conflict swap works; settings survive a restart; Interface size 2x
  screenshots of HUD, pack, make, notebook, title, settings: nothing
  overlaps or leaves the window at 1920x1080.
- No `SDL_SCANCODE_` outside `Input.cpp` and the binding table.
- Day length 48: a scripted hour of game time takes twice the ticks; hunger
  lost per game day is unchanged (unit test on the rate helpers).
- `tools/package.py` produces a zip that starts from a clean folder; `--demo`
  ends a life at day 7 with the card and leaves a retired survivor.
- All harness scripts still pass with default bindings.
