// The journal and the notebook (phase 10). A character with a notebook writes
// down what happens (legacy/Journal.h); the notebook (J) holds six pages in
// tabs, docked right: leads, the way out, people, those who came before (their
// journals read with Enter), know-how, and this life's own journal. A dead
// character's journal is read at their grave in the same panel.
#include "core/GameShared.h"

#include "world/BuildingKinds.h"
#include "world/WeatherRules.h"

using namespace gameshared;

namespace
{
// Once-a-life entries (Game::mJournalFlags).
enum JournalFlag : int
{
    kFlagStart = 1,
    kFlagFirstNight = 2,
    kFlagPowerOut = 4,
    kFlagTapsDry = 8,
    kFlagFirstKill = 16
};

constexpr std::array<const char*, 6> kNotebookTabs = {"Leads", "The way out", "People", "Before you", "Know-how", "Journal"};
constexpr int kTabLeads = 0;
constexpr int kTabWayOut = 1;
constexpr int kTabPeople = 2;
constexpr int kTabBeforeYou = 3;
constexpr int kTabKnowHow = 4;
constexpr int kTabJournal = 5;
constexpr float kGraveFoundTiles = 2.0f;
constexpr float kReaderReachTiles = 2.5f;
constexpr float kReadDepression = 3.0f;
constexpr float kReadEscapedRelief = 10.0f;

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

std::string lowered(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const char* woundWord(WoundType type)
{
    switch (type)
    {
    case WoundType::Cut: return "cut";
    case WoundType::Fracture: return "fracture";
    case WoundType::Bite: return "bite";
    case WoundType::Bruise: return "bruise";
    }
    return "wound";
}
} // namespace

void Game::loadJournalLines(const std::filesystem::path& assetsRoot)
{
    std::string error;
    const std::string path = (assetsRoot / "data" / "journal_lines.json").string();
    if (!journal::loadLines(path, mJournalLines, &error))
    {
        mJournalLines = journal::defaults();
        std::cerr << "[Journal] " << path << ": " << error << " (using the built-in words)\n";
    }
}

legacy::LegacyState Game::legacyState() const
{
    legacy::LegacyState state;
    state.graves = mLegacyGraves.empty() ? mSaveManager.graves() : mLegacyGraves;
    state.retirements = mSaveManager.retirements();
    state.history = mSaveManager.characterHistory();
    return state;
}

void Game::journalCheckpoint(RunCheckpoint& cp) const
{
    cp.journalSaved = true;
    cp.journal = mJournal;
    cp.journalFlags = mJournalFlags;
    cp.journalDays = mJournalDays;
    cp.journalSeason = mJournalSeason;
}

void Game::restoreJournal(const RunCheckpoint& cp, bool restored)
{
    if (restored && cp.journalSaved)
    {
        mJournal = cp.journal;
        mJournalFlags = cp.journalFlags;
        mJournalDays = cp.journalDays;
        mJournalSeason = cp.journalSeason;
    }
    // Wounds already had are not new.
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Wounds>(mPlayerEntity))
    {
        mJournalWounds = mWorld.getComponent<Wounds>(mPlayerEntity).active.size();
    }
    reconcileMarkers();
}

void Game::resetJournal()
{
    mStayedForGood = false;
    mStayConfirm = 0;
    mJournal.clear();
    mJournalFlags = 0;
    mJournalDays = 0;
    mJournalSeason = -1;
    mJournalWounds = 0;
    mJournalHorde = false;
    mJournalCrisis = false;
    mGravesFound.clear();
    closeReader();
}

void Game::noteJournal(journal::Kind kind, const std::string& a, const std::string& b, int n)
{
    if (!playerHasItem(mItems.notebook)) return; // nothing to write in
    const journal::Event event{mCurrentDay, kind, a, b, n};
    const journal::Entry entry = journal::entryFor(event, mSaveManager.currentRunSeed(), mJournalLines);
    if (journal::add(mJournal, entry))
    {
        std::cout << "[Journal] day " << entry.day << " (" << journal::key(kind) << "): " << entry.text << '\n';
    }
}

void Game::updateJournal(float)
{
    if (mPlayerEntity == kInvalidEntity || mRunState != RunState::Playing) return;
    const bool notebook = playerHasItem(mItems.notebook);
    if (notebook && (mJournalFlags & kFlagStart) == 0)
    {
        mJournalFlags |= kFlagStart;
        noteJournal(journal::Kind::Start);
    }
    const auto once = [&](int flag, bool happened, journal::Kind kind)
    {
        if (!happened || (mJournalFlags & flag) != 0) return;
        mJournalFlags |= flag;
        noteJournal(kind);
    };
    once(kFlagFirstNight, mCurrentDay >= 2 && mGameHours >= 6.0, journal::Kind::FirstNight);
    once(kFlagPowerOut, mPowerOutAnnounced, journal::Kind::PowerOut);
    once(kFlagTapsDry, mFirstDays.waterOffAnnounced, journal::Kind::TapsDry);
    once(kFlagFirstKill, mRunStats.kills > 0, journal::Kind::FirstKill);

    // A new wound (bruises are not written down).
    if (mWorld.hasComponent<Wounds>(mPlayerEntity))
    {
        const std::vector<Wound>& wounds = mWorld.getComponent<Wounds>(mPlayerEntity).active;
        for (std::size_t i = mJournalWounds; i < wounds.size(); ++i)
        {
            const Wound& w = wounds[i];
            const std::string part = woundrules::partName(w.part);
            if (w.type == WoundType::Bite) noteJournal(journal::Kind::Bitten, part);
            else if (w.type != WoundType::Bruise) noteJournal(journal::Kind::Wounded, part, woundWord(w.type));
        }
        mJournalWounds = wounds.size();
    }
    // A horde gone by; the first day of a crisis.
    if (mHorde.active) mJournalHorde = true;
    else if (mJournalHorde)
    {
        mJournalHorde = false;
        noteJournal(journal::Kind::HordePassed);
    }
    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        const bool crisis = mWorld.getComponent<MentalState>(mPlayerEntity).inCrisis;
        if (crisis && !mJournalCrisis) noteJournal(journal::Kind::LowPoint);
        mJournalCrisis = crisis;
    }
    // Ten days, twenty...; the season turning.
    if (mCurrentDay >= 10 && (mCurrentDay / 10) * 10 > mJournalDays)
    {
        mJournalDays = (mCurrentDay / 10) * 10;
        noteJournal(journal::Kind::Days, {}, {}, mJournalDays);
    }
    const int season = static_cast<int>(weatherrules::seasonOf(mCurrentDay));
    if (mJournalSeason >= 0 && season != mJournalSeason)
    {
        noteJournal(journal::Kind::SeasonTurned, lowered(weatherrules::seasonName(weatherrules::seasonOf(mCurrentDay))));
    }
    mJournalSeason = season;

    // A grave of the line found (once a life each).
    const glm::vec2 here = playerWorldCenter();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    for (std::size_t i = 0; i < mLegacyGraves.size(); ++i)
    {
        const GraveRecord& g = mLegacyGraves[i];
        if (g.risen || !g.ofBloodline()) continue;
        if (std::find(mGravesFound.begin(), mGravesFound.end(), static_cast<int>(i)) != mGravesFound.end()) continue;
        if (glm::length(glm::vec2(g.x, g.y) + glm::vec2(kSpriteSize * 0.5f) - here) > kGraveFoundTiles * tile) continue;
        mGravesFound.push_back(static_cast<int>(i));
        noteJournal(journal::Kind::GraveFound, g.name, {}, g.day);
    }

    // The reader closes when they walk away from the grave.
    if (mReader.open && mReader.atGrave && mReader.grave >= 0 && mReader.grave < static_cast<int>(mLegacyGraves.size()))
    {
        const GraveRecord& g = mLegacyGraves[static_cast<std::size_t>(mReader.grave)];
        if (glm::length(glm::vec2(g.x, g.y) + glm::vec2(kSpriteSize * 0.5f) - here) > kReaderReachTiles * tile)
        {
            closeReader();
            mShowNotebook = false;
        }
    }
}

std::vector<journal::Entry> Game::finishJournal(journal::Kind end, const std::string& lastLine)
{
    // A retirement always leaves its last words; any other end only in a notebook.
    const bool always = end == journal::Kind::EndRetired || end == journal::Kind::EndStayed;
    if (!always && !playerHasItem(mItems.notebook)) return {};
    std::vector<journal::Entry> out = playerHasItem(mItems.notebook) ? mJournal : std::vector<journal::Entry>{};
    journal::Entry last = journal::entryFor(journal::Event{mCurrentDay, end, {}, {}, 0}, mSaveManager.currentRunSeed(), mJournalLines);
    if (!lastLine.empty()) last.text = lastLine + " " + last.text;
    journal::add(out, last);
    std::cout << "[Journal] " << mCurrentCharacterName << "'s journal ends with " << out.size() << " entries: " << last.text << '\n';
    return out;
}

std::string Game::markerName(const InventorySlot& slot) const
{
    const int grave = slot.loaded - 1;
    if (grave < 0 || grave >= static_cast<int>(mLegacyGraves.size())) return "Marker";
    return "Marker: " + mLegacyGraves[static_cast<std::size_t>(grave)].name;
}

// ---------------------------------------------------------------------------
// Reading

bool Game::learnFromGraveNotebook(int graveIndex)
{
    if (graveIndex < 0 || graveIndex >= static_cast<int>(mLegacyGraves.size())) return false;
    GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(graveIndex)];
    if (!grave.notebook) return false;
    const clues::Mask before = mLeads;
    clues::transfer(mLeads, mGateSeen, static_cast<clues::Mask>(grave.leads), grave.seen);
    for (int lead = 1; lead <= clues::kLeadCount; ++lead)
    {
        if ((mLeads & clues::bit(lead)) != 0 && (before & clues::bit(lead)) == 0)
        {
            std::cout << "[Clues] lead " << lead << " learned (" << grave.name << "'s notebook)\n";
        }
    }
    grave.notebook = false; // read: it is this character's now
    return clues::count(mLeads) > clues::count(before);
}

void Game::openReader(int graveIndex, int historyIndex, bool atGrave)
{
    mReader = ReaderState{};
    mReader.open = true;
    mReader.grave = graveIndex;
    mReader.history = historyIndex;
    mReader.atGrave = atGrave;
    mShowNotebook = true;
    mShowInventory = false;
    mShowCrafting = false;
    mContainerOpen = false;
    closeTalk();
    markJournalRead(historyIndex, graveIndex);
    const std::string name = historyIndex >= 0 ? mSaveManager.characterHistory()[static_cast<std::size_t>(historyIndex)].name
                           : graveIndex >= 0   ? mLegacyGraves[static_cast<std::size_t>(graveIndex)].name
                                               : std::string("?");
    std::cout << "[Journal] reading " << name << "'s journal" << (atGrave ? " at the grave" : " in the notebook") << '\n';
}

void Game::closeReader()
{
    mReader = ReaderState{};
}

void Game::markJournalRead(int historyIndex, int graveIndex)
{
    const bool learned = learnFromGraveNotebook(graveIndex);
    if (learned)
    {
        mInteractionMessage = "Pages about a gate in it. J, then Right, to read them.";
        mInteractionMessageTimer = 4.0f;
    }
    if (historyIndex < 0 || historyIndex >= static_cast<int>(mSaveManager.characterHistory().size())) return;
    const CharacterHistoryRecord& seen = mSaveManager.characterHistory()[static_cast<std::size_t>(historyIndex)];
    CharacterHistoryRecord* life = mSaveManager.lifeRecord(seen.runIndex, seen.name);
    if (life == nullptr || life->journalRead) return;
    life->journalRead = true;
    // Someone of the line got out: it lifts. Anyone else: it weighs.
    if (life->escaped) addDepression(-kReadEscapedRelief);
    else addDepression(kReadDepression);
    if (!mLegacyGraves.empty()) syncLegacyToSave();
    else mSaveManager.flushLegacy();
    std::cout << "[Journal] first reading of " << life->name << "'s journal (" << life->journal.size() << " entries)\n";
}

// ---------------------------------------------------------------------------
// The notebook's input

void Game::updateNotebookInput()
{
    if (!mShowNotebook)
    {
        if (mReader.open) closeReader();
        return;
    }
    const auto& history = mSaveManager.characterHistory();
    if (mReader.open)
    {
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN)) ++mReader.scroll;
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP)) mReader.scroll = std::max(0, mReader.scroll - 1);
        if (mInput.wasKeyPressed(SDL_SCANCODE_BACKSPACE))
        {
            const bool atGrave = mReader.atGrave;
            closeReader();
            if (atGrave) mShowNotebook = false;
            return;
        }
        const bool act = mInput.wasKeyPressed(SDL_SCANCODE_E) || mInput.wasKeyPressed(SDL_SCANCODE_RETURN);
        if (act && mReader.atGrave && mReader.grave >= 0 && mReader.grave < static_cast<int>(mLegacyGraves.size()))
        {
            const int grave = mReader.grave;
            if (graveWeathered(mLegacyGraves[static_cast<std::size_t>(grave)]))
            {
                closeReader();
                mShowNotebook = false;
                beginTend(grave);
            }
            else if (markerAvailable(grave))
            {
                takeMarker(grave);
            }
        }
        return;
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_RIGHT)) mNotebookPage = (mNotebookPage + 1) % static_cast<int>(kNotebookTabs.size());
    if (mInput.wasKeyPressed(SDL_SCANCODE_LEFT))
    {
        mNotebookPage = (mNotebookPage + static_cast<int>(kNotebookTabs.size()) - 1) % static_cast<int>(kNotebookTabs.size());
    }
    if (mNotebookPage != kTabBeforeYou || history.empty()) return;
    const int rows = static_cast<int>(history.size());
    if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN)) mNotebookCursor = std::min(rows - 1, mNotebookCursor + 1);
    if (mInput.wasKeyPressed(SDL_SCANCODE_UP)) mNotebookCursor = std::max(0, mNotebookCursor - 1);
    mNotebookCursor = std::clamp(mNotebookCursor, 0, rows - 1);
    if (!mInput.wasKeyPressed(SDL_SCANCODE_RETURN)) return;
    // Newest first: the cursor's row is the history from the end.
    const int index = rows - 1 - mNotebookCursor;
    const CharacterHistoryRecord& life = history[static_cast<std::size_t>(index)];
    // A journal lies with its grave until someone has read it there; the ones
    // who stayed or got out left theirs with the line.
    int grave = -1;
    for (std::size_t i = 0; i < mLegacyGraves.size(); ++i)
    {
        if (mLegacyGraves[i].runIndex == life.runIndex && mLegacyGraves[i].name == life.name) grave = static_cast<int>(i);
    }
    if (grave >= 0 && !life.journalRead && !mLegacyGraves[static_cast<std::size_t>(grave)].buried)
    {
        mInteractionMessage = life.name + "'s journal lies with them. Find the grave.";
        mInteractionMessageTimer = 3.0f;
        return;
    }
    openReader(grave, index, false);
}

// ---------------------------------------------------------------------------
// The notebook's pages

void Game::renderNotebookOverlay()
{
    if (mRunState != RunState::Playing || !mShowNotebook) return;
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const float panelW = dockedPanelWidth(true, mWindowWidth);
    const float panelX = w - panelW;
    constexpr float kPad = 24.0f;
    const float left = panelX + kPad;
    const float textW = panelW - kPad * 2.0f;

    mSpriteBatch.draw(glm::vec2(panelX, 0.0f), glm::vec2(panelW, h), uiUv, kPanelBack);
    const std::string title = mReader.open ? "JOURNAL" : "NOTEBOOK";
    mFont.drawText(mSpriteBatch, glm::vec2(left, 22.0f), title, kPanelInk, 3.0f);
    const std::string sub = mCurrentCharacterName + ", day " + std::to_string(mCurrentDay);
    mFont.drawText(mSpriteBatch, glm::vec2(left + textW, 30.0f), sub, kPanelDim, 1.0f, TextAlign::Right);
    mSpriteBatch.draw(glm::vec2(left, 64.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);

    const float footY = h - 30.0f;
    std::string hint;
    float top = 80.0f;
    if (mReader.open)
    {
        renderReaderPage(left, textW, top, footY - 12.0f);
        hint = "Up/Down: read on   Backspace: back   J: close";
    }
    else
    {
        renderNotebookTabs(left, textW, top);
        top += 44.0f;
        switch (mNotebookPage)
        {
        case kTabLeads: renderLeadsPage(left, textW, top, footY - 12.0f); break;
        case kTabWayOut: renderWayOutPage(left, textW, top, footY - 12.0f); break;
        case kTabPeople: renderPeoplePage(left, textW, top, footY - 12.0f); break;
        case kTabBeforeYou: renderBeforeYouPage(left, textW, top, footY - 12.0f); break;
        case kTabKnowHow: renderKnowHowPage(left, textW, top, footY - 12.0f); break;
        default: renderJournalPage(left, textW, top, footY - 12.0f); break;
        }
        hint = mNotebookPage == kTabBeforeYou ? "Left/Right: page   Up/Down, Enter: read   J: close" : "Left/Right: page   J: close";
    }
    mSpriteBatch.draw(glm::vec2(left, footY - 8.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);
    mFont.drawText(mSpriteBatch, glm::vec2(left, footY), hint, kPanelDim, 1.0f);
}

void Game::renderNotebookTabs(float left, float textW, float y)
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    constexpr float kScale = 2.0f;
    float total = 0.0f;
    for (const char* tab : kNotebookTabs) total += mFont.measureText(tab, kScale).x;
    const float gap = std::max(8.0f, (textW - total) / static_cast<float>(kNotebookTabs.size() - 1));
    // Too narrow for every name: the chosen tab by name, the rest by number.
    const bool fits = total + gap * static_cast<float>(kNotebookTabs.size() - 1) <= textW + 0.5f;
    float x = left;
    for (int i = 0; i < static_cast<int>(kNotebookTabs.size()); ++i)
    {
        const std::string label = fits || i == mNotebookPage ? kNotebookTabs[static_cast<std::size_t>(i)] : std::to_string(i + 1);
        const float lw = mFont.measureText(label, kScale).x;
        const bool chosen = i == mNotebookPage;
        if (chosen) mSpriteBatch.draw(glm::vec2(x - 5.0f, y - 5.0f), glm::vec2(lw + 10.0f, mFont.lineHeight(kScale) + 8.0f), uiUv, kPanelInk);
        mFont.drawText(mSpriteBatch, glm::vec2(x, y), label, chosen ? kPanelOnInk : kPanelDim, kScale);
        x += lw + (fits ? gap : 16.0f);
    }
    mSpriteBatch.draw(glm::vec2(left, y + mFont.lineHeight(kScale) + 12.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);
}

void Game::renderLeadsPage(float left, float textW, float top, float bottom)
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    constexpr float kBody = 2.0f;
    const float line = mFont.lineHeight(kBody) + 6.0f;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, (textW - 18.0f) / (6.0f * kBody)));
    const glm::vec2 here = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    float y = top;
    const auto leads = currentLegacyLeads();
    // The homes of the people met, after what earlier lives left.
    std::vector<std::pair<std::string, glm::vec4>> rows;
    for (const legacy::Lead& lead : leads)
    {
        rows.emplace_back(legacy::describeLead(lead, here, tileSize), legacyLeadColor(lead.kind));
    }
    for (const SurvivorRecord& r : mSurvivors)
    {
        if (!r.alive || !r.met || !r.homeKnown || r.retired || r.companion || r.homeX < 0) continue;
        const glm::vec2 home((static_cast<float>(r.homeX) + 0.5f) * tileSize, (static_cast<float>(r.homeY) + 0.5f) * tileSize);
        const int metres = legacy::metresBetween(here, home, tileSize);
        rows.emplace_back(r.name + "'s home - " + (metres < 2 ? std::string("right here")
                                                              : std::to_string(metres) + " m " + legacy::screenDirection(here, home)),
                          glm::vec4(0.62f, 0.86f, 0.64f, 1.0f));
    }
    if (rows.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "Nothing yet. No one came this way before you.", kPanelDim, kBody);
        return;
    }
    std::size_t shown = 0;
    for (const auto& [text, color] : rows)
    {
        if (y + line > bottom - 16.0f) break;
        mSpriteBatch.draw(glm::vec2(left, y + 5.0f), glm::vec2(8.0f, 8.0f), uiUv, color);
        mFont.drawText(mSpriteBatch, glm::vec2(left + 18.0f, y), truncateHudText(text, maxChars), kPanelInk, kBody);
        y += line;
        ++shown;
    }
    if (shown < rows.size())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left + 18.0f, y), "and " + std::to_string(rows.size() - shown) + " more on the map (M)",
                       kPanelDim, 1.0f);
    }
}

void Game::renderPeoplePage(float left, float textW, float top, float bottom)
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    constexpr float kBody = 2.0f;
    const float line = mFont.lineHeight(kBody) + 6.0f;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, (textW - 18.0f) / (6.0f * kBody)));
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const int run = mSaveManager.runNumber();
    std::vector<const SurvivorRecord*> people;
    for (const SurvivorRecord& r : mSurvivors)
    {
        if (r.met && !r.retired) people.push_back(&r);
    }
    std::stable_sort(people.begin(), people.end(), [&](const SurvivorRecord* a, const SurvivorRecord* b)
    {
        if (a->alive != b->alive) return a->alive;
        if (a->metRun != b->metRun) return a->metRun > b->metRun;
        return a->lastSeenDay > b->lastSeenDay;
    });
    float y = top;
    if (people.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "Nobody met yet.", kPanelDim, kBody);
        return;
    }
    const LayoutData& layout = mMapGenerator.layoutData();
    for (const SurvivorRecord* person : people)
    {
        if (y + line * 2.0f > bottom) break;
        const SurvivorRecord& r = *person;
        std::string where = "home not known";
        if (r.homeKnown && r.homeX >= 0)
        {
            const glm::vec2 home((static_cast<float>(r.homeX) + 0.5f) * tileSize, (static_cast<float>(r.homeY) + 0.5f) * tileSize);
            std::string kind = "a house";
            if (r.homeBuilding > 0 && r.homeBuilding <= static_cast<int>(layout.buildings.size()))
            {
                kind = "the " + lowered(buildingkinds::name(buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(r.homeBuilding - 1)])));
            }
            const int metres = legacy::metresBetween(here, home, tileSize);
            where = metres < 3 ? kind + ", right here" : kind + ", " + std::to_string(metres) + " m " + legacy::screenDirection(here, home);
        }
        const survivorrules::Personality p{r.aggression, r.cooperation, r.competence};
        const char* standing = survivorrules::standingWord(r.trust, survivorrules::attitude(p, 0.0f, r.trust, r.angryDay == mCurrentDay));
        std::string text;
        glm::vec4 color = kPanelInk;
        if (!r.alive)
        {
            text = r.name + " - dead.";
            color = kPanelDim;
        }
        else if (r.metRun != run)
        {
            text = "Someone wrote: " + r.name + ", " + where + ". " + standing;
            color = kPanelDim;
        }
        else
        {
            text = r.name + " - " + where + ". " + standing + " Seen day " + std::to_string(r.lastSeenDay) + ".";
        }
        mSpriteBatch.draw(glm::vec2(left, y + 5.0f), glm::vec2(8.0f, 8.0f), uiUv,
                          r.alive ? glm::vec4(0.62f, 0.86f, 0.64f, 1.0f) : glm::vec4(0.40f, 0.40f, 0.38f, 1.0f));
        for (const std::string& part : wrapText(text, maxChars))
        {
            mFont.drawText(mSpriteBatch, glm::vec2(left + 18.0f, y), part, color, kBody);
            y += line;
        }
        y += 4.0f;
    }
}

void Game::renderBeforeYouPage(float left, float textW, float top, float bottom)
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    constexpr float kBody = 2.0f;
    const float line = mFont.lineHeight(kBody) + 8.0f;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, textW / (6.0f * kBody)));
    const auto& history = mSaveManager.characterHistory();
    const auto& graves = mLegacyGraves.empty() ? mSaveManager.graves() : mLegacyGraves;
    const float statsY = bottom - 34.0f;
    float y = top;
    if (history.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "No one. You are the first.", kPanelDim, kBody);
    }
    // Newest first, the cursor kept in view.
    const int rows = static_cast<int>(history.size());
    const int visible = std::max(1, static_cast<int>((statsY - 12.0f - top) / line));
    const int first = std::clamp(mNotebookCursor - visible + 1, 0, std::max(0, rows - visible));
    for (int row = first; row < rows && row < first + visible; ++row)
    {
        const CharacterHistoryRecord& rec = history[static_cast<std::size_t>(rows - 1 - row)];
        std::string text = rec.name + ", run " + std::to_string(rec.runIndex) + " - ";
        glm::vec4 color = kPanelInk;
        if (rec.escaped) text += "got out on day " + std::to_string(rec.dayEnded);
        else if (rec.retired) text += (rec.chosen ? "stayed, day " : "retired on day ") + std::to_string(rec.dayEnded);
        else
        {
            text += "day " + std::to_string(rec.dayEnded) + ", " + rec.cause;
            for (const GraveRecord& grave : graves)
            {
                if (grave.runIndex != rec.runIndex || grave.name != rec.name) continue;
                if (grave.risen) { text += " (walks again)"; color = legacyLeadColor(legacy::LeadKind::Risen); }
                else if (grave.buried) text += " (in the cemetery)";
                else if (grave.laidToRest) text += " (at rest)";
                break;
            }
        }
        const std::string mark = rec.journalRead ? "  [read]" : "";
        const bool chosen = row == mNotebookCursor;
        if (chosen) mSpriteBatch.draw(glm::vec2(left - 6.0f, y - 5.0f), glm::vec2(textW + 12.0f, line), uiUv, kPanelInk);
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), truncateHudText(text + mark, maxChars), chosen ? kPanelOnInk : color, kBody);
        y += line;
    }

    // Totals across every life in this town.
    int daysTotal = 0;
    int longest = 0;
    std::string longestName;
    for (const CharacterHistoryRecord& rec : history)
    {
        const int days = std::max(1, rec.dayEnded - rec.dayStarted + 1);
        daysTotal += days;
        if (days > longest)
        {
            longest = days;
            longestName = rec.name;
        }
    }
    int laidToRest = 0;
    int buried = 0;
    for (const GraveRecord& grave : graves)
    {
        laidToRest += grave.laidToRest ? 1 : 0;
        buried += grave.buried ? 1 : 0;
    }
    std::string stats = "Lives " + std::to_string(history.size()) + "   Days lived " + std::to_string(daysTotal);
    if (longest > 0) stats += "   Longest " + std::to_string(longest) + (longest == 1 ? " day (" : " days (") + longestName + ")";
    std::string stats2;
    if (laidToRest > 0) stats2 += "Laid to rest " + std::to_string(laidToRest) + "   ";
    if (buried > 0) stats2 += "In the cemetery " + std::to_string(buried) + "   ";
    if (mTownContainers > 0) stats2 += "Cupboards unopened " + std::to_string(mTownUnopened) + " of " + std::to_string(mTownContainers);
    mSpriteBatch.draw(glm::vec2(left, statsY - 8.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);
    const std::size_t smallChars = static_cast<std::size_t>(std::max(8.0f, textW / 6.0f));
    mFont.drawText(mSpriteBatch, glm::vec2(left, statsY), truncateHudText(stats, smallChars), kPanelDim, 1.0f);
    if (!stats2.empty()) mFont.drawText(mSpriteBatch, glm::vec2(left, statsY + 16.0f), truncateHudText(stats2, smallChars), kPanelDim, 1.0f);
}

void Game::renderKnowHowPage(float left, float textW, float top, float bottom)
{
    constexpr float kBody = 2.0f;
    const float line = mFont.lineHeight(kBody) + 6.0f;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, textW / (6.0f * kBody)));
    float y = top;
    const auto heading = [&](const std::string& text)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), text, kPanelDim, 1.0f);
        y += 20.0f;
    };
    const auto para = [&](const std::string& text, const glm::vec4& color)
    {
        for (const std::string& part : wrapText(text, maxChars))
        {
            if (y + line > bottom) return;
            mFont.drawText(mSpriteBatch, glm::vec2(left, y), part, color, kBody);
            y += line;
        }
    };
    heading("RECIPES");
    std::string names;
    for (const auto& [a, b] : mSaveManager.discoveredRecipes())
    {
        const CraftingRecipe* recipe = mCraftingSystem.findRecipe(a, b);
        if (recipe != nullptr) names += (names.empty() ? "" : ", ") + recipe->label;
    }
    para(names.empty() ? "Nothing written down. Carry a notebook when you work out a recipe." : names, names.empty() ? kPanelDim : kPanelInk);
    y += 12.0f;
    // The Unknown's rules, once seen often enough to write down (phase 4).
    bool anyRule = false;
    for (const std::string& key : mLearnedRules)
    {
        for (int r = 0; r < zombiekinds::kUnknownRuleCount; ++r)
        {
            const auto rule = static_cast<zombiekinds::UnknownRule>(r);
            if (key != zombiekinds::unknownRuleKey(rule)) continue;
            if (!anyRule) heading("WHAT THEY DO");
            anyRule = true;
            para(zombiekinds::unknownRuleLine(rule), kPanelInk);
        }
    }
    if (anyRule) y += 12.0f;
    // The kinds of building someone has looked into (phase 6).
    heading("BUILDINGS KNOWN");
    std::vector<std::string> kinds;
    const LayoutData& layout = mMapGenerator.layoutData();
    for (const int building : mSaveManager.scavengeMemory().seenBuildings)
    {
        if (building <= 0 || building > static_cast<int>(layout.buildings.size())) continue;
        const std::string name = buildingkinds::name(buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(building - 1)]));
        if (std::find(kinds.begin(), kinds.end(), name) == kinds.end()) kinds.push_back(name);
    }
    std::string list;
    for (const std::string& k : kinds) list += (list.empty() ? "" : ", ") + k;
    para(list.empty() ? "None looked into yet." : list + ".", list.empty() ? kPanelDim : kPanelInk);
}

void Game::renderJournalPage(float left, float textW, float top, float bottom)
{
    constexpr float kBody = 2.0f;
    const float line = mFont.lineHeight(kBody) + 6.0f;
    const float dayW = mFont.measureText("Day 00", 1.0f).x + 14.0f;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, (textW - dayW) / (6.0f * kBody)));
    float y = top;
    if (!playerHasItem(mItems.notebook))
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "Nothing is being written. Carry a notebook.", kPanelDim, kBody);
        return;
    }
    if (mJournal.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "Nothing written yet.", kPanelDim, kBody);
        return;
    }
    // The newest at the foot: as many as fit, from the end.
    std::vector<std::pair<int, std::string>> lines;
    for (const journal::Entry& e : mJournal)
    {
        bool first = true;
        for (const std::string& part : wrapText(e.text, maxChars))
        {
            lines.emplace_back(first ? e.day : 0, part);
            first = false;
        }
    }
    const std::size_t fit = static_cast<std::size_t>(std::max(1.0f, (bottom - top) / line));
    const std::size_t start = lines.size() > fit ? lines.size() - fit : 0;
    for (std::size_t i = start; i < lines.size(); ++i)
    {
        if (lines[i].first > 0)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(left, y + 4.0f), "Day " + std::to_string(lines[i].first), kPanelDim, 1.0f);
        }
        mFont.drawText(mSpriteBatch, glm::vec2(left + dayW, y), lines[i].second, kPanelInk, kBody);
        y += line;
    }
}

void Game::renderReaderPage(float left, float textW, float top, float bottom)
{
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    constexpr float kBody = 2.0f;
    const float line = mFont.lineHeight(kBody) + 6.0f;
    const auto& history = mSaveManager.characterHistory();
    const CharacterHistoryRecord* life =
        mReader.history >= 0 && mReader.history < static_cast<int>(history.size()) ? &history[static_cast<std::size_t>(mReader.history)] : nullptr;
    const GraveRecord* grave =
        mReader.grave >= 0 && mReader.grave < static_cast<int>(mLegacyGraves.size()) ? &mLegacyGraves[static_cast<std::size_t>(mReader.grave)] : nullptr;
    const std::size_t wideChars = static_cast<std::size_t>(std::max(8.0f, textW / (6.0f * kBody)));
    float y = top;

    // Who, how long, how it ended (the epitaph, faded on a weathered grave).
    const std::string name = life != nullptr ? life->name : grave != nullptr ? grave->name : std::string("Someone");
    mFont.drawText(mSpriteBatch, glm::vec2(left, y), truncateHudText(name, wideChars), kPanelInk, kBody);
    y += line;
    const int dayEnded = life != nullptr ? life->dayEnded : grave != nullptr ? grave->day : 1;
    std::string days = dayEnded <= 1 ? "One day." : "Day 1 to day " + std::to_string(dayEnded) + ".";
    std::string epitaph;
    if (life != nullptr && life->escaped) epitaph = "Got out by the gate.";
    else if (life != nullptr && life->retired) epitaph = life->chosen ? "Stayed for good." : "Could not go on.";
    else if (grave != nullptr) epitaph = grave->cause + ".";
    else if (life != nullptr) epitaph = life->cause + ".";
    if (grave != nullptr && graveWeathered(*grave)) epitaph = "...";
    mFont.drawText(mSpriteBatch, glm::vec2(left, y), truncateHudText(days + "  " + epitaph, wideChars), kPanelDim, kBody);
    y += line + 6.0f;
    mSpriteBatch.draw(glm::vec2(left, y - 4.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);
    y += 8.0f;

    // What the grave offers, at the foot.
    std::string action;
    if (mReader.atGrave && grave != nullptr)
    {
        if (graveWeathered(*grave)) action = "E: Tend the grave (5 s)";
        else if (markerAvailable(mReader.grave)) action = "E: Take the marker";
        else if (grave->markerTaken) action = "The marker is gone from it.";
    }
    const float entriesBottom = action.empty() ? bottom : bottom - 30.0f;

    // The entries, day by day, scrolling.
    const float dayW = mFont.measureText("Day 00", 1.0f).x + 14.0f;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, (textW - dayW) / (6.0f * kBody)));
    std::vector<std::pair<int, std::string>> lines;
    if (life != nullptr)
    {
        for (const journal::Entry& e : life->journal)
        {
            bool first = true;
            for (const std::string& part : wrapText(e.text, maxChars))
            {
                lines.emplace_back(first ? e.day : 0, part);
                first = false;
            }
        }
    }
    if (lines.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "Nothing was written down.", kPanelDim, kBody);
    }
    const int fit = std::max(1, static_cast<int>((entriesBottom - y) / line));
    mReader.scroll = std::clamp(mReader.scroll, 0, std::max(0, static_cast<int>(lines.size()) - fit));
    for (int i = mReader.scroll; i < static_cast<int>(lines.size()) && i < mReader.scroll + fit; ++i)
    {
        const auto& [day, text] = lines[static_cast<std::size_t>(i)];
        if (day > 0) mFont.drawText(mSpriteBatch, glm::vec2(left, y + 4.0f), "Day " + std::to_string(day), kPanelDim, 1.0f);
        mFont.drawText(mSpriteBatch, glm::vec2(left + dayW, y), text, kPanelInk, kBody);
        y += line;
    }
    if (static_cast<int>(lines.size()) > mReader.scroll + fit)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left + textW, entriesBottom - 14.0f), "more below", kPanelDim, 1.0f, TextAlign::Right);
    }
    if (!action.empty())
    {
        mSpriteBatch.draw(glm::vec2(left, bottom - 34.0f), glm::vec2(textW, 1.0f), uiUv, kPanelRule);
        mFont.drawText(mSpriteBatch, glm::vec2(left, bottom - 24.0f), action, kPanelInk, kBody);
    }
}
