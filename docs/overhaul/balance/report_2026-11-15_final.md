# Final (phase 14): profiles

Written by `tools/soak_report.py` on 2026-11-15 from `final_normal.csv`, `final_careful.csv`, `final_bold.csv`, `final_before_population.csv`.
Grim Human tone, Overwhelming Swarm, 24-minute days and the first life of a world unless a column says otherwise.

| Measure | Target | normal | careful (1-30) | bold (1-30) | before pop. 0.8 |
|---|---|---:|---:|---:|---:|
| Lives (clean runs) | 40 | 40 | 29 | 29 | 40 |
| Median days survived | normal 2-4, careful 3-6, bold 1.5-3 (phase file: 5-8, 7-12, 3-6; see DECISIONS) | 2.94 | 2.63 | 2.50 | 2.46 |
| First quartile |  | 1.96 | 1.89 | 1.92 | 1.84 |
| Third quartile |  | 3.88 | 3.69 | 3.30 | 3.13 |
| Mean days |  | 2.78 | 2.74 | 2.62 | 2.45 |
| Lives ending on day 1 | under 5 % | 10 % | 10 % | 3 % | 5 % |
| Died on night 1 |  | 0 % | 3 % | 3 % | 2 % |
| Reached day 7 | careful 5-20 % | 0 % | 0 % | 0 % | 0 % |
| Reached day 15 | tenth life (phase file: careful 10-25 %) | 0 % | 0 % | 0 % | 0 % |
| Cause: the dead | 45-60 % | 72 % | 62 % | 76 % | 78 % |
| Cause: infection and illness | 10-20 % | 12 % | 7 % | 3 % | 5 % |
| Cause: thirst | 5-12 % | 0 % | 0 % | 0 % | 0 % |
| Cause: hunger | 3-10 % | 0 % | 0 % | 0 % | 0 % |
| Cause: cold | 2-8 % with heat | 0 % | 0 % | 0 % | 0 % |
| Cause: heat |  | 0 % | 0 % | 0 % | 0 % |
| Cause: fire | under 5 % | 0 % | 0 % | 0 % | 0 % |
| Cause: other people | 2-8 % (Society 20-35 %) | 15 % | 31 % | 21 % | 18 % |
| Retired (crisis) | 1-5 % | 0 % | 0 % | 0 % | 0 % |
| Kills per day (median) | 1-4 | 9.1 | 10.8 | 11.4 | 10.9 |
| First fight on day 1 | 60-90 % | 95 % | 93 % | 93 % | 85 % |
| Quiet hours, days 1-3 | 60-80 % | 46 % | 49 % | 49 % | 41 % |
| Quiet hours, days 8-14 | 35-55 % | - | - | - | - |
| Grabs per day (median) |  | 1.6 | 1.5 | 1.9 | 1.9 |
| Boards up (most, median) |  | 4.5 | 5.0 | 5.0 | 5.0 |
| Containers searched (median) |  | 50.0 | 50.0 | 62.0 | 52.5 |
| Drinks carried at dawn of day 3 | 4-7 | 6 | 6 | 6 | 6 |
| Nights slept 6 hours or more | over 70 % | 17 % | 21 % | 19 % | 23 % |
| Depression on day 7 (median) | 35-55 alone | - | - | - | - |
| Lives panicked over 5 % of waking time | under 15 % | 75 % | 83 % | 79 % | 75 % |

**normal** (`final_normal.csv`):

    lives 40: median days 2.94 (quartiles 1.96 to 3.88; a median of 40 seeds moves 2.36 to 3.29 by chance), mean 2.78
    ended on day 1: 4 of 40 (10 %); died on night 1: 0 of 40 (0 %) (at home 0); alive at the end: 0
    causes (of 40 that ended): dead 29 (72 %), people 6 (15 %), infection 5 (12 %)
    median kills per day 9.1, boards (most up) 4.5, searched 50.0
    first fight on day 1: 95 % of lives; quiet hours (none within 15 tiles) days 1-3: 46 %, days 8-14: -; grabs per day (median) 1.6
    nights slept 6 hours or more: 17 % of 90 whole nights (median 1 hours)
    reached day 7: 0 of 40 (0 %), day 15: 0 of 40 (0 %); drink carried at dawn of day 3 (median): 6 (30); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 75 %

**careful (1-30)** (`final_careful.csv`):

    lives 29: median days 2.63 (quartiles 1.89 to 3.69; a median of 29 seeds moves 2.14 to 3.05 by chance), mean 2.74
    ended on day 1: 3 of 29 (10 %); died on night 1: 1 of 29 (3 %) (at home 1); alive at the end: 0
    causes (of 29 that ended): dead 18 (62 %), people 9 (31 %), infection 2 (7 %)
    median kills per day 10.8, boards (most up) 5.0, searched 50.0
    first fight on day 1: 93 % of lives; quiet hours (none within 15 tiles) days 1-3: 49 %, days 8-14: -; grabs per day (median) 1.5
    nights slept 6 hours or more: 21 % of 62 whole nights (median 1 hours)
    reached day 7: 0 of 29 (0 %), day 15: 0 of 29 (0 %); drink carried at dawn of day 3 (median): 6 (21); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 83 %

**bold (1-30)** (`final_bold.csv`):

    lives 29: median days 2.50 (quartiles 1.92 to 3.30; a median of 29 seeds moves 2.08 to 3.13 by chance), mean 2.62
    ended on day 1: 1 of 29 (3 %); died on night 1: 1 of 29 (3 %) (at home 1); alive at the end: 0
    causes (of 29 that ended): dead 22 (76 %), people 6 (21 %), infection 1 (3 %)
    median kills per day 11.4, boards (most up) 5.0, searched 62.0
    first fight on day 1: 93 % of lives; quiet hours (none within 15 tiles) days 1-3: 49 %, days 8-14: -; grabs per day (median) 1.9
    nights slept 6 hours or more: 19 % of 59 whole nights (median 1 hours)
    reached day 7: 0 of 29 (0 %), day 15: 0 of 29 (0 %); drink carried at dawn of day 3 (median): 6 (22); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 79 %

**before pop. 0.8** (`final_before_population.csv`):

    lives 40: median days 2.46 (quartiles 1.84 to 3.13; a median of 40 seeds moves 2.11 to 2.76 by chance), mean 2.45
    ended on day 1: 2 of 40 (5 %); died on night 1: 1 of 40 (2 %) (at home 0); alive at the end: 0
    causes (of 40 that ended): dead 31 (78 %), people 7 (18 %), infection 2 (5 %)
    median kills per day 10.9, boards (most up) 5.0, searched 52.5
    first fight on day 1: 85 % of lives; quiet hours (none within 15 tiles) days 1-3: 41 %, days 8-14: -; grabs per day (median) 1.9
    nights slept 6 hours or more: 23 % of 75 whole nights (median 0 hours)
    reached day 7: 0 of 40 (0 %), day 15: 0 of 40 (0 %); drink carried at dawn of day 3 (median): 6 (29); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 75 %

Columns: the final numbers with the normal bot (seeds 1-40); careful and bold (seeds 1-30; on the same 30 seeds normal's median is 3.20 days, so careful 2.63 and bold 2.50 sit below it); and the same build with the town's population at 1 instead of 0.8 (the last row of changes.md before it). Two of 60 profile runs stood three game hours on one tile (careful seed 8 between a parked car and a pond, bold seed 26 at a search) and are left out.

Duels (30 seeds, the bot `bold` with a bat, 30 seconds, the dead starting two tiles off and heading for it; seed 4 is left out, its spawn tile is a wall): one Shambler: won 24 of 29 (83 %; target over 90 %), died 0, health lost when won: median 0 (target under 15); in the other five the bot walked off before a blow and the Shambler lost it. Three Shamblers: won 13 of 30 (43 %; target 40-60 %), died 0, the rest fled. A Brute: won 15 of 29 (52 %; target 30-50 %), died 0, the rest fled.

Not measured by the soak: night 1 callers by board state (the `first_day` and `night_siege` scripts hold them), night 3 callers, food found per day of searching, planks by dusk of day 2, bandages per life, depression on day 7 alone and with a companion (no normal life reaches day 7), anything on days 8 to 15, and the campaign's `gate` profile (never built).
