# DEAD PIXEL SURVIVAL

**Game Design Document // v0.3**
**Post Architectural Review — Revised**

| | |
|---|---|
| **Language** | C++ |
| **Renderer** | SDL2 + OpenGL |
| **Art** | Pixel Art — Aseprite + Tiled |
| **Scope** | Solo Developer / Exploration Project |
| **Inspiration** | Project Zomboid |
| **Target Platform** | Desktop (Windows / Linux / macOS) |
| **Map Model** | Fixed world seed per save slot |
| **Multiplayer** | Single-player only. Multiplayer is not a design goal. |

---

This document is the complete and final design specification for Dead Pixel Survival. It incorporates all systems from v0.1, architectural resolutions from the Opus critical review, and v0.3 revisions addressing desensitisation endgame, save slot lifespan, save system robustness, three-item crafting resolution, and campaign objective placement. This document is ready for architecture and implementation.

---

## Changelog

### v0.1 → v0.2 — Architectural Resolutions

- Map model changed from fully random per run to **fixed world seed per save slot**. Layout is deterministic — loot, NPCs, and world state randomise per run. Resolves the legacy persistence coordinate conflict.
- Legacy structures now persist to exact coordinates across runs.
- Desensitisation crisis state confirmed as intentional design.
- Finite loot confirmed as intentional. No farming system planned.
- Crafting failure model clarified — food crafting follows different rules.
- Section 3 added — UI Design (full desktop specification).
- Section 17 added — Base Building.
- Section 18 added — Vehicles (three types, driveable).
- Section 19 added — Fire Propagation.
- Minimum Viable Loop defined with explicit expansion layers.

### v0.2 → v0.3 — Design Revisions

- **Desensitisation endgame** revised — depression crisis now triggers a Psychological Retirement sequence with distinct legacy effects, replacing silent unplayability with a legible narrative ending.
- **Save slot lifespan** explicitly stated — a world can be exhausted, and that is a valid endpoint.
- **Three-item crafting** resolved — sequential two-item combination model with discoverable intermediate items.
- **Save system robustness** specified — autosave policy, atomic writes for legacy files, backup and integrity checks.
- **Campaign objective placement algorithm** sketched — graph distance, infection timeline tuning, validation pass.
- **Multiplayer** explicitly declared out of scope.
- **Audio implementation** noted — SDL_mixer positional audio limitations acknowledged, OpenAL Soft as fallback if wall occlusion matters.

---

## Table of Contents

1. [Concept](#1-concept)
2. [World Design](#2-world-design)
3. [User Interface Design](#3-user-interface-design)
4. [Threat Modes](#4-threat-modes)
5. [Legacy System](#5-legacy-system)
6. [Game Modes](#6-game-modes)
7. [Core Simulation Systems](#7-core-simulation-systems)
8. [Combat System](#8-combat-system)
9. [Weather and Seasons](#9-weather-and-seasons)
10. [Sleep System](#10-sleep-system)
11. [Mental Health and Psychology](#11-mental-health-and-psychology)
12. [Noise Propagation Model](#12-noise-propagation-model)
13. [Scent and Blood Trail System](#13-scent-and-blood-trail-system)
14. [Infection Spread and Zombie Decomposition](#14-infection-spread-and-zombie-decomposition)
15. [NPC Survivor System](#15-npc-survivor-system)
16. [Crafting Discovery System](#16-crafting-discovery-system)
17. [Base Building](#17-base-building)
18. [Vehicles](#18-vehicles)
19. [Fire Propagation](#19-fire-propagation)
20. [Technical Stack](#20-technical-stack)
21. [Minimum Viable Loop](#21-minimum-viable-loop)
22. [Development Roadmap](#22-development-roadmap)

---

## 1. Concept

### 1.1 Overview

Dead Pixel Survival is a top-down pixel art zombie survival game built from scratch in C++. Inspired by Project Zomboid's deep simulation philosophy, it adds a fixed-seed persistent world, a legacy system between deaths, and player-defined threat modes. Every run takes place in the same world — the layout never changes, but the state of that world is shaped by every decision made in every previous run.

### 1.2 Design Pillars

**Every action has weight**
Noise, light, and smell attract zombies. Resource decisions are permanent — the world does not restock. No safe zone lasts forever. The player never makes a trivially safe choice.

**Slow burn tension**
Resources deplete permanently. Infection spreads. The world gets worse whether the player acts or not. Time is always working against the player.

**Emergent stories**
Traits, world events, NPC encounters, and legacy features create organic narratives no two players share. The fixed world means players learn its geography — and feel the weight of returning to places where previous characters died.

**Pixel-first identity**
The art constraint is a feature. Chunky tiles, readable sprites, and moody palettes define the look. The limitation is the aesthetic.

### 1.3 Core Identity

- Fixed world seed per save slot — same layout every run, state changes permanently
- Permadeath with five-feature legacy system between runs
- Procedural loot, NPC placement, and world events within the fixed layout
- Realistic simulation — needs, psychology, injury, skill decay
- Player-defined threat mode — six options selected at run start
- Campaign mode (objective-driven) and Endless mode (sandbox survival)
- Desktop game — full screen real estate, proper UI
- Single-player only — multiplayer is not a design goal
- Solo developer exploration project

---

## 2. World Design

### 2.1 Fixed Seed Map Model

The world map is generated once from a seed stored in the save slot. Every run in that save slot takes place in the same world — identical district positions, identical road layout, identical building placements. What changes per run: loot table rolls, NPC starting positions and personalities, zombie density distribution, weather patterns, and starting conditions.

This decision resolves the legacy persistence conflict — structures built at coordinates (452, 310) in a previous run are still at (452, 310) in the next run because the building they were attached to has not moved. The world is a place the player learns and returns to, not a randomly shuffled level.

**Replayability source:** World state change, not layout change. A district that was safe on run one may be overwhelmed by run five. A base built on run two persists decayed into run six. The same geography tells different stories.

### 2.2 Map Generation — One-Time at Save Creation

When the player creates a save slot, the generator runs once and produces the full world layout. Districts are placed with geographic logic — residential borders commercial, industrial sits at the town edge, medical is near the commercial center, forest outskirts ring the boundary. Road network connects all districts. Building templates are placed within districts according to type rules.

The generator must satisfy several constraints during this one-time creation: all districts must be reachable by road, the campaign objective location must be placed at a distance requiring significant traversal, and infection geography must give some districts a head start on others to create the Day 1 variance of hope and hopelessness.

**Campaign Objective Placement Algorithm**

During one-time map generation, the objective location is placed using a three-step process:

1. **Graph distance.** After district placement and road generation, compute the shortest path in districts from the player spawn to every district. Select the district at maximum graph distance as the objective district. If multiple districts tie, prefer the one adjacent to forest outskirts (thematically: the edge of the city, the way out).

2. **Infection timeline.** The objective district's starting infection percentage is set low — it begins as one of the safer areas. Its daily infection growth rate is tuned so that, at the natural spread rate with no player intervention, the district crosses the overwhelmed threshold at approximately 2.5× the minimum travel time from spawn. This gives a skilled player roughly 1.5× the travel time as margin — enough to survive setbacks but not enough to ignore urgency.

3. **Validation.** The generator confirms that a navigable road path exists from spawn to objective. If the constraint fails (unlikely with the adjacency rules, but possible), the generator re-rolls NPC and zombie placement — not the layout — until the path is viable. The layout seed is never re-rolled.

In Endless mode, the objective district data is generated identically but never surfaced to the player. This keeps both modes on the same generation path per §6.3.

### 2.3 Districts

- **Residential** — houses, garages, backyards. Moderate loot, lower zombie density in early runs.
- **Commercial** — shops, restaurants, offices. High loot, moderate density, high NPC competition.
- **Industrial** — warehouses, factories. Tools and materials, high zombie density, vehicle spawns.
- **Medical** — hospital, clinic, pharmacy. Critical medical loot, very high zombie density.
- **Forest outskirts** — foraging resources, wildlife, sparse zombies, exposure risk.

Each district has distinct loot tables, zombie density, and threat profiles. The generator ensures district adjacency makes geographic sense.

### 2.4 Per-Run Variance Within Fixed Layout

- **Loot table rolls** — same containers exist, different contents each run.
- **NPC starting positions and personalities** — regenerated each run within district logic.
- **Zombie density distribution** — varies by threat mode and run number.
- **Weather seed** — different weather patterns each run.
- **World state carries forward** — looted containers stay looted, burned areas stay burned, built structures persist decayed, graves remain.

### 2.5 World Feel

**Day / Night Cycle**
Night dramatically increases zombie aggression and spawn rate. Light sources become simultaneously critical and dangerous. The player must balance visibility against exposure.

**Weather System**
Rain masks movement sound but reduces visibility and accelerates illness risk. Heat affects stamina drain. Weather is a system that reshapes survival priorities. See Section 9.

**World Decay Across Runs**
The world accumulates history. Districts scavenged heavily in previous runs have visibly empty shelves. Areas the player burned have charred tiles. Graves mark death locations. The longer a save slot is played, the more the world looks like it has been through something — because it has.

---

## 3. User Interface Design

> *"The simulation is complex. The interface is not. Every piece of information has a place and a reason to be there."*

### 3.1 Layout Philosophy

Dead Pixel Survival is a desktop game with full screen real estate. The UI occupies that space properly. The game world renders at full screen. UI elements are overlaid in defined zones — four corners and a bottom strip — and never obscure the center play area where moment-to-moment gameplay happens. All panels are semi-transparent so the world always shows through.

### 3.2 Screen Zones

| Zone | Content |
|---|---|
| **Top Left** | Character state panel — physical needs and mental state indicators. |
| **Top Center** | Time, day counter, and weather indicator. |
| **Top Right** | Minimap — 200×200px, semi-transparent, world-aware. |
| **Bottom Strip** | Full-width, 80px tall — hotbar, equipped item detail, active status flags. |
| **Contextual** | Interaction prompts float near world objects. Panels slide in on key press. |

### 3.3 Top Left — Character State Panel

A compact vertical stack of status indicators. Each need shown as an icon plus a five-segment bar. Segments represent roughly 20% of the need. Icons change expression as bars deplete — a bread icon cracks and goes hollow, a water droplet shrinks. Panel is roughly 180px wide, semi-transparent dark background, left-anchored.

**Physical Needs:**
- **Hunger** — bread icon, five-segment bar, icon degrades as bar depletes.
- **Thirst** — droplet icon, five-segment bar, droplet shrinks.
- **Fatigue** — eye icon, five-segment bar, eye closes progressively.
- **Sleep debt** — moon icon, bar fills as debt accumulates rather than depletes.
- **Body temperature** — thermometer icon, colour shifts from blue (cold) to white (neutral) to red (hot).
- **Illness** — biohazard icon, only visible when illness is active, pulses slowly.

**Mental State:**
- **Panic** — a small heartbeat line. Flat when calm, erratic when panicking, jagged during breakdown.
- **Depression** — a small face icon. Shifts expression across five states from neutral to hollow.

### 3.4 Top Right — Minimap

A 200×200px square map, semi-transparent. Shows the current district and immediate adjacent districts. Fully explored areas render clearly. Unexplored areas are dark.

- Player position as a small directional arrow showing facing direction.
- Known NPC positions as small dots — only NPCs the player has observed.
- Noise events as brief ripple animations at their source location — teaches the noise propagation model visually.
- Blood trail as a faint red line in the player's recent path when actively bleeding.
- Zombie positions are not shown. The player reads the environment, not a radar.

Pressing **M** opens a full-screen world map showing all explored districts with legacy markers — graves, previous base locations, burned areas, known objective direction.

### 3.5 Top Center — Time and Day

A pixel art 12-hour clock face, current day number in tally mark style, and a small weather indicator icon. The clock hands move in real time relative to game speed. The weather icon transitions — clear, clouds building, rain, storm — giving advance notice matching the art-based warning system. Run duration shown as days survived, always visible.

### 3.6 Bottom Strip

**Left Section — Hotbar**
Eight quick-access inventory slots numbered 1–8. Each slot shows the item sprite, a thin condition bar beneath the sprite (green to red), and the key number. The equipped slot is highlighted with a subtle border. Stack counts shown for stackable items.

**Center Section — Equipped Item Detail**
A slightly larger display of the currently equipped item. Shows item name in small text, condition as a more detailed bar, and relevant stat — weapon shows damage tier as dots, tool shows durability as segments, food shows hunger recovery estimate.

**Right Section — Status Flags**
A row of small icon badges that appear contextually — only when active. Never shown when irrelevant.

- **Bleeding** — pulses, intensity indicates bleed rate.
- **Infected wound** — appears when a wound is untreated and infected.
- **Wet** — after rain exposure.
- **Encumbered** — carrying too much, movement penalty active.
- **Noise level** — shows current noise tier the player is generating. Critical feedback for stealth.
- **Crouching indicator** — small chevron when crouched.

### 3.7 Contextual Panels

**Inventory Panel (Tab)**
Full inventory grid, roughly 40% of screen width, slides in from the left. Container interaction opens a split view — player inventory left, container right. Drag and drop between panels. Closes when the player moves.

**Crafting Panel (C)**
A combination interface, not a recipe list. Select two items from inventory, a small animation plays, result appears if successful. Failed combinations show a brief shake animation. Discovered recipes shown as sketched thumbnails in the notebook page — drawn style, not clinical UI. Preserves the discovery philosophy while giving the player a reference.

**Interaction Prompt**
When the player is near an interactable object or NPC, a small prompt floats near the object in world space — not in a fixed UI location. Shows available actions as single-word labels: Loot, Open, Barricade, Talk, Observe. Disappears when the player moves away.

**NPC Interaction Panel**
When engaging an NPC, a simple panel appears with contextual action buttons. The NPC's rough state is communicated through their sprite expression and a few lines of ambient dialogue generated from their current needs and personality. No menu trees — contextual choices that resolve based on their utility AI scores.

**Journal Panel (J)**
The notebook. Shows discovered crafting recipes as sketched entries, journal entries from previous characters and NPCs found in the world, and the legacy cemetery list. Styled as an actual notebook — graph paper aesthetic, handwritten pixel font. The one UI element that fully commits to the world's aesthetic rather than clinical information display.

### 3.8 Death Screen

Not a menu. A full-screen moment. The screen desaturates to a still view of where the player died. Text appears in the handwritten pixel font:

- *[Character name]*
- *Survived [X] days*
- *[Cause of death — written naturally: "Overwhelmed in the medical district on Day 14"]*

Below: a brief run log of notable events — first kill, first NPC encountered, last thing eaten, last structure built. Not a stats dump. A eulogy.

Then quietly: *Their grave has been marked.*

Then the legacy panel slides in showing what carried forward — traits unlocked, whether the journal was recovered, what structures persist, world memory updates.

Then a single prompt: **Begin again.**

### 3.9 Retirement Screen

Distinct from the death screen. When a character enters Psychological Retirement (see §11.4), the character sits down wherever they are. The camera pulls back slightly. The screen does not desaturate. Text appears:

- *[Character name]*
- *Survived [X] days*
- *They stopped.*

The run log is replaced by a final journal entry — longer and more reflective than standard entries, auto-generated from the character's run history. What they built. Who they met. How many they killed. Written in past tense, as someone looking back.

Then the legacy panel shows the retirement-specific effects. Then: **Begin again.**

### 3.10 Visual Language

- Semi-transparent dark panels — the world always shows through. Nothing is fully opaque.
- Pixel art icons throughout — consistent with the game art style.
- Handwritten pixel font for narrative text. Clean pixel font for data.
- Colour language: green = safe, amber = warning, red = danger, white = neutral.
- Animations are subtle — bars drain smoothly, icons pulse slowly. Nothing flashes aggressively.
- The world is always visible. Only the death screen, retirement screen, world map, and journal go full-screen.

---

## 4. Threat Modes

The player selects a threat mode at run start. This replaces a conventional difficulty selector — rather than Easy, Normal, or Hard, the player chooses how the world wants to kill them. All threat modes use the same underlying zombie AI; the mode shapes population, behaviour, and evolution across runs.

**01 — The Overwhelming Swarm**
Zombies are individually weak but never stop coming. One zombie is manageable. Fifty is a death sentence. Panic and resource anxiety. PZ-faithful.

**02 — The Stalker**
A small number of zombies, persistent. They hear the player, track them, and do not give up. Paranoia and stealth-focused gameplay.

**03 — The Mutating Threat**
Zombies evolve as days pass. Early game: slow and dumb. Late game: faster, climb obstacles, break doors. A ticking clock.

**04 — The Infected Society**
Other survivors are equally dangerous. Factions, desperation, and unpredictable NPC behaviour are the primary threat. Zombies are background noise.

**05 — The Unknown**
Zombies behave inconsistently — sometimes passive, sometimes frenzied, triggered by conditions the player must discover. Dread through mystery.

**06 — Hybrid: Swarm + Mutation**
Begins as overwhelming swarm. Over days, they mutate into something worse. Two-phase escalating threat.

---

## 5. Legacy System

> *"Death is not the end of the story. It is the end of a chapter."*

The fixed world seed means every legacy feature is spatially coherent — a structure built at a specific location persists at that exact location because the world layout does not change between runs. All five features are active simultaneously by default. Players can toggle features or use presets: **Fresh Start** (nothing carries over) or **Living World** (all five active).

### 5.1 Journal Entries

Dead characters leave handwritten notes at their last known locations. Future run characters find and read them. High-competence NPCs also write entries. The simulation produces world-building that costs nothing to script.

### 5.2 Persistent Structures

Bases the player built persist into the next run in one degradation stage below their last state. A fully intact barricade persists as damaged. A damaged barricade persists as breaking. A breaking barricade does not persist. Structures never survive indefinitely — recent construction carries forward meaningfully, old construction decays to nothing across enough runs.

### 5.3 Bloodline Traits

Each death unlocks a trait fragment available to future characters. A character who died as a skilled carpenter leaves a ghost of that knowledge. Future characters can start with partial versions of mastered skills. The bloodline's collective competence builds over many runs. Psychological Retirement (see §11.4) unlocks the unique **Hardened** trait — the only source of depression resistance in the game.

### 5.4 World Memory

The world remembers everything. Burned areas stay burned. Looted containers stay looted. Hordes drawn somewhere remain affected. NPC bases that were helped or betrayed leave evidence. The world is not reset between runs — it continues aging.

**Loot is finite and does not restock.** This is a design decision, not an oversight. Players who deplete the world have survived long enough to witness the full arc of an apocalypse.

**Save Slot Lifespan**

A save slot has a natural lifespan. The world's resources are finite. Its survivors are finite. Across enough runs, every container empties, every NPC dies, and every district is overwhelmed. When the world is exhausted, the story of that world is over.

This is not a failure state — it is the conclusion of that world's history. The legacy cemetery, the accumulated graves, the burned districts, and the empty shelves are the record of everything that happened. The player is meant to start a new seed and begin a new world with a new history.

The game does not force this. No popup tells the player the world is done. They feel it — fewer supplies each run, longer walks between anything useful, more graves than buildings. The world communicates its exhaustion the same way it communicates everything else: through state, not UI.

### 5.5 Named Graves

Every dead character receives a grave marker at their death location. The cemetery grows run by run — a physical record of every failure. Finding ten graves tells a future character that others have tried and failed here. NPC burials enter the same cemetery. Graves are permanent world objects.

Retired characters receive a distinct marker — a shelter marker instead of a cross. A future character finding it reads: *"Someone lived here long enough to choose to stop."* Mechanically identical to a grave for legacy purposes. Narratively different.

---

## 6. Game Modes

### 6.1 Campaign Mode

A goal exists — escape the city, reach a radio signal, find a cure. The objective location is fixed at world generation (see §2.2 for placement algorithm). Each failed run gets the player closer to understanding the route and the threats. The legacy system carries full weight — runs build on each other toward the objective.

The objective location is placed at a distance requiring roughly two full seasons of normal survival pace to reach. The destination has its own infection timeline — waiting too long makes it unreachable. The campaign is a race against both distance and world deterioration.

### 6.2 Endless Mode

No win condition. Pure survival sandbox. How long can you last? The world gets richer and more depleted the more you die. Legacy builds a living history. Population declines run by run as the finite survivor pool dwindles.

### 6.3 Shared Engine

Both modes run on exactly the same systems. Campaign adds an objective layer and narrative wrapper. One game with two lenses, not two games.

### 6.4 Tone Presets

**Hopeless** — PZ-faithful. Bleak, sparse UI accents, no music. The world is ending.

**Grim but Human** — Dark with moments of warmth. Found notes tell human stories. Loss lands harder because connection was possible.

**Brutal Arcade** — Realistic systems, faster and more aggressive tone. Action-horror energy.

---

## 7. Core Simulation Systems

### 7.1 Physical Needs

- **Hunger** — decays by activity. Severe hunger reduces max stamina and causes morale penalty.
- **Thirst** — decays faster than hunger, accelerated by heat and exertion.
- **Fatigue** — short-term exhaustion recovered by rest. Affects stamina ceiling and movement speed.
- **Sleep Debt** — long-term accumulation. Only recovered by actual sleep. See Section 10.
- **Body Temperature** — affected by weather, clothing, activity, and shelter.
- **Illness** — caused by infection, exposure, contaminated water, and biological hazards.

### 7.2 Skills

Skills are earned through actions. XP accumulates from relevant activities — carpentry from building, foraging from scavenging, combat from fighting. Skills degrade without use over long runs. Skills have soft crafting gates — a skilled carpenter discovers reinforced barricade recipes that a low-skill player attempting the same combination would fail.

### 7.3 Traits

Character creation presents positive and negative traits with point costs. Positive traits cost points, negative traits grant them. The budget forces meaningful tradeoffs. Bloodline traits from the legacy system provide starting fragments of knowledge earned by previous deaths.

### 7.4 Loot and Inventory

Loot is container-based — backpacks, shelves, vehicles, bodies, hidden caches. Items degrade over time. The world's supply is finite and non-respawning. Heavily scavenged districts look visibly picked over. Other survivor NPCs compete for the same resources.

### 7.5 Injury System

Wounds require treatment — bandaging stops bleeding, disinfecting prevents infection, suturing closes deep wounds, rest allows healing. Untreated wounds progress to infection and illness. Broken bones require splinting and rest. Bladed zombie attacks cause more bleeding, interacting directly with the scent trail system.

---

## 8. Combat System

> *"Fighting is a tax, not a reward. Every engagement costs something. The optimal play is always avoidance."*

### 8.1 Attack System

Cursor-aimed swings directed toward wherever the mouse points, with a visible arc indicator showing the hit zone. Each weapon has arc width and range. Swinging has wind-up and recovery frames — the player is vulnerable during both. Missed swings cost stamina the same as landed ones.

### 8.2 Facing and Awareness

The character has a forward-facing cone of roughly 180 degrees. Cursor-aimed attacks allow 360-degree swings but facing direction determines defensive exposure.

- **Threats inside the cone:** full speed reaction, normal damage received.
- **Threats outside the cone:** no warning, damage increased roughly 50%, grabs cost double stamina to break.
- **Backs against walls:** eliminate rear vulnerability — natural wall-hugging behaviour without forcing it.

### 8.3 Stamina

Shared resource between combat, running, and base building. Each swing costs stamina scaled to weapon weight.

**Exhausted (below 20%):** Swings miss more. Movement slows. Grabs cost more to break. Morale hit.

**Recovery:** Passive when not in combat. Faster when crouching or still.

**Needs interaction:** Hunger and fatigue reduce max stamina ceiling, not just current stamina.

### 8.4 Grab Mechanic

Triggered when a zombie enters melee range and the player fails to interrupt. Breaking free costs a flat stamina chunk. Multiple simultaneous grabs stack the cost. A grabbed player cannot swing — only push or break free. Two grabs at once is often fatal for an exhausted player.

### 8.5 Weapon Categories

**Blunt — Bats, Pipes, Crowbars, Hammers**
- Widest swing arc, high stamina cost, knockback on hit.
- No bleeding caused — less scent trail risk.
- Condition degrades slowly. Exhausted swings can miss entirely and cause stumble.

**Bladed — Knives, Axes, Machetes**
- Narrower arc, faster swing speed, causes bleeding on hit.
- Gets stuck occasionally — stamina-cost pull to free, leaving the player temporarily defenseless.
- Condition degrades faster on bone contact. Player bleeds more from bladed zombie attacks.

**Improvised — Chair Legs, Bottles, Bricks**
- Always available. Weakest damage, shortest range. Break after several uses.
- Some have secondary uses — a bottle becomes a molotov ingredient, a crowbar opens doors.

**Firearms — Pistols, Shotguns, Rifles**
- Only category attacking from outside grab range. Massive noise radius — multiple districts.
- Accuracy degrades when exhausted or panicking. Suppressed options exist but are still noisy.
- Best use: controlled situations, buying escape time. Never the first option.

**Throwables — Molotovs, Rocks, Flares, Noise Makers**
- Rocks and noise makers: distraction tools. Redirect zombie attention without engaging.
- Molotovs: area denial. Fire spreads and persists. Can trap the player.
- Flares: attract zombies to a point — useful for clearing a path.

### 8.6 Weapon Condition

Every weapon has a condition state: Good → Worn → Damaged → Breaking → Broken. Condition affects performance linearly. Bladed weapons in poor condition get stuck more. Blunt weapons in poor condition lose knockback before damage. Broken weapons become improvised. Condition visible through sprite changes — no UI number.

### 8.7 Positioning

**Shove** — Low stamina cost push creating 1–2 tile distance. Primary spacing tool.

**Knockdown** — Heavy blunt hits on low-health zombies. Downed zombies rise slowly — stomping finishes them.

**Chokepoints** — Doorways and staircases create natural 1-on-1 encounters. Core survival strategy.

### 8.8 Combat System Interactions

- Every melee swing generates medium-tier noise.
- Gunshots generate explosive-tier noise — immediate horde response.
- Bleeding wounds interact with the scent trail system.
- Morale drops from witnessing death, repeated killing, being grabbed.
- Exhausted combat accelerates fatigue need decay.

---

## 9. Weather and Seasons

> *"Weather is not atmosphere. It is a system that actively reshapes survival priorities."*

### 9.1 Seasons

**Spring** — Default start. Moderate temperature, regular rain, moderate zombie activity. The balanced baseline.

**Summer** — High temperatures. Overheating causes stamina drain and thirst acceleration. Heavy clothing protects from bites but causes heat exhaustion. Longer daylight — shorter night threat window.

**Autumn** — Dropping temperature. More frequent heavy rain. Food spoils faster. The beginning of the long decline.

**Winter** — Body temperature becomes a primary survival concern. Outdoor scavenging becomes timed. Water sources freeze. Snow muffles movement noise. Blood trails dramatically more visible in snow.

Seasonal transitions take several in-game days. The player feels winter coming before it arrives.

### 9.2 Fog

Occurs in any season, most commonly autumn and winter. Outdoor visibility reduces to roughly half. Indoor visibility unaffected. Sound direction becomes slightly unreliable in heavy fog. Stalker threat mode becomes especially dangerous in fog. Duration 2–6 in-game hours.

### 9.3 Storm Events

**Thunderstorm** — Maximum rain intensity — noise propagation reduced to minimum. Movement nearly silent. Lightning strikes generate explosive-tier noise at random outdoor locations, scattering zombie wandering.

**Blizzard (Winter)** — Near-zero outdoor visibility. Heavy movement penalty. Body temperature drops at double rate. Zombie activity also drops. Shelter is mandatory.

**Heatwave (Summer)** — No rain, constant heat exhaustion risk outdoors. Nighttime becomes the only safe scavenging window.

**Storm Preparation** — Weather patterns give advance warning through environmental changes — pressure shifts, wind changes, sky colour. The top-center weather icon transitions to give advance notice. No forecast UI beyond that. The world communicates through art and the weather indicator.

---

## 10. Sleep System

> *"Sleep is not a button you press when convenient. It is a biological debt that collects interest."*

### 10.1 Two-Layer Fatigue

**Fatigue** — Short-term. Recovered by rest and low activity. Affects stamina ceiling and movement speed. Can be temporarily masked by stimulants.

**Sleep Debt** — Long-term. Only recovered by actual sleep. Cannot be resolved by rest alone. Inevitable collapse if ignored. The player needs roughly 6–8 in-game hours of sleep per 24-hour cycle.

### 10.2 Sleep Debt Effects

**Mild (1 missed cycle)** — Slight accuracy reduction. Slower stamina recovery. Morale drops passively. Subtle visual edge blur.

**Moderate (2 missed cycles)** — Hallucinations begin. Peripheral movement that resolves to nothing. Faint sounds. Combat stamina ceiling reduced noticeably.

**Severe (3+ missed cycles)** — Forced microsleeps — 1–2 second blackouts randomly. Hallucinations harder to distinguish from real threats. Collapse imminent.

**Collapse** — Player passes out wherever they stand. Wakes after a full sleep cycle — 6–8 in-game hours later. Whatever happened during that time happened without them.

### 10.3 Sleep Quality by Location

| Location | Recovery | Notes |
|---|---|---|
| **Unsafe** | Partial | Outdoors or unbarricaded. Wakes at nearby sounds — often to immediate threat. |
| **Marginal** | Full | Indoors, one entry point, no barricades. Random wake events possible. |
| **Safe** | Full, uninterrupted | Barricaded, single entry secured, noise traps set. |
| **Optimal** | Full, faster cycle | Safe location plus a bed or sleeping bag. Morale recovery bonus on waking. |

### 10.4 Stimulants and Noise Traps

Caffeine and medication suppress symptoms temporarily without reducing actual debt. Coming off stimulants when debt is severe accelerates collapse. Noise traps around a sleep location wake the player with reaction time — but generate noise that may call more zombies. The trap that saves is also the trap that calls the horde.

---

## 11. Mental Health and Psychology

> *"The mind breaks before the body in survival situations."*

### 11.1 Two Distinct Mental States

**Panic** — Acute, short-term. Triggered by immediate threats. Spikes fast, recovers quickly once threat resolves.

**Depression** — Chronic, long-term. Builds from accumulated trauma, isolation, and loss. Recovers slowly.

Both states affect the player simultaneously and independently. High panic during a depressive episode is the most dangerous mental state in the game.

### 11.2 Panic — Triggers and Effects

**Triggers:**
- Zombie within close range, being grabbed, witnessing a survivor die.
- Sudden loud noise at close range, waking from collapse surrounded.
- First kill — higher effect early, diminishes with desensitisation.

**Effects by Level:**

| Level | Effects |
|---|---|
| **Low** | Slight weapon sway, minor accuracy reduction, slightly higher stamina costs. |
| **Moderate** | Visible shaking, louder movement sounds, accuracy reduced, peripheral awareness cone narrows. |
| **High** | Combat significantly harder, movement erratic, items may drop when grabbed. |
| **Breakdown** | Brief loss of player control for 2–3 seconds. Extreme simultaneous stressors only. |

**Panic Recovery:** Distance from threat, silence, sitting still, eating comfort food, smoking (if trait taken), finding another living survivor.

### 11.3 Depression — Triggers and Effects

**Triggers:**
- Extended time alone. Death of a recruited NPC. Base destroyed.
- Finding the grave of a previous character. Cumulative kills exceeding a threshold.
- Failing to save a survivor who asked for help. Reading a journal from someone who did not make it.

**Effects by Level:**

| Level | Effects |
|---|---|
| **Mild** | Passive morale drain. Sleep quality slightly reduced even in safe locations. |
| **Moderate** | Stamina ceiling reduced. Some positive morale triggers stop working. |
| **Severe** | Player occasionally hesitates — 1–2 second pause before swinging, eating, or leaving shelter. |
| **Crisis** | Player cannot perform certain actions. Cannot attack. Cannot loot a body. Cannot leave shelter. |

**Depression Recovery:** Human contact, completing a meaningful goal, building something, finding comfort items, sleeping well consistently, time.

### 11.4 Desensitisation and Psychological Retirement

Repeated violence reduces panic triggers over time. A player who has killed hundreds of zombies stops panicking at the sight of one. Desensitisation reduces panic response but accelerates depression accumulation. The player becomes harder on the outside and more fragile on the inside.

**Psychological Retirement**

When a character enters depression crisis and remains there for a full 24-hour in-game cycle without recovery, the game triggers a Retirement sequence — a distinct ending separate from death.

The character sits down wherever they are. The camera pulls back slightly. The retirement screen appears (see §3.9). A final journal entry is auto-generated from the character's run history — what they built, who they met, how many they killed. Written in past tense, as someone looking back.

**Legacy effects of retirement (distinct from death):**

- **Unique bloodline trait: Hardened.** Future characters can start with a depression resistance fragment — a slower accumulation rate on the early depression triggers. This is the only way to earn this trait. It does not prevent crisis. It delays it. The bloodline learns endurance, not immunity.
- **Retirement marker instead of grave.** The location is marked with a distinct world object — not a cross but a shelter marker. A future character finding it reads: *"Someone lived here long enough to choose to stop."*
- **Journal entry is guaranteed.** A retired character always leaves an entry, regardless of whether they had a notebook. The act of stopping is the act of writing. This is the only exception to the notebook requirement.

Retirement is the rarest and most meaningful legacy event in the game. It is the reward for mastery — not continued survival, but a record that someone endured longer than the world thought possible, and the next character starts slightly harder because of it.

### 11.5 Morale Recovery

- **Hot food and coffee** — Strong panic recovery, mild depression recovery.
- **Alcohol** — Immediate morale boost, worsens sleep quality, dependency risk.
- **Books and music players** — No panic recovery, strong depression recovery over time.
- **Comfort items** — Personal effects found in loot. One-time strong depression recovery. Rare.
- **Human contact** — Strongest depression recovery. Talking, trading, travelling with a survivor.
- **Accomplishment** — Reaching objectives, fortifying a base, surviving a horde. Depression recovery tied to progress.

---

## 12. Noise Propagation Model

> *"Sound is a resource you spend and a weapon the world uses against you."*

### 12.1 The Model

Every noise event generates a radius and a duration. Zombies within the radius investigate the source location — they path toward it, not teleport. The noise model is event-driven, not continuous — calculated on trigger, not every tick.

### 12.2 Noise Tiers

| Tier | Radius | Examples |
|---|---|---|
| **Whisper** | 2–4 tiles | Crouching movement, eating quietly, careful item pickup. Rain masks entirely. |
| **Soft** | 5–10 tiles | Normal walking, careful door opening, rummaging, quiet conversation. Rain reduces 30–50%. |
| **Medium** | 11–20 tiles | Running, melee combat swings, breaking windows carefully, coughing, zombie growls. |
| **Loud** | 21–40 tiles | Smashing windows, generators running, triggered alarms, shouting, vehicle engines. |
| **Explosive** | 41–80+ tiles | Gunshots — pistol lower end, shotgun and rifle upper end. Explosions. Persist as attractors for minutes. |

### 12.3 Environmental Modifiers

**Surfaces:**
- Carpet, soil, grass — movement noise reduced one tier.
- Metal grating, gravel — movement noise increased one tier.
- Crouching — reduces movement noise one tier on any surface.

**Weather:**
- Rain — reduces all radii 30–50%. Heavy rain nearly halves noise range. Tradeoff: rain also reduces the player's ability to detect threats.
- Wind — directional. Noise travels further downwind, shorter upwind.
- Fog — no effect on noise, only visibility.

**Structures:**
- Walls absorb noise — indoor sounds propagate less outside.
- Closed doors reduce propagation by roughly 40%.
- Broken windows and open doors allow full propagation.
- Basements nearly isolate sound — a generator underground is significantly quieter.

### 12.4 Sound as a Tool

- Rocks and noise makers redirect zombie attention without engaging.
- Triggered alarms across town can pull a horde away from the player's position.
- A working car alarm is one of the most powerful tools and one of the most dangerous to activate.
- Flares generate sustained noise at a placeable location.

### 12.5 NPC Noise

NPCs generate noise on the same model. A panicking NPC running and shouting generates loud-tier. A careful NPC scavenging generates soft-tier. Zombie growls and combat generate medium to loud tier. A distant gunshot tells the player an NPC just made a decision.

---

## 13. Scent and Blood Trail System

> *"Bleeding is not just a health problem. It is a navigation problem."*

### 13.1 The Trail

When bleeding, the player leaves blood tile markers every few steps. Zombies within detection radius follow the trail sequentially toward its freshest point — the player's current position. Sound attraction is immediate and directional. Scent tracking is delayed and persistent. A zombie that missed the sound of a fight may still follow the trail.

### 13.2 Bleed Rates and Decay

| Wound Type | Bleed Rate | Trail Density | Scent Intensity |
|---|---|---|---|
| Minor cuts | Slow | Sparse | Low |
| Deep lacerations | Moderate | Regular | Moderate |
| Severe wounds | Fast | Dense | High |
| Multiple wounds | Stacked | Cumulative | Cumulative |

Bandaging stops new markers. Existing markers persist until they decay naturally. Rain rapidly washes the trail. Water crossings immediately break it at that point.

### 13.3 Trail Decay by Environment

- **Dry indoor** — Slow fade. Persists for many in-game minutes.
- **Outdoor surfaces** — Moderate fade.
- **Rain** — Rapidly washes trail. Heavy rain eliminates within minutes.
- **Snow** — Blood trails are dramatically more visible in snow (see §9.1).

### 13.4 Zombie Bleed

Bladed weapon hits cause zombie bleeding — a visual marker the player can follow if a zombie fled. Bloated decomposed zombies release a proximity biological hazard on death. Zombie blood does not attract other zombies — they do not track each other.

### 13.5 NPC Scent Interaction

NPCs bleed on the same model. A wounded NPC fleeing may bring zombies toward their shelter — or toward the player if they fled that direction. Finding a blood trail leading to an NPC's base tells the player they have had a difficult time recently.

---

## 14. Infection Spread and Zombie Decomposition

> *"The apocalypse is not a static backdrop. It is an ongoing process."*

### 14.1 Infection Spread

The city starts with a generated infection percentage per district. Every in-game day, a background calculation runs per district considering zombie density, NPC population, player intervention, and adjacency to overwhelmed districts. Districts near overwhelmed areas have higher daily conversion probability.

Districts shift visually as infection spreads — more abandoned vehicles, more bodies, more broken windows. No UI percentage. The art communicates the state. When a district crosses a critical threshold it becomes overwhelmed — remaining survivors flee or convert, resource nodes become heavily contested.

In campaign mode, the objective location has its own infection timeline. Waiting too long makes it unreachable.

### 14.2 Zombie Decomposition Stages

| Stage | Age | Movement | Threat | Special |
|---|---|---|---|---|
| **Fresh** | 1–3 days | Fastest | Highest | Bladed kills leave active scent markers |
| **Bloated** | 4–10 days | Slower | Durable | Biological hazard splash on death |
| **Decayed** | 11–20 days | Slow | Low individual | Proximity creates disease risk in numbers |
| **Skeletal** | 20+ days | Barely mobile | Minimal | Obstacle. Tells the player the infection hit early |

Decomposition state is readable world history. Fresh zombies mean a new outbreak. Skeletons mean the initial wave was weeks ago. Combined with infection spread, the player can assess a district's timeline before any map marker tells them.

**Decomposition and Legacy:** In subsequent runs, decomposition state carries over partially through world memory. A district cleared in a previous run has fewer fresh zombies. The world ages across runs, not just within them.

---

## 15. NPC Survivor System

> *"Survivors are not quest givers. They are other people trying not to die, with the same urgency you have."*

### 15.1 Simulation Loop

NPCs have the same needs as the player running on the same decay rates. Decision-making uses utility AI — every possible action scores against current needs and threat assessment. Highest score executes. Simple, cheap, and produces believable behaviour because multiple needs compete simultaneously.

Each NPC resolves their decision cycle in this order every tick:
1. Assess threats — are there zombies nearby? How many? Fight or flee?
2. Assess needs — what is the most urgent physical need?
3. Make a decision — what action best addresses the combined assessment?
4. Execute — move, scavenge, sleep, eat, hide, fight, interact.

### 15.2 Simulation Tiers

| Tier | Scope | Fidelity | Notes |
|---|---|---|---|
| **Tier 1 — Active** | Player's current district | Full simulation every tick | Full pathfinding, needs, combat |
| **Tier 2 — Adjacent** | Neighboring districts | Every few seconds | Real needs decay, waypoint pathfinding |
| **Tier 3 — Distant** | All other districts | Compressed | Probabilistic resolution on approach |

**Tier 3 resolution:** When the player approaches a distant district, the game resolves elapsed time probabilistically based on NPC competence, available resources, and nearby threats. The result is not guaranteed to match full simulation — this divergence is acceptable and occasionally produces dramatic discoveries. The fast-forward resolution must be **deterministic given the same inputs** (elapsed time, snapshot state, random seed) to ensure save/load consistency.

### 15.3 Procedural Personality

| Axis | Low | High |
|---|---|---|
| **Aggression** | Avoids conflict, flees, hoards quietly | Attacks proactively, may attack the player |
| **Cooperation** | Territorial, refuses interaction | Willing to trade, share info, temporarily recruit |
| **Desperation** | Stable, predictable | Desperate, unpredictable (dynamic — rises as needs worsen) |
| **Competence** | Poor decisions, dies quickly, bad loot | Survives longer, better shelter, writes journal entries |

### 15.4 Faction Attitudes

**Organised** — Groups of 2–4 with a base. Trade from strength. Defensive of territory.

**Desperate** — Solo or pairs, high needs. Cooperation means nothing when starving.

**Neutral** — No strong disposition. Behaviour driven by player approach and current desperation.

**Hostile** — High aggression regardless of needs. Attack on sight or after a single warning.

Attitudes shift dynamically. A neutral NPC pushed to high desperation trends hostile. An organised group that loses members destabilises toward desperate.

### 15.5 Interaction Options

- **Observe** — Watch without engaging. Learn behaviour pattern and needs state safely.
- **Approach** — Signal non-aggression. Response depends on aggression and desperation.
- **Trade** — Items exchanged at need-driven prices. Desperate NPCs overvalue food and water.
- **Share information** — Tell the NPC about a location or threat. They may reciprocate.
- **Recruit temporarily** — They follow and help but consume resources. They leave if needs become critical.
- **Get betrayed** — An outcome, not a player choice. Always earned by circumstances — never random.

### 15.6 NPC Death and Legacy

NPCs die constantly without the player. The world is losing people. Dead NPCs leave lootable bodies with time-of-death recorded. Competent NPCs may have left journal entries. Their bases become abandoned sites. They enter the legacy cemetery only if buried.

World population is finite and non-replenishing. Every living person becomes rarer as runs progress. Finding a living survivor on Day 40 means something different than Day 5.

### 15.7 The Ambient Presence

High-competence NPCs leave traces — a reinforced door, shelves partially restocked, careful notes about zombie patterns, traps set around a perimeter. The player walks into a building and knows someone was here. Someone smart. This ambient presence costs almost nothing to implement because it is a natural result of NPC actions leaving traces in the world.

---

## 16. Crafting Discovery System

> *"You do not know what you can make until you try to make it."*

### 16.1 Core Mechanic

No recipe list. No greyed-out options. The player selects two items and attempts to combine them. Success creates a new item. Partial success creates something unintended. Failure produces nothing and consumes nothing.

**Multi-component recipes use sequential combination.** The crafting interface always combines exactly two items. Recipes requiring three or more components are resolved through intermediate items. Rags + alcohol produces a soaked rag. Soaked rag + bottle produces a molotov. The intermediate item exists in inventory — it is a real item with a name, a sprite, and the possibility of alternative uses the player may discover.

This keeps the crafting interface simple (always two items), creates discoverable intermediate items that expand the experimentation space, and avoids a variable-input UI that would complicate both the interface and the recipe system.

All recipe category examples below describe final outputs. The intermediate steps are part of the discovery — the GDD does not enumerate them because the player is meant to find them.

**Food crafting follows different rules.** Poisonous food combinations are a discovery risk — incorrectly prepared foraged food can cause illness on consumption. The no-punishment rule applies to item combination attempts only, not to the consequences of eating something incorrectly prepared.

### 16.2 Recipe Memory

Discovered recipes record in a craftable notebook item. Without a notebook, knowledge dies with the character. A notebook found in the world contains recipes written by a previous character or NPC. Notebooks persist into next runs through the persistent structures legacy feature.

### 16.3 Recipe Categories

**Medical**
- Rags + alcohol → disinfected bandage.
- Needle + thread → suture kit.
- Herbal plants + water + heat → natural remedy.
- Specific plant combinations → stronger remedies (discoverable through foraging skill).

**Weapons and Tools**
- Bottle + rags + alcohol → molotov (via soaked rag intermediate).
- Nails + bat → spiked bat.
- Rope + rocks → improvised sling.
- Glass shards + tape → improvised knuckles.
- Broken bottle → improvised blade (discovered automatically when a bottle breaks during use).

**Traps**
- Tin cans + wire + doorframe → noise trap.
- Nails + board → floor spike.
- Bottle + flammable liquid + trigger wire → fire trap.
- Rope + branch + bait → animal snare (requires foraging skill discovery).

**Food**
- Raw meat + fire → cooked food.
- Multiple ingredient combinations → actual meals (higher morale recovery).
- Berries + water + heat → tea (morale and warmth bonus).
- Specific foraged combinations → poisonous by accident.

**Comfort and Utility**
- Candle + jar → enclosed light source — quieter, less zombie attraction.
- Fabric + stuffing → improvised sleeping pad.
- Personal items + notebook → memorial entry — no mechanical effect, pure legacy world object.

### 16.4 Design Principles

- No visible recipe list. Notebook entries are descriptive, not formulaic.
- No ingredient highlighting. The game never signals that two items are combinable.
- No mandatory crafting — every recipe has a found alternative.
- Skill interaction — high-skill characters discover more from the same combination attempts.

### 16.5 Legacy and Crafting

- **Notebooks** persist as physical objects through persistent structures.
- **Journal entries** — high-competence NPCs write recipe hints as observations, not spoilers.
- **Bloodline traits** — a character who mastered a crafting category passes a starting recipe fragment to successors.
- **World memory** — a crafting station the player built persists in decayed form next run.

---

## 17. Base Building

> *"A base is not a home. It is a series of desperate decisions about where to spend limited time and materials to buy one more night of safe sleep."*

### 17.1 Construction Model

Construction operates on two layers. The tile grid is fixed from building templates — the player cannot modify floors, walls, or terrain. Players place, modify, and destroy objects on the object layer on top of the existing tile structure. Built structures are ECS entities distinct from the template tile grid beneath them.

The player targets a tile or tile edge and selects a construction option from the contextual interaction prompt. A brief animation plays — the character is occupied for several seconds, vulnerable and generating soft-tier noise. The structure appears when complete.

### 17.2 Fortification Objects

| Object | Description | Notes |
|---|---|---|
| **Wooden barricade** | Boards nailed across a window or doorway | Slows zombie entry. Multiple stack — triple-barricaded door takes significantly longer to breach. |
| **Metal barricade** | More durable than wood | Requires metal scraps and tools. Fire-resistant. |
| **Door brace** | Plank wedged under a door handle | Cheap, fast, temporary. Buys time, not security. |
| **Noise trap** | Tin cans on wire across a doorway | Wakes the player. Also alerts zombies. |
| **Floor spike** | Nails through a board, placed flat | Damages anything walking over it, including the player. One-time use. |
| **Fire trap** | Flammable trigger connected to fuel source | Destroys itself on activation. High risk in enclosed spaces. |

### 17.3 Functional Structures

| Object | Function | Notes |
|---|---|---|
| **Campfire** | Cooking, warmth, light | Requires fuel and safe surface. Medium-tier noise and light — attracts zombies. |
| **Reinforced campfire** | Contained fire in a metal drum | Slightly less noise, same warmth. More materials. |
| **Workbench** | Enables advanced crafting combinations | Requires planks and screws. |
| **Storage crate** | Built container with extra capacity | Persists into legacy runs. |
| **Sleeping area** | Cleared space with improvised bedding | Upgrades sleep quality from marginal to safe. |
| **Generator** | Found, not built — placed and connected | Powers electric lights. Loud-tier noise constantly. Only rational at a well-fortified location. |

**Note on generator noise and sleep:** A generator running at a barricaded base generates loud-tier noise attracting zombies to the barricades all night. Over enough nights, the barricades take cumulative damage and will eventually fail during sleep. Generators have a hidden cost: barricade repair rate. Players discover this interaction naturally.

### 17.4 Materials

- **Planks** — from wooden furniture, pallets, wood piles. Common.
- **Nails** — from hardware stores, toolboxes, construction sites. Moderate.
- **Metal scraps** — from industrial districts, vehicles, appliances. Less common.
- **Rope and wire** — from hardware and camping gear. Moderate.
- **Screws and bolts** — from toolboxes. Less common.
- **Fuel** — from gas stations, generators, jerry cans. Scarce and heavy.

Tools affect construction quality and speed. A hammer builds faster and stronger than a rock. A crowbar dismantles barricades without destroying the materials — recoverable for rebuilding elsewhere.

### 17.5 Degradation and Repair

All player-built structures degrade. Barricades take damage from zombie attacks. Rain accelerates wood degradation. Metal degrades slower. Sprite shows visible damage states — intact, damaged, breaking, broken. Repair requires the same materials at a reduced cost. Neglected bases decay and eventually force the player to repair or relocate.

### 17.6 Legacy Interaction

Built structures persist into the next run in one degradation stage below their current state. Fully intact persists as damaged. Damaged persists as breaking. Breaking does not persist. Structures never survive indefinitely across runs — recent construction carries forward meaningfully, old construction decays to nothing.

---

## 18. Vehicles

> *"A car is noise, speed, storage, and a weapon simultaneously. No other system creates decisions like it."*

### 18.1 Decision

Vehicles are driveable, scoped tightly. Three types only. Vehicles are large entities on the object layer occupying multiple tiles as a single object. The player finds them in various condition states throughout the world.

### 18.2 Vehicle Types

| Type | Speed | Storage | Noise | Seats | Notes |
|---|---|---|---|---|---|
| **Sedan** | Moderate | Moderate | Loud | Player + 1 NPC | Most common vehicle. |
| **Pickup Truck** | Slower | Large | Louder | Player + 1 NPC | Can carry large items impossible on foot. |
| **Bicycle** | Slow | None (player carry only) | Whisper | Player only | The only vehicle that does not attract zombies through noise. Invaluable for stealth. |

**Bicycle and surface noise:** The bicycle generates whisper-tier movement noise by default. Surface modifiers still apply — a bicycle on gravel generates soft-tier. The bicycle is silent, not magically silent.

### 18.3 Vehicle Mechanics

**Starting** — Requires a key found in the world near the vehicle, or hotwiring — requires mechanical skill, takes time, generates noise, possible failure.

**Fuel** — Vehicles consume from the world's finite supply. Fuel is heavy — carrying extra cans has inventory cost. Running dry leaves the vehicle as a storage container and obstacle.

**Condition** — Four states: Good → Worn → Damaged → Broken. Driven vehicles degrade slowly. Collision degrades faster. Damaged vehicles are slower and louder. Broken vehicles retain storage utility.

**Noise** — Running motor vehicles generate loud-tier noise constantly. Moving through a district announces the player's presence across a wide radius.

**As a weapon** — Driving into zombies damages and knocks them aside. Damages vehicle condition rapidly. Viable as emergency escape only.

**Storage** — Vehicle inventory persists in the world even when the player is not using it. A car used as a remote supply cache is a valid strategy.

**Legacy** — Vehicles persist across runs through world memory. A car parked and abandoned in a previous run is still there, condition degraded one stage. Supplies inside persist unless NPCs looted it.

---

## 19. Fire Propagation

> *"Fire is a tool that does not negotiate. Once started, it follows its own rules regardless of intent."*

### 19.1 Flammability Model

Each tile type has a flammability rating. Fire spreads tile by tile based on these ratings.

| Rating | Examples | Behaviour |
|---|---|---|
| **Non-flammable** | Concrete floors, metal surfaces, tile | Fire does not spread. Natural firebreaks. |
| **Low flammability** | Wooden floors, standard furniture | Fire spreads slowly, requires sustained adjacent flame. |
| **High flammability** | Wooden structures, fabric, paper, wooden barricades, dry grass | Fire spreads quickly. |
| **Fuel** | Accelerant-soaked surfaces, gas stations, fuel cans | Instant ignition, aggressive spread. |

Spread calculation runs every few game seconds per active fire tile. Adjacent flammable tiles have a percentage chance to ignite based on flammability and fire intensity. Fire intensity increases as it burns — a fire burning for thirty seconds spreads faster than one just started.

### 19.2 Fire as a System

- **Light** — Significant light radius. Attracts zombies through both light and noise.
- **Noise** — Active fire generates medium-tier noise continuously. Large fires generate loud-tier.
- **Heat** — Proximity raises body temperature. Warming in winter. Standing too close causes damage.
- **Damage** — Fire damages any entity on or adjacent to a burning tile. Zombies do not understand fire — they walk through it if it stands between them and the player.
- **Structure destruction** — Fire destroys wooden barricades, doors, and furniture. Metal and stone survive. A well-defended wooden base is not fire-safe.

### 19.3 Weather Interaction

Rain reduces fire spread probability per tick. Heavy rain can suppress small fires entirely and will eventually extinguish large fires. A fire started just before rain becomes a timed tool. Wind direction affects spread — fire travels faster downwind. Drought conditions in summer increase outdoor fire spread significantly.

### 19.4 Player Interaction

**Starting fire:** Molotovs, lighters on flammable surfaces, fire traps triggering, uncontrolled campfires.

**Suppressing fire:** Water containers dumped on burning tiles, sand or soil items, creating firebreaks by removing flammable objects from the fire's path, waiting for rain.

The player cannot fully control fire once it reaches high intensity. This is intentional. A molotov thrown in a panic inside a building has ended many runs.

---

## 20. Technical Stack

### 20.1 Core

| Component | Choice | Notes |
|---|---|---|
| **Language** | C++ | Raw performance, direct memory control, no GC pauses. |
| **Windowing / Input** | SDL2 | Cross-platform, minimal overhead. |
| **Rendering** | OpenGL | GPU sprites and tilemaps. Pixel-perfect upscaling at the OpenGL level. |
| **Build System** | CMake | Cross-platform build configuration. Standard for C++ projects. |

### 20.2 Support Libraries

| Library | Purpose |
|---|---|
| **SDL_mixer** | Audio — sound effects and ambient audio. |
| **stb_image** | Single-header PNG loading for sprite sheets. |
| **nlohmann/json** | Save files, world state, run history, and legacy data as JSON. |
| **Tiled + custom loader** | Design building templates. Load and assemble procedurally at runtime. |

### 20.3 Art Pipeline

- **Aseprite** — pixel art and animation. Export sprite sheets directly into asset folder.
- **Sprite size** — 16×16 or 32×32 characters. Fast to animate, easy to variant.
- **Animations** — 4–8 frames per action (walk, attack, forage, death). Sufficient at pixel scale.
- **Tilesets** — one tileset per district type covering indoor and outdoor environments. Infected variants through tile layer swaps.

### 20.4 Architecture

**Pattern:** Entity Component System (ECS) — entities are IDs, components are pure data, systems contain logic. Player and NPCs share the same component definitions.

**NPC pathfinding:** A\* for active-zone NPCs. Waypoint-based for adjacent-zone NPCs. Compressed probability resolution for distant-zone NPCs.

**Noise model:** Tile-radius propagation with environmental modifier lookup table. Event-driven, not continuous.

**Map seed:** Generated once at save slot creation. Stored in the save file. World layout is deterministic from this seed across all runs in the slot.

### 20.5 Save System

**Run state** is saved automatically on three triggers: player sleeps, player enters a new district, and every 10 real-time minutes as a background checkpoint. On death or retirement, the run state file is deleted — permadeath is enforced by file deletion, not by a flag. The most recent checkpoint is a recovery mechanism against crashes, not a save-scum vector — the checkpoint overwrites itself and is deleted on death identically to the manual save.

**Legacy state** is stored in a separate persistent world file. This file is the most valuable data in the game. It is written using atomic write — write to a temporary file, then rename to replace the original. A backup copy of the previous version is retained as `[slot]_legacy.backup.json`. If the primary file fails integrity check on load, the backup is offered automatically. Legacy state is only written at run end (death, retirement, or quit-to-menu) — never mid-run.

**Format** is JSON for both files during development. If serialisation time or file size becomes a bottleneck at Layer 8+ (world simulation with full NPC state and per-tile tracking), migrate to a binary format or chunked district-level files. Profile before migrating — JSON may be sufficient for the scope of this project.

### 20.6 Audio Design

Audio is a critical system in a game where sound is the primary threat mechanic.

**Positional audio:** Sounds have world positions. The engine calculates left/right pan and volume based on distance and direction from the player. Consistent with the noise propagation model. SDL_mixer provides basic stereo panning via `Mix_SetPosition()`. If wall occlusion becomes important to the audio feel, consider migrating to OpenAL Soft for proper occlusion and spatialization.

**Ambient soundscapes:** Per-district, per-weather, per-time-of-day ambient layers. Residential at night sounds different from industrial at noon.

**Music policy:** Hopeless tone: no music. Grim but Human: sparse ambient music during downtime only. Brutal Arcade: more active soundtrack. No music during active danger in any tone.

**Sound effect scope:** Every system that generates noise in the propagation model must have a corresponding sound effect. Priority: movement tiers, weapon types, zombie states, weather, fire, and vehicle noise.

---

## 21. Minimum Viable Loop

> *"The smallest version of this game that is fun to play."*

The minimum playable loop is defined explicitly. This is the first implementation target. Everything beyond it is an expansion layer with an explicit cut line. Do not implement expansion layers before the core loop is fun.

### 21.1 Core Loop — Required for First Playable Build

- **Movement** with facing direction — WASD movement, mouse-facing, crouching.
- **One physical need** — hunger. Decay over time. Find food. Eat food.
- **Basic zombie** — wanders, attracted by noise, chases player, attacks.
- **Noise propagation model** — basic version. Movement generates noise. Zombies investigate.
- **One weapon type** — improvised. Swing, hit, zombie dies. Stamina cost.
- **Death** — player dies, simple death screen with days survived.
- **Restart** — run resets, new run begins in the same world.
- **One legacy feature** — named graves. Death location marked. Visible in subsequent runs.

This is a playable game. Not a complete game — but a loop that can be iterated on and that validates the core feel before any advanced system is built.

### 21.2 Expansion Layers — Add in This Order

Each layer is independent and additive. Test each before adding the next.

**Layer 1 — Full Needs and Sleep**
- Add thirst, fatigue, sleep debt, body temperature, illness.
- Add sleep quality system and barricade-based safe sleep.

**Layer 2 — Full Combat**
- Add all five weapon categories.
- Add weapon condition and degradation.
- Add grab mechanic.
- Add facing-based awareness and rear vulnerability.

**Layer 3 — Inventory and Loot**
- Add full inventory system with weight and capacity.
- Add container-based loot with district-appropriate tables.
- Add item degradation and food spoilage.

**Layer 4 — Mental Health**
- Add panic system with triggers and effects.
- Add depression system with triggers and effects.
- Add desensitisation mechanic and psychological retirement.

**Layer 5 — Base Building**
- Add object layer construction.
- Add barricades, noise traps, campfire, sleeping area.
- Add structure degradation.

**Layer 6 — NPC Survivors**
- Add NPC entities with needs simulation.
- Add utility AI decision system.
- Add interaction options — observe, approach, trade.
- Add NPC death, body persistence, time-of-death tracking.

**Layer 7 — Advanced Systems**
- Add full noise propagation environmental modifiers.
- Add scent and blood trail system.
- Add crafting discovery system with sequential combination model.
- Add vehicles — all three types.
- Add fire propagation.

**Layer 8 — World Simulation**
- Add weather and seasons.
- Add infection spread and zombie decomposition stages.
- Add NPC simulation tiers and compressed fast-forward resolution.

**Layer 9 — Full Legacy System**
- Add all five legacy features beyond named graves.
- Add journal entries and notebook crafting.
- Add bloodline traits including Hardened (retirement-only trait).
- Add world memory for burns, loot, and NPC events.
- Add retirement markers.

**Layer 10 — Game Modes and Polish**
- Add campaign mode with objective placement.
- Add tone presets.
- Add threat mode selection at run start.
- Add skill system with XP and degradation.
- Add trait system at character creation.

### 21.3 Cut Line Philosophy

**Layers 1 through 5** constitute a complete and satisfying survival game.

**Layers 6 through 10** are what make it extraordinary.

Build the extraordinary only after the satisfying is working. This is an exploration project — the cut line exists to protect against building complexity before depth.

---

## 22. Development Roadmap

This is an exploration project. Timelines are guidelines, not deadlines. Go as deep as desired on any system. Rebuild when understanding improves. A realistic estimate for reaching the minimum viable loop is 2–3 months. A realistic estimate for a feature-complete prototype across all layers is 2–3 years of sustained part-time work. Both are fine.

### Month 1 — C++ Foundations + SDL2

- Learn pointers, memory management, headers, and CMake.
- Get a window open, a sprite rendering on screen, basic keyboard and mouse input.
- Implement a basic game loop with delta time.

### Month 2 — ECS Architecture + Noise Model

Build the ECS from scratch and prototype the noise propagation model early — it is the foundational mechanic every other system references. If the noise model does not feel right, nothing built on top of it will.

- Entity manager with component registration.
- Transform, sprite, velocity, and needs components.
- Basic AABB collision detection.
- Noise event system with tile-radius propagation.
- Zombie entity attracted by noise — first dangerous entity in the game.

### Month 3 — Tile Map + Camera + Minimum Viable Loop

- Tiled JSON map parser and tile layer rendering.
- Camera with smooth follow and boundary clamping.
- Collision layer from tile map data.
- Hunger need with decay. Food items. Death from starvation or zombies.
- Death screen. Named graves. **The minimum viable loop is now playable.**

### Month 4 — Full Needs + Sleep System

- All six physical needs with decay rates and interactions.
- Sleep debt system with escalating effects and collapse.
- Sleep quality based on location and barricades.

### Month 5 — Full Combat System

- All five weapon categories with condition degradation.
- Grab mechanic with stamina cost.
- Facing-based awareness and rear vulnerability.
- Positioning — shove, knockdown, chokepoint behaviour.

### Month 6 — Inventory, Loot, Base Building

- Full inventory system with weight and capacity.
- Container-based loot with district tables.
- Object layer construction — barricades, campfire, sleeping area, noise traps.
- Structure degradation and repair.

### Month 7 — Mental Health + Scent + Blood Trail

- Panic system with full trigger and effect chain.
- Depression system including desensitisation and psychological retirement.
- Blood trail markers and zombie tracking behaviour.

### Month 8 — NPC Survivors v1

- NPC entities with needs simulation.
- Utility AI decision system.
- Three simulation tiers with compressed fast-forward.
- Basic interaction — observe, approach, trade.
- Start with the simplest possible NPC: one wandering survivor with one need (hunger) and one behaviour (scavenge or flee). Iterate from there.

### Month 9+ — Advanced Systems and World Simulation

- Weather and seasons with full environmental modifier integration.
- Infection spread and zombie decomposition stages.
- Fire propagation system.
- Vehicles — all three types with full mechanics.
- Crafting discovery system with sequential combination model and notebook persistence.

### Month 12+ — Legacy, Modes, and Polish

- Full five-feature legacy system including retirement markers.
- Campaign mode with objective placement and infection timeline.
- Threat mode selection and tone presets.
- Skill and trait systems.
- Full UI polish — all panels, death screen, retirement screen, journal.

---

*This document is complete. Every major system is specified. All architectural conflicts have been resolved. The minimum viable loop is defined. The expansion layers are sequenced. The tech stack is confirmed. Dead Pixel Survival is ready for implementation.*

**— End of Document — GDD v0.3 — Dead Pixel Survival —**
