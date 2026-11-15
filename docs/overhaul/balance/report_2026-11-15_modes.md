# Final (phase 14): threat modes and tones

Written by `tools/soak_report.py` on 2026-11-15 from `final_normal.csv (seeds 1-15)`, `final_stalker.csv`, `final_mutating.csv`, `final_society.csv`, `final_unknown.csv`, `final_hybrid.csv`, `final_normal.csv (seeds 1-20)`, `final_hopeless.csv`, `final_brutal.csv`.
Grim Human tone, Overwhelming Swarm, 24-minute days and the first life of a world unless a column says otherwise.

| Measure | Target | swarm (1-15) | stalker | mutating | society | unknown | hybrid | grim (1-20) | hopeless | brutal |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Lives (clean runs) | 40 | 15 | 15 | 15 | 15 | 15 | 14 | 20 | 20 | 20 |
| Median days survived | normal 2-4, careful 3-6, bold 1.5-3 (phase file: 5-8, 7-12, 3-6; see DECISIONS) | 3.13 | 3.92 | 3.49 | 5.70 | 3.94 | 2.97 | 3.20 | 2.73 | 2.19 |
| First quartile |  | 2.24 | 3.62 | 2.97 | 5.02 | 2.68 | 2.41 | 2.15 | 1.87 | 1.78 |
| Third quartile |  | 3.89 | 4.67 | 4.08 | 6.65 | 4.94 | 3.98 | 4.08 | 3.59 | 3.01 |
| Mean days |  | 2.96 | 4.13 | 3.47 | 5.56 | 3.81 | 2.82 | 2.94 | 2.59 | 2.36 |
| Lives ending on day 1 | under 5 % | 13 % | 0 % | 0 % | 0 % | 0 % | 14 % | 15 % | 20 % | 10 % |
| Died on night 1 |  | 0 % | 7 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 5 % |
| Reached day 7 | careful 5-20 % | 0 % | 7 % | 0 % | 40 % | 7 % | 0 % | 0 % | 0 % | 0 % |
| Reached day 15 | tenth life (phase file: careful 10-25 %) | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % |
| Cause: the dead | 45-60 % | 60 % | 60 % | 67 % | 82 % | 93 % | 57 % | 60 % | 80 % | 60 % |
| Cause: infection and illness | 10-20 % | 27 % | 20 % | 13 % | 9 % | 0 % | 21 % | 20 % | 5 % | 5 % |
| Cause: thirst | 5-12 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 5 % | 0 % |
| Cause: hunger | 3-10 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % |
| Cause: cold | 2-8 % with heat | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % |
| Cause: heat |  | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % |
| Cause: fire | under 5 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % |
| Cause: other people | 2-8 % (Society 20-35 %) | 13 % | 20 % | 20 % | 0 % | 7 % | 21 % | 20 % | 10 % | 35 % |
| Retired (crisis) | 1-5 % | 0 % | 0 % | 0 % | 9 % | 0 % | 0 % | 0 % | 0 % | 0 % |
| Kills per day (median) | 1-4 | 9.7 | 6.0 | 7.8 | 5.2 | 8.1 | 10.8 | 9.0 | 12.8 | 15.0 |
| First fight on day 1 | 60-90 % | 100 % | 100 % | 100 % | 73 % | 87 % | 100 % | 90 % | 90 % | 85 % |
| Quiet hours, days 1-3 | 60-80 % | 47 % | 59 % | 51 % | 69 % | 42 % | 48 % | 49 % | 40 % | 34 % |
| Quiet hours, days 8-14 | 35-55 % | - | - | - | - | - | - | - | - | - |
| Grabs per day (median) |  | 1.8 | 1.4 | 1.4 | 1.1 | 1.2 | 1.5 | 1.8 | 2.4 | 1.7 |
| Boards up (most, median) |  | 5.0 | 5.0 | 5.0 | 5.0 | 4.0 | 4.0 | 4.0 | 5.0 | 5.0 |
| Containers searched (median) |  | 55.0 | 70.0 | 69.0 | 85.0 | 77.0 | 40.5 | 54.5 | 77.0 | 50.0 |
| Drinks carried at dawn of day 3 | 4-7 | 6 | 6 | 6 | 5 | 6 | 7 | 6 | 7 | 6 |
| Nights slept 6 hours or more | over 70 % | 16 % | 12 % | 19 % | 27 % | 15 % | 16 % | 14 % | 14 % | 8 % |
| Depression on day 7 (median) | 35-55 alone | - | 99 | - | 53 | 96 | - | - | - | - |
| Lives panicked over 5 % of waking time | under 15 % | 80 % | 80 % | 73 % | 87 % | 87 % | 86 % | 75 % | 85 % | 80 % |

**swarm (1-15)** (`final_normal.csv (seeds 1-15)`):

    lives 15: median days 3.13 (quartiles 2.24 to 3.89; a median of 15 seeds moves 2.35 to 3.64 by chance), mean 2.96
    ended on day 1: 2 of 15 (13 %); died on night 1: 0 of 15 (0 %) (at home 0); alive at the end: 0
    causes (of 15 that ended): dead 9 (60 %), infection 4 (27 %), people 2 (13 %)
    median kills per day 9.7, boards (most up) 5.0, searched 55.0
    first fight on day 1: 100 % of lives; quiet hours (none within 15 tiles) days 1-3: 47 %, days 8-14: -; grabs per day (median) 1.8
    nights slept 6 hours or more: 16 % of 38 whole nights (median 1 hours)
    reached day 7: 0 of 15 (0 %), day 15: 0 of 15 (0 %); drink carried at dawn of day 3 (median): 6 (13); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 80 %

**stalker** (`final_stalker.csv`):

    lives 15: median days 3.92 (quartiles 3.62 to 4.67; a median of 15 seeds moves 3.68 to 4.65 by chance), mean 4.13
    ended on day 1: 0 of 15 (0 %); died on night 1: 1 of 15 (7 %) (at home 0); alive at the end: 0
    causes (of 15 that ended): dead 9 (60 %), people 3 (20 %), infection 3 (20 %)
    median kills per day 6.0, boards (most up) 5.0, searched 70.0
    first fight on day 1: 100 % of lives; quiet hours (none within 15 tiles) days 1-3: 59 %, days 8-14: -; grabs per day (median) 1.4
    nights slept 6 hours or more: 12 % of 51 whole nights (median 0 hours)
    reached day 7: 1 of 15 (7 %), day 15: 0 of 15 (0 %); drink carried at dawn of day 3 (median): 6 (14); depression on day 7 (median): 99 (1); lives with panic above 75 for over 5 % of waking time: 80 %

**mutating** (`final_mutating.csv`):

    lives 15: median days 3.49 (quartiles 2.97 to 4.08; a median of 15 seeds moves 2.99 to 4.00 by chance), mean 3.47
    ended on day 1: 0 of 15 (0 %); died on night 1: 0 of 15 (0 %) (at home 0); alive at the end: 0
    causes (of 15 that ended): dead 10 (67 %), people 3 (20 %), infection 2 (13 %)
    median kills per day 7.8, boards (most up) 5.0, searched 69.0
    first fight on day 1: 100 % of lives; quiet hours (none within 15 tiles) days 1-3: 51 %, days 8-14: -; grabs per day (median) 1.4
    nights slept 6 hours or more: 19 % of 42 whole nights (median 0 hours)
    reached day 7: 0 of 15 (0 %), day 15: 0 of 15 (0 %); drink carried at dawn of day 3 (median): 6 (15); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 73 %

**society** (`final_society.csv`):

    lives 15: median days 5.70 (quartiles 5.02 to 6.65; a median of 15 seeds moves 5.10 to 6.63 by chance), mean 5.56
    ended on day 1: 0 of 15 (0 %); died on night 1: 0 of 15 (0 %) (at home 0); alive at the end: 4
    causes (of 11 that ended): dead 9 (82 %), retired 1 (9 %), infection 1 (9 %)
    median kills per day 5.2, boards (most up) 5.0, searched 85.0
    first fight on day 1: 73 % of lives; quiet hours (none within 15 tiles) days 1-3: 69 %, days 8-14: -; grabs per day (median) 1.1
    nights slept 6 hours or more: 27 % of 75 whole nights (median 2 hours)
    reached day 7: 6 of 15 (40 %), day 15: 0 of 15 (0 %); drink carried at dawn of day 3 (median): 5 (15); depression on day 7 (median): 53 (5); lives with panic above 75 for over 5 % of waking time: 87 %

**unknown** (`final_unknown.csv`):

    lives 15: median days 3.94 (quartiles 2.68 to 4.94; a median of 15 seeds moves 2.94 to 4.93 by chance), mean 3.81
    ended on day 1: 0 of 15 (0 %); died on night 1: 0 of 15 (0 %) (at home 0); alive at the end: 1
    causes (of 14 that ended): dead 13 (93 %), people 1 (7 %)
    median kills per day 8.1, boards (most up) 4.0, searched 77.0
    first fight on day 1: 87 % of lives; quiet hours (none within 15 tiles) days 1-3: 42 %, days 8-14: -; grabs per day (median) 1.2
    nights slept 6 hours or more: 15 % of 46 whole nights (median 0 hours)
    reached day 7: 1 of 15 (7 %), day 15: 0 of 15 (0 %); drink carried at dawn of day 3 (median): 6 (14); depression on day 7 (median): 96 (1); lives with panic above 75 for over 5 % of waking time: 87 %

**hybrid** (`final_hybrid.csv`):

    lives 14: median days 2.97 (quartiles 2.41 to 3.98; a median of 14 seeds moves 2.49 to 3.92 by chance), mean 2.82
    ended on day 1: 2 of 14 (14 %); died on night 1: 0 of 14 (0 %) (at home 0); alive at the end: 0
    causes (of 14 that ended): dead 8 (57 %), people 3 (21 %), infection 3 (21 %)
    median kills per day 10.8, boards (most up) 4.0, searched 40.5
    first fight on day 1: 100 % of lives; quiet hours (none within 15 tiles) days 1-3: 48 %, days 8-14: -; grabs per day (median) 1.5
    nights slept 6 hours or more: 16 % of 32 whole nights (median 0 hours)
    reached day 7: 0 of 14 (0 %), day 15: 0 of 14 (0 %); drink carried at dawn of day 3 (median): 7 (11); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 86 %

**grim (1-20)** (`final_normal.csv (seeds 1-20)`):

    lives 20: median days 3.20 (quartiles 2.15 to 4.08; a median of 20 seeds moves 2.35 to 3.91 by chance), mean 2.94
    ended on day 1: 3 of 20 (15 %); died on night 1: 0 of 20 (0 %) (at home 0); alive at the end: 0
    causes (of 20 that ended): dead 12 (60 %), infection 4 (20 %), people 4 (20 %)
    median kills per day 9.0, boards (most up) 4.0, searched 54.5
    first fight on day 1: 90 % of lives; quiet hours (none within 15 tiles) days 1-3: 49 %, days 8-14: -; grabs per day (median) 1.8
    nights slept 6 hours or more: 14 % of 50 whole nights (median 0 hours)
    reached day 7: 0 of 20 (0 %), day 15: 0 of 20 (0 %); drink carried at dawn of day 3 (median): 6 (17); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 75 %

**hopeless** (`final_hopeless.csv`):

    lives 20: median days 2.73 (quartiles 1.87 to 3.59; a median of 20 seeds moves 2.17 to 3.39 by chance), mean 2.59
    ended on day 1: 4 of 20 (20 %); died on night 1: 0 of 20 (0 %) (at home 0); alive at the end: 0
    causes (of 20 that ended): dead 16 (80 %), people 2 (10 %), thirst 1 (5 %), infection 1 (5 %)
    median kills per day 12.8, boards (most up) 5.0, searched 77.0
    first fight on day 1: 90 % of lives; quiet hours (none within 15 tiles) days 1-3: 40 %, days 8-14: -; grabs per day (median) 2.4
    nights slept 6 hours or more: 14 % of 42 whole nights (median 0 hours)
    reached day 7: 0 of 20 (0 %), day 15: 0 of 20 (0 %); drink carried at dawn of day 3 (median): 7 (15); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 85 %

**brutal** (`final_brutal.csv`):

    lives 20: median days 2.19 (quartiles 1.78 to 3.01; a median of 20 seeds moves 1.94 to 2.67 by chance), mean 2.36
    ended on day 1: 2 of 20 (10 %); died on night 1: 1 of 20 (5 %) (at home 0); alive at the end: 0
    causes (of 20 that ended): dead 12 (60 %), people 7 (35 %), infection 1 (5 %)
    median kills per day 15.0, boards (most up) 5.0, searched 50.0
    first fight on day 1: 85 % of lives; quiet hours (none within 15 tiles) days 1-3: 34 %, days 8-14: -; grabs per day (median) 1.7
    nights slept 6 hours or more: 8 % of 36 whole nights (median 2 hours)
    reached day 7: 0 of 20 (0 %), day 15: 0 of 20 (0 %); drink carried at dawn of day 3 (median): 6 (14); depression on day 7 (median): -; lives with panic above 75 for over 5 % of waking time: 80 %

Threat modes against Swarm on the same 15 seeds (the first column), normal bot, Grim Human. Measures where a mode differs by a clear margin:

- Stalker: kills a day 6.0 against 9.7; quiet hours 59 % against 47 %; median 3.92 against 3.13 (the two bootstrap ranges, 3.68-4.65 and 2.35-3.64, do not overlap). Met.
- Infected Society: median 5.70 against 3.13 (4 of 15 alive at day 7); kills 5.2 against 9.7; quiet 69 % against 47 %; first fight on day 1 73 % against 100 %. Met. But deaths at people's hands are 0 of 11 against a target of 20 to 35 %: unmet.
- Mutating Plague: kills 7.8 against 9.7, containers searched 69 against 55; median 3.49 (range 2.99-4.00, overlapping Swarm's). Margins are small: not clearly met.
- The Unknown: the dead 93 % of deaths against 60 %; kills 8.1; median 3.95 (range 2.94-4.93, overlapping). Not clearly met.
- Hybrid: containers searched 40.5 against 55; day-1 deaths 2 of 14; median 2.97 and kills 10.8 close to Swarm. Not clearly met.

Tones against Grim Human on seeds 1-20 (median 3.20 days, kills 9.0 a day): Hopeless median 2.73 = x0.85 (target x0.7 to x0.85: met, at its edge). Brutal Arcade kills 15.0 a day = x1.67 (target x2 or more: unmet) and median 2.19 = x0.68 (target within +-20 %: unmet). Brutal's seeds 15-20 failed to start the first time (exit 0xC0000142 while other soaks were starting) and were run again on their own.
