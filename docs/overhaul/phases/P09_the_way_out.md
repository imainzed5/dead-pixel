# Phase 9: the way out

**Goal.** A campaign the town's memory wins: get out of town. Nobody can do
it in one life. Each life finds a piece, moves a piece, or leaves a note,
and one day someone opens the gate.

**The player notices:** a wall round the town they never questioned; a radio
voice; a name written in three places; a gate with a dead motor; a key on a
dead sergeant's belt; a last five minutes that are the loudest in the game.

**Feel target.** Life 9 reads life 4's notebook, walks to the gate with the
last can of fuel, and knows exactly how bad the next five minutes will be.

## The fiction, in six lines

The town was sealed in the first week. The brick wall round the map (it
exists) is the quarantine line. There is one way through: the **service
gate** in the far district, a steel gate with an electric motor. The guard
detail that held it is dead; their sergeant walked off with the key. The
motor's fuse is blown and its generator is dry. Past the gate is a road.
That is all anyone knows, and nobody says what is at the end of it.

## Core

### 1. The gate

- Placed once at world creation where the campaign objective is placed
  today (`ensureCampaignObjective`: the district farthest from the centre):
  a 3-tile steel gate set into the boundary wall at the end of the nearest
  road, a guard hut beside it (a small brick building: desk, locker,
  the **generator** outside under a lean-to, a **control box** on the wall).
  The generator, the box and the gate are new structure sprites; the gate
  has closed, opening (3 frames) and open states.
- The map generator must not change existing towns: the gate complex is
  stamped over whatever is there at creation for new worlds, and at first
  load for existing ones (deterministic from the world seed; record
  `gatePlaced` in the layout file; scenery replants round it).
- It exists in both game modes. In Endless nothing points to it and the
  notebook has no "way out" page until a clue is found; it can still be
  opened. (One game, two lenses.)

### 2. What opening it takes

Four things, each **world memory** once done:

| Need | Where it is | Persists as |
|---|---|---|
| **Gate Key** | Carried by "the Sergeant", a named Brute that walks a fixed district (seeded: not the gate's, not the start's). Killing it drops the key. If the key-holder dies, the key is in their pack by their grave, or on them if they rise | The key's whereabouts: on the Sergeant, in a pack, in a container, on the floor indoors, or turned in the box |
| **Gate Fuse** | In the Hardware store's stock room, in a locked cabinet (Crowbar) | Fitted in the control box (a held action, 10 s) |
| **Fuel** | 4 Fuel Cans poured into the generator (each a 6 s held action) | The generator's level, 0 to 4 |
| **A working generator** | It is seized: needs a Toolbox and Mechanics 3, **or** a Toolbox and 3 separate repair sessions of 60 s each by anyone (each session is Loud every 2 s) | Repair progress 0 to 3 |

The control box shows the state as four small lamps (dark, amber, lit) drawn
in the world, and E on it lists what is missing in the prompt line: "Needs:
fuse, fuel 1 of 4, generator."

### 3. Opening it: the last stand

With all four in place and the key in hand, E on the box: "Start it."

- The generator runs: Explosive noise every second for **180 s** (three
  game hours on the clock; three real minutes). The gate grinds open one
  third per minute. Every one of the dead within 60 tiles comes; the
  director spawns fresh ones at the edge of the district each 20 s (3, then
  4, then 5...). The gate district's infection sets the base crowd.
- The player must stay within 12 tiles of the box: leaving stops the motor
  (it restarts from where it was, using no extra fuel; each start costs 30 s
  of grinding before it moves again).
- The guard hut can be fortified beforehand (P02): this is what all the
  boards, alarms, spike boards and the companion were for. The hut has two
  windows and one door; the lean-to is open.
- At 180 s the gate is open. Walking through the gate tile ends the run:
  **"<Name> got out."**
- If the player dies during it: progress stays (fuel is used up at 1 can per
  60 s of running; the fuse and repairs stay; the key lies with the body).
  The gate keeps the fraction it reached for the rest of that run only; next
  life it has closed.

### 4. The ending and after

- Run-end screen variant "Out": name, days, the lives it took ("9 lives. 143
  days."), the names of every character whose work went into it (who found
  the fuse, who killed the Sergeant, who poured each can: record
  `contributors` on the gate state), and one line: "The road goes on."
- Legacy: the character gets a marker at the gate ("<Name> left by this gate
  on day 22."). The bloodline gains the trait **Wayfinder** (starts with the
  gate and every seen building kind on the map).
- The gate then stands open in that world for ever. Later lives may leave by
  it at any time, from day 1: that is the reward, and it turns the save into
  a different game (how much can this character do for the town before
  leaving, or will they stay?). A character who leaves is "gone", not dead
  or retired: no grave, no survivor. An open gate also lets the dead in: the
  gate district's population target x1.5 from then on.
- The title screen's "The town remembers" card adds "Got out: 1".
- A companion with you at the gate leaves too and is named on the screen.

### 5. Clues

The campaign is a chain of **leads**, each a fact with a text and, usually,
a place. Rules in `src/world/Clues.h/.cpp`; content in
`assets/data/clues.json`. A lead is `known` to a character or not; the
world holds where each lead can be learned.

| # | Lead | Learned from | Text in the notebook |
|---|---|---|---|
| 1 | There is a way out | The **broadcast** (a Radio with a Battery, any time: a looped voice, 3 lines over a minute); or a note on the fridge of 1 house in 6; or any survivor's "hello" after trust 10 | "They sealed the town. One gate still opens: the service gate, east side." (direction from the layout) |
| 2 | Where the gate is | Walking within 15 tiles of it; or the Police station's wall map (E on it); or lead 1 + a survivor with competence over 0.6 | Marks the gate on the town map |
| 3 | It needs power | E on the control box; or the guard hut's log book | "Gate motor: fuse blown, generator seized, tank dry." |
| 4 | The Sergeant has the key | The guard hut's log book ("Sgt. <Name> took the key when he went to find his kid. <Street>."); or the Police station duty board | Names the Sergeant and the district he walks |
| 5 | Where a fuse is | The Hardware store's order book (E on the checkout); or a note in the Garage | Marks the Hardware store |
| 6 | The generator can be fixed | A manual in the Garage (a readable item-less find on the workbench): grants the 3-session method, and Mechanics XP 100 | "It can be freed with tools and patience." |

- Names (the Sergeant, the street) are generated from the world seed, so each
  save has its own.
- Readable things are furniture or wall pieces with a "Read" prompt (new
  `InteractableType::Readable` with a `clueId`): fridge notes, the wall map,
  the duty board, log book, order book, manual. They stay in the world for
  ever; reading is free and quiet (a held 2 s).
- **Knowledge and death.** A character's known leads are written into their
  **Notebook** if they carry one (automatically, as recipes are today). The
  notebook goes with their pack. A new character knows nothing but can:
  read the original sources again; fetch an earlier character's notebook
  from their pack (reading it learns everything in it at once); or be told
  by a survivor or a retired character. The run-start card adds, when true,
  "Someone before you was writing things down."
  Physical progress (fuse fitted, fuel, repairs, the key's whereabouts) is
  never lost.
- The notebook (J) gains a page **The way out** once lead 1 is known: the
  known leads as plain sentences, then the four needs with their state as
  far as this character knows it ("Fuel: 2 of 4, as of day 6 of an earlier
  life" when read from a notebook; live when seen at the box).
- The objective line (top right) shows the next unmet step this character
  knows about, only when no urgent need or day-plan step outranks it:
  "Find the Sergeant, Mill Street."

### 6. Pacing it across lives

- Distances: the gate is far (existing placement). Fuel cans weigh enough
  that 4 means at least two trips or a vehicle (P11) or a companion (P08).
- The Sergeant: a Brute with 260 health, walks with 4 to 6 Shamblers that
  clump on him. Day and night he follows a loop of 5 road points in his
  district (so he can be found and watched). Killing him is a planned fight:
  fire, a spike-board doorway, a gun and its consequences.
- Infection: the gate district's infection rises as today; when overwhelmed
  (75) the last stand's base crowd is doubled. No deadline kills. The world
  getting worse is the clock.
- Bot and tests can set gate state directly (harness below).

### 7. Campaign and Endless on the title screen

- "Campaign" becomes "The way out": new characters start with the objective
  line "Find out why nobody has left." until lead 1, and one fridge note is
  guaranteed in the start home in the first life of a world.
- "Endless" is unchanged, with the gate as a secret.
- Remove the old campaign code: the objective tile, the deadline, the
  retirement on touching it (`GameUpdate.cpp` campaign block,
  `CampaignObjectiveState` becomes `GateState`; migrate: an old save's
  objective tile becomes the gate's district).

## Later

- A second way out (the river) for worlds whose gate district burned.
- What is at the end of the road. Leave it unsaid.

## Files

New: `src/world/Clues.*`, `src/world/Gate.*` (rules: state, what is
missing, last-stand schedule), `src/core/GameGate.cpp`,
`src/core/GameClues.cpp`, `assets/data/clues.json`, `tests/test_clues.cpp`,
`tests/test_gate.cpp`, `tools/harness/clues.txt`, `gate.txt`,
`gate_across_lives.txt`.
Changed: `MapGenerator` (stamp the gate complex), `LayoutData.h`,
`StructureView` and `generate_sprites.py` (gate, generator, box, readable
props), `Interactable.h`, `InteractionSystem`, `ThreatDirectorSystem` (last
stand), `GameRunEnd.cpp` (the "Out" ending), `GameLegacy.cpp`, `LegacyRules`
(leads for notebooks and the key), `GameUi.cpp` (notebook page, objective,
run-end variant, title card), `SaveManager` (gate state file, clue
knowledge in checkpoints and notebooks; version 5), `Traits.h` (Wayfinder),
harness (`gate fuse|fuel N|repair N|key`, `goto gate|sergeant|clue N`,
`learn N`, `clues`, `start gate`).

## Acceptance

- `clues.txt`: a radio with a battery teaches lead 1 within 60 s; the fridge
  note teaches it; the notebook page shows it; `kill`, `newrun`: the new
  character does not know it; fetching the pack and its notebook teaches
  every lead the dead one had. Screenshots: `way_out_page`, `fridge_note`.
- `gate.txt`: with everything set by harness commands, "Start it" runs 180 s,
  the dead arrive in rising numbers (print counts each 20 s), the gate
  opens in thirds (three screenshots), walking through ends the run with the
  "Out" screen naming the contributors.
- `gate_across_lives.txt`: life 1 fits the fuse and dies; life 2 pours two
  cans and dies during the stand; life 3 finds the fuse fitted, the level as
  left, and the key by life 2's grave. The title card reads right after an
  escape; the next life can walk out on day 1.
- Seeds 1234, 42, 777: the gate complex is reachable by road, the Sergeant's
  loop is walkable, every readable exists and is reachable.
- An old save with a campaign objective loads with a gate in that district.
- Unit tests: what-is-missing text for every state; lead sources; notebook
  transfer; last-stand spawn schedule; contributor record.
