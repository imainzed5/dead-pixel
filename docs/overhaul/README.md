# Dead Pixel Survival: the content overhaul

Written 2026-11-13. This folder is the complete plan for the content overhaul:
gameplay, feel, simulation and the systems underneath. It is written so that
an implementer (Claude Opus 5.5) can carry it out from start to finish
**without asking the user anything**. Every design decision is already made
here. Where something is left open, `00_ground_rules.md` says how to decide.

## How to use this folder

1. Read `00_ground_rules.md` in full. It holds the working rules, the build
   and test commands, the commit policy and the rule for deciding things this
   plan does not cover.
2. Read `01_vision.md` once. It says what the game should feel like, and every
   phase is judged against it.
3. Skim `02_current_state.md`. It lists what exists today and the defects
   found while planning.
4. Keep `03_content_tables.md` open while working. It is the one source of
   truth for item ids, recipes, loot tables, zombie kinds, skills and traits.
5. Do the phases in order. Each phase file is self-contained: goal, rules with
   numbers, data, code changes, tests, harness script, acceptance checks.
6. After each phase: build, run the tests, run the phase's harness script,
   look at the screenshots, update the status table below, add a line to the
   log in `docs/revival_plan.md`, and commit.

## Decisions the user made (2026-11-13)

- **Scope:** everything, in phases. Every system gets deepened; the game stays
  playable after each phase.
- **Campaign goal:** get out of town. Clues reveal a way out that needs parts
  and preparation, and progress carries across lives.
- **Commits:** one local commit per finished phase on a new branch
  (`overhaul`, cut from `art-and-feel`). Never push. No `Co-Authored-By`
  trailer and no AI attribution of any kind.

Earlier decisions that still stand (from `docs/revival_plan.md` and the
project memory): ship-quality goal; the hook is the fixed town that remembers;
isometric camera stays; all art stays code-drawn at today's pixel scale;
sounds stay generated; the interface follows the "bare hands" style; home base
comes before fighting; no engine switch.

## Phases

| # | File | What the player gets | Status |
|---|---|---|---|
| 1 | `phases/P01_foundations.md` | Bug fixes, content in data files, storage in cupboards, a bot that plays the game for balance runs | done |
| 2 | `phases/P02_home_base.md` | A home worth defending: furniture as material, windows, traps, stations, a base the next life inherits | done |
| 3 | `phases/P03_fighting.md` | Shove, knockdown, grabs by the dead, weapon kinds that differ, throwables, guns that cost you | done |
| 4 | `phases/P04_the_dead.md` | Five kinds of dead, senses (sight, sound, blood), hordes, threat modes that really differ | done |
| 5 | `phases/P05_body_and_mind.md` | Skills, chosen traits, deeper wounds and illness, sleep debt and a mind that shows | done |
| 6 | `phases/P06_scavenging.md` | Buildings with a purpose, locks and windows, power and water running out, cooking, foraging, growing food | done |
| 7 | `phases/P07_weather_and_fire.md` | Weather and seasons that change the rules; fire that follows them | done |
| 8 | `phases/P08_survivors.md` | Other people with needs, homes and tempers: trade, travel together, get robbed | done |
| 9 | `phases/P09_the_way_out.md` | The campaign: clues, the gate, a last stand, and getting out | done |
| 10 | `phases/P10_legacy.md` | Journals, what a bloodline learns, a town that ages between lives | done |
| 11 | `phases/P11_vehicles.md` | Three vehicles with trunks, hot-wiring and wear | not started |
| 12 | `phases/P12_sound_and_mood.md` | A sound for everything that makes noise, ambience, sparse music, mood on screen | not started |
| 13 | `phases/P13_run_flow.md` | Life after day 3, settings, key bindings, first-run teaching, demo build | not started |
| 14 | `phases/P14_balance.md` | Numbers tuned by bot runs against written targets | not started |

Order matters: later phases use what earlier ones build (P01's data files and
bot, P02's base, P03's combat verbs, P05's skills). Do not reorder. A phase may
be left at its boundary at any time; the game is whole after each one.

## Size

Each phase is roughly one to three days of focused implementation. If a phase
turns out much larger than that, finish its "Core" list, commit, and record
what was moved to "Later" in `DECISIONS.md`. Never leave a phase half-wired.

## Files

- `00_ground_rules.md`: how to work.
- `01_vision.md`: what it should feel like.
- `02_current_state.md`: what exists and what is broken.
- `03_content_tables.md`: ids and content, authoritative.
- `04_keys.md`: what every key does after each phase, authoritative.
- `DECISIONS.md`: a running log. Add an entry whenever you decide something
  this plan did not settle, or move something to "Later".
- `phases/`: one file per phase.
