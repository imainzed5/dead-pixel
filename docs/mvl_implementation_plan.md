# Plan: Dead Pixel Survival — MVL Implementation

## TL;DR
Build the Minimum Viable Loop (GDD §21.1) from zero — no code exists yet. The developer is brand new to C++ with only VS Code installed on Windows. The plan covers environment setup through a playable loop: move, eat, fight zombies, die, see grave next run. Ten phases, each independently verifiable, structured so each phase teaches necessary C++ concepts incrementally.

**Recommended toolchain:** MSYS2 (MinGW-w64 GCC) + CMake + Ninja. SDL2 and SDL2_mixer installed via MSYS2's pacman. No vcpkg needed for MVL scope. Lightweight, cross-platform skills, all dependencies in one `pacman` command.

---

## Phase 0 — Environment + Hello Window
**Goal:** Working C++ build chain. An SDL2 window opens and closes cleanly.
**C++ concepts introduced:** compilation, headers, linking, CMake basics, main(), includes.
**Depends on:** Nothing.

1. Install MSYS2 from msys2.org. Open UCRT64 terminal.
2. Install toolchain: `pacman -S mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_mixer`
3. Add `C:\msys64\ucrt64\bin` to Windows PATH.
4. Install VS Code extensions: **C/C++** (Microsoft), **CMake Tools** (Microsoft).
5. Initialize git repo (`git init`), create `.gitignore` for build artifacts.
6. Create project directory structure (see "Directory Structure" below).
7. Write `CMakeLists.txt` — find SDL2 package, create executable, link SDL2.
8. Write `src/main.cpp` — SDL_Init, SDL_CreateWindow, event loop polling SDL_QUIT, SDL_Delay, SDL_DestroyWindow, SDL_Quit.
9. Build with CMake + Ninja via VS Code CMake Tools extension.
10. Move existing GDD files into `docs/`.

**Verification:**
- `cmake --build build` succeeds with zero errors.
- Running the executable opens a titled window that closes on X or Escape.

---

## Phase 1 — OpenGL Rendering + Sprite on Screen
**Goal:** Render a 32×32 placeholder sprite using OpenGL through SDL2.
**C++ concepts introduced:** pointers, structs, header/source file pairs, `#include` guards, basic memory (stack allocation).
**Depends on:** Phase 0.

1. Create SDL2 window with OpenGL 3.3 Core context (`SDL_GL_SetAttribute`, `SDL_GL_CreateContext`).
2. Load OpenGL function pointers — use `glad` (generated from glad.davemorris.io for GL 3.3 Core) or raw `SDL_GL_GetProcAddress`. Recommend glad for simplicity.
3. Write vertex + fragment shaders for textured 2D quads (`assets/shaders/sprite.vert`, `sprite.frag`). Shader takes a model-view-projection matrix, texture sampler, and vertex color.
4. Create `Shader` class — loads, compiles, links shader programs. Error reporting on compile failure.
5. Add `stb_image.h` to `third_party/`. Create `Texture` class — loads PNG, creates GL texture, stores width/height.
6. Create `SpriteBatch` class — collects quad vertices (position, UV, color) into a dynamic VBO, flushes in a single draw call. Batch key: texture ID.
7. Create a placeholder sprite sheet: a 128×128 PNG with a few 32×32 colored squares (player, zombie, food, wall, grave). Can be drawn in any paint tool — does not need Aseprite yet.
8. Set up orthographic projection matrix matching the window size. Render one sprite at screen center.

**Verification:**
- A colored 32×32 sprite renders at a known position. Background is a distinct clear color.
- Resizing the window maintains pixel-perfect rendering.

---

## Phase 2 — Game Loop + Input + Movement
**Goal:** Player sprite moves with WASD and faces the mouse cursor.
**C++ concepts introduced:** classes with methods, enums, delta time (float arithmetic), basic math (atan2, vector normalization).
**Depends on:** Phase 1.

1. Create `Game` class — owns the window, renderer, and runs the main loop. Fixed timestep update (e.g., 60 ticks/sec), variable render. `Game::run()` contains the loop.
2. Create `Input` class — polls SDL events each frame, stores key states (pressed/held/released) and mouse position + button states. Accessible as a singleton or passed to systems.
3. Player entity (ad-hoc struct for now): position (float x, y), velocity, speed, facing angle, crouching flag.
4. WASD → velocity vector → normalize diagonal → multiply by speed × dt → update position. Crouching (Ctrl or C) halves speed.
5. Mouse screen position → world position conversion (accounting for camera later). `atan2(mouseY - playerY, mouseX - playerX)` → facing angle.
6. Render player sprite rotated or flipped based on facing direction (left/right flip is simplest for now; 4-directional facing if preferred).
7. Frame-rate independent movement using delta time.

**Verification:**
- Player moves in 8 directions at consistent speed. Diagonal is not faster.
- Player sprite faces toward the mouse cursor.
- Crouching visibly slows movement. FPS counter shows stable frame rate.

---

## Phase 3 — ECS Architecture
**Goal:** Refactor Phase 2's ad-hoc player into a proper Entity Component System. This is the backbone everything else plugs into (GDD §20.4).
**C++ concepts introduced:** templates, `std::unordered_map`, `std::vector`, type erasure basics, `static_cast`, dynamic allocation (`new`/`delete` or `std::unique_ptr`).
**Depends on:** Phase 2 (refactoring existing code).

1. Define `Entity` as a `uint32_t` ID. `EntityManager` — creates IDs (incrementing counter), recycles destroyed IDs via a free list.
2. `ComponentArray<T>` — templated. Dense array of T indexed by entity. Sparse array maps entity → dense index. Supports add, remove, get, has.
3. `World` class — the ECS coordinator. Registers component types, creates/destroys entities, provides `getComponent<T>(entity)`. Stores systems.
4. Define initial components as plain structs (no methods, just data):
   - `Transform` — float x, y, rotation.
   - `Sprite` — texture ID, source rect (for sprite sheet region), layer/z-order.
   - `Velocity` — float dx, dy.
   - `PlayerInput` — tag component (marks the player entity).
   - `Facing` — float angle, bool crouching.
5. Define initial systems as classes with an `update(World&, float dt)` method:
   - `InputSystem` — queries entities with PlayerInput + Velocity + Facing. Reads Input state, writes Velocity and Facing.
   - `MovementSystem` — queries Transform + Velocity. Applies velocity × dt to position.
   - `RenderSystem` — queries Transform + Sprite. Sorts by z-order. Submits to SpriteBatch.
6. Migrate the player from Phase 2 into the ECS. Behaviour must be identical.

**Verification:**
- Identical behaviour to Phase 2 (movement, facing, rendering).
- Can create multiple entities (e.g., spawn 10 static "test" entities) and all render correctly.
- Destroying an entity removes it from rendering.

---

## Phase 4 — Tile Map + Camera
**Goal:** Load a Tiled JSON map, render it, player collides with walls, camera follows.
**C++ concepts introduced:** JSON parsing (nlohmann/json), file I/O (`std::ifstream`), 2D arrays, coordinate math (screen ↔ world ↔ tile).
**Depends on:** Phase 3.

1. Add `nlohmann/json.hpp` to `third_party/`.
2. Create a simple test map in **Tiled**: ~40×30 tiles, 32×32 tile size. One tileset PNG. A few rooms connected by doorways. Outdoor area. Mark wall tiles with a custom property `solid: true` or use a dedicated collision layer.
3. `TiledLoader` — reads the JSON, extracts tile layers, tileset references, collision data, and tile properties (like surface type for noise later).
4. `TileMap` — stores layers as 2D arrays of tile IDs. Provides `isSolid(tileX, tileY)` and `getSurfaceType(tileX, tileY)`.
5. Tile rendering: iterate visible tiles (based on camera viewport), submit to SpriteBatch using tileset texture + source rects computed from tile IDs.
6. `Camera` — position (float x, y), viewport size, `worldToScreen()` and `screenToWorld()` conversion. Smooth follow: lerp toward player position each frame. Clamp to map bounds.
7. Update SpriteBatch and Input to use camera transforms. Mouse → world position now accounts for camera offset.
8. `CollisionSystem` — before applying movement, check if the target position overlaps any solid tile. Simple AABB: player has a collision box (smaller than 32×32 — e.g., 20×20 centered at feet). Slide along walls (resolve X and Y independently).

**Verification:**
- Multiple rooms render from the Tiled map. Player walks between rooms through doorways.
- Player cannot walk through walls. Sliding along walls feels natural.
- Camera smoothly follows the player. Does not show beyond map edges.

---

## Phase 5 — Noise Propagation Prototype
**Goal:** Player actions generate noise events with tile radii. Visual debug overlay. Foundation for zombie AI.
**C++ concepts introduced:** `std::queue`/`std::vector` for event processing, game time tracking, distance calculations.
**Depends on:** Phase 4 (needs tile map for surface types).

1. `NoiseEvent` struct: world position, radius (in tiles), remaining duration (seconds), source entity ID.
2. `NoiseModel` class: active event list. `emitNoise(position, tier, duration)`. Each frame: decay durations, remove expired. Provide `getNoiseAtPosition(worldPos)` — returns highest active noise intensity at that point.
3. Noise tier enum mapping to radii: Whisper(3), Soft(7), Medium(15), Loud(30), Explosive(60). Values from GDD §12.2.
4. `MovementSystem` emits noise on player movement: standing still = none, crouching walk = Whisper, normal walk = Soft, running (Shift held) = Medium.
5. Surface modifier: `TileMap::getSurfaceType()` adjusts tier up/down. Carpet/grass = -1 tier, gravel/metal = +1 tier, crouching = -1 tier (stacks).
6. **Debug overlay** (toggle with F1): render translucent circles at each active noise event's position + radius. Color-coded by tier. This visualisation is critical for tuning and will remain useful through all future development.

**Verification:**
- Debug overlay shows circles appearing around the player that vary with movement speed, crouch state, and floor surface.
- Standing still produces no noise. Running on gravel shows Medium+ circles. Crouching on carpet shows nothing.

---

## Phase 6 — Zombie AI
**Goal:** Zombies wander, investigate noise, chase on sight, attack the player.
**C++ concepts introduced:** state machines (enum + switch), A* pathfinding algorithm, `std::priority_queue`, line-of-sight raycasting.
**Depends on:** Phase 5.

1. `ZombieAI` component: state enum (Wander, Investigate, Chase, Attack), target position, state timer, detection radius.
2. `Health` component: current HP, max HP. Added to player and zombies.
3. A* pathfinding on the tile grid. `Pathfinder` class: takes start tile + end tile + TileMap, returns path as `std::vector<Vec2>`. Cache or limit path requests per frame (max 2-3 per frame for performance).
4. **Wander state:** pick a random walkable tile within 5-tile radius, path to it. On arrival, idle for 1-3 seconds, pick again.
5. **Investigate state:** triggered when a noise event overlaps the zombie's position. Path toward the noise source. On arrival, if player not visible, return to Wander after a short search.
6. **Chase state:** triggered when zombie has line-of-sight to player within ~8 tiles. Recalculate path to player every 0.5 seconds. Faster movement than wander. Generate Medium-tier noise (zombie growls — GDD §12.2).
7. **Attack state:** when adjacent to player (1 tile range). Deal damage to player Health. Brief cooldown between attacks.
8. Line-of-sight: Bresenham's line or simple tile raycast — check if any solid tile blocks the line from zombie to player.
9. Spawn 5-10 zombies at designated positions on the test map at load time.
10. Player death: when Health reaches 0, transition to death state (freeze input, play death — Phase 9 will add the full death screen).

**Verification:**
- Zombies wander aimlessly when undisturbed.
- Running near zombies (visible in noise debug overlay) causes them to investigate.
- Zombies spotting the player chase aggressively. They path around walls.
- Zombies deal damage. Player can die.

---

## Phase 7 — Combat + Stamina
**Goal:** Player can fight back. Swinging costs stamina. One weapon type: improvised.
**C++ concepts introduced:** hitbox geometry (arc intersection), resource management patterns, game feel tuning constants.
**Depends on:** Phase 6.

1. `Stamina` component: current, max, recovery rate. Added to player.
2. `Combat` component: equipped weapon (struct with arc width, range in tiles, damage, stamina cost, durability, condition), attack cooldown timer, attack state (idle/windup/active/recovery).
3. One weapon definition: **Improvised** — arc width 90°, range 1.5 tiles, low damage, stamina cost 15, durability 8 uses.
4. Left-click triggers attack toward mouse-facing direction. Attack phases: windup (0.2s, no damage, player locked) → active (0.1s, arc hitcheck) → recovery (0.3s, no input, vulnerable). Missed swings cost the same stamina.
5. Arc hit detection: for each zombie, check if it's within range AND within the arc angle centered on the facing direction.
6. **Visual arc indicator**: during windup, draw a translucent arc showing the hit zone. Gives the player feedback before the hit resolves.
7. Stamina drain per swing. Recovery when not attacking (1-2 seconds idle). Exhausted state below 20%: swings may miss (random chance), movement slowed (multiply speed by 0.6).
8. Weapon condition: each hit decrements durability. When durability reaches 0, weapon breaks — damage and arc halved, model it as a weaker fallback. Pickup replacement weapons from world (simple item entities for now).
9. Combat generates Medium-tier noise per swing (GDD §8.8).
10. Zombie knockback on hit: push zombie 0.5-1 tile away from player.

**Verification:**
- Player clicks to swing, arc indicator shows, zombies in arc take damage and die after enough hits.
- Stamina bar visibly drains. Exhausted state is noticeable (missed swings, slow movement).
- Weapon breaks after N uses. Combat noise attracts nearby zombies.

---

## Phase 8 — Hunger + Food + Needs Death
**Goal:** Hunger decays over time. Food exists in the world. Starvation kills.
**C++ concepts introduced:** gradual system decay, item interaction patterns, simple state machines for items.
**Depends on:** Phase 7.

1. `Needs` component: hunger (float 0.0 to 100.0), decay rate per second (base ~0.02/sec ≈ empty in ~80 min game time). Running doubles decay. Crouching halves it.
2. `NeedsSystem`: ticks hunger down each frame. When hunger < 20: reduce max stamina by 40% (interacts with Stamina component). When hunger reaches 0: drain Health slowly (starvation death).
3. Food item entities placed in the world (on the test map, put 5-8 food items in rooms).
4. `Interactable` component: interaction type (Pickup), displayed prompt text.
5. Interaction system: when player is within 1 tile of an interactable and presses E, execute the interaction. For food: remove entity, add 25-40 hunger points.
6. Simple HUD element (top-left): hunger as a 5-segment bar with bread icon (GDD §3.3). Use a simple colored rectangle bar for now — pixel art HUD comes in polish.
7. Day/time system: `GameClock` class tracking elapsed in-game time. Configurable day length (e.g., 1 real minute = 1 game hour → 24 min real time per day). "Day X" counter displayed top-center.

**Verification:**
- Hunger bar depletes visibly over time. Running drains faster.
- Eating food restores hunger. When hunger hits 0, player slowly dies.
- Days survived counter increments. Hunger being low visibly reduces stamina max.

---

## Phase 9 — Death Screen + Named Graves + Restart = MVL COMPLETE
**Goal:** Full death → legacy → restart loop. The minimum viable loop is playable.
**C++ concepts introduced:** JSON file I/O (read/write), game state management, scene transitions.
**Depends on:** Phase 8.

1. **Game state machine**: enum of states (Playing, Dead, Restarting). `Game` class transitions between them.
2. **Death trigger**: when player HP reaches 0 (combat or starvation), record: death position, day count, cause of death string (e.g., "Killed by zombies on Day 3" or "Starved on Day 7"), character name (hardcoded or simple random generator from a name list).
3. **Death screen** (GDD §3.8): transition to Dead state. Render the world frozen and desaturated (grayscale shader or tinted overlay). Overlay text in a clean pixel font:
   - Character name
   - "Survived X days"
   - Cause of death
   - "Their grave has been marked."
   - After 3 seconds: "Begin again" prompt. Press Enter or click to restart.
4. **World seed**: generate a random uint32 seed on first-ever run. Store in `save/[slot].json`. On map load, seed the RNG with this value. Same seed → same tile map layout.
5. **Legacy file** (`save/[slot]_legacy.json`): JSON array of grave entries. Each entry: `{ name, x, y, day, cause }`. On death: append entry, write file (atomic write: write to `.tmp`, rename over original).
6. **Grave entities**: on each new run, after map loads, read legacy file. For each grave entry, spawn a grave entity at (x, y) with a `Sprite` (cross/tombstone from the placeholder sheet) and an `Interactable` that shows "Here lies [name] — Survived [X] days — [cause]".
7. **Restart**: on "Begin again", reset all game state (destroy all entities, reload map, respawn player, roll new loot/zombie positions with a per-run seed derived from run number + world seed). Legacy file persists — graves appear.
8. **Per-run seed**: `worldSeed XOR runNumber` produces a different per-run seed for loot and zombie placement, while the map layout seed stays fixed. Increment run number in the legacy file.

**Verification (the MVL acceptance test):**
1. Launch game. Walk around. Find food, eat it. Fight zombies.
2. Die (to zombies or starvation).
3. Death screen shows name, days survived, cause of death.
4. Press Enter. New run starts. Same map layout.
5. Walk to previous death location. **Find your own grave.** Interact — shows previous character's name and survival stats.
6. Die again. Restart. Two graves now exist.
7. **The loop is complete. The MVL is playable.**

---

## Directory Structure

```
dead-pixel/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── docs/
│   ├── dead_pixel_survival_gdd_v0.3.md
│   └── dead_pixel_survival_gdd.html
├── src/
│   ├── main.cpp                    — Entry point, creates Game, calls run()
│   ├── core/
│   │   ├── Game.h / .cpp           — Game loop, state machine, owns World
│   │   ├── Window.h / .cpp         — SDL2 window + OpenGL context
│   │   ├── Input.h / .cpp          — Keyboard/mouse state
│   │   └── GameClock.h / .cpp      — In-game time, day counter
│   ├── ecs/
│   │   ├── Types.h                 — Entity = uint32_t, component type IDs
│   │   ├── EntityManager.h / .cpp  — Create/destroy/recycle entity IDs
│   │   ├── ComponentArray.h        — Templated dense+sparse component storage
│   │   └── World.h / .cpp          — ECS coordinator
│   ├── rendering/
│   │   ├── Shader.h / .cpp         — Load, compile, link GL shaders
│   │   ├── SpriteBatch.h / .cpp    — Batched textured quad rendering
│   │   ├── Texture.h / .cpp        — PNG loading via stb_image → GL texture
│   │   └── Camera.h / .cpp         — Orthographic camera, smooth follow
│   ├── components/
│   │   ├── Transform.h             — x, y, rotation
│   │   ├── Sprite.h                — texture ID, source rect, z-order
│   │   ├── Velocity.h              — dx, dy
│   │   ├── PlayerInput.h           — Tag component
│   │   ├── Facing.h                — angle, crouching flag
│   │   ├── Health.h                — current, max
│   │   ├── Stamina.h               — current, max, recovery rate
│   │   ├── Needs.h                 — hunger (float 0-100)
│   │   ├── ZombieAI.h              — state, target, timers
│   │   ├── Combat.h                — weapon stats, attack state, cooldown
│   │   └── Interactable.h          — type enum, prompt text, data
│   ├── systems/
│   │   ├── InputSystem.h / .cpp
│   │   ├── MovementSystem.h / .cpp
│   │   ├── CollisionSystem.h / .cpp
│   │   ├── RenderSystem.h / .cpp
│   │   ├── NoiseSystem.h / .cpp
│   │   ├── ZombieAISystem.h / .cpp
│   │   ├── CombatSystem.h / .cpp
│   │   ├── NeedsSystem.h / .cpp
│   │   ├── InteractionSystem.h / .cpp
│   │   └── DeathSystem.h / .cpp
│   ├── map/
│   │   ├── TileMap.h / .cpp        — Tile data, collision, surface queries
│   │   └── TiledLoader.h / .cpp    — Tiled JSON parsing
│   ├── ai/
│   │   └── Pathfinder.h / .cpp     — A* on tile grid
│   ├── noise/
│   │   └── NoiseModel.h / .cpp     — Event queue, radius propagation
│   ├── save/
│   │   └── SaveManager.h / .cpp    — World seed, legacy graves, JSON I/O
│   └── ui/
│       ├── HUD.h / .cpp            — Hunger bar, day counter
│       └── DeathScreen.h / .cpp    — Death overlay rendering
├── assets/
│   ├── shaders/
│   │   ├── sprite.vert
│   │   └── sprite.frag
│   ├── sprites/
│   │   └── placeholder.png         — 32×32 placeholder sprite sheet
│   ├── maps/
│   │   └── test_map.json           — Simple Tiled map for development
│   └── fonts/
│       └── pixel_font.png          — Bitmap font for UI text
└── third_party/
    ├── glad/                       — OpenGL loader (generated)
    │   ├── glad.h
    │   └── glad.c
    ├── stb_image.h
    └── nlohmann/
        └── json.hpp
```

---

## Relevant Files (to create or reuse)

- `CMakeLists.txt` — **Create.** Find SDL2 via `find_package`, glob or list sources, link SDL2 + OpenGL + SDL2_mixer.
- `src/main.cpp` — **Create.** Entry point.
- `src/core/Game.h / .cpp` — **Create.** Central game class. Reuse pattern: fixed timestep loop from Phase 2, extended through every phase.
- `src/ecs/World.h / .cpp` — **Create.** The ECS coordinator. Phase 3 is dedicated to this. Every subsequent phase adds components and systems to it.
- `src/rendering/SpriteBatch.h / .cpp` — **Create.** Phase 1. Used by everything that renders. Must support batching 1000+ quads for tile maps.
- `src/noise/NoiseModel.h / .cpp` — **Create.** Phase 5. The foundational mechanic per GDD — prototyped early (Month 2 in roadmap) so the game's feel can be validated.
- `src/ai/Pathfinder.h / .cpp` — **Create.** Phase 6. A* implementation reused by zombie AI and later by NPC AI (Layer 6+).
- `src/save/SaveManager.h / .cpp` — **Create.** Phase 9. Atomic write pattern from GDD §20.5. Legacy grave persistence.
- `assets/sprites/placeholder.png` — **Create.** Simple 128×128 PNG sprite sheet with colored 32×32 cells for player/zombie/food/grave/wall. Placeholder only — replaced when real art is made.
- `assets/maps/test_map.json` — **Create in Tiled.** Phase 4. ~40×30 tile map with rooms, doorways, outdoor area, different floor surfaces.
- `docs/dead_pixel_survival_gdd_v0.3.md` — **Move.** Already exists. Move into docs/ folder.

---

## Verification — Full MVL Acceptance Test

After Phase 9, run this manual test:

1. Launch the game. A tiled map renders. Player sprite is visible.
2. Move with WASD. Player faces mouse. Crouch with C — movement slows.
3. Hunger bar in top-left depletes over time. Running drains faster.
4. Find a food item in a room. Press E to eat. Hunger bar partially refills.
5. Encounter zombies wandering. Approach quietly (crouching) — they don't react.
6. Run near them — noise debug overlay shows they investigate.
7. Zombie spots you — chases. Another zombie hears the commotion and joins.
8. Click to swing weapon. Arc indicator shows. Hit zombie — it takes damage and is knocked back.
9. Kill a zombie after several hits. Weapon condition degrades.
10. Take damage from a zombie. Health decreases.
11. Die (from combat or starvation).
12. Death screen: name, days survived, cause. "Their grave has been marked." "Begin again."
13. Press Enter. New run starts. Same map layout.
14. Walk to previous death location. **A grave marker exists.** Interact — shows previous character's name and survival stats.
15. Die again. Restart. Two graves now exist.
16. **MVL is verified.**

---

## Decisions

- **Toolchain:** MSYS2 + MinGW-w64 (GCC/G++) over Visual Studio Build Tools. Lighter, teaches portable skills, SDL2 available via pacman.
- **OpenGL loader:** glad (generated for 3.3 Core) rather than GLEW — single file, no extra dependency.
- **Sprite size:** 32×32 confirmed. Placeholder art first — does not block any phase.
- **No audio in MVL:** Sound effects add atmosphere but are not required for the core loop. Add in Layer 2 or Layer 7 when the noise propagation model gets environmental modifiers.
- **Phases are sequential within the MVL** — each depends on the previous. No parallelism within MVL. After MVL, expansion layers can be tackled more flexibly.
- **ECS is built from scratch** (not a library like EnTT) — this is a learning project. The ECS in Phase 3 will be simple (max ~1000 entities for MVL scope). Can be replaced or upgraded later.
- **Test map is hand-made in Tiled** — no procedural generation in MVL. The fixed-seed procedural generator (GDD §2.2) is a post-MVL feature. MVL uses a single authored map to validate all systems.

---

## Scope Boundaries

**In scope for this plan (MVL):**
- Movement, facing, crouching
- Hunger need with starvation death
- Zombie AI: wander, investigate noise, chase, attack
- Noise propagation (basic: tiers, surface modifiers)
- One weapon type (improvised), stamina, combat
- Tile map rendering + camera + collision
- Death screen
- Named graves (legacy persistence across runs)
- Fixed world seed with per-run variance

**Explicitly out of scope (deferred to expansion layers):**
- Thirst, fatigue, sleep debt, body temp, illness (Layer 1)
- Multiple weapon types, grab mechanic, weapon condition states (Layer 2)
- Full inventory system with weight/capacity (Layer 3)
- Panic, depression, desensitisation (Layer 4)
- Base building (Layer 5)
- NPC survivors (Layer 6)
- Blood trails, crafting, vehicles, fire (Layer 7)
- Weather, seasons, infection spread (Layer 8)
- Full legacy system (Layer 9)
- Campaign mode, threat modes, skills, traits (Layer 10)
- Procedural map generation (post-MVL — MVL uses a hand-crafted test map)
- Real art assets (placeholder sprites throughout MVL)
- Audio/music (not required for MVL loop validation)

---

## Post-MVL Note

Once the MVL is verified and the core loop feels right, the GDD §21.2 expansion layers provide the implementation sequence. Each layer gets its own implementation plan at that time — not now. The MVL plan exists to reach a playable game as fast as possible so the fundamental feel can be validated before complexity is added.










###############################





Roadmap

M0 — Scope lock and planning package. Freeze generator-first runtime, fixed-world-per-slot persistence, Tiled as tooling only unless later proven necessary, and a save migration policy. Produce the backlog document and the World Foundation Refactor spec in docs.
M1 — World Foundation Refactor. Split save data into slot meta, world layout, persistent world state, and run-state checkpoint. Make one layout per slot, then reuse it forever in that slot.
M2 — Prototype stabilization on the corrected foundation. Add contextual interaction prompts, better container handling, structure repair and dismantle loops, checkpoint behavior, and enough clarity to get trustworthy playtest data.
M3 — District-aware content pass. Add district identity, district-based loot and spawn weighting, and early world-memory hooks so the game stops behaving like a generic procedural prototype.
M4 — NPC Survivor v0. Limit scope to the active district, a small personality model, and three interactions: observe, approach, trade. Do not start with factions, betrayal, or multi-district simulation.
M5 — Psychology completion pass. Expand panic and depression using actual survivor presence, add the retirement crisis window, and add retirement-specific legacy behavior.
M6 — Weather and seasons v0. Add temperature curves, rain, storm states, visibility pressure, and noise interaction. Keep it systemic, not content-heavy.
M7 — Infection spread and zombie aging v0. Use districts and time progression to drive overwhelmed states, infection spread, and decomposition stages.
M8 — Crafting discovery and notebook system. Add the two-item combination loop, intermediate items, notebook persistence, and only a narrow first recipe set tied to existing survival systems.
M9 — Base building v2 and world memory. Expand structures, repairs, dismantling, generator interactions, and real persistent world-state consequences.
M10 — Vehicles and fire. Add both as narrow first slices only after save, districts, weather, and world memory are stable.
M11 — Full legacy completion. Add bloodline traits, retirement markers, richer inter-run history, and broader carryover.
M12 — Game modes and threat framing. Add Campaign, Endless, threat modes, and tone presets only after the strategic systems they depend on are already credible.
M13 — UI architecture overhaul. Rebuild the prototype HUD into the GDD layout after the system surface is mostly stable.
M14 — Audio, balance, and content polish. Add district-aware ambience, weather-aware audio, economy tuning, spawn tuning, and long-seed playtests.
M15 — Hardening and release-candidate prep. Do docs sync, regression coverage, save-version handling, crash recovery validation, and performance work.