#pragma once

#include <array>
#include <vector>

#include <glm/vec2.hpp>

#include "components/Structure.h"
#include "map/TileMap.h"

// What the build wheel (B) offers: stations made where they stand, from
// materials and tools in the pack (03_content_tables.md, section 9). The
// old build mode and its flat props are gone; this holds the recipes and
// the rule for where each may stand. The game slice is core/GameBase.cpp.
struct StationRecipe
{
    StructureType type = StructureType::Campfire;
    const char* name = "";
    const char* materialKey = "";  // item consumed, by key
    int materialCount = 0;
    const char* secondKey = nullptr; // a second material, or none
    int secondCount = 0;
    bool needsHammer = false;
    bool needsSaw = false;
    float seconds = 1.0f;
    bool hammering = false;        // Loud each second instead of Soft
    enum class Where
    {
        Indoors,
        Outdoors,
        OutdoorsOrHardFloor
    } where = Where::Indoors;
};

class BuildSystem
{
public:
    static constexpr int kRecipeCount = 5;
    static const std::array<StationRecipe, kRecipeCount> kRecipes;

    // Tiles from the player a station may be set down.
    static constexpr float kReachTiles = 2.0f;

    [[nodiscard]] static const StationRecipe* recipeFor(StructureType type);

    // A station of `type` may stand on `tile`: free floor (no wall, door,
    // window, furniture or standing scenery, nobody on it), indoors or out
    // as the recipe says, and not on wood or carpet for a campfire indoors.
    [[nodiscard]] static bool placeable(const TileMap& map, const glm::ivec2& tile, StructureType type,
                                        const std::vector<glm::ivec2>& occupied);

    // Wear per game hour: rain doubles it for wooden things outdoors.
    [[nodiscard]] static float decayPerHour(StructureType type, bool outdoors, bool raining);

    // One stage worse between lives: Intact -> Damaged -> Breaking -> gone.
    // False when the thing does not last into the next life at all.
    [[nodiscard]] static bool carriesOver(StructureType type, bool sprung);
    [[nodiscard]] static bool wearsBetweenLives(StructureType type);
};
