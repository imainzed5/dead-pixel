# Phase 5: body and mind

**Goal.** The character becomes someone: chosen at the start, changed by
what they do, worn down in ways the player can see and treat.

**The player notices:** a character screen with traits to pick, skills that
tick up from doing things, a broken leg that needs a splint, hands that
shake, a night without sleep that plays tricks, a book that helps.

**Feel target.** Day 9: limping on a splint, three days without proper
sleep, something moving at the edge of the screen that is not there.

Tables: skills in `03_content_tables.md` section 7, traits in section 8.

## Core

### 1. Character creation

A screen between "New Run" and the run, in the bare-hands style, over the
dimmed town.

- Left: the name (seeded as today; Enter rerolls; typing replaces it, 16
  characters) and the bloodline line if any ("Hardened. Carpentry runs in
  the family.").
- Middle: two columns, "Good at" and "Bad at", the traits of section 8, each
  with cost or gain and one line of effect. Up/Down moves, Enter toggles,
  Left/Right switches column. A chosen row is inverted.
- Right: "Points: 2" (amber when below 0), the resulting starting numbers
  (health, carry, skills above 0), and "Begin" (disabled below 0 points).
- "Quick start" (Space): the seed's pick, as today (one positive, one
  negative, costs balanced).
- `Traits` becomes lists: `std::vector<PositiveTrait>`,
  `std::vector<NegativeTrait>` (max 3 each), saved in the checkpoint and in
  grave and history records (old saves: the single values become one-item
  lists).
- New trait effects wired where the old ones are (`GameScene.cpp` at spawn,
  `NeedsSystem`, `MentalStateSystem`, `InputSystem`, wake range in
  `updateSleep`).
- Harness: `newrun [trait,trait,...]`; script `character.txt` screenshots
  the screen with nothing chosen, a valid pick and an overspent pick.

### 2. Skills

- New component `Skills { std::array<float, 7> xp; std::array<int, 7>
  level; std::array<double, 7> lastUsedHour; }` on the player; rules in
  `src/world/SkillRules.h/.cpp` (thresholds, decay, every per-level effect as
  a function: `boardSeconds(level)`, `swingStaminaScale(level)`, ...).
- Award XP at the action sites with one call, `awardXp(Skill, amount)`,
  which also raises the level and posts "Carpentry 2" in the message line
  (one quiet line, ink, 3 s).
- Apply effects by calling the rule functions where the base numbers are
  used. Do not scatter `if (level >= 3)` in systems: put those checks in
  `SkillRules` (`canReinforceBoards(level)`).
- Recipes may name `skill` and `skillLevel`: below it, the craft takes twice
  as long and fails 40 % of the time ("It comes apart in your hands."; the
  design document's soft gate). Inputs are kept on failure, except food.
  Set: Spike Board Carpentry 1; Suture Kit Medicine 1; Snare Living off the
  land 1; Stew Living off the land 2.
- Pack panel gains a second tab, **BODY** (the tab row is PACK / BODY /
  MAKE): left column wounds and conditions with what each needs ("Deep cut,
  left arm: needs stitches"), right column the seven skills as a name, level
  pips (5 small squares) and a thin progress line. Traits listed under.
- Saved in the checkpoint. On death, the top two skills by level go into the
  grave record (`skills: [(id, level)]`) for phase 10.

### 3. Wounds worth treating

Extend `Wound` and `WoundSystem`; rules in `src/world/WoundRules.h/.cpp`.

- `Wound` gains `part` (Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg),
  `depth` (Shallow, Deep), `dressed` (bool), `stitched`, `splinted`.
  `severity` is dropped (map old saves: severity above 0.75 becomes Deep).
- New type **Fracture** (arms and legs only). Sources: a Brute's blow (25 %),
  being hit by a car, a failed window climb (5 %), a fall when knocked down
  while over the carry limit (10 %).
- Where a wound lands: claws: arms 50 %, torso 30 %, head 10 %, legs 10 %;
  bites while held: arms 40 %, torso 30 %, neck/head 30 %; Crawlers: legs.
- Effects by part (stack, worst of each kind counts):

| Wound | Arm | Leg | Torso | Head |
|---|---|---|---|---|
| Cut or bite, undressed | Swing stamina +15 % | Move -8 % | Max stamina -10 | Sight range -1 tile (view vignette) |
| Deep, not stitched | Bleeds again after any swing (30 %) | Bleeds again after running | Bleeds slowly until stitched | as torso |
| Fracture, no splint | Cannot use two-handed weapons, wear x2 on swings missed | Move -45 %, cannot run | - | - |
| Fracture, splinted | Swing wind-up +20 % | Move -20 %, run at walk speed | - | - |

- Healing times (game hours): bruise 2, shallow cut 6, deep cut 30 (12 once
  stitched), bite 18, fracture 96 splinted (never, unsplinted). Rest in bed
  heals x1.5. Medicine skill and `Slow Healer` scale these.
- Treatment is one verb: F with a medical item uses it on the worst wound it
  can treat (the BODY tab lets you pick: cursor on a wound, Enter lists what
  you carry that helps). Bandage: dresses, stops bleeding. Disinfected
  Bandage: dresses and removes contamination. Suture Kit: stitches a deep
  wound (8 s, +6 panic without Painkillers taken in the last 2 h). Splint:
  sets a fracture (10 s). Antibiotics: cure infection (as today). Bottle of
  Spirits: F on a wound disinfects it (hurts: +8 panic), uses a quarter
  bottle (4 uses).
- Dressings get dirty: a dressed wound becomes undressed-and-contaminated
  after 12 game hours unless redressed (Bandages are a running cost).
- Infection: as today (3 h for a dirty bite), plus any undressed deep cut
  has 4 % per game hour. Infected wounds raise body temperature (fever:
  +0.3 C per 20 illness) and are the only thing that makes illness climb
  past 60 on its own.
- **The bite question.** A zombie bite does not doom the character (keep
  today's rule: dirty bites infect; antibiotics cure). Tone preset Hopeless
  only: an infected bite untreated for 24 game hours becomes "Turning":
  nothing cures it, death in 12 to 24 hours, and the character always rises.
  Say so in the tone's description.
- Max health penalty stays 5 per wound (10 per fracture).
- HUD conditions list names wounds briefly: "Bleeding", "Deep cut",
  "Broken leg", "Infected bite", "Fever", worst first.
- Character sprite: a limp (uneven walk-cycle timing) when a leg is hurt;
  the hurt arm held in (the rig's "hurt" pose arm) when an arm is fractured.

### 4. Sleep debt with teeth

`NeedsSystem` and a new `src/world/SleepRules.h/.cpp`.

| Sleep debt | Name (HUD) | Effects |
|---|---|---|
| 50-70 | Drowsy | Stamina regen -15 % |
| 70-85 | Tired | Aim spread +3 degrees, swing wind-up +10 %, view edges soften |
| 85-95 | Exhausted | **Things that are not there**: section 5. Stamina cap -25 % |
| 95-100 | Dead on your feet | Microsleeps: every 40 to 90 s the screen closes for 1.2 s and input is lost; collapse at 100 (exists: 3 h where you stand) |

- Coffee: -20 shown debt for 4 game hours (the real debt keeps rising); when
  it wears off, +5 debt. Two in 6 hours: +10 panic, jitter.
- Painkillers and Spirits before sleep: quality -0.15.
- Bed, bedroll, safe room: P02's table. Waking by noise: at the quality's
  wake rule. A full sleep (6+ h at quality 0.8+) gives "Rested" for 6 h:
  stamina regen +10 %, depression -8 (exists).

### 5. A mind that shows

Rework `MentalStateSystem`'s outputs; inputs stay, plus new triggers.

**Panic** (fast):
- New triggers: being grabbed +12; a Screamer's scream within 10 tiles +8; a
  gunshot of your own (P03); waking to a zombie within 5 tiles +30; a horde
  within 15 tiles +0.8/s; first kill of the run +10; darkness indoors with
  no light and a zombie heard within 10 tiles +1/s to a ceiling of 40.
- Effects:

| Panic | What happens |
|---|---|
| 25-50 "Uneasy" | Aim wobble (exists); heartbeat (P12); stamina costs +10 % |
| 50-75 "Shaken" | Hands shake: held actions (boarding, treating, reloading, lock work) take +30 %; footsteps one tier louder half the time; the view narrows (vignette 15 %) |
| 75-90 "Panicking" | Movement jitter (exists); 10 % chance to fumble a reload or a treatment (restart it); cannot sleep |
| 90+ "Breaking" | Screams (exists). If also grabbed or below 30 % health: 2 s of lost control (runs straight away from the nearest zombie), at most once a minute |

- Recovery: as today (3/s when safe) x2 while crouched still out of sight,
  x1.5 near a friendly survivor; Painkillers -15 (exists); Chocolate -6;
  Spirits -20; Herbal Tea -12; a deep breath: hold C still for 3 s out of
  sight: -10, once a minute (a small verb the player learns).

**Depression** (slow):
- New triggers: finding a grave of your own bloodline for the first time
  this run +4 (but laying a risen to rest -10); reading a dead character's
  journal +3; a travelling companion dying +20; your base broken into +6;
  each kill past 20 in a run +0.3 x (desensitisation / 100).
- New relief: reading a Book in shelter (F, 20 game minutes, -10, each book
  once per character; the pack shows it as read); a hot meal (anything from
  a `fire` station) -3; Stew -8; Keepsake -25 once; a full week alive -10
  with the line "A week."; building and closing up (P02); company (exists);
  music from a Radio with a Battery in shelter -1 per game hour (P06).
- Effects:

| Depression | What happens |
|---|---|
| 30-50 "Low" | Good sleep gives 0.9 of its rest |
| 50-70 "Worn down" | Stamina cap -10 %; comfort items give half |
| 70-90 "Numb" | Hesitation: 15 % of swings, searches and treatments start 0.6 s late; move -5 % (replaces today's sliding scale) |
| 90+ "Giving up" | The crisis (exists: 24 h to retirement). Cannot start building; cannot loot bodies |

**Desensitisation:** now also fades: -1 per game day without a kill. Above 60
the character no longer gets "first kill" or "Uneasy" from a single zombie.

**Seeing things** (sleep debt 85+ or panic 75+ for 60 s, or fever over 39):
- Every 30 to 80 s one false sign, drawn from: a zombie silhouette at the
  screen edge that fades when looked at for 0.5 s or approached within 4
  tiles (never inside the building you are in); a knock at a door that has
  nobody at it; a groan from an empty direction; the minimap showing a dot
  that vanishes. Never a false sign that deals damage or blocks movement.
- They do not make noise in the simulation and the dead do not react.
- Tests assert a hallucination never touches gameplay state.

### 6. Comfort items

Books, Keepsakes, Coffee, Spirits, Chocolate get icons, loot entries (P06
finishes the tables; until then add to `bedroom`, `living`, `kitchen`) and
their effects above.

## Later

- Dependency on Spirits and Painkillers.
- A "Smoker" trait with cigarettes.

## Files

New: `src/components/Skills.h`, `src/world/SkillRules.*`,
`src/world/WoundRules.*`, `src/world/SleepRules.*`,
`src/world/MindRules.*` (bands and effects as pure functions),
`src/systems/HallucinationSystem.*`, `src/core/GameCharacter.cpp` (creation
screen), `tests/test_skills.cpp`, `tests/test_wounds.cpp`,
`tests/test_mind.cpp`, `tools/harness/character.txt`, `body.txt`, `mind.txt`.
Changed: `Traits.h`, `Wounds.h`, `WoundSystem`, `NeedsSystem`,
`MentalStateSystem`, `CombatSystem`, `GameInventory.cpp` (treatment),
`GameUi.cpp` (BODY tab, conditions), `Hud`, `GameMenus.cpp`, `SaveManager`,
`charrig.py`, harness (`wound TYPE PART [deep]`, `set skill NAME LEVEL`,
`set debt`, `expect condition NAME`).

## Acceptance

- `character.txt`: three screenshots; an overspent build cannot begin; a
  built character starts with the stated health, carry limit and skills.
- `body.txt`: a leg fracture slows the player 45 % (measure tiles in 5 s),
  a splint brings it to 20 %; a deep cut bleeds again after a swing until
  stitched; an old dressing gets dirty at 12 h; BODY tab screenshot fits.
- `mind.txt`: at debt 90, at least one false silhouette appears within
  120 s and changes no simulation state (zombie count, noise events equal
  before and after); panic 60 makes boarding take 30 % longer; a book
  lowers depression by 10 once.
- Boarding 10 doors raises Carpentry to level 1 and boarding gets faster
  (unit test on `SkillRules`, harness on the timing).
- Old checkpoints load (single traits, wounds without parts).
- `bot_week` x10: the bot treats wounds with what it has; record deaths by
  cause; "infection" should appear but not lead.
