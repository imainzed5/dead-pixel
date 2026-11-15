# Phase 10: legacy and the aging world

**Goal.** Finish the hook. Every life should leave words, knowledge and
changes, and the town should be older each time you wake up in it.

**The player notices:** a journal page by a grave that reads like the life
that wrote it; a new character who is a little handy because three before
them were; a cemetery behind the church-sized building near the centre that
they made themselves; a town that is quieter and emptier each life.

**Feel target.** Reading a dead character's last entry and recognising the
night it describes.

Already built in earlier phases and not repeated here: bases that carry
over (P02), container contents and seen buildings as world memory (P01,
P06), survivors who remember (P08), campaign progress and notebooks (P09),
thinner districts where you killed (P04).

## Core

### 1. The journal

- Every character keeps a **journal** automatically if they carry a Notebook
  (a retiring character always does, per the design document). It is a list
  of entries, each made by an event, each one or two short sentences in the
  past tense, in the game's voice.
- `src/legacy/Journal.h/.cpp`: `struct JournalEvent {int day; JournalKind
  kind; std::string a, b; int n;}`; `std::string write(const JournalEvent&,
  seed)` picks one of 3 or 4 phrasings per kind from
  `assets/data/journal_lines.json`. At most 3 entries a day; the most
  important kept (rank in the data).

| Event | Example |
|---|---|
| First night survived | "Day 1. Two of them at the door. The boards held." |
| Taps died / power failed | "Day 5. The lights went. It is very dark." |
| First kill | "I killed one today. It was wearing a wedding ring." |
| A wound | "Bitten on the arm. Cleaned it. We will see." |
| A base built up | "The crate is finished. It feels like a pantry." |
| A survivor met / traded / lost | "Met Ana. She would not come outside. We traded through the window." |
| A horde passed | "They went through in the night. I counted until I could not." |
| A clue learned | "There is a gate. Of course there is a gate." |
| A grave found / a risen laid to rest | "Found Tom's grave by the diner. I did what he would have wanted." |
| Ten days / a season turned | "Ten days. I have stopped counting cans and started counting days." |
| Low points (crisis began) | "I sat on the stairs all afternoon." |

- On death or retirement a last entry is added from the cause ("The writing
  stops here." for a sudden death; a full sentence for retirement, built
  from run stats as the design document describes).
- The journal is stored in the grave record (`journal: [entries]`) and
  shown: (a) at the grave or pack, E "Read" opens a parchment-free panel in
  the bare-hands style: name, days, then the entries with their day numbers,
  scrolling; (b) in the notebook's "Who came before" page, Enter on a name.
- Reading a journal for the first time: depression +3, and the reader
  learns any recipes and clues the notebook held (P09). Reading the journal
  of a character who got out: depression -10.
- The current character's own journal is the last page of the notebook (J).

### 2. What a bloodline learns

Replaces "one of four fixed traits" with echoes that stack slowly.

- **Skill echoes.** Each grave stores the dead character's top two skills
  (P05). A new character starts each skill at `floor(sum of echoes for that
  skill / 3)` levels, capped at 2, where each grave contributes its level in
  that skill x (1.0 if its journal has been read by any later life, else
  0.5). Shown on the character screen: "Carpentry runs in the family (1)."
- **Bloodline traits** (keep the four, add two, fix their effects), each
  earned by what happened, at most 2 active, most recently earned first:

| Trait | Earned when | Effect |
|---|---|---|
| Hardened | a retirement | Depression gain x0.7 below 60 (exists) |
| Scavenger | 3 lives ended by hunger or thirst | Starts with 1 Canned Food and 1 Water Bottle more; search time -10 % |
| Firewise | a death by fire, or 30 tiles burned in one life | Fire damage -40 %; sees fuel (P07) |
| Vigilant | 4 graves | Panic falls 15 % faster; wakes 1 s earlier |
| Gravedigger | 3 risen laid to rest | Laying to rest gives -10 depression (double); the risen are marked on the map from 20 tiles |
| Wayfinder | someone got out (P09) | Gate and seen buildings on the map |

- `selectBloodlineForNewRun` (`SaveManager.cpp`) becomes
  `legacy::bloodlineFor(const LegacyState&)` in `LegacyRules`, tested.

### 3. The town ages between lives

One function, `legacy::ageWorld(WorldState&, int daysLived, seed)`, run in
`persistWorldForNextRun`, tested as a pure rule. For each life that ends:

| What | Rule |
|---|---|
| Built things | One stage worse (P02); boards on doors and windows lose 25 % health; a board at 0 is gone |
| Unkept bases | A base with no survivor living in it: each crate loses 20 % of its food stacks (spoiled or taken); 10 % chance per life it is found by a survivor and stripped (the survivor's stock gains it: it can be traded back) |
| Gardens | Seedlings if watered in the last 3 days of the life, else gone |
| Bodies | The life's corpses become bones decals (max 40 kept, oldest dropped); world memory |
| Infection | Each district: +0.5 per day lived, -10 flat, floor at its starting value (it creeps up across lives) |
| The dead | Kills thin districts (P04). The starting population also grows 4 % per life (cap +60 %) |
| Survivors | Advanced by `daysLived` (P08) |
| Graves | A grave older than 5 lives becomes "weathered": the name still reads, the epitaph fades to "..." unless a later life tends it (E "Tend": 5 s, -4 depression, resets the age) |
| Burns | Grass regrows after 2 lives; gutted buildings stay |
| Windows and doors | Broken stays broken |
| Litter | Ruin level by infection (P04) |

- The run-start card gains one line of time passing when the last life was
  long: "Eleven days since anyone lived here."
- The title card adds "Days lived in this town: 143".

### 4. Burial

- A character's own grave appears where they fell (exists). New: **carry a
  pack to the cemetery.** Each world has one cemetery plot: a fenced grass
  patch placed at world creation near the town centre (stamped like the gate
  in P09, with a gate-less fence and a few old stones).
- At a grave whose pack you have taken, or at a risen you laid to rest, E
  "Take the marker" puts a weightless "Marker: <Name>" in the pack; at the
  cemetery, F places it as a headstone. Effects: depression -12 once per
  marker; the grave lead moves to the cemetery; that character's skill echo
  counts as read.
- Survivors who die near you can be buried the same way (E "Bury" at
  their body: 30 s by hand, no tool, Soft noise; it makes a grave where
  they lie).
- The cemetery is the save's record: rows of stones with names and days,
  readable with E, drawn in order of burial. It is on the town map from the
  first burial.

### 5. Retiring by choice

- From day 10, at your base (a house with 3+ closures and a bed), with
  hunger, thirst and health above 50 and no infected wound, the pause menu
  offers "Stay here for good". Confirm twice. The character retires: they
  become a survivor at that base (P08), the base is kept, and the bloodline
  earns Hardened only if depression was under 50 (a peaceful retirement) or
  as today if in crisis.
- The run-end screen for a chosen retirement reads "<Name> stayed."

### 6. Fresh Start and Living World

- The title screen's world options get one toggle per save, set at world
  creation: **Living World** (everything above; default) or **Fresh Start**
  (no legacy at all: graves, bases, memory and aging off; for players who
  want a plain survival run). Stored in the slot meta. The per-feature
  toggles of the design document are not built.

### 7. The notebook, reorganised

Pages, in order, each a tab across the top in the docked style (Left/Right):
**Leads** (what is out there: graves, packs, risen, bases, survivors' homes,
with distance and direction), **The way out** (P09), **People** (survivors
met), **Before you** (earlier lives; Enter reads a journal), **Know-how**
(recipes, threat-mode rules learned, building kinds seen), **Journal**
(this life). Totals at the foot of "Before you".

## Later

- Notes left on purpose: write a line and pin it to a door for the next
  life to find.
- A family tree view.

## Files

New: `src/legacy/Journal.*`, `src/legacy/WorldAging.*`,
`assets/data/journal_lines.json`, `src/core/GameJournal.cpp`,
`src/core/GameCemetery.cpp`, `tests/test_journal.cpp`,
`tests/test_aging.cpp`, `tests/test_bloodline.cpp`,
`tools/harness/journal.txt`, `aging.txt`, `cemetery.txt`.
Changed: `LegacyRules.*`, `GameLegacy.cpp`, `GameRunEnd.cpp`, `SaveManager`
(grave records: journal, skills, age, buried; cemetery tile; slot option),
`MapGenerator` (stamp the cemetery), `GameUi.cpp` (notebook tabs, journal
reader, character screen echoes), `GameMenus.cpp` (stay for good; world
option), harness (`journal`, `age LIVES`, `goto cemetery`, `bury`).

## Acceptance

- `journal.txt`: a scripted life (survive night 1, kill one, board up, get
  bitten, die on day 3) produces 6 to 9 entries that read correctly
  (print them; screenshot the reader panel at a grave in the next life).
- `aging.txt`: build a crate and boards, `kill`, `newrun` three times: the
  crate goes Intact, Damaged, Breaking, gone; boards lose a quarter each
  life; district infection is higher than at world creation; the same seed
  gives the same result twice.
- `cemetery.txt`: take a marker, place it, the lead moves, depression falls
  once, the stone reads the name after a reload.
- Four lives with Carpentry 3 deaths, journals read: the fifth starts with
  Carpentry 2 (unit test on the echo rule and a harness check).
- Fresh Start world: no graves spawn after a death, no memory is written.
- Old saves: graves without journals show "Nothing was written down."
