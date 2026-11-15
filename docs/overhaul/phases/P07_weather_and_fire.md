# Phase 7: weather, seasons and fire

**Goal.** Weather stops being a backdrop. Each condition changes at least
two rules the player cares about, the seasons turn a long life into four
different games, and fire follows the weather.

**The player notices:** waiting for rain to cross a street unheard, fog you
can hide in, a heatwave that moves the day's work to dawn and dusk, the
first snow, a fire that the wind takes down a row of houses.

**Feel target.** "It's raining. Go now."

Phase 1 fixed the weather clock; phase 4 wired rain into noise and
visibility into the dead's sight; phase 2 gave rain to collectors. This
phase finishes the set.

## Core

### 1. One table of consequences

Implement as data in `src/world/WeatherRules.h/.cpp` (pure; one function per
column) and have every system ask it. No system reads `WeatherCondition`
directly after this phase.

| Condition | Player sight | Dead's sight | Noise radius | Body | Fire | Other |
|---|---|---|---|---|---|---|
| Clear | full | x1 | x1 | by temperature | x1 | |
| Cloudy | full | x1 | x1 | | x1 | Night is 10 % darker |
| Rain | view range -10 % | x0.85 | x0.7 | -4 C outdoors (0 with Raincoat) | spread x0.6, outdoor fires die in 2 h | Collectors fill; blood trails 3 min; footprints in mud |
| Heavy rain | -25 % | x0.7 | x0.55 | -4.5 C; wet for 1 h after | x0.35, die in 40 min | Collectors x2 |
| Thunderstorm | -30 %, flashes light everything for 0.2 s | x0.6 (x1.5 during a flash) | x0.45; thunder is Explosive | -6 C | x0.2; lightning may start a fire on a tree (5 % per strike) | Sleep quality -0.1 |
| Fog | view fades from 5 tiles | x0.45 | x1 | | x1 | The dead are not drawn beyond 6 tiles; minimap shows nothing beyond 6 |
| Heatwave | full | x1 | x1 | +8 C; thirst x1.6 outdoors by day; overheating | grass fires spread x1.5 | Food spoils x1.5 |
| Snow (winter rain) | -15 % | x0.8 | x0.75; footsteps one tier quieter | -3 C | x0.5 | Footprints; blood shows for 30 min; collectors give half |
| Blizzard | view 4 tiles | x0.3 | x0.4 | -8 C, cold drift x2 | fires outdoors die | The dead move x0.6; the player x0.75 outdoors |

- "Player sight" is drawn as a distance fog on the world shader (a uniform:
  fog start, end, colour) centred on the player; indoors is unaffected.
- **Wet.** A new condition: in rain without a Raincoat for 20 game minutes
  you are Wet for 1 hour after shelter: cold drift x1.5, a fire or a full
  hour indoors dries you. HUD condition "Wet".
- **Overheated** (body above 38.3, exists as a threshold): stamina regen
  -30 %, thirst x1.5; above 39.5: health loss 0.05/s. Shade (indoors, under a
  tree canopy) and water fix it. Carrying a Winter Coat in a heatwave: +3 C.
- **Footprints.** In mud (rain on grass or dirt) and snow, the player,
  survivors and the dead leave prints (decals, 10 game minutes; the
  particle/decals layer has stains already). Prints do nothing to the rules;
  they tell the player who passed. Cosmetic, not saved.

### 2. Seasons

- 15 days each, Spring first, as today. Daylight changes: dusk and dawn
  hours come from the season (`FirstDays::kDuskHour` and `kDawnHour` become
  functions of the day): Spring 06:00-20:00, Summer 05:00-21:30, Autumn
  07:00-18:30, Winter 08:00-17:00, blended over the 3 transition days. Night
  callers, lamps, panic-in-the-dark and the HUD clock tint use them.
- **Summer:** heatwaves; long days; food spoils x1.25; ponds shrink (cosmetic
  variant of water edges, optional); the dead are x1.1 more active by night.
- **Autumn:** rain most days; leaves down (exists); bushes stop fruiting on
  day 40; mushrooms common; the first frost line on day 43: "Frost on the
  windows."
- **Winter:** snow in place of rain; ground tiles outdoors use a snow overlay
  (a fringe-style overlay row in the tileset, alpha by accumulation 0..1,
  building over 6 hours of snow and melting over a clear day above 2 C);
  ponds freeze (walkable, slippery: acceleration halved; Dirty Water needs
  an axe or hammer to reach); rain collectors freeze (no fill below 0 C);
  gardens do not grow; indoors needs a fire at night or a Winter Coat; the
  dead are x0.8 speed in snow.
- **Clothing.** `clothingInsulation` = the best `warmth` item carried (Jacket
  5, Winter Coat 11) + 2 if also carrying Rags (wrapped). The pack shows a
  small "worn" mark on the item that counts. (No worn slots; the plan keeps
  the existing "carried counts" rule.)
- The clock line of the HUD shows the season as a word after the weather
  ("Day 17 - Summer - Clear, 29"). The day card at dawn of each season's
  first day names it: "Summer."
- Sprites: snow overlay tiles, snow-capped roof variants (a white wash by
  accumulation drawn by `StructureView` as a tint + a few flake sprites, no
  new roof set), snowflake particles in `Atmosphere` (slow, drifting, in
  place of rain streaks when temperature is 1 C or less), frozen pond tile.

### 3. Weather you can read ahead

- The sky warns: 90 game minutes before rain, cloud shadows thicken and the
  wind rises (drive `Atmosphere` from `WeatherSystem::forecast`); before a
  storm, distant thunder (P12) and a darker tint; before fog, the horizon
  haze comes in at dusk.
- The Radio (P06) says it in words.
- The HUD weather word gains a second word when change is within 90
  minutes: "Clear, clouding".

### 4. Fire, rebuilt on a grid

Replace the per-entity fire scan in `GameFire.cpp` with a tile grid
(`src/systems/FireSystem.h/.cpp`): `fuel` (0..1 by what is on the tile),
`heat`, `burning`, `burned`, stored per tile in a sparse map of active
tiles. Entities ask the grid by tile (O(1)), the grid never scans entities.

- **Fuel by tile:** grass 0.5 (0.8 in a heatwave, 0.2 wet, 0 under snow),
  wood floor 0.6, carpet 0.7, furniture +0.3 (wood) or +0.05 (metal), a
  wooden wall of a house 0.4 (walls burn now: section 5), boards on a door
  or window +0.2, a tree 0.9, brick, tile, concrete, asphalt, metal 0.
- **Spread:** each burning tile every 2 s tries its 4 neighbours (and the
  2 downwind diagonals when wind is over 0.5): chance = fuel x (0.25 + 0.3 x
  heat) x weather factor x (1.6 downwind, 0.6 upwind). Burning uses fuel at
  0.02/s; heat rises to 1 over 20 s; a tile with no fuel left is burned.
- **Rain:** removes heat (table); a tile drops out at heat 0.
- **Damage:** 14/s to the player and 18/s to others x heat (as today),
  using the spatial grid of phase 4. Kills by a fire the player lit are
  credited to the player (store `litBy` on the fire event).
- **Smoke:** indoors, a burning building fills with smoke: after 20 s inside
  a building with 3 or more burning tiles: "Coughing" (stamina regen -50 %,
  health -1/s after 40 s). Smoke is drawn as a grey haze under the roof and
  rising puffs above it; a column of smoke over a burning building is
  visible across the map by day and marks it on the minimap.
- **Light and noise:** burning tiles light their surroundings (cluster them:
  one light per 3x3 block, capped at 8 of the shader's 16); a fire of 6 or
  more tiles is Loud every 2 s; the dead walk toward fire they can see and
  do not avoid it (they burn: 20 % of zombies set alight keep walking for
  6 s and spread fire to what they touch).

### 5. What fire does to the town

- **Houses burn.** When 40 % of a wooden building's floor tiles are burned,
  it is "gutted": its roof is drawn as charred rafters (one new roof sprite
  set: `roofp_burnt_<mask>`), its doors and windows are gone (open), its
  furniture and containers are destroyed with what they held, and its walls
  are drawn scorched (a dark tint by `StructureView`). Brick and metal
  buildings burn out inside but keep doors and walls.
- Gutted buildings and burned tiles are world memory (burned tiles already
  are). A gutted home cannot be a start home. The town map marks them.
- Burned grass regrows after 2 lives; burned trees stand as black trunks.

### 6. Putting fire out, and using it

- Water Bottle on a fire tile within 3 tiles (F, exists as dousing): -0.9
  heat. Bottles and rain are the only ways to put a fire out.
- **Firebreak:** breaking up furniture and prying boards removes fuel, which
  matters now.
- **Campfire safety:** a campfire on stone, tile, concrete or bare dirt is
  safe; on grass it spreads in a heatwave (10 % per hour) and on wood or
  carpet at any time (5 % per hour, P02's rule now uses the grid).
- **Molotov:** lights a 3-tile cluster at heat 0.8.
- **Fire as a wall:** a burning doorway is not walked through by survivors;
  the dead walk through and burn.
- Trait `Firewise` finally does something: fire damage to you -40 %, and you
  see a tile's fuel as a faint shimmer when holding a Molotov or Matches.

## Later

- Wind direction shown by a weather vane on a roof near the centre.
- Flooding roads in a storm.

## Files

New: `src/world/WeatherRules.*`, `src/systems/FireSystem.*`,
`tests/test_weather_rules.cpp`, `tests/test_fire.cpp`,
`tools/harness/weather_rules.txt`, `seasons.txt`, `fire_spread.txt`.
Changed: `WeatherSystem`, `Atmosphere`, `NeedsSystem`, `NoiseModel`,
`ZombieSenses`, `FirstDays` (dusk and dawn by season), `GameFire.cpp`
(thin wrapper over `FireSystem`), `StructureView`, `SceneryView`,
`GameRender.cpp` and `sprite.frag` (distance fog), `Hud`, `SaveManager`
(gutted buildings; fire grid in checkpoints), `generate_sprites.py`, harness
(`weather KIND [HOURS]` exists; add `season NAME`, `wind X Y`, `ignite X Y`,
`fires`).

## Acceptance

- `weather_rules.txt`: for each condition, one screenshot outdoors at noon
  and the measured values printed by a new `weatherinfo` command match the
  table (noise scale, sight scale, body target).
- Fog: a zombie 8 tiles away is not drawn; at 5 it is.
- `seasons.txt`: `season winter`, snow builds on the ground over 6 game
  hours (three screenshots), the pond is walkable, a collector does not
  fill, body temperature outdoors at night without a coat reaches "Cold"
  within 2 game hours and with a Winter Coat and a fire indoors does not.
- `fire_spread.txt`: a molotov on grass in a heatwave with wind east: after
  5 game minutes the burned area is longer east than west (count tiles); in
  heavy rain the same fire is out within 40 game minutes; a wooden house lit
  inside is gutted and stays gutted after `kill` and `newrun` (screenshot of
  rafters).
- `perf 300` with a 60-tile fire burning: tick time within the phase 4
  budget.
- Unit tests: every column of the table; spread chance by wind; fuel by
  tile; season hours.
