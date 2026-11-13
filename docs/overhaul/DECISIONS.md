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

## Ideas, not built

(none yet)
