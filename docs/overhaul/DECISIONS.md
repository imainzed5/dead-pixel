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

## Moved to "Later"

- 2026-11-13, P01: medical and supply caches stay in code (with ids from
  `ItemIds`), not loot classes. The phase lists it as "Later".
- 2026-11-13, P01: a bot that lives past day 1. The baseline shows the
  normal bot dies on day 1 on every seed; a better fighter (shove, timing,
  doors closed behind it) fits phase 3, when the shove exists.

## Balance: needs a design change

- 2026-11-13, P01: the soak baseline. A bot that searches in daylight and
  fights what chases it dies on day 1 on all ten seeds, mostly while
  searching away from home with several of the dead converging. Either day 1
  is too crowded near home or melee is too weak against two or three at
  once; phases 3, 4 and 14 should read this table.

## Ideas, not built

(none yet)
