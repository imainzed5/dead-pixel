# Final (phase 14): ten lives in one world

Written by `tools/soak_report.py` on 2026-11-15 from `final_lives.csv`.
Grim Human tone, Overwhelming Swarm, 24-minute days and the first life of a world unless a column says otherwise.

| Measure | Target | ten lives |
|---|---|---:|
| Lives (clean runs) | 40 | 3 |
| Median days survived | normal 2-4, careful 3-6, bold 1.5-3 (phase file: 5-8, 7-12, 3-6; see DECISIONS) | 3.26 |
| First quartile |  | 3.10 |
| Third quartile |  | 3.77 |
| Mean days |  | 3.49 |
| Lives ending on day 1 | under 5 % | 0 % |
| Died on night 1 |  | 0 % |
| Reached day 7 | careful 5-20 % | 0 % |
| Reached day 15 | tenth life (phase file: careful 10-25 %) | 0 % |
| Cause: the dead | 45-60 % | 0 % |
| Cause: infection and illness | 10-20 % | 100 % |
| Cause: thirst | 5-12 % | 0 % |
| Cause: hunger | 3-10 % | 0 % |
| Cause: cold | 2-8 % with heat | 0 % |
| Cause: heat |  | 0 % |
| Cause: fire | under 5 % | 0 % |
| Cause: other people | 2-8 % (Society 20-35 %) | 0 % |
| Retired (crisis) | 1-5 % | 0 % |
| Kills per day (median) | 1-4 | 11.0 |
| First fight on day 1 | 60-90 % | 100 % |
| Quiet hours, days 1-3 | 60-80 % | 54 % |
| Quiet hours, days 8-14 | 35-55 % | - |
| Grabs per day (median) |  | 1.4 |
| Boards up (most, median) |  | 4.0 |
| Containers searched (median) |  | 55.0 |
| Drinks carried at dawn of day 3 | 4-7 | 6 |
| Nights slept 6 hours or more | over 70 % | 33 % |
| Depression on day 7 (median) | 35-55 alone | - |
| Lives panicked over 5 % of waking time | under 15 % | 67 % |

**ten lives** (`final_lives.csv`):

    lives 3: median days 3.26 (quartiles 3.10 to 3.77; a median of 3 seeds moves 2.93 to 4.29 by chance), mean 3.49
    ended on day 1: 0 of 3 (0 %); died on night 1: 0 of 3 (0 %) (at home 0); alive at the end: 0
    causes (of 3 that ended): infection 3 (100 %)
    median kills per day 11.0, boards (most up) 4.0, searched 55.0
    first fight on day 1: 100 % of lives; quiet hours (none within 15 tiles) days 1-3: 54 %, days 8-14: -; grabs per day (median) 1.4
    nights slept 6 hours or more: 33 % of 9 whole nights (median 3 hours)
    reached day 7: 0 of 3 (0 %), day 15: 0 of 3 (0 %); drink carried at dawn of day 3 (median): 6 (3); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 67 %
    by life in the world (median days, lives): 1: 3.26 (3), 2: 0.56 (3), 3: 2.28 (3), 4: 2.20 (3), 5: 1.97 (3), 6: 0.96 (3), 7: 3.07 (3), 8: 1.45 (3), 9: 2.81 (3), 10: 2.33 (3)
    containers holding anything, the life's home district (median share): start: 100 %, after 1: 40 %, after 2: 50 %, after 3: 50 %, after 4: 60 %, after 5: 50 %, after 6: 67 %, after 7: 57 %, after 8: 50 %, after 9: 50 %, after 10: 67 %
    containers holding anything, the town (median share): start: 50 %, after 1: 52 %, after 2: 50 %, after 3: 47 %, after 4: 47 %, after 5: 47 %, after 6: 46 %, after 7: 47 %, after 8: 44 %, after 9: 45 %, after 10: 43 %

Ten lives in each of five worlds (seeds 1-5), normal bot, Living World. Two worlds (seeds 3 and 4) had one life each that stood three game hours on one tile; the soak marks the whole world unclean, so the table above reads seeds 1, 2 and 5 only. Counting every life that ended in a death across all five worlds: life 1 median 3.64 days (5 lives), lives 8 to 10 median 2.11 (15 lives), life 10 median 2.33 (5 lives). Target: the tenth life +30 % or more over the first. Unmet: it is about 35 % shorter.

The dead at the start of each life rise from about 35 to about 55-60 by the tenth (seed 1: 32, 41, 39, 47, 53, 38, 52, 57, 60, 49), more than the town's ageing alone gives (4 % more a life, capped at 60 %); the risen and the districts that turn between lives are the likely rest (not traced). Whatever a life leaves for the next does not make up for it. Second lives often die within the day (seed 1 and seed 2, life 2, to the garage pair in the first hours).

Stocked containers: in the life's home district 50 % still hold anything after 5 lives (target under 35 %: unmet, not traced); in the whole town 47 % after 5 lives and 43 % after 10 (target 25-50 % after 20 lives: on course).
