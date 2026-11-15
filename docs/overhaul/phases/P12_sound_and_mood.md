# Phase 12: sound and mood

**Goal.** In a game where sound is the threat, sound must be the main way
the player reads the world. Then the screen should carry the character's
state without numbers.

**The player notices:** knowing a Brute is near before seeing it; rain on
the roof from inside; a street that sounds empty and one that does not; a
heartbeat; a single low note when the sun goes.

**Feel target.** Playing with eyes half shut for ten seconds and knowing
roughly what is around you.

All audio is synthesised by `tools/generate_sounds.py` (extend it; keep it
deterministic with fixed seeds) into 16-bit mono WAVs. No recorded or
downloaded audio.

## Core

### 1. The audio engine

`AudioManager` today: 16 channels, pan only. Extend:

- **Distance and direction.** `playAt(SoundId, worldPos, baseVolume)`:
  volume falls linearly from full at 2 tiles to silent at the sound's
  audible range (per sound, in tiles; ties to its noise tier: Whisper 4,
  Soft 9, Medium 18, Loud 32, Explosive 60); pan from screen x. Use
  `Mix_SetPosition`.
- **Occlusion, cheaply.** A sound from another building, or from outdoors
  while you are indoors (or the reverse), with that shell sealed (phase 4's
  rule): volume x0.45 and a low-pass variant if the sound has one (generate
  a `_muffled` twin for the dozen sounds that matter: groans, banging,
  screams, gunshots, rain).
- **Voices and priority.** 24 channels. Each sound has a priority; when
  full, the quietest lowest-priority voice is dropped. Limit identical
  sounds to 3 at once (groans, footsteps).
- **Variation.** Every one-shot has 3 to 5 variants and plays with pitch
  +-6 % (pre-generated variants; SDL_mixer does not pitch).
- **Loops and layers.** `setLayer(LayerId, volume)` with 1.5 s crossfades,
  for ambience and music.
- **Sound table.** `assets/data/sounds.json`: id, files, range, priority,
  volume, loop. `SoundId` enum is generated from it or kept in step by a
  test.
- Volumes: master, effects, ambience, music (settings in P13; stored in the
  slot-independent settings file).
- Harness: `--mute` launch option; a `sounds` command prints sounds played
  in the last second (so scripts can assert that a sound fired).

### 2. A sound for everything that makes noise

The rule from the design document: every source in the noise model has a
sound. Use `03_content_tables.md` section 10 as the checklist. Minimum set
(each with variants):

- **Feet:** player on grass, dirt, wood, tile, carpet, asphalt, gravel,
  metal, snow, mud; walk, run, crouch. Others' feet: softer copies.
- **Body:** breath when stamina is under 30 % (a loop that rises), a
  heartbeat loop driven by panic (rate 60 to 140), eating, drinking,
  coughing, a held breath out (the calming verb), pain grunts by damage.
- **Hands:** door open, close, locked rattle, creak (slow open while
  crouched); cupboard, fridge, drawer, locker; search rummage loop; hammer
  on board; board pried; plank splitting; furniture drag (light, heavy);
  furniture breaking; window break, glass underfoot, curtain; can alarm;
  spike board; match; fire crackle (small, large loops); water pour; tap
  running, tap sputter (day 2), tap dry; bottle fill; pack open and close;
  item pick up and drop by weight class; craft finish; bandage; splint;
  stitches; reload, dry click, jam.
- **Weapons:** swing by class (light whoosh, heavy whoosh), hit by class
  (blunt thud, blade cut, heavy crunch), wall hit, stick and pull, shove,
  stomp, weapon breaking; pistol, shotgun (each with a distant tail variant
  used beyond 15 tiles), casing; thrown object whoosh, rock landing, bottle
  smash, alarm clock ring, flare hiss.
- **The dead, by kind:** Shambler groan (idle, alerted, chasing), Runner
  shriek on the start and fast breath, Brute low growl and heavy step,
  Crawler scrape and wet drag, Screamer scream (long, distinct, the
  loudest thing they make), lunge snarl, grab, bite, body fall, door bang,
  board crack, window bang. A horde: a crowd-groan loop whose volume
  follows the number within 30 tiles, heard from the direction of the
  crowd's centre.
- **People:** a survivor's cough, a called word-shape (no words: a short
  voiced "hey" tone), a door shut hard.
- **World:** rain light, heavy, on a roof (indoors variant), thunder near
  and far, wind light and gusting, wind in trees, snow hush, blizzard,
  fog drip; birds by day (several), crows (in ruined districts), insects at
  night, a dog far off (rare, by day, in districts under 25 infection:
  never an entity); the power's hum from lamps and fridges, and its
  stopping; a car engine by kind (idle, drive), crank fail, car alarm, shop
  alarm, bicycle tick; generator (P09), gate grind, gate open.
- **Interface:** cursor move, select, back, error, tab change: tiny, dry,
  wooden clicks. Day card: one low struck note. Death: the existing sting,
  remade softer. Getting out: wind, and nothing else.

### 3. Ambience that tells the truth

An ambience mixer (`src/audio/Ambience.h/.cpp`) sets layer volumes every
half second from the world:

| Layer | Driven by |
|---|---|
| Wind | wind strength; x0.4 indoors |
| Rain / snow / storm | condition; "on the roof" variant indoors; silent in a sealed room with no windows (rare) |
| Birds | day, clear or cloudy, district infection under 50, **no dead within 12 tiles**: birds stop when something is near. This is the tell |
| Insects | night, above 8 C, the same rule |
| Crows | ruin level 2+ |
| Town hum | power on, within 6 tiles of a lamp or fridge |
| Room tone | indoors: a faint closed-in air; changes when a door or window to outside is open (outdoor layers come up) |
| Crowd | number of the dead within 30 tiles |
| Fire | burning tiles within 12 tiles |
| Tinnitus | after a gunshot of your own or a Screamer within 4 tiles: other layers duck 60 % for 3 s with a thin tone |

Hearing through the door: with the ear to a closed door (crouch within 1
tile, still for 1 s), sounds from the other side lose the occlusion cut for
as long as you stay. A small listening verb, taught by the prompt line
"Listening."

### 4. Music: almost none

- Generated in the same tool: slow pads and single notes (sine and filtered
  noise, minor and suspended chords, 60 to 90 s loops) and three short
  stings.
- Policy by tone preset (the design document): **Hopeless:** no music at
  all. **Grim Human:** a pad may fade in only when calm (no dead within 20
  tiles for 3 game hours, indoors or at dawn or dusk), at low volume, for
  one loop, at most once per 6 game hours. **Brutal Arcade:** a low pulse
  under fights of 4 or more, and the calm pad more often.
- Never during danger in Hopeless or Grim Human. Stings: dusk ("the sun is
  going"), the first night caller's first bang each night (a single low
  note under it), dawn survived.
- The Radio's music (P06): a thin, band-limited tune heard only near the
  radio.

### 5. Mood on the screen

Post-effects in `sprite.frag` or a final full-screen pass (add a simple
framebuffer if the renderer draws straight to the back buffer: render the
world and HUD to a texture, then one quad with the effects; UI stays crisp
by drawing it after the pass).

| State | On screen |
|---|---|
| Health under 40 % | Red edge (exists); under 20 % it pulses with the heartbeat |
| Panic 50+ | Vignette closes 15 to 35 %; a 1 px shake at heartbeat rate at 75+ |
| Tired (debt 70+) | Edges blur slightly; blinks (a quick close to 60 % black) every 20 to 40 s at 85+ |
| Depression 50+ | Saturation falls to 0.8, then 0.6 at 70+, 0.45 at 90+ (multiplies the tone's) |
| Cold | A blue cast at the edges; breath puffs from the character outdoors under 5 C |
| Overheated | A slow shimmer near the horizon line; warm cast |
| Wet | Drops on the character (a few dark pixels cycling); darker clothes tint |
| Coughing (smoke), fever | Slight double image at the edges for fever; grey haze for smoke |
| Held (grabbed) | Zoom in 6 %, shake (P03) |
| Listening at a door | Edges dim 10 %; sound opens up |
| Dusk and dawn | The existing tints, plus long shadows: character and scenery shadows stretch and swing with the hour (shadow sprites skewed by sun angle; night: none except by lamps) |

- All effects scale with one setting, "Screen effects: full / reduced /
  off" (P13), and are off for harness screenshots unless a script turns them
  on (`fx on`), so earlier scripts' shots do not change.
- Character animation carries state too: shoulders down and a slower idle
  when depressed, a shiver when cold, a hand on the knee when exhausted (two
  or three extra idle variants in the rig).

### 6. Small things that make it a place

- Doors swing (2 frames between closed and open) instead of cutting.
- Curtains move in wind at a broken window.
- Smoke from chimneys of occupied homes (P08) and from fires.
- Dust in light: a few motes in the flashlight cone and lamp pools indoors.
- Flies over bodies older than a game day (a few dark pixels; a faint buzz
  within 2 tiles).
- Clocks: the town-centre building has a clock on its wall showing the game
  time while the power is on, stopped at the minute it failed after.

## Later

- OpenAL for true occlusion.
- A short generated tune for the title screen (keep it silent with wind
  until then).

## Files

New: `src/audio/Ambience.*`, `assets/data/sounds.json`,
`src/rendering/PostFx.*` (if a framebuffer pass is added),
`tests/test_ambience.cpp` (layer volumes from world facts, pure),
`tests/test_sounds.cpp` (every id has files; every noise source in the
table has a sound), `tools/harness/soundscape.txt`, `mood.txt`.
Changed: `AudioManager.*`, `SoundId.h`, `generate_sounds.py` (large),
`sprite.frag`, `GameRender.cpp`, `charrig.py` (idle variants), every site
that emits a noise event gains a sound call (best done by having one
function do both: `Game::makeNoise(pos, tier, seconds, SoundId)`), harness
(`sounds`, `fx on|off`, `--mute`).

## Acceptance

- `test_sounds`: every `SoundId` resolves to files that exist; every row of
  the noise table has a `SoundId`.
- `soundscape.txt`: standing outdoors by day with no dead near, the `sounds`
  log shows the birds layer up; spawn a zombie at 8 tiles: birds drop to 0
  within 2 s; go indoors and shut the door: rain layer swaps to the roof
  variant; a door bang two buildings away plays the muffled twin.
- A gunshot ducks the ambience and plays its distant tail when heard from
  20 tiles (teleport and check the log).
- Tone Hopeless: over a scripted calm 6 game hours no music layer rises;
  Grim Human: one does, once.
- `mood.txt`: screenshots with `fx on` at panic 80, debt 90, depression 90,
  cold, held: each differs visibly from the neutral shot, and the HUD text
  stays sharp.
- The game runs with no audio device (CI, `--mute`) without errors.
- The asset folder stays small: total generated audio under 40 MB; long
  loops at 22 kHz.
