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

- 2026-11-14, P02: interior doorways are doors, so a piece could never leave
  its room. A push now carries a piece through an open (or broken) doorway to
  the floor beyond it in one step; a piece never comes to rest in a door
  frame (`furnitureuse::pushTarget`).
- 2026-11-14, P02: boards raise the path cost for the dead: 30 per board on a
  door or window (a closed door 25, a window 30, a blocked doorway 40, an
  empty frame 6). Without it a boarded window stayed the cheapest way in and
  the dead walked past a weaker door to it. Callers still pick by strength.
- 2026-11-14, P02: a door's "blocking furniture" is a piece on the floor tile
  the door opens onto inside the same building, interior doors included (the
  dead route through an interior doorway blocked this way at cost 40 too).
  Ways into a house (the day plan, sleep safety, the callers' choice) are
  only doors with ground outside them, and the house's windows.
- 2026-11-14, P02: the old `env_*` prop sprites stay in the atlas: the town's
  scattered scenery (graves, crates, campfires by the road) still uses them.
  Only build mode's use of them is gone.
- 2026-11-14, P02: a candle or lantern is set down unlit; Matches light it
  (E). The harness has `light` for scripts.
- 2026-11-14, P02: a zombie battering a barricade with nothing in sight gives
  up like any other (in `base.txt` it broke the door in about 25 s, then gave
  up on the bookshelf at 103 of 350 after about 35 s). Night callers keep at
  it until dawn as before.
- 2026-11-14, P02: the window's "lit" check runs once a game minute (1 real
  second) and sends wandering or investigating dead outside, within 10 tiles
  and in sight of the window, to the ground outside it. They then see the
  player through the glass (6 tiles) or not.
- 2026-11-14, P02: placing a build is by the wheel and a click on the ghost
  tile; the harness `build KIND` uses the same start (materials taken, a held
  build that cancels and refunds) on the nearest free tile in reach.
- 2026-11-14, P02: the wheel is an oval (190 by 110 px), not a circle: on a
  circle the two lower names overlapped.
- 2026-11-14, P02: `base.txt` on seed 1234: the home has no wardrobe, so the
  script hauls a bookshelf (as heavy, the same 350 health). `haul` takes
  kinds in order of preference.
- 2026-11-14, P02: the bot boards the home's ways in weakest first (by the
  same strength the callers use), windows too, with planks, 2 nails and a
  hammer; without planks it draws the curtains on whole windows at dusk. It
  now picks up Nails.
- 2026-11-14, P02: soak rows gain "Boards" (the most boards up at once,
  doors and windows) and "Night 1 at home" (died between 18:00 on day 1 and
  08:00 on day 2, standing in the home); metrics rows gain `at_home`.
- 2026-11-14, P03: gun shots -> hitscan, with the shot's line drawn for a
  frame. A bullet that flies would let one step through it at 60 Hz; the
  Projectile component stays for survivors' bullets and things thrown.
- 2026-11-14, P03: down and getting up -> the death fall held on its last
  frame, and played backwards (at 0.8 of its length) to rise. No new rig
  frames; the read is clear enough.
- 2026-11-14, P03: the spear's reach -> its wind-up is 0.06 s shorter, and a
  blow on one winding up a lunge pushes it out to the spear's range.
- 2026-11-14, P03: a wall at your back covers it -> no back claw (x1.5) and
  no certain grab from behind; the check samples the tiles behind the
  player's facing out to one tile.
- 2026-11-14, P03: the second after tearing free -> a lunge that lands is a
  claw, never a grab, so the dead cannot chain holds.
- 2026-11-14, P03: stabbing while held -> short blades only (reach 34 px or
  less: Knife, Glass Shiv), as the phase says; a Machete or Axe cannot.
- 2026-11-14, P03: "from behind" -> the take-down reads the zombie's drawn
  facing; the melee backstab bonus stays as it was (the swing's direction
  against the zombie's movement).
- 2026-11-14, P03: R -> reloads only when nothing in reach shows an "R:"
  prompt (boards, taps, repairs keep it). With a gun empty or jammed and
  nothing else in reach, the prompt says "R: Reload" / "R: Clear the jam".
- 2026-11-14, P03: G was "approach a survivor" -> that moved to E with
  nothing else in reach. O and T are unchanged.
- 2026-11-14, P03: loaded rounds -> a grave pack, a retirement pack, a
  car's trunk or a crate holds a gun's rounds as loose ammo beside it
  (`InventorySystem::itemsUnloaded`); a cupboard keeps them in the gun
  (slot `loaded`, an optional save field, no version bump).
- 2026-11-14, P03: the chair leg -> only a Table gives one (there is no
  chair furniture yet), in place of one plank, when no weapon is carried.
- 2026-11-14, P03: flares, ringing clocks and things in flight -> not
  saved; a checkpoint drops them (they last seconds to a minute and a half).
- 2026-11-14, P03: the fixed rare finds -> two Pistols (3 and 5 rounds in),
  a Shotgun with 4 shells beside it, two Machetes, two Sledgehammers, each in
  a different wardrobe from the outer third of the town's wardrobes (by
  distance from the map centre), shuffled by the world seed. Rolled loot
  goes in beside them; the container record keeps them gone once taken.
  Radios, Backpacks and Keepsakes wait for their items (P05, P07).
- 2026-11-14, P03: loot until P06's classes -> pistol_ammo in `bedroom`,
  alarm_clock in `living`, frying_pan in `kitchen`, flare in `trunk` and
  `storage`, rock in `storage`, all at weight 1. Rocks have no other source
  yet.
- 2026-11-14, P03: a wound clock -> F winds it (8 s); thrown, it keeps
  counting in the air and rings where it lands; left in the pack it rings
  where you are (and the dead come to you). An unwound clock thrown lands
  like a rock. A thrown one survives 1 time in 3 (dropped where it fell
  after ringing); one rung in the pack is never lost.
- 2026-11-14, P03: a thrown bottle -> leaves one Glass Shards where it
  breaks (the Glass Shiv's material). A rock, a can or an unwound clock is
  left where it fell, to be picked up again.
- 2026-11-14, P03: a lure -> the dead within the noise's reach (a flare:
  15 tiles and in sight of it) go there and stand for 20 s (`lureTimer`);
  the ones already after the player do not turn. A clock or flare pulls
  again every 5 s while it rings or burns.
- 2026-11-14, P03: being held -> the view pulls in 6 % and shakes; the
  pack, crafting and any open container shut, and nothing in reach can be
  used. Bites while held do not push the player about.
- 2026-11-14, P03: the shotgun's two-handed pose -> only for the shot
  (`player_shootlong`, 24 frames); carrying it uses the pistol's loadout.
  A full long-gun loadout (216 frames) does not fit the character block
  under the scenery.
- 2026-11-14, P03: the yellow swing-arc overlay -> not drawn for a shove;
  the shove has its own short white arc.
- 2026-11-14, P03: the Controls panel keeps 26 rows -> L went, Space and G
  came in, and V / Shift+V share a row.
- 2026-11-14, P03: harness -> `shove`, `reload`, `select ITEM NAME`,
  `throw DX DY [ITEM NAME]`, `zombie DX DY [idle] [down] [lunging] [away]
  [close] [exact] [hp N]`, and `expect held|free|down N|lured N|lunging N|
  kills N|loaded N|landed DX DY|burning DX DY`, `expect zombies all R`.
  "Facing away" is staged (`away`: drawn facing away and reeling 3 s so it
  cannot turn), since the dead see all round (8 tiles by day) and turn at
  once; the take-down itself is tested on one lying down. `slay` counts
  toward `expect kills` (the run's kill count), so scripts count it in.
- 2026-11-14, P03: the struggle meter -> it holds for 0.5 s after each tap
  before draining at 1.5/s. Draining all the time, four quick taps (one
  every 4 ticks) reached only 3.7 of 4, against the acceptance "four Space
  taps free the player"; a pause of half a second still lets the hold
  close again on one who stops.
- 2026-11-14, P03: a stomp's 30 px -> a body on the next tile (32 px) is
  just out of reach: you step onto it to stomp (the dead do not block the
  player's feet). `fighting.txt` stages it at 20 px (`close`).
- 2026-11-14, P03: a shotgun "drops two" at 1.5 tiles -> only side by side:
  pellets stop at the first body and there are five, so two in a line
  shield each other. `guns.txt` puts them 18 px apart across the aim.
- 2026-11-14, P03: two older unit tests (`zombie_attacks_are_survivable_lunges`)
  needed the Combat component registered, and the lone-zombie test now
  tears free of each grab at once: with nobody struggling a hold bites
  every 1.4 s, which the old "survivable in 10 s" bound did not allow.

- 2026-11-14, P04: the corpse loot class lists battery, keepsake and
  chocolate, which are not items yet -> left out; the class is rags 4,
  matches 2, painkillers 2, pistol_ammo 1. Add them with their items.
- 2026-11-14, P04: P03's `guns.txt` ("every wanderer within 27 tiles comes
  to a shot") no longer holds under P04's rules: Loud carries 20 % less
  upwind and a Brute hears at 0.7, so a kind is only sure to hear within
  16.8 tiles. The script now puts a Shambler 14 tiles off and checks every
  one within 16 comes; the later kill counts move up by one for it.
- 2026-11-14, P04: a tier's radius is now where it is heard (P03's
  question): a noise is heard out to its radius after shells, weather,
  hearing and wind; no fading threshold.
- 2026-11-14, P04: asphalt roads are "gravel" ground for footsteps (+1
  tier), so a walk on the road in heavy rain is Medium and is heard at 5
  tiles. The acceptance ("a walking player is not heard at 5 tiles") is
  tested on ordinary pavement: new harness command `goto pavement`.
- 2026-11-14, P04: districts are numbered only inside their building
  boxes; the streets are district 0. For ruin overlays and the town map's
  shading each street tile belongs to the nearest district (one fill per
  run). Spawn mix and kills still use the tile's own district.
- 2026-11-14, P04: ruin overlays -> litter (cans, paper) 2.5/5/8 % of open
  tiles by level, blood smears on 3/8/14 % of tiles beside a wall, bodies
  (a kind's last death frame) 2/6/12 per district. The phase gives only
  the 12-body cap; abandoned bags have no sprite yet and are left out.
- 2026-11-14, P04: the district readout is the word on the entry toast and
  the town map labels; the F3 debug keeps percentages.
- 2026-11-14, P04: a Crawler is hidden past 2 tiles when on grass or beside
  (within one tile of) a piece of furniture. The minimap never drew the
  dead, so there was nothing to hide there.
- 2026-11-14, P04: The Unknown's rules are noticed by what the player can
  see, once a minute at most each: by day one standing still within 6
  tiles, or at night one hunting within 10 (DormantByDay); one coming to a
  sound without seeing you within 10 (BlindHearing); one calm within 6 in
  the rain, or hunting after it stops (RainCalm); wanderers within 20 tiles
  turned by your flashlight at night (DrawnToLight); one looking past you while you
  stand still within 8 (IgnoreStill). Three sightings write it down.
- 2026-11-14, P04: learned rules live in world memory with the district
  infection, so they reach later lives without a notebook item; the
  notebook (J) lists them under "What we know".
- 2026-11-14, P04: the swarm's 0.94 and society's 0.82 speed and each
  mode's "migration" (how often the director draws wanderers in) are the
  old runtime mode tuning, moved into `zombies.json` unchanged; the phase
  table does not mention them.
- 2026-11-14, P04: The Unknown's "frenzied when the rain stops" lasts 120
  s (2 game hours); the follower is told where you are once a game hour.
  The phase gives no numbers for either.
- 2026-11-14, P04: the atlas grew to 4096 x 4096; the kinds fill a block
  of their own in the lower half, so every older sprite keeps its place
  (the upper half is pixel-identical). The Crawler is drawn for two
  facings (SE, NE) and mirrored.
- 2026-11-14, P04: W moves the player diagonally in tile space (screen up),
  so `kinds.txt` puts the dormant Crawler on that diagonal (-2, -2): one
  straight north at 3 tiles is passed at 2.1 tiles and never wakes.

- 2026-11-14, P04: "Stalker and Swarm must differ in cause" needed a
  cause that says more than "Killed by zombies" -> a death by the dead now
  names what did it: "Overrun by the dead" (five or more within 3 tiles),
  "Hunted down by the zombie that followed" (the Stalker's follower struck
  last), "Run down by a zombie" (a Runner), "Beaten down by a big zombie"
  (a Brute), "Bitten by a crawling zombie" (a Crawler), else "Killed by
  zombies". Each keeps a word the legacy reads as "will rise".
- 2026-11-14, P04: the HUD's district line under the clock shows the ruin
  word ("Quiet", "Uneasy", "Turning", "Overrun"), not a percentage.
- 2026-11-14, P04: the harness's `god on` refilled health only before each
  tick, so a horde biting in one tick could still kill (`horde.txt` died
  on night 8). God mode now also holds at the death checks.
- 2026-11-14, P04: the Crawler's cover check only searches for furniture
  indoors (furniture stands indoors; the lookup is a scan of every piece),
  after the 2-tile distance test: the first version cost `perf` 1 ms on
  average and a 13.6 ms worst tick.

- 2026-11-14, P05: saves -> no version bump. A wound without a part is on
  the torso; a severity above 0.75 on a cut or bite reads as Deep; single
  traits become one-item lists; a checkpoint with no traits takes the
  seed's old pick (`traitrules::legacyPick`), so old characters keep theirs.
- 2026-11-14, P05: the character screen -> harness `newrun` is the quick
  start and `newrun create` shows the screen. The name is typed with
  letters ("-" for a space, Backspace) without SDL text input; Space quick
  starts and begins at once.
- 2026-11-14, P05: Tab cycles PACK -> BODY -> MAKE -> closed; Esc closes
  any of them. N still opens MAKE straight away.
- 2026-11-14, P05: boarding -> a held action (4 s at Carpentry 0, scaled by
  panic), not instant. R again while boarding does not start over. A
  board's strength is a per-board `toughness` (Carpentry, and x1.5 when
  reinforced with a second plank from level 3), saved with the boards.
- 2026-11-14, P05: panic drift -> now only nudges a walk (keys held). It
  used to move a player standing still, which carried them out of reach
  of a door mid-board and, with Shaken's louder steps, made footsteps out
  of standing still. A bug the louder steps exposed, not a retune.
- 2026-11-14, P05: a failed window climb (exhausted or overloaded) slips
  back with a 5 % chance of a broken leg; an overloaded knockdown by a
  Brute, 10 %. Car hits wait for P11.
- 2026-11-14, P05: an unsplinted broken arm -> no two-handed weapons, and a
  missed swing wears the weapon twice as much (the spec's "hurts").
- 2026-11-14, P05: the Disinfected Bandage dresses two wounds and cleans
  them but no longer cures an infection (antibiotics do). Painkillers take
  15 off panic as well as the pain.
- 2026-11-14, P05: rebleeding -> a deep unstitched arm wound reopens on 30 %
  of swings; a deep leg wound on a 30 % roll every 10 s of running. Either
  is `Bleeding` at 1 health a second for 6 s. A deep torso or head wound
  bleeds slowly (2 an hour, 1 dressed) in WoundSystem until stitched.
- 2026-11-14, P05: half of claw and bite wounds are deep; a Crawler's bite
  on the legs is always shallow.
- 2026-11-14, P05: fever above 39 C (the seeing-things trigger) needs
  illness from an infection near the top; it is rare on purpose.
- 2026-11-14, P05: a book is read over 20 game minutes indoors; each copy
  once per character, counted in the slot's `loaded`. The keepsake works
  once per character (`MentalState::keepsakeUsed`).
- 2026-11-14, P05: hesitation (Numb) delays swings and treatments.
  Searching a container is an instant open, so it has no hesitation.
- 2026-11-14, P05: Fighting XP comes from kills and shove saves only; XP
  per landed blow is not wired (kills already follow landed blows).
  Cooking XP (Living off the land) is any Fire recipe at a fire, and its
  output counts as a hot meal.
- 2026-11-14, P05: the broken-arm frames (`player_<load>_{idle,walk,run}armin`)
  are a new atlas block at y 3456, full width (the character block was
  full). The limp is uneven frame timing on the ordinary walk.
- 2026-11-14, P05: the harness counts rebleeds (`expect rebleeds`) rather
  than checking `Bleeding` after a fixed number of swings, because each
  swing is a 30 % roll.
- 2026-11-14, P05: older scripts -> boarding waits are 4.5 s a board (6 s
  where the player is shaken); panels close with Esc; `base` waits 14 s
  for a table broken up while shaken; `first_day` closes each cupboard
  with Esc (it had left the fridge open since the storage panel came in,
  so it never took the kit or boarded the door).
- 2026-11-14, P05: dressed wounds stay on the body now, so the blood trail
  counts only undressed cuts and bites.

- 2026-11-14, P06: where locks live -> in `Game` (`mLocks`: tile, alarm,
  spent), rebuilt from the layout's kinds each run and thinned by the
  world memory's "unlocked" list, not a field on the door record. Door
  records only exist for doors that changed; a lock is a fact of the town.
- 2026-11-14, P06: the inner lock (the pharmacy's stock room door)
  -> E does not turn them from inside either; only the street and back
  doors turn from inside. They exist to be forced.
- 2026-11-14, P06: which break-ins ring an alarm -> any forced door,
  broken door or broken window of an alarmed shop while the power is on,
  taped windows included (the tape is quiet, the alarm is not). An alarm
  needs its box: a building rings only if it has one (`addAlarmBox`), so
  the box is always there to stop it.
- 2026-11-14, P06: foraging spots -> only scenery (bush, tree base, tuft of
  tall grass); `forage::Spot::Wild` exists for open Wilderness ground but
  nothing uses it yet (see Later). Bushes and trees already stand in every
  district, so the land is worth searching without new art.
- 2026-11-14, P06: "Make panel filtered to what this station can do" -> a
  fourth Make tab, FOOD (everything cooked at a fire); E at a lit campfire
  opens it, X at one feeds it fuel (E had lit and put out fires). A kitchen
  stove counts as a fire while the power is on.
- 2026-11-14, P06: floor lamps without power -> the switch is remembered
  (E still flips it, nothing lights) so the room lights again if the
  harness forces the power back. Power is per run, as the phase says.
- 2026-11-14, P06: a dead garden bed -> E clears it (gives back nothing).
  Beds carry to the next life as seedlings (stage 0, regrow timer reset);
  snares carry over too, reset to set and empty.
- 2026-11-14, P06: where battery charge lives -> in the flashlight's or
  radio's own slot condition (1.0 full, 0 flat): no new save field, and it
  rides through containers, drops and checkpoints for free. The last-rain
  hour (for mushrooms) is not saved: a resume forgets one day of rain.
- 2026-11-14, P06: kinds and the world -> kinds are a pure function of the
  layout (old layouts get the same kinds on load); 50 % of locked
  buildings get an unlocked back door by a hash of the building, not a
  roll; a key ring is decided per kitchen container by hash (1 in 8) and
  remembered once used; a toilet gives one fill per world; there is no
  bathtub piece, so toilets are the indoor dirty water; whether a mushroom
  is bad is rolled when it is eaten, not when found.
- 2026-11-14, P06: Game.cpp was near its size limit -> `buildNeedsEnvironment`
  moved to `GameBody.cpp`; the phase's new code is in `GameScavenge.cpp`
  (locks, alarms, searching, water, foraging, gardens, snares, piles) and
  `GamePower.cpp` (power, lamps, fridges, batteries, radio).
- 2026-11-14, P06: shop signs -> drawn by `StructureView` on the eave
  above the street door (lifted 61 px and depth-sorted over the roof
  edge), sheared to the wall's slope. On the wall face below the eave they
  were hidden by the roof at default zoom.
- 2026-11-14, P06: harness additions -> `goto kind|forage|wild`, `recipe`,
  `power on|off|auto|soon`, `grow DAYS [dry]`, `locks [arm]`, `fridge
  mark`, and checks for messages, prompts, power, alarms, locks, gardens,
  fridge rate and needs. `locks arm` exists because seed 1234's pharmacy
  rolled no alarm (60 %): `break_in` arms the nearest locked building
  rather than search seeds.
- 2026-11-14, P06: "interiors walkable end to end" -> the unit test floods
  each non-house interior from the street door as built and again with the
  furniture in, and requires every tile reached before to be reached after
  and every piece to be usable. The map generator sometimes leaves a
  doorless sliver room (seed 1234, a workshop) that no furniture could
  open; that is the generator's, not the furnisher's, and is not counted.
- 2026-11-14, P06: snares said "Something is already here." everywhere ->
  the occupied check counted the player's own tile; it now looks for a
  structure on the tile only.
- 2026-11-14, P06: Scavenging 2's "worth searching" mark -> a small pale
  diamond pulsing over unsearched containers within 3 tiles; searched
  state only, never contents.
- 2026-11-14, P06: older scripts -> `storage` waits 2.5 s after E (the
  first opening of a cupboard is now a search); `guns` stands its target
  still (`still`, not `idle`): an idle one drifted 13 px before the shot,
  how far depending on every roll the town takes first, and at six tiles
  the 11 px hit circle is that tight.
- 2026-11-14, P06: the bot -> drinks Dirty Water only when nothing clean is
  carried and thirst is below 35; fills empty bottles at an unused toilet
  or a pond within 60 tiles once the taps are off; forces locked doors with
  a crowbar (Careful never does, and goes round); ranges 8 tiles further
  each day, up to 40 more.
- 2026-11-14, P06: the first soak found the player starting inside a floor
  lamp (the start is the house's middle tile, where a free-standing lamp
  can land) and unable to move: seed 8 never left the tile. The player's
  start is now nudged off furniture like every other spawn. A lamp also
  took E from a cupboard or bottle beside it; it now yields to anything
  else in reach, as a bush does (step to its free side to switch it).
- 2026-11-14, P06: the bot stepped while pressing E at a cupboard (it
  leans in to reach), which cancelled the new held search: it searched 1
  to 14 containers a run and boarded nothing (0 boards on all ten seeds).
  It now stands still on the press, as phase 5 made it do for boards, and
  likewise when it forces a lock.
- 2026-11-14, P06: `power_out` forces the failure with `power soon`
  instead of `day 5`, `hours 18.9` and a wait: the run seed shifts the
  hour by up to 12 hours, so a fixed clock would not meet it. The hour
  itself is the unit test's (`power_fails_on_day_five_near_dusk`).
- 2026-11-14, P06: with a panel docked, the HUD's message line ran off the
  left edge of the narrowed world and over the condition labels (seen in
  `cooking`'s shots). It is now shortened to the width that is left (the
  panel shows the full text) and sits above the condition labels.

- 2026-11-14, P07: where Snow sits in `WeatherCondition` -> appended after
  the existing values (with Heatwave and Blizzard), so saved weather
  records keep their numbers.
- 2026-11-14, P07: "Body" in the table -> a shift of the ambient
  temperature, outdoors only (`weatherrules::bodyShift`); indoors the
  shelter rule stands. The heatwave's +8 counts only out of the shade
  (indoors, or within reach of a tree canopy).
- 2026-11-14, P07: the Raincoat -> as the table (rain and heavy rain 0),
  and a thunderstorm with a Raincoat is -2 C rather than 0: the wind still
  bites. Snow and the blizzard ignore it. It sits in bedroom loot at weight 1.
- 2026-11-14, P07: clothing in hot weather -> carried warmth never lifts
  the ambient above 30 C (where heat starts to tell), so a coat in summer
  does not cook the player; the heatwave's coat rule (+3 C) is the only
  extra.
- 2026-11-14, P07: storm fires -> a thunderstorm puts out fires in the
  open as heavy rain does (40 game minutes); the table gave only its
  spread (x0.2).
- 2026-11-14, P07: wind -> carries a fire's spread from strength 0.25 (the
  downwind x1.6, upwind x0.6), and adds the two downwind diagonals above
  0.5, as the phase says. The weather system's wind drifts slowly toward a
  direction and strength that each condition rolls.
- 2026-11-14, P07: the HUD's second weather word -> "clearing",
  "clouding", "rain near", "storm near", "fog near", "snow near",
  "blizzard near", "warming", from the forecast 90 game minutes ahead.
- 2026-11-14, P07: Wet and smoke state (`wetHours`, `smokeSeconds`) are not
  saved: both last an hour at most, and a resume after shelter is dry.
- 2026-11-14, P07: snow on the ground -> builds to full over 6 game hours
  of snow (3 in a blizzard), melts over 12 game hours above 2 C, is kept
  in the weather record (`"snow"`, optional, no version bump). Ponds are
  frozen for the whole of winter rather than by temperature, so a warm
  winter afternoon does not drown a player standing on the ice.
- 2026-11-14, P07: the clear-day view -> 20 tiles; the table's percentages
  fade the world from that (rain -10 % starts the fog at 9.9 tiles). Fog
  fades from 5 tiles to 9, the blizzard from 3 to 5.5.
- 2026-11-14, P07: the dead's night -> follows the season's dusk and dawn
  (`FirstDays` hours become functions of the day), so a winter night is
  longer for them too; summer's x1.1 by night is on top.
- 2026-11-14, P07: thunder and lightning -> every visual flash of
  `Atmosphere` gives the dead 0.2 s of x1.5 sight; the Explosive thunder
  near the player and the 5 % tree fire come from a slower simulation
  timer (a strike every 40 to 120 s in a storm), which also lights the
  sky, so the dead are not drawn across town by every flicker.
- 2026-11-14, P07: fuel of a wall -> walls and doorways of Plaster (wooden)
  buildings are 0.4; brick and metal buildings burn only inside. A put-out
  tile that had burned a while (0.1 of its fuel) counts as burned and
  chars a tree on it, as a burned-out one does; the next life finds both.
- 2026-11-14, P07: "burned grass regrows after 2 lives" -> it is green
  again on the third life after the fire (the two lives after it find it
  black).
- 2026-11-14, P07: the fire grid in checkpoints -> the existing
  `FireTileRecord` holds a tile's heat (`intensity`) and fuel
  (`lifeRemaining`), with an optional `byPlayer`. Fires restored from
  world memory at a new life are not the player's.
- 2026-11-14, P07: the season's day card -> on a season's first day the
  dawn card's line names it ("Summer."); on day 43 the line is "Frost on
  the windows." Both replace the day's usual line, not add one.
- 2026-11-14, P07: footsteps in snow -> one tier quieter, applied where the
  player's footstep noise is made (`GameUpdate`), not in `NoiseModel`, as
  the tier comes from the weather and the ground under the player.
- 2026-11-14, P07: the smoke column -> particles above a burning building
  and a grey mark on the minimap (and town map); no tall column sprite.
- 2026-11-14, P07: "a burning doorway is not walked through by survivors"
  -> any survivor stops before a burning tile in its way (survivors are
  P08's; today's are the legacy ones). The dead walk through and burn.
- 2026-11-14, P07: harness additions -> `season NAME`, `wind X Y`, `air C`
  (held air temperature; `air off`), `ignite DX DY [N|molotov]`, `fires`,
  `weatherinfo`, `strip` (warm things out of the pack), `goto pond [on]`,
  and the checks `expect weatherinfo NOISE SIGHT BODY`, `expect
  drawn|undrawn DX DY`, `expect spread east|west`, `expect fires N`,
  `expect gutted|notgutted`, `expect walkable|solid`, `expect collector
  MAX`. `expect burning` reads the grid. `weatherinfo` also prints the
  body temperature and clothing.
- 2026-11-14, P07: the ground round a house (the strip inside its lot) is
  sheltered, so snow does not lie on it and a night there is not cold.
  `seasons.txt` tests the cold in open grass (`goto wild`). Changing which
  tiles are indoors was out of scope.
- 2026-11-14, P07: snow on roofs -> a pale wash on the roof sprite by
  accumulation (green and blue lifted more than red, so a red roof goes
  grey-white, not pink), no snow-capped roof set; the gutted roof is the
  one new roof set (`roofp_burnt_<mask>`).
- 2026-11-14, P07: burned ground -> the tile itself is drawn charred dark,
  as well as the old scorch decal, so a burned floor shows inside a
  gutted house.
- 2026-11-14, P07: a frozen pond's prompt -> "R: Break the ice, fill
  bottles (dirty)"; without an axe or hammer R says "The pond is frozen.
  An axe or a hammer would break the ice."
- 2026-11-14, P08: a survivor's stock -> their own `Inventory` (the
  record's `stock` when far or between lives), so trade, robbery, "Carry
  this" and a body's loot all use the container code. No separate store.
- 2026-11-14, P08: water -> a survivor not travelling with you drinks from
  their house's tap (or rain barrel) when they have no bottle, so water
  never sets their stock days or kills them; food does. Thirst killed
  every survivor between lives in the first runs of `advance`.
- 2026-11-14, P08: between lives -> one meal a day from stock, else a
  forage roll (20 % + 40 % x competence); a loot trip when food is under
  the low-stock line; death from hunger only after 6 days without food and
  none left. Without the forage roll half the town starved in ten days,
  which reads as a bug, not a world.
- 2026-11-14, P08: going out for supplies -> once a game day, near or far,
  and a trip cut short by the dead counts as that day's. Near survivors
  otherwise stripped the cupboards round the player faster than the
  between-lives rule does elsewhere.
- 2026-11-14, P08: the dead and survivors out of sight -> a survivor beyond
  the near radius (simulated an hour at a time) is not in the dead's grid,
  and the dead see a survivor at 0.6 of their sight. A setup with the
  houses' own dead cleared from lived-in homes. Before this, 5 of 12 died
  in the first 10 hours with nobody near them; now about 1.
- 2026-11-14, P08: health -> survivors mend 1 an hour when fed and watered
  (3 asleep or far), as the player's slow mending; otherwise one bite
  stayed with them for the whole life.
- 2026-11-14, P08: Fortify -> every door but one is boarded (they still
  come and go; they do not climb through windows), then the windows. A
  house with one door gets its windows boarded. Planks and two nails a
  board, as the player.
- 2026-11-14, P08: "they climb windows" -> not built: survivors path
  through doors only (open, close, shut behind them; a locked door stops
  them). The shared A* (`Pathfinder`, the living mover) has no window
  edges; a window route is "Later".
- 2026-11-14, P08: where survivors go -> containers within 40 tiles of
  home, never in a lived-in house, the player's home or base, an armoury
  or a building with a locked door (no keys, no crowbar); one they could
  not reach is skipped for the life. A locked door on the path made them
  walk into it for ever.
- 2026-11-14, P08: a companion and the dead at your side -> a companion
  also fights one of the dead within 3 tiles of you when they are within 8
  tiles of you, not only one on themselves; otherwise they watched you
  being held.
- 2026-11-14, P08: someone talking to you stands still (unless they are
  fleeing or fighting), so the panel does not close as they walk off.
- 2026-11-14, P08: a companion's grave -> a legacy grave with
  `travelledWith` set; it is not a life of the bloodline (not counted in
  "N came before you"), keeps no journal page, and the card and map list it
  like any grave. Their pack lies by it.
- 2026-11-14, P08: the notebook's "Who is out there" -> a section of the
  one notebook page (tabs are P10's): up to five, living first; met by an
  earlier life reads "Someone wrote: NAME, WHERE. STANDING." A record keeps
  `metRun` for that (no version bump: it defaults to 0).
- 2026-11-14, P08: the map's mark for a known home -> a small code-drawn
  house in pale green with the name, and "Someone lives here" in the
  legend when one shows.
- 2026-11-14, P08: harness additions -> `survivor ...` (with `house`,
  `crowd`, `dead`, `fill`), `survivors`, `goto survivor`, `goto open N`,
  `set trust`, `set shunger`, `talk`, `trade`, `advance DAYS [apply]`,
  `waitfor SECONDS survivor COND`, and the survivor, talk, card and advance
  checks (README). `survivor fill` keeps that container for the focus:
  everyone else skips it, or another survivor emptied it first.
- 2026-11-14, P08: older scripts that test the player and the dead alone
  (`break_in`, `windows`, `mind`) start with `survivors gone`: the town's
  people now draw the dead (noise, prey) and make noise, so "the dead stay
  calm" and "no noise was made" no longer held with Dev and Hana or Viktor
  in earshot. `fighting` waits for the hold (`waitfor 10 held`) instead of
  4.5 s: whether a lunge from the front claws or grabs (55/45) follows the
  zombie's entity number, which the survivors shifted. `guns` expects 6
  and 8 kills, not 8 and 10: `slay` finds five of the dead, not seven, now
  that lived-in houses are cleared of theirs.
- 2026-11-14, P08: the dead cleared from lived-in homes at setup -> never an
  earlier life's risen (the legacy's own); a retiree's home held one in
  `legacy.txt`, and it vanished.
- 2026-11-14, P08: the Controls panel's columns -> 440 px (at most half the
  window less margins), so "Use, set down, snare, radio" and the others no
  longer run into the second column; no keys were added or taken away (O
  and T were already gone; E now says "Interact, talk; hold: push").
- 2026-11-14, P08: the bot -> it leaves alone the cupboards of a house
  someone lives in (it walked in, was warned out, kept searching and was
  killed by the owner: six of ten soak lives ended on day 1 with no kills),
  leaves alone cupboards within 12 tiles of a hostile one's home, and runs
  from a hostile survivor in sight within 10 tiles (or one going for it
  within 8), fighting only when cornered; at home it does not run from one
  only watching. It does not talk or trade.
- 2026-11-14, P08: survivors and sight -> a hostile survivor goes for you
  only along the dead's line of sight (walls; glass only so far), and
  shoots only along a clear line; before, the one with the pistol shot
  the bot through a wall from seven tiles.
- 2026-11-14, P08: the garage and the start -> the two hostile ones are
  never cast within 20 tiles of where the first life starts while a garage
  or house away from it is free (`HomeChoice::nearStart`), and a later life
  does not start within 20 tiles of a hostile household (aggression over
  0.7, or the garage) while another house is free. In two soak seeds they
  lived 12 and 16 tiles from the start and killed the bot on day 1; they are
  somewhere to learn to avoid, not the neighbours on the first morning.
- 2026-11-14, P08: the metrics' `boards_up` -> boards on the survivors'
  own homes are not counted (it is the player's work; their Fortify put it
  from 13 to 72 in the first soak).
- 2026-11-14, P08: a death by a survivor's blow -> the cause reads "Killed
  by NAME" (it read "Killed by zombies"); without a zombie word in it the
  character stays down.
- 2026-11-14, P08: the perf check -> appended to `perf.txt` (20 survivors
  and 100 of the dead by day, after the 300 at night), not a new script.
- 2026-11-14, P08: `companion.txt` -> the companion fights bare-handed and
  is left waiting for the Brute; with a bat at the player's side they
  killed the Brute without a scratch (they fight by their numbers, which
  are phase 14's to weigh).

- 2026-11-14, P09: where the gate goes -> stamped once into the town wall
  when the slot's layout is made or loaded, in the old campaign
  objective's district if the slot had one, else the district farthest
  from the start; the old `campaignObjective` is gone. One gate per world,
  and v4 saves keep the place they were sent to.
- 2026-11-14, P09: the control box -> a box on a post beside the gate, its
  four lamps drawn as small tinted quads (dark, amber, lit), not a sprite
  per state. Reads at the current pixel scale without new art per state.
- 2026-11-14, P09: the gate tiles -> blocked for walking until it opens,
  then free; the generator tile stays blocked. The wall has no other gap.
- 2026-11-14, P09: the key -> its whereabouts are gate state (on the
  Sergeant, loose, turned in), not a tracked item. On the Sergeant it
  respawns him each life if he is not walking; loose, it is put back where
  it was last seen outdoors only when nothing in the town (packs,
  containers, floors) holds it. The fuse is put back in the hardware
  cabinet when it is not fitted and nothing holds one.
- 2026-11-14, P09: the fuse cabinet -> locked like the armoury (X with a
  crowbar). Falls back to a container in the guard hut when the town has
  no hardware store.
- 2026-11-14, P09: a Fuel Can -> 8 kg, so carrying four is a choice.
- 2026-11-14, P09: fuel to start -> the first start needs all 4 cans; a
  restart in the same life needs any fuel. A new life finds the gate shut
  again and needs 4 again (a stand burns about a can a minute, so a death
  late in it means pouring again).
- 2026-11-14, P09: pouring before the generator is freed -> allowed. The
  table lists the needs side by side, and blocking it only adds a trip.
- 2026-11-14, P09: the stand mid-way -> kept only in the run checkpoint
  (resume puts it back); between lives it is gone, as the phase says.
- 2026-11-14, P09: readables -> each can be read once a life (E then does
  the container's search). Read every time, a note whose leads were known
  swallowed E and the fridge never opened.
- 2026-11-14, P09: the radio broadcast -> starts 6 s after the radio goes
  on, after the forecast line, and teaches lead 1 after three lines.
- 2026-11-14, P09: the inherited notebook -> found in the grave's pack
  (`Pack` legacy link); taking it carries the leads and the gate page as
  "as of day N of an earlier life". A grave only gets one when the dead
  character carried a notebook and knew something.
- 2026-11-14, P09: a companion at the gate -> leaves too when within 8
  tiles as the player walks through; named on the Out screen, not a
  survivor again.
- 2026-11-14, P09: HUD messages -> wrap upward, at most three lines of 72
  characters. The notes are longer than one line and were cut off.
- 2026-11-14, P09: dropping on the floor -> a pile never merges into a
  body (`Corpse`). The key fell into the Sergeant's body and read as gone.
- 2026-11-14, P09: harness scripts with held jobs at the generator ->
  `slay` the dead near it first (a hit breaks a held job off, god mode or
  not), after the Sergeant has been met so he is not slain with them.
  `take ITEM` closes the container it opened.
- 2026-11-14, P09: older scripts the gate moved -> `guns` expects seven
  slain near the start, not five (the town changed, so the dead stand
  elsewhere); `horde` starts with `survivors gone` (its horde set out near
  a lived-in house and that house's evening lamp drew all of it in);
  `perf` lets the slain settle half a second before timing (the tick the
  Sergeant falls in, his key dropped and the gate saved, took 20 to 34 ms).
- 2026-11-14, P09: `gate_across_lives` -> life 2 pours the four cans in two
  trips (a pack with the starting kit holds two cans at 8 kg); life 3 tops
  the fuel up from 3 (the stand burned one) before it can start.
- 2026-11-14, P09: the title's Game Mode -> "THE WAY OUT" (campaign) and
  "ENDLESS"; Endless has no way-out objective but the gate is still there
  and can be opened.
- 2026-11-14, P10: where a journal lives -> on the character's history
  record (and the run checkpoint while the life goes on), not on the
  grave. Why: retired and escaped lives have no grave, and "Before you"
  lists every life.
- 2026-11-14, P10: having a notebook -> one is put in the home kit (a
  drawer of the first home) every life; the Start entry says "Found this
  notebook in a drawer". Why: the spec writes only while a Notebook is
  carried, and the item was otherwise rare; picking it up is still the
  player's choice.
- 2026-11-14, P10: at most three entries a day (the least important of
  the day goes, ties the newer); the last entry always stays. Why: a
  busy first day otherwise crowds out the rest of the life.
- 2026-11-14, P10: the reader -> drawn in the notebook's place (docked
  right, the same panel), opened by E at a grave or stone of the line, or
  Enter on "Before you"; it closes on J, Backspace or walking 2.5 tiles
  from the grave. A journal still with its grave, never read, says "X's
  journal lies with them. Find the grave." on "Before you". Why: reading
  at the grave is the moment the spec asks for; after that it is the
  player's to read again.
- 2026-11-14, P10: reading a journal the first time -> +3 depression, or
  -10 for one who got out. Read is a flag on the record, so echoes count
  it for every later life.
- 2026-11-14, P10: the marker -> an item (`Marker`, weightless, one to a slot) whose "loaded"
  field is the grave's index + 1; taken with E from a grave with no pack
  (emptied or gone) that is not risen. F inside the fence sets it: the
  grave moves to the next stone slot (three old stones take the first
  three), its lead reads "X's stone, in the cemetery", -12 depression
  once. Dying with a marker puts it back on its grave. Why: an item is
  carried, dropped and saved with no new machinery.
- 2026-11-14, P10: the cemetery -> a 9 x 8 fenced plot stamped once like
  the gate (save layout `cemetery`), on open ground near the town middle,
  with one gap; three old stones of the town stand in it. Scenery is not
  placed inside.
- 2026-11-14, P10: survivors buried by hand -> E on an emptied survivor's
  body, 30 s; the grave is a survivor's ("Buried by NAME"), where the
  body lay. Why:
  carrying a body is a system of its own; noted in "Later".
- 2026-11-14, P10: Gravedigger's "marks" -> the town map rings the risen
  within 20 tiles of the player.
- 2026-11-14, P10: the world kind -> chosen on the title's "Start New
  World" row (Left/Right: Fresh Start / Living World), stored on the
  save; old saves are Living World. Fresh Start forgets the town at each
  life's end (the same `forgetWorld` as before) and skips aging, graves
  the card, echoes and bloodline traits; the list of lives is still kept
  for the title's count.
- 2026-11-14, P10: district infection between lives -> the town ages it at
  the life's end (+0.5 a day lived, -10 a life, and the P07 burn), floored
  at the next life's start, and the world's first look records the
  infection "at creation". The old P04 code took the -10 at load and set
  `appliedRunSeed` to the new seed, so a life's first look could be
  skipped; aging now writes the ended life's seed, and the next life
  thins its districts as P04 meant.
- 2026-11-14, P10: storage crates and spike boards now wear between lives
  (the BuildSystem flag), with the crate's stages Intact, Damaged,
  Breaking, gone; a crate in a building nobody living keeps loses a fifth
  of its food stacks and, one time in ten, is found and emptied by the
  nearest living survivor (they get the things). Boards lose a quarter of
  a board's health per board each life.
- 2026-11-14, P10: burned grass regrows after 2 lives (was 3), as the
  spec's table says.
- 2026-11-14, P10: population -> each earlier life adds 4 % to the town's
  dead (at most 60 %), Living World only. Why: the spec's "the town fills
  in"; capped so a long bloodline is not swamped.
- 2026-11-14, P10: bones -> a pile where each body of the dead lay when a
  life ended (the last 40 kept, Living World), in the save's aging record.
- 2026-11-14, P10: Vigilant's "woken a second sooner" -> the night caller
  wakes the player when 1.5 tiles farther off (a walker's second); panic
  falls 15 % faster as before.
- 2026-11-14, P10: Scavenger -> searching takes 10 % less (not the
  armoury); Firewise and Gravedigger as in the spec's table.
- 2026-11-14, P10: setting a stone -> its own journal kind (`stone_set`,
  rank 6), so a life that set one says so.
- 2026-11-14, P10: "Stay here for good" -> on the pause menu from day 10,
  three presses (the label changes to confirm); refused with the reason
  under the menu unless the building has three boarded ways in and a bed,
  and needs, health and infection are fine. It ends the life as a chosen
  retirement ("X stayed.").
- 2026-11-14, P10: the harness's `age LIVES DAYS` ages the save as that
  many lives ending now (the next `newrun` sees it); `aging.txt` uses three
  40-day lives because one-day lives clean more than they add.
- 2026-11-14, P10: `boards.txt` passes but never boards its door: boarding
  needs two nails a board (P02) and the script gives none. Left as it is
  (it checks what it prints, not the boards); `journal.txt` and
  `aging.txt` give nails.

- 2026-11-15, P11: where vehicles are placed -> `vehiclerules::place`
  picks open ground beside buildings from the layout and the world seed
  after the town is made; nothing is stamped, so the generator's layout for
  every existing seed is unchanged. Standing scenery (trees, lamp posts,
  hydrants, bins) planted later skips the tiles
  under a vehicle (each tile rolls its own, so no other tile changes).
- 2026-11-15, P11: the run's random stream -> spawning still takes the old
  two draws per vehicle spawn tile from the run's RNG when the save has no
  vehicles, so loot, the dead and survivors roll as before; vehicles use
  their own RNG from the run seed (`^ 0x7EC1`), trunks theirs (`^ 0x7A11`).
- 2026-11-15, P11: old saves -> a sedan record without a kind is read as a
  sedan at its old place, centred; the rest of the fleet is placed around
  it once and remembered (`fleetPlaced`). Save version stays 5; every new
  field is optional with a default.
- 2026-11-15, P11: the key's container -> a Kitchen or Bedroom container in
  the nearest house (a counter, drawer or wardrobe, picked by the seed);
  when that house has none, the key is in the car. Searching it says "Car
  keys. The red sedan outside." and the car is known for good. A sedan
  from an old save keeps its spot and has no key (hot-wire it).
- 2026-11-15, P11: paints -> four for a sedan, two for a pickup, so "the
  red sedan" means something; the sprites are drawn per paint by the
  generator, five facings each (the other three mirrored).
- 2026-11-15, P11: the owner's car -> the car nearest a survivor's home,
  within 10 tiles, is theirs; getting into it is the theft (noted once a
  life). A companion within 4 tiles gets into a car with you (seats 2).
- 2026-11-15, P11: the E chain -> at a vehicle, E is the vehicle's first
  for what needs a thing in hand or a load: Refuel with a Fuel Can
  selected, Mend it with a Toolbox, Load and Unload at a pickup's
  tailgate; otherwise the door or trunk prompt as before. "Set it off" on
  a live alarm is last of all, and only when nothing else in reach offered
  E (a door, a window, someone to talk to, a container, tall grass): the
  first soak of the phase had the bot set off two alarms (seeds 5 and 8)
  with presses meant for other things; a player would have too. The
  "V: Get in" hint is added to whatever prompt is showing (a corpse, a
  forage spot) rather than replacing it.
- 2026-11-15, P11: thin scenery -> bins, hydrants and the cemetery's low
  fence are run down; lamp posts and trees stop the car (bushes and grass
  never blocked). A doorway is a wall to every vehicle, the bicycle too,
  open or not: none of them goes indoors. Hordes needed no new rule: they
  already ignore Soft noises and turn for Loud ones, so a parked car is
  nothing to them and a running engine turns them.
- 2026-11-15, P11: Shift+V refuelling is gone (the phase replaces it with E
  "Refuel"); a can adds 12 units, 6 s held.
- 2026-11-15, P11: the trunk -> an invisible container entity that rides
  with the car (prompt "Open the trunk" or "Open the truck bed") and opens
  with the normal held search; it is saved through the container records
  like any other.
- 2026-11-15, P11: the bicycle turns round instead of backing up: with the
  keys pointing behind it, it brakes and turns at walking pace (a car
  brakes and reverses). Survivors never ride it; a companion (P08) rides
  in a car only.
- 2026-11-15, P11: Unload at a doorway -> the piece goes on the first free
  tile behind the tailgate; a doorway counts only when its door is open,
  and the piece then stands across it (the doorway is blocked, P02). A
  shut door: "The door is shut."
- 2026-11-15, P11: a moved piece of furniture keeps the building it was
  made in on its record; checks that need the building it is in now (the
  harness's `expect blocked`) read the tile under it.
- 2026-11-15, P11: the engine noise is the player's noise while driving:
  the footstep emitter is off in a vehicle, the engine's Loud every 0.3 s
  is on (Medium every second idling), a pickup's heard x1.2 through a new
  `reach` on noise events.
- 2026-11-15, P11: the harness's autopilot (`drive road`) follows the
  asphalt with a breadth-first search back onto it and a lane-centring
  nudge; `vehicle fetch` and `vehicle park door` are shortcuts (the piece
  is moved against the tailgate; the pickup is put at a door and the door
  opened) so `pickup_haul.txt` checks the load, the drive and the unload,
  not pushing a wardrobe across town.
- 2026-11-15, P11: `mind.txt` changed: its three "dormant" dead are
  Shamblers, and the harness's `dormant` only makes a Crawler lie in
  wait, so they wandered. The town now makes more entities (ten vehicles
  and their trunks), the dead's wander rolls are keyed by entity, and on
  seed 1234 one wandered to the home's door and banged on it: 16 noises
  in the "nothing changes" check and the boarding interrupted. They are
  now `dormant still` (stand where put, still look and listen), which is
  what the script meant.
- 2026-11-15, P11: `Game.cpp` only shrinks: the component registrations
  are one `World::registerComponents<...>()` call (a new variadic
  helper), which makes room for `TrunkOf` and the checkpoint's
  `persistVehicles`.
- 2026-11-15, P11: `vehicles.txt` lets the tail batter the parked sedan
  for 60 s: by then the window is gone and the condition is near nothing,
  so the script sets it back (`set condition`) before the run-downs and
  again before the save, so the next life's -15 shows.

- 2026-11-15, P12: one call does both. `Game::makeNoise(pos, tier, seconds,
  SoundId, ...)` emits the noise event with its sound attached; the mixer
  takes the heard sounds from `NoiseModel` at the top of the next tick
  (`updateSoundscape`) and plays each at its source. Every older
  `emitNoise` became a `makeNoise` with the same tier, seconds and source,
  so the dead hear exactly what they heard before. Sounds that are not
  noises to the dead (a car door, a curtain, a casing, a pack closing, the
  interface) are `playAt` or `play` only and add nothing to the simulation:
  no entities, no noise events, and no draws from the run's random
  streams (the mixer has its own, `mSoundRng`, and the mood its own
  xorshift).
- 2026-11-15, P12: occlusion is one cut. A sound from another building, or
  across the shell while it is sealed (`buildingSealedNow`: phase 4's rule,
  worked out for the ear, uncached, never in the simulation's path), plays
  at x0.45 and as its `_muffled` twin when it has one (13 sounds: the
  groans, growl and shriek, the Screamer, door, window and board bangs, the
  hammer, the called "hey", both guns; rain has its roof layer instead).
  Beyond 15 tiles a sound with a far tail
  plays the tail. Listening at a door lifts the whole cut while you stay
  (crouched within a tile of a closed door, still 1 s), and the edges dim
  10 %; it shows as "Listening." in front of the prompt line.
- 2026-11-15, P12: the heartbeat is timed one-shots, not a loop: a beat at
  60 to 140 a minute by panic (100 at least under 20 % health), so the
  rate is exact and the red edge can pulse on the beat
  (`mHeartbeatSeen`). The breath under 30 % stamina is a loop that rises.
- 2026-11-15, P12: stings are silent in Hopeless (no music at all, as the
  design says, and a sting is music); Grim Human and Brutal Arcade get the
  dusk note, the first night caller's first bang each night, and dawn
  survived. Death plays the remade, softer sting in every tone.
- 2026-11-15, P12: the device opens at 22 kHz stereo (every file is 22 kHz
  mono, so nothing is resampled); 24 channels, 3 identical at once, the
  quietest lowest-priority voice dropped. The 11 old placeholder WAVs are
  gone, replaced by 589 generated files and `assets/data/sounds.json`.
- 2026-11-15, P12: the volumes (master, effects, ambience, music) and the
  screen-effects level are read from `save/settings.json` if it exists and
  never written: the screen for them, and writing the file, are P13's.
  Harness runs set screen effects to off whatever the file says.
- 2026-11-15, P12: the existing body effects (the red edge under 40 %
  health, the panic bands, the held shake from P03, the dusk and night
  tints) stay as they were, outside the screen-effects setting; the new
  pass adds on top of them (so panic shows the old bands and the new
  vignette). The new pass, the mood idles, the breath puffs and the small
  things (long shadows, drops, flies, motes, curtains, the town clock) all
  follow the setting, so every older script's shots are unchanged; the
  pass is skipped entirely when its parameters are neutral.
- 2026-11-15, P12: others' footsteps play at 0.6 of the player's; a
  survivor's feet are the asphalt walk or run whatever the ground (the
  dead's steps go by their kind: the Brute's heavy step, the Crawler's
  drag). The radio's tune is a layer, 0.7 indoors and 0.45 outdoors (it is
  in your pack, so it is never far).
- 2026-11-15, P12: the tinnitus tone comes in within a tenth of a second
  (the other layers crossfade over 1.5 s) and follows its target down over
  the 3 s; it is not ducked itself.
- 2026-11-15, P12: door noise tiers are unchanged: a door opened while
  crouched plays the slow creak but stays the tier it was (the phase asks
  for the sound, not a quieter door).
- 2026-11-15, P12: chimney smoke from occupied homes already existed (P08,
  `updateSurvivorSmoke`), and fire smoke (P07); neither is duplicated.
  Breath puffs outdoors under 5 C use the same smoke particles.
- 2026-11-15, P12: the town clock is on the wall of the non-house building
  nearest the map's centre (the town has no town hall); it shows the game
  time with the power on and stops at the power's failure hour after.
- 2026-11-15, P12: the mood idles (shoulders down, a shiver, a hand on the
  knee) are three new rows in the rig, drawn at the same pixel scale in
  the five facings; the sheet grows from 5632 to 5696 px tall and every
  older sprite keeps its place and pixels.
- 2026-11-15, P12: the F2 panel gets a second C row ("Still by a door:
  listen"); it has room for 28.
- 2026-11-15, P12: `soundscape.txt` keeps the dead off for its six calm
  hours with a `slay 200` every half hour: the town's wanderers otherwise
  reach the house and the calm (no dead within 20 tiles for 3 game hours)
  never holds, which is the rule working, not what the script tests.

## Moved to "Later"

- 2026-11-13, P01: medical and supply caches stay in code (with ids from
  `ItemIds`), not loot classes. The phase lists it as "Later".
- 2026-11-13, P01: a bot that lives three days. After the second pass the
  normal bot's median is 1.2 days (careful 0.9, so careful is still worse
  than normal); the bar is not met. A better fighter (shove, timing, doors
  closed behind it) fits phase 3, and the pressure itself is phase 14's.

- 2026-11-14, P03: a take-down from behind on a standing zombie is rare in
  play: the dead have no field of view (they see all round), so one rarely
  stands with its back to you. P04's senses should give them a facing that
  matters (and the crouch should count).
- 2026-11-14, P03: a Loud noise (the pistol, 30 tiles) is heard only to
  about 27.6 tiles: the dead investigate when its strength (falling
  linearly to 0 at the edge) is at least 0.08, so the last stretch is
  silent. `guns.txt` checks every wanderer within 27 tiles comes. P04's
  hearing should decide whether a tier's radius is where it is heard.

- 2026-11-14, P04: zombies that play dead among corpses, and a follower
  in every mode on long runs (the phase's own "Later").
- 2026-11-14, P04: the horde's warning sounds (distant groaning, birds
  gone) and each kind's own sound wait for P12, as the phase says; the
  Crawler's scrape plays a soft footstep for now.

- 2026-11-14, P05: Loner's half trust (survivors are P08); tea, the
  radio and stew as comforts (P06); journals and a companion's company
  are hooks only (P10, P08); dependency on Spirits and Painkillers and a
  Smoker trait are the phase's own "Later".

- 2026-11-14, P06: fishing at ponds and a house generator (the phase's own
  "Later"); the radio's music in shelter (P12) and its campaign broadcast
  (P09); foraging on open Wilderness ground (`forage::Spot::Wild` is in the
  rules, unused).

- 2026-11-14, P07: the weather vane and flooded roads in a storm (the
  phase's own "Later"); summer's shrinking ponds (optional in the phase);
  distant thunder before a storm waits for P12's sounds (the darker sky
  and the cloud and wind warnings are in).

- 2026-11-14, P08: groups that move home when their district is overrun,
  and survivors at your door at night asking in (the phase's own
  "Later"); survivors climbing through windows (see above); the town map's
  "blue house by the diner" descriptions (the notebook says the building
  kind, distance and direction).

- 2026-11-14, P09: a second way out (the river) for worlds whose gate
  district burned, and what is at the end of the road (the phase's own
  "Later"). The gate's own sounds (the motor, the grinding) wait for P12;
  for now each second of the stand plays a quiet weapon-hit knock. The
  run-start card can overlap the end of a long objective step at
  1280x720 for the seconds it shows ("The gate is open. Walk out." under
  a two-grave card); left for P13's run flow.

- 2026-11-14, P10: notes left on purpose for the next life, and a family
  tree view (the phase's own "Later"); carrying a survivor's body to the
  cemetery (they are buried where they lie); a journal entry for weather
  that turned a life (the season line covers part of it).

- 2026-11-15, P11: siphoning fuel with a hose item, a second seat for
  carrying a wounded survivor home, and car radios (the phase's own
  "Later"). Also moved:
  - vehicles do not collide with each other (each is solid to walls,
    scenery, the dead and the player, not to another vehicle);
  - scenery run down is gone for the run only: a checkpoint resume or the
    next life puts the bush or bin back;
  - flats come from running down scenery alone: driving over a spike
    board (P02) does not burst a tyre yet;
  - the engine, the crank and the alarm play a loud footstep, and the
    dead battering a car, a wall hit and a run-down play the weapon-hit
    knock, until P12 gives each its own sound;
  - the companion's seat carries no one else (a wounded survivor is the
    "Later" above), and survivors never take a vehicle themselves.

- 2026-11-15, P12: OpenAL for true occlusion and a generated title tune
  (the phase's own "Later"; the title stays wind only). Also moved:
  - doors still cut between closed and open: two swing frames need a door
    sprite per facing and per state, and the door's own animation timing
    in the collision rules, more than this phase's small things should
    carry;
  - wet shows as drops on the character, not as darker clothes (the rig
    has no clothes tint to darken without a second set of sprites);
  - survivors' feet are a generic asphalt step: their positions are not
    looked up against the ground the way the player's are;
  - the settings screen for the four volumes and screen effects (P13 reads
    and writes the settings file; this phase only reads it);
  - nobody has listened to the generated sounds on speakers yet: the
    harness is silent by design, so levels, the mix and the variants are
    checked by numbers only (ranges, volumes, layer levels) and want a
    listening pass by a person.

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

- 2026-11-14, P02: night 1 at home. The bar (at most 1 of 10 bot runs dead
  in the home on night 1) is not met: 2 of 10, both on day 1's evening
  before any caller, to the ordinary dead round the house. Median 1.0 days
  (P01 1.2). Not retuned here (day one's lethality is phases 3, 4 and 14's);
  see `docs/revival_plan.md` ("Overhaul phase 2").

- 2026-11-14, P04: Stalker and Swarm differ in day of death (bot median 2.5
  against 1.5 days) but not in cause: in both the bot dies "Overrun by the
  dead" at home (7 and 8 of 10), and the Stalker's follower never struck
  last. The bot does not play to a mode (it boards up and holds whatever
  comes). Not retuned, as the phase's numbers stand; a bot that reads the
  mode, or a follower that matters more, is phase 14's to weigh. See
  `docs/revival_plan.md` ("Overhaul phase 4").

- 2026-11-14, P06: days survived against phase 5. The bar ("rises") is
  met on the mean (1.9 against 1.6) and on the longest lives (four reach
  day 4 or 5), not on the median (1.6 against 1.8). No life in either
  phase died of thirst, so "deaths from thirst after day 3 fall" cannot
  be read. Two leads for phase 14:
  - Seed 9 died at home on night 1 (phase 5: none of ten): it searched 7
    containers, killed 3 and put up no boards before the dead came in.
    Not looked into.
  - Boards come down and are not put back: on seed 1 three boards were up
    at 17:00 on day 1 and none by 23:00, and the bot did not board again.
  See `docs/revival_plan.md` ("Overhaul phase 6").

- 2026-11-14, P08: the bot lives shorter with the town's people in it:
  median 1.3 days, mean 1.5 (phase 7: 1.7, 2.0); the same build with the
  survivors taken away (`survivors gone`) gives 1.8 and 1.8. Two of ten
  lives end within the hour after one from the garage went for the bot
  (seed 9: two pistol shots from full health; seed 8: with twelve of the
  dead round it too). The player's own pistol, 55 a shot, takes a
  character from full health to dead in two shots about a second and a
  half apart, and the garage goes out for supplies across the town. Not
  retuned (lethality is phase 14's);
  a gun in a survivor's hand doing less to people than to the dead, or
  warning shots first, are the levers. See `docs/revival_plan.md`
  ("Overhaul phase 8").

- 2026-11-15, P11: the bot's week fell to median 1.7 days, mean 1.8
  (phase 10: 2.1, 2.0), kills 278 (351), boards 11 (14). Not the vehicles:
  the same build with none in town gives 1.9 and 1.8, and with cars that
  do not push the player 1.7 and 1.9, each seed swinging by days between
  the three. The dead's wander is keyed by entity and any change in how
  many entities the town makes moves every later arrival. Ten seeds are
  too few to read a change smaller than that spread; phase 14 should soak
  more seeds. See `docs/revival_plan.md` ("Overhaul phase 11").
- 2026-11-15, P11: battering is harsh against a crowd: 3 a blow from
  each of the dead round a car takes a sedan from 84 to nothing in 60 to
  90 s once the tail arrives (`vehicles.txt` sets it back), and in `perf`
  300 of the dead stop a sedan within a few tiles and batter it to Broken
  in under eleven seconds. As the phase says; whether a car should hold
  longer against a crowd is phase 14's.

## Ideas, not built

- 2026-11-15, P11: a horn, a Loud noise on purpose from the seat, to pull
  the dead off a house the way the alarm does from outside.
- 2026-11-15, P11: a dead car pushed (in neutral, Hold E) across a street
  or a doorway as a barricade the dead batter instead of the boards.
- 2026-11-15, P11: a parked car's trunk as a cache the town's survivors
  can find between lives, like the player's crates (P10).
- 2026-11-15, P12: a "listen" readout for the deaf-of-ear: while listening
  at a door, a faint direction mark at the screen's edge for each sound
  heard through it (an accessibility setting for P13).
- 2026-11-15, P12: a survivor's radio heard through their walls at night,
  a way to find an occupied house by ear.
- 2026-11-15, P12: the dead's groans shift with the weather (hoarser in
  the cold, fewer in heavy rain), so the street's sound says the season
  too.
