# Phase 14: balance

**Goal.** Numbers that come from runs. Use the bot to play hundreds of lives,
compare against written targets, change one thing at a time, and write down
what was changed and why.

This phase adds no features. It ends when the targets hold, or when a target
has been tried three ways and the reason it cannot hold is written down.

## Tools (built in phase 1, extended along the way)

- `tools/soak.py`: runs `bot_week.txt` (or a named script) over seeds, in
  fresh directories, in parallel (3 at a time), and prints a table and
  writes `soak_<date>.csv`.
- Extend it here with: `--profile careful|normal|bold`, `--mode` (threat
  mode), `--tone`, `--days N`, `--lives N` (play N lives in one world to
  measure legacy effects), `--tune key=value` (passes `tune` commands), and
  `--compare old.csv` (prints the difference per metric).
- Metrics per life: days survived; cause of death; kills; day of first
  wound; times grabbed and times freed; food and water held at each dawn;
  containers searched per day; boards up at each dusk; callers per night and
  how many got in; noise events per game hour; furthest distance from home;
  skill levels at death; depression and panic maxima; per life in a world:
  where the previous base stood and whether it was reused.
- A `tools/soak_report.py` that turns a CSV into `docs/overhaul/balance/
  report_<date>.md` with small text tables (no charts needed).

## Targets

"Bot" means the `normal` profile unless stated. The bot is a weak, steady
player: a careful human does better. Targets are for Grim Human tone,
Overwhelming Swarm mode, 24-minute days, 40 seeds, first life of a world.

### Survival curve

| Measure | Target |
|---|---|
| Lives ending on day 1 | under 5 % |
| Median day of death, bot `normal` | 5 to 8 |
| Median, `careful` | 7 to 12 |
| Median, `bold` | 3 to 6 |
| Lives reaching day 15, `careful` | 10 to 25 % |
| Tenth life in the same world vs first (median days) | +30 % or more (legacy must help) |

### Causes of death (share of lives, `normal`)

| Cause | Target |
|---|---|
| The dead (claws, bites while held) | 45 to 60 % |
| Infection and illness | 10 to 20 % |
| Thirst | 5 to 12 % |
| Hunger | 3 to 10 % |
| Cold or heat | 2 to 8 % (higher in winter runs: up to 25 %) |
| Fire | under 5 % |
| Other people (after P08) | 2 to 8 %; Infected Society 20 to 35 % |
| Any single cause | never above 65 % |

### Pressure

| Measure | Target |
|---|---|
| Night 1: callers that get into a house with all doors boarded once | 0 |
| Night 1: a house with nothing boarded | the player is woken; death in under 25 % |
| Night 3: callers get in when day 2's and day 3's plans were done | under 30 % of nights |
| First fight (a zombie within 2 tiles), day 1, `normal` | in 60 to 90 % of lives |
| Game hours per day with no zombie within 15 tiles (days 1 to 3) | 60 to 80 % (quiet first) |
| Same, days 8 to 14 | 35 to 55 % |
| Zombies killed per day, `normal` | 1 to 4 (fighting is a tax) |
| A lone Shambler vs a bot that only shoves and swings a bat | bot wins over 90 %, loses under 15 health on median |
| Three Shamblers at once vs the same | bot wins 40 to 60 % |
| A Brute alone vs the same | bot wins 30 to 50 % |

### Supplies

| Measure | Target |
|---|---|
| Days of food found per day of searching, days 1 to 5 | 1.2 to 2.0 |
| Same, days 10 to 15 in the home district | under 0.6 (the home district runs out) |
| Water held at dawn of day 3 (bottles), plan followed | 4 to 7 |
| Planks available by dusk of day 2, plan followed (found + broken furniture) | 6 or more |
| Bandages used per life | 2 to 6 |
| Stocked containers left in the home district after 5 lives | under 35 % |
| Stocked containers left in the whole town after 20 lives | 25 to 50 % (a save lasts) |

### Mind and body

| Measure | Target |
|---|---|
| Lives ending in retirement by crisis, bot | 1 to 5 % |
| Depression at day 7, alone, needs met | 35 to 55 |
| Same, with a companion from day 4 | under 25 |
| Sleep: nights with 6+ hours, plan followed | over 70 % |
| Panic above 75 for more than 5 % of waking time | under 15 % of lives |

### Modes and tones

Each threat mode must differ from Swarm on at least two measures by a clear
margin (write the two per mode in the report). Hopeless: median days x0.7
to x0.85 of Grim Human. Brutal Arcade: kills per day x2 or more, median
days within +-20 %.

### Campaign

| Measure | Target |
|---|---|
| Lives to open the gate, scripted "competent" bot that knows the leads (a special profile `gate` that works the chain) | 4 to 9 |
| Last stand survived with the hut boarded, 2 alarms, a companion, a gun | 50 to 70 % |
| Last stand with nothing prepared | under 10 % |

### Performance

Day 15, 250+ zombies, 12 survivors, rain, a 20-tile fire: average tick under
4 ms, worst under 16 ms, in Release. A 15-day life in the hidden harness in
under 6 minutes.

## Method

1. Baseline: run the full matrix once; commit the CSVs and the report under
   `docs/overhaul/balance/`.
2. Take the target furthest off. Form one explanation. Change one group of
   numbers in `assets/data/tuning.json` (or the data file that owns it).
   Rerun only the affected slice (20 seeds). Keep or revert.
3. Log each change in `docs/overhaul/balance/changes.md`: date, target,
   what was changed (old to new), effect on the target, side effects.
4. Never tune by changing a rule's shape here. If a target cannot be reached
   with numbers, write it in `DECISIONS.md` under "Balance: needs a design
   change" with what you saw, and leave the numbers at the best found.
5. Check the feel after the numbers: play the harness scripts
   `first_day`, `night_siege`, `fighting`, `horde` with `--watch` or read
   their screenshots. A number that hits the target and reads badly on
   screen (callers arriving in a clump, a fight that lasts 40 s) is wrong.
6. Finish with the full matrix again and a final report.

## Order of attack

1. Day 1 to 3 survival and night callers (the first three days must hold).
2. Single-zombie and three-zombie fights (grab odds, shove distance,
   stamina costs).
3. Supplies per day and the home district running out.
4. Kinds' shares by day; hordes; days 7 to 15.
5. Needs, cold and heat by season (run `--days 60` with a `careful` bot and
   season starts forced by harness).
6. Mind.
7. Modes and tones.
8. Survivors and the campaign.
9. Performance.

## What the bot cannot tell you

The bot does not feel dread, boredom or unfairness. After the numbers hold,
write a short list in the final report, "For a person to check", of the
things only play can settle (is night 1 frightening, is day 4 dull, do
grabs feel fair, is the gate stand too long), for the user's own playtest.
Do not guess at them.

## Acceptance

- `docs/overhaul/balance/` holds: baseline and final CSVs and reports, and
  `changes.md`.
- Every target above is met in the final report, or listed with the three
  attempts made and why it stands.
- All unit tests and harness scripts pass with the final numbers (update
  scripts whose expectations were numeric).
- The README's "What is in it" section and `docs/revival_plan.md` are
  brought up to date for the whole overhaul, and the status table in
  `docs/overhaul/README.md` reads "done" for every phase.
