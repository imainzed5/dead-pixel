// The other people in town (overhaul phase 8): the survivors file, who lives
// where, what the brain asks of the world (doors, cupboards, boards, blows),
// meeting them (the talk, trade and give panels), companions, robbery and
// theft, and what happens to everyone between lives.
#include "core/GameShared.h"
#include "world/BuildingKinds.h"
#include "world/Storage.h"

#include <fstream>

#include <nlohmann/json.hpp>

using namespace gameshared;
using survivorrules::Attitude;
using survivorrules::Kind;
using survivorrules::Role;

namespace
{
constexpr float kHalf = 16.0f;
constexpr float kSayRangeTiles = 14.0f;
constexpr float kSaySeconds = 4.5f;
constexpr float kStockWeight = 60.0f;    // what a survivor keeps at home
constexpr float kCompanionWeight = 15.0f; // what a companion carries
constexpr float kRobTakeTiles = 1.3f;
// The ones who go for you on sight do not live this near where a life
// starts: somewhere to learn to avoid, not the neighbours on the first morning.
constexpr float kHostileNeighbourTiles = 20.0f;

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kHalf, t.y + kHalf);
}

glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

Kind itemKind(const ItemDef& def)
{
    if (def.category == ItemCategory::Weapon) return Kind::Weapon;
    if (def.category == ItemCategory::Medical) return Kind::Medicine;
    if (def.category == ItemCategory::Drink) return Kind::Drink;
    if (def.category == ItemCategory::Food) return Kind::Food;
    return Kind::Other;
}

// Something they would eat or drink without a fire or a filter.
bool edible(const ItemDef& def, const InventorySlot& slot)
{
    return def.hungerRestore > 0.0f && def.rawIllness <= 0.0f && slot.condition > 0.15f &&
           def.category != ItemCategory::Medical;
}

bool drinkable(const ItemDef& def, const InventorySlot& slot)
{
    return def.thirstRestore > 0.0f && def.illnessRelief >= 0.0f && def.key != "dirty_water" && slot.condition > 0.15f;
}

bool medicine(const ItemDef& def)
{
    return def.category == ItemCategory::Medical && (def.stopsBleeding || def.woundTreatment > 0 || def.healthRestore > 0.0f);
}

std::string replaceAll(std::string text, const std::string& key, const std::string& value)
{
    for (std::size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size()))
    {
        text.replace(at, key.size(), value);
    }
    return text;
}

std::string directionWords(const char* screen)
{
    const std::string d = screen != nullptr ? screen : "";
    if (d == "N") return "north";
    if (d == "NE") return "north-east";
    if (d == "E") return "east";
    if (d == "SE") return "south-east";
    if (d == "S") return "south";
    if (d == "SW") return "south-west";
    if (d == "W") return "west";
    if (d == "NW") return "north-west";
    return "far";
}

std::string lowerText(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> wrapText(const std::string& text, std::size_t maxChars)
{
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string word;
    std::string line;
    while (words >> word)
    {
        if (!line.empty() && line.size() + 1 + word.size() > maxChars)
        {
            lines.push_back(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

const FurniturePiece* pieceAt(const std::vector<FurniturePiece>& furniture, const glm::ivec2& tile)
{
    const auto it = std::find_if(furniture.begin(), furniture.end(), [&](const FurniturePiece& p) { return p.tile == tile; });
    return it != furniture.end() ? &*it : nullptr;
}

bool hasPair(const std::vector<std::pair<int, int>>& tiles, const glm::ivec2& tile)
{
    return std::any_of(tiles.begin(), tiles.end(), [&](const auto& p) { return p.first == tile.x && p.second == tile.y; });
}
} // namespace

// ---------------------------------------------------------------------------
// What the brain asks of the world

class Game::SurvivorHooks : public SurvivorHooksBase
{
public:
    explicit SurvivorHooks(Game& game) : g(game) {}

    Stock stock(Entity e) override
    {
        Stock out;
        if (!g.mWorld.hasComponent<Inventory>(e)) return out;
        const Inventory& inv = g.mWorld.getComponent<Inventory>(e);
        int planks = 0;
        int nails = 0;
        int rounds = 0;
        bool pistol = false;
        for (const InventorySlot& slot : inv.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = g.mItemDatabase.find(slot.itemId);
            if (def == nullptr) continue;
            out.food = out.food || edible(*def, slot);
            out.drink = out.drink || drinkable(*def, slot);
            out.medicine = out.medicine || medicine(*def);
            if (slot.itemId == g.mItems.woodPlanks) planks += slot.count;
            if (slot.itemId == g.mItems.nails) nails += slot.count;
            if (slot.itemId == g.mItems.pistolAmmo) rounds += slot.count;
            if (slot.itemId == g.mItems.pistol)
            {
                pistol = true;
                rounds += slot.loaded;
            }
            else if (def->category == ItemCategory::Weapon && def->weaponCategory != static_cast<int>(WeaponCategory::Ranged))
            {
                out.weapon = true;
            }
        }
        out.boards = planks >= 1 && nails >= 2;
        out.gun = pistol && rounds > 0;
        // Water they have at home besides (rain, a tap): only a companion,
        // away from it, lives on what is carried.
        std::vector<survivorrules::StockLine> lines = g.survivorStockLines(e);
        const bool companion = g.mWorld.hasComponent<SurvivorAI>(e) && g.mWorld.getComponent<SurvivorAI>(e).companion;
        if (!companion)
        {
            survivorrules::StockLine tap;
            tap.itemId = -2;
            tap.count = 1000;
            tap.kind = Kind::Drink;
            tap.thirst = 100.0f;
            lines.push_back(tap);
            out.drink = true;
        }
        out.days = survivorrules::stockDays(lines);
        return out;
    }

    bool consume(Entity e, bool drink) override
    {
        if (!g.mWorld.hasComponent<Inventory>(e) || !g.mWorld.hasComponent<SurvivorAI>(e)) return false;
        Inventory& inv = g.mWorld.getComponent<Inventory>(e);
        SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        int best = -1;
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            const InventorySlot& slot = inv.slots[static_cast<std::size_t>(i)];
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = g.mItemDatabase.find(slot.itemId);
            if (def == nullptr) continue;
            const bool fits = drink ? drinkable(*def, slot) : (edible(*def, slot) && def->category != ItemCategory::Drink);
            if (fits)
            {
                best = i;
                break;
            }
        }
        if (best < 0 && drink && !ai.companion)
        {
            ai.thirst = std::min(100.0f, ai.thirst + 45.0f); // the rain barrel, the tap
            return true;
        }
        if (best < 0) return false;
        const ItemDef* def = g.mItemDatabase.find(inv.slots[static_cast<std::size_t>(best)].itemId);
        ai.hunger = std::min(100.0f, ai.hunger + def->hungerRestore);
        ai.thirst = std::min(100.0f, ai.thirst + def->thirstRestore);
        InventorySystem::consumeOne(inv, g.mItemDatabase, best);
        return true;
    }

    bool tend(Entity e) override
    {
        if (!g.mWorld.hasComponent<Inventory>(e) || !g.mWorld.hasComponent<SurvivorAI>(e)) return false;
        Inventory& inv = g.mWorld.getComponent<Inventory>(e);
        SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            const InventorySlot& slot = inv.slots[static_cast<std::size_t>(i)];
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = g.mItemDatabase.find(slot.itemId);
            if (def == nullptr || !medicine(*def)) continue;
            ai.wounds = std::max(0, ai.wounds - 1);
            ai.bleedSeconds = 0.0f;
            if (g.mWorld.hasComponent<Health>(e))
            {
                Health& h = g.mWorld.getComponent<Health>(e);
                h.current = std::min(h.maximum, h.current + 12.0f + def->healthRestore);
            }
            InventorySystem::consumeOne(inv, g.mItemDatabase, i);
            return true;
        }
        return false;
    }

    // Containers a survivor may search: within reach of home, never searched,
    // in nobody's house and not the player's home.
    bool searchable(const SurvivorAI& ai, const SurvivorContainer& c) const
    {
        if (!g.mWorld.hasComponent<Inventory>(c.entity) || c.stand.x < 0) return false;
        const ScavengeMemoryRecord& memory = g.mSaveManager.scavengeMemory();
        if (hasPair(memory.searched, c.tile) || g.searchedThisRun(c.tile)) return false;
        if (c.building > 0 && (g.survivorLivesIn(c.building) || c.building == g.mFirstDays.homeBuilding ||
                               c.building == g.mSaveManager.baseRecord().building))
        {
            return false;
        }
        if (std::find(ai.skipped.begin(), ai.skipped.end(), c.stand) != ai.skipped.end()) return false;
        const FurniturePiece* piece = pieceAt(g.mFurniture, c.tile);
        if (piece != nullptr && piece->armoury) return false; // locked; they have no crowbar
        if (c.building > 0)
        {
            // A locked building is not theirs to get into (they have no keys,
            // and the way the path takes may be the locked door).
            const std::vector<glm::ivec2>& doors = g.survivorHomeDoors(c.building);
            if (std::any_of(doors.begin(), doors.end(), [&](const glm::ivec2& d) { return g.doorLocked(d); }))
            {
                return false;
            }
        }
        return glm::length(glm::vec2(c.tile - ai.homeTile)) <= survivorrules::kScavengeRadiusTiles;
    }

    Entity findContainer(Entity e, glm::ivec2& standAt) override
    {
        const SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        const glm::ivec2 here = worldToTile(g.mTileMap, centreOf(g.mWorld.getComponent<Transform>(e)));
        Entity best = kInvalidEntity;
        float bestDistance = 0.0f;
        for (const SurvivorContainer& c : g.mSurvivorContainers)
        {
            if (!searchable(ai, c)) continue;
            const float d = glm::length(glm::vec2(c.tile - here));
            if (best == kInvalidEntity || d < bestDistance)
            {
                best = c.entity;
                bestDistance = d;
                standAt = c.stand;
            }
        }
        return best;
    }

    // Food, water, medicine and a better weapon, from `from` into `to`.
    int takeUseful(Inventory& from, Inventory& to, float bestWeapon)
    {
        int moved = 0;
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            InventorySlot& slot = from.slots[static_cast<std::size_t>(i)];
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = g.mItemDatabase.find(slot.itemId);
            if (def == nullptr) continue;
            bool wanted = edible(*def, slot) || drinkable(*def, slot) || medicine(*def);
            if (!wanted && def->category == ItemCategory::Weapon && def->weaponCategory != static_cast<int>(WeaponCategory::Ranged))
            {
                const float score = weaponStatsForItem(*def, &g.mItemDatabase).damage;
                if (score > bestWeapon)
                {
                    wanted = true;
                    bestWeapon = score;
                }
            }
            if (!wanted) continue;
            moved += storage::moveItem(from, i, to, g.mItemDatabase, slot.count);
        }
        return moved;
    }

    float bestWeaponOf(const Inventory& inv) const
    {
        float best = 0.0f;
        for (const InventorySlot& slot : inv.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = g.mItemDatabase.find(slot.itemId);
            if (def != nullptr && def->category == ItemCategory::Weapon &&
                def->weaponCategory != static_cast<int>(WeaponCategory::Ranged))
            {
                best = std::max(best, weaponStatsForItem(*def, &g.mItemDatabase).damage);
            }
        }
        return best;
    }

    void search(Entity e, Entity container) override
    {
        if (!g.mWorld.hasComponent<Inventory>(container) || !g.mWorld.hasComponent<Inventory>(e)) return;
        SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        Inventory& box = g.mWorld.getComponent<Inventory>(container);
        Inventory& stock = g.mWorld.getComponent<Inventory>(e);
        int inside = 0;
        for (const InventorySlot& slot : box.slots) inside += slot.itemId >= 0 ? slot.count : 0;
        const int moved = takeUseful(box, stock, bestWeaponOf(stock));
        const Transform& t = g.mWorld.getComponent<Transform>(container);
        const glm::ivec2 tile = worldToTile(g.mTileMap, glm::vec2(t.x + 1.0f, t.y + 1.0f));
        g.recordContainer(container);
        g.markSearchedBySurvivor(tile);
        g.mNoiseModel.emitNoise(tileCentre(g.mTileMap, tile), NoiseTier::Soft, 0.6f, e);
        ai.carried.clear();
        ai.carried.emplace_back(moved, 0);
        ai.lastSearched = tile;
        ai.outDay = g.mCurrentDay; // one trip a day
        std::cout << "[Survivors] " << ai.name << " searched (" << tile.x << "," << tile.y << "): took " << moved << " of "
                  << inside << "\n";
    }

    void store(Entity e) override
    {
        SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        const int moved = ai.carried.empty() ? 0 : ai.carried.front().first;
        ai.carried.clear();
        std::cout << "[Survivors] " << ai.name << " home with " << moved << " things\n";
    }

    // The floor inside a door or window of `building`, to work from.
    bool insideOf(int building, const glm::ivec2& way, glm::ivec2& standAt) const
    {
        const glm::ivec2 sides[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const glm::ivec2& side : sides)
        {
            const glm::ivec2 in = way + side;
            if (g.mTileMap.inBounds(in.x, in.y) && g.mTileMap.getBuildingId(in.x, in.y) == building &&
                g.mTileMap.isIndoors(in.x, in.y) && !g.mTileMap.isSolid(in.x, in.y) &&
                g.mTileMap.doorState(in.x, in.y) == DoorState::None)
            {
                standAt = in;
                return true;
            }
        }
        return false;
    }

    bool openWayIn(int building, glm::ivec2& door, glm::ivec2& standAt) override
    {
        // Every door but one (they still have to come and go, and they do
        // not climb through windows), then the windows.
        const std::vector<glm::ivec2>& doors = g.survivorHomeDoors(building);
        int open = 0;
        for (const glm::ivec2& d : doors)
        {
            if (g.mDoors.boards(d) == 0 && g.mTileMap.doorState(d.x, d.y) != DoorState::Broken) ++open;
        }
        if (open >= 2)
        {
            for (const glm::ivec2& d : doors)
            {
                if (g.mDoors.boards(d) > 0 || g.mTileMap.doorState(d.x, d.y) == DoorState::Broken) continue;
                if (insideOf(building, d, standAt))
                {
                    door = d;
                    return true;
                }
            }
        }
        for (const WayIn& way : g.waysIn(building))
        {
            if (!way.window || g.mWindows.boards(way.tile) > 0) continue;
            if (insideOf(building, way.tile, standAt))
            {
                door = way.tile;
                return true;
            }
        }
        return false;
    }

    void board(Entity e, const glm::ivec2& way) override
    {
        if (!g.mWorld.hasComponent<Inventory>(e)) return;
        Inventory& inv = g.mWorld.getComponent<Inventory>(e);
        if (InventorySystem::countItem(inv, g.mItems.woodPlanks) < 1 || InventorySystem::countItem(inv, g.mItems.nails) < 2) return;
        const SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        const float toughness = 1.0f + 0.3f * ai.p.competence;
        const bool window = g.mTileMap.hasWindow(way.x, way.y);
        if (window)
        {
            if (!g.mWindows.addBoard(g.mTileMap, way, toughness)) return;
            g.mSaveManager.setWindows(g.windowRecords());
        }
        else
        {
            if (g.mTileMap.doorState(way.x, way.y) == DoorState::Open && g.playerTile() != way) g.mDoors.toggle(g.mTileMap, way);
            if (g.mTileMap.doorState(way.x, way.y) != DoorState::Closed) return;
            if (!g.mDoors.addBoard(g.mTileMap, way, toughness)) return;
        }
        InventorySystem::removeItem(inv, g.mItems.woodPlanks, 1);
        InventorySystem::removeItem(inv, g.mItems.nails, 2);
        g.mNoiseModel.emitNoise(tileCentre(g.mTileMap, way), NoiseTier::Medium, 1.5f, e);
        std::cout << "[Survivors] " << ai.name << " boarded the " << (window ? "window" : "door") << " at (" << way.x << ","
                  << way.y << ")\n";
    }

    void shutHome(int building) override
    {
        for (const glm::ivec2& d : g.survivorHomeDoors(building))
        {
            closeDoor(d);
        }
    }

    glm::ivec2 bedTile(int building, const glm::ivec2& near) override
    {
        const glm::ivec2 sides[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        glm::ivec2 best(-1);
        float bestDistance = 0.0f;
        for (const FurniturePiece& piece : g.mFurniture)
        {
            if (piece.building != building || (piece.kind != FurnitureKind::Bed && piece.kind != FurnitureKind::Cot)) continue;
            for (const glm::ivec2& side : sides)
            {
                const glm::ivec2 at = piece.tile + side;
                if (!g.mTileMap.inBounds(at.x, at.y) || g.mTileMap.isSolid(at.x, at.y) ||
                    g.mTileMap.getBuildingId(at.x, at.y) != building || !g.mTileMap.isIndoors(at.x, at.y))
                {
                    continue;
                }
                const float d = glm::length(glm::vec2(at - near));
                if (best.x < 0 || d < bestDistance)
                {
                    best = at;
                    bestDistance = d;
                }
            }
        }
        return best.x >= 0 ? best : roomTile(building, 7u);
    }

    glm::ivec2 roomTile(int building, std::uint32_t roll) override
    {
        const LayoutData& layout = g.mMapGenerator.layoutData();
        if (building <= 0 || building > static_cast<int>(layout.buildings.size())) return glm::ivec2(-1);
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(building - 1)];
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            roll = roll * 1664525u + 1013904223u;
            const int x = b.x + static_cast<int>((roll >> 8) % static_cast<std::uint32_t>(std::max(1, b.w)));
            const int y = b.y + static_cast<int>((roll >> 20) % static_cast<std::uint32_t>(std::max(1, b.h)));
            if (g.mTileMap.inBounds(x, y) && g.mTileMap.getBuildingId(x, y) == building && g.mTileMap.isIndoors(x, y) &&
                !g.mTileMap.isSolid(x, y) && g.mTileMap.doorState(x, y) == DoorState::None)
            {
                return glm::ivec2(x, y);
            }
        }
        return g.homeTileOf(building);
    }

    bool openDoor(Entity e, const glm::ivec2& door) override
    {
        if (g.mDoors.boards(door) > 0) return false;
        SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        const bool own = !g.survivorHomeDoors(ai.homeBuilding).empty() &&
                         std::find(g.survivorHomeDoors(ai.homeBuilding).begin(), g.survivorHomeDoors(ai.homeBuilding).end(),
                                   door) != g.survivorHomeDoors(ai.homeBuilding).end();
        if (!own && g.doorLocked(door)) return false;
        if (!own) g.rattleAlarm(door);
        g.mDoors.open(g.mTileMap, door);
        ai.openedDoor = door;
        return true;
    }

    void closeDoor(const glm::ivec2& door) override
    {
        if (g.mTileMap.doorState(door.x, door.y) != DoorState::Open) return;
        if (g.playerTile() == door) return;
        bool blocked = false;
        g.mWorld.forEach<SurvivorAI, Transform>(
            [&](Entity, SurvivorAI& other, Transform& t)
            {
                if (other.alive && worldToTile(g.mTileMap, centreOf(t)) == door) blocked = true;
            });
        if (!blocked) g.mDoors.toggle(g.mTileMap, door);
    }

    bool tileBurning(const glm::ivec2& tile) override { return g.mFireSystem.burning(tile); }

    WeaponStats weaponOf(Entity e) const
    {
        WeaponStats stats = makeWeaponStats(WeaponCategory::Unarmed);
        if (!g.mWorld.hasComponent<Inventory>(e)) return stats;
        float best = stats.damage;
        for (const InventorySlot& slot : g.mWorld.getComponent<Inventory>(e).slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = g.mItemDatabase.find(slot.itemId);
            if (def == nullptr || def->category != ItemCategory::Weapon ||
                def->weaponCategory == static_cast<int>(WeaponCategory::Ranged))
            {
                continue;
            }
            const WeaponStats s = weaponStatsForItem(*def, &g.mItemDatabase);
            if (s.damage > best)
            {
                best = s.damage;
                stats = s;
            }
        }
        return stats;
    }

    void hit(Entity e, Entity target, float damage, WeaponCategory category, bool shot)
    {
        const glm::vec2 from = centreOf(g.mWorld.getComponent<Transform>(e));
        const glm::vec2 at = centreOf(g.mWorld.getComponent<Transform>(target));
        const glm::vec2 to = at - from;
        const glm::vec2 dir = glm::length(to) > 0.1f ? to / glm::length(to) : glm::vec2(1.0f, 0.0f);
        if (target == g.mPlayerEntity)
        {
            if (!g.mWorld.hasComponent<Health>(target)) return;
            Health& h = g.mWorld.getComponent<Health>(target);
            h.current -= damage;
            g.mPlayerHurtTimer = 0.3f;
            g.mShakeTrauma = std::min(1.0f, g.mShakeTrauma + 0.3f);
            g.mParticleSystem.spawnBloodSpray(at, dir, 10, 1.0f);
            g.mAudioManager.playSound(SoundId::PlayerHurt, 0.6f);
            if ((category == WeaponCategory::Bladed || shot) && !g.mWorld.hasComponent<Bleeding>(target))
            {
                g.mWorld.addComponent<Bleeding>(target, Bleeding{});
            }
            const SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
            g.mLastBlowBy = ai.name;            g.mInteractionMessage = shot ? ai.name + " shoots you." : ai.name + " hits you.";
            g.mInteractionMessageTimer = 2.0f;
            return;
        }
        if (g.mWorld.hasComponent<ZombieAI>(target) && g.mWorld.hasComponent<Health>(target))
        {
            ZombieAI& z = g.mWorld.getComponent<ZombieAI>(target);
            Health& h = g.mWorld.getComponent<Health>(target);
            const bool wasStanding = h.current > 0.0f;
            h.current -= damage;
            if (wasStanding && h.current <= 0.0f) ++g.mWorld.getComponent<SurvivorAI>(e).kills;
            z.lastDamagedBy = e;
            z.lastBlow = static_cast<int>(category);
            z.lastBlowHeavy = false;
            ZombieBlow::land(z, dir, shot ? 10.0f : 26.0f);
        }
    }

    void blowAt(Entity e, Entity target) override
    {
        if (target == kInvalidEntity || !g.mWorld.hasComponent<Transform>(target)) return;
        const SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        const WeaponStats w = weaponOf(e);
        const float damage = w.damage * (0.6f + 0.6f * ai.p.competence);
        g.mNoiseModel.emitNoise(centreOf(g.mWorld.getComponent<Transform>(e)), NoiseTier::Soft, 0.5f, e);
        hit(e, target, damage, w.category, false);
    }

    void shootAt(Entity e, Entity target) override
    {
        if (target == kInvalidEntity || !g.mWorld.hasComponent<Transform>(target) || !g.mWorld.hasComponent<Inventory>(e)) return;
        Inventory& inv = g.mWorld.getComponent<Inventory>(e);
        // A round from the gun, else from the box.
        bool fired = false;
        for (InventorySlot& slot : inv.slots)
        {
            if (slot.itemId == g.mItems.pistol && slot.loaded > 0)
            {
                --slot.loaded;
                fired = true;
                break;
            }
        }
        if (!fired) fired = InventorySystem::removeItem(inv, g.mItems.pistolAmmo, 1);
        if (!fired) return;
        SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        const glm::vec2 from = centreOf(g.mWorld.getComponent<Transform>(e));
        const ItemDef* pistol = g.mItemDatabase.find(g.mItems.pistol);
        const WeaponStats w = pistol != nullptr ? weaponStatsForItem(*pistol, &g.mItemDatabase) : makeWeaponStats(WeaponCategory::Ranged);
        g.mNoiseModel.emitNoise(from, NoiseTier::Loud, 2.0f, e);
        const float player = glm::length(from - g.playerWorldCenter()) / 32.0f;
        if (player < 30.0f) g.mAudioManager.playSound(SoundId::WeaponHit, std::clamp(1.0f - player / 30.0f, 0.15f, 0.9f));
        const float roll = survivorrules::unitHash(g.mSaveManager.worldSeed(), static_cast<std::uint32_t>(ai.id),
                                                   static_cast<std::uint32_t>(g.mHudClock * 60.0f), 0x5407u);
        if (roll < 0.45f + 0.4f * ai.p.competence) hit(e, target, w.damage, WeaponCategory::Ranged, true);
    }

    void bleed(Entity, const glm::vec2& where) override
    {
        g.mParticleSystem.spawnBlood(where, 2);
        const glm::ivec2 t = worldToTile(g.mTileMap, where);
        const bloodtrail::Place place = g.mTileMap.inBounds(t.x, t.y) && g.mTileMap.isIndoors(t.x, t.y)
                                            ? bloodtrail::Place::Indoor
                                            : bloodtrail::Place::Outdoor;
        g.mBloodTrail.addMarker(where, 0.5f, place, false);
    }

    void farScavenge(Entity e) override
    {
        SurvivorAI& ai = g.mWorld.getComponent<SurvivorAI>(e);
        const SurvivorContainer* best = nullptr;
        float bestDistance = 0.0f;
        for (const SurvivorContainer& c : g.mSurvivorContainers)
        {
            if (!searchable(ai, c)) continue;
            const float d = glm::length(glm::vec2(c.tile - ai.homeTile));
            if (best == nullptr || d < bestDistance)
            {
                best = &c;
                bestDistance = d;
            }
        }
        if (best == nullptr) return;
        Inventory& box = g.mWorld.getComponent<Inventory>(best->entity);
        Inventory& stock = g.mWorld.getComponent<Inventory>(e);
        const int moved = takeUseful(box, stock, bestWeaponOf(stock));
        g.recordContainer(best->entity);
        g.markSearchedBySurvivor(best->tile);
        std::cout << "[Survivors] " << ai.name << " (far) emptied (" << best->tile.x << "," << best->tile.y << "): took "
                  << moved << "\n";
    }

private:
    Game& g;
};

// ---------------------------------------------------------------------------
// Lines

void Game::loadSurvivorLines(const std::filesystem::path& assetsRoot)
{
    mSurvivorLines.clear();
    std::ifstream in(assetsRoot / "data" / "survivor_lines.json");
    if (!in)
    {
        std::cerr << "[Survivors] survivor_lines.json missing\n";
        return;
    }
    nlohmann::json root;
    try
    {
        in >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Survivors] survivor_lines.json: " << e.what() << "\n";
        return;
    }
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        if (it->is_array())
        {
            for (const auto& line : *it)
            {
                if (line.is_string()) mSurvivorLines[it.key()].push_back(line.get<std::string>());
            }
        }
        else if (it->is_object())
        {
            for (auto sub = it->begin(); sub != it->end(); ++sub)
            {
                if (!sub->is_array()) continue;
                for (const auto& line : *sub)
                {
                    if (line.is_string()) mSurvivorLines[it.key() + "." + sub.key()].push_back(line.get<std::string>());
                }
            }
        }
    }
}

const std::string& Game::survivorLine(const std::string& pool, const std::string& name, std::uint32_t salt) const
{
    static const std::string kNothing;
    const auto it = mSurvivorLines.find(pool);
    if (it == mSurvivorLines.end() || it->second.empty()) return kNothing;
    return it->second[survivorrules::lineIndex(name, mCurrentDay, salt, it->second.size())];
}

// ---------------------------------------------------------------------------
// Records, homes and the cast

void Game::loadSurvivorRecords()
{
    mSurvivors = mSaveManager.loadSurvivors();
    for (std::size_t i = 0; i < mSurvivors.size(); ++i) mSurvivors[i].id = static_cast<int>(i);
    mTheftNoted.clear();
}

void Game::avoidSurvivorHomes()
{
    const glm::ivec2 start = mSpawnData.playerStart;
    if (!mTileMap.inBounds(start.x, start.y)) return;
    const int base = mSaveManager.baseRecord().building;
    const auto someoneHome = [&](int building)
    {
        if (building <= 0 || building == base) return false;
        return std::any_of(mSurvivors.begin(), mSurvivors.end(),
                           [&](const SurvivorRecord& r) { return r.alive && r.homeBuilding == building; });
    };
    // Nor next door to the ones who shoot on sight (a later life: the first
    // is cast with the garage away from the start).
    const auto hostileNear = [&](const glm::ivec2& tile)
    {
        return std::any_of(mSurvivors.begin(), mSurvivors.end(),
                           [&](const SurvivorRecord& r)
                           {
                               return r.alive && !r.companion && !r.retired && r.homeX >= 0 &&
                                      (r.group == 1 || r.aggression > 0.7f) &&
                                      glm::length(glm::vec2(tile - glm::ivec2(r.homeX, r.homeY))) < kHostileNeighbourTiles;
                           });
    };
    const int startBuilding = mTileMap.getBuildingId(start.x, start.y);
    if (!someoneHome(startBuilding) && (startBuilding == base || !hostileNear(start))) return;
    glm::ivec2 best(-1);
    float bestDistance = 0.0f;
    const LayoutData& layout = mMapGenerator.layoutData();
    // Far from the hostile ones if anywhere is (pass 0); else the nearest free house.
    for (int pass = 0; pass < 2 && best.x < 0; ++pass)
    {
        for (int y = 0; y < mTileMap.height(); ++y)
        {
            for (int x = 0; x < mTileMap.width(); ++x)
            {
                const int b = mTileMap.getBuildingId(x, y);
                if (b <= 0 || !mTileMap.isIndoors(x, y) || mTileMap.isSolid(x, y) || mTileMap.hasFurniture(x, y)) continue;
                if (someoneHome(b) || guttedInMemory(b)) continue;
                if (b > static_cast<int>(layout.buildings.size()) ||
                    buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(b - 1)]) != BuildingKind::House)
                {
                    continue;
                }
                if (pass == 0 && hostileNear(glm::ivec2(x, y))) continue;
                const float distance = glm::length(glm::vec2(glm::ivec2(x, y) - start));
                if (best.x < 0 || distance < bestDistance)
                {
                    best = glm::ivec2(x, y);
                    bestDistance = distance;
                }
            }
        }
    }
    if (best.x < 0) return;
    const int building = mTileMap.getBuildingId(best.x, best.y);
    mSpawnData.playerStart = best;
    std::erase_if(mSpawnData.zombieSpawns, [&](const glm::ivec2& tile) { return mTileMap.getBuildingId(tile.x, tile.y) == building; });
    std::cout << "[Survivors] " << (someoneHome(startBuilding) ? "someone lives in the start house" : "the start house is by a hostile one")
              << "; starting in building " << building << "\n";
}

glm::ivec2 Game::homeTileOf(int building) const
{
    const LayoutData& layout = mMapGenerator.layoutData();
    if (building <= 0 || building > static_cast<int>(layout.buildings.size())) return glm::ivec2(-1);
    const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(building - 1)];
    const glm::vec2 centre(static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f, static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f);
    glm::ivec2 best(-1);
    float bestDistance = 0.0f;
    for (int y = b.y; y < b.y + b.h; ++y)
    {
        for (int x = b.x; x < b.x + b.w; ++x)
        {
            if (!mTileMap.inBounds(x, y) || mTileMap.getBuildingId(x, y) != building || !mTileMap.isIndoors(x, y) ||
                mTileMap.isSolid(x, y) || mTileMap.hasFurniture(x, y) || mTileMap.doorState(x, y) != DoorState::None)
            {
                continue;
            }
            const float d = glm::length(glm::vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f) - centre);
            if (best.x < 0 || d < bestDistance)
            {
                best = glm::ivec2(x, y);
                bestDistance = d;
            }
        }
    }
    return best;
}

void Game::buildSurvivorContainers()
{
    mSurvivorContainers.clear();
    mSurvivorHomeDoors.clear();
    const glm::ivec2 sides[4] = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}};
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory&, Transform& t)
        {
            if (inter.type != InteractableType::Container || mWorld.hasComponent<Sprite>(e) || mWorld.hasComponent<Structure>(e) ||
                mWorld.hasComponent<Corpse>(e) || mWorld.hasComponent<Vehicle>(e) || mWorld.hasComponent<SurvivorAI>(e) ||
                inter.prompt == "Search the pile")
            {
                return;
            }
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 1.0f, t.y + 1.0f));
            const FurniturePiece* piece = pieceAt(mFurniture, tile);
            if (piece == nullptr || piece->kind == FurnitureKind::Bed || piece->kind == FurnitureKind::Cot) return;
            SurvivorContainer c;
            c.entity = e;
            c.tile = tile;
            c.building = mTileMap.getBuildingId(tile.x, tile.y);
            for (const glm::ivec2& side : sides)
            {
                const glm::ivec2 at = tile + side;
                if (mTileMap.inBounds(at.x, at.y) && !mTileMap.isSolid(at.x, at.y) && mTileMap.doorState(at.x, at.y) == DoorState::None &&
                    mTileMap.getBuildingId(at.x, at.y) == c.building)
                {
                    c.stand = at;
                    break;
                }
            }
            if (c.stand.x >= 0) mSurvivorContainers.push_back(c);
        });
}

const std::vector<glm::ivec2>& Game::survivorHomeDoors(int building)
{
    auto it = mSurvivorHomeDoors.find(building);
    if (it != mSurvivorHomeDoors.end()) return it->second;
    std::vector<glm::ivec2> doors;
    for (const WayIn& way : waysIn(building))
    {
        if (!way.window) doors.push_back(way.tile);
    }
    return mSurvivorHomeDoors.emplace(building, std::move(doors)).first->second;
}

Entity Game::survivorNextContainer(Entity survivor)
{
    if (!mWorld.hasComponent<SurvivorAI>(survivor)) return kInvalidEntity;
    SurvivorHooks hooks(*this);
    glm::ivec2 stand(-1);
    return hooks.findContainer(survivor, stand);
}

bool Game::survivorCouldBoard(int building)
{
    SurvivorHooks hooks(*this);
    glm::ivec2 way(-1);
    glm::ivec2 stand(-1);
    return hooks.openWayIn(building, way, stand);
}

void Game::setupSurvivors()
{
    buildSurvivorContainers();
    if (mSurvivors.empty())
    {
        castSurvivors();
    }
    // Older files (version 3): everyone alive without a home gets one.
    std::vector<int> taken;
    for (const SurvivorRecord& r : mSurvivors)
    {
        if (r.homeBuilding > 0) taken.push_back(r.homeBuilding);
    }
    for (SurvivorRecord& r : mSurvivors)
    {
        if (r.alive && r.homeBuilding <= 0) giveSurvivorHome(r, taken);
    }
    // Nobody shares a house with the dead: whatever was in one is long gone.
    // Not an earlier life risen: that one is the legacy's, and stays.
    std::vector<Entity> cleared;
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI&, Transform& t)
        {
            if (mWorld.hasComponent<LegacyLink>(e)) return;
            const glm::ivec2 at = worldToTile(mTileMap, centreOf(t));
            if (mTileMap.inBounds(at.x, at.y) && mTileMap.isIndoors(at.x, at.y) && survivorLivesIn(mTileMap.getBuildingId(at.x, at.y)))
            {
                cleared.push_back(e);
            }
        });
    for (Entity e : cleared) mWorld.destroyEntity(e);
    int living = 0;
    int bodies = 0;
    for (std::size_t i = 0; i < mSurvivors.size(); ++i)
    {
        const SurvivorRecord& r = mSurvivors[i];
        if ((!r.alive && r.buried) || r.left) continue; // the buried, and those who went out by the gate
        spawnSurvivor(i);
        (r.alive ? living : bodies) += 1;
    }
    std::cout << "[Survivors] " << living << " living, " << bodies << " bodies, " << mSurvivorContainers.size()
              << " containers they know, " << cleared.size() << " of the dead cleared from their homes\n";
}

void Game::castSurvivors()
{
    const LayoutData& layout = mMapGenerator.layoutData();
    const int startBuilding = mTileMap.inBounds(mSpawnData.playerStart.x, mSpawnData.playerStart.y)
                                  ? mTileMap.getBuildingId(mSpawnData.playerStart.x, mSpawnData.playerStart.y)
                                  : 0;
    const int base = mSaveManager.baseRecord().building;
    std::vector<survivorrules::HomeChoice> houses;
    std::vector<survivorrules::HomeChoice> garages;
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        const int building = static_cast<int>(i) + 1;
        if (building == startBuilding || building == base || building == mFirstDays.homeBuilding || guttedInMemory(building) ||
            buildingGutted(building))
        {
            continue;
        }
        const BuildingKind kind = buildingkinds::kindOf(layout.buildings[i]);
        if (kind != BuildingKind::House && kind != BuildingKind::Garage) continue;
        const glm::ivec2 tile = homeTileOf(building);
        if (tile.x < 0) continue;
        survivorrules::HomeChoice choice;
        choice.building = building;
        choice.tile = tile;
        choice.garage = kind == BuildingKind::Garage;
        choice.big = layout.buildings[i].w * layout.buildings[i].h >= 90;
        // The hostile ones are not housed by the start (the rules choose).
        choice.nearStart = startBuilding > 0 &&
                           glm::length(glm::vec2(tile - mSpawnData.playerStart)) < kHostileNeighbourTiles;
        (choice.garage ? garages : houses).push_back(choice);
    }
    const auto names = mSurvivorLines.find("names");
    const std::vector<std::string> pool = names != mSurvivorLines.end() ? names->second : std::vector<std::string>{};
    const bool society = saveThreatModeFromIndex(mTitleModeIndex) == SaveThreatMode::InfectedSociety;
    const std::vector<survivorrules::CastMember> cast =
        survivorrules::castWorld(mSaveManager.worldSeed() ^ 0x5017u, society, houses, garages, pool);

    mSurvivors.clear();
    for (const survivorrules::CastMember& m : cast)
    {
        SurvivorRecord r;
        r.id = static_cast<int>(mSurvivors.size());
        r.name = m.name;
        r.role = static_cast<int>(m.role);
        r.partner = m.partner;
        r.group = m.group;
        r.outfit = m.outfit;
        r.stayer = m.stayer;
        r.aggression = m.p.aggression;
        r.cooperation = m.p.cooperation;
        r.competence = m.p.competence;
        r.spawnDay = mCurrentDay;
        r.hunger = 80.0f;
        r.thirst = 80.0f;
        if (m.home >= 0)
        {
            const survivorrules::HomeChoice& home = m.home < static_cast<int>(houses.size())
                                                        ? houses[static_cast<std::size_t>(m.home)]
                                                        : garages[static_cast<std::size_t>(m.home) - houses.size()];
            r.homeBuilding = home.building;
            r.homeX = home.tile.x;
            r.homeY = home.tile.y;
            r.x = static_cast<float>(home.tile.x * mTileMap.tileWidth());
            r.y = static_cast<float>(home.tile.y * mTileMap.tileHeight());
            r.homeDistrictId = mTileMap.getDistrictId(home.tile.x, home.tile.y);
        }
        for (const auto& [key, count] : m.stock)
        {
            const int id = mItemDatabase.idOf(key);
            if (id < 0) continue;
            ContainerItemRecord item;
            item.itemId = id;
            item.count = count;
            if (key == "pistol") item.loaded = 0;
            r.stock.push_back(item);
        }
        mSurvivors.push_back(r);
    }
    std::cout << "[Survivors] cast " << mSurvivors.size() << " people (" << houses.size() << " houses, " << garages.size()
              << " garages free)\n";
    mSaveManager.saveSurvivors(mSurvivors);
}

void Game::giveSurvivorHome(SurvivorRecord& record, std::vector<int>& taken)
{
    const LayoutData& layout = mMapGenerator.layoutData();
    const int startBuilding = mTileMap.inBounds(mSpawnData.playerStart.x, mSpawnData.playerStart.y)
                                  ? mTileMap.getBuildingId(mSpawnData.playerStart.x, mSpawnData.playerStart.y)
                                  : 0;
    const glm::vec2 from(record.x / static_cast<float>(mTileMap.tileWidth()), record.y / static_cast<float>(mTileMap.tileHeight()));
    int best = 0;
    glm::ivec2 bestTile(-1);
    float bestDistance = 0.0f;
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        const int building = static_cast<int>(i) + 1;
        if (buildingkinds::kindOf(layout.buildings[i]) != BuildingKind::House) continue;
        if (building == startBuilding || building == mSaveManager.baseRecord().building || guttedInMemory(building)) continue;
        if (std::find(taken.begin(), taken.end(), building) != taken.end()) continue;
        const glm::ivec2 tile = homeTileOf(building);
        if (tile.x < 0) continue;
        const float d = glm::length(glm::vec2(tile) - from);
        if (best == 0 || d < bestDistance)
        {
            best = building;
            bestTile = tile;
            bestDistance = d;
        }
    }
    if (best == 0) return;
    taken.push_back(best);
    record.homeBuilding = best;
    record.homeX = bestTile.x;
    record.homeY = bestTile.y;
    record.homeDistrictId = mTileMap.getDistrictId(bestTile.x, bestTile.y);
    std::cout << "[Survivors] " << record.name << " has a home now (building " << best << ")\n";
}

Entity Game::spawnSurvivor(std::size_t index)
{
    const SurvivorRecord& r = mSurvivors[index];
    const Entity e = mWorld.createEntity();
    glm::ivec2 home(r.homeX, r.homeY);
    if (r.homeBuilding > 0 && (home.x < 0 || !mTileMap.inBounds(home.x, home.y) || mTileMap.isSolid(home.x, home.y)))
    {
        home = homeTileOf(r.homeBuilding);
    }
    Transform t{};
    t.x = r.x;
    t.y = r.y;
    const glm::ivec2 at = worldToTile(mTileMap, glm::vec2(r.x + kHalf, r.y + kHalf));
    if ((r.x == 0.0f && r.y == 0.0f) || !mTileMap.inBounds(at.x, at.y) || (r.alive && mTileMap.isSolid(at.x, at.y)))
    {
        if (home.x >= 0)
        {
            t.x = static_cast<float>(home.x * mTileMap.tileWidth());
            t.y = static_cast<float>(home.y * mTileMap.tileHeight());
        }
    }
    t.prevX = t.x;
    t.prevY = t.y;

    const int outfit = ((r.outfit % 3) + 3) % 3;
    Animation anim{};
    anim.dir = 1;
    anim.currentAnim = "surv" + std::to_string(outfit) + "_none_idle_se";
    anim.frameDuration = 0.55f;
    Sprite sprite{};
    sprite.textureId = mSpriteSheet.textureId();
    sprite.color = glm::vec4(1.0f);
    sprite.layer = 9;
    sprite.drawSize = SpriteSizes::kHumanoid;
    if (!r.alive)
    {
        anim.currentAnim = "surv" + std::to_string(outfit) + "_death_se";
        anim.looping = false;
        anim.currentFrame = std::max(0, mSpriteSheet.frameCount(anim.currentAnim) - 1);
        anim.finished = true;
        sprite.layer = 3;
    }
    sprite.uvRect = mSpriteSheet.frameUv(anim.currentAnim, anim.currentFrame);

    SurvivorAI ai{};
    ai.id = r.id;
    ai.name = r.name;
    ai.p.aggression = r.aggression;
    ai.p.cooperation = r.cooperation;
    ai.p.competence = r.competence;
    ai.role = static_cast<Role>(std::clamp(r.role, 0, 3));
    ai.outfit = outfit;
    ai.group = r.group;
    ai.stayer = r.stayer;
    ai.homeBuilding = r.homeBuilding;
    ai.homeTile = home.x >= 0 ? home : at;
    ai.hunger = r.hunger;
    ai.thirst = r.thirst;
    ai.sleepDebt = r.sleepDebt;
    ai.wounds = r.wounds;
    ai.trust = r.trust;
    ai.angryDay = r.angryDay;
    ai.warnedHome = r.warnedHome;
    ai.avoiding = r.avoiding;
    ai.ignoredHours = r.ignoredHours;
    ai.outDay = r.outDay;
    ai.alive = r.alive;
    ai.buried = r.buried;
    ai.state = r.alive ? SurvivorState::Idle : SurvivorState::Dead;
    ai.thinkTimer = static_cast<float>(r.id % 7) * 0.29f; // not all on one tick
    ai.farHourClock = static_cast<float>(r.id % 13) * 4.0f;
    const int district = home.x >= 0 ? mTileMap.getDistrictId(home.x, home.y) : 0;
    ai.danger = survivorrules::dangerOf(district > 0 ? mDistrictInfectionSystem.getInfection(district) : 0.0f);
    ai.attitude = survivorrules::attitude(ai.p, 0.0f, ai.trust, false);

    Inventory stock{};
    stock.maxSlots = Inventory::kMaxSlots;
    stock.maxWeight = kStockWeight;
    for (const ContainerItemRecord& item : r.stock)
    {
        InventorySystem::addItemWithCondition(stock, mItemDatabase, item.itemId, item.count, item.condition, item.loaded);
    }

    mWorld.addComponent<Transform>(e, t);
    mWorld.addComponent<Sprite>(e, sprite);
    mWorld.addComponent<Animation>(e, anim);
    mWorld.addComponent<Velocity>(e, Velocity{});
    mWorld.addComponent<Health>(e, Health{r.alive ? std::max(1.0f, r.health) : 0.0f, 60.0f, 60.0f});
    mWorld.addComponent<SurvivorAI>(e, ai);
    mWorld.addComponent<Inventory>(e, stock);
    return e;
}

Entity Game::survivorEntity(int id) const
{
    Entity found = kInvalidEntity;
    const_cast<World&>(mWorld).forEach<SurvivorAI>(
        [&](Entity e, SurvivorAI& ai)
        {
            if (ai.id == id) found = e;
        });
    return found;
}

Entity Game::companionEntity() const
{
    Entity found = kInvalidEntity;
    const_cast<World&>(mWorld).forEach<SurvivorAI>(
        [&](Entity e, SurvivorAI& ai)
        {
            if (ai.alive && ai.companion) found = e;
        });
    return found;
}

bool Game::survivorLivesIn(int building) const
{
    if (building <= 0) return false;
    return std::any_of(mSurvivors.begin(), mSurvivors.end(),
                       [&](const SurvivorRecord& r) { return r.alive && !r.companion && r.homeBuilding == building; });
}

bool Game::companionNear(float tiles) const
{
    const Entity c = companionEntity();
    if (c == kInvalidEntity || !mWorld.hasComponent<Transform>(c)) return false;
    return glm::length(centreOf(mWorld.getComponent<Transform>(c)) - playerWorldCenter()) <=
           tiles * static_cast<float>(mTileMap.tileWidth());
}

std::vector<survivorrules::StockLine> Game::survivorStockLines(Entity survivor) const
{
    std::vector<survivorrules::StockLine> lines;
    if (!mWorld.hasComponent<Inventory>(survivor)) return lines;
    for (const InventorySlot& slot : mWorld.getComponent<Inventory>(survivor).slots)
    {
        survivorrules::StockLine line;
        if (slot.itemId >= 0 && slot.count > 0)
        {
            if (const ItemDef* def = mItemDatabase.find(slot.itemId))
            {
                line.itemId = slot.itemId;
                line.count = slot.count;
                line.kind = itemKind(*def);
                line.hunger = edible(*def, slot) ? def->hungerRestore : 0.0f;
                line.thirst = drinkable(*def, slot) ? def->thirstRestore : 0.0f;
                if (line.kind == Kind::Weapon) line.weaponScore = std::max(0.1f, weaponStatsForItem(*def, &mItemDatabase).damage);
            }
        }
        lines.push_back(line);
    }
    return lines;
}

void Game::markSearchedBySurvivor(const glm::ivec2& tile)
{
    ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
    if (hasPair(memory.searched, tile)) return;
    memory.searched.emplace_back(tile.x, tile.y);
    mFurnitureView.setAlt(tile, true); // drawn ajar: someone has been
    mTownUnopened = std::max(0, mTownUnopened - 1);
    const int district = mTileMap.getDistrictId(tile.x, tile.y);
    if (district >= 0 && district < static_cast<int>(mDistrictStocked.size()))
    {
        mDistrictStocked[static_cast<std::size_t>(district)] = std::max(0, mDistrictStocked[static_cast<std::size_t>(district)] - 1);
    }
    mSaveManager.setScavengeMemory(std::move(memory));
    mSaveManager.saveWorldMemory();
}

// ---------------------------------------------------------------------------
// Each tick

void Game::survivorSays(Entity survivor, const std::string& pool, std::uint32_t salt)
{
    if (!mWorld.hasComponent<SurvivorAI>(survivor)) return;
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(survivor);
    const std::string& line = survivorLine(pool, ai.name, salt);
    if (line.empty()) return;
    ai.say = line;
    ai.sayTimer = kSaySeconds;
    if (mTalk.open && mTalk.who == survivor) mTalk.line = line;
}

void Game::updateSurvivors(float dtSeconds)
{
    mLitHomes.clear();
    if (mSurvivors.empty() || mPlayerEntity == kInvalidEntity) return;

    // A weapon out: a swing in the last 5 s, or a gun in hand.
    bool gunInHand = false;
    if (mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        const Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
        gunInHand = combat.weapon.itemId >= 0 && combat.weapon.category == WeaponCategory::Ranged;
        if (combat.phase != AttackPhase::Idle && !combat.shoving) mSinceWeaponOut = 0.0f;
    }
    mSinceWeaponOut = gunInHand ? 0.0f : mSinceWeaponOut + dtSeconds;
    const bool weaponOut = mSinceWeaponOut < 5.0f;

    const glm::vec2 player = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const bool playerAlive = mWorld.hasComponent<Health>(mPlayerEntity) && mWorld.getComponent<Health>(mPlayerEntity).current > 0.0f;
    const bool asleep = mWorld.hasComponent<Sleeping>(mPlayerEntity);

    SurvivorTick tick;
    tick.map = &mTileMap;
    tick.noise = &mNoiseModel;
    tick.grid = &mZombieAISystem.grid();
    tick.dt = dtSeconds;
    tick.day = mCurrentDay;
    tick.hours = mGameHours;
    tick.night = nightNow();
    tick.player = mPlayerEntity;
    tick.playerCentre = player;
    tick.playerValid = playerAlive;
    tick.playerCrouched = mWorld.hasComponent<Facing>(mPlayerEntity) && mWorld.getComponent<Facing>(mPlayerEntity).crouching;
    tick.playerAsleep = asleep;
    tick.seed = mSaveManager.worldSeed() ^ (static_cast<std::uint32_t>(mSaveManager.runNumber()) * 0x9E3779B1u);
    SurvivorHooks hooks(*this);
    mSurvivorBrain.update(mWorld, tick, hooks);
    // Someone talking with you stands still, unless the dead are on them.
    if (mTalk.open && mWorld.hasComponent<SurvivorAI>(mTalk.who) && mWorld.hasComponent<Velocity>(mTalk.who))
    {
        const SurvivorState state = mWorld.getComponent<SurvivorAI>(mTalk.who).state;
        if (state != SurvivorState::Flee && state != SurvivorState::Fight && state != SurvivorState::Attack)
        {
            mWorld.getComponent<Velocity>(mTalk.who) = Velocity{};
        }
    }

    const glm::ivec2 playerAt = playerTile();
    const int playerBuilding = mTileMap.inBounds(playerAt.x, playerAt.y) && mTileMap.isIndoors(playerAt.x, playerAt.y)
                                   ? mTileMap.getBuildingId(playerAt.x, playerAt.y)
                                   : 0;
    glm::vec2 facing(1.0f, 0.0f);
    if (mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        const float a = mWorld.getComponent<Facing>(mPlayerEntity).angleRadians;
        facing = glm::vec2(std::cos(a), std::sin(a));
    }
    const double hours = mGameHours;
    const float gameHoursThisTick = static_cast<float>(dtSeconds * kGameHoursPerRealSecond);

    std::vector<Entity> fallen;
    mWorld.forEach<SurvivorAI, Transform, Health>(
        [&](Entity e, SurvivorAI& ai, Transform& t, Health& h)
        {
            if (!ai.alive) return;
            if (ai.id < 0 || ai.id >= static_cast<int>(mSurvivors.size())) return;
            SurvivorRecord& record = mSurvivors[static_cast<std::size_t>(ai.id)];
            ai.sayTimer = std::max(0.0f, ai.sayTimer - dtSeconds);
            ai.sincePlayerHit += dtSeconds;
            if (h.current <= 0.0f)
            {
                fallen.push_back(e);
                return;
            }
            const glm::vec2 centre = centreOf(t);
            const float d = glm::length(centre - player) / tileSize;
            const glm::ivec2 here = worldToTile(mTileMap, centre);
            const bool atHome = ai.homeBuilding > 0 && mTileMap.getBuildingId(here.x, here.y) == ai.homeBuilding;
            const bool lonerTrait = playerTraits() != nullptr && playerTraits()->has(NegativeTrait::Loner);

            // What they hold, for the frames.
            ai.loadout = 0;
            if (mWorld.hasComponent<Inventory>(e))
            {
                for (const InventorySlot& slot : mWorld.getComponent<Inventory>(e).slots)
                {
                    if (slot.itemId < 0 || slot.count <= 0) continue;
                    const ItemDef* def = mItemDatabase.find(slot.itemId);
                    if (def == nullptr || def->category != ItemCategory::Weapon) continue;
                    const int load = def->weaponCategory == static_cast<int>(WeaponCategory::Ranged) ? 3
                                     : def->weaponCategory == static_cast<int>(WeaponCategory::Bladed) ? 2 : 1;
                    ai.loadout = std::max(ai.loadout, load);
                }
            }

            // Hurt by you.
            if (ai.playerDamage > 0.0f)
            {
                ai.sincePlayerHit = 0.0f;
                ai.hurtFlash = 0.3f;
                if (ai.playerDamage >= 10.0f) ai.bleedSeconds = std::max(ai.bleedSeconds, 6.0f);
                ai.playerDamage = 0.0f;
                if (!ai.companion)
                {
                    ai.angryDay = mCurrentDay;
                    survivorSays(e, "hurt", static_cast<std::uint32_t>(h.current));
                    const bool fightsBack = ai.p.aggression > 0.4f || ai.loadout > 0;
                    ai.state = fightsBack && h.current > h.maximum * 0.3f ? SurvivorState::Attack : SurvivorState::GoHome;
                    ai.path.clear();
                }
            }

            // Seen: the notebook's list, and where they live.
            if (playerAlive && d < 10.0f)
            {
                if (!record.met || record.metRun != mSaveManager.runNumber())
                {
                    std::cout << "[Survivors] met " << ai.name << "\n";
                    noteJournal(journal::Kind::SurvivorMet, ai.name);
                }
                record.met = true;
                record.metRun = mSaveManager.runNumber();
                record.lastSeenDay = mCurrentDay;
                if (atHome || (playerBuilding > 0 && playerBuilding == ai.homeBuilding)) record.homeKnown = true;
                // Someone who remembers the last one, or the one before.
                if (d < 6.0f && !ai.companion && ai.attitude != Attitude::Hostile &&
                    std::find(record.told.begin(), record.told.end(), "greeted") == record.told.end())
                {
                    record.told.emplace_back("greeted");
                    if (record.retired) survivorSays(e, "bloodline");
                    else if (ai.trust >= survivorrules::kRememberedStart && ai.trust < survivorrules::kRememberedStart + 1.0f)
                        survivorSays(e, "remembered");
                }
            }

            if (ai.companion)
            {
                Inventory* pack = mWorld.hasComponent<Inventory>(e) ? &mWorld.getComponent<Inventory>(e) : nullptr;
                if (pack != nullptr) pack->maxWeight = kCompanionWeight;
                if (!ai.waiting && ai.state != SurvivorState::Follow) ai.state = SurvivorState::Follow;
                if (ai.waiting && ai.state != SurvivorState::Wait) ai.state = SurvivorState::Wait;
                // Their own pack first, then they ask.
                bool outOfFood = false;
                bool outOfWater = false;
                if (ai.hunger < survivorrules::kHungryBelow && !hooks.consume(e, false)) outOfFood = true;
                if (ai.thirst < survivorrules::kHungryBelow && !hooks.consume(e, true)) outOfWater = true;
                ai.askClock -= dtSeconds;
                if ((outOfFood || outOfWater) && ai.askClock <= 0.0f && d < 10.0f)
                {
                    survivorSays(e, outOfWater ? "companion_thirsty" : "companion_hungry");
                    ai.askClock = 90.0f;
                }
                if (outOfFood || outOfWater)
                {
                    ai.ignoredHours += gameHoursThisTick;
                    if (ai.ignoredHours > survivorrules::kIgnoredHours)
                    {
                        ai.trust = survivorrules::clampTrust(ai.trust + survivorrules::kIgnoredTrustPerDay * gameHoursThisTick / 24.0f);
                    }
                }
                else
                {
                    ai.ignoredHours = 0.0f;
                }
                if (ai.trust < survivorrules::kLeaveBelowTrust)
                {
                    ai.companion = false;
                    ai.waiting = false;
                    ai.state = SurvivorState::GoHome;
                    record.companion = false;
                    if (pack != nullptr) pack->maxWeight = kStockWeight;
                    survivorSays(e, "companion_leave");
                    mInteractionMessage = ai.name + " has gone home.";
                    mInteractionMessageTimer = 4.0f;
                    std::cout << "[Survivors] " << ai.name << " left you\n";
                }
            }
            else if (playerAlive)
            {
                // A weapon out near them.
                if (weaponOut && d < survivorrules::kWeaponDrawnTiles && ai.attitude == Attitude::Wary && !ai.warnedWeapon)
                {
                    ai.warnedWeapon = true;
                    ai.trust = survivorrules::clampTrust(ai.trust + survivorrules::kWeaponDrawnTrust);
                    survivorSays(e, "warn");
                }
                if (mSinceWeaponOut > 10.0f) ai.warnedWeapon = false;

                // In their house uninvited: one warning, then they turn.
                if (playerBuilding > 0 && playerBuilding == ai.homeBuilding && ai.trust < survivorrules::kHomeWarnTrust && d < 14.0f)
                {
                    if (!ai.warnedHome)
                    {
                        ai.warnedHome = true;
                        ai.trespassSeconds = 0.0f;
                        survivorSays(e, "trespass");
                        std::cout << "[Survivors] " << ai.name << " warns you out\n";
                    }
                    else
                    {
                        ai.trespassSeconds += dtSeconds;
                        if (ai.trespassSeconds > 5.0f && ai.angryDay != mCurrentDay)
                        {
                            ai.angryDay = mCurrentDay;
                            std::cout << "[Survivors] " << ai.name << " turns on you\n";
                        }
                    }
                }

                // Going for you, and giving up.
                const bool hostile = ai.attitude == Attitude::Hostile || ai.angryDay == mCurrentDay;
                const float reach = weaponOut ? 10.0f : 6.0f;
                // Only someone they can see (the dead's sight: walls, and glass only so far).
                if (hostile && !ai.avoiding && !asleep && d < reach && ai.state != SurvivorState::Attack &&
                    ZombieAISystem::hasLineOfSight(mTileMap, centre, player) &&
                    ai.state != SurvivorState::Flee && h.current > h.maximum * 0.3f)
                {
                    ai.state = SurvivorState::Attack;
                    ai.path.clear();
                    survivorSays(e, "hello_hostile", 7u);
                    std::cout << "[Survivors] " << ai.name << " goes for you\n";
                }
                if (ai.state == SurvivorState::Attack && (d > 14.0f || h.current < h.maximum * 0.3f || !hostile))
                {
                    ai.state = SurvivorState::GoHome;
                    ai.path.clear();
                }

                // The desperate follow you, and take what they can.
                if (!ai.robbed && !ai.avoiding && !hostile && d < 20.0f && !SurvivorBrain::isOverride(ai.state) &&
                    survivorrules::wouldRob(ai.desperation, ai.trust, false))
                {
                    ai.state = SurvivorState::Rob;
                    ai.robSeconds = 0.0f;
                    std::cout << "[Survivors] " << ai.name << " follows you\n";
                }
                if (ai.state == SurvivorState::Rob)
                {
                    ai.robSeconds += dtSeconds;
                    const glm::vec2 toThem = d > 0.01f ? (centre - player) / (d * tileSize) : glm::vec2(0.0f);
                    const bool backTurned = playerBuilding > 0 && glm::dot(facing, toThem) < -0.3f && d < 2.0f;
                    if ((asleep && d < kRobTakeTiles) || backTurned)
                    {
                        // Food and water from your pack: not a fight.
                        int taken = 0;
                        if (mWorld.hasComponent<Inventory>(mPlayerEntity) && mWorld.hasComponent<Inventory>(e))
                        {
                            Inventory& pack = mWorld.getComponent<Inventory>(mPlayerEntity);
                            Inventory& theirs = mWorld.getComponent<Inventory>(e);
                            for (int i = 0; i < Inventory::kMaxSlots && taken < 4; ++i)
                            {
                                const InventorySlot& slot = pack.slots[static_cast<std::size_t>(i)];
                                if (slot.itemId < 0 || slot.count <= 0) continue;
                                const ItemDef* def = mItemDatabase.find(slot.itemId);
                                if (def == nullptr || !(edible(*def, slot) || drinkable(*def, slot))) continue;
                                taken += storage::moveItem(pack, i, theirs, mItemDatabase, std::min(slot.count, 4 - taken));
                            }
                        }
                        ai.robbed = true;
                        ai.avoiding = true;
                        ai.state = SurvivorState::GoHome;
                        ai.path.clear();
                        if (asleep)
                        {
                            mRobbedAsleep = mRobbedAsleep || taken > 0;
                        }
                        else
                        {
                            survivorSays(e, "rob");
                            mInteractionMessage = taken > 0 ? "Someone was in your pack. Food and water gone." : "Someone was behind you.";
                            mInteractionMessageTimer = 4.0f;
                        }
                        std::cout << "[Survivors] " << ai.name << " robbed you: " << taken << " things\n";
                    }
                    else if (ai.robSeconds > 60.0f)
                    {
                        ai.state = SurvivorState::GoHome;
                        ai.robbed = true; // they lost their nerve
                    }
                }
                if (ai.avoiding && d < 8.0f && ai.state != SurvivorState::GoHome && ai.state != SurvivorState::Flee)
                {
                    ai.state = SurvivorState::GoHome;
                }
            }

            // A lamp in the window at night, while they are up.
            const bool awake = !(ai.state == SurvivorState::Sleep && ai.actionTimer > 0.0f);
            if ((atHome || ai.far) && awake && hours >= 18.0 && hours < 23.0 && ai.homeBuilding > 0 &&
                std::find(mLitHomes.begin(), mLitHomes.end(), ai.homeBuilding) == mLitHomes.end())
            {
                mLitHomes.push_back(ai.homeBuilding);
            }
            (void)lonerTrait;
        });
    for (Entity e : fallen) onSurvivorDeath(e);
    updateSurvivorSmoke(dtSeconds);
}

void Game::onSurvivorDeath(Entity survivor)
{
    if (!mWorld.hasComponent<SurvivorAI>(survivor)) return;
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(survivor);
    if (!ai.alive) return;
    ai.alive = false;
    ai.state = SurvivorState::Dead;
    ai.say.clear();
    ai.sayTimer = 0.0f;
    if (mWorld.hasComponent<Velocity>(survivor)) mWorld.getComponent<Velocity>(survivor) = Velocity{};
    if (mWorld.hasComponent<Health>(survivor)) mWorld.getComponent<Health>(survivor).current = 0.0f;
    if (mTalk.open && mTalk.who == survivor) closeTalk();
    if (mContainerOpen && mOpenContainerEntity == survivor) closeContainer();

    const Transform& t = mWorld.getComponent<Transform>(survivor);
    const glm::vec2 centre = centreOf(t);
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float d = glm::length(centre - playerWorldCenter()) / tileSize;
    SurvivorRecord* record = ai.id >= 0 && ai.id < static_cast<int>(mSurvivors.size()) ? &mSurvivors[static_cast<std::size_t>(ai.id)] : nullptr;
    if (record != nullptr)
    {
        record->alive = false;
        record->deathDay = mCurrentDay;
    }
    std::cout << "[Survivors] " << ai.name << " died on Day " << mCurrentDay << (ai.companion ? " (your companion)" : "") << "\n";
    if (record != nullptr && record->met && record->metRun == mSaveManager.runNumber()) noteJournal(journal::Kind::SurvivorLost, ai.name);

    // Killed by you: it weighs, and those who knew them hear of it.
    if (ai.sincePlayerHit < 3.0f && !ai.companion)
    {
        addPanic(20.0f);
        const float numb = mWorld.hasComponent<MentalState>(mPlayerEntity) ? mWorld.getComponent<MentalState>(mPlayerEntity).desensitisation : 0.0f;
        if (numb < 60.0f) addDepression(12.0f);
        mWorld.forEach<SurvivorAI, Transform>(
            [&](Entity other, SurvivorAI& o, Transform& ot)
            {
                if (other == survivor || !o.alive) return;
                if (glm::length(centreOf(ot) - centre) / tileSize > survivorrules::kKilledNearTiles) return;
                o.trust = survivorrules::clampTrust(o.trust + survivorrules::kKilledFriendTrust);
                o.angryDay = mCurrentDay;
            });
        mInteractionMessage = "You killed " + ai.name + ".";
        mInteractionMessageTimer = 4.0f;
    }

    if (ai.companion)
    {
        // Beside you: a grave where they fell, and it stays with you.
        addDepression(20.0f);
        GraveRecord grave;
        grave.name = ai.name;
        grave.x = t.x;
        grave.y = t.y;
        grave.day = mCurrentDay;
        grave.runIndex = mSaveManager.runNumber();
        grave.cause = "Travelled with " + mCurrentCharacterName;
        grave.travelledWith = mCurrentCharacterName;
        if (mWorld.hasComponent<Inventory>(survivor))
        {
            grave.items = InventorySystem::itemsUnloaded(mWorld.getComponent<Inventory>(survivor), mItemDatabase);
        }
        mSaveManager.addGrave(grave);
        mLegacyGraves.push_back(grave);
        mLegacyPackSpawned.push_back(false);
        const int index = static_cast<int>(mLegacyGraves.size()) - 1;
        spawnGrave(index);
        if (!grave.items.empty()) spawnLegacyPack(index, glm::vec2(grave.x + 22.0f, grave.y + 12.0f));
        if (record != nullptr)
        {
            record->buried = true;
            record->companion = false;
            record->stock.clear();
        }
        mInteractionMessage = ai.name + " is dead. You bury them where they fell.";
        mInteractionMessageTimer = 5.0f;
        std::cout << "[Survivors] grave for " << ai.name << ", travelled with " << mCurrentCharacterName << "\n";
        mWorld.destroyEntity(survivor);
        return;
    }
    if (d < 12.0f && ai.sincePlayerHit >= 3.0f)
    {
        mInteractionMessage = ai.name + " is dead.";
        mInteractionMessageTimer = 3.0f;
    }
    // The body stays, with what they had on them.
    if (mWorld.hasComponent<Sprite>(survivor)) mWorld.getComponent<Sprite>(survivor).layer = 3;
}

// ---------------------------------------------------------------------------
// The file

void Game::syncSurvivorRecords()
{
    mWorld.forEach<SurvivorAI, Transform, Health>(
        [&](Entity e, SurvivorAI& ai, Transform& t, Health& h)
        {
            if (ai.id < 0 || ai.id >= static_cast<int>(mSurvivors.size())) return;
            SurvivorRecord& r = mSurvivors[static_cast<std::size_t>(ai.id)];
            r.alive = ai.alive;
            r.x = t.x;
            r.y = t.y;
            r.hunger = ai.hunger;
            r.thirst = ai.thirst;
            r.sleepDebt = ai.sleepDebt;
            r.health = std::max(0.0f, h.current);
            r.wounds = ai.wounds;
            r.trust = ai.trust;
            r.companion = ai.companion;
            r.angryDay = ai.angryDay;
            r.warnedHome = ai.warnedHome;
            r.avoiding = ai.avoiding;
            r.ignoredHours = ai.ignoredHours;
            r.outDay = ai.outDay;
            r.homeBuilding = ai.homeBuilding;
            r.homeX = ai.homeTile.x;
            r.homeY = ai.homeTile.y;
            r.stock.clear();
            if (mWorld.hasComponent<Inventory>(e))
            {
                for (const InventorySlot& slot : mWorld.getComponent<Inventory>(e).slots)
                {
                    if (slot.itemId < 0 || slot.count <= 0) continue;
                    r.stock.push_back(ContainerItemRecord{slot.itemId, slot.count, slot.condition, slot.loaded});
                }
            }
        });
}

void Game::saveSurvivorRecords()
{
    if (mSurvivors.empty()) return;
    syncSurvivorRecords();
    mSaveManager.saveSurvivors(mSurvivors);
}

survivorrules::BetweenLivesResult Game::advanceSurvivors(int days, bool apply)
{
    syncSurvivorRecords();
    const ScavengeMemoryRecord& memory = mSaveManager.scavengeMemory();
    const int base = mSaveManager.baseRecord().building;

    std::vector<int> ids;
    std::vector<survivorrules::LifeSurvivor> people;
    for (const SurvivorRecord& r : mSurvivors)
    {
        if (!r.alive) continue;
        survivorrules::LifeSurvivor s;
        s.id = r.id;
        s.home = glm::ivec2(r.homeX, r.homeY);
        s.homeBuilding = r.homeBuilding;
        s.competence = r.competence;
        const int district = r.homeX >= 0 && mTileMap.inBounds(r.homeX, r.homeY) ? mTileMap.getDistrictId(r.homeX, r.homeY) : 0;
        s.danger = survivorrules::dangerOf(district > 0 ? mDistrictInfectionSystem.getInfection(district) : 0.0f);
        for (const ContainerItemRecord& item : r.stock)
        {
            const ItemDef* def = mItemDatabase.find(item.itemId);
            if (def == nullptr) continue;
            InventorySlot slot;
            slot.itemId = item.itemId;
            slot.condition = item.condition;
            if (edible(*def, slot) && def->category != ItemCategory::Drink) s.food += item.count;
            else if (drinkable(*def, slot)) s.drink += item.count;
        }
        const bool partnerAlive = r.partner >= 0 && r.partner < static_cast<int>(mSurvivors.size()) &&
                                  mSurvivors[static_cast<std::size_t>(r.partner)].alive;
        const bool groupAlive = r.group > 0 && std::any_of(mSurvivors.begin(), mSurvivors.end(), [&](const SurvivorRecord& o)
                                                           { return o.id != r.id && o.alive && o.group == r.group; });
        s.companioned = partnerAlive || groupAlive;
        people.push_back(s);
        ids.push_back(r.id);
    }
    std::vector<survivorrules::LifeContainer> boxes;
    std::vector<const SurvivorContainer*> sources;
    for (const SurvivorContainer& c : mSurvivorContainers)
    {
        if (!mWorld.hasComponent<Inventory>(c.entity)) continue;
        survivorrules::LifeContainer box;
        box.tile = c.tile;
        box.building = c.building;
        box.searched = hasPair(memory.searched, c.tile) || searchedThisRun(c.tile);
        box.protectedHome = c.building > 0 && (survivorLivesIn(c.building) || c.building == base || c.building == mFirstDays.homeBuilding);
        for (const InventorySlot& slot : mWorld.getComponent<Inventory>(c.entity).slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = mItemDatabase.find(slot.itemId);
            if (def == nullptr) continue;
            if (edible(*def, slot) && def->category != ItemCategory::Drink) box.food += slot.count;
            else if (drinkable(*def, slot)) box.drink += slot.count;
            else if (medicine(*def)) box.medicine += slot.count;
        }
        boxes.push_back(box);
        sources.push_back(&c);
    }
    const std::uint32_t seed = mSaveManager.worldSeed() ^ (static_cast<std::uint32_t>(mSaveManager.runNumber()) * 0x9E3779B1u) ^ 0xB17Eu;
    survivorrules::BetweenLivesResult result = survivorrules::advanceBetweenLives(people, boxes, days, seed, 1);
    if (!apply) return result;

    // What they took: the cupboards really empty.
    for (const survivorrules::LootEvent& loot : result.loots)
    {
        const SurvivorContainer& c = *sources[static_cast<std::size_t>(loot.container)];
        SurvivorRecord& r = mSurvivors[static_cast<std::size_t>(ids[static_cast<std::size_t>(loot.survivor)])];
        Inventory& box = mWorld.getComponent<Inventory>(c.entity);
        for (InventorySlot& slot : box.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = mItemDatabase.find(slot.itemId);
            if (def == nullptr || !(edible(*def, slot) || drinkable(*def, slot) || medicine(*def))) continue;
            r.stock.push_back(ContainerItemRecord{slot.itemId, slot.count, slot.condition, slot.loaded});
            slot = InventorySlot{};
        }
        recordContainer(c.entity);
        markSearchedBySurvivor(c.tile);
    }
    // What they ate and drank.
    for (std::size_t i = 0; i < people.size(); ++i)
    {
        SurvivorRecord& r = mSurvivors[static_cast<std::size_t>(ids[i])];
        int food = result.eatenFood[i];
        int drink = result.drunk[i];
        for (ContainerItemRecord& item : r.stock)
        {
            const ItemDef* def = mItemDatabase.find(item.itemId);
            if (def == nullptr) continue;
            InventorySlot slot;
            slot.itemId = item.itemId;
            slot.condition = item.condition;
            const bool isFood = edible(*def, slot) && def->category != ItemCategory::Drink;
            const bool isDrink = !isFood && drinkable(*def, slot);
            int& left = isFood ? food : drink;
            if (!(isFood || isDrink)) continue;
            const int eat = std::min(left, item.count);
            item.count -= eat;
            left -= eat;
        }
        std::erase_if(r.stock, [](const ContainerItemRecord& item) { return item.count <= 0; });
        r.hunger = 70.0f;
        r.thirst = 70.0f;
    }
    // Who did not make it: a grave if someone was left to dig it, else a
    // body at home.
    for (const survivorrules::DeathEvent& death : result.deaths)
    {
        SurvivorRecord& r = mSurvivors[static_cast<std::size_t>(ids[static_cast<std::size_t>(death.survivor)])];
        r.alive = false;
        r.deathDay = death.day;
        r.companion = false;
        const glm::ivec2 home = r.homeX >= 0 ? glm::ivec2(r.homeX, r.homeY) : worldToTile(mTileMap, glm::vec2(r.x, r.y));
        r.x = static_cast<float>(home.x * mTileMap.tileWidth());
        r.y = static_cast<float>(home.y * mTileMap.tileHeight());
        if (people[static_cast<std::size_t>(death.survivor)].companioned)
        {
            std::string by = "the others";
            if (r.partner >= 0 && r.partner < static_cast<int>(mSurvivors.size())) by = mSurvivors[static_cast<std::size_t>(r.partner)].name;
            GraveRecord grave;
            grave.name = r.name;
            grave.x = r.x + 40.0f;
            grave.y = r.y;
            grave.day = death.day;
            grave.runIndex = mSaveManager.runNumber();
            grave.cause = death.starved ? "Starved" : "Died at home";
            grave.travelledWith = by;
            for (const ContainerItemRecord& item : r.stock) grave.items.emplace_back(item.itemId, item.count);
            r.stock.clear();
            r.buried = true;
            mSaveManager.addGrave(grave);
        }
    }
    std::cout << "[Survivors] between lives: " << days << " days, " << result.loots.size() << " containers emptied, "
              << result.deaths.size() << " died, fingerprint " << std::hex << result.fingerprint << std::dec << "\n";
    return result;
}

void Game::survivorsBetweenLives(int days, bool retired)
{
    if (mSurvivors.empty()) return;
    syncSurvivorRecords();
    // A companion goes home (or stays at the base) and remembers you.
    std::vector<bool> wasCompanion(mSurvivors.size(), false);
    for (SurvivorRecord& r : mSurvivors)
    {
        wasCompanion[static_cast<std::size_t>(r.id)] = r.companion;
        if (r.alive && r.companion && r.homeX >= 0)
        {
            r.x = static_cast<float>(r.homeX * mTileMap.tileWidth());
            r.y = static_cast<float>(r.homeY * mTileMap.tileHeight());
        }
        r.companion = false;
    }
    (void)advanceSurvivors(days, true);
    // A new life: they do not know the next one.
    for (SurvivorRecord& r : mSurvivors)
    {
        r.trust = survivorrules::trustForNewLife(r.trust, wasCompanion[static_cast<std::size_t>(r.id)], r.retired);
        r.angryDay = -1;
        r.warnedHome = false;
        r.avoiding = false;
        r.ignoredHours = 0.0f;
        r.helloDay = -1;
        r.outDay = -1;
        r.told.clear();
    }
    std::cout << "[Survivors] a new life (" << (retired ? "retired" : "died") << ")\n";
}

void Game::noteTakenFrom(Entity container)
{
    if (container == kInvalidEntity || mWorld.hasComponent<SurvivorAI>(container) || !mWorld.hasComponent<Transform>(container)) return;
    if (std::find(mTheftNoted.begin(), mTheftNoted.end(), container) != mTheftNoted.end()) return;
    const Transform& ct = mWorld.getComponent<Transform>(container);
    const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(ct.x + 1.0f, ct.y + 1.0f));
    const int building = mTileMap.getBuildingId(tile.x, tile.y);
    if (!survivorLivesIn(building)) return;
    mTheftNoted.push_back(container);
    const glm::vec2 player = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    mWorld.forEach<SurvivorAI, Transform>(
        [&](Entity e, SurvivorAI& ai, Transform& t)
        {
            if (!ai.alive || ai.companion || ai.homeBuilding != building) return;
            ai.trust = survivorrules::clampTrust(ai.trust + survivorrules::kTheftTrust);
            const glm::vec2 centre = centreOf(t);
            const glm::ivec2 at = worldToTile(mTileMap, centre);
            const bool sees = glm::length(centre - player) / tileSize < 8.0f || mTileMap.getBuildingId(at.x, at.y) == building;
            std::cout << "[Survivors] theft from " << ai.name << (sees ? " (seen)" : "") << "\n";
            if (sees && !ai.far)
            {
                ai.angryDay = mCurrentDay;
                ai.state = SurvivorState::Attack;
                ai.path.clear();
                survivorSays(e, "hurt", 3u);
            }
        });
}

// ---------------------------------------------------------------------------
// Talk

bool Game::talkToSurvivor()
{
    const glm::vec2 player = playerWorldCenter();
    const float reach = (survivorrules::kTalkTiles + 0.25f) * static_cast<float>(std::max(1, mTileMap.tileWidth()));
    Entity best = kInvalidEntity;
    float bestDistance = reach;
    mWorld.forEach<SurvivorAI, Transform>(
        [&](Entity e, SurvivorAI& ai, Transform& t)
        {
            if (!ai.alive && ai.buried) return;
            if (ai.alive && ai.state == SurvivorState::Sleep && ai.actionTimer > 0.0f) return; // asleep
            const float d = glm::length(centreOf(t) - player);
            if (d <= bestDistance)
            {
                best = e;
                bestDistance = d;
            }
        });
    if (best == kInvalidEntity) return false;
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(best);
    if (!ai.alive)
    {
        // A body comes after anything else within reach.
        if (!mProximityPrompt.empty()) return false;
        // Emptied, they can be buried where they lie (phase 10).
        bool empty = true;
        if (mWorld.hasComponent<Inventory>(best))
        {
            for (const InventorySlot& slot : mWorld.getComponent<Inventory>(best).slots)
            {
                if (slot.itemId >= 0 && slot.count > 0) empty = false;
            }
        }
        mProximityPrompt = empty ? "E: Bury " + ai.name + " (30 s)" : "E: Search " + ai.name + "'s body";
        if (!mInput.wasKeyPressed(SDL_SCANCODE_E)) return false;
        if (empty) beginBurial(best);
        else openContainer(best);
        return true;
    }
    const bool hostile = !ai.companion && (ai.attitude == Attitude::Hostile || ai.angryDay == mCurrentDay);
    if (ai.state == SurvivorState::Flee || ai.state == SurvivorState::Attack) return false;
    mProximityPrompt = "E: Talk to " + ai.name;
    if (!mInput.wasKeyPressed(SDL_SCANCODE_E)) return false;
    if (hostile)
    {
        survivorSays(best, "hello_hostile");
        return true;
    }
    openTalk(best);
    return true;
}

void Game::openTalk(Entity survivor)
{
    mTalk = TalkState{};
    mTalk.open = true;
    mTalk.who = survivor;
    mShowInventory = false;
    mShowCrafting = false;
    closeContainer();
    if (mWorld.hasComponent<Velocity>(mPlayerEntity)) mWorld.getComponent<Velocity>(mPlayerEntity) = Velocity{};
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(survivor);
    if (ai.companion) mTalk.line = survivorLine("companion_come", ai.name, 1u);
    else mTalk.line = survivorLine(std::string("hello_") + survivorrules::attitudeName(ai.attitude), ai.name, 0u);
    std::cout << "[Survivors] talking to " << ai.name << " (" << survivorrules::attitudeName(ai.attitude) << ", trust "
              << static_cast<int>(ai.trust) << ")\n";
}

void Game::closeTalk()
{
    mTalk = TalkState{};
}

std::vector<std::string> Game::talkChoices() const
{
    std::vector<std::string> out;
    if (!mTalk.open || !mWorld.hasComponent<SurvivorAI>(mTalk.who)) return out;
    const SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(mTalk.who);
    if (ai.companion)
    {
        out.emplace_back(ai.waiting ? "Come" : "Wait here");
        out.emplace_back("Carry this");
        out.emplace_back("Give");
        const glm::ivec2 here = playerTile();
        const int building = mTileMap.inBounds(here.x, here.y) && mTileMap.isIndoors(here.x, here.y) ? mTileMap.getBuildingId(here.x, here.y) : 0;
        if (building > 0 && (building == mFirstDays.homeBuilding || building == mSaveManager.baseRecord().building)) out.emplace_back("This is home now");
        out.emplace_back("Leave");
        return out;
    }
    out.emplace_back("Say hello");
    out.emplace_back("Trade");
    out.emplace_back("Give");
    const bool canTravel = ai.role == Role::Loner || ai.role == Role::Retired || (ai.role == Role::Pair && !ai.stayer);
    if (canTravel && companionEntity() == kInvalidEntity) out.emplace_back("Ask them along");
    out.emplace_back("Leave");
    return out;
}

void Game::talkChoose(int choice)
{
    const std::vector<std::string> choices = talkChoices();
    if (choice < 0 || choice >= static_cast<int>(choices.size())) return;
    const Entity who = mTalk.who;
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(who);
    SurvivorRecord& record = mSurvivors[static_cast<std::size_t>(ai.id)];
    const std::string& pick = choices[static_cast<std::size_t>(choice)];
    const bool lonerTrait = playerTraits() != nullptr && playerTraits()->has(NegativeTrait::Loner);
    std::cout << "[Survivors] you: " << pick << "\n";
    if (pick == "Say hello")
    {
        if (record.helloDay != mCurrentDay)
        {
            record.helloDay = mCurrentDay;
            ai.trust = survivorrules::clampTrust(ai.trust + survivorrules::trustChange(survivorrules::kHelloTrust, lonerTrait));
            // One of the bloodline teaches what they knew.
            if (record.retired)
            {
                for (const CraftingRecipe& recipe : mCraftingSystem.recipes())
                {
                    if (recipe.startsKnown || mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB)) continue;
                    mSaveManager.addDiscoveredRecipe(recipe.inputA, recipe.inputB);
                    const ItemDef* a = mItemDatabase.find(recipe.inputA);
                    const ItemDef* b = recipe.inputB >= 0 ? mItemDatabase.find(recipe.inputB) : nullptr;
                    const ItemDef* made = mItemDatabase.find(recipe.outputItemId);
                    mTalk.line = "Watch. " + std::string(made != nullptr ? made->name : "It") + ", from " +
                                 lowerText(a != nullptr ? a->name : "this") +
                                 (b != nullptr ? " and " + lowerText(b->name) : std::string()) + ".";
                    std::cout << "[Survivors] " << ai.name << " taught a recipe\n";
                    return;
                }
            }
        }
        // What they know of the way out comes before the gossip (phase 9).
        if (const std::string gateLine = survivorGateLead(record, ai.trust, record.competence); !gateLine.empty())
        {
            mTalk.line = gateLine;
            return;
        }
        mTalk.line = survivorLead(who);
        return;
    }
    if (pick == "Trade")
    {
        if (!survivorrules::willTrade(ai.attitude, ai.trust))
        {
            mTalk.line = survivorLine("refuse", ai.name, 11u);
            return;
        }
        mTalk.view = TalkState::View::Trade;
        mTalk.packSide = true;
        mTalk.packCursor = 0;
        mTalk.theirCursor = 0;
        mTalk.offer.assign(Inventory::kMaxSlots, 0);
        mTalk.ask.assign(Inventory::kMaxSlots, 0);
        return;
    }
    if (pick == "Give")
    {
        mTalk.view = TalkState::View::Give;
        mTalk.packCursor = 0;
        return;
    }
    if (pick == "Ask them along")
    {
        if (ai.trust >= survivorrules::kAskAlongTrust)
        {
            ai.companion = true;
            ai.waiting = false;
            ai.state = SurvivorState::Follow;
            ai.path.clear();
            record.companion = true;
            if (mWorld.hasComponent<Inventory>(who)) mWorld.getComponent<Inventory>(who).maxWeight = kCompanionWeight;
            mTalk.line = survivorLine("companion_join", ai.name, 0u);
            mInteractionMessage = ai.name + " comes with you.";
            mInteractionMessageTimer = 3.0f;
            std::cout << "[Survivors] " << ai.name << " is your companion\n";
        }
        else
        {
            mTalk.line = survivorLine("companion_refuse", ai.name, 0u);
        }
        return;
    }
    if (pick == "Wait here")
    {
        ai.waiting = true;
        ai.state = SurvivorState::Wait;
        mTalk.line = survivorLine("companion_wait", ai.name, 0u);
        return;
    }
    if (pick == "Come")
    {
        ai.waiting = false;
        ai.state = SurvivorState::Follow;
        mTalk.line = survivorLine("companion_come", ai.name, 0u);
        return;
    }
    if (pick == "Carry this")
    {
        closeTalk();
        openContainer(who);
        return;
    }
    if (pick == "This is home now")
    {
        const glm::ivec2 here = playerTile();
        const int building = mTileMap.getBuildingId(here.x, here.y);
        ai.homeBuilding = building;
        ai.homeTile = homeTileOf(building).x >= 0 ? homeTileOf(building) : here;
        ai.companion = false;
        ai.waiting = false;
        ai.state = SurvivorState::Idle;
        record.companion = false;
        record.homeBuilding = building;
        record.homeKnown = true;
        mSurvivorHomeDoors.erase(building);
        if (mWorld.hasComponent<Inventory>(who)) mWorld.getComponent<Inventory>(who).maxWeight = kStockWeight;
        mTalk.line = survivorLine("companion_home", ai.name, 0u);
        std::cout << "[Survivors] " << ai.name << " lives at your base now (building " << building << ")\n";
        return;
    }
    // Leave.
    survivorSays(who, "goodbye");
    closeTalk();
}

std::string Game::survivorSeems(Entity survivor) const
{
    const SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(survivor);
    const Health* h = mWorld.hasComponent<Health>(survivor) ? &mWorld.getComponent<Health>(survivor) : nullptr;
    const char* body = "well";
    if (ai.hunger < 40.0f || ai.thirst < 40.0f) body = "hungry";
    else if (ai.wounds > 0 || (h != nullptr && h->current < h->maximum * 0.6f)) body = "hurt";
    else if (ai.sleepDebt > 60.0f) body = "tired";
    const std::string manner = survivorrules::attitudeName(ai.attitude);
    const std::string a = survivorLine(std::string("seems_body.") + body, ai.name, 5u);
    const std::string b = survivorLine("seems_manner." + manner, ai.name, 6u);
    return a + (a.empty() || b.empty() ? "" : " ") + b;
}

float Game::survivorWorth(Entity survivor, int itemId) const
{
    const ItemDef* def = mItemDatabase.find(itemId);
    if (def == nullptr || !mWorld.hasComponent<SurvivorAI>(survivor)) return 0.0f;
    const SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(survivor);
    survivorrules::Wants wants;
    wants.hungry = ai.hunger < survivorrules::kHungryBelow;
    wants.thirsty = ai.thirst < survivorrules::kHungryBelow;
    const Health* h = mWorld.hasComponent<Health>(survivor) ? &mWorld.getComponent<Health>(survivor) : nullptr;
    wants.hurt = ai.wounds > 0 || (h != nullptr && h->current < h->maximum * 0.6f);
    return survivorrules::worth(def->value, itemKind(*def), wants);
}

std::vector<int> Game::survivorSpare(Entity survivor) const
{
    return survivorrules::spareCounts(survivorStockLines(survivor));
}

std::string Game::survivorLead(Entity survivor)
{
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(survivor);
    SurvivorRecord& record = mSurvivors[static_cast<std::size_t>(ai.id)];
    const glm::vec2 player = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const LayoutData& layout = mMapGenerator.layoutData();
    const auto told = [&](const std::string& key) { return std::find(record.told.begin(), record.told.end(), key) != record.told.end(); };
    const std::size_t start = survivorrules::lineIndex(ai.name, mCurrentDay, 0x1EADu, 3);
    for (std::size_t k = 0; k < 3; ++k)
    {
        const std::size_t kind = (start + k) % 3;
        if (kind == 0)
        {
            // Someone else, out there.
            for (SurvivorRecord& other : mSurvivors)
            {
                if (other.id == record.id || !other.alive || other.homeBuilding <= 0 || other.homeKnown) continue;
                const std::string key = "person:" + std::to_string(other.id);
                if (told(key)) continue;
                record.told.push_back(key);
                other.met = true;
                other.metRun = mSaveManager.runNumber();
                other.homeKnown = true;
                const glm::vec2 home = tileCentre(mTileMap, glm::ivec2(other.homeX, other.homeY));
                survivorrules::Personality p{other.aggression, other.cooperation, other.competence};
                const char* standing = survivorrules::standingWord(other.trust, survivorrules::attitude(p, 0.0f, other.trust, false));
                std::string line = survivorLine("lead_person", ai.name, static_cast<std::uint32_t>(other.id));
                line = replaceAll(line, "{name}", other.name);
                line = replaceAll(line, "{dir}", directionWords(legacy::screenDirection(player, home)));
                return replaceAll(line, "{standing}", standing);
            }
        }
        else if (kind == 1)
        {
            // A shop nobody has been through.
            int best = 0;
            float bestDistance = 0.0f;
            for (std::size_t i = 0; i < layout.buildings.size(); ++i)
            {
                const int building = static_cast<int>(i) + 1;
                const BuildingKind bk = buildingkinds::kindOf(layout.buildings[i]);
                if (bk == BuildingKind::House || told("supplies:" + std::to_string(building))) continue;
                int unopened = 0;
                for (const SurvivorContainer& c : mSurvivorContainers)
                {
                    if (c.building == building && !hasPair(mSaveManager.scavengeMemory().searched, c.tile)) ++unopened;
                }
                if (unopened < 2) continue;
                const glm::vec2 at = tileCentre(mTileMap, homeTileOf(building));
                const float d = glm::length(at - tileCentre(mTileMap, ai.homeTile)) / tileSize;
                if (d > 60.0f) continue;
                if (best == 0 || d < bestDistance)
                {
                    best = building;
                    bestDistance = d;
                }
            }
            if (best > 0)
            {
                record.told.push_back("supplies:" + std::to_string(best));
                ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
                if (std::find(memory.seenBuildings.begin(), memory.seenBuildings.end(), best) == memory.seenBuildings.end())
                {
                    memory.seenBuildings.push_back(best); // on the map now
                    mSaveManager.setScavengeMemory(std::move(memory));
                    mSaveManager.saveWorldMemory();
                }
                const glm::vec2 at = tileCentre(mTileMap, homeTileOf(best));
                std::string line = survivorLine("lead_supplies", ai.name, static_cast<std::uint32_t>(best));
                line = replaceAll(line, "{kind}", lowerText(buildingkinds::name(buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(best - 1)]))));
                return replaceAll(line, "{dir}", directionWords(legacy::screenDirection(player, at)));
            }
        }
        else
        {
            // Where the dead are thick.
            int worst = 0;
            float worstInfection = 40.0f;
            glm::vec2 worstAt(0.0f);
            for (const auto& district : layout.districts)
            {
                const float infection = mDistrictInfectionSystem.getInfection(district.id);
                if (infection <= worstInfection || told("danger:" + std::to_string(district.id))) continue;
                worst = district.id;
                worstInfection = infection;
                worstAt = glm::vec2((static_cast<float>(district.minX + district.maxX) * 0.5f) * tileSize,
                                    (static_cast<float>(district.minY + district.maxY) * 0.5f) * tileSize);
            }
            if (worst > 0)
            {
                record.told.push_back("danger:" + std::to_string(worst));
                const std::string line = survivorLine("lead_danger", ai.name, static_cast<std::uint32_t>(worst));
                return replaceAll(line, "{dir}", directionWords(legacy::screenDirection(player, worstAt)));
            }
        }
    }
    return survivorLine("lead_none", ai.name, 0u);
}

void Game::tradeAgree()
{
    const Entity who = mTalk.who;
    if (!mWorld.hasComponent<Inventory>(who) || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(who);
    Inventory& pack = mWorld.getComponent<Inventory>(mPlayerEntity);
    Inventory& theirs = mWorld.getComponent<Inventory>(who);
    float given = 0.0f;
    float taken = 0.0f;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const std::size_t s = static_cast<std::size_t>(i);
        if (mTalk.offer[s] > 0) given += survivorWorth(who, pack.slots[s].itemId) * static_cast<float>(mTalk.offer[s]);
        if (mTalk.ask[s] > 0) taken += survivorWorth(who, theirs.slots[s].itemId) * static_cast<float>(mTalk.ask[s]);
    }
    const survivorrules::Verdict v = survivorrules::verdict(given, taken);
    std::cout << "[Survivors] trade with " << ai.name << ": you give " << given << ", take " << taken << " ("
              << survivorrules::verdictText(v) << ")\n";
    if (v == survivorrules::Verdict::WantMore)
    {
        mTalk.line = survivorLine("refuse", ai.name, 13u);
        return;
    }
    // Swap through a holding inventory so slots do not shift under us.
    Inventory outgoing{};
    Inventory incoming{};
    outgoing.maxWeight = incoming.maxWeight = 1000.0f;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const std::size_t s = static_cast<std::size_t>(i);
        if (mTalk.offer[s] > 0) storage::moveItem(pack, i, outgoing, mItemDatabase, mTalk.offer[s]);
        if (mTalk.ask[s] > 0) storage::moveItem(theirs, i, incoming, mItemDatabase, mTalk.ask[s]);
    }
    int overflow = 0;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        if (incoming.slots[static_cast<std::size_t>(i)].count > 0)
        {
            const int count = incoming.slots[static_cast<std::size_t>(i)].count;
            overflow += count - storage::moveItem(incoming, i, pack, mItemDatabase, count);
        }
        if (outgoing.slots[static_cast<std::size_t>(i)].count > 0)
        {
            storage::moveItem(outgoing, i, theirs, mItemDatabase, outgoing.slots[static_cast<std::size_t>(i)].count);
        }
    }
    // What would not fit goes back to them.
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        if (incoming.slots[static_cast<std::size_t>(i)].count > 0)
            storage::moveItem(incoming, i, theirs, mItemDatabase, incoming.slots[static_cast<std::size_t>(i)].count);
    }
    const bool lonerTrait = playerTraits() != nullptr && playerTraits()->has(NegativeTrait::Loner);
    ai.trust = survivorrules::clampTrust(ai.trust + survivorrules::trustChange(survivorrules::verdictTrust(v), lonerTrait));
    mTalk.offer.assign(Inventory::kMaxSlots, 0);
    mTalk.ask.assign(Inventory::kMaxSlots, 0);
    mTalk.packCursor = 0;
    mTalk.theirCursor = 0;
    mTalk.line = survivorLine("thank", ai.name, 17u);
    noteJournal(journal::Kind::SurvivorTraded, ai.name);
    if (overflow > 0)
    {
        mInteractionMessage = "No room in your pack for all of it.";
        mInteractionMessageTimer = 2.5f;
    }
    mAudioManager.playSound(SoundId::WeaponHit, 0.3f);
}

void Game::handleTalkInput()
{
    if (!mTalk.open) return;
    if (mTalk.who == kInvalidEntity || !mWorld.hasComponent<SurvivorAI>(mTalk.who) || !mWorld.getComponent<SurvivorAI>(mTalk.who).alive ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        closeTalk();
        return;
    }
    const float d = glm::length(centreOf(mWorld.getComponent<Transform>(mTalk.who)) - playerWorldCenter()) /
                    static_cast<float>(std::max(1, mTileMap.tileWidth()));
    if (d > 4.0f)
    {
        closeTalk();
        return;
    }
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(mTalk.who);
    ai.faceTo = playerWorldCenter() - centreOf(mWorld.getComponent<Transform>(mTalk.who));
    const bool up = mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W);
    const bool down = mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S);
    const bool pick = mInput.wasKeyPressed(SDL_SCANCODE_E) || mInput.wasKeyPressed(SDL_SCANCODE_RETURN);
    const bool shift = mInput.isKeyDown(SDL_SCANCODE_LSHIFT) || mInput.isKeyDown(SDL_SCANCODE_RSHIFT);
    if (mInput.wasKeyPressed(SDL_SCANCODE_TAB))
    {
        if (mTalk.view != TalkState::View::Talk) mTalk.view = TalkState::View::Talk;
        else closeTalk();
        return;
    }

    if (mTalk.view == TalkState::View::Talk)
    {
        const int n = static_cast<int>(talkChoices().size());
        if (n <= 0) return;
        if (up) mTalk.cursor = (mTalk.cursor + n - 1) % n;
        if (down) mTalk.cursor = (mTalk.cursor + 1) % n;
        mTalk.cursor = std::clamp(mTalk.cursor, 0, n - 1);
        static constexpr SDL_Scancode kNumbers[5] = {SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5};
        for (int i = 0; i < std::min(n, 5); ++i)
        {
            if (mInput.wasKeyPressed(kNumbers[i]))
            {
                mTalk.cursor = i;
                talkChoose(i);
                return;
            }
        }
        if (pick) talkChoose(mTalk.cursor);
        return;
    }

    Inventory& pack = mWorld.getComponent<Inventory>(mPlayerEntity);
    if (mTalk.view == TalkState::View::Give)
    {
        const int rows = storage::filledSlots(pack);
        if (rows <= 0) return;
        if (up) mTalk.packCursor = (mTalk.packCursor + rows - 1) % rows;
        if (down) mTalk.packCursor = (mTalk.packCursor + 1) % rows;
        mTalk.packCursor = std::clamp(mTalk.packCursor, 0, rows - 1);
        if (pick && mWorld.hasComponent<Inventory>(mTalk.who))
        {
            const int slot = storage::nthFilledSlot(pack, mTalk.packCursor);
            if (slot < 0) return;
            const int itemId = pack.slots[static_cast<std::size_t>(slot)].itemId;
            const float w = survivorWorth(mTalk.who, itemId);
            Inventory& theirs = mWorld.getComponent<Inventory>(mTalk.who);
            const float before = theirs.maxWeight;
            theirs.maxWeight = std::max(theirs.maxWeight, InventorySystem::totalWeight(theirs, mItemDatabase) + 5.0f);
            const int moved = storage::moveItem(pack, slot, theirs, mItemDatabase, 1);
            theirs.maxWeight = before;
            if (moved <= 0) return;
            const bool lonerTrait = playerTraits() != nullptr && playerTraits()->has(NegativeTrait::Loner);
            ai.trust = survivorrules::clampTrust(ai.trust + survivorrules::trustChange(survivorrules::giftTrust(w), lonerTrait));
            ai.ignoredHours = 0.0f;
            mTalk.line = survivorLine("thank", ai.name, static_cast<std::uint32_t>(itemId));
            const ItemDef* def = mItemDatabase.find(itemId);
            std::cout << "[Survivors] gave " << ai.name << " " << (def != nullptr ? def->name : "?") << " (trust "
                      << static_cast<int>(ai.trust) << ")\n";
            const int left = storage::filledSlots(pack);
            mTalk.packCursor = std::clamp(mTalk.packCursor, 0, std::max(0, left - 1));
        }
        return;
    }

    // Trade.
    if (!mWorld.hasComponent<Inventory>(mTalk.who)) return;
    const Inventory& theirs = mWorld.getComponent<Inventory>(mTalk.who);
    const std::vector<int> spare = survivorSpare(mTalk.who);
    std::vector<int> theirRows;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        if (theirs.slots[static_cast<std::size_t>(i)].count > 0 && spare[static_cast<std::size_t>(i)] > 0) theirRows.push_back(i);
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_A) || mInput.wasKeyPressed(SDL_SCANCODE_LEFT)) mTalk.packSide = true;
    if (mInput.wasKeyPressed(SDL_SCANCODE_D) || mInput.wasKeyPressed(SDL_SCANCODE_RIGHT)) mTalk.packSide = false;
    const int rows = mTalk.packSide ? storage::filledSlots(pack) : static_cast<int>(theirRows.size());
    int& cursor = mTalk.packSide ? mTalk.packCursor : mTalk.theirCursor;
    if (rows > 0)
    {
        if (up) cursor = (cursor + rows - 1) % rows;
        if (down) cursor = (cursor + 1) % rows;
        cursor = std::clamp(cursor, 0, rows - 1);
        const int slot = mTalk.packSide ? storage::nthFilledSlot(pack, cursor) : theirRows[static_cast<std::size_t>(cursor)];
        if (slot >= 0)
        {
            const std::size_t s = static_cast<std::size_t>(slot);
            const int most = mTalk.packSide ? pack.slots[s].count : spare[s];
            int& count = mTalk.packSide ? mTalk.offer[s] : mTalk.ask[s];
            if (mInput.wasKeyPressed(SDL_SCANCODE_E)) count = shift ? most : std::min(most, count + 1);
            if (mInput.wasKeyPressed(SDL_SCANCODE_BACKSPACE)) count = shift ? 0 : std::max(0, count - 1);
        }
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_RETURN)) tradeAgree();
}

// ---------------------------------------------------------------------------
// Drawing

void Game::renderTalkPanel()
{
    if (!mTalk.open || mRunState != RunState::Playing || !mWorld.hasComponent<SurvivorAI>(mTalk.who) ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }
    const SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(mTalk.who);
    const bool trade = mTalk.view == TalkState::View::Trade;
    const float panelW = dockedPanelWidth(trade, mWindowWidth);
    const float panelX = static_cast<float>(mWindowWidth) - panelW;
    const float panelH = static_cast<float>(mWindowHeight);
    const float pad = ContainerLayout::kPadding;
    const float left = panelX + pad;
    const float right = panelX + panelW - pad;
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const auto rect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) { mSpriteBatch.draw(pos, size, white, color); };
    const auto text = [&](const glm::vec2& pos, const std::string& s, const glm::vec4& color, float scale, TextAlign align = TextAlign::Left)
    { mFont.drawText(mSpriteBatch, pos, s, color, scale, align); };
    const auto fit = [&](const std::string& s, float width, float scale)
    { return truncateHudText(s, static_cast<std::size_t>(std::max(4.0f, width / (6.0f * scale)))); };

    rect(glm::vec2(panelX, 0.0f), glm::vec2(panelW, panelH), kPanelBack);
    text(glm::vec2(left, 22.0f), fit(ai.name, panelW * 0.55f, 3.0f), kPanelInk, 3.0f);
    const std::string standing = ai.companion ? "With you." : survivorrules::standingWord(ai.trust, ai.attitude);
    text(glm::vec2(right, 30.0f), standing, ai.attitude == Attitude::Hostile ? kPanelBad : kPanelDim, 2.0f, TextAlign::Right);
    rect(glm::vec2(left, 64.0f), glm::vec2(panelW - pad * 2.0f, 1.0f), kPanelRule);

    const Inventory& pack = mWorld.getComponent<Inventory>(mPlayerEntity);
    const auto itemName = [&](int itemId)
    {
        const ItemDef* def = mItemDatabase.find(itemId);
        return def != nullptr ? def->name : "Item #" + std::to_string(itemId);
    };

    if (mTalk.view == TalkState::View::Talk)
    {
        const std::size_t chars = static_cast<std::size_t>(std::max(10.0f, (panelW - pad * 2.0f) / 12.0f));
        float y = 78.0f;
        for (const std::string& line : wrapText(survivorSeems(mTalk.who), chars))
        {
            text(glm::vec2(left, y), line, kPanelDim, 2.0f);
            y += 22.0f;
        }
        y += 16.0f;
        if (!mTalk.line.empty())
        {
            for (const std::string& line : wrapText("\"" + mTalk.line + "\"", chars))
            {
                text(glm::vec2(left, y), line, kPanelInk, 2.0f);
                y += 24.0f;
            }
        }
        y = std::max(y + 20.0f, 200.0f);
        rect(glm::vec2(left, y - 12.0f), glm::vec2(panelW - pad * 2.0f, 1.0f), kPanelRule);
        const std::vector<std::string> choices = talkChoices();
        for (std::size_t i = 0; i < choices.size(); ++i)
        {
            const bool selected = static_cast<int>(i) == mTalk.cursor;
            if (selected) rect(glm::vec2(left - 4.0f, y - 3.0f), glm::vec2(panelW - pad * 2.0f + 8.0f, ContainerLayout::kRowH), kPanelInk);
            text(glm::vec2(left, y), std::to_string(i + 1) + "  " + choices[i], selected ? kPanelOnInk : kPanelInk, 2.0f);
            y += 28.0f;
        }
        text(glm::vec2(left, panelH - 26.0f), "W/S choose   E or 1-5 pick   Esc close", kPanelDim, 1.0f);
        return;
    }

    if (mTalk.view == TalkState::View::Give)
    {
        text(glm::vec2(left, 78.0f), "GIVE", kPanelAmber, 2.0f);
        if (!mTalk.line.empty()) text(glm::vec2(right, 82.0f), fit("\"" + mTalk.line + "\"", panelW * 0.6f, 1.0f), kPanelDim, 1.0f, TextAlign::Right);
        rect(glm::vec2(left, 100.0f), glm::vec2(panelW - pad * 2.0f, 1.0f), kPanelRule);
        float y = ContainerLayout::kListY;
        int row = 0;
        for (const InventorySlot& slot : pack.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            if (y + ContainerLayout::kRowH > panelH - 40.0f) break;
            const bool selected = row == mTalk.packCursor;
            if (selected) rect(glm::vec2(left - 4.0f, y - 3.0f), glm::vec2(panelW - pad * 2.0f + 8.0f, ContainerLayout::kRowH), kPanelInk);
            const glm::vec4 ink = selected ? kPanelOnInk : kPanelInk;
            text(glm::vec2(left, y), fit(itemName(slot.itemId), panelW - pad * 2.0f - 60.0f, 2.0f), ink, 2.0f);
            if (slot.count > 1) text(glm::vec2(right, y), "x" + std::to_string(slot.count), selected ? kPanelOnInk : kPanelDim, 2.0f, TextAlign::Right);
            y += ContainerLayout::kRowH;
            ++row;
        }
        if (row == 0) text(glm::vec2(left, y), "Nothing to give", kPanelDim, 2.0f);
        text(glm::vec2(left, panelH - 26.0f), "E give one   W/S choose   Esc back", kPanelDim, 1.0f);
        return;
    }

    // Trade: your pack, what they will part with, and what they make of it.
    const Inventory& theirs = mWorld.getComponent<Inventory>(mTalk.who);
    const std::vector<int> spare = survivorSpare(mTalk.who);
    const float columnW = (panelW - pad * 3.0f) * 0.5f;
    const float packX = left;
    const float theirX = packX + columnW + pad;
    const float listBottom = panelH - 120.0f;
    const auto column = [&](float x, const std::string& title, bool active, bool mine)
    {
        text(glm::vec2(x, 78.0f), title, active ? kPanelAmber : kPanelDim, 2.0f);
        rect(glm::vec2(x, 100.0f), glm::vec2(columnW, 1.0f), kPanelRule);
        float y = ContainerLayout::kListY;
        int row = 0;
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            const std::size_t s = static_cast<std::size_t>(i);
            const InventorySlot& slot = mine ? pack.slots[s] : theirs.slots[s];
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const int available = mine ? slot.count : spare[s];
            if (available <= 0) continue;
            if (y + ContainerLayout::kRowH > listBottom) break;
            const int chosen = mine ? mTalk.offer[s] : mTalk.ask[s];
            const bool selected = active && row == (mine ? mTalk.packCursor : mTalk.theirCursor);
            if (selected) rect(glm::vec2(x - 4.0f, y - 3.0f), glm::vec2(columnW + 8.0f, ContainerLayout::kRowH), kPanelInk);
            const glm::vec4 ink = selected ? kPanelOnInk : kPanelInk;
            const std::string tail = (chosen > 0 ? std::to_string(chosen) + " of " : std::string()) + std::to_string(available);
            const float tailW = mFont.measureText(tail, 2.0f).x;
            text(glm::vec2(x, y), fit(itemName(slot.itemId), columnW - tailW - 12.0f, 2.0f), ink, 2.0f);
            text(glm::vec2(x + columnW, y), tail, selected ? kPanelOnInk : (chosen > 0 ? kPanelAmber : kPanelDim), 2.0f, TextAlign::Right);
            y += ContainerLayout::kRowH;
            ++row;
        }
        if (row == 0) text(glm::vec2(x, y), mine ? "Empty" : "Nothing to spare", kPanelDim, 2.0f);
    };
    column(packX, "YOUR PACK", mTalk.packSide, true);
    column(theirX, "THEY WILL PART WITH", !mTalk.packSide, false);
    rect(glm::vec2(theirX - pad * 0.5f, 78.0f), glm::vec2(1.0f, listBottom - 78.0f), kPanelRule);

    float given = 0.0f;
    float taken = 0.0f;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const std::size_t s = static_cast<std::size_t>(i);
        if (mTalk.offer[s] > 0) given += survivorWorth(mTalk.who, pack.slots[s].itemId) * static_cast<float>(mTalk.offer[s]);
        if (mTalk.ask[s] > 0) taken += survivorWorth(mTalk.who, theirs.slots[s].itemId) * static_cast<float>(mTalk.ask[s]);
    }
    const bool any = given > 0.0f || taken > 0.0f;
    const survivorrules::Verdict v = survivorrules::verdict(given, taken);
    const float barY = listBottom + 18.0f;
    rect(glm::vec2(left, barY - 10.0f), glm::vec2(panelW - pad * 2.0f, 1.0f), kPanelRule);
    const glm::vec4 verdictInk = !any ? kPanelDim : v == survivorrules::Verdict::WantMore ? kPanelBad : v == survivorrules::Verdict::Fair ? kPanelInk : kPanelGood;
    text(glm::vec2(left, barY), any ? survivorrules::verdictText(v) : "Put something on each side", verdictInk, 2.0f);
    // The bar: what you give against what you take (half way is even).
    const float trackX = left;
    const float trackW = panelW - pad * 2.0f;
    const float trackY = barY + 30.0f;
    rect(glm::vec2(trackX, trackY), glm::vec2(trackW, 6.0f), kPanelTrack);
    const float share = any ? given / std::max(0.01f, given + taken) : 0.0f;
    rect(glm::vec2(trackX, trackY), glm::vec2(std::round(trackW * share), 6.0f), verdictInk);
    rect(glm::vec2(trackX + std::round(trackW * 0.5f), trackY - 3.0f), glm::vec2(1.0f, 12.0f), kPanelInk);
    if (!mTalk.line.empty()) text(glm::vec2(right, barY + 4.0f), fit("\"" + mTalk.line + "\"", trackW * 0.5f, 1.0f), kPanelDim, 1.0f, TextAlign::Right);
    text(glm::vec2(left, panelH - 26.0f), "E add   Shift+E all   Bksp take back   A/D side   Enter agree   Esc back", kPanelDim, 1.0f);
}

void Game::renderSurvivorSay()
{
    if (mRunState != RunState::Playing) return;
    const glm::vec2 player = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float zoom = mCamera.zoom();
    mWorld.forEach<SurvivorAI, Transform>(
        [&](Entity, SurvivorAI& ai, Transform& t)
        {
            if (!ai.alive || ai.sayTimer <= 0.0f || ai.say.empty()) return;
            const glm::vec2 centre = centreOf(t);
            if (glm::length(centre - player) / tileSize > kSayRangeTiles) return;
            const glm::ivec2 tile = worldToTile(mTileMap, centre);
            if (mStructureView.isHiddenAt(tile.x, tile.y)) return;
            const float alpha = std::min(1.0f, ai.sayTimer / 0.6f);
            const glm::vec2 screen = mCamera.worldToScreen(centre);
            const glm::vec2 pos(std::round(screen.x), std::round(screen.y - 70.0f * zoom - 14.0f));
            mFont.drawText(mSpriteBatch, pos + glm::vec2(1.0f, 1.0f), ai.say, glm::vec4(0.0f, 0.0f, 0.0f, 0.75f * alpha), 1.0f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, pos, ai.say, glm::vec4(kPanelInk.r, kPanelInk.g, kPanelInk.b, alpha), 1.0f, TextAlign::Center);
        });
}

void Game::updateSurvivorSmoke(float dtSeconds)
{
    // A fire for a meal: a thin column over the house by day.
    mSurvivorSmokeTimer += dtSeconds;
    if (mSurvivorSmokeTimer < 0.5f) return;
    mSurvivorSmokeTimer = 0.0f;
    const double h = mGameHours;
    const bool meal = (h >= 7.0 && h < 9.0) || (h >= 12.0 && h < 13.0) || (h >= 17.0 && h < 19.0);
    if (!meal) return;
    const LayoutData& layout = mMapGenerator.layoutData();
    const glm::vec2 player = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    std::vector<int> done;
    for (const SurvivorRecord& r : mSurvivors)
    {
        if (!r.alive || r.companion || r.homeBuilding <= 0 || r.homeBuilding > static_cast<int>(layout.buildings.size())) continue;
        if (std::find(done.begin(), done.end(), r.homeBuilding) != done.end()) continue;
        done.push_back(r.homeBuilding);
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(r.homeBuilding - 1)];
        const glm::vec2 chimney((static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f) * tileSize,
                                (static_cast<float>(b.y) + 1.0f) * tileSize);
        if (glm::length(chimney - player) / tileSize > 50.0f) continue;
        mParticleSystem.spawnSmoke(chimney, 1);
    }
}
