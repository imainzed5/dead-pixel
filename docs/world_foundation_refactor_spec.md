# World Foundation Refactor — Implementation Spec

**Version:** 1.0
**Created:** 2026-04-11
**Milestone:** M1

---

## Problem Statement

The current implementation uses `worldSeed ^ runNumber` as the seed for **both** map layout generation and entity spawning. This means every new run produces a completely different town layout, making graves, structures, and all persistent world references meaningless — they reference coordinates in a geography that no longer exists.

The GDD requires a **fixed world per save slot**: the town layout is generated once and reused forever. Only run-variant elements (loot, zombies, weather) change between runs. Legacy markers (graves, retirement markers, structures) must appear at stable coordinates on a stable map.

---

## Architecture Changes

### 1. Save Model — Four Layers

| Layer | File | Created | Survives Death | Survives Retirement |
|---|---|---|---|---|
| Slot Meta | `{slot}.json` | Slot creation | Yes | Yes |
| World Layout | `{slot}_layout.json` | Slot creation (once) | Yes | Yes |
| Persistent World State | `{slot}_legacy.json`, `{slot}_structures.json` | First death/build | Yes | Yes |
| Run-State Checkpoint | Future (M2) | During run | **No** (deleted on death) | **No** |

**Slot Meta** gains a `saveVersion` field. Version 0 (missing field) is legacy-incompatible; the game creates a fresh slot.

**World Layout** stores tile layers, building rects, door positions, district definitions, and the canonical player start position. Created once at slot creation using `worldSeed`. Never regenerated.

### 2. Map Generation — Split into Layout + Spawns

`MapGenerator::generate()` is replaced by:
- `generateLayout(layoutSeed)` — Produces roads, buildings, doors, walls, decor patches, district assignments. Output: `LayoutData` struct.
- `generateSpawns(runSeed)` — Produces zombie, loot, container, and decor entity spawn points against the stable layout. Output: `SpawnData` struct.

### 3. District Model — Lightweight First Pass

Each building becomes a district (ID 1+). All non-building tiles are district 0 (Wilderness).

```
DistrictType:
  Wilderness = 0  (outdoor, roads)
  Residential = 1 (small buildings, carpet floors)
  Commercial = 2  (medium buildings, mixed floors)
  Industrial = 3  (large buildings)
```

District data: type, tile bounds, building indices, adjacency list.
Adjacency: two districts are adjacent if their bounding boxes are within 5 tiles.

### 4. Legacy Separation — Death vs. Retirement

`GraveRecord` remains for deaths. A new `RetirementRecord` struct tracks retirements separately with different data (no "cause of death"). The legacy file stores both arrays. Retirement markers render differently from graves in-game.

### 5. TileMap — District Layer

TileMap gains a fourth layer (`mDistrictLayer`) storing per-tile district IDs, plus a `getDistrictId(tileX, tileY)` query.

---

## File-by-File Changes

### `src/map/LayoutData.h` (NEW)
- `DistrictType` enum
- `LayoutData` struct with tile layers, buildings, doors, districts, playerStart

### `src/map/MapGenerator.h`
- Remove `generate()`. Add `generateLayout()`, `loadLayout()`, `applyToTileMap()`, `generateSpawns()`.
- Store `LayoutData mLayout` as member.

### `src/map/MapGenerator.cpp`
- `generateLayout()`: calls fillBase → borderWalls → carveRoads → placeBuildings → addInteriorWalls → scatterDecor → assignDistricts. Packs results into LayoutData.
- `loadLayout()`: restores internal state from saved LayoutData.
- `applyToTileMap()`: sets tile layers and tileset on a TileMap.
- `generateSpawns()`: re-seeds RNG with runSeed, calls selectSpawns.
- New `assignDistricts()`: assigns district IDs to tiles and builds DistrictDef list.

### `src/map/TileMap.h`
- Add `mDistrictLayer`, `setDistrictLayer()`, `getDistrictId()`.

### `src/map/TileMap.cpp`
- Implement district layer storage and query.

### `src/save/SaveManager.h`
- Add `RetirementRecord` struct.
- Add `saveWorldLayout()`, `loadWorldLayout()`, `hasWorldLayout()`.
- Add `recordRetirement()` separate from `recordDeath()`.
- Add `retirements()` accessor.
- Add save version constant and version field.

### `src/save/SaveManager.cpp`
- Layout serialization/deserialization (JSON).
- Version checking in `loadMeta()` — reject version 0.
- `recordRetirement()` implementation.
- Layout file path management.

### `src/core/Game.h`
- Add `MapGenerator mMapGenerator` as persistent member.
- Remove local MapGenerator creation in initialize/startNewRun.

### `src/core/Game.cpp`
- `initialize()`: Reorder — load save slot first, then load-or-create layout, then load texture from tileset info.
- `startNewRun()`: Remove map regeneration. Only call `generateSpawns()` + `setupScene()`.
- `beginRetirementState()`: Call `recordRetirement()` instead of `recordDeath()`.

---

## Verification Criteria

1. **Layout stability:** Create a slot, start run 1, note building positions. Die. Start run 2. All buildings, roads, walls, doors are identical.
2. **Spawn variance:** Zombie and loot positions differ between runs in the same slot.
3. **Grave placement:** Die in run 1 at position X. In run 2, grave appears at position X on the same map.
4. **Structure persistence:** Build a barricade. Die. Next run, barricade exists at same coordinates, degraded.
5. **Retirement separation:** Retire a character. Legacy file shows a retirement record, not a grave.
6. **Save version:** Delete slot files. Start fresh. Slot meta contains `saveVersion: 1`.
7. **Legacy rejection:** Load an old save without `saveVersion`. Game creates a fresh slot instead of crashing.
8. **District assignment:** Every building tile has a non-zero district ID. Outdoor tiles are district 0.
