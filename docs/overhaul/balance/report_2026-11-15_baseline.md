# Baseline (phase 14, before any game number changed)

Written by `tools/soak_report.py` on 2026-11-15 from `baseline_p13.csv`, `baseline_instrument.csv`, `baseline_bot2.csv`, `baseline_bot3.csv`.
Grim Human tone, Overwhelming Swarm, 24-minute days and the first life of a world unless a column says otherwise.

| Measure | Target | phase 13 | instrument | bot2 | bot3 (1-40) |
|---|---|---:|---:|---:|---:|
| Lives (clean runs) | 40 | 60 | 57 | 56 | 40 |
| Median days survived | normal 2-4, careful 3-6, bold 1.5-3 (phase file: 5-8, 7-12, 3-6; see DECISIONS) | 1.24 | 1.44 | 1.35 | 1.42 |
| First quartile |  | 0.82 | 0.92 | 1.01 | 1.04 |
| Third quartile |  | 1.91 | 2.08 | 2.00 | 2.02 |
| Mean days |  | 1.40 | 1.60 | 1.52 | 1.56 |
| Lives ending on day 1 | under 5 % | 15 % | 16 % | 16 % | 20 % |
| Died on night 1 |  | 17 % | 19 % | 16 % | 10 % |
| Reached day 7 | careful 5-20 % | 0 % | 0 % | 0 % | 0 % |
| Reached day 15 | tenth life (phase file: careful 10-25 %) | 0 % | 0 % | 0 % | 0 % |
| Cause: the dead | 45-60 % | 91 % | 72 % | 84 % | 75 % |
| Cause: infection and illness | 10-20 % | 0 % | 7 % | 5 % | 5 % |
| Cause: thirst | 5-12 % | 0 % | 0 % | 0 % | 0 % |
| Cause: hunger | 3-10 % | 0 % | 0 % | 0 % | 0 % |
| Cause: cold | 2-8 % with heat | 0 % | 0 % | 0 % | 0 % |
| Cause: heat |  | 0 % | 0 % | 0 % | 0 % |
| Cause: fire | under 5 % | 0 % | 0 % | 0 % | 0 % |
| Cause: other people | 2-8 % (Society 20-35 %) | 9 % | 21 % | 11 % | 18 % |
| Retired (crisis) | 1-5 % | 0 % | 0 % | 0 % | 0 % |
| Kills per day (median) | 1-4 | 16.0 | 16.9 | 18.1 | 17.9 |
| First fight on day 1 | 60-90 % | - | 95 % | 93 % | 90 % |
| Quiet hours, days 1-3 | 60-80 % | - | 31 % | 29 % | 29 % |
| Quiet hours, days 8-14 | 35-55 % | - | - | - | - |
| Grabs per day (median) |  | - | 2.9 | 3.0 | 3.4 |
| Boards up (most, median) |  | 0.0 | 0.0 | 5.0 | 5.0 |
| Containers searched (median) |  | 43.5 | 38.0 | 35.5 | 36.5 |
| Drinks carried at dawn of day 3 | 4-7 | - | - | 8 | 6 |
| Nights slept 6 hours or more | over 70 % | - | - | 11 % | 13 % |
| Depression on day 7 (median) | 35-55 alone | - | - | - | - |
| Lives panicked over 5 % of waking time | under 15 % | - | - | 82 % | 75 % |

**phase 13** (`baseline_p13.csv`):

    lives 60: median days 1.24 (quartiles 0.82 to 1.91; a median of 60 seeds moves 1.08 to 1.57 by chance), mean 1.40
    ended on day 1: 9 of 60 (15 %); died on night 1: 10 of 60 (17 %) (at home 4); alive at the end: 2
    causes (of 58 that ended): dead 53 (91 %), people 5 (9 %)
    median kills per day 16.0, boards (most up) 0.0, searched 43.5
    reached day 7: 0 of 60 (0 %), day 15: 0 of 60 (0 %); drink carried at dawn of day 3 (median): -; depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: -

**instrument** (`baseline_instrument.csv`):

    lives 57: median days 1.44 (quartiles 0.92 to 2.08; a median of 57 seeds moves 1.25 to 1.75 by chance), mean 1.60
    ended on day 1: 9 of 57 (16 %); died on night 1: 11 of 57 (19 %) (at home 3); alive at the end: 0
    causes (of 57 that ended): dead 41 (72 %), people 12 (21 %), infection 4 (7 %)
    median kills per day 16.9, boards (most up) 0.0, searched 38.0
    first fight on day 1: 95 % of lives; quiet hours (none within 15 tiles) days 1-3: 31 %, days 8-14: -; grabs per day (median) 2.9
    reached day 7: 0 of 57 (0 %), day 15: 0 of 57 (0 %); drink carried at dawn of day 3 (median): -; depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: -

**bot2** (`baseline_bot2.csv`):

    lives 56: median days 1.35 (quartiles 1.01 to 2.00; a median of 56 seeds moves 1.25 to 1.71 by chance), mean 1.52
    ended on day 1: 9 of 56 (16 %); died on night 1: 9 of 56 (16 %) (at home 4); alive at the end: 0
    causes (of 56 that ended): dead 47 (84 %), people 6 (11 %), infection 3 (5 %)
    median kills per day 18.1, boards (most up) 5.0, searched 35.5
    first fight on day 1: 93 % of lives; quiet hours (none within 15 tiles) days 1-3: 29 %, days 8-14: -; grabs per day (median) 3.0
    nights slept 6 hours or more: 11 % of 55 whole nights (median 0 hours)
    reached day 7: 0 of 56 (0 %), day 15: 0 of 56 (0 %); drink carried at dawn of day 3 (median): 8 (15); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 82 %

**bot3 (1-40)** (`baseline_bot3.csv`):

    lives 40: median days 1.42 (quartiles 1.04 to 2.02; a median of 40 seeds moves 1.25 to 1.85 by chance), mean 1.56
    ended on day 1: 8 of 40 (20 %); died on night 1: 4 of 40 (10 %) (at home 2); alive at the end: 0
    causes (of 40 that ended): dead 30 (75 %), people 7 (18 %), infection 2 (5 %), other 1 (2 %)
    median kills per day 17.9, boards (most up) 5.0, searched 36.5
    first fight on day 1: 90 % of lives; quiet hours (none within 15 tiles) days 1-3: 29 %, days 8-14: -; grabs per day (median) 3.4
    nights slept 6 hours or more: 13 % of 38 whole nights (median 0 hours)
    reached day 7: 0 of 40 (0 %), day 15: 0 of 40 (0 %); drink carried at dawn of day 3 (median): 6 (13); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 75 %

Columns: phase 13's build as committed (60 seeds); the instrument build (the dead's own random stream, death causes; 60 seeds); bot2, the first four bot fixes (60 seeds, 56 clean: 56, 58 and 60 were stopped by hand); bot3, all six bot fixes (seeds 1-40). The game's numbers are phase 13's in all four. phase 13's causes predate the cause attribution (a death from cold or a person's blow could read as the dead). Quiet hours, the first fight, grabs and sleep were not recorded before the instrument build.
Duels on bot3 (12 seeds, an early check): one Shambler won 12 of 12 with no health lost; three Shamblers won 5 of 12 and fled the rest, none died; a Brute won 9 of 12, 0 to 14 health lost.
