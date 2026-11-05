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
        const ItemDef* notebookItem = mItemDatabase.findByName("Notebook");
        const bool hasNotebook = notebookItem != nullptr && InventorySystem::countItem(inv, notebookItem->id) > 0;

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

            if (InventorySystem::countItem(inv, recipe.inputA) > 0 && InventorySystem::countItem(inv, recipe.inputB) > 0)
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
                for (int i = 0; i < 3; ++i)
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
            mCraftingCategory = (mCraftingCategory + 2) % 3;
            mCraftingRecipeCursor = 0;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_D) || mInput.wasKeyPressed(SDL_SCANCODE_RIGHT))
        {
            mCraftingCategory = (mCraftingCategory + 1) % 3;
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
            else if (recipe.requiresWorkbench && !hasNearbyWorkbench(mWorld, mPlayerEntity))
            {
                mInteractionMessage = "Need nearby workbench surface.";
                mInteractionMessageTimer = 2.5f;
            }
            else
            {
                const bool queueAppend =
                    craftQueueAppendRequested ||
                    mInput.isKeyDown(SDL_SCANCODE_LSHIFT) ||
                    mInput.isKeyDown(SDL_SCANCODE_RSHIFT);

                if (!hasCraftTools(inv, recipe))
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

                if (!interrupted && recipe.requiresWorkbench && !hasNearbyWorkbench(mWorld, mPlayerEntity))
                {
                    interrupted = true;
                    interruptReason = "Crafting interrupted: left workbench range.";
                }

                if (!interrupted && !hasCraftTools(inv, recipe))
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
                    const float craftDuration = std::max(0.25f, static_cast<float>(recipe.craftTimeSeconds));
                    mActiveCraftProgressSeconds += dtSeconds;

                    if (mActiveCraftProgressSeconds >= craftDuration)
                    {
                        mActiveCraftProgressSeconds = 0.0f;

                        Inventory preview = inv;
                        if (!InventorySystem::removeItem(preview, recipe.inputA, 1) ||
                            !InventorySystem::removeItem(preview, recipe.inputB, 1))
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

                                const ItemDef* notebook = mItemDatabase.findByName("Notebook");
                                const bool hasNotebook = notebook != nullptr && InventorySystem::countItem(inv, notebook->id) > 0;
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
            // Keep inventory disabled while crafting panel is open.
        }
        else
        {
        if (mContainerOpen)
        {
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
        }
        else
        {
            mShowInventory = !mShowInventory;
            if (mShowInventory && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                mInventoryCursor = inv.activeSlot;
            }
        }
        }
    }

    // Container view input
    if (mContainerOpen && mOpenContainerEntity != kInvalidEntity &&
        mWorld.hasComponent<Inventory>(mOpenContainerEntity) &&
        mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& containerInv = mWorld.getComponent<Inventory>(mOpenContainerEntity);
        Inventory& playerInv = mWorld.getComponent<Inventory>(mPlayerEntity);

        // Count filled slots for cursor bounds
        int filledSlots = 0;
        for (const auto& slot : containerInv.slots)
        {
            if (slot.itemId >= 0 && slot.count > 0) ++filledSlots;
        }

        if (filledSlots == 0)
        {
            std::string thing;
            if (mWorld.hasComponent<Interactable>(mOpenContainerEntity))
            {
                thing = searchedThing(mWorld.getComponent<Interactable>(mOpenContainerEntity).prompt);
            }
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
            mInteractionMessage = thing.empty() ? "Container empty." : "The " + thing + " is empty.";
            mInteractionMessageTimer = 2.0f;
        }
        else
        {
            if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
            {
                mContainerCursor = (mContainerCursor + filledSlots - 1) % filledSlots;
            }
            if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
            {
                mContainerCursor = (mContainerCursor + 1) % filledSlots;
            }
            mContainerCursor = std::clamp(mContainerCursor, 0, filledSlots - 1);

            // E or Enter: take selected item
            if (mInput.wasKeyPressed(SDL_SCANCODE_E) || mInput.wasKeyPressed(SDL_SCANCODE_RETURN))
            {
                // Find the Nth filled slot
                int idx = 0;
                for (auto& slot : containerInv.slots)
                {
                    if (slot.itemId < 0 || slot.count <= 0) continue;
                    if (idx == mContainerCursor)
                    {
                        auto result = InventorySystem::addItem(playerInv, mItemDatabase, slot.itemId, 1);
                        if (result.added > 0)
                        {
                            const ItemDef* item = mItemDatabase.find(slot.itemId);
                            std::string name = item ? item->name : "item";
                            slot.count -= 1;
                            if (slot.count <= 0)
                            {
                                slot.itemId = -1;
                                slot.count = 0;
                                slot.condition = 1.0f;
                            }
                            mInteractionMessage = "Took " + name + ".";
                            mInteractionMessageTimer = 2.0f;
                            mAudioManager.playSound(SoundId::WeaponHit, 0.3f);
                            ++mRunStats.itemsLooted;

                            // Track looted position for world memory
                            if (mWorld.hasComponent<Transform>(mOpenContainerEntity))
                            {
                                const auto& ct = mWorld.getComponent<Transform>(mOpenContainerEntity);
                                const int tx = static_cast<int>(ct.x) / mTileMap.tileWidth();
                                const int ty = static_cast<int>(ct.y) / mTileMap.tileHeight();
                                mSaveManager.addLootedPosition(tx, ty);
                            }
                        }
                        else
                        {
                            mInteractionMessage = "Inventory full.";
                            mInteractionMessageTimer = 2.0f;
                        }
                        break;
                    }
                    ++idx;
                }
            }

            // Escape: close container
            if (mInput.wasKeyPressed(SDL_SCANCODE_ESCAPE))
            {
                mContainerOpen = false;
                mOpenContainerEntity = kInvalidEntity;
            }
        }
    }

    // Inventory cursor navigation (when inventory is open)
    if (mShowInventory && !mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
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

        // Grid navigation: W/S or Up/Down move a row, Left/Right move a slot.
        constexpr int kRowStep = InventoryLayout::kColumns;
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            mInventoryCursor = (mInventoryCursor + Inventory::kMaxSlots - kRowStep) % Inventory::kMaxSlots;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            mInventoryCursor = (mInventoryCursor + kRowStep) % Inventory::kMaxSlots;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_LEFT))
        {
            mInventoryCursor = (mInventoryCursor + Inventory::kMaxSlots - 1) % Inventory::kMaxSlots;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_RIGHT))
        {
            mInventoryCursor = (mInventoryCursor + 1) % Inventory::kMaxSlots;
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
    if (!mShowCrafting && !mBuildSystem.isBuildMode() && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
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
        if (mInput.wasKeyPressed(SDL_SCANCODE_F) || useInventoryCursorItemRequested)
        {
            const int useSlot = mShowInventory ? mInventoryCursor : inv.activeSlot;
            const auto& slot = inv.slots[static_cast<std::size_t>(useSlot)];
            if (slot.itemId >= 0 && slot.count > 0)
            {
                const ItemDef* item = mItemDatabase.find(slot.itemId);
                if (item)
                {
                    if (item->category == ItemCategory::Food && mWorld.hasComponent<Needs>(mPlayerEntity))
                    {
                        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                        const float restore = item->hungerRestore * slot.condition;
                        needs.hunger = std::min(needs.maxHunger, needs.hunger + restore);

                        // Spoiled food causes illness
                        if (slot.condition < 0.3f)
                        {
                            needs.illness = std::min(100.0f, needs.illness + (1.0f - slot.condition) * 20.0f);
                            mInteractionMessage = "Ate spoiled " + item->name + "... feeling sick.";
                        }
                        else
                        {
                            mInteractionMessage = "Ate " + item->name + ".";
                        }

                        InventorySystem::removeItem(inv, slot.itemId, 1);
                        mInteractionMessageTimer = 3.0f;
                        mAudioManager.playSound(SoundId::FoodEat, 0.5f);
                    }
                    else if (item->category == ItemCategory::Drink && mWorld.hasComponent<Needs>(mPlayerEntity))
                    {
                        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                        needs.thirst = std::min(needs.maxThirst, needs.thirst + item->thirstRestore);
                        InventorySystem::removeItem(inv, slot.itemId, 1);
                        mInteractionMessage = "Drank " + item->name + ".";
                        mInteractionMessageTimer = 3.0f;
                        mAudioManager.playSound(SoundId::FoodEat, 0.4f);
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

                        if (item->stopsBleeding && mWorld.hasComponent<Bleeding>(mPlayerEntity))
                        {
                            mWorld.removeComponent<Bleeding>(mPlayerEntity);
                            used = true;
                        }

                        if (item->woundTreatment > 0 && mWorld.hasComponent<Wounds>(mPlayerEntity))
                        {
                            Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
                            const int treated = treatWorstWounds(wounds, item->woundTreatment);
                            if (treated > 0)
                            {
                                used = true;
                                if (mWorld.hasComponent<Health>(mPlayerEntity))
                                {
                                    Health& health = mWorld.getComponent<Health>(mPlayerEntity);
                                    syncHealthWithWounds(health, wounds);
                                }
                            }
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
                        Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
                        combat.weapon = weaponStatsForItem(*item);
                        mInteractionMessage = "Equipped " + item->name + ".";
                        mInteractionMessageTimer = 3.0f;
                    }
                }
            }
        }

    }
}
