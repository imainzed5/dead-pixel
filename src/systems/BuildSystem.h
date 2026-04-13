#pragma once

#include <glm/vec2.hpp>

#include "components/Inventory.h"
#include "components/Structure.h"
#include "core/Input.h"
#include "ecs/World.h"
#include "items/ItemDatabase.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"
#include "rendering/SpriteSheet.h"

struct BuildRecipe
{
    StructureType type;
    const char* name;
    const char* sprite;
    int materialId;      // item ID consumed
    int materialCost;
    float hp;
};

class BuildSystem
{
public:
    std::string update(World& world, const Input& input, const TileMap& tileMap,
                NoiseModel& noiseModel, const ItemDatabase& db,
                const SpriteSheet& spriteSheet, Entity playerEntity,
                const glm::vec2& mouseWorld, float dtSeconds, double gameHours);

    [[nodiscard]] bool isBuildMode() const { return mBuildMode; }
    [[nodiscard]] int selectedRecipe() const { return mSelectedRecipe; }
    [[nodiscard]] glm::vec2 ghostPosition() const { return mGhostPosition; }
    [[nodiscard]] bool ghostValid() const { return mGhostValid; }

    static constexpr int kRecipeCount = 6;
    static const BuildRecipe kRecipes[kRecipeCount];

private:
    bool mBuildMode = false;
    int mSelectedRecipe = 0;
    glm::vec2 mGhostPosition{0.0f, 0.0f};
    bool mGhostValid = false;

    static constexpr float kSpriteSize = 32.0f;
};
