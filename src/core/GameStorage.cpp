// Containers: the two-column panel (pack and container), what the world
// remembers of each container, and food going off in cupboards and fridges.
#include "core/GameShared.h"
#include "world/Storage.h"

#include <iomanip>
#include <sstream>

using namespace gameshared;

namespace
{
constexpr float kContainerSpoilStepHours = 1.0f;
}

FurnitureKind Game::containerKindAt(const glm::ivec2& tile) const
{
    const auto piece = std::find_if(mFurniture.begin(), mFurniture.end(),
                                    [&](const FurniturePiece& p) { return p.tile == tile; });
    return piece != mFurniture.end() ? piece->kind : FurnitureKind::Count;
}

std::string Game::containerName(Entity container) const
{
    std::string name = "Container";
    if (mWorld.hasComponent<Corpse>(container)) return "Body";
    if (mWorld.hasComponent<SurvivorAI>(container))
    {
        const SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(container);
        return ai.alive ? ai.name + "'s pack" : ai.name;
    }
    if (mWorld.hasComponent<Interactable>(container))
    {
        std::string thing = searchedThing(mWorld.getComponent<Interactable>(container).prompt);
        if (!thing.empty())
        {
            thing[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(thing[0])));
            name = thing;
        }
    }
    return name;
}

void Game::recordContainer(Entity container)
{
    if (!mWorld.hasComponent<Inventory>(container) || !mWorld.hasComponent<Transform>(container))
    {
        return;
    }
    const Transform& t = mWorld.getComponent<Transform>(container);
    const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x, t.y));
    // Only furniture is remembered by its tile: a crate keeps its things in
    // the structures file, a pile on the floor lasts this life.
    if (mWorld.hasComponent<Structure>(container) || mWorld.hasComponent<Corpse>(container) ||
        mWorld.hasComponent<SurvivorAI>(container) ||
        (mWorld.hasComponent<Interactable>(container) && mWorld.getComponent<Interactable>(container).prompt == "Search the pile"))
    {
        return;
    }
    mSaveManager.setContainerRecord(
        storage::toRecord(mWorld.getComponent<Inventory>(container), tile.x, tile.y, mSaveManager.runNumber()));
    mSaveManager.saveWorldMemory();
}

void Game::refreshContainerRecords()
{
    // Containers the world already remembers keep what is in them now (food
    // that went off while you were away stays off). Untouched ones are not
    // written: they roll fresh for the next life.
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory& inv, Transform& t)
        {
            if (inter.type != InteractableType::Container || mWorld.hasComponent<Structure>(e) ||
                inter.prompt == "Search the pile") return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x, t.y));
            const ContainerRecord* existing = mSaveManager.containerRecord(tile.x, tile.y);
            if (existing == nullptr) return;
            mSaveManager.setContainerRecord(storage::toRecord(inv, tile.x, tile.y, existing->runIndex));
        });
}

void Game::spoilContainers(float gameHours)
{
    mContainerSpoilHours += gameHours;
    if (mContainerSpoilHours < kContainerSpoilStepHours)
    {
        return;
    }
    const float step = mContainerSpoilHours;
    mContainerSpoilHours = 0.0f;
    const bool power = powerOn(); // a fridge keeps things only while it runs
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory& inv, Transform& t)
        {
            if (inter.type != InteractableType::Container || e == mPlayerEntity) return;
            const FurnitureKind kind = containerKindAt(worldToTile(mTileMap, glm::vec2(t.x, t.y)));
            InventorySystem::degradeFood(inv, mItemDatabase, step * storage::spoilScale(kind, power));
        });
}

void Game::closeContainer()
{
    mContainerOpen = false;
    mOpenContainerEntity = kInvalidEntity;
}

void Game::openContainer(Entity container)
{
    mContainerOpen = true;
    mOpenContainerEntity = container;
    mContainerCursor = 0;
    mPackCursor = 0;
    mContainerPackSide = false;
    mShowInventory = false;
    mShowCrafting = false;
    ++mContainersOpened;
    noteContainerSearched(container, false);
    // Scavenging: the first look through each (phase 5).
    const bool companionPack = mWorld.hasComponent<SurvivorAI>(container) && mWorld.getComponent<SurvivorAI>(container).alive;
    if (!companionPack && std::find(mXpSearched.begin(), mXpSearched.end(), container) == mXpSearched.end())
    {
        mXpSearched.push_back(container);
        awardXp(Skill::Scavenging, skillrules::kXpSearch);
    }
}

int Game::containerMove(bool fromPack, int row, bool wholeStack)
{
    if (!mContainerOpen || mOpenContainerEntity == kInvalidEntity || mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Inventory>(mOpenContainerEntity) || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return 0;
    }
    Inventory& containerInv = mWorld.getComponent<Inventory>(mOpenContainerEntity);
    Inventory& pack = mWorld.getComponent<Inventory>(mPlayerEntity);
    Inventory& from = fromPack ? pack : containerInv;
    Inventory& to = fromPack ? containerInv : pack;

    const int slotIndex = storage::nthFilledSlot(from, row);
    if (slotIndex < 0)
    {
        return 0;
    }
    const InventorySlot slot = from.slots[static_cast<std::size_t>(slotIndex)];
    if (fromPack && slot.itemId == mItems.marker)
    {
        mInteractionMessage = "The marker stays with you until it is set.";
        mInteractionMessageTimer = 2.0f;
        return 0;
    }
    const ItemDef* item = mItemDatabase.find(slot.itemId);
    const std::string name = item != nullptr ? item->name : "item";
    const int moved = storage::moveItem(from, slotIndex, to, mItemDatabase, wholeStack ? slot.count : 1);
    if (moved <= 0)
    {
        std::string thing = mWorld.hasComponent<Interactable>(mOpenContainerEntity)
                                ? searchedThing(mWorld.getComponent<Interactable>(mOpenContainerEntity).prompt)
                                : std::string{};
        if (thing.empty()) thing = "container";
        mInteractionMessage = fromPack ? "The " + thing + " is full." : std::string("No room in your pack.");
        mInteractionMessageTimer = 2.0f;
        return 0;
    }

    const std::string countText = moved > 1 ? " x" + std::to_string(moved) : "";
    mInteractionMessage = (fromPack ? "Put away " : "Took ") + name + countText + ".";
    mInteractionMessageTimer = 2.0f;
    mAudioManager.playSound(SoundId::WeaponHit, 0.3f);
    if (!fromPack)
    {
        mRunStats.itemsLooted += moved;
        noteTakenFrom(mOpenContainerEntity); // someone's cupboard
        if (slot.itemId == mItems.notebook) notebookTaken(mOpenContainerEntity); // an earlier life's notes (phase 9)
    }
    noteContainerSearched(mOpenContainerEntity, !fromPack);
    recordContainer(mOpenContainerEntity);
    return moved;
}

ContainerLayout Game::containerLayout() const
{
    ContainerLayout layout;
    layout.panelW = dockedPanelWidth(true, mWindowWidth);
    layout.panelX = static_cast<float>(mWindowWidth) - layout.panelW;
    layout.panelH = static_cast<float>(mWindowHeight);
    layout.columnW = (layout.panelW - ContainerLayout::kPadding * 3.0f) * 0.5f;
    layout.packX = layout.panelX + ContainerLayout::kPadding;
    layout.boxX = layout.packX + layout.columnW + ContainerLayout::kPadding;
    return layout;
}

void Game::handleContainerInput()
{
    if (!mContainerOpen)
    {
        return;
    }
    if (mOpenContainerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mOpenContainerEntity) ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        closeContainer();
        return;
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        closeContainer();
        return;
    }

    const Inventory& containerInv = mWorld.getComponent<Inventory>(mOpenContainerEntity);
    const Inventory& pack = mWorld.getComponent<Inventory>(mPlayerEntity);

    if (mInput.wasKeyPressed(SDL_SCANCODE_A) || mInput.wasKeyPressed(SDL_SCANCODE_LEFT))
    {
        mContainerPackSide = true;
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_D) || mInput.wasKeyPressed(SDL_SCANCODE_RIGHT))
    {
        mContainerPackSide = false;
    }

    const bool shift = mInput.isKeyDown(SDL_SCANCODE_LSHIFT) || mInput.isKeyDown(SDL_SCANCODE_RSHIFT);

    // The mouse: a click on a row moves one, shift-click the stack.
    if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
    {
        const ContainerLayout layout = containerLayout();
        const float mx = static_cast<float>(mInput.mouseX());
        const float my = static_cast<float>(mInput.mouseY());
        if (mx >= layout.panelX && my >= ContainerLayout::kListY)
        {
            const int row = static_cast<int>((my - ContainerLayout::kListY) / ContainerLayout::kRowH);
            const bool onPack = mx < layout.boxX - ContainerLayout::kPadding * 0.5f;
            const int rows = storage::filledSlots(onPack ? pack : containerInv);
            if (row >= 0 && row < rows)
            {
                mContainerPackSide = onPack;
                (onPack ? mPackCursor : mContainerCursor) = row;
                containerMove(onPack, row, shift);
            }
        }
    }

    int& cursor = mContainerPackSide ? mPackCursor : mContainerCursor;
    const int rows = storage::filledSlots(mContainerPackSide ? pack : containerInv);
    if (rows > 0)
    {
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            cursor = (cursor + rows - 1) % rows;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            cursor = (cursor + 1) % rows;
        }
        cursor = std::clamp(cursor, 0, rows - 1);
        if (mInput.wasKeyPressed(SDL_SCANCODE_E) || mInput.wasKeyPressed(SDL_SCANCODE_RETURN))
        {
            containerMove(mContainerPackSide, cursor, shift);
        }
    }

    // Keep both cursors on a row after things moved.
    mPackCursor = std::clamp(mPackCursor, 0, std::max(0, storage::filledSlots(pack) - 1));
    mContainerCursor = std::clamp(mContainerCursor, 0, std::max(0, storage::filledSlots(containerInv) - 1));
}

void Game::renderContainerOverlay()
{
    if (!mContainerOpen || mOpenContainerEntity == kInvalidEntity || mRunState != RunState::Playing ||
        !mWorld.hasComponent<Inventory>(mOpenContainerEntity) || mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }

    const Inventory& containerInv = mWorld.getComponent<Inventory>(mOpenContainerEntity);
    const Inventory& pack = mWorld.getComponent<Inventory>(mPlayerEntity);
    const ContainerLayout layout = containerLayout();
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const auto rect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        mSpriteBatch.draw(pos, size, white, color);
    };
    const auto text = [&](const glm::vec2& pos, const std::string& s, const glm::vec4& color, float scale,
                          TextAlign align = TextAlign::Left)
    {
        mFont.drawText(mSpriteBatch, pos, s, color, scale, align);
    };
    const auto fit = [&](const std::string& s, float width, float scale)
    {
        return truncateHudText(s, static_cast<std::size_t>(std::max(4.0f, width / (6.0f * scale))));
    };

    rect(glm::vec2(layout.panelX, 0.0f), glm::vec2(layout.panelW, layout.panelH), kPanelBack);

    // Header: the container's name, how full it is.
    const std::string name = containerName(mOpenContainerEntity);
    const float right = layout.panelX + layout.panelW - ContainerLayout::kPadding;
    text(glm::vec2(layout.packX, 22.0f), fit(name, layout.panelW * 0.6f, 3.0f), kPanelInk, 3.0f);
    const int used = storage::filledSlots(containerInv);
    text(glm::vec2(right, 30.0f), std::to_string(used) + " of " + std::to_string(containerInv.maxSlots), kPanelDim,
         2.0f, TextAlign::Right);
    rect(glm::vec2(layout.packX, 64.0f), glm::vec2(layout.panelW - ContainerLayout::kPadding * 2.0f, 1.0f), kPanelRule);

    const auto column = [&](const Inventory& inv, float x, const std::string& title, const std::string& weightText,
                            bool active, int cursor)
    {
        text(glm::vec2(x, 78.0f), title, active ? kPanelAmber : kPanelDim, 2.0f);
        text(glm::vec2(x + layout.columnW, 82.0f), weightText, kPanelDim, 1.0f, TextAlign::Right);
        rect(glm::vec2(x, 100.0f), glm::vec2(layout.columnW, 1.0f), kPanelRule);

        float y = ContainerLayout::kListY;
        int row = 0;
        for (const InventorySlot& slot : inv.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            if (y + ContainerLayout::kRowH > layout.panelH - 40.0f) break;
            const ItemDef* item = mItemDatabase.find(slot.itemId);
            std::string label = item != nullptr ? packItemName(slot) : "Item #" + std::to_string(slot.itemId);
            const bool selected = active && row == cursor;
            if (selected)
            {
                rect(glm::vec2(x - 4.0f, y - 3.0f), glm::vec2(layout.columnW + 8.0f, ContainerLayout::kRowH), kPanelInk);
            }
            const glm::vec4 ink = selected ? kPanelOnInk : kPanelInk;
            const std::string countText = slot.count > 1 ? "x" + std::to_string(slot.count) : "";
            const bool worn = item != nullptr && (item->spoilHours > 0.0f || item->category == ItemCategory::Weapon) &&
                              slot.condition < 0.995f;
            std::string tail = countText;
            if (worn)
            {
                tail = std::to_string(static_cast<int>(std::round(slot.condition * 100.0f))) + "%" +
                       (countText.empty() ? "" : "  " + countText);
            }
            const float tailW = mFont.measureText(tail, 2.0f).x;
            text(glm::vec2(x, y), fit(label, layout.columnW - tailW - 12.0f, 2.0f), ink, 2.0f);
            if (!tail.empty())
            {
                const glm::vec4 tailInk = selected ? kPanelOnInk
                                                   : (worn && slot.condition < 0.25f ? kPanelBad : kPanelDim);
                text(glm::vec2(x + layout.columnW, y), tail, tailInk, 2.0f, TextAlign::Right);
            }
            y += ContainerLayout::kRowH;
            ++row;
        }
        if (row == 0)
        {
            text(glm::vec2(x, y), "Empty", kPanelDim, 2.0f);
        }
    };

    const auto kg = [&](const Inventory& inv)
    {
        std::ostringstream s;
        s << std::fixed << std::setprecision(1) << InventorySystem::totalWeight(inv, mItemDatabase) << " / "
          << std::setprecision(0) << inv.maxWeight << " kg";
        return s.str();
    };
    column(pack, layout.packX, "PACK", kg(pack), mContainerPackSide, mPackCursor);
    column(containerInv, layout.boxX, "INSIDE", kg(containerInv), !mContainerPackSide, mContainerCursor);
    rect(glm::vec2(layout.boxX - ContainerLayout::kPadding * 0.5f, 78.0f), glm::vec2(1.0f, layout.panelH - 130.0f),
         kPanelRule);

    text(glm::vec2(layout.packX, layout.panelH - 26.0f), "E move   Shift+E move all   A/D side   Esc close", kPanelDim,
         1.0f);
}
