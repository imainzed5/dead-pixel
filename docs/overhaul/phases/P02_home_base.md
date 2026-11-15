# Phase 2: the home base

**Goal.** Make the house a place the player shapes with their hands and
defends with what is in it, and make that work outlast them. This is the
"Wave 5" the user chose earlier.

**The player notices:** you can shove the wardrobe against the door, break a
table into planks, board windows, hang cans that rattle, build a workbench,
and the next character finds all of it.

**Feel target.** Late afternoon on day 2: the player looks round the house
and decides what to sacrifice. The table becomes planks. The bookcase goes
against the back door. It is a little sad and it is theirs.

## Core

### 1. Retire the old build mode

- Remove the B-key build mode and its six flat props (`BuildSystem` placement
  and ghost, `renderBuildModeOverlay`, the `env_*` prop sprites it used).
  Keep `Structure` as the component for built things and keep the structures
  save file.
- Saved structures of the old kinds: Barricade and NoiseTrap are dropped on
  load; Campfire, SleepingArea, SupplyCache and RainCollector are loaded as
  the new Campfire, Bedroll, Storage Crate and Rain Collector.
- B now opens the **build wheel** (section 6).

### 2. Furniture as material

New rules module `src/world/FurnitureUse.h/.cpp` (pure rules, tested) and
game slice `src/core/GameBase.cpp`.

Each `FurnitureKind` gets, in the `kInfo` table: `movable` (bool), `weight`
class (light, heavy), `planks` (0 to 4), `breakSeconds`, `health`.

| Pieces | Movable | Planks | Break s (hands / hammer / axe or crowbar) | Health as a barricade |
|---|---|---|---|---|
| `TvStand`, `Plant`, `Crates` | yes, light | 1 (`Plant` 0) | 8 / 5 / 3 | 150 |
| `Table`, `Bookshelf`, `Wardrobe`, `Dresser`, `StoreShelf` | yes, heavy | 2 (`Wardrobe`, `Bookshelf` 3) | 14 / 9 / 5 | 350 |
| `Couch` | yes, heavy | 1 + 2 Rags | 14 / 9 / 5 | 300 |
| `Bed` | yes, heavy | 2 + 2 Rags | 14 / 9 / 5 | 250 |
| `Fridge`, `Stove`, `MetalRack` | no | 0; with a Crowbar, 20 s: 1 Scrap Metal, the piece is gone | - | - |
| `Counter`, `Sink`, `Toilet`, `Checkout`, `Workbench`, `Barrels` | no | 0 | - | - |

These are the 19 kinds in `Furniture.h` today. Pieces added later (P06)
state their own row there. Breaking the bed of a home removes its sleep
spot; the day plan's "Sleep in your bed" then accepts a Bedroll.

**Pushing.** Stand next to a movable piece, hold E for 0.4 s (a tap still
searches it), then walk into it: it slides one tile at a time in the
direction you face if that tile is free floor (not wall, door frame, other
furniture, a body or a person). Light pieces move at 60 % of walk speed,
heavy at 35 %; pushing costs 6 stamina per tile (heavy 12) and makes Medium
noise per tile (heavy pieces) or Soft (light). You cannot push while over
the carry limit. Release E to let go. The piece keeps its contents.

**Blocking a door.** A piece standing on the tile just inside a doorway (the
floor tile a door opens onto) blocks the doorway: nobody passes, the dead
must break the piece (it takes door-battering damage after the door and its
boards are gone; its health is in the table). The player can still climb
over a light piece (1.2 s, Soft noise); heavy pieces must be pushed away.
`Pathfinder` treats the doorway as a closed door with extra cost 40.

**Breaking up.** X next to a piece (not while its container holds items:
"Empty it first.") starts a held action with a progress line; it makes Loud
noise each second; on finish the piece is gone and the planks (and Rags) drop
into your pack, overflow onto the floor. A Saw in the pack adds 1 plank to
pieces that give 2 or more. Carpentry XP later (P05).

**Memory.** Furniture moved or broken is world memory: a list of
`{fromTile, toTile | removed}` in a new save file `<slot>_furniture.json`,
applied after `furnishWorld`. The fixed layout stays the source; the file
holds only differences. Checkpoints save it too.

Tests: push rules (blocked tiles, doorway, stamina), break yields by tool,
door blocked by furniture in the pathfinder cost, save round trip, apply
moves on top of a fresh furnishing, "every room stays reachable" is no
longer guaranteed after the player moves things (that is the point), but the
initial furnishing guarantee test stays.

### 3. Windows

Today windows are painted on outside walls every few tiles
(`StructureView`). They become real.

- `TileMap` gains a window layer like the door layer: `WindowState { None,
  Intact, Broken }` plus boards 0 to 2 and board health. Windows are the wall
  tiles `StructureView` already draws a window on (`t.window` in
  `StructureView.cpp`: an outside wall flanked by walls, not next to a door,
  every third tile, three in four by tile hash); move that choice into a
  shared rule (`structures::hasWindow`) that fills the window layer when the
  town is built, so drawing and rules agree and a seed keeps its windows.
- An intact window blocks movement and noise like a wall but **not sight**
  (both ways, 6 tiles through it). The dead who see you through a window
  come to it and batter it: glass has 60 health; breaking is Loud and drops
  Glass Shards outside.
- A broken, unboarded window can be climbed by the player (E, 1.5 s, Soft
  noise; a cut 30 % of the time unless you carry Rags or a Jacket to lay
  over it, which is used up) and by the dead (4 s, one at a time).
- The player can break a window: attack it (Loud), or crouch + E with Duct
  Tape for a quiet break (Soft; uses the tape; P06 adds the tape).
- Boards: R at a window from inside, as with doors (1 Plank + 2 Nails,
  hammer, up to 2, 300 health each). X pries one off. Boarded windows block
  sight.
- Curtains: E at an intact window from inside toggles "covered": blocks sight
  both ways. Free, quiet. Light from inside (fire, candle, flashlight) shows
  through an uncovered window at night and counts as "lit" for the dead
  within 10 tiles outside.
- Sprites: broken window, boarded window (1 and 2 boards), curtained window,
  in each wall material and both wall runs; cut-away stubs for each. Add to
  `make_wall_piece` in `tools/generate_sprites.py`.
- Memory: broken and boarded windows persist like doors. Checkpoints too.
- Boards on doors now also cost 2 Nails each. The home kit gains 12 Nails.
  Prying a board gives back the plank; nails come back only with a Crowbar.

### 4. Night callers use the whole house

- A caller picks the weakest way in among the home's doors and windows within
  12 tiles of where it arrives: lowest (boards x health + blocking furniture
  health), with glass counting 60. Ties go to the nearest.
- The day plan's "Board up the doors (n/m)" becomes "Close up the house
  (n/m)" and counts doors and ground-floor windows that are boarded, blocked
  by furniture, or (windows) at least curtained. Day 1 asks for doors
  boarded and windows curtained; day 3 for everything boarded.
- `plan` harness output lists windows.

### 5. Traps and lights (placeable items)

Recipes and stats are in `03_content_tables.md` sections 3 and 9.

- **Can Alarm:** F with it selected, at a door or window: hangs it. When that
  door or window is battered, opened or climbed by anyone but you, it rattles:
  Loud noise for 3 s, wakes you from anywhere in the building, and is spent
  (drops 2 Tin Cans). You can step past your own alarm only by taking it
  down (E).
- **Spike Board:** F on an indoor floor tile: the first three of the dead to
  cross take 35 damage and are slowed 50 % for 4 s; then it breaks. It hurts
  you too (10 damage, a cut) unless you crouch over it.
- **Candle / Can Lantern:** F places it; Matches light it. Candle: light
  radius 2.5 tiles for 6 game hours, seen through windows. Can Lantern:
  radius 2, 8 hours, not seen through windows. Both are fire sources (a
  knocked candle, when a zombie crosses its tile, starts a fire 20 % of the
  time on a flammable floor).
- Placed things are entities with `Structure` (new types `CanAlarm`,
  `SpikeBoard`, `Light`) and are saved with structures.

### 6. Stations and the build wheel

B opens a small radial list at the cursor (bare-hands style: plain text
entries around the point, the chosen one in amber when it lacks materials,
ink when it can be built, with its cost under it). Entries: Campfire,
Workbench, Storage Crate, Rain Collector, Bedroll (Garden Bed joins in P06).
Costs, tools, times and health are in `03_content_tables.md` section 9.

- Choosing an entry shows a ghost on the tile under the cursor within 2
  tiles; left click starts a held build with a progress line and Soft noise
  every second (hammering: Loud each second for Workbench and Crate).
  Moving or being hit cancels and refunds.
- **Workbench:** counts for `station: workbench`; also lets you repair a
  weapon (E at the bench with a weapon in hand: 1 Scrap Metal or 2 Nails or
  Duct Tape restores 40 % wear, 8 s).
- **Storage Crate:** a container, 16 slots, 60 kg.
- **Rain Collector:** outdoors only. Holds up to 6 bottles' worth; fills at 1
  bottle per game hour of rain (2 in heavy rain, storm). R at it fills empty
  bottles, as at a tap. Water from it is clean.
- **Campfire:** needs Matches to light (not used up 80 % of the time); burns
  4 game hours per 2 Planks added (E adds); warmth as today; a `station:
  fire`; light radius 5; Medium noise every 5 s. Indoors on wood or carpet it
  is a fire risk (5 % per game hour to spread). Rain puts an outdoor fire
  out.
- **Bedroll:** a sleep spot, quality 0.75, 7 hours.
- Wear: built things lose health as today (`decayRatePerHour`); rain doubles
  it for wood outdoors; R with the build materials repairs.

### 7. Sleep and safety

Sleep quality now depends on the room, as the design document asks:

| Where | Quality | Wake chance per caller within 12 tiles |
|---|---|---|
| Outdoors | 0.4 | always |
| Indoors, a way in is open (open door, broken unboarded window) | 0.6 | always |
| Indoors, all ways in closed | 0.8 | when a door or window is hit |
| Indoors, all ways in boarded or blocked | 0.95 | when a board breaks or an alarm rattles |
| ...and in a bed | +0.05 (max 1.0) | |

The sleep prompt says which: "Sleep (the back window is open)".

### 8. The base carries over

- At run end, everything above persists by the rules already stated
  (furniture moves, windows, boards, alarms not sprung, crates and their
  contents, workbench and collector one stage worse).
- **A new character starts in a different home** (today: one of the six
  nearest homes by run seed) but the legacy card and notebook list
  "A house someone held: 14 tiles north-east" for the last home that had 3 or
  more closures or a station. It is a lead like a grave or a pack
  (`legacy::LeadKind::Base`).
- 30 % of the time (run seed), and always if it was the previous character's
  home and they died away from it, the new character starts in that house.
- Depression: finishing a station or fully closing the house gives -6
  ("accomplishment"), once per game day.

## Later

- Metal barricades (Scrap Metal boards: 700 health, do not burn).
- Door brace (a single plank wedged: 2 s, 120 health, no tools).

## Files

New: `src/world/FurnitureUse.h/.cpp`, `src/world/Windows.h/.cpp` (rules, as
`Doors`), `src/core/GameBase.cpp`, `src/core/GameWindows.cpp`,
`tests/test_base.cpp`, `tests/test_windows.cpp`,
`tools/harness/base.txt`, `tools/harness/windows.txt`.
Changed: `Furniture.*`, `FurnitureView`, `StructureView`, `TileMap`,
`Pathfinder`, `BuildSystem` (gutted to stations), `FirstDays`,
`GameFirstDays.cpp`, `GameDoors.cpp`, `ZombieAISystem` (window targets,
furniture in doorways), `SaveManager`, `LegacyRules`, `GameUi.cpp`,
`generate_sprites.py`, harness (`push DIR`, `break`, `goto window`,
`windows`, `build KIND`).

## Acceptance

- `base.txt`: push a wardrobe onto the back-door tile; a spawned zombie
  (`zombie`) batters the door, then the wardrobe, and takes more than 60 s of
  game time in all; break a table with a hammer and get 2 planks; build a
  crate, stash items, `kill`, `newrun`, `goto base`: the wardrobe is still
  at the door and the crate holds the items. Screenshots: `base_pushed`,
  `base_wheel`, `base_next_life`.
- `windows.txt`: a zombie outside sees the player through a window at night
  with a candle lit and comes; with the curtain closed it does not; it breaks
  the glass, climbs in; boards stop it for at least 40 s; broken and boarded
  windows survive `save`/`resume` and the next life. Screenshots of each
  window state in two wall materials, inside (cut-away) and outside.
- `night_siege.txt` and `first_day.txt` updated for nails and the new plan
  wording, and passing.
- The bot boards windows too; `bot_week` over 10 seeds shows night 1 deaths
  at home in at most 1 of 10 runs.
