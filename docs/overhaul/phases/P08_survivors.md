# Phase 8: survivors

**Goal.** Other people who are trying not to die, with the same needs as the
player, a place they live, and a temper. Few, named, finite, remembered.

**The player notices:** smoke from a chimney two streets over; someone who
watches from a window and does not come out; a trade that helps; a week
travelling with a person and then burying them; a house found stripped by
someone who was hungrier.

**Feel target.** Meeting anyone alive on day 12 makes the player stop and
think before stepping into view.

## What exists and goes

Today's `SurvivorAISystem` (wander, circle, flee, hit) and the observe /
approach / give keys are replaced. Keep: the `SurvivorAI` component (grown),
names, the survivors save file, retired characters who "live on".

## Core

### 1. How many, and for ever

- A new world holds **12 survivors** (Infected Society mode: 20), created
  once at world creation with a name, a personality, a home building and a
  small stock. They are **world memory**: the survivors file holds each
  one's state between lives. Nobody is created after that except retired
  player characters. When they are dead, the town is empty of the living.
- Between lives each survivor is advanced by a cheap rule
  (`SurvivorRules::advanceBetweenLives`): days passed = the dead character's
  days alive; each survivor's stock falls by its upkeep, it loots a share of
  the nearest unlooted containers (they really empty: write the container
  records), and it dies with chance per day = 1.5 % x (2 - competence) x
  district danger; a dead survivor leaves a body or a grave (if it had a
  companion) and its home as it was. Deterministic from the world seed and
  life number, tested.

### 2. Who they are

`SurvivorAI` fields: `aggression`, `cooperation`, `competence` (exist),
`desperation` (derived each tick from needs), `trust` toward the player
(-100..100, world memory), `homeBuilding`, `stock` (an `Inventory`),
`companionOf` (another survivor id or the player), `role` in a group.

- **Loners** (6): one person, one house.
- **Two pairs** (4): share a home; one stays when the other goes out.
- **One group of 2** (Infected Society: a group of 5 plus more loners) that
  is hostile to anyone at its door: "the ones at the garage".
- Attitude comes from the numbers, not a label: hostile if aggression over
  0.7 or (desperation over 0.7 and cooperation under 0.4); open if
  cooperation over 0.6 and trust at or above 0; otherwise wary.
- Looks: three survivor outfits in the rig (they stop wearing the player's
  frames with a tint), each in the loadouts the player has.

### 3. A day in their life (utility AI)

`src/systems/SurvivorBrain.cpp`; scoring in `src/world/SurvivorRules.*`
(pure, tested). Every 2 s each survivor within 60 tiles of the player scores
its options and takes the best; each option is a small state with a goal
tile and an end condition.

| Option | Score rises with | What it does |
|---|---|---|
| Flee | dead within 6 tiles x (1 - aggression); low health | Runs home by the flow of its own A* path; shuts doors behind it |
| Fight | 1 or 2 dead within 3 tiles, aggression, a weapon | Uses the player's combat rules (swing, shove) through the same `CombatRules`; competence sets timing |
| Eat / drink | hunger, thirst | Uses an item from its stock |
| Sleep | sleep debt, night, at home | In its bed; wakes by the player's rules |
| Scavenge | stock below 3 days of food and water, daytime | Walks to the nearest unsearched container within 40 tiles of home, searches (takes food, water, medicine, a better weapon), carries it home, stores it |
| Fortify | competence, planks in stock, a way in unboarded | Boards its doors and windows with P02's rules |
| Tend | a wound and medicine | Treats itself |
| Watch | the player seen within 12 tiles, wary | Stands at a window or door; follows the player with its facing |
| Idle | else | Sits, paces the room, reads |

- They use doors, close them, climb windows, obey furniture and noise rules,
  make noise, bleed and leave trails. The dead hunt them (phase 4).
- Needs use `NeedsSystem` rates x0.8 on a small needs struct (hunger, thirst,
  sleep debt, health, wounds as a count). They can starve.
- **Far away** (over 60 tiles from the player): no pathing. Once per game
  hour: consume upkeep; with daytime and low stock, "scavenge abstractly"
  (empty one container near home, move its food to stock; 2 % x danger chance
  of a wound; 0.5 % of death). The same rule as between lives, hourly.
- Traces: because they act through the real rules, the player finds boarded
  houses, emptied shelves with doors ajar, a campfire's ash, bodies of the
  dead near a home, and sometimes a body.

### 4. Meeting them

One key: **E on a survivor** within 2 tiles (prompt "Talk"), when they are
not hostile or fleeing. It opens a small panel docked right (540 px), in
the bare-hands style, with their name, one line of how they seem ("Thin.
Watching your hands."), and up to five choices:

| Choice | Needs | Outcome |
|---|---|---|
| Say hello | - | Trust +3 once a day. They say one line: where they have been, what they saw ("The pharmacy's alarm still works. Don't."), from a pool keyed to what they know. This is how survivors hand out leads: a kind of building seen, a horde's direction, a clue (P09) |
| Trade | open, or wary with trust 10+ | The trade panel (section 5) |
| Give | - | Hand over an item: trust + its worth x2 |
| Ask them along | trust 40+, a loner or the travelling half of a pair | They become a companion (section 6) |
| Leave | - | |

- Approaching with a weapon drawn (a swing in the last 5 s, or a gun in
  hand) within 8 tiles: wary ones say "Put it down." and trust -5; hostile
  ones attack sooner.
- Entering a survivor's home uninvited (trust under 20): they warn once
  ("Out."), then turn hostile for the day; taking from their containers is
  theft: trust -40, hostile at once if seen.
- A survivor with desperation over 0.8 and trust under 10 may **rob** you:
  they follow at a distance for up to a game hour and, if you sleep or turn
  your back indoors, take food and water from your crate or pack (not a
  fight; you wake to "Someone has been here."), then avoid you. Always
  earned by circumstances: only the desperate, never at random.
- Hostile survivors fight like the player: they use what they hold,
  including a gun if their stock has one (noise and all). Killing a person:
  panic +20, depression +12 (+0 after 60 desensitisation), the body can be
  searched, and survivors who knew them lose 50 trust if they find out
  (within 20 tiles at the time).
- Text only; no voices. Lines are short and in the game's voice. Write 8
  lines per situation (hello by attitude, warn, thank, refuse, hurt, rob,
  goodbye), chosen by hash of name and day, in
  `assets/data/survivor_lines.json`.

### 5. Trade

- Left: your pack; right: what they will part with (their stock minus 2
  days of their own food and water and their weapon). Each side builds an
  offer; a bar between shows "They want more / Fair / Generous".
- Worth is need-driven: base value per item (data: a `value` field in
  `items.json`, e.g. Canned Food 10, Water Bottle 8, Antibiotics 30,
  Bandage 8, Pistol Ammo 4 each, Nails 0.5 each, Book 6), multiplied for
  them by what they lack (food x2 when hungry, medicine x3 when hurt) and
  for you never (the player judges their own need).
- A fair or better trade: trust +5; generous: +10.

### 6. Companions

- One at a time. They follow within 3 to 5 tiles, stop when you crouch, wait
  where told (E on them: "Wait here" / "Come"), fight and shove by their
  numbers, carry 15 kg (their pack is a container you can open: E, then
  "Carry this").
- They eat and drink from their own pack first, then ask ("I'm out of
  water."). Ignored for 12 hours: trust -10 per day; under 20 they leave
  for home with what they carry.
- At your base they sleep in a second bed or bedroll if there is one and
  keep watch in turns: with a companion, the sleep quality table of P02 gets
  +0.1 and you are woken 3 s earlier.
- Company: the existing depression relief while near (stronger: -72/day is
  today's number; keep). Their death near you: depression +20, and they get
  a grave where they fall (name, day, "travelled with <name>"), which later
  lives find.
- A companion can be asked to stay at the base for good ("This is home
  now."): they move their home building there. A base with someone in it
  is not looted by others and is kept boarded between lives (their Fortify
  option), which is the strongest way to pass a base on.
- When the player dies, a living companion goes home (or stays at the base)
  and remembers: the next character starts at trust 15 with them ("You look
  like someone I knew.") if the dead character had 60 or more.

### 7. Retired characters

A character who retires (crisis, or by choice in P13) joins the 12 as a
survivor with competence 0.8, cooperation 0.7, the base as home and their
pack as stock. They greet the bloodline with trust 50 and will teach: "Say
hello" gives one known recipe or one clue they held.

### 8. Seeing them

- Name labels at close range (exists). No health bars.
- The notebook's "Who is out there" page lists survivors met: name, where
  they live (if seen), how things stand ("Wary." / "Owes you." / "Do not go
  back."), last seen day. World memory, so later lives inherit the list as
  "Someone wrote: Ana, the blue house by the diner. Trades."
- The town map marks known homes with a small house mark.
- Smoke: a survivor with a fire lit shows a thin smoke column by day
  (P07's smoke) and a lit window by night. That is how you find them.

## Later

- Groups that move home when their district is overrun.
- Survivors who come to your door at night asking to be let in.

## Files

New: `src/world/SurvivorRules.*`, `src/systems/SurvivorBrain.cpp`,
`src/core/GameSurvivors.cpp` (talk and trade panels),
`assets/data/survivor_lines.json`, `tests/test_survivor_rules.cpp`,
`tools/harness/survivors.txt`, `trade.txt`, `companion.txt`.
Changed: `SurvivorAI.h`, `SurvivorAISystem.*` (thin shell or removed),
`SurvivorInteractionSystem.*` (removed), `GameScene.cpp` (spawn from the
survivors file), `GameRunEnd.cpp` (advance between lives), `SaveManager`
(survivor records: trust, home, stock, companion, known lines; version 4),
`ZombieSenses` (targets), `GameUi.cpp` (notebook page, map marks),
`charrig.py` (outfits), `items.json` (`value`), harness (`survivor DX DY
[open|wary|hostile]`, `goto survivor [NAME]`, `set trust N`, `survivors`,
`talk N`, `advance DAYS`), Controls panel (O, G, T keys removed).

## Acceptance

- `survivors.txt`: a survivor with low stock walks to a container, searches
  it, walks home and stores the food (the container record is empty after);
  eats when hungry; flees three of the dead and shuts its door; boards a
  door when it has planks. `advance 10`: stocks fall, some containers near
  homes are emptied, the result is the same twice for one seed.
- `trade.txt`: the talk panel and the trade panel fit at 1280x720
  (screenshots); a hungry survivor takes one can for two bandages; trust
  rises.
- `companion.txt`: at trust 40 a loner follows, waits, comes, fights one
  zombie beside the player; killed by a spawned Brute, they leave a named
  grave that the next life's legacy card lists.
- Unit tests: scoring table, attitude from numbers, worth by need, between-
  lives advance (determinism, stock, death chance), robbery conditions.
- `perf`: 20 survivors near the player and 100 zombies: within budget.
- An old survivors file (version 3) loads: existing survivors get homes,
  empty stock and trust 0.
