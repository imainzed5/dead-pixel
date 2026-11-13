# Content tables (authoritative)

One place for every id and every list, so phases never clash. If a phase file
and this file disagree, this file wins. Numbers here are starting values;
phase 14 tunes them.

Units: weight in kg; time in game hours unless marked "s" (real seconds);
distance in tiles (1 tile = 32 world px).

## 1. Items

`assets/data/items.json`. Phase 1 adds a `key` to every item (the name used
in code and in other data files) and the fields in section 1.3. Ids 0 to 29
exist today and keep their meaning for ever.

### 1.1 Existing items and their keys

| Id | Key | Name | Notes on changes |
|---|---|---|---|
| 0 | `canned_food` | Canned Food | Eating one leaves a Tin Can (P02) |
| 1 | `apple` | Apple | |
| 2 | `bread` | Bread | |
| 3 | `water_bottle` | Water Bottle | |
| 4 | `bandage` | Bandage | |
| 5 | `knife` | Knife | |
| 6 | `baseball_bat` | Baseball Bat | |
| 7 | `pipe` | Pipe | |
| 8 | `axe` | Axe | Also a tool: boards doors (already), breaks furniture fast (P02) |
| 9 | `wood_planks` | Wood Planks | |
| 10 | `scrap_metal` | Scrap Metal | |
| 11 | `rope` | Rope | |
| 12 | `fuel_can` | Fuel Can | |
| 13 | `glass_shards` | Glass Shards | Dropped by a broken window (P02) |
| 14 | `pistol_ammo` | Pistol Ammo | Gets a gun and loot sources in P03 |
| 15 | `antibiotics` | Antibiotics | |
| 16 | `painkillers` | Painkillers | |
| 17 | `hammer` | Hammer | |
| 18 | `flashlight` | Flashlight | Needs a Battery from P06 on (starts with one inside) |
| 19 | `matches` | Matches | |
| 20 | `jacket` | Jacket | Warmth 5 |
| 21 | `rat_meat` | Raw Meat | Renamed; caught by a Snare (P06); raw gives illness |
| 22 | `first_aid_kit` | First Aid Kit | |
| 23 | `notebook` | Notebook | Also carries clues and journal pages (P09, P10) |
| 24 | `disinfected_bandage` | Disinfected Bandage | |
| 25 | `soaked_rag` | Soaked Rag | |
| 26 | `molotov` | Molotov | Thrown with the throw verb from P03 |
| 27 | `spiked_bat` | Spiked Bat | |
| 28 | `spear` | Spear | |
| 29 | `empty_bottle` | Empty Bottle | |

### 1.2 New items

| Id | Key | Name | Category | Wt | Stack | Phase | What it is for |
|---|---|---|---|---|---|---|---|
| 30 | `nails` | Nails | material | 0.02 | 50 | P02 | Boards (2 per board), spike board, repairs |
| 31 | `crowbar` | Crowbar | weapon (blunt) | 1.5 | 1 | P02 | Pries boards and breaks furniture without waste; forces locked doors (P06) |
| 32 | `saw` | Saw | material (tool) | 0.8 | 1 | P02 | Breaking furniture gives one more plank; needed for the workbench and crate |
| 33 | `tin_can` | Tin Can | material | 0.05 | 10 | P02 | Can alarm, can lantern; left by Canned Food |
| 34 | `wire` | Wire | material | 0.1 | 10 | P02 | Can alarm, snare |
| 35 | `rags` | Rags | material | 0.05 | 10 | P02 | From tearing a Sheet-type find or a Jacket; dressings, soaked rag |
| 36 | `candle` | Candle | material | 0.1 | 5 | P02 | A small quiet light: burns 6 h placed |
| 37 | `rock` | Rock | weapon (thrown) | 0.3 | 5 | P03 | Thrown: Medium noise where it lands, 6 damage |
| 38 | `alarm_clock` | Alarm Clock | material | 0.3 | 1 | P03 | Thrown or set down: after 8 s rings Loud for 20 s |
| 39 | `flare` | Flare | weapon (thrown) | 0.2 | 3 | P03 | Burns 90 s: light radius 6, Medium noise each 2 s |
| 40 | `pistol` | Pistol | weapon (ranged) | 0.9 | 1 | P03 | Uses `pistol_ammo`, 8 in the magazine |
| 41 | `shotgun` | Shotgun | weapon (ranged) | 3.2 | 1 | P03 | Uses `shotgun_shells`, 2 loaded, cone of 5 pellets |
| 42 | `shotgun_shells` | Shotgun Shells | material | 0.05 | 12 | P03 | |
| 43 | `machete` | Machete | weapon (bladed) | 0.9 | 1 | P03 | Long blade |
| 44 | `sledgehammer` | Sledgehammer | weapon (blunt) | 4.5 | 1 | P03 | Heavy: slow, knocks down |
| 45 | `chair_leg` | Chair Leg | weapon (improvised) | 0.6 | 1 | P03 | From breaking a chair or table |
| 46 | `frying_pan` | Frying Pan | weapon (improvised) | 1.0 | 1 | P03 | Also counts as a Cooking Pot for single portions (P06) |
| 47 | `splint` | Splint | medical | 0.3 | 2 | P05 | Sets a fracture |
| 48 | `suture_kit` | Suture Kit | medical | 0.1 | 3 | P05 | Closes a deep cut |
| 49 | `needle_and_thread` | Needle and Thread | material | 0.05 | 3 | P05 | Makes a Suture Kit |
| 50 | `spirits` | Bottle of Spirits | drink | 0.7 | 2 | P05 | Disinfects, calms, burns; worsens sleep |
| 51 | `coffee` | Coffee | drink | 0.2 | 5 | P05 | Hides tiredness for 4 h |
| 52 | `book` | Book | material | 0.4 | 3 | P05 | Read in shelter: depression relief |
| 53 | `keepsake` | Keepsake | material | 0.1 | 1 | P05 | Rare; one strong lift, once |
| 54 | `herbal_tea` | Herbal Tea | drink | 0.4 | 3 | P06 | Warmth and calm |
| 55 | `cooked_meat` | Cooked Meat | food | 0.3 | 5 | P06 | |
| 56 | `berries` | Berries | food | 0.1 | 10 | P06 | Foraged |
| 57 | `mushrooms` | Mushrooms | food | 0.1 | 10 | P06 | Foraged; one kind in four is bad |
| 58 | `herbs` | Herbs | material | 0.05 | 10 | P06 | Foraged; tea |
| 59 | `dirty_water` | Dirty Water | drink | 0.4 | 3 | P06 | Drinkable at a price; boil or treat it |
| 60 | `cooking_pot` | Cooking Pot | material (tool) | 1.2 | 1 | P06 | Boiling, rice, stew |
| 61 | `rice` | Rice | food | 0.5 | 5 | P06 | Must be cooked |
| 62 | `cooked_rice` | Cooked Rice | food | 0.5 | 5 | P06 | |
| 63 | `chips` | Chips | food | 0.1 | 5 | P06 | Small food, small comfort |
| 64 | `soda` | Soda | drink | 0.4 | 5 | P06 | |
| 65 | `chocolate` | Chocolate | food | 0.1 | 5 | P06 | Comfort |
| 66 | `battery` | Battery | material | 0.1 | 6 | P06 | Flashlight (8 h of light each), Radio |
| 67 | `radio` | Radio | material | 0.6 | 1 | P06 | Weather a day ahead; the broadcast (P09) |
| 68 | `backpack` | Backpack | material | 0.8 | 1 | P06 | Carried: +10 kg carry limit (one counts) |
| 69 | `duct_tape` | Duct Tape | material | 0.2 | 3 | P06 | Mends a weapon 30 %; seals a window crack |
| 70 | `snare` | Snare | material | 0.3 | 3 | P06 | Set outdoors on grass; may catch Raw Meat |
| 71 | `seed_packet` | Seed Packet | material | 0.05 | 5 | P06 | Planted in a garden bed |
| 72 | `vegetables` | Vegetables | food | 0.3 | 8 | P06 | Grown |
| 73 | `winter_coat` | Winter Coat | material | 1.6 | 1 | P07 | Warmth 11 |
| 74 | `raincoat` | Raincoat | material | 0.6 | 1 | P07 | Rain does not chill you |
| 75 | `gate_key` | Gate Key | material | 0.05 | 1 | P09 | Campaign |
| 76 | `gate_fuse` | Gate Fuse | material | 0.4 | 1 | P09 | Campaign |
| 77 | `toolbox` | Toolbox | material (tool) | 3.0 | 1 | P06 | Counts as Hammer and Saw; frees the gate generator (P09); vehicle repair (P11) |
| 78 | `stew` | Stew | food | 0.6 | 3 | P06 | The best meal |
| 79 | `purification_tablets` | Purification Tablets | medical | 0.05 | 10 | P06 | Cleans one Dirty Water |
| 81 | `can_alarm` | Can Alarm | material | 0.3 | 3 | P02 | Hung on a door or window |
| 82 | `spike_board` | Spike Board | material | 1.2 | 2 | P02 | Laid on a floor tile |
| 83 | `can_lantern` | Can Lantern | material | 0.2 | 2 | P02 | A candle in a can: light the dead cannot see from afar |
| 84 | `glass_shiv` | Glass Shiv | weapon (bladed) | 0.2 | 1 | P03 | Weak, quick, breaks soon |

Id 80 is left free on purpose.

### 1.3 Item fields (schema after phase 1)

Existing fields stay. New optional fields, all with defaults:

| Field | Type | Meaning |
|---|---|---|
| `key` | string | Unique, snake case. Required after phase 1. |
| `tags` | string list | `tool:hammer`, `tool:saw`, `tool:pry`, `tool:pot`, `fuel`, `light`, `warm`, `comfort`, `placeable`, `throwable`, `ammo:pistol`, `ammo:shotgun`, `campaign` |
| `leaves` | string | Key of the item left when one is used up (`canned_food` leaves `tin_can`; `water_bottle` leaves `empty_bottle`) |
| `warmth` | number | Degrees of clothing insulation while carried; the best single item counts |
| `depressionRelief` | number | Applied on use |
| `sleepDebtMask` | number | Hours the item hides tiredness |
| `rawIllness` | number | Illness added when eaten uncooked |
| `weapon` | object | Overrides for the class defaults: `arc`, `range`, `damage`, `stamina`, `knockback`, `durability`, `windup`, `recovery`, `magazine`, `ammo`, `pellets`, `noise` |
| `throw` | object | `range` (tiles), `damage`, `noiseTier`, `noiseSeconds`, `effect` (`none`, `fire`, `ring`, `flare`) |

The existing per-item scales (`damageScale`, `rangeScale`, `durabilityScale`)
keep working; `weapon` overrides win where both are given.

## 2. Weapons (P03)

Class defaults stay in `makeWeaponStats`. Per-item values:

| Item | Class | Arc | Range px | Damage | Stamina | Knockback px | Durability | Wind-up s | Recovery s | Special |
|---|---|---|---|---|---|---|---|---|---|---|
| Bare hands | unarmed | 45 | 26 | 8 | 8 | 16 | none | 0.14 | 0.22 | Shove is free of wear |
| Chair Leg | improvised | 90 | 44 | 20 | 12 | 22 | 8 | 0.18 | 0.28 | |
| Frying Pan | improvised | 80 | 40 | 24 | 14 | 30 | 14 | 0.20 | 0.30 | Loud hit: Loud noise in place of Medium |
| Glass Shiv | bladed | 40 | 28 | 26 | 8 | 6 | 4 | 0.12 | 0.20 | Bleeds |
| Knife | bladed | 60 | 32 | 42 | 12 | 12 | 12 | 0.16 | 0.24 | Bleeds; quiet kill from behind |
| Machete | bladed | 80 | 46 | 48 | 16 | 16 | 16 | 0.20 | 0.30 | Bleeds; can stick |
| Axe | bladed | 80 | 44 | 55 | 22 | 30 | 18 | 0.28 | 0.40 | Bleeds; can stick; tool |
| Spear | bladed | 30 | 64 | 34 | 14 | 20 | 10 | 0.20 | 0.34 | Reach: hits before a lunge lands |
| Pipe | blunt | 100 | 46 | 30 | 18 | 44 | 16 | 0.22 | 0.32 | |
| Baseball Bat | blunt | 120 | 48 | 32 | 20 | 48 | 14 | 0.22 | 0.32 | |
| Spiked Bat | blunt | 120 | 48 | 43 | 20 | 48 | 21 | 0.22 | 0.32 | Bleeds |
| Crowbar | blunt | 90 | 46 | 34 | 20 | 40 | 30 | 0.24 | 0.34 | Tool |
| Hammer | blunt | 70 | 34 | 28 | 14 | 26 | 24 | 0.18 | 0.28 | Tool |
| Sledgehammer | blunt | 110 | 52 | 70 | 38 | 80 | 20 | 0.42 | 0.60 | Knocks down on any hit |
| Pistol | ranged | 6 | 12 tiles | 55 | 3 | 10 | 200 | 0.10 | 0.25 | Loud, 30 tiles |
| Shotgun | ranged | 24 | 7 tiles | 5 x 30 | 6 | 40 | 120 | 0.18 | 0.70 | Explosive, 60 tiles |

Today's wind-up 0.20 s, active 0.10 s and recovery 0.30 s are the defaults
when an item gives none. Existing `damageScale` values for Spiked Bat and
Spear are replaced by the rows above.

## 3. Recipes

`assets/data/recipes.json`. Phase 1 changes ids to keys and adds:
`station` (`none`, `workbench`, `fire`), `skill` and `skillLevel` (P05),
`inputB` may be absent (one input), `tools` by tag (`tool:hammer`).

| Out | Inputs | Tools | Station | Time s | Known at start | Phase | Clues (A / B) |
|---|---|---|---|---|---|---|---|
| Disinfected Bandage | Bandage + Antibiotics | | none | 6 | no | exists | |
| Disinfected Bandage | Bandage + Bottle of Spirits | | none | 6 | no | P05 | "something clean" / "something strong" |
| Soaked Rag | Bandage + Fuel Can | | none | 4 | no | exists | |
| Soaked Rag | Rags + Bottle of Spirits | | none | 4 | no | P05 | "cloth" / "something that burns" |
| Molotov | **Empty Bottle** + Soaked Rag | Matches | none | 6 | no | P01 (was Water Bottle; migrate known recipes) | |
| Spiked Bat | Baseball Bat + Scrap Metal | hammer | workbench | 10 | no | exists | |
| Spiked Bat | Baseball Bat + Nails (6) | hammer | none | 10 | no | P03 | "something to swing" / "something sharp and small" |
| Spear | Knife + Wood Planks | Rope | none | 8 | no | exists | |
| Glass Shiv | Glass Shards + Rags | | none | 4 | no | P03 | "something sharp" / "something to hold it by" |
| Spike Board | Wood Planks + Nails (8) | hammer | none | 8 | yes | P02 | |
| Can Alarm | Tin Can (3) + Wire | | none | 5 | yes | P02 | |
| Can Lantern | Candle + Tin Can | | none | 3 | no | P02 | "a small flame" / "something to keep the wind off" |
| Rags (2) | Jacket | Knife | none | 4 | yes | P02 | one input |
| Bandage | Rags + Water Bottle | | fire | 6 | no | P05 | "cloth" / "something to wash it in" |
| Splint | Wood Planks + Rags | | none | 6 | no | P05 | "something stiff" / "something to bind it" |
| Suture Kit | Needle and Thread + Bottle of Spirits | | none | 5 | no | P05 | "something to sew with" / "something to clean it" |
| Water Bottle | Dirty Water | pot | fire | 10 | yes | P06 | one input |
| Water Bottle | Dirty Water + Purification Tablets | | none | 2 | yes | P06 | |
| Cooked Meat | Raw Meat | | fire | 8 | yes | P06 | one input |
| Cooked Rice | Rice + Water Bottle | pot | fire | 12 | yes | P06 | |
| Stew (2) | Cooked Meat + Vegetables | pot | fire | 14 | no | P06 | "meat" / "something from the ground" |
| Herbal Tea | Herbs + Water Bottle | pot | fire | 6 | no | P06 | "leaves" / "water" |
| Snare | Wire + Rope | | none | 6 | no | P06 | "something thin and strong" / "something to tie it down" |

A count in brackets is how many of that input one craft uses (new field
`countA`, `countB`, default 1).

## 4. Building kinds (P06)

A building's kind is chosen once from its archetype and a hash of its
position, and saved in the layout (new optional field; old layouts get it
computed the same way, so a saved town keeps its kinds).

| Kind | From archetype | Guaranteed per town | Rooms get loot class | Locked at start |
|---|---|---|---|---|
| House | Residential | all residential | by room role (as today) | 25 % |
| Grocery | Commercial | 1, then 25 % of the rest | Store = `grocery_shelf`, back room = `grocery_stock` | 20 % |
| Pharmacy | Commercial | 1 | `pharmacy_shelf`, `pharmacy_stock` | 60 % |
| Hardware | Commercial | 1 | `hardware_shelf`, `hardware_stock` | 30 % |
| Clinic | Commercial | 1 | `clinic` | 40 % |
| Police | Commercial | 1 | `police`, `armoury` (one locker) | 100 % |
| Diner | Commercial | 15 % of the rest | `diner_kitchen` | 10 % |
| Office | Commercial | the rest | `office` | 30 % |
| Garage | Industrial | 1 | `garage` | 30 % |
| Warehouse | Industrial | 40 % of the rest | `warehouse` | 50 % |
| Workshop | Industrial | the rest | `workshop` (as today) | 20 % |

Guarantees are filled in the order of the table, taking the commercial
building nearest the town centre that is not yet assigned. If a town has too
few commercial buildings, the later guarantees are skipped (log it).

## 5. Loot tables (P01 moves today's; P06 adds the rest)

`assets/data/loot.json`: for each loot class, `stockedChance` (before tone),
`rolls` (how many stacks, min and max) and weighted entries `{item, weight,
min, max}`. Today's rule (first stack at 45 % x tone supplies, second at
25 %) becomes `stockedChance 0.45`, `rolls [1, 2]` with the second roll at
0.25, for the nine existing classes, with equal weights over today's item
lists. New classes:

| Class | Stocked | Rolls | Entries (item: weight) |
|---|---|---|---|
| `fridge` | 0.45 | 1-2 | canned_food 2, apple 3, bread 3, water_bottle 3, soda 2, vegetables 1 |
| `kitchen` | 0.45 | 1-2 | canned_food 4, water_bottle 2, matches 2, knife 1, bread 2, rice 2, cooking_pot 1, frying_pan 1, tin_can 1, chips 1, coffee 1, candle 1 |
| `bathroom` | 0.45 | 1-2 | bandage 4, painkillers 3, antibiotics 1, rags 2, needle_and_thread 1 |
| `bedroom` | 0.45 | 1-2 | jacket 3, notebook 2, flashlight 2, painkillers 1, matches 1, book 3, battery 2, backpack 1, keepsake 1, rags 2, winter_coat 1 |
| `living` | 0.45 | 1-2 | notebook 2, matches 2, flashlight 1, book 4, candle 2, radio 1, battery 2, alarm_clock 2, spirits 1 |
| `grocery_shelf` | 0.50 | 1-3 | canned_food 6, water_bottle 4, bread 2, rice 3, chips 3, soda 3, chocolate 2, coffee 2 |
| `grocery_stock` | 0.60 | 2-3 | canned_food 6, water_bottle 5, rice 4, soda 2, seed_packet 1 |
| `pharmacy_shelf` | 0.50 | 1-2 | bandage 5, painkillers 4, antibiotics 2, purification_tablets 2, first_aid_kit 1 |
| `pharmacy_stock` | 0.65 | 2-3 | antibiotics 4, first_aid_kit 2, suture_kit 2, splint 2, disinfected_bandage 2 |
| `hardware_shelf` | 0.50 | 1-3 | nails 6, wood_planks 3, rope 3, wire 3, duct_tape 3, hammer 2, saw 1, candle 2, battery 2, seed_packet 2 |
| `hardware_stock` | 0.60 | 2-3 | nails 5, wood_planks 5, scrap_metal 3, crowbar 2, axe 1, saw 2, fuel_can 1, toolbox 1 |
| `clinic` | 0.55 | 1-3 | bandage 4, antibiotics 3, suture_kit 3, splint 3, painkillers 3, first_aid_kit 2, spirits 1 |
| `police` | 0.40 | 1-2 | pistol_ammo 4, flashlight 2, battery 2, radio 1, first_aid_kit 1, flare 3 |
| `armoury` | 1.00 | 2-3 | pistol 3, pistol_ammo 5, shotgun 1, shotgun_shells 3 |
| `diner_kitchen` | 0.50 | 1-3 | canned_food 4, rice 3, cooking_pot 2, frying_pan 2, knife 2, coffee 3, soda 2, matches 1 |
| `office` | 0.35 | 1-2 | notebook 4, book 2, coffee 2, battery 1, painkillers 1, keepsake 1 |
| `garage` | 0.55 | 1-3 | fuel_can 4, scrap_metal 4, toolbox 1, crowbar 2, pipe 3, wire 2, duct_tape 2, battery 2 |
| `warehouse` | 0.50 | 1-3 | wood_planks 5, scrap_metal 4, rope 3, nails 3, canned_food 2, fuel_can 1, sledgehammer 1 |
| `workshop` | 0.45 | 1-2 | hammer 3, scrap_metal 3, rope 2, wood_planks 3, pipe 2, axe 1, nails 4, saw 2, wire 2 |
| `storage` | 0.45 | 1-2 | wood_planks 3, scrap_metal 3, rope 2, fuel_can 2, glass_shards 1, nails 2, tin_can 2 |
| `checkout` | 0.45 | 1-2 | notebook 1, matches 3, flashlight 1, painkillers 2, chocolate 3, battery 3, soda 2 |
| `trunk` (vehicles) | 0.60 | 1-2 | fuel_can 4, water_bottle 3, bandage 2, toolbox 1, rope 2, flare 2 |
| `corpse` (bodies of the dead, P04) | 0.12 | 1 | rags 4, matches 2, painkillers 2, battery 1, keepsake 1, pistol_ammo 1, chocolate 1 |

Rare, fixed finds per town (placed once at world creation, remembered when
taken): 1 Shotgun and 2 Pistols outside the armoury (bedroom wardrobes of
houses far from the centre), 3 Radios, 4 Backpacks, 2 Sledgehammers,
2 Machetes, 6 Keepsakes.

## 6. The dead (P04)

`assets/data/zombies.json`. Speeds are multiples of the base 112 px/s (before
the stage and threat-mode scales that exist today).

| Kind | Share at start | HP | Speed | Damage | Sight tiles | Hearing | Door damage | What makes it different |
|---|---|---|---|---|---|---|---|---|
| Shambler | 70 % | 30-70 | 0.8-1.2 | 7 | 8 | x1 | x1 | Today's zombie |
| Runner | 6 %, +1.5 % per day to 18 % | 25-40 | 1.7 | 6 | 9 | x1 | x0.5 | Sprints when it sees you; wind-up 0.40 s; tires after 8 s and slows to 1.0 for 4 s |
| Brute | 6 %, +1 % per day to 14 % | 150-190 | 0.6 | 14 | 7 | x0.7 | x3 | Not staggered by light blows (knockback under 30 px); its blow knocks you down for 0.8 s |
| Crawler | 10 % | 20-35 | 0.35 | 5 | 5 | x1 | x0 | Low: hidden by furniture and grass; bites legs (slows you 25 % for 20 s); cannot be shoved; not seen on the minimap |
| Screamer | 4 %, +0.5 % per day to 8 % | 30-45 | 0.9 | 4 | 10 | x1.5 | x0.5 | On seeing you: screams for 1.2 s (Loud, 30 tiles), then keeps distance and screams every 10 s |
| Risen (legacy) | by graves | 140 | 0.93 | 9 | 8 | x1 | x1 | Exists; keeps its leash |

Decomposition stages keep today's speed and damage scales and gain: Bloated
dies with a burst (1.5 tile radius: illness +8, panic +10 if within it);
Decayed in groups of 4 or more within 3 tiles raise illness +1 per game hour
nearby; Skeletal can be pushed through (no body blocking).

## 7. Skills (P05)

Levels 0 to 5. XP for the next level: 100, 250, 500, 900, 1500.

| Skill | Earns XP from | Each level gives |
|---|---|---|
| Carpentry | Boarding (10), breaking furniture (8), building (20), repairs (6) | Board and build time -8 %; board strength +6 %; level 2: salvages nails; level 3: reinforced boards (x1.5 hp, 2 planks); level 4: breaking furniture gives +1 plank |
| Fighting | A blow landed (2), a kill (8), a shove that saves a hit (4) | Swing stamina -5 %; wind-up -4 %; level 2: shove knocks down the weakened; level 3: weapon wear -20 %; level 5: one blow in ten does double |
| Fitness | Running (1 per 20 tiles), carrying over 20 kg (1 per game hour) | Stamina +6; run speed +2 %; carry limit +1.5 kg |
| Scavenging | Searching a container (3), finding a rare (15) | Search time -10 %; level 2: sees "worth searching" on unsearched containers within 3 tiles; level 3: 10 % chance of one more item; level 5: 25 % |
| Medicine | Treating a wound (10), a splint or stitches (20) | Treatment time -10 %; healing +8 %; level 2: sees infection before it sets; level 4: antibiotics work at half dose (one in two is not used up) |
| Mechanics | Hot-wiring (25), repairs (15), refuelling (3), traps set (6) | Hot-wire time -15 % and failure -10 points; repairs +10 %; level 3: fixes a broken generator |
| Living off the land | Foraging (5), cooking (4), a snare catch (12), a harvest (15) | Forage yield +10 %; level 2: tells bad mushrooms apart; level 3: snares twice as likely; level 4: crops grow 25 % faster |

Skills not used for 3 game days lose 2 XP per game hour down to the floor of
their current level (levels never drop).

## 8. Traits (P05)

Character creation: 0 points to start; positives cost, negatives give; the
total must be 0 or more. At most 3 of each. Today's six traits are kept.

| Positive | Cost | Effect |
|---|---|---|
| Tough | 3 | +20 max health (fixed in P01) |
| Quick | 3 | +15 % move speed |
| Light Sleeper | 1 | Wakes at 1.5x the range |
| Handy | 2 | Carpentry starts at 2 |
| Brawler | 2 | Fighting starts at 2 |
| Nurse | 2 | Medicine starts at 2 |
| Outdoorsy | 2 | Living off the land starts at 2; rain chills half as much |
| Steady | 2 | Panic gain -25 % |
| Strong Back | 2 | Carry limit +6 kg |
| Night Owl | 1 | Sees 20 % further in the dark; sleep debt builds 10 % slower |

| Negative | Gives | Effect |
|---|---|---|
| Clumsy | 2 | Footsteps one tier louder |
| Hungry | 2 | Hunger +50 % |
| Nervous | 2 | Panic gain +30 % |
| Frail | 3 | -20 max health |
| Heavy Sleeper | 1 | Wakes at 0.6x the range |
| Thin-Skinned | 2 | Cold and heat bite 30 % harder |
| Squeamish | 1 | Each kill adds 2 panic; desensitisation builds at half speed |
| Slow Healer | 2 | Wounds take 50 % longer |
| Loner | 1 | Company gives half the relief; survivors trust slower |
| Weak Stomach | 1 | Spoiled or raw food makes twice the illness |

Bloodline traits (Hardened, Scavenger, Firewise, Vigilant) stay and are joined
by the skill echoes of phase 10.

## 9. Structures the player makes (P02, P06)

| Thing | Made from | Tool | Time s | Health | Where | Carried to the next life |
|---|---|---|---|---|---|---|
| Board on a door | 1 Plank + 2 Nails | hammer | 4 | 450 | closed door, up to 3 | yes (already) |
| Board on a window | 1 Plank + 2 Nails | hammer | 4 | 300 | window, up to 2 | yes |
| Furniture against a door | a movable piece | none | push | piece's health | inside the doorway tile | as furniture moved (yes) |
| Can Alarm | item | none | 2 | - | door or window | yes if not sprung |
| Spike Board | item | none | 2 | 3 uses | indoor floor tile | yes, uses left |
| Campfire | 2 Planks + Matches | none | 5 | 60 | outdoors or on a hard floor | no (ashes mark remains) |
| Workbench | 4 Planks + 12 Nails | hammer, saw | 20 | 150 | indoors | yes, one stage worse |
| Storage Crate | 3 Planks + 8 Nails | hammer, saw | 12 | 150 | indoors | yes, with its contents |
| Rain Collector | 2 Scrap Metal + 1 Plank | hammer | 12 | 100 | outdoors | yes, one stage worse |
| Bedroll | 2 Rags + 1 Rope | none | 6 | 80 | indoors | no |
| Garden Bed (P06) | 2 Planks + 1 Seed Packet | none | 15 | - | outdoors on grass or dirt | yes, crops reset to seedlings |
| Candle / Can Lantern | item | Matches | 1 | - | any floor | no |

"One stage worse": Intact becomes Damaged, Damaged becomes Breaking, Breaking
is gone (the design document's rule).

## 10. Noise sources (reference for P03, P04, P12)

Tier radii today: Whisper 3, Soft 4, Medium 15, Loud 30, Explosive 60 tiles.
Phase 4 changes Soft to 7 and adds walls, doors and weather.

| Source | Tier | Seconds |
|---|---|---|
| Crouch-walk / walk / run | Whisper / Soft / Medium (surface shifts as today) | 0.35-0.55 |
| Swing (any) | Medium | 0.45 |
| Shove | Soft | 0.35 |
| Frying pan hit | Loud | 0.6 |
| Door opened / closed gently (crouched) | Soft / Whisper | 0.4 |
| Door battered, board hammered | Loud | 0.7 |
| Window broken | Loud | 0.7 |
| Furniture pushed / broken up | Medium / Loud | 0.6 |
| Pistol / Shotgun | Loud / Explosive | 0.8 / 0.9 |
| Rock lands | Medium | 0.5 |
| Alarm clock, can alarm, car alarm | Loud, repeating | 20 / 3 / 45 |
| Flare | Medium every 2 s | 90 |
| Scream (player, Screamer, survivor) | Loud | 0.7-1.2 |
| Zombie groan (chasing) | Soft every 3-5 s | 0.4 |
| Survivor walking / fighting | as the player | |
| Generator (P09 gate) | Explosive every 1 s | 180 |
