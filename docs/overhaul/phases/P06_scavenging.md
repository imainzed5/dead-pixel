# Phase 6: scavenging and the town that runs out

**Goal.** Give the town places worth walking to, make getting in a decision,
and make the world visibly run down: water, then power, then food. Then give
the long game its answers: boiling, cooking, snares, a garden.

**The player notices:** a pharmacy with a locked door and an alarm, the day
the lights go out, a fridge that starts to smell, rain in a collector, a
first stew, beans coming up behind the house.

**Feel target.** Day 5, dusk. The street lamps do not come on.

Tables: building kinds (section 4), loot (section 5), recipes (section 3) in
`03_content_tables.md`.

## Core

### 1. Buildings with a purpose

- `LayoutData` buildings gain `kind` (enum `BuildingKind`), assigned by the
  rule in section 4 of the tables (`src/world/BuildingKinds.h/.cpp`, pure,
  tested: same seed, same kinds; guarantees met when the town has the
  buildings). Saved in the layout; computed for old layouts.
- `Furniture.cpp`'s furnisher picks roles by kind: Grocery: aisles of shop
  shelves, a checkout, a stock room; Pharmacy: a counter with shelves
  behind, a locked stock room; Hardware: racks and bins; Clinic: cots (new
  piece, sleepable, quality 0.7), cabinets; Police: desks, a locker room,
  one **armoury locker** (always locked: needs a Crowbar and 30 s, Loud);
  Diner: tables and a kitchen with stoves; Office: desks, filing cabinets;
  Garage: workbench, tool racks, barrels; Warehouse: pallet stacks (give 2
  Planks each when broken up, no tools) and crates.
- New furniture pieces (64x80 sprites, same style): cot, desk, filing
  cabinet, locker, pallet stack, pharmacy shelf, tool rack, diner table.
  Reuse existing pieces wherever one fits.
- **Changing furnishing changes saved towns.** That is allowed once, here:
  on loading a version-2 save, non-house buildings are refurnished by the
  new rules and container records for tiles that no longer hold a container
  are dropped (log the count). Houses are not touched. Raise the save
  version to 3.
- Outside: each kind gets a sign above its street door drawn by
  `StructureView` (a small board with a pictogram: cross, wrench, cart,
  badge, cup, crate): readable at default zoom, no text. The town map (M)
  marks kinds you have **seen** (walked within 8 tiles of the door) with the
  same pictograms; seen kinds are world memory, so later lives know where
  the pharmacy is.
- The fixed rare finds of section 5 are placed here (move the three guns
  from phase 3's stopgap).

### 2. Getting in

- Doors gain `locked` (bool, chosen at world creation by the kind's chance;
  the home is never locked; world memory once forced). A locked door will not
  open with E: "Locked."
- Ways in, each a trade:

| Way | Needs | Time | Noise | After |
|---|---|---|---|---|
| Force the door | Crowbar | 6 s | Loud each 2 s | Door works, lock broken |
| Break the door | any weapon, or shoulder (25 stamina a try) | 150 damage | Loud per hit | Door broken for good |
| Break a window and climb | - | 1.5 s | Loud | P02 rules |
| Quiet window | Duct Tape | 4 s | Soft | Window broken |
| The back way | - | - | - | 50 % of locked buildings have an unlocked back door |
| Keys | found, not carried: 1 kitchen container in 8 holds a key ring (no item) | - | - | Searching it unlocks the 4 nearest locked houses: "Keys. They fit the houses on this street." |

- **Alarms.** Pharmacy 60 %, Police 100 %, Grocery 30 %, Hardware 30 %: while
  the power is on (section 4), forcing or breaking in starts an alarm: Loud
  every 2 s for 45 s, from the door. It can be stopped at a small box inside
  by the door (E, 3 s). After the power fails, alarms are dead. The lesson:
  go for the pharmacy early and loud, or later and quiet.
- **Searching takes time.** Opening a container is a held action: 1.5 s for
  a cupboard, 3 s for a wardrobe, crate or shelf run, 5 s for a locker or
  filing cabinet (Scavenging skill shortens). Soft noise. The panel opens
  when it finishes. Containers already searched this run open at once.
  Being hit cancels.
- **What is inside is unknown until searched,** but searched state shows: a
  searched cupboard's door stands ajar (a sprite variant for each container
  piece: one extra frame). This is also how the player sees a building was
  picked over in an earlier life.

### 3. Water after the taps

- Sources: bottles found; the Rain Collector (P02); **Dirty Water**: R with
  Empty Bottles at a pond or water tile (they exist on the map), at a toilet
  cistern or bathtub (one fill each, once per world), or from a collector
  that has stood more than 3 days.
- Drinking Dirty Water: thirst as water, illness +12 (+24 with `Weak
  Stomach`), and 25 % "Bad stomach" for 6 hours (thirst and hunger drain
  x1.5).
- Cleaning: boil (fire station, pot, recipe) or Purification Tablets.
- The day-3 plan step "Find water (n/3), or build a rain collector" now
  also accepts boiled water.

### 4. Power

- The town has power until **day 5 at 19:00** (run seed shifts it by up to
  12 hours). From day 4 it flickers: lamps and fridges stutter for a few
  seconds a few times a day, with the line "The lights dip." once.
- While on: street lamps light at dusk (they already draw; two in three
  work); fridges halve spoilage; shop alarms work; house lights come
  from one new furniture piece, a floor lamp, placed in living rooms and
  bedrooms by the furnisher and toggled with E (there are no wall
  switches). A lit room shows through uncurtained windows.
- When it fails: the line "The power is out." (a day card if asleep: at dawn,
  "The power went in the night."); lamps and street lamps dead; fridge
  contents spoil at normal speed (and "the fridge smells" becomes true: food
  in fridges at condition below 0.3 gives the searching line "It has turned.");
  alarms dead; radios still work on batteries.
- Power state is per run (checkpoint), not world memory: each life starts in
  a town with power for five days. (The apocalypse restarts per life as it
  does for taps; this is the game's existing conceit.)
- Batteries: the Flashlight holds one Battery's worth: 8 game hours of light;
  F with a Battery selected swaps it in. The HUD belt shows a dim flashlight
  icon when empty. Starts full.

### 5. Food

- **Spoilage** exists (condition). Add states to the item name in the pack:
  "fresh" (above 0.6), "stale" (0.3 to 0.6: 75 % of the food value),
  "spoiled" (below 0.3: half value, illness as today). Canned and dry food
  (`spoilHours` 0) never spoils: set Canned Food, Rice, Chips, Chocolate,
  Soda, Coffee to 0; Bread 48, Apple 72, Vegetables 96, Cooked Meat 24, Raw
  Meat 12, Stew 24, Cooked Rice 24, Berries 24, Mushrooms 24.
- **Cooking** at a `fire` station (campfire, or a kitchen stove while the
  power is on): E at the fire opens the Make panel filtered to what this
  station can do. Hot food gives +5 warmth for 2 h and depression relief
  (P05).
- **Hunger values:** Canned Food 30 (exists), Rice raw: cannot be eaten;
  Cooked Rice 45; Cooked Meat 35; Raw Meat 20 with illness +20; Stew 60;
  Vegetables 15; Berries 6; Mushrooms 8; Chips 10; Chocolate 12; Soda thirst
  20; Herbal Tea thirst 25, warmth, panic -12.
- **Tin cans** are left by Canned Food (data field `leaves`).

### 6. Living off the land

- **Foraging.** In Wilderness district tiles and on tree, bush and tall-grass
  scenery, E on a bush or at a tree base searches it (3 s): by a hash of
  tile and day, Berries (bushes, spring to autumn), Mushrooms (tree bases,
  rain in the last 2 days), Herbs (tufts, any time), or nothing (60 %).
  A spot gives again after 3 game days (per run; not world memory).
  One Mushroom in four is bad (illness +25) unless Living off the land is 2.
- **Snares.** F places a Snare on grass outdoors, at least 15 tiles from any
  building. Each dawn it has 25 % (50 % at skill 3) to hold Raw Meat, 10 %
  to be sprung empty; a zombie crossing it breaks it. E collects and resets.
- **Garden beds** (build wheel): 1 Seed Packet makes a bed that yields 4
  Vegetables after 6 game days if watered (R with a Water Bottle, or any
  rain) at least every 2 days; dry for 3 days: it dies. It then regrows
  from its own seed every 5 days while kept watered. Drawn in four stages.
  The dead trample a bed they walk through (50 % to reset it to stage 1).
  World memory: a bed persists to the next life as seedlings.
- **Rain barrels in the wild.** None. Water is the collector's job.

### 7. Carrying

- Backpack: +10 kg while carried (one counts). Shown in the weight line as
  "18.4 / 35 kg".
- Dropping: Backspace in the pack, on the cursor item, drops it on the floor as a pickup (stack with Shift). Items on the
  floor persist for the run (checkpoint) and across lives only inside
  buildings (world memory: floor items indoors).
- The pack's detail line names where things are useful ("Fits: flashlight,
  radio").

### 8. The radio

- With a Battery (20 hours of listening): F toggles it on. On: once per game
  hour it gives the weather for the next 12 hours in one line ("Rain by
  evening. Colder tonight.") from the weather RNG's upcoming states (expose
  `WeatherSystem::forecast(hours)`); in shelter it plays faint music (P12)
  and lifts depression (P05). It makes Soft noise.
- It carries the campaign broadcast in P09.

### 9. The town runs out, visibly

- At the start of each life, count containers still stocked per district;
  the town map tints districts by what is left (four steps). The notebook's
  totals add "Cupboards still unopened: 212 of 640."
- Scavenging skill level 2's "worth searching" mark uses searched state
  only (never reveals contents).

## Later

- Fishing at ponds.
- Generators for a house (the gate's generator in P09 is the only one).

## Files

New: `src/world/BuildingKinds.*`, `src/world/Power.*` (rules: when it fails,
flicker schedule), `src/world/Forage.*`, `src/world/Garden.*`,
`src/core/GameScavenge.cpp`, `src/core/GamePower.cpp`,
`tests/test_buildings.cpp`, `tests/test_power.cpp`, `tests/test_forage.cpp`,
`tools/harness/town_kinds.txt`, `break_in.txt`, `power_out.txt`,
`cooking.txt`, `garden.txt`.
Changed: `LayoutData.h`, `MapGenerator` (kinds only), `Furniture.*`,
`FurnitureView`, `StructureView` (signs), `Doors.*` (locks), `GameDoors.cpp`,
`loot.json`, `items.json`, `recipes.json`, `GameInventory.cpp`,
`GameUi.cpp` (map pictograms, Make at a station), `FirstDays` (plans),
`SaveManager` (version 3), `generate_sprites.py`, harness (`goto kind NAME`,
`power on|off`, `forage`, `grow DAYS`, `locks`).

## Acceptance

- `town_kinds.txt` on seeds 1234, 42, 777: each town has at least one
  Pharmacy, Grocery, Hardware, Clinic, Police and Garage (or logs why not);
  screenshots of each kind's street front show its sign; interiors are
  walkable end to end (the furniture reachability test runs for every kind).
- `break_in.txt`: a locked pharmacy: E says "Locked."; a crowbar opens it in
  6 s and the alarm rings; wanderers within 30 tiles turn; the box stops it.
  After `power off`, no alarm.
- `power_out.txt`: `day 5`, `hours 18.9`, wait: the line appears, street
  lamps go dark (screenshot before and after), fridge food starts spoiling
  at full speed (measure condition over 2 hours).
- `cooking.txt`: Dirty Water + pot at a campfire gives a Water Bottle; Rice
  cooks; raw meat sickens, cooked does not.
- `garden.txt`: a bed planted, watered, `grow 6`: 4 Vegetables; dry 3 days:
  dead. Next life: seedlings.
- A version-2 save loads, houses unchanged, shops refurnished, no crash.
- `bot_week` x10 with the bot allowed to range: days survived rises against
  phase 5 (there is now more to find); deaths from thirst after day 3 fall.
