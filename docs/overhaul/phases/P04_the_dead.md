# Phase 4: the dead

**Goal.** The dead become a readable, varied, town-sized pressure: kinds you
learn to tell apart, senses that obey the world (walls, doors, rain, blood),
crowds that move as crowds, and six threat modes that are six different
games.

**The player notices:** a runner breaking from a group, a scream that turns
heads three streets away, the dead following a blood trail to the back door,
a horde passing through on night 7, quiet rain.

**Feel target.** You shut the door, crouch, and listen to them go past.

Kinds and numbers: `03_content_tables.md` section 6.

## Core

### 1. Performance first: paths and buckets

- **Flow field for the player.** Once every 0.25 s, build one breadth-first
  distance field from the player's tile over walkable tiles (closed doors
  cost 25, furniture-blocked doorways 40, breakable windows 30), out to 40
  tiles. Chasing zombies step downhill on it; no per-zombie A*. Keep A* for
  migration and investigation, with a budget of 6 path requests per tick in a
  queue (others wait a tick).
- **Spatial grid.** A uniform grid of 4x4-tile cells holding zombie, survivor
  and corpse entities, rebuilt each tick. Separation, grabs, shoves, fire
  damage, noise hearing and "zombies within N tiles" queries use it.
- **Sleeping the far ones.** Zombies further than 45 tiles from the player
  and not migrating update at 6 Hz (every tenth tick, with dt x10) and skip
  separation.
- Target: 300 zombies, under 4 ms a tick in Release. Add a harness command
  `perf N` that spawns N zombies around the player and prints average and
  worst tick time over 600 ticks.

### 2. Kinds

- `assets/data/zombies.json` and `src/world/ZombieKinds.h/.cpp` (load, pick a
  kind for a spawn from the day and an RNG, pure and tested). `ZombieAI`
  gains `kind` (saved in `ZombieRecord`; old records are Shamblers).
- Spawn mix: section 6 shares, applied in `GameScene.cpp` (start list) and
  `ThreatDirector` spawns. District infection over 50 doubles the Runner and
  Screamer shares there.
- Behaviour, in `ZombieAISystem` (split the file: `ZombieSenses.cpp`,
  `ZombieMove.cpp`, `ZombieAttack.cpp`, kinds as small strategy tables, no
  inheritance):
  - **Runner:** on first sight, 0.5 s "start" (head snaps up: the tell), then
    1.7x speed for 8 s, then winded 4 s at 1.0x. Wind-up 0.40 s. Always
    grabs when it reaches you from a sprint.
  - **Brute:** ignores knockback under 30 px; its claw is a shove that knocks
    the player down for 0.8 s (cannot act, can be grabbed by others); door
    and board damage x3; the floor shakes lightly within 4 tiles when it
    walks (a small camera pulse: the tell).
  - **Crawler:** drawn low (prone frames); not drawn behind furniture and
    tall grass from more than 2 tiles; 0.35x speed; bite to the legs: 5
    damage and "Hobbled" (move -25 %, 20 s). Comes out from under beds and
    cars: 30 % of Crawlers start "dormant" beside furniture indoors and wake
    when you are within 2 tiles (a scrape sound 0.6 s before they move).
  - **Screamer:** on sight, stops and screams 1.2 s (Loud); keeps 4 to 6
    tiles away, screams every 10 s while it sees you; flees from a rush at
    0.9x. Killing it first is the lesson.
- Art (`tools/charrig.py`): Runner = lean build, forward-tilted run cycle and
  a "start" pose; Brute = wider torso and limbs (scale x1.25 across, x1.08
  tall), heavy walk; Crawler = prone drag cycle, 2 facings mirrored;
  Screamer = head back, arms down, jaw open (3 frames). Each kind keeps the
  three clothing looks. Bodies on the floor keep their kind's build.
- Readability: each kind has a distinct silhouette and one tint cue (Runner
  redder skin, Brute greyer, Screamer pale), and each has its own sound in
  P12. No floating labels.

### 3. Decomposition that matters

- Stage comes from `spawnDay` as today. Add the effects in section 6:
  Bloated burst on death (a green-grey puff, 1.5 tiles: illness +8, panic
  +10; killing it with fire or from 2+ tiles away is clean); Decayed crowds
  raise illness; Skeletal can be walked through at half speed.
- New zombies spawned by the director after day 1 are Fresh (the newly dead);
  the starting population ages. So over a long run the town shows two
  populations: old slow ones and fresh fast ones.
- World memory: at run end save the count of kills per district; next run's
  starting population in that district is reduced by 50 % of them (the
  design document's "a district cleared stays thinner"), capped at 60 %.

### 4. Senses

**Sight** (`ZombieSenses`):
- Range by kind, x0.6 in the dark, x1.3 if the player is lit (flashlight,
  beside a fire, a lit uncurtained window behind them), x weather
  visibility (`WeatherState.visibilityMultiplier`, finally used), x0.5 if the
  player is crouched and still, x0.75 crouched and moving. A front cone of
  160 degrees for unaware zombies; alerted ones see all round.
- Sight is checked at 10 Hz per zombie, staggered by entity id.
- **Awareness meter.** Seeing the player fills a 0..1 meter at (1.6 -
  distance/range) per second; at 1 the zombie chases. Below 1 it turns and
  shuffles toward what it saw ("Investigate"). The meter drains 0.5 per
  second out of sight. This gives the player a beat to break line of sight.
  Runners fill twice as fast.
- Small world tell: a zombie whose meter is above 0.4 lifts its head (one
  frame change in the rig). No icons.

**Hearing** (`NoiseModel`):
- Radii: Whisper 3, Soft 7, Medium 15, Loud 30, Explosive 60 tiles.
- **Walls and doors.** When a noise is emitted indoors, or heard indoors,
  loudness crossing the building's shell is reduced: all doors and windows of
  that building closed: -1 tier; any open or broken: no change. Compute per
  event at emit time: `indoorBuildingId` and `sealed` (bool), stored on the
  event; a listener in a different building id (or outdoors) applies the
  reduction. Cheap and good enough; no ray casting.
- **Weather.** Rain x0.7, heavy rain x0.55, storm x0.45 on every radius
  (thunder itself is Explosive at a random outdoor tile within 40 tiles of
  the player every 40 to 120 s of a storm: it scatters wanderers toward it).
  Wind: +20 % downwind, -20 % upwind for Loud and above.
- **Who makes noise.** Survivors emit as the player does. Chasing zombies
  groan (Soft every 3 to 5 s), which is how one chase becomes a crowd.
  Zombies ignore noise whose source is a zombie unless it is a Screamer's
  scream or a door being battered.
- **Memory.** A zombie investigating a noise walks to it, looks around for
  6 to 10 s (turning in place), then wanders. Loud and Explosive events
  leave an "attractor" for 60 and 180 s: zombies that reach the spot stay
  near it for that long (so a gunshot gathers a crowd that lingers).

**Smell: blood trails** (new `src/world/BloodTrail.h/.cpp`):
- While the player has `Bleeding` or an undressed Cut or Bite, a marker is
  dropped every 1.5 tiles walked: `{tile, strength, age}`. Bleeding: strength
  1.0; undressed wound: 0.4.
- Markers fade: 20 game minutes indoors, 10 outdoors, 3 in rain, at once on
  water tiles. A zombie within 5 tiles of a marker, not chasing, with no
  noise to follow, walks to the freshest marker within 5 tiles of itself,
  then the next fresher: it follows the trail. Rate: checked at 2 Hz.
- Markers are drawn: small dark-red drops on the ground (decal layer), so the
  player can see their own trail and what follows it.
- Bladed kills leave a pool that counts as a strength 0.3 marker for 10 game
  minutes ("fresh blood"); blunt kills do not. This makes the weapon classes
  matter after the fight.
- Checkpoints save markers; not world memory.

### 5. Crowds

- **Clumping.** Wandering zombies within 6 tiles of 3 or more others drift
  toward the group's centre 30 % of the time: groups of 4 to 10 form on
  their own and wander as one (share a heading: each adopts the average
  heading of neighbours within 4 tiles when choosing a new step).
- **Hordes.** The director creates a horde event:
  - **When:** night 7, then every 4 to 6 nights (run seed); in Swarm and
    Hybrid modes every 3 to 4 nights starting night 5.
  - **What:** 14 + 2 per day past day 7 (cap 40) zombies gathered from the
    nearest groups (spawned fresh at the map edge if too few), given a path
    that crosses the town along roads and passes within 6 to 10 tiles of the
    player's home. They walk at 0.6x, do not stop for Soft noise, and any
    that sense the player peel off and chase; Loud noise turns all within
    range.
  - **Warning:** from dusk that day, distant groaning (P12) and birds gone;
    the day card at dawn of a horde day reads "Something is moving on the
    far side of town." The objective panel adds "Tonight: stay quiet."
  - A quiet, dark, closed-up house is passed by. Light in a window, a fire, a
    sprung alarm or a gunshot is not.
  - After passing, the horde disperses into groups in the district it ends in,
    which raises that district's infection by 5.
- **Night callers** stay as in phase 2, but from night 4 on, 1 in 4 callers
  is a Runner and from night 6 one caller per night is a Brute.

### 6. Threat modes as different games

Keep the six. Each sets mix, senses and director, in `zombies.json` under
`modes` (replace the scalar switch in `GameShared.h`):

| Mode | The idea | What changes |
|---|---|---|
| Overwhelming Swarm | Many, weak, endless | Population x1.45; health x0.8; Runners and Brutes halved; clumping distance 9 tiles; hordes early and often |
| Stalker | Few, relentless | Population x0.5; awareness never drains; give-up time 600 s (not 90); blood trails last 3x; one named "follower" per run that always knows the district you are in and arrives each night |
| Mutating Threat | It gets worse | Day 1: Shamblers only. Runner, Brute and Screamer shares grow +2 % a day with no cap below 35 % each; from day 10 doors take x2 damage; from day 15 the dead climb intact windows |
| Infected Society | People are the danger | Population x0.7, slow; survivors x2, 40 % hostile (P08 makes that mean something); until P08, as today |
| The Unknown | You must learn the rule | Each run picks 2 of 5 hidden rules by seed: dormant by day and frenzied at night (x1.5 speed); blind but hearing x2; drawn to light from 20 tiles; calm in rain and frenzied when it stops; ignore you while you stand still. The notebook records a rule once seen 3 times ("They do not see well, but they hear.") and passes to later lives via notebooks |
| Hybrid | Swarm, then mutation | Swarm rules to day 7, then Mutating rules on top |

The title screen's one-line descriptions are rewritten to match.

### 7. District infection you can see

- Infection changes spawn mix (section 2), not only stats.
- Visible state, no percentage: per district, at run start and each dawn,
  set a "ruin level" 0 to 3 from infection (under 25, 25-50, 50-75,
  overwhelmed) that adds scenery overlays by position hash: more litter and
  abandoned bags, smears on walls, bodies on the street (static corpse
  decals, max 12 per district), birds absent (P12). `SceneryView` takes the
  level; nothing is saved but the infection number (saved per run today;
  move it to world memory so districts stay turned between lives, with -10
  per life as "it burned itself down a little").
- Clearing: each kill in a district lowers its infection by 0.15; the daily
  rise stays. So a district can be held down by work, never cleaned.
- The HUD's district word stays ("quiet", "uneasy", "turning", "overrun").
  The town map shades districts by level.

### 8. The dead and others

- Zombies target the nearest of: player, survivors (P08 depends on this),
  and, for noise, anything. Chasing a survivor uses A* with the queue (few).
- Bodies of the dead can be searched (E, loot class `corpse`, rolled once
  when first searched); the risen keep their packs as today.

## Later

- Zombies that play dead among corpses.
- A "follower" for every mode on long runs.

## Files

New: `src/world/ZombieKinds.*`, `src/world/BloodTrail.*`,
`src/ai/FlowField.*`, `src/ecs/SpatialGrid.h`, `src/systems/ZombieSenses.cpp`,
`ZombieMove.cpp`, `ZombieAttack.cpp`, `src/world/Horde.*` (rules: schedule,
size, route), `assets/data/zombies.json`, `tests/test_zombie_kinds.cpp`,
`tests/test_senses.cpp`, `tests/test_bloodtrail.cpp`, `tests/test_horde.cpp`,
`tools/harness/kinds.txt`, `senses.txt`, `horde.txt`, `perf.txt`.
Changed: `ZombieAI.h`, `ZombieAISystem.*`, `ThreatDirectorSystem.*`,
`NoiseModel.*`, `DistrictInfectionSystem.*`, `GameShared.h` (mode tuning
out), `GameScene.cpp`, `GameUpdate.cpp`, `SceneryView`, `SaveManager`,
`charrig.py`, `generate_sprites.py`, harness (`zombie DX DY KIND`, `horde`,
`trail`, `perf N`, `noise TIER`), title text.

## Acceptance

- `kinds.txt`: one of each kind spawned near the player with god mode on;
  screenshots show five silhouettes that can be told apart at default zoom;
  the Runner covers 10 tiles in under 4 s; the Brute is not moved by a knife;
  the Screamer's scream brings a wanderer placed 20 tiles away; a dormant
  Crawler wakes at 2 tiles.
- `senses.txt`: a crouched, still player 5 tiles from an unaware Shambler in
  the dark is not seen for 10 s; the same player running is. A swing indoors
  with doors shut is not heard 10 tiles outside; with the door open it is.
  In heavy rain a walking player is not heard at 5 tiles. A bleeding player
  walks 15 tiles and stops; a zombie placed at the start of the trail
  arrives within 60 s; with a bandage on and rain, it does not.
- `horde.txt`: `day 7`, `hours 21`, force a horde; with lights out and the
  house shut, no zombie enters within the night; with a candle in an
  uncurtained window, some do. Screenshot `horde_passing`.
- `perf 300`: average tick under 4 ms, worst under 12 ms.
- Unit tests: kind shares by day, awareness fill and drain, noise through a
  sealed and an open building, weather scales, trail fade by place, horde
  schedule by seed and mode.
- `bot_week` x10 per threat mode (60 runs): record the table. Stalker and
  Swarm must differ in cause and day of death.
