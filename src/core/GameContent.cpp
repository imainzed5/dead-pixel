// Content from assets/data: items, recipes, loot classes and tuning.
#include "core/GameShared.h"

#include "core/Tuning.h"

bool Game::loadContent(const std::filesystem::path& assetsRoot)
{
    const std::filesystem::path data = assetsRoot / "data";
    if (!mItemDatabase.loadFromFile((data / "items.json").string()))
    {
        std::cerr << "Failed to load item database.\n";
        return false;
    }
    if (!mItems.resolve(mItemDatabase))
    {
        std::cerr << "items.json lacks items the code names (see above).\n";
        return false;
    }
    if (!mCraftingSystem.loadRecipesFromFile((data / "recipes.json").string(), mItemDatabase))
    {
        std::cerr << "Failed to load crafting recipes.\n";
        return false;
    }
    std::string lootError;
    if (!mLoot.load((data / "loot.json").string(), mItemDatabase, &lootError))
    {
        std::cerr << "Failed to load loot tables: " << lootError << '\n';
        return false;
    }
    std::string deadError;
    if (!zombiekinds::activeTable().load((data / "zombies.json").string(), &deadError))
    {
        std::cerr << "Failed to load zombies.json: " << deadError << "\n";
        return false;
    }
    Tuning::load((data / "tuning.json").string());
    loadSurvivorLines(assetsRoot);
    return true;
}
