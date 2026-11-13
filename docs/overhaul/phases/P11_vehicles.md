# Phase 11: vehicles

**Goal.** Three vehicles that each change how the town is crossed, built as
real things in the world: they have a body, a trunk, a key or a wire, and a
cost in noise.

**The player notices:** a bicycle that makes the far districts reachable in
silence; a sedan that will not start until the third try while the street
fills; a pickup carrying four fuel cans and a wardrobe to the gate.

**Feel target.** Sitting in a dead car, crouched, hands under the dash,
counting the seconds.

## Core

### 1. A vehicle is its own body

Today the car is a sprite that follows the player's position. Replace:

- `Vehicle` component gains `kind` (Sedan, Pickup, Bicycle), `heading`
  (radians), `speed`, `steer`, `trunk` (an `Inventory` entity link),
  `started`, `keyKnown`, `alarm` (bool), `tyres` (0..4 good).
- `src/systems/VehicleSystem.h/.cpp` moves it: W/S throttle and brake or
  reverse, A/D steer (screen-relative input is turned into a heading the way
  the player's movement is, so "up" is up the screen). Simple arcade model:
  acceleration, drag, a turn rate that falls with speed, no drifting.
- Collision: the body is an oriented box tested as 3 circles along its
  length against solid tiles (walls, trees, furniture, closed doors,
  boundary wall). Hitting a wall above 120 px/s: damage to the vehicle
  (speed/20), Loud noise, the player takes speed/40. Fences and thin
  scenery (bushes, lamp posts, hydrants, bins) are run down above 150 px/s
  (the piece is removed for the run; a lamp post stops the car).
- The player is hidden while inside and takes no claw or grab; the dead
  batter the vehicle (condition damage 3 per blow) and after 20 s of
  battering by 3 or more, a window breaks and the driver can be grabbed.
- Vehicles are drawn in 8 headings (sprites from the generator: sedan,
  pickup, bicycle with rider, each 8 facings, the mirrored ones reused), with
  headlights as a cone light at night (seen by the dead from 14 tiles;
  toggle with Q, the flashlight key).

| Kind | Top speed px/s | Noise (engine) | Trunk | Fuel use per game hour driven | Seats | Count per town |
|---|---|---|---|---|---|---|
| Sedan | 250 | Loud every 0.3 s | 12 slots, 60 kg | 6 units of 30 | 2 | 3-5 |
| Pickup | 210 | Loud every 0.3 s, heard x1.2 | 20 slots, 150 kg; carries 1 movable furniture piece | 8 of 40 | 2 | 1-2 |
| Bicycle | 170 (uses stamina: 2/s; coasting free) | Whisper (surface rules apply) | none; the rider's pack | - | 1 | 4-6 |

- Where they are: fixed at world creation (driveways, the garage, the police
  lot, bicycles by houses and the school-sized building), world memory
  after: position, heading, condition, fuel, trunk contents, tyres, key
  known. A car left at the gate is at the gate next life, one stage worse
  (condition -15).

### 2. Starting one

- **Keys.** 1 car in 3 has its key in it ("The key is in it."). For the
  rest, the key is in a container of the nearest house (kitchen or bedroom;
  finding it says "Car keys. The red sedan outside."; no key items: the
  world remembers the car is unlocked for good).
- **Hot-wiring.** In the seat, hold E: 20 s base (Mechanics shortens),
  40 % to fail at the end (-10 points per Mechanics level): a failure is a
  Loud "crank" and you may try again. Success is permanent for that car.
- **Starting.** Condition under 50: each start has (50 - condition) x 2 % to
  fail (a Loud crank; try again). The engine idling is Medium.
- **Alarms.** 1 car in 5 has a live alarm for the first 10 days of a life
  (its own battery; the town's power does not matter). Opening its door
  without the key sets it off: Loud every 1 s for 45 s. It can be used on
  purpose: E on a parked alarmed car from outside, "Set it off", then run.
- **Fuel.** Refuelling becomes E "Refuel" at the car with a Fuel Can
  selected (6 s; replaces Shift+V). There is no siphoning: fuel comes only
  from cans (Garage, hardware stock, trunks). Fuel is scarce on purpose.

### 3. Driving has consequences

- Engine noise as the table; the dead within hearing turn and follow at
  their speed, so a drive drags a tail that arrives where you park 30 to
  120 s later. Parking and going straight indoors is the mistake.
- Running the dead down: above 140 px/s a body hit takes 65 damage and is
  thrown (falls, P03); the car loses 5 condition and 35 % of its speed; a
  Brute stops the car (15 condition). Below 140 the car is blocked and they
  start battering.
- Wear: 0.5 condition per game hour driven; rain and snow x1.5; off-road
  (grass, dirt) x2 and top speed x0.7. Snow (P07): turn rate halved.
- Condition bands: Good (70+), Worn (40+: top speed x0.86, as today),
  Damaged (10+: x0.72, engine Explosive, starts fail), Broken (under 10:
  will not run; still a trunk and a place to hide).
- **Repair.** At the car with a Toolbox: 60 s, 1 Scrap Metal: +25 condition
  (Mechanics adds). Tyres: a flat (10 % when running down scenery; spike
  boards) cuts top speed 30 % each; no spare tyres: a flat stays flat. A
  Garage has a lift: repairs there give double.
- **Bicycle:** no fuel, near-silent, no protection: the dead grab you off it
  (you fall: 5 damage, the bike lies where it fell). It cannot cross
  furniture or bodies at speed (a body stops it: fall). Uphill does not
  exist; rain makes braking long.
- **Carrying furniture** (Pickup): push a movable piece (P02) against the
  tailgate and E "Load": it rides in the bed; E at the tailgate "Unload".
  This is how a wardrobe reaches the guard hut.
- **Hiding.** Crouch inside a parked car with the engine off: the dead that
  have not seen you get in do not see you (sight x0.2).

### 4. Getting in and out, and the HUD

- V stays as enter/exit (driver's door side must be free; exit puts you on
  the free side). E at the trunk opens it as a container.
- While driving, the bottom-left shows, in the bare-hands style, above the
  health lines: a thin fuel line, a thin condition line and the speed as a
  short word (Idle, Slow, Fast). No dials. The belt is hidden. The camera
  leads 3 tiles ahead along the heading and zooms out 15 % above 150 px/s.
- A companion (P08) gets in when you do and out when you do.

### 5. The dead and survivors with vehicles

- Survivors do not drive. One survivor home in a world has a car outside
  that is "theirs": taking it is theft (P08).
- Hordes (P04) ignore parked cars; a running engine turns a horde.

## Later

- Siphoning with a hose item.
- A second seat for carrying a wounded survivor home.
- Car radios.

## Files

New: `src/systems/VehicleSystem.*`, `src/world/VehicleRules.*` (start
chance, wear, bands, hot-wire: pure and tested), `tests/test_vehicles.cpp`,
`tools/harness/vehicles.txt`, `bicycle.txt`, `pickup_haul.txt`.
Changed: `Vehicle.h`, `GameVehicles.cpp` (thin: enter/exit, panels),
`GameScene.cpp` (spawn from world memory), `MapGenerator` (vehicle spots by
kind), `CollisionSystem`, `ZombieAttack.cpp` (batter vehicles),
`SaveManager` (vehicle records: kind, heading, trunk, key known, tyres),
`Hud`, `generate_sprites.py` (three vehicles, 8 headings), harness (`goto
vehicle [KIND]`, `drive DIR SECONDS`, `vehicles`, `set fuel N`, `set
condition N`), Controls panel and README.

## Acceptance

- `vehicles.txt`: enter a keyed sedan, drive a road for 20 s without
  clipping a wall, park; the tail of the dead arrives; trunk opens and
  stores; hitting a wall damages the car; `save`/`resume` and the next life
  keep position, heading, fuel, trunk. Screenshots in four headings.
- Hot-wire: fails and cranks Loud at least once in 10 tries at Mechanics 0,
  never at Mechanics 5 (unit test); a started car stays startable.
- `bicycle.txt`: crosses 40 tiles; a zombie 8 tiles from the route does not
  turn; a zombie reached grabs the rider off.
- `pickup_haul.txt`: load a wardrobe, drive, unload it into a doorway.
- Running down a Shambler at speed kills or downs it and slows the car; a
  Brute stops it.
- `perf`: 6 vehicles in the world cost nothing when parked.
