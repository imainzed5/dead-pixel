// Inventory, container, hotbar and crafting panel input.
#include "core/GameShared.h"

using namespace gameshared;

// Crafting panel toggle (N) and its controls.
void Game::updateCraftingPanel(float dtSeconds, bool drivingVehicle)
{
    // Crafting panel toggle (N) and panel controls.
    if (mInput.wasKeyPressed(SDL_SCANCODE_N) && mRunState == RunState::Playing && !drivingVehicle)
    {
        mShowCrafting = !mShowCrafting;
        if (mShowCrafting)
        {
            mShowInventory = false;
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
            mCraftingRecipeCursor = 0;
            mCraftingQuantity = 1;
            mInteractionMessage.clear();
            mInteractionMessageTimer = 0.0f;

            if (mWorld.hasComponent<Velocity>(mPlayerEntity))
            {
                Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
                velocity.dx = 0.0f;
                velocity.dy = 0.0f;
            }
        }
        else if (!mCraftQueue.empty())
        {
            mCraftQueue.clear();
            mActiveCraftProgressSeconds = 0.0f;
            mInteractionMessage = "Crafting interrupted.";
            mInteractionMessageTimer = 2.0f;
        }
    }

    if (mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

        // Discovery trigger: holding both ingredients together reveals recipe entry.
        const bool hasNotebook = InventorySystem::countItem(inv, mItems.notebook) > 0;

        // With a notebook in hand, write down what this life already worked out.
        if (hasNotebook)
        {
            bool wrote = false;
            for (const auto& [a, b] : mSessionDiscoveredRecipes)
            {
                wrote = mSaveManager.addDiscoveredRecipe(a, b) || wrote;
            }
            if (wrote)
            {
                mSaveManager.saveDiscoveredRecipes();
                mInteractionMessage = "Written in your notebook.";
                mInteractionMessageTimer = std::max(mInteractionMessageTimer, 2.5f);
            }
        }
        for (const CraftingRecipe& recipe : mCraftingSystem.recipes())
        {
            const bool alreadyKnown =
                recipe.startsKnown ||
                mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB) ||
                std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), CraftingSystem::normalizePair(recipe.inputA, recipe.inputB)) != mSessionDiscoveredRecipes.end();
            if (alreadyKnown)
            {
                continue;
            }

            if (InventorySystem::countItem(inv, recipe.inputA) > 0 &&
                (recipe.inputB < 0 || InventorySystem::countItem(inv, recipe.inputB) > 0))
            {
                const auto normalized = CraftingSystem::normalizePair(recipe.inputA, recipe.inputB);
                if (std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), normalized) == mSessionDiscoveredRecipes.end())
                {
                    mSessionDiscoveredRecipes.push_back(normalized);
                }
                if (hasNotebook)
                {
                    if (mSaveManager.addDiscoveredRecipe(recipe.inputA, recipe.inputB))
                    {
                        mSaveManager.saveDiscoveredRecipes();
                        mInteractionMessage = "Notebook updated: " + recipe.label;
                    }
                }
                else if (mInteractionMessage.empty())
                {
                    mInteractionMessage = "Discovered " + recipe.label + ". Carry a notebook to preserve it.";
                }
                mInteractionMessageTimer = std::max(mInteractionMessageTimer, 2.5f);
            }
        }

        const auto visible = buildVisibleRecipeList(
            mCraftingSystem,
            mSaveManager,
            inv,
            mSessionDiscoveredRecipes,
            mCraftingCategory);

        if (visible.empty())
        {
            mCraftingRecipeCursor = 0;
        }
        else
        {
            mCraftingRecipeCursor = std::clamp(mCraftingRecipeCursor, 0, static_cast<int>(visible.size()) - 1);
        }

        bool craftActivateRequested =
            mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
            mInput.wasKeyPressed(SDL_SCANCODE_SPACE);
        bool craftQueueAppendRequested = false;

        {
            const CraftingLayout layout = craftingLayout();
            const glm::vec2 mouse(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY()));
            const bool mouseMoved = mouse != mCraftingLastMouse;
            mCraftingLastMouse = mouse;
            const auto inside = [&](const glm::vec4& r)
            {
                return mouse.x >= r.x && mouse.x < r.x + r.z && mouse.y >= r.y && mouse.y < r.y + r.w;
            };

            if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
            {
                for (int i = 0; i < kCraftingCategories; ++i)
                {
                    if (inside(layout.tabs[static_cast<std::size_t>(i)]) && i != mCraftingCategory)
                    {
                        mCraftingCategory = i;
                        mCraftingRecipeCursor = 0;
                    }
                }
            }

            const int count = static_cast<int>(visible.size());
            const int first = craftingListScroll(count, layout.maxRows);
            const glm::vec4 listRect(layout.listPos.x, layout.listPos.y, layout.listW,
                                     layout.rowH * static_cast<float>(layout.maxRows));
            if (count > 0 && inside(listRect))
            {
                const int hovered = first + static_cast<int>((mouse.y - layout.listPos.y) / layout.rowH);
                if (hovered >= 0 && hovered < count)
                {
                    // Hover selects only when the mouse moves, so W/S still work
                    // with the pointer resting on the list.
                    if (mouseMoved)
                    {
                        mCraftingRecipeCursor = hovered;
                    }
                    if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
                    {
                        mCraftingRecipeCursor = hovered;
                        craftActivateRequested = true;
                    }
                    if (mInput.wasMouseButtonPressed(SDL_BUTTON_RIGHT))
                    {
                        mCraftingRecipeCursor = hovered;
                        craftActivateRequested = true;
                        craftQueueAppendRequested = true;
                    }
                }
            }
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            if (!visible.empty())
            {
                mCraftingRecipeCursor = (mCraftingRecipeCursor + static_cast<int>(visible.size()) - 1) % static_cast<int>(visible.size());
            }
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            if (!visible.empty())
            {
                mCraftingRecipeCursor = (mCraftingRecipeCursor + 1) % static_cast<int>(visible.size());
            }
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_A) || mInput.wasKeyPressed(SDL_SCANCODE_LEFT))
        {
            mCraftingCategory = (mCraftingCategory + kCraftingCategories - 1) % kCraftingCategories;
            mCraftingRecipeCursor = 0;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_D) || mInput.wasKeyPressed(SDL_SCANCODE_RIGHT))
        {
            mCraftingCategory = (mCraftingCategory + 1) % kCraftingCategories;
            mCraftingRecipeCursor = 0;
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_MINUS) || mInput.wasKeyPressed(SDL_SCANCODE_KP_MINUS))
        {
            mCraftingQuantity = std::max(1, mCraftingQuantity - 1);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_EQUALS) || mInput.wasKeyPressed(SDL_SCANCODE_KP_PLUS))
        {
            mCraftingQuantity = std::min(10, mCraftingQuantity + 1);
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_X) || mInput.wasKeyPressed(SDL_SCANCODE_BACKSPACE))
        {
            if (!mCraftQueue.empty())
            {
                mCraftQueue.clear();
                mActiveCraftProgressSeconds = 0.0f;
                mInteractionMessage = "Crafting cancelled.";
                mInteractionMessageTimer = 2.0f;
            }
        }

        if (!visible.empty() && craftActivateRequested)
        {
            const int selectedRecipeIndex = static_cast<int>(visible[static_cast<std::size_t>(mCraftingRecipeCursor)]);
            const CraftingRecipe& recipe = mCraftingSystem.recipes()[static_cast<std::size_t>(selectedRecipeIndex)];

            const bool discovered =
                recipe.startsKnown ||
                mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB) ||
                std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), CraftingSystem::normalizePair(recipe.inputA, recipe.inputB)) != mSessionDiscoveredRecipes.end();

            if (!discovered)
            {
                mInteractionMessage = "Recipe is still undiscovered.";
                mInteractionMessageTimer = 2.0f;
            }
            else if (!craftStationNearby(recipe.station, mFurniture, mWorld, mTileMap, playerTile(), powerOn(),
                                        !mFireSystem.burningNear(playerTile(), 3).empty()))
            {
                mInteractionMessage = recipe.station == CraftStation::Workbench ? "Needs a workbench nearby." : "Needs a fire nearby.";
                mInteractionMessageTimer = 2.5f;
            }
            else
            {
                const bool queueAppend =
                    craftQueueAppendRequested ||
                    mInput.isKeyDown(SDL_SCANCODE_LSHIFT) ||
                    mInput.isKeyDown(SDL_SCANCODE_RSHIFT);

                if (!hasCraftTools(inv, mItemDatabase, recipe))
                {
                    mInteractionMessage = "Missing required tools.";
                    mInteractionMessageTimer = 2.0f;
                }
                else if (!hasCraftIngredients(inv, recipe, 1))
                {
                    mInteractionMessage = "Missing ingredients.";
                    mInteractionMessageTimer = 2.0f;
                }
                else
                {
                    if (!queueAppend)
                    {
                        mCraftQueue.clear();
                        mActiveCraftProgressSeconds = 0.0f;
                    }

                    if (!mCraftQueue.empty() && mCraftQueue.back().recipeIndex == selectedRecipeIndex)
                    {
                        mCraftQueue.back().remaining += mCraftingQuantity;
                    }
                    else
                    {
                        mCraftQueue.push_back({selectedRecipeIndex, mCraftingQuantity});
                    }

                    mInteractionMessage = queueAppend ? "Added recipe to queue." : "Crafting queued.";
                    mInteractionMessageTimer = 1.8f;
                }
            }
        }

        // Timed crafting progression with interrupt checks.
        if (!mCraftQueue.empty())
        {
            CraftQueueEntry& active = mCraftQueue.front();
            if (active.recipeIndex < 0 || active.recipeIndex >= static_cast<int>(mCraftingSystem.recipes().size()) || active.remaining <= 0)
            {
                mCraftQueue.pop_front();
                mActiveCraftProgressSeconds = 0.0f;
            }
            else
            {
                const CraftingRecipe& recipe = mCraftingSystem.recipes()[static_cast<std::size_t>(active.recipeIndex)];

                bool interrupted = false;
                std::string interruptReason;

                if (mWorld.hasComponent<Velocity>(mPlayerEntity))
                {
                    const Velocity& v = mWorld.getComponent<Velocity>(mPlayerEntity);
                    const float speedSq = v.dx * v.dx + v.dy * v.dy;
                    if (speedSq > 4.0f)
                    {
                        interrupted = true;
                        interruptReason = "Crafting interrupted by movement.";
                    }
                }

                if (!interrupted && !craftStationNearby(recipe.station, mFurniture, mWorld, mTileMap, playerTile(), powerOn(),
                                        !mFireSystem.burningNear(playerTile(), 3).empty()))
                {
                    interrupted = true;
                    interruptReason = "Crafting interrupted: walked away from it.";
                }

                if (!interrupted && !hasCraftTools(inv, mItemDatabase, recipe))
                {
                    interrupted = true;
                    interruptReason = "Crafting interrupted: required tool unavailable.";
                }

                if (!interrupted && !hasCraftIngredients(inv, recipe, 1))
                {
                    interrupted = true;
                    interruptReason = "Crafting interrupted: missing ingredients.";
                }

                if (interrupted)
                {
                    mCraftQueue.clear();
                    mActiveCraftProgressSeconds = 0.0f;
                    mInteractionMessage = interruptReason;
                    mInteractionMessageTimer = 2.0f;
                }
                else
                {
                    // Below the recipe's skill: twice as long (phase 5).
                    const int skillHave = recipe.skill >= 0 ? skillLevel(static_cast<Skill>(recipe.skill)) : 0;
                    const float gate = recipe.skill >= 0 ? skillrules::craftTimeScale(skillHave, recipe.skillLevel) : 1.0f;
                    const float craftDuration = std::max(0.25f, static_cast<float>(recipe.craftTimeSeconds) * gate);
                    mActiveCraftProgressSeconds += dtSeconds;

                    if (mActiveCraftProgressSeconds >= craftDuration)
                    {
                        mActiveCraftProgressSeconds = 0.0f;

                        // ... and it fails 40 % of the time: the inputs are kept, but food is spoiled.
                        if (recipe.skill >= 0 && bodyRoll() < skillrules::craftFailChance(skillHave, recipe.skillLevel))
                        {
                            const auto spoil = [&](int itemId, int count)
                            {
                                const ItemDef* def = itemId >= 0 ? mItemDatabase.find(itemId) : nullptr;
                                if (def != nullptr && def->category == ItemCategory::Food) InventorySystem::removeItem(inv, itemId, count);
                            };
                            spoil(recipe.inputA, recipe.countA);
                            spoil(recipe.inputB, recipe.countB);
                            --active.remaining;
                            if (active.remaining <= 0) mCraftQueue.pop_front();
                            mInteractionMessage = "It comes apart in your hands.";
                            mInteractionMessageTimer = 2.5f;
                            return;
                        }
                        Inventory preview = inv;
                        if (!InventorySystem::removeItem(preview, recipe.inputA, recipe.countA) ||
                            (recipe.inputB >= 0 && !InventorySystem::removeItem(preview, recipe.inputB, recipe.countB)))
                        {
                            mCraftQueue.clear();
                            mInteractionMessage = "Crafting failed: ingredients changed.";
                            mInteractionMessageTimer = 2.0f;
                        }
                        else
                        {
                            const AddItemResult addResult =
                                InventorySystem::addItem(preview, mItemDatabase, recipe.outputItemId, recipe.outputCount);
                            if (addResult.added < recipe.outputCount)
                            {
                                mCraftQueue.clear();
                                mInteractionMessage = "Crafting failed: inventory is full.";
                                mInteractionMessageTimer = 2.0f;
                            }
                            else
                            {
                                inv = preview;

                                const auto normalized = CraftingSystem::normalizePair(recipe.inputA, recipe.inputB);
                                if (std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), normalized) == mSessionDiscoveredRecipes.end())
                                {
                                    mSessionDiscoveredRecipes.push_back(normalized);
                                }

                                const bool hasNotebook = InventorySystem::countItem(inv, mItems.notebook) > 0;
                                if (hasNotebook)
                                {
                                    if (mSaveManager.addDiscoveredRecipe(recipe.inputA, recipe.inputB))
                                    {
                                        mSaveManager.saveDiscoveredRecipes();
                                    }
                                }

                                --active.remaining;
                                const ItemDef* outDef = mItemDatabase.find(recipe.outputItemId);
                                mInteractionMessage = "Crafted " + std::string(outDef ? outDef->name : recipe.label) + ".";
                                if (!hasNotebook)
                                {
                                    mInteractionMessage += " Carry a notebook to preserve discoveries.";
                                }
                                mInteractionMessageTimer = 1.8f;
                                mAudioManager.playSound(SoundId::WeaponHit, 0.35f);
                                // Cooking at a fire: Living off the land.
                                if (recipe.station == CraftStation::Fire) awardXp(Skill::LivingOffTheLand, skillrules::kXpCook);

                                if (active.remaining <= 0)
                                {
                                    mCraftQueue.pop_front();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// Inventory toggle, container view, inventory cursor and hotbar keys.
void Game::updateInventoryInput(bool drivingVehicle)
{
    bool useInventoryCursorItemRequested = false;

    // Talking to someone: the panel has the keys.
    if (mTalk.open)
    {
        handleTalkInput();
        return;
    }

    // Inventory toggle (Tab)
    if (mInput.wasKeyPressed(SDL_SCANCODE_TAB))
    {
        if (drivingVehicle)
        {
            mInteractionMessage = "Stop the vehicle before opening inventory.";
            mInteractionMessageTimer = 1.6f;
        }
        else if (mShowCrafting)
        {
            // MAKE is the last tab: Tab closes it.
            mShowCrafting = false;
            if (!mCraftQueue.empty())
            {
                mCraftQueue.clear();
                mActiveCraftProgressSeconds = 0.0f;
                mInteractionMessage = "Crafting interrupted.";
                mInteractionMessageTimer = 2.0f;
            }
        }
        else if (mContainerOpen)
        {
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
        }
        else if (!mShowInventory)
        {
            // PACK.
            mShowInventory = true;
            mPackTab = 0;
            mBodyChoosing = false;
            if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                mInventoryCursor = inv.activeSlot;
            }
        }
        else if (mPackTab == 0)
        {
            // BODY (phase 5).
            mPackTab = 1;
            mBodyChoosing = false;
        }
        else
        {
            // MAKE: the crafting panel.
            mShowInventory = false;
            mPackTab = 0;
            mBodyChoosing = false;
            mShowCrafting = true;
            mCraftingRecipeCursor = 0;
            mCraftingQuantity = 1;
            mInteractionMessage.clear();
            mInteractionMessageTimer = 0.0f;
            if (mWorld.hasComponent<Velocity>(mPlayerEntity))
            {
                Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
                velocity.dx = 0.0f;
                velocity.dy = 0.0f;
            }
        }
    }

    // Container view input (GameStorage.cpp)
    handleContainerInput();

    // The BODY tab: wounds, and what helps them.
    const bool bodyTab = mShowInventory && !mShowCrafting && mPackTab == 1;
    if (bodyTab)
    {
        updateBodyTabInput();
    }

    // Inventory cursor navigation (when inventory is open)
    if (mShowInventory && !bodyTab && !mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

        const InventoryLayout layout = computeInventoryLayout(mWindowWidth, mWindowHeight);
        const int hoveredSlot = layout.slotAt(glm::vec2(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY())));

        if (hoveredSlot >= 0)
        {
            mInventoryCursor = hoveredSlot;

            if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
            {
                const auto& hovered = inv.slots[static_cast<std::size_t>(mInventoryCursor)];
                if (hovered.itemId >= 0 && hovered.count > 0)
                {
                    useInventoryCursorItemRequested = true;
                }
            }

            if (mInput.wasMouseButtonPressed(SDL_BUTTON_RIGHT) && mInventoryCursor != inv.activeSlot)
            {
                std::swap(inv.slots[static_cast<std::size_t>(mInventoryCursor)],
                          inv.slots[static_cast<std::size_t>(inv.activeSlot)]);
                mInteractionMessage = "Swapped items.";
                mInteractionMessageTimer = 2.0f;
            }
        }

        // Up/Down (or W/S) move between the rows, Left/Right along one.
        mInventoryCursor = std::clamp(mInventoryCursor, 0, Inventory::kMaxSlots - 1);
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            mInventoryCursor = InventoryLayout::neighbour(mInventoryCursor, 0, -1);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            mInventoryCursor = InventoryLayout::neighbour(mInventoryCursor, 0, 1);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_LEFT))
        {
            mInventoryCursor = InventoryLayout::neighbour(mInventoryCursor, -1, 0);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_RIGHT))
        {
            mInventoryCursor = InventoryLayout::neighbour(mInventoryCursor, 1, 0);
        }

        // Backspace: the cursor item onto the floor (Shift: the whole stack).
        if (mInput.wasKeyPressed(SDL_SCANCODE_BACKSPACE))
        {
            dropFromPack(mInventoryCursor, mInput.isKeyDown(SDL_SCANCODE_LSHIFT) || mInput.isKeyDown(SDL_SCANCODE_RSHIFT));
        }

        // Enter key: swap cursor slot with active hotbar slot
        if (mInput.wasKeyPressed(SDL_SCANCODE_RETURN) && mInventoryCursor != inv.activeSlot)
        {
            std::swap(inv.slots[static_cast<std::size_t>(mInventoryCursor)],
                      inv.slots[static_cast<std::size_t>(inv.activeSlot)]);
            mInteractionMessage = "Swapped items.";
            mInteractionMessageTimer = 2.0f;
        }
    }

    // Hotbar selection (1-8 keys) when not in build mode
    if (!mShowCrafting && !mBuildWheelOpen && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        for (int key = 0; key < 8; ++key)
        {
            if (mInput.wasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + key)))
            {
                inv.activeSlot = key;
            }
        }

        // Use item (F key or inventory click): use cursor slot when inventory is open, otherwise active hotbar slot
        if ((mInput.wasKeyPressed(SDL_SCANCODE_F) && !bodyTab) || useInventoryCursorItemRequested)
        {
            const int useSlot = mShowInventory ? mInventoryCursor : inv.activeSlot;
            const auto& slot = inv.slots[static_cast<std::size_t>(useSlot)];
            // Alarms, spike boards and lights are set down, not used up.
            if (placeMarker(useSlot) || placeSnare(useSlot) || placeItem(useSlot) || useFightingItem(useSlot) || useBatteryOrRadio(useSlot))
            {
                // Set down, wound, or poured on a fire.
            }
            else if (slot.itemId >= 0 && slot.count > 0)
            {
                const ItemDef* item = mItemDatabase.find(slot.itemId);
                const double nowHour = absoluteHour();
                if (item)
                {
                    // A medical item (or a bottle of Spirits) goes on the worst
                    // wound it can help; a book is read, a keepsake held.
                    if (!item->treats.empty() && treatWithItem(useSlot, -1))
                    {
                        // Treated (or begun: stitching and splints take a while).
                    }
                    else if (useComfortItem(useSlot))
                    {
                        // Reading, or holding it.
                    }
                    else if (eatOrDrink(useSlot))
                    {
                        // Eaten or drunk (phase 6: fresh, stale or spoiled; raw; dirty).
                    }
                    else if (item->category == ItemCategory::Medical)
                    {
                        bool used = false;

                        if (item->curesInfection && mWorld.hasComponent<Wounds>(mPlayerEntity))
                        {
                            Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
                            for (Wound& wound : wounds.active)
                            {
                                if (!wound.infected)
                                {
                                    continue;
                                }

                                wound.infected = false;
                                wound.infectionTimer = 0.0f;
                                used = true;
                            }
                        }

                        if (item->illnessRelief > 0.0f && mWorld.hasComponent<Needs>(mPlayerEntity))
                        {
                            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                            const float previousIllness = needs.illness;
                            needs.illness = std::max(0.0f, needs.illness - item->illnessRelief);
                            used = used || needs.illness < previousIllness;
                        }

                        if (item->panicRelief > 0.0f && mWorld.hasComponent<MentalState>(mPlayerEntity))
                        {
                            MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                            const float previousPanic = mental.panic;
                            mental.panic = std::max(0.0f, mental.panic - item->panicRelief);
                            used = used || mental.panic < previousPanic;
                        }
                        // Painkillers: stitches hurt less for a while, and sleep comes worse.
                        if (item->hasTag("sedative") && mWorld.hasComponent<Needs>(mPlayerEntity))
                        {
                            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                            needs.lastSedativeHour = nowHour;
                            needs.lastPainkillerHour = nowHour;
                            used = true;
                        }

                        if (item->stopsBleeding && mWorld.hasComponent<Bleeding>(mPlayerEntity))
                        {
                            mWorld.removeComponent<Bleeding>(mPlayerEntity);
                            used = true;
                        }

                        if (item->healthRestore > 0.0f && mWorld.hasComponent<Health>(mPlayerEntity))
                        {
                            Health& health = mWorld.getComponent<Health>(mPlayerEntity);
                            const float healedAmount = std::min(item->healthRestore, health.maximum - health.current);
                            if (healedAmount > 0.0f)
                            {
                                health.current += healedAmount;
                                used = true;
                            }
                        }

                        if (used)
                        {
                            InventorySystem::removeItem(inv, slot.itemId, 1);
                            mInteractionMessage = "Used " + item->name + ".";
                            mInteractionMessageTimer = 3.0f;
                        }
                        else
                        {
                            mInteractionMessage = "You don't need that right now.";
                            mInteractionMessageTimer = 2.0f;
                        }
                    }
                    else if (item->category == ItemCategory::Weapon && mWorld.hasComponent<Combat>(mPlayerEntity))
                    {
                        equipWeaponFromSlot(useSlot);
                        mInteractionMessage = "Equipped " + item->name + ".";
                        mInteractionMessageTimer = 3.0f;
                    }
                }
            }
        }

    }
}

void Game::equipWeaponFromSlot(int slotIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Combat>(mPlayerEntity) ||
        !mWorld.hasComponent<Inventory>(mPlayerEntity) || slotIndex < 0 || slotIndex >= Inventory::kMaxSlots)
    {
        return;
    }
    const InventorySlot& slot = mWorld.getComponent<Inventory>(mPlayerEntity).slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr || item->category != ItemCategory::Weapon)
    {
        return;
    }
    WeaponStats stats = weaponStatsForItem(*item, &mItemDatabase);
    stats.ammo = std::clamp(slot.loaded, 0, std::max(0, stats.magazine));
    stats.durability = std::clamp(static_cast<int>(std::round(static_cast<float>(stats.maxDurability) * slot.condition)),
                                  0, stats.maxDurability);
    stats.condition = weaponConditionFor(stats.durability, stats.maxDurability);
    mWorld.getComponent<Combat>(mPlayerEntity).weapon = stats;
    mEquippedWeaponSlot = slotIndex;
}

void Game::equipStartingWeapon(int preferredSlot)
{
    mEquippedWeaponSlot = -1;
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Combat>(mPlayerEntity) ||
        !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }
    mWorld.getComponent<Combat>(mPlayerEntity).weapon = makeWeaponStats(WeaponCategory::Unarmed);
    equipWeaponFromSlot(preferredSlot);
    for (int i = 0; mEquippedWeaponSlot < 0 && i < Inventory::kMaxSlots; ++i)
    {
        equipWeaponFromSlot(i);
    }
}

void Game::syncEquippedWeapon()
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Combat>(mPlayerEntity) ||
        !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }
    Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
    if (combat.weapon.itemId < 0)
    {
        return;
    }
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const auto holds = [&](int i)
    {
        return i >= 0 && i < Inventory::kMaxSlots && inv.slots[static_cast<std::size_t>(i)].itemId == combat.weapon.itemId &&
               inv.slots[static_cast<std::size_t>(i)].count > 0;
    };
    // Slots can be swapped: follow the item.
    if (!holds(mEquippedWeaponSlot))
    {
        mEquippedWeaponSlot = -1;
        for (int i = 0; i < Inventory::kMaxSlots && mEquippedWeaponSlot < 0; ++i)
        {
            if (holds(i)) mEquippedWeaponSlot = i;
        }
    }
    if (mEquippedWeaponSlot < 0)
    {
        combat.weapon = makeWeaponStats(WeaponCategory::Unarmed);
        return;
    }
    InventorySlot& held = inv.slots[static_cast<std::size_t>(mEquippedWeaponSlot)];
    held.condition = static_cast<float>(combat.weapon.durability) / static_cast<float>(std::max(1, combat.weapon.maxDurability));
    if (combat.weapon.category == WeaponCategory::Ranged)
    {
        held.loaded = combat.weapon.ammo;
    }
}
