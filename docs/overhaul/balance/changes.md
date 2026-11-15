# Balance changes (phase 14)

Every change made in phase 14, in the order made, with the target it was
aimed at, what changed (old to new), what it did and its side effects.
Three kinds, kept apart so each number has its own reason: the instrument
(how the runs are measured), the bot (how it plays, within the player's
own keys and sight), and the game's numbers (`assets/data/tuning.json`).

Soaks are `tools/soak.py` with the `normal` bot, Overwhelming Swarm, Grim
Human, 24-minute days, the first life of a world. Screens ran seeds 1-40;
"median" is the median day of death, and the bootstrap range printed by the
soak says how far a median of that many seeds moves by chance (about
plus or minus 0.3 days at 40 seeds).

## The instrument

| Date | What | Why | Effect |
|---|---|---|---|
| 2026-11-15 | Traced phase 13's moved seeds (2, 5, 6, 7) | Lead 7: is the instrument sound? | Phase 12's and 13's builds play identically until night 1's first callers set out; the change is night 1's weak first caller (30 health), not the bot's aims (day 4 on) or the single presses (those would differ from the first hour). Runs are deterministic per seed. |
| 2026-11-15 | The dead's own random stream keyed by where and what they rose as, not by entity id (`EntityFactory::spawnZombie`) | Lead 7 and the brief's (b): one more crate or car shifted every later arrival and moved whole runs | A town that makes one more entity no longer reshuffles the dead. Re-baselined (table below). |
| 2026-11-15 | Death causes name what took the last health: a blow in the last half hour is the striker's (the dead, or a person: "Killed by Hana", "Bled out after Hana's blow"); else bleeding, thirst, hunger, cold ("Froze"), heat ("Died of heat"), an infected wound, illness ("Died of illness") | Before, a survivor's knife followed by bleeding read "Bled out", and a death from cold read "Killed by zombies" | Causes can be counted by class. |
| 2026-11-15 | `tools/soak.py`: 40 seeds by default, median with quartiles and a bootstrap range, day-1 and night-1 deaths, causes by class, kills per day, first fight, quiet hours, grabs, sleep, boards at dusk, drink at dawn of day 3, depression on day 7, panic; `--profile`, `--mode`, `--tone`, `--days`, `--lives`, `--tune`, `--compare`, `--duel`, `--from-metrics`; runs that did not finish clean are left out of the numbers | Ten seeds could not show a change under the spread | One soak line per measure the phase asks for. |
| 2026-11-15 | Harness metrics gained `first_fight`, `first_wound`, `quiet_min`, `sampled_min`, `grabs`, `far_tiles`, `stocked_home`, `boxes_home`, `stocked_town`, `boxes_town`; `bot PROFILE DAYS SECONDS` stops the bot after SECONDS | The quiet-hours, first-fight, grab and town-runs-out targets; duels of 30 s | |
| 2026-11-15 | `tools/soak_report.py` | The phase's report | `report_*.md` in this folder. |
| 2026-11-15 | Three duel scripts (`duel_one`, `duel_three`, `duel_brute`) | The fight targets | First with the dead three tiles off: on 10 of 30 seeds a bot not yet seen walked into a house to search and they never met. Now two tiles off, and the soak counts only duels that met (one of the dead within 2 tiles). Results in the final report. |

Sleep was first counted from hourly rows, which thin out while asleep (the
clock runs fast), so a night slept looked like a night missing; it is now
summed between rows.

## The bot (`src/dev/Autoplayer.cpp`)

| What it did | What it does now | Effect (seeds 1-40) |
|---|---|---|
| Stood a pixel out of reach of most windows and never boarded (boards 0 in every life) | Leans in until the prompt offers R, then boards; gives a window up after 1.5 s | Boards at dusk on day 1: 0 to 4; most boards up 0 to 5 |
| Went to bed before boarding | Boards first, then bed | |
| Never broke furniture for planks | With no planks, nails and a hammer, breaks a movable piece of the home's furniture (not the bed) for planks | |
| Stood 3 game hours pressed against a parked pickup the routes do not know about (seed 1), or against a truck bed it could not open (seed 43) | After half an hour on one tile at a goal it gives the goal and everything in reach up and steps off a random way; a car in the way is stepped back from | Seeds 1 and 43 play on |
| Left the home's door standing open, so the house counted as open and the dead in the street woke it all night | Shuts an open door of the home in the evening, boards or none | Night-1 deaths 18 % to 10 % |
| Ran from one attacking person until it died tired | Stands and fights one attacking person when at half health or better and armed; still runs from two, or when hurt | |

bot2 (the first four rows) against the instrument build, 60 seeds: median
1.44 to 1.35 days (inside the spread), boards 0 to 5. bot3 (all six)
against bot2, seeds 1-40: median 1.28 to 1.42, night-1 deaths 18 % to 10 %,
nights slept 6 hours 8 % to 13 %.

## The game's numbers

Each row is a screen against the row it builds on, seeds 1-40. Rows 1 to
5 were screened on the bot2 build (median 1.28 on these seeds, day-1
deaths 18 %, night-1 18 %, kills 17 a day, quiet hours 31 %, people 15 %);
rows 6 on, on bot3 (median 1.42, day-1 20 %, night-1 10 %, kills 17.9,
quiet 29 %).

| # | Target | Hypothesis | Change (old to new) | Effect | Kept |
|---|---|---|---|---|---|
| 1 | Day 1 under 5 %; quiet first; kills 1-4 | The director's near quota keeps one of the dead within 14 tiles of the player by day from the first hour and three at night from night 1, so the dead come to the player wherever they go; the fights it feeds are what kill on day 1 | `threat.near.day` 1 to 0, `threat.near.night` 3 to 1, `threat.near.nightGrowth` 1 to 0.5 (`dayGrowth` stays 0.5): none sent by day on day 1, one more each two days | Median 1.28 to 1.74, kills 17 to 13.1, quiet 31 to 40 %; deaths at people's hands 15 to 33 % (lives last into the garage's rounds) | yes |
| 2 | Day 1 under 5 % | The migration starts 150 s (two and a half game hours) into day 1, while the player is still searching the home | `threat.grace.seconds` 150 to 600 (ten game hours: the first migrants arrive at dusk) | Median 1.28 to 1.76, day-1 deaths 18 to 8 % | yes |
| 3 | Day 1 under 5 % | Lead 1: 7 a bite against health back at about 1 an hour | `zombie.attack.damage` 7 to 5 | Median 1.32, day-1 18 %, night-1 31 %: inside the spread; the bot is overrun, not worn down | no |
| 4 | People 2-8 % | Lead 4: a pistol at 55 a shot kills in two | `survivors.shot.toPlayer` and `survivors.blow.toPlayer` 1 to 0.5 (what a person's shot or blow does to the player) | People deaths 6 to 6 of 40: the same lives die a few minutes later, to the same person | no |
| 5 | Night 1 at home | The bot is woken by the dead 4 tiles off | `sleep.threat.tiles` 4 to 2 | No change: the waking was the 12-tile range of a house with a door open (found by tracing; fixed in the bot, row 5 of the bot's table) | no |
| 6 | Day 1, quiet first, kills | Rows 1 and 2 together, on bot3 | as rows 1 and 2 | Median 1.42 to 2.06, day-1 20 to 5 %, night-1 10 to 12 %, kills 17.9 to 11.4, quiet 29 to 40 %, nights slept 13 to 22 %, people 18 to 22 % | yes |
| 7 | Night 1; days 2-3; kills | The dead drift toward the player every 70 s by day and 35 s at night; the home fills faster than a bot (or a person) can fight it down | `threat.migration.dayInterval` 70 to 140, `threat.migration.nightInterval` 35 to 70 | Median 2.06 to 2.46, night-1 12 to 2 % (at home 0), kills 11.4 to 10.9, quiet 40 to 41 % | yes |
| 8 | Days 2-3 | The town doubles in two or three days in Swarm | `threat.growth.daily` 0.18 to 0.09 (on row 6) | Median 2.06 to 2.09: no effect inside a three-day life | no |
| 9 | People 2-8 % | As row 4, harder | `survivors.shot.toPlayer` 1 to 0.3 (on row 6) | People deaths 9 to 9: the same seeds | no |
| 10 | Kills 1-4, quiet first | Fewer of the dead in town altogether | `zombie.population.scale` 1 to 0.8 (on row 6) | Median 2.06 to 2.13, quartiles 1.74-3.57; day-1 5 to 12 %, people 22 to 28 %, kills 11.4 to 9.8, quiet 40 to 45 %; single seeds swing by days both ways | see row 12 |
| 11 | Day 1; days 2-3 | Lead 1: health back too slowly between fights | `health.recovery.perSecond` 0.02 to 0.05 (on row 6) | Median 2.06 to 2.12, kills 11.4 to 12.2: no effect | no |
| 12 | Kills 1-4, quiet first | Row 10 on row 7 | `zombie.population.scale` 1 to 0.8 (on row 7) | Median 2.46 to 2.94 (out of the 2.11-2.76 the first could move by chance), mean 2.45 to 2.78; kills 10.9 to 9.1, quiet 41 to 46 %, infection 5 to 12 %, people 18 to 15 %, night-1 2 to 0 %; day-1 5 to 10 % (seeds 10 and 15: one of the dead early, Tomas at the start); 26 seeds longer, 14 shorter | yes |
| 13 | Lead 6: a crowd breaks a car in 11 s | 3 a blow from each of the dead round a car | `vehicle.batter.damage` 3 to 1.5 | In `vehicles.txt` the parked sedan the drive's tail batters for a game hour ends at 24 (Damaged) instead of 0 (Broken); the script passes unchanged. Driven into 300 in `perf` it still ends Broken within ten seconds at either value (ramming the crowd does most of it) | yes |

The kills target (1 to 4 a day) was worked three ways (rows 1, 7 and 10/12)
and moved from 17.9 to 9.1; quiet hours (days 1 to 3) from 29 to 46 %.
Why they stand is in `DECISIONS.md` ("Balance: needs a design change").
Tried and put back: the dead's bite (row 3), people's shots (rows 4, 9),
the bed's threat range (row 5), the town's growth (row 8) and health
coming back (row 11).

The numbers as they stand are in `assets/data/tuning.json`: the grace,
the near quota, the migration intervals, the population scale and the
car's battering are phase 14's; the rest are as phase 13 left them.
