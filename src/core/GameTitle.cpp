// The title (phase 13): Continue, New Life, New World (one screen of
// choices), The Town (this world's record, its dead, their journals),
// Settings, Controls, Credits, Quit. The town behind it is the live preview.
#include "core/GameShared.h"

using namespace gameshared;

namespace
{
enum TitleItem
{
    kItemContinue,
    kItemNewLife,
    kItemNewWorld,
    kItemTown,
    kItemSettings,
    kItemControls,
    kItemCredits,
    kItemQuit
};

enum NewWorldRow
{
    kRowMode,
    kRowThreat,
    kRowTone,
    kRowWorld,
    kRowDay,
    kRowSeed,
    kRowMake
};

const glm::vec4 kTitleRed(0.90f, 0.30f, 0.24f, 1.0f);

std::vector<std::string> wrapWords(const std::string& text, std::size_t maxChars)
{
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while (start < text.size())
    {
        std::size_t end = text.find(' ', start);
        if (end == std::string::npos) end = text.size();
        const std::string word = text.substr(start, end - start);
        if (!line.empty() && line.size() + 1 + word.size() > maxChars)
        {
            lines.push_back(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
        start = end + 1;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

int dayMinutesStep(int minutes, int delta)
{
    int index = 0;
    for (int i = 0; i < 3; ++i)
    {
        if (daylength::kChoices[i] == minutes) index = i;
    }
    return daylength::kChoices[((index + delta) % 3 + 3) % 3];
}
} // namespace

// ---------------------------------------------------------------------------
// Input

void Game::updateTitleScreen(float dtSeconds)
{
    mTitlePulseTimer += dtSeconds;
    mTitleConfirmNewWorldTimer = std::max(0.0f, mTitleConfirmNewWorldTimer - dtSeconds);
    if (mTitleConfirmNewWorldTimer <= 0.0f) mTitleConfirmNewWorld = false;
    mTitleStatusTimer = std::max(0.0f, mTitleStatusTimer - dtSeconds);
    if (mTitleStatusTimer <= 0.0f) mTitleStatusMessage.clear();

    // The checkpoint lives on disk: re-read it after input (slot changes,
    // deletes) or once a second, not every tick.
    mTitleCheckpointRefreshTimer -= dtSeconds;
    if (mTitleCheckpointRefreshTimer <= 0.0f || mInput.anyKeyPressed()) refreshTitleCheckpoint();

    if (mShowControls)
    {
        if (mInput.pressed(Action::UiConfirm)) mShowControls = false;
        return;
    }
    switch (mTitleView)
    {
    case TitleView::Main: updateTitleMain(); break;
    case TitleView::NewWorld: updateNewWorld(); break;
    case TitleView::Town: updateTown(); break;
    case TitleView::Credits:
        if (mInput.pressed(Action::UiConfirm) || mInput.pressed(Action::UiSpace)) mTitleView = TitleView::Main;
        break;
    }
}

void Game::updateTitleMain()
{
    const int count = static_cast<int>(kTitleMenuItems.size());
    if (mInput.pressed(Action::UiUp) || mInput.pressed(Action::MoveUp))
    {
        mTitleMenuSelection = wrapIndex(mTitleMenuSelection - 1, count);
        if (mTitleMenuSelection == kItemContinue && !mTitleHasCheckpoint) mTitleMenuSelection = count - 1;
    }
    if (mInput.pressed(Action::UiDown) || mInput.pressed(Action::MoveDown))
    {
        mTitleMenuSelection = wrapIndex(mTitleMenuSelection + 1, count);
        if (mTitleMenuSelection == kItemContinue && !mTitleHasCheckpoint) mTitleMenuSelection = kItemNewLife;
    }
    if (mTitleMenuSelection == kItemContinue && !mTitleHasCheckpoint) mTitleMenuSelection = kItemNewLife;

    // Left and Right: the save slot (each is its own town).
    const int slotDelta = (mInput.pressed(Action::UiRight) || mInput.pressed(Action::MoveRight) ? 1 : 0) -
                          (mInput.pressed(Action::UiLeft) || mInput.pressed(Action::MoveLeft) ? 1 : 0);
    if (slotDelta != 0)
    {
        const int newIndex = wrapIndex(mTitleSaveSlotIndex + slotDelta, static_cast<int>(kTitleSaveSlots.size()));
        if (loadSlotAndWorld(kTitleSaveSlots[static_cast<std::size_t>(newIndex)]))
        {
            mTitleSaveSlotIndex = newIndex;
            mTitleStatusMessage = std::string("Slot: ") + kTitleSaveLabels[static_cast<std::size_t>(newIndex)];
            refreshTitleCheckpoint();
        }
        else
        {
            mTitleStatusMessage = "Unable to load slot data.";
        }
        mTitleStatusTimer = 2.0f;
    }

    if (!(mInput.pressed(Action::UiConfirm) || mInput.pressed(Action::UiSpace))) return;
    switch (mTitleMenuSelection)
    {
    case kItemContinue:
        if (mTitleHasCheckpoint)
        {
            mIgnoreCheckpointRestoreOnce = false;
            startNewRun();
        }
        break;
    case kItemNewLife: // the same town, someone new (through the character screen)
        mSaveManager.deleteCheckpoint();
        mIgnoreCheckpointRestoreOnce = true;
        syncPresentationSettingsToSave();
        mCreateCharacterOnStart = true;
        startNewRun();
        break;
    case kItemNewWorld:
        mTitleView = TitleView::NewWorld;
        mNewWorldRow = 0;
        mNewWorldSeed.clear();
        mNewWorldDayMinutes = mSaveManager.presentationSettings().dayMinutes;
        mTitleConfirmNewWorld = false;
        break;
    case kItemTown:
        mTitleView = TitleView::Town;
        mTownReading = false;
        mNotebookCursor = 0;
        mReader = ReaderState{};
        break;
    case kItemSettings: openSettings(); break;
    case kItemControls: mShowControls = true; break;
    case kItemCredits: mTitleView = TitleView::Credits; break;
    default: mIsRunning = false; break;
    }
}

void Game::updateNewWorld()
{
    const int rows = static_cast<int>(kNewWorldRows.size());
    if (mInput.pressed(Action::UiUp) || mInput.pressed(Action::MoveUp)) mNewWorldRow = wrapIndex(mNewWorldRow - 1, rows);
    if (mInput.pressed(Action::UiDown) || mInput.pressed(Action::MoveDown)) mNewWorldRow = wrapIndex(mNewWorldRow + 1, rows);
    if (mNewWorldRow != kRowMake) mTitleConfirmNewWorld = false;

    // The seed is typed (digits; Backspace takes one off). Only the number
    // row and keypad digits, whatever WASD is bound to.
    if (mNewWorldRow == kRowSeed)
    {
        const int digit = mInput.digitTyped();
        if (digit >= 0 && mNewWorldSeed.size() < 9) mNewWorldSeed.push_back(static_cast<char>('0' + digit));
        if (mInput.pressed(Action::UiErase) && !mNewWorldSeed.empty()) mNewWorldSeed.pop_back();
    }

    int delta = 0;
    if (mInput.pressed(Action::UiLeft) || (mNewWorldRow != kRowSeed && mInput.pressed(Action::MoveLeft))) delta = -1;
    if (mInput.pressed(Action::UiRight) || (mNewWorldRow != kRowSeed && mInput.pressed(Action::MoveRight))) delta = 1;
    const bool confirm = mInput.pressed(Action::UiConfirm);
    if (confirm && mNewWorldRow != kRowMake && mNewWorldRow != kRowSeed) delta = 1;
    if (delta != 0)
    {
        switch (mNewWorldRow)
        {
        case kRowMode: mTitleGameModeIndex = wrapIndex(mTitleGameModeIndex + delta, static_cast<int>(kTitleGameModeNames.size())); break;
        case kRowThreat: mTitleModeIndex = wrapIndex(mTitleModeIndex + delta, static_cast<int>(kTitleModeNames.size())); break;
        case kRowTone: mTitleToneIndex = wrapIndex(mTitleToneIndex + delta, static_cast<int>(kTitleToneNames.size())); break;
        case kRowWorld: mTitleFreshStart = !mTitleFreshStart; break;
        case kRowDay: mNewWorldDayMinutes = dayMinutesStep(mNewWorldDayMinutes, delta); break;
        default: break;
        }
    }
    if (!confirm || mNewWorldRow != kRowMake) return;
    // A town with a past asks once more before it is gone.
    const bool hasPast = !mSaveManager.characterHistory().empty() || mTitleHasCheckpoint;
    if (hasPast && !mTitleConfirmNewWorld)
    {
        mTitleConfirmNewWorld = true;
        mTitleConfirmNewWorldTimer = 4.0f;
        return;
    }
    startNewWorld();
}

void Game::startNewWorld()
{
    mTitleConfirmNewWorld = false;
    mTitleConfirmNewWorldTimer = 0.0f;
    std::optional<std::uint32_t> seed;
    if (!mNewWorldSeed.empty()) seed = static_cast<std::uint32_t>(std::stoul(mNewWorldSeed));
    if (!mSaveManager.resetCurrentSlotWorld(true, seed))
    {
        mTitleStatusMessage = "Failed to reset world for this slot.";
        mTitleStatusTimer = 3.0f;
        return;
    }
    {
        // Set once, at the world's making (phases 10 and 13).
        RunPresentationSettings settings = mSaveManager.presentationSettings();
        settings.livingWorld = !mTitleFreshStart;
        settings.dayMinutes = daylength::clampMinutes(mNewWorldDayMinutes);
        mSaveManager.setPresentationSettings(settings);
    }
    syncPresentationSettingsToSave();
    // The teaching seen in this slot goes with the old town (per save).
    mSettings.taught.erase(mActiveSaveSlot);
    mSettings.hintsShown.erase(mActiveSaveSlot);
    if (!mHarness) saveSettings();
    if (!loadSlotAndWorld(kTitleSaveSlots[static_cast<std::size_t>(mTitleSaveSlotIndex)]))
    {
        mTitleStatusMessage = "World reset complete, but reload failed.";
        mTitleStatusTimer = 3.0f;
        return;
    }
    mTitleView = TitleView::Main;
    mIgnoreCheckpointRestoreOnce = true;
    mCreateCharacterOnStart = true;
    startNewRun();
}

void Game::updateTown()
{
    const auto& history = mSaveManager.characterHistory();
    if (mTownReading)
    {
        if (mInput.pressed(Action::UiDown) || mInput.pressed(Action::MoveDown)) ++mReader.scroll;
        if (mInput.pressed(Action::UiUp) || mInput.pressed(Action::MoveUp)) mReader.scroll = std::max(0, mReader.scroll - 1);
        if (mInput.pressed(Action::UiErase) || mInput.pressed(Action::UiConfirm)) mTownReading = false;
        return;
    }
    const int rows = static_cast<int>(history.size());
    if (rows == 0) return;
    if (mInput.pressed(Action::UiDown) || mInput.pressed(Action::MoveDown)) mNotebookCursor = std::min(rows - 1, mNotebookCursor + 1);
    if (mInput.pressed(Action::UiUp) || mInput.pressed(Action::MoveUp)) mNotebookCursor = std::max(0, mNotebookCursor - 1);
    mNotebookCursor = std::clamp(mNotebookCursor, 0, rows - 1);
    if (!mInput.pressed(Action::UiConfirm)) return;
    // Newest first. A journal still lying with its grave is found there, in
    // the town; the ones read, kept or carried out are here.
    const int index = rows - 1 - mNotebookCursor;
    const CharacterHistoryRecord& life = history[static_cast<std::size_t>(index)];
    int grave = -1;
    const auto& graves = mSaveManager.graves();
    for (std::size_t i = 0; i < graves.size(); ++i)
    {
        if (graves[i].runIndex == life.runIndex && graves[i].name == life.name) grave = static_cast<int>(i);
    }
    if (grave >= 0 && !life.journalRead && !graves[static_cast<std::size_t>(grave)].buried)
    {
        mTitleStatusMessage = life.name + "'s journal lies with them. Find the grave.";
        mTitleStatusTimer = 3.0f;
        return;
    }
    mReader = ReaderState{};
    mReader.open = true;
    mReader.history = index;
    mReader.grave = grave;
    mTownReading = true;
}

// ---------------------------------------------------------------------------
// Drawing

void Game::renderTitleScreen()
{
    if (mRunState != RunState::TitleScreen) return;

    // The town itself is the backdrop (the title camera drifts over it);
    // the menu sits on the darker left.
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    constexpr int kStrips = 48;
    for (int i = 0; i < kStrips; ++i)
    {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kStrips);
        const float fadeT = std::clamp((t - 0.30f) / 0.70f, 0.0f, 1.0f);
        const float alpha = 0.92f - 0.50f * fadeT * fadeT * (3.0f - 2.0f * fadeT);
        // Strips meet exactly; overlapping edges would show as dark lines.
        const float x0 = std::floor(w * static_cast<float>(i) / kStrips);
        const float x1 = std::floor(w * static_cast<float>(i + 1) / kStrips);
        mSpriteBatch.draw(glm::vec2(x0, 0.0f), glm::vec2(x1 - x0, h), uiUv, glm::vec4(0.02f, 0.025f, 0.03f, alpha));
    }

    switch (mTitleView)
    {
    case TitleView::Main: renderTitleMain(); break;
    case TitleView::NewWorld: renderNewWorld(); break;
    case TitleView::Town: renderTown(); break;
    case TitleView::Credits: renderTitleMain(); renderTitleCredits(); break;
    }

    // The version, small, bottom-right; the slot and town above it.
    const float left = std::max(32.0f, std::floor(w * 0.06f));
    const std::string version = std::string("v") + kGameVersion + (mDemo ? "  demo" : "");
    const glm::vec4 faint(kPanelDim.r, kPanelDim.g, kPanelDim.b, 0.7f);
    mFont.drawText(mSpriteBatch, glm::vec2(w - 12.0f, h - 16.0f), version, faint, 1.0f, TextAlign::Right);
    if (mTitleView != TitleView::Town)
    {
        const std::string seed = std::string(kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)]) +
                                 " slot  -  town " + std::to_string(mSaveManager.worldSeed());
        mFont.drawText(mSpriteBatch, glm::vec2(w - left, h - 30.0f), seed, faint, 1.0f, TextAlign::Right);
    }

    if (!mTitleStatusMessage.empty() && mTitleStatusTimer > 0.0f)
    {
        const float alpha = std::clamp(mTitleStatusTimer / 0.4f, 0.0f, 1.0f);
        const glm::vec2 size = mFont.measureText(mTitleStatusMessage, 2.0f);
        const glm::vec2 pos(std::floor((w - size.x) * 0.5f), h - 92.0f);
        mSpriteBatch.draw(pos - glm::vec2(14.0f, 8.0f), size + glm::vec2(28.0f, 16.0f), uiUv,
                          glm::vec4(kPanelBack.r, kPanelBack.g, kPanelBack.b, 0.9f * alpha));
        mFont.drawText(mSpriteBatch, pos, mTitleStatusMessage, glm::vec4(kPanelAmber.r, kPanelAmber.g, kPanelAmber.b, alpha), 2.0f);
    }
}

void Game::renderTitleMain()
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const glm::vec4 ink = kPanelInk;
    const glm::vec4 dimInk = kPanelDim;
    const float left = std::max(32.0f, std::floor(w * 0.06f));
    float y = std::max(28.0f, std::floor(h * 0.08f));

    // Title.
    const float titleScale = (w >= 1100.0f && h >= 680.0f) ? 6.0f : 4.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left, y), "DEAD PIXEL", ink, titleScale);
    y += mFont.lineHeight(titleScale) + 2.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left + 2.0f, y), "SURVIVAL", kTitleRed, titleScale * 0.5f);
    y += mFont.lineHeight(titleScale * 0.5f) + 10.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left + 2.0f, y), "The town remembers every life you lose in it.", dimInk, 1.0f);
    y += 26.0f;

    // The slot, changed with Left and Right.
    const std::string slot = std::string("< ") + kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)] + " >";
    mFont.drawText(mSpriteBatch, glm::vec2(left, y), "SLOT", dimInk, 1.0f);
    mFont.drawText(mSpriteBatch, glm::vec2(left + 48.0f, y - 4.0f), slot, kPanelAmber, 2.0f);
    y += 30.0f;

    // Menu.
    const float rowH = 28.0f;
    const float rowW = std::min(560.0f, w - left * 2.0f);
    for (int i = 0; i < static_cast<int>(kTitleMenuItems.size()); ++i)
    {
        const float rowY = y + static_cast<float>(i) * rowH;
        const bool selected = i == mTitleMenuSelection;
        const bool disabled = i == kItemContinue && !mTitleHasCheckpoint;
        if (selected) mSpriteBatch.draw(glm::vec2(left - 14.0f, rowY - 6.0f), glm::vec2(rowW, rowH - 2.0f), uiUv, ink);
        glm::vec4 color = selected ? kPanelOnInk : ink;
        if (disabled) color = glm::vec4(0.42f, 0.41f, 0.39f, 1.0f);
        mFont.drawText(mSpriteBatch, glm::vec2(left, rowY), kTitleMenuItems[static_cast<std::size_t>(i)], color, 2.0f);
    }
    y += static_cast<float>(kTitleMenuItems.size()) * rowH + 12.0f;

    // What the chosen item does.
    const auto& history = mSaveManager.characterHistory();
    std::string about;
    switch (mTitleMenuSelection)
    {
    case kItemContinue:
        about = mTitleHasCheckpoint
                    ? "Pick up where " + mTitleCheckpointName + " left off, day " + std::to_string(mTitleCheckpointDay) + "."
                    : "No run in progress in this slot.";
        break;
    case kItemNewLife:
        about = legacy::runHeadline(mSaveManager.runNumber(), static_cast<int>(history.size())) +
                ". Someone new wakes up in this town.";
        break;
    case kItemNewWorld: about = "A different town: its mode, threat, tone and day length, chosen on one page."; break;
    case kItemTown: about = "What this town remembers: every life, every grave, their journals."; break;
    case kItemSettings: about = "Sound, screen, game and keys."; break;
    case kItemControls: about = "Every key, by what you are doing."; break;
    case kItemCredits: about = "Who made this."; break;
    default: about = "The town will still be here."; break;
    }
    const std::size_t aboutChars = static_cast<std::size_t>(std::max(16.0f, (rowW - 16.0f) / 12.0f));
    for (const std::string& line : wrapWords(about, aboutChars))
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), line, dimInk, 2.0f);
        y += mFont.lineHeight(2.0f) + 4.0f;
    }

    // The town remembers, in brief (The Town has the rest).
    if (!history.empty())
    {
        const int graves = static_cast<int>(mSaveManager.graves().size());
        const int gotOut = static_cast<int>(std::count_if(history.begin(), history.end(),
                                                          [](const CharacterHistoryRecord& r) { return r.escaped; }));
        std::string line = std::to_string(history.size()) + (history.size() == 1 ? " life, " : " lives, ") +
                           std::to_string(mSaveManager.totalDaysSurvived()) + " days";
        if (graves > 0) line += ", " + std::to_string(graves) + (graves == 1 ? " grave" : " graves");
        if (gotOut > 0) line += ", " + std::to_string(gotOut) + " got out";
        const glm::vec2 size = mFont.measureText(line, 2.0f);
        const float cardX = w - left - size.x - 36.0f;
        const float cardY = h - 72.0f - size.y - 28.0f;
        if (cardX > left + rowW + 24.0f || cardY > y + 12.0f)
        {
            mSpriteBatch.draw(glm::vec2(cardX, cardY), size + glm::vec2(36.0f, 40.0f), uiUv, kPanelBack);
            mFont.drawText(mSpriteBatch, glm::vec2(cardX + 18.0f, cardY + 10.0f), "THE TOWN REMEMBERS", dimInk, 1.0f);
            mFont.drawText(mSpriteBatch, glm::vec2(cardX + 18.0f, cardY + 26.0f), line, ink, 2.0f);
        }
    }

    mFont.drawText(mSpriteBatch, glm::vec2(left, h - 30.0f), "Up/Down choose    Left/Right slot    Enter select", dimInk, 1.0f);
}

void Game::renderNewWorld()
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const float left = std::max(32.0f, std::floor(w * 0.06f));
    float y = std::max(28.0f, std::floor(h * 0.08f));
    mFont.drawText(mSpriteBatch, glm::vec2(left, y), "NEW WORLD", kPanelInk, 4.0f);
    y += mFont.lineHeight(4.0f) + 8.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left + 2.0f, y), "A different town. Chosen once, kept for every life in it.", kPanelDim, 1.0f);
    y += 30.0f;

    const float rowH = 30.0f;
    const float rowW = std::min(640.0f, w - left * 2.0f);
    const float valueX = left + 220.0f;
    const auto valueOf = [&](int row) -> std::string
    {
        switch (row)
        {
        case kRowMode: return kTitleGameModeNames[static_cast<std::size_t>(mTitleGameModeIndex)];
        case kRowThreat: return kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)];
        case kRowTone: return kTitleToneNames[static_cast<std::size_t>(mTitleToneIndex)];
        case kRowWorld: return mTitleFreshStart ? "FRESH START" : "LIVING WORLD";
        case kRowDay: return std::to_string(mNewWorldDayMinutes) + " MIN A DAY";
        case kRowSeed: return mNewWorldSeed.empty() ? "RANDOM" : mNewWorldSeed;
        default: return {};
        }
    };
    for (int i = 0; i < static_cast<int>(kNewWorldRows.size()); ++i)
    {
        const float rowY = y + static_cast<float>(i) * rowH + (i == kRowMake ? 12.0f : 0.0f);
        const bool selected = i == mNewWorldRow;
        if (selected) mSpriteBatch.draw(glm::vec2(left - 14.0f, rowY - 6.0f), glm::vec2(rowW, rowH - 2.0f), uiUv, kPanelInk);
        const glm::vec4 ink = selected ? kPanelOnInk : kPanelInk;
        std::string name = kNewWorldRows[static_cast<std::size_t>(i)];
        if (i == kRowMake && mTitleConfirmNewWorld) name = "Enter again: this town is gone";
        mFont.drawText(mSpriteBatch, glm::vec2(left, rowY), name, i == kRowMake && mTitleConfirmNewWorld && !selected ? kTitleRed : ink, 2.0f);
        std::string value = valueOf(i);
        if (!value.empty())
        {
            if (selected && i == kRowSeed) value += "_";
            else if (selected) value = "< " + value + " >";
            mFont.drawText(mSpriteBatch, glm::vec2(valueX, rowY), value, selected ? kPanelOnInk : kPanelAmber, 2.0f);
        }
    }
    y += static_cast<float>(kNewWorldRows.size()) * rowH + 30.0f;

    std::string about;
    switch (mNewWorldRow)
    {
    case kRowMode: about = kTitleGameModeDescriptions[static_cast<std::size_t>(mTitleGameModeIndex)]; break;
    case kRowThreat: about = kTitleModeDescriptions[static_cast<std::size_t>(mTitleModeIndex)]; break;
    case kRowTone: about = kTitleToneDescriptions[static_cast<std::size_t>(mTitleToneIndex)]; break;
    case kRowWorld:
        about = mTitleFreshStart ? "Fresh Start: nothing carries over from one life to the next."
                                 : "Living World: graves, bases and an older town carry over.";
        break;
    case kRowDay: about = "Real minutes in a game day. Hunger and the clock follow it; a fight does not."; break;
    case kRowSeed: about = "Type digits for a town you can make again. Blank is random. Backspace takes one off."; break;
    default:
        about = mSaveManager.characterHistory().empty() ? "Wake up in it."
                                                        : "This slot's town and everything it remembers will be gone.";
        break;
    }
    const std::size_t chars = static_cast<std::size_t>(std::max(16.0f, (rowW - 16.0f) / 12.0f));
    for (const std::string& line : wrapWords(about, chars))
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), line, kPanelDim, 2.0f);
        y += mFont.lineHeight(2.0f) + 4.0f;
    }
    mFont.drawText(mSpriteBatch, glm::vec2(left, h - 30.0f), "Up/Down choose    Left/Right change    Enter: make it    Esc: back",
                   kPanelDim, 1.0f);
}

void Game::renderTown()
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const float panelW = std::min(w - 32.0f, std::max(dockedPanelWidth(true, mWindowWidth), 760.0f));
    const float panelX = std::floor((w - panelW) * 0.5f);
    constexpr float kPad = 24.0f;
    const float left = panelX + kPad;
    const float textW = panelW - kPad * 2.0f;
    mSpriteBatch.draw(glm::vec2(panelX, 0.0f), glm::vec2(panelW, h), uiUv, glm::vec4(kPanelBack.r, kPanelBack.g, kPanelBack.b, 0.96f));
    mFont.drawText(mSpriteBatch, glm::vec2(left, 22.0f), mTownReading ? "JOURNAL" : "THE TOWN", kPanelInk, 3.0f);
    const std::string sub = std::string(kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)]) + " slot, town " +
                            std::to_string(mSaveManager.worldSeed());
    mFont.drawText(mSpriteBatch, glm::vec2(left + textW, 30.0f), sub, kPanelDim, 1.0f, TextAlign::Right);
    mSpriteBatch.draw(glm::vec2(left, 64.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);

    const float footY = h - 30.0f;
    float top = 80.0f;
    std::string hint;
    if (mTownReading)
    {
        renderReaderPage(left, textW, top, footY - 12.0f);
        hint = "Up/Down: read on   Enter or Esc: back";
    }
    else
    {
        // The record: lives, graves, got out, days.
        const auto& history = mSaveManager.characterHistory();
        const int graves = static_cast<int>(mSaveManager.graves().size());
        const int gotOut = static_cast<int>(std::count_if(history.begin(), history.end(),
                                                          [](const CharacterHistoryRecord& r) { return r.escaped; }));
        const int stayed = static_cast<int>(std::count_if(history.begin(), history.end(),
                                                          [](const CharacterHistoryRecord& r) { return r.retired && !r.escaped; }));
        std::string record = "Lives " + std::to_string(history.size()) + "   Graves " + std::to_string(graves) + "   Got out " +
                             std::to_string(gotOut) + "   Stayed " + std::to_string(stayed) + "   Days " +
                             std::to_string(mSaveManager.totalDaysSurvived());
        if (!mSaveManager.livingWorld()) record += "   Fresh Start";
        mFont.drawText(mSpriteBatch, glm::vec2(left, top), record, kPanelAmber, 2.0f);
        top += 34.0f;
        mFont.drawText(mSpriteBatch, glm::vec2(left, top), "THE CEMETERY AND THE REST, NEWEST FIRST", kPanelDim, 1.0f);
        top += 20.0f;
        renderBeforeYouPage(left, textW, top, footY - 12.0f);
        hint = history.empty() ? "Esc: back" : "Up/Down: choose   Enter: read their journal   Esc: back";
    }
    mSpriteBatch.draw(glm::vec2(left, footY - 8.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);
    mFont.drawText(mSpriteBatch, glm::vec2(left, footY), hint, kPanelDim, 1.0f);
}

void Game::renderTitleCredits()
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const glm::vec2 size(std::min(560.0f, w - 40.0f), 220.0f);
    const glm::vec2 pos(std::floor((w - size.x) * 0.5f), std::floor((h - size.y) * 0.5f));
    mSpriteBatch.draw(pos, size, uiUv, glm::vec4(kPanelBack.r, kPanelBack.g, kPanelBack.b, 0.97f));
    const float cx = pos.x + size.x * 0.5f;
    mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 24.0f), "CREDITS", kPanelInk, 3.0f, TextAlign::Center);
    mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 74.0f), "Dead Pixel Survival", kPanelInk, 2.0f, TextAlign::Center);
    mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 102.0f), "Design and code: L. Bartolome", kPanelDim, 2.0f, TextAlign::Center);
    mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 128.0f), "C++20, SDL2, OpenGL", kPanelDim, 2.0f, TextAlign::Center);
    mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + size.y - 30.0f), "Enter to close", kPanelDim, 1.0f, TextAlign::Center);
}
