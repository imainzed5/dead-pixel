# Decisions log

Add an entry whenever you decide something the plan did not settle, move
something to "Later", or find the plan wrong. Newest at the bottom of each
section. One or two lines each.

Format: `- YYYY-MM-DD, P0N: what was open -> what was chosen. Why.`

## Made by the user

- 2026-11-13: plan size -> everything, in phases.
- 2026-11-13: campaign goal -> get out of town; progress carries across
  lives.
- 2026-11-13: commits -> one local commit per finished phase on a new
  branch, never pushed, no AI attribution.
- Earlier (see `docs/revival_plan.md` and project memory): ship-quality;
  the hook is the town that remembers; isometric stays; art stays
  code-drawn at today's pixel scale; "bare hands" interface; home base
  before fighting; no engine switch.

## Made in the plan (2026-11-13), with the reason

- Phase order puts foundations first, then home base, then fighting, as the
  user asked; the campaign comes after survivors because survivors hand out
  clues.
- Item ids stay numeric in saves; code and data use keys. Cheapest way to
  keep old saves and stop hard-coded ids.
- Clothing stays "the best carried item counts"; no worn slots. Keeps the
  pack simple; the design document's intent (clothing matters) still holds.
- No key items for houses or cars; the world remembers "unlocked". The
  inventory slot has no per-item data to bind a key to a door.
- The dead grab instead of rolling a 60 % hit. Being caught becomes
  something the player plays out of; the shove becomes the core verb.
- A bite does not doom the character except in the Hopeless tone. Keeps
  today's rule; long lives are what the legacy layer needs.
- Power and taps restart each life (per run), like the taps do today. The
  town's fiction resets its first week per life; everything the player
  changed does not.
- Survivors are a fixed 12 per world, never replaced. The design document's
  "finite population".
- The gate may be opened in Endless too. One game, two lenses.
- No siphoning, no fishing, no gamepad, no languages, no generators for
  houses: listed as "Later" in their phases.
- Music is generated and almost absent, by tone preset, as the design
  document says.

## Made during implementation

- 2026-11-13, P01: loot counts when porting the nine tables -> every entry
  weight 1, min 1, max 1; the second stack comes with `extraRollChance` 0.25.
  Today's 1-in-4 doubling of a stack is gone, so a stocked cupboard holds
  slightly less. Section 5's rule, and phase 14 tunes it.
- 2026-11-13, P01: a container record carries the run that last changed it.
  The home kit skips containers already remembered this run, so a resume
  does not put the kit in twice; earlier lives' records still get the new
  kit. Simplest rule that keeps both.
- 2026-11-13, P01: when a container record is written -> on every move in or
  out (saved at once), and records that exist are refreshed before each
  checkpoint and at run end. Containers never touched are not written and
  roll fresh next life.
- 2026-11-13, P01: container spoilage -> every container's food spoils once
  per game hour (fridges at half speed, power always on until phase 6).
  Cheaper than every tick; same result.
- 2026-11-13, P01: crates hold 16 slots now (the plan said "phase 2"); the
  kind already exists as furniture.
- 2026-11-13, P01: removing `homeFurnitureSpawns` and `containerSpawns`
  shifts later per-run rolls (indoor decor and so on). The layout
  fingerprint test still passes, so the town itself is unchanged. Accepted.
- 2026-11-13, P01: systems without `ItemIds` (`BuildSystem`,
  `InteractionSystem`) look items up with `db.idOf("key")`; build recipes
  name their material by key.
- 2026-11-13, P01: the workbench check is "within 3 tiles, centre to centre,
  of a Workbench piece" (`furniture::kindWithin`); recipes say
  `station: "workbench"` or `"fire"` (a fire entity within 3 tiles).
- 2026-11-13, P01: with a container open the player stands still and does
  not swing, as with the crafting panel: A/D, W/S, E and the mouse belong to
  the panel.
- 2026-11-13, P01: hit-stop -> input, combat, zombie AI, movement,
  collision, projectiles, bodies sliding, animation and particles run on
  `feelDt`; everything else on `dt`.
- 2026-11-13, P01: when the campaign deadline passes the deadline is cleared
  (set to 0) after the line is shown, so it shows once across saves; the
  objective stays active.
- 2026-11-13, P01: harness `open` opens the nearest container directly. In
  `storage.txt` a bottle lying on the floor took the E press in the second
  life; the check is about storage, not about E.
- 2026-11-13, P01: the bot's behaviour beyond the plan's list: it engages
  only the dead that are chasing it and that it can see; leaves a fight that
  goes nowhere for a minute; flees when out of breath against two; treats
  wounds with bandages; keeps indoors at home when hurt or after dark;
  skips containers with two or more of the dead within 7 tiles. Each was
  needed to stop it standing still or walking into a crowd.

- 2026-11-13, P01: panic drift -> an offset of up to 80 px/s, taken off
  before the next tick's steering, not a push added every tick. The old rule
  built up to 750-1600 px/s at high panic (a defect, older than P01).
- 2026-11-13, P01: the bot, second pass: best weapon by damage and reach
  after wear (a broken knife does not count); walk unless one is within 3
  tiles; rest when fatigue passes 60; lean in to reach furniture; fight when
  pinned; eat and drink at 60; home below 70 % health and by 16:30 (careful
  16:00, bold 17:00); search the home's cupboards first and stop going out
  once carrying 6 food, 4 drinks and 2 weapons; keep still at home after
  dark. Tried and dropped: holding the house at night and running for home
  at dark (both worse; the open is safer than a house the dead get into).
- 2026-11-13, P01: harness runs make no sound (audio is not opened with
  `--script`).

## Moved to "Later"

- 2026-11-13, P01: medical and supply caches stay in code (with ids from
  `ItemIds`), not loot classes. The phase lists it as "Later".
- 2026-11-13, P01: a bot that lives three days. After the second pass the
  normal bot's median is 1.2 days (careful 0.9, so careful is still worse
  than normal); the bar is not met. A better fighter (shove, timing, doors
  closed behind it) fits phase 3, and the pressure itself is phase 14's.

## Balance: needs a design change

- 2026-11-13, P01: the soak baseline. After the bot's second pass it lives
  a median of 1.2 days and none of ten lives the week. A player standing in
  the home all day is dead by about 22:00 on day 1 (also before P01); one
  started at home at 15:00 with a bat, hammer, planks, food and water dies
  within 1.4 days on 6 of 10 seeds after killing 19-50; hordes of 15-45 form
  on most seeds; health returns about 1 an hour against 7 a bite; eight of the dead
  outside at dusk took three of six boards within the hour. Day one is more lethal than "a sensible player
  gets through the first days"; phases 3, 4 and 14 should read
  `docs/revival_plan.md` ("Overhaul phase 1").

## Ideas, not built

(none yet)
