# Phase 3: fighting

**Goal.** Fighting stays a tax, but becomes a craft: spacing, timing, the
right tool, and a way out. Weapons differ in what they let you do, not only
in numbers.

**The player notices:** a shove, the dead grabbing hold, blades that stick,
heavy weapons that put them on the floor, rocks and clocks that pull them
away, guns that solve one problem and cause three.

**Feel target.** Two in a hallway: shove the first into the second, split the
first one's head while it reels, back through the door, shut it. Twelve
seconds, half your stamina, heart going.

Weapon numbers are in `03_content_tables.md` section 2; noise in section 10.

## Core

### 1. Per-item weapon stats

- `ItemDatabase` reads the `weapon` object; `weaponStatsForItem` builds
  `WeaponStats` from class defaults, then item overrides. `WeaponStats` gains
  `windupSeconds`, `recoverySeconds`, `canStick`, `knocksDown`, `reach`
  (bool), `noiseTier`, `magazine`, `ammoItemId`, `pellets`.
- `CombatSystem` uses the weapon's wind-up and recovery (defaults 0.20 and
  0.30; active stays 0.10).
- The pack panel's weapon detail adds a "Speed" bar (from wind-up +
  recovery) next to Damage, Reach and Condition, and one line of what is
  special ("Bleeds. Can stick." / "Knocks down." / "Loud.").

### 2. Shove (Space)

- A push with open hands or the weapon's haft: 0.10 s wind-up, 0.25 s
  recovery, 10 stamina (6 when unarmed), arc 100 degrees, range 36 px, no
  damage, no wear, Soft noise.
- Every zombie in the arc is thrown back 56 px and staggered 0.6 s; one
  thrown into another staggers that one 0.3 s; one thrown into a wall takes
  5 damage and staggers 0.9 s. It breaks a lunge like a blow does.
- A zombie under 35 % health, or hit in the back, **falls** (section 4).
- Brutes move 16 px and do not stagger; Crawlers are not moved (P04).
- Below 20 % stamina a shove moves them half as far.
- Space is free during play today (it is used only in menus, the crafting
  panel and to restart after a run ends).

### 3. The dead grab

Replaces "60 % hit chance" as the way damage happens up close, so that being
caught is a state the player plays out of.

- A lunge that reaches you (same reach test as today) is now one of:
  - **a claw** (55 %): today's hit: 7 damage, wound chance as today;
  - **a grab** (45 %; always, if you were hit from outside your front 180
    degrees or are below 20 % stamina): the zombie holds you.
- While held: you cannot walk, swing or use items. Each holder bites every
  1.4 s: 9 damage, a Bite wound 60 % of the time. Other zombies arriving
  grab too.
- Breaking free: tap Space. Each tap costs 7 stamina and adds 1 to a
  struggle meter that needs 4 per holder; the meter drains 1.5 per second.
  With no stamina, taps count half. On breaking free, every holder is shoved
  (as section 2) and you are free and unable to be grabbed for 1.0 s.
- A weapon in hand helps: Knife or Glass Shiv: holding left mouse while held
  stabs the holder for 60 % weapon damage every 0.7 s (the only attack
  allowed). Killing the holder frees you.
- HUD: while held, a short line under the health lines, "HELD - SPACE", with
  a row of small ticks filling. The view pulls in 6 % and shakes slightly.
- Player hit chance constant (`0.6`) is removed; a lunge that reaches,
  lands. Dodging is done with feet, a shove or a blow that breaks the lunge.
- Tuning targets: one Shambler against a player who knows the shove deals no
  damage; against a player who does nothing, death in about 60 s (as
  before).
- The player's right-click grab-and-execute stays, renamed in the Controls
  panel to "Take down (from behind)": it now needs the zombie's back (more
  than 90 degrees from its facing; use the facing the animation uses, not
  velocity, so it works on a standing zombie) or a zombie on the floor. With
  a blade it is quiet (Soft, not Loud).

### 4. Knockdown and the floor

- New zombie state `Down`: lies for 2.5 s (Brute 1.5 s), then takes 1.2 s to
  get up, during which it cannot attack. A downed zombie cannot grab.
- Causes: a shove on a weakened or turned zombie; any Sledgehammer hit; a
  blunt hit that deals 45 or more in one blow; being thrown down by a car.
- **Stomp:** a swing aimed at a downed zombie within 30 px with any weapon is
  a downward strike: damage x2.0 (unarmed: a stomp, 30 damage, 14 stamina).
- Animation: the rig has a fall and a lying pose; add "down" (the last fall
  frame, held) and "rise" (the fall reversed at 0.8x) in `tools/charrig.py`.
- Crawlers (P04) are always "low" but not `Down`: they can be stomped.

### 5. Blades stick, blunt things tire

- **Stick:** a killing or 40+ damage hit with a weapon that `canStick`
  (Axe, Machete, Spear) has a chance to lodge: 12 % in good condition, 20 %
  worn, 30 % damaged. Pulling it free adds 0.5 s to recovery and costs 8
  stamina ("It sticks."). Fighting level 3 halves the chance (P05).
- **Reach:** the Spear's swing resolves 0.06 s into the active window before
  others, and a zombie hit by a reach weapon while winding up is pushed to
  the weapon's range. The point: a spear keeps one zombie off you for ever
  and is useless against three.
- **Back attacks on the player:** a claw from outside your front 180 degrees
  does +50 % damage. Standing with your back within 1 tile of a wall counts
  as covered.
- **Exhausted** (stamina under 20 %): swings 35 % likely to miss (as today),
  wind-up +30 %, move speed -15 %.
- Wear: today every swing costs 1 durability. Change to: a hit costs 1, a
  miss costs 0, a hit on a wall or furniture (new: swings stop at walls)
  costs 2. Broken weapons become "Broken X": half damage as today, and they
  can be repaired at a workbench (P02).

### 6. Throwing (G)

One verb for everything thrown, replacing the L-key molotov.

- With a `throwable` item selected on the belt, hold G: an arc line is drawn
  from the player to the cursor, clamped to the item's range (Rock 9 tiles,
  Alarm Clock 6, Flare 8, Molotov 7, Empty Bottle 8, Tin Can 8); release to
  throw. Walls stop it; it flies through broken or open windows and open
  doors. Flight speed 420 px/s with a small arc drawn by scaling the sprite.
- **Rock, Tin Can, Empty Bottle:** on landing, Medium noise (bottle: Loud,
  breaks, leaves Glass Shards); a zombie hit takes 6 damage and turns to
  the thrower. The dead that hear it investigate the landing spot for up to
  20 s, then wander. Rocks can be picked up again.
- **Alarm Clock:** wind it (F: "Set for 8 seconds") then throw or drop; rings
  Loud for 20 s. One use in three it survives and can be picked up.
- **Flare:** burns 90 s where it lands: light radius 6, Medium noise every
  2 s, the dead within 15 tiles that see the light walk to it and stand.
  Rain halves its time.
- **Molotov:** as today's effect, thrown this way. Remove the L key from
  input, the Controls panel and the README. Dousing a fire with water moves
  to F with a Water Bottle selected while facing a fire within 3 tiles.
- New key G conflicts with "approach survivor" (G today): move survivor
  approach to E on a survivor (P08 rebuilds that anyway).
- Thrown objects are entities with `Projectile` extended by `kind` and
  `arcHeight`; add `ThrownSystem` or extend `ProjectileSystem`.

### 7. Guns

- Items `pistol`, `shotgun`; ammo `pistol_ammo`, `shotgun_shells`. The rounds
  loaded in a gun are a new field, `InventorySlot::loaded` (int, saved in
  checkpoints, packs and containers). Wear stays in `condition`.
- Fire with left mouse (wind-up is the aim settle). Reload with R when no
  door, window or tap is in reach and a gun is in hand: Pistol 1.6 s for the
  magazine, Shotgun 0.9 s per shell; moving at a walk is allowed, running
  cancels.
- Accuracy: the shot leaves at the aim angle plus a spread: base 1.5 degrees
  (pistol), +4 while walking, +10 while running, + panic x 0.12 degrees per
  point above 25, +6 when exhausted. Crouched and still: x0.6. A thin
  aim line is drawn from the player while a gun is in hand, its length the
  gun's range, widening with the spread.
- Pistol: one bullet, 55 damage, x2.5 within 1 tile ("point blank"), stops
  at the first body. Shotgun: 5 pellets of 30 across 24 degrees, range 7
  tiles, knockback 40 each, anything within 2 tiles hit by 3+ pellets falls.
- Noise: Pistol Loud (30 tiles), Shotgun Explosive (60 tiles). A shot
  indoors with every door and window shut is one tier quieter outside the
  building (after P04's walls rule; until then, always full).
- Every shot adds 4 panic to the shooter below desensitisation 30.
- Jams: a gun below 33 % condition jams 10 % of shots ("It jams.": R clears,
  1.0 s).
- Sources: loot classes `police`, `armoury` arrive in P06. Until then, add
  Pistol Ammo to `bedroom` at weight 1, and the fixed rare finds of
  `03_content_tables.md` section 5 (2 Pistols, 1 Shotgun) placed at world
  creation in far houses; remembered when taken.
- Sprites: the rig has a "gun" loadout; add a long-gun variant for the
  shotgun (two-handed pose) and item icons for both guns and shells.

### 8. New weapons and the improvised class

- Add Crowbar, Machete, Sledgehammer, Chair Leg, Frying Pan, Glass Shiv with
  icons. Breaking a chair or table (P02) gives one Chair Leg in place of one
  plank, once per piece, if you carry no weapon.
- Starting weapon stays the Knife.

### 9. Feedback

- Each class has its own hit effect: blunt = short hit-stop (as today) +
  dull flash; bladed = no hit-stop on a kill, a longer blood arc; heavy
  (Sledgehammer) = 2x shake and a dust ring; gunshot = muzzle flash (exists),
  a 1-frame white line along the shot, casing (exists).
- A shove has a short white arc and a scuff of dust at the target's feet.
- Low stamina: the stamina line pulses amber under 20 %; breath sound is
  P12's job.

## Later

- Suppressed pistol (found item) at Medium noise.
- Throwing knives.

## Files

New: `src/world/CombatRules.h/.cpp` (pure rules: stick chance, grab odds,
struggle meter, spread, stomp, shove outcomes), `src/systems/ThrownSystem.*`,
`tests/test_combat_rules.cpp`, `tools/harness/fighting.txt`,
`tools/harness/throwing.txt`, `tools/harness/guns.txt`.
Changed: `Combat.h`, `CombatSystem`, `ZombieAI.h`, `ZombieAISystem`
(grab, `Down`), `ProjectileSystem`, `InputSystem`, `Input`, `ItemDatabase`,
`Inventory.h`, `SaveManager` (slot `loaded`), `Hud`, `GameUi.cpp`,
`GameUpdate.cpp`, `GameFire.cpp`, `charrig.py`, `generate_sprites.py`,
`DevHarness` (`shove`, `throw X Y`, `reload`, `zombie DX DY [facing away]`,
`expect held|free|down N`), Controls panel, README, bot (uses shove when
2 are within 1.5 tiles; never uses guns).

## Acceptance

- `fighting.txt`: shove throws a zombie back and breaks its lunge; a zombie
  shoved from behind falls; a stomp kills a downed 50-health zombie in one
  blow with a bat; a spawned zombie that reaches an idle player grabs within
  three lunges; four Space taps free the player; a Sledgehammer hit puts a
  full-health Shambler down. Screenshots: `shove`, `held`, `down`, `stomp`.
- `throwing.txt`: a rock thrown 8 tiles away turns two wanderers toward the
  landing spot and not toward the player; an alarm clock holds three of the
  dead at it for 20 s; a molotov thrown with G lights its tile.
- `guns.txt`: a pistol shot kills a 50-health zombie at 6 tiles, brings
  every wanderer within 30 tiles to investigate, uses one round; reload
  works; a shotgun at 1.5 tiles drops two.
- Unit tests for every rule in `CombatRules`.
- `combat_feel.txt`, `weapon_wear.txt`, `corpses.txt` still pass (update
  wear expectations: hits only).
- `bot_week` x10: median day of death does not fall by more than one day
  against the phase 2 baseline (grabs must not make the game harder for a
  player who shoves; the bot shoves).
