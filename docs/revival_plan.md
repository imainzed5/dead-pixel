# Dead Pixel Survival — Revival Plan

Started: 2026-11-05 · Branch: `revival` · Owner: Claude (autonomous until handoff)

This is the living plan for reviving the project after a seven-month pause. It
replaces the old milestone backlog as the source of truth for what happens next.
The GDD (`dead_pixel_survival_gdd_v0.3.md`) is still the destination.

## Direction (decided)

- **Goal:** ship-quality. Build as if it will be released (itch.io demo first).
- **Hook:** the fixed world that remembers past runs. Graves, decayed bases and
  districts that have turned since last time, on top of Zomboid-style simulation.
  Every change is tested against this.
- **Scope:** keep every existing system and deepen them in waves, ordered by
  closeness to the hook:
  1. Core loop: scavenging, needs, noise and zombies, combat, readability.
  2. Legacy layer: graves, persistent bases, world memory, bloodlines.
  3. Extras: NPCs, vehicles, fire, crafting, modes, tone presets.
- **Camera:** isometric stays. Readability work covers wall/roof occlusion and
  diagonal-facing characters.
- **Tech:** no engine switch. Clean-up happens alongside features.

## Working rules

1. Nothing counts as done until it is seen working: a harness screenshot or
   metrics run, not just a successful build.
2. Every phase ends with a build, the tests, a harness smoke run and a commit.
3. `Game.cpp` only shrinks. New behaviour goes into systems or UI modules.
4. Saves stay compatible within the revival, or bump `saveVersion` on purpose.

## Phases

| # | Phase | Status |
|---|---|---|
| 0 | Branch, plan, CI fix | done |
| 1 | Dev harness: scripted autoplay, offscreen screenshots, metrics, input-latch fix | done |
| 2 | Baseline screenshots and assessment | done |
| 3 | Test target: core library, save round-trip and logic tests | done |
| 4 | Wave 1a: simulation pressure (mental, needs/temperature, zombie pressure) | done |
| 5 | Wave 1b: readability (scale/zoom, HUD rework, night lighting) | done |
| 6 | Walls with height, wall/roof occlusion, art generator upgrade | done |
| 7 | Split remaining `Game.cpp` (vehicles, fire, persistence duplication) | done |
| 8 | Wave 2: make the legacy hook visible | done |
| 9 | Wave 3: extras polish, tone presets with real effects | |
| 10 | Docs, README and audit sync | |
| 11 | Final verification and handoff | |

`Game.cpp` is split continuously: each wave moves the code it touches out
(e.g. the HUD rework extracts the HUD renderer).

## Baseline assessment (2026-11-05)

From the harness tour (`tools/harness/baseline_tour.txt`) and a two-day idle
soak (`tools/harness/soak_idle.txt`, world seed 1234):

**Look and readability**
- At default zoom the player is ~20 px tall; items are drawn as big as people.
  Humanoid sprites are authored at 64 px but rendered at 32 px.
- Buildings read as floor plans: walls are low studded tiles the player stands on.
- Ground is speckled with random dirt/stone/water diamonds that mean nothing.
- HUD: unlabeled bars, overlapping HP text, a column of small debug-style text,
  a large mode/threat banner, a hotbar of tiny text. Inventory is a bare text
  list over the world with no panel.
- Night darkens everything evenly. No light around the player or from windows.

**Simulation (idle soak, 2 game days)**
- 44 zombies, none came within 10 tiles in two days. No ambient pressure.
- Depression reaches the cap by 17:00 on day 1 from "isolation" (no zombie in
  sight gives +0.2/s). The 24 h crisis never starts because decay runs after the
  clamp, leaving depression at 99.998 < 100.
- Body temperature sits at 30 °C every night, indoors, with no consequence.
- Panic stays at 0 alone in the dark.
- HP stays at 100 until thirst hits exactly 0, then death within an hour.
  No gradual warning effects.
- A kill was credited with no player action: any zombie death counts as a kill.
- Idle survival ≈ 46 game hours, ended by thirst.

**Engine**
- Harness runs ~4,000 ticks/s hidden (2 game days in 43 s, Release).

## Wave 1a: survival model (2026-11-05)

Rates are per real second; 60 s = 1 game hour, 1440 s = 1 game day.

**Zombie pressure** (`ThreatDirectorSystem`, `ZombieAISystem`)
- Wandering is a correlated random walk with per-zombie RNG, idle pauses and
  wall bounces; long trips follow A* paths.
- Threat director, after a 150 s grace period: when fewer zombies than "desired"
  are within 14 tiles (day 1 + 0.5/day, night 3 + 1/day), it sends 2 (day) or 3
  (night) far wanderers toward a point 6–14 tiles from the player, every 70 s by
  day and 35 s by night.
- Population grows toward initial × (1 + 0.18/day) × (1 + infection rise) ×
  mode scale, spawning at least 24 tiles from the player, weighted toward
  infected districts. Threat modes scale migration and population.
- Attacks are telegraphed lunges: 0.55 s wind-up, hit only if the player is
  still in reach, 60% hit chance, 7 damage, 1.5 s cooldown. A lone zombie needs
  about 50 s to kill a player who does nothing. 35% of hits wound; 30% of
  wounds are bites and half of those are dirty.

**Needs and temperature** (`NeedsSystem`)
- Thirst empties in ~37 game hours, hunger in ~67, sleep debt reaches "tired"
  (70) after ~20 waking hours. Sleep halves the drain and pauses debt.
- Penalties ramp: stamina cap first (hungry/thirsty, parched/starving, cold,
  sick), then illness, then slow HP loss (dehydration ~8 h to death, starvation
  ~16 h, grave illness ~40 h).
- Body temperature drifts toward a target set by the air it feels: outdoor
  temperature, +10 under a roof, −4 for rain and −4 for wind outdoors, +18 near a
  fire or campfire, +5 with a jacket in the pack, plus heat from walking or
  running. Spring nights outdoors are chilly; rainy autumn nights are cold;
  winter nights outdoors are hypothermic and need a fire even indoors.
- Bites heal in 18 h, cuts in 6 h, bruises in 2 h. A dirty bite left untreated
  for 3 h becomes infected: +3 illness per game hour until antibiotics.

**Mind** (`MentalStateSystem`)
- Panic comes from zombies in line of sight within 7 tiles (hunters count fully,
  unaware wanderers 35%), low HP, and being outdoors at night (rises to a floor
  of 20, never higher). At 90+ with a hunter in view the character screams.
- Depression: +14/day alone, +3/night awake, +29/day per unmet need (hungry,
  thirsty, tired, cold, sick). Relief: −17/day sheltered with needs met, −72/day
  of company, −8 per full night's sleep.

**Idle soak results** (two days standing still, seeds 1234 / 42 / 777): found
and killed at 14:00 day 1 / 21:00 day 1 / survived one encounter, then died of
thirst at 07:30 on day 3. Idle play is never safe for long, but the timing
varies.

## Wave 1b: readability (2026-11-05)

**Scale and light**
- Characters draw at 64 px (pickups 24 px); default zoom 1.5, minimum 0.75.
- Night: ambient drops to 20% with dusk and dawn tints; light pools are
  ellipses on the iso ground; the player carries a faint personal light;
  campfires and fires flicker. Q toggles a flashlight cone (if carried) that
  doubles how far zombies can see you at night.

**HUD** (`src/ui/Hud.h/.cpp`, built from a `HudState` snapshot each frame)
- Top left: health and stamina bars with values. The part of the stamina bar
  that needs have taken away is drawn dark red.
- Below it, conditions in the style of status icons: only problems are listed,
  worst first, colored by severity (info, mild, serious, critical; critical
  pulses). Thresholds come from `NeedsSystem`, with early warnings (Peckish,
  Thirsty, Drowsy) shown before penalties start.
- Top right: day, clock (blue at night, amber at dusk), weather and outdoor
  temperature, and the infection of the district you stand in. Below it, one
  objective: the most urgent thing to do next (medical, water, food, sleep,
  search, or the campaign goal).
- Top center: a toast when you enter a new district.
- Bottom center: hotbar with item icons, counts and the selected item's name,
  with messages and the interaction prompt above it. Bottom left: car readout
  when driving or near a car.
- Red vignette below 40% health.
- Run mode, threat, tone, kills, loot and traits moved to the pause panel.
- Inventory (Tab) is a 4×5 slot grid matching the hotbar, with item details,
  freshness strips and a carry weight bar. Controls (F2) is a centered panel.
- Harness: `set STAT VALUE` forces player stats; `tools/harness/hud_states.txt`
  screenshots a fresh, a stressed and a night character.

## Walls, roofs and the town (2026-11-06)

**Structures** (`src/rendering/StructureView.h/.cpp`)
- Wall tiles draw as 56 px blocks (`kWallHeight`, must match `WALL_H` in the
  generator) that sort with entities by iso depth, so you can stand behind a
  wall. Each wall knows its run (along x, along y, corner), material (plaster
  houses, brick shops and the map edge, metal industrial) and whether it is on
  a building's outside edge. Outside walls get windows every few tiles;
  doorways get a frame on the face they open through.
- Each building has a flat roof (shingles, tar, ribbed metal) over its
  interior. It fades out while you are inside or standing at one of its
  doorways, and everything under a closed roof (zombies, loot) is not drawn.
- Cut-away: walls of your own building that are in front of you drop to 8 px
  stubs; outside, walls in a narrow band right in front of you do the same.

**Map** (`MapGenerator`, `src/map/TileIds.h`)
- Ground ids gained wood, tile floor, asphalt and sidewalk; each ground id has
  four textured variants picked per tile. Roads are asphalt with sidewalks.
- Rooms get one floor each by building type: houses wood/carpet/tile, shops
  tile or concrete, industrial concrete or metal plate.
- Buildings are closed: notch corners are walled, doors are only placed where
  there is floor behind them, and interior walls never block a doorway.
- Outdoor decor comes in clusters: dirt patches, small ponds, darker grass.

**Art** (`tools/generate_sprites.py`) draws the tileset (1024×128) and the wall,
window, doorway, stub and roof sprites; the old atlas content is unchanged.

**Harness:** `enter N` teleports into building N; `tools/harness/interiors.txt`
screenshots several interiors by day and night.

**Tests:** `tests/test_structures.cpp` covers wall classification, roof
open/fade/hiding, the cut-away rule, sprite collection and the generator
guarantees (closed buildings, doors that lead inside, one floor per room,
sidewalks along roads).

## Game.cpp split (2026-11-06)

`Game.cpp` (9,100 lines) is now ten files in `src/core/`, each a slice of the
`Game` class:

| File | What it holds |
|---|---|
| `GameShared.h` | constants, panel layouts and small helpers (namespace `gameshared`) |
| `Game.cpp` | startup, runs, saving, input plumbing, small queries |
| `GameScene.cpp` | building the world for a run (map, entities, loot, legacy traces) |
| `GameUpdate.cpp` | `update()` (now ~690 lines) plus sleep, animation, particles, noise |
| `GameMenus.cpp` | title screen and pause menu input |
| `GameInventory.cpp` | inventory, container, hotbar and crafting panel input |
| `GameFire.cpp` | lighting/putting out fires, burn and spread simulation |
| `GameVehicles.cpp` | getting in and out, refuelling, driving, fuel, wear, run-overs |
| `GameRunEnd.cpp` | death and retirement and what the next run inherits |
| `GameRender.cpp` / `GameUi.cpp` | world rendering / HUD snapshot and menu panels |

Death and retirement share one persistence path (`persistWorldForNextRun`).
Two fixes came with it: burn scars from earlier runs are no longer re-logged
as new world-memory events at every run end, and burn events carry the ending
run's index like graves and history do. Harness: `kill [cause]` and
`retire [cause]` end the run the way the game does.

## Wave 2: the legacy hook (2026-11-06)

The hook is a fixed town that remembers earlier runs. The save already held
that memory, but the game barely showed it: a grave sprite, a cairn, and a
notebook listing events by tile coordinates. Now every earlier life leaves
something the next one can find and act on:

| How the life ended | What later runs find |
|---|---|
| Zombies, or anything with an infected bite | They **rise**: a named, tougher zombie that stays near where they fell. Put them down and they get a grave, with their pack beside it. |
| Anything else | A **grave** with their epitaph, and their **pack** (everything they carried) until someone empties it |
| Retiring | A cairn, and they **live on** as a cautious survivor who keeps to themselves |

- **Run-start card:** "Run 4 - three came before you", then the nearest
  leads with distance and direction. Directions are on-screen directions
  (N is up), the same as the map's.
- **Name labels** over the risen, graves and retired survivors at close range.
- **Town map (M):**
  - isometric, oriented like the game;
  - buildings drawn as floor plans, plus roads, burn scars and infected
    districts;
  - named legacy markers and a legend.
- **Notebook (J):**
  - *What is out there*: live distance and direction to each lead.
  - *Who came before*: newest first, marked "walks again" or "at rest".
  - Totals across lives.
- **Minimap** dots use the same colours as the map and notebook
  (`legacyLeadColor`).
- **Start homes:** each run starts inside one of the homes near the town
  centre, chosen by the run seed.
- **Container panel:** restyled. The header no longer overflows, and the text
  uses an integer scale.
- **Code:**
  - The rules live in `src/legacy/LegacyRules` (who rises, lead order and
    text) and are unit tested.
  - The entities live in `GameLegacy.cpp`.
  - Grave records gained `items`, `risen` and `laidToRest`, with a save
    round-trip test.
- **Harness:** `slay R` and `goto risen|pack|grave`. The script
  `tools/harness/legacy.txt` plays four lives and captures each step.

## Wave 3: polish (2026-11-06)

- **Run-end screen:** death and retirement share one screen:
  - who they were and how it ended, with traits and run stats;
  - **what they leave behind**: "They will not stay down", "Their pack will
    wait by the grave" or "They will live on in town";
  - a grave or cairn icon, a fade-in, and a slim bar while the restart is
    held back.
- **Tone presets now change the run** (`src/systems/ToneTuning.h`, tested).
  GRIM HUMAN is the baseline.

  | Tone | Supplies | Weapons | Zombies at start | Zombie health | Nights | Colour | Stamina regen |
  |---|---|---|---|---|---|---|---|
  | HOPELESS | 70% | 80% | 100% | 115% | darker | drained | 90% |
  | BRUTAL ARCADE | 115% | 150% | 135% | 70% | brighter | punchier | 130% |

  Spawn cuts now pick a random subset, so they spread over the town instead
  of emptying the last-listed buildings. On the title screen the town behind
  the menu previews the tone's colour.
- **Title screen:**
  - big title, the menu on the left, and a line explaining the selected
    option (threat and tone descriptions included);
  - a "The town remembers" card: lives, graves, risen still walking, packs
    waiting, survivors;
  - the cursor skips "Continue Run" when there is nothing to continue;
  - "Options" is now named for what it opens: "Controls".
- **Doors** (`src/world/Doors.h`, tested):
  - Every doorway now holds a door, closed at the start of a run. A closed
    door blocks movement and sight like a wall.
  - Press **E** at a door to open or close it. It will not shut on someone
    standing in the doorway.
  - Zombies chasing or investigating something behind a closed door path to
    it and batter it. Each bang draws more of them, and after about 15
    zombie-seconds the door gives way: a loud crash, "A door gives way.",
    and the doorway stays open for the rest of the run.
  - Paths treat a closed door as expensive (cost 25), so the dead only batter
    the door that stands between them and you, not every door on the way.
  - Survivors open doors as they pass and leave them open.
  - **The town remembers broken doors**: a door the dead broke down is still
    broken in every later run. Mid-run checkpoints keep open and broken
    doors, this run's burn scars and burning fires.
  - New sprites: closed and broken doors in each wall material. In cut-away
    view, a closed door shows as a low wooden stub.
  - Harness: `zombie DX DY`, `doors`, `goto door`, `save` and `resume`; the
    scripts are `tools/harness/doors.txt` and `tools/harness/doors_memory.txt`.
- **Crafting screen rework:**
  - A centred panel in the notebook's style, with whole-number text scales,
    item icons and category tabs. Mouse hover and click share one layout with
    drawing, and hover follows the mouse only when it moves, so W/S keep working.
  - Undiscovered recipes read "Fuel Can + ?" and give a clue to the other half
    ("something that burns"). Each recipe has a clue for each ingredient
    (`hintA`/`hintB` in `recipes.json`).
  - Each recipe's needs (ingredients, tools, workbench) are listed with have /
    need counts. The panel shows progress, a craft button, "Missing: X" and
    recent messages.
  - Recipes written in a notebook appear in the J notebook under "What we
    know". Picking up a notebook writes down what this life already worked out.
  - Crafting takes 4 to 10 s instead of 40 to 90 s.
  - Crafted items now earn their cost:
    - Disinfected Bandage dresses two wounds and kills infection.
    - Spiked Bat hits 35% harder, lasts 50% longer and causes bleeding
      (per-item weapon scales in `items.json`).
    - New recipe: **Spear** (Knife + Wood Planks, Rope as a tool), with longer
      reach and lighter blows.
  - Harness script: `tools/harness/crafting.txt`.
- **Item icons:** every item has its own 32x32 icon in one style (solid
  shapes, light from the top-left, 1px dark outline). There are 16 redrawn
  icons and 13 new ones, from `add_item_icons` in `generate_sprites.py`.
  Nothing else in the atlas changes.
- **Furniture** (`src/world/Furniture.h`, `src/rendering/FurnitureView.h`, tested):
  - Every building is furnished from its layout, the same in every run.
    Rooms get a role from their floor and size: kitchen, bathroom, bedroom,
    living room or a one-room home; shop floor or back room; workshop.
  - 19 kinds of furniture, drawn as 64x80 isometric sprites that sort with
    walls and characters. Pieces with a front (fridge, bed, shelves...) stand
    with their back to a north or west wall; counters, shop shelves and racks
    run along the wall. Tall pieces fade when they would hide the player.
  - Furniture blocks movement but not sight, shots or fire, and it burns more
    readily. Doorways and the tiles around them stay clear, every room stays
    walkable end to end, and every piece can be reached (tests check this).
  - Most pieces are containers ("Search fridge") with loot that fits them:
    food in fridges, medicine in bathroom cabinets, tools in workshops. About
    half are bare (tone scales it). Searched furniture stays empty in later
    runs. Beds are sleep spots (8 h, better rest than a sleeping bag);
    sleeping bags now only appear in buildings without a bed.
  - The old stand-in props (flat crates, shelves and beds) and the district
    "loot container" pass are gone; indoor blood stains remain. Run spawns
    that land on furniture move to a free tile beside it.
  - An empty container says what it is: "Search fridge (empty)", "The fridge
    is empty.", and the panel header names it.
  - Harness: `goto furniture [KIND|any] [stocked]`; the script is
    `tools/harness/furniture.txt`.
- **Weapon in hand:** the weapon you fight with is an inventory item. Runs
  start with the knife in hand (it used to sit in the hotbar while you fought
  with default "improvised" stats). Wear is kept in the item, so taking a
  weapon out again does not mend it, and checkpoints remember which one was
  in hand. Weapon slots show a wear bar and a gold corner on the one in hand;
  the name reads "Knife - worn - in hand". Script:
  `tools/harness/weapon_wear.txt`.

## Known issues found during revival

- ~~Input transitions cleared per render frame, not per tick~~ (fixed: latched
  per tick).
- ~~Any zombie death counted as a player kill~~ (fixed: kills credit the last
  damage source).
- ~~Bites could never infect: wounds healed before the infection timer~~ (fixed).
- ~~Title screen re-read the checkpoint file every tick~~ (fixed: refresh on
  input or once a second).
- ~~Buildings have open doorways and no doors, so no shelter can be closed off~~
  (fixed: doors).
- ~~HUD status thresholds predated the Wave 1a thresholds~~ (fixed: the HUD reads
  them from `NeedsSystem`).
- ~~The crafting screen lists undiscovered recipes as "???" with no hint~~
  (fixed: "Item + ?" with a clue).
- ~~Several items reuse other items' sprites~~ (fixed: one icon per item).
- ~~Tone presets are stored and shown but have no gameplay effect~~ (fixed:
  `ToneTuning`).
- ~~`beginDeathState` and `beginRetirementState` duplicate persistence code~~
  (fixed: shared `persistWorldForNextRun`).
- ~~Mid-run checkpoints do not save fires or burn scars~~ (fixed: checkpoints
  keep doors, scars and fires). Zombies are still respawned on resume.
- ~~The crafting screen is a bare debug-style layout~~ (fixed).
- ~~Equipping a weapon from the inventory resets its durability to full~~
  (fixed: wear lives in the item).
- ~~No wall/roof occlusion: indoor zombies and loot can hide behind walls~~
  (fixed: wall blocks, cut-away and roofs).
- ~~Building interiors have no furniture; loot lies on bare floors~~ (fixed:
  furniture).
- ~~Zombies killed by fire or gunshots stayed standing, frozen, and blocked
  sleep nearby~~ (fixed: every dead zombie is cleared away).
- Medical and supply caches still use the old flat crate and shelf sprites.
- Character sprites face only up/down/right; isometric movement is diagonal.
- ~~Notebook, world map and minimap text rendered as garbage~~ (fixed: each
  UI layer draws its shapes with the sprite sheet, then all of its text with
  the font atlas).
- ~~Container panel header overflowed its box; hint text barely visible~~
  (fixed).

## Log

- 2026-11-05: Reviewed project, committed shelved M8–M10 work (`11926fb`),
  fixed CI dependencies (`5183a9c`), opened `revival` branch.
- 2026-11-05: Dev harness and per-tick input latching (`16b7400`); unit test
  runner and depression-crisis fix (`8d8590f`).
- 2026-11-05: Wave 1a survival model: threat director, zombie AI rewrite,
  needs/temperature/panic/depression retune, telegraphed attacks, wound fixes,
  kill attribution, building layer on the tile map.
- 2026-11-05: Wave 1b scale, default zoom, night lighting and flashlight
  (`76d40cb`).
- 2026-11-06: Wave 1b HUD rework: `Hud` module, conditions list, clock and
  objective panel, icon hotbar, district toast, inventory grid, controls and
  pause panels, harness `set` command.
- 2026-11-06: Walls with height, roofs and cut-away (`StructureView`), tileset
  variants, room floors, asphalt roads with sidewalks, closed buildings,
  harness `enter` command.
- 2026-11-06: `Game.cpp` split into ten files; `update()` carved into named
  steps; shared run-end persistence; burn-scar event fix; harness `kill` and
  `retire`.
- 2026-11-06: Wave 2 legacy hook: risen characters, packs, retired survivors,
  run-start card, name labels, isometric town map, notebook rework, varied
  start homes, UI text layering fix, harness `slay`/`goto`.
- 2026-11-06: Wave 3 run-end screen, tone presets with real effects, title
  rework (`87a97ff`).
- 2026-11-06: Wave 3 doors: open/close with E, zombies batter closed doors
  down, survivors pass through, door sprites, harness `zombie`/`doors`
  (`88a3447`).
- 2026-11-06: Broken doors persist across runs; checkpoints keep doors, burn
  scars and fires; harness `save`/`resume`.
- 2026-11-06: Crafting screen rework, ingredient clues, recipes in the
  notebook, crafted items with real effects, Spear.
- 2026-11-06: Item icons: one per item in a single outlined style.
- 2026-11-06: Furniture: furnished rooms, furniture containers and beds,
  furniture sprites; dead zombies cleared however they died (`07e665e`).
- 2026-11-06: Weapon in hand is an inventory item with lasting wear.
