// The way out's clues (overhaul phase 9): what a character knows of the
// gate, where it learns it (notes, a board, a book, the radio, the box,
// the living, an earlier life's notebook) and the notebook's page that
// keeps it. The rules are world/Clues'; the leads are this life's (the
// checkpoint), the notebook carries them across a death (the grave).
#include "core/GameShared.h"

#include <iostream>
#include <sstream>

#include "components/LegacyLink.h"

using namespace gameshared;

namespace
{
glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

constexpr float kReachTiles = 1.5f; // a corner counts: furniture is read from beside it
constexpr float kBroadcastSeconds = 20.0f;

std::vector<std::string> wrapWords(const std::string& text, std::size_t maxChars)
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
} // namespace

void Game::loadClueContent(const std::filesystem::path& assetsRoot)
{
    std::string error;
    const std::string path = (assetsRoot / "data" / "clues.json").string();
    if (!clues::loadContent(path, mClueContent, &error))
    {
        mClueContent = clues::defaults();
        std::cerr << "[Clues] " << path << ": " << error << " (using the built-in words)\n";
    }
}

void Game::learnLeads(clues::Mask leads, const char* how)
{
    const auto fresh = static_cast<clues::Mask>(leads & static_cast<clues::Mask>(~mLeads));
    if (fresh == 0) return;
    mLeads = static_cast<clues::Mask>(mLeads | fresh);
    noteJournal(journal::Kind::ClueLearned);
    for (int lead = 1; lead <= clues::kLeadCount; ++lead)
    {
        if ((fresh & clues::bit(lead)) != 0) std::cout << "[Clues] lead " << lead << " learned (" << how << ")\n";
    }
}

void Game::noteGateSeen()
{
    if (!gatePlaced()) return;
    mGateSeen = clues::seenFrom(mGate, mSaveManager.runNumber(), mCurrentDay);
}

bool Game::handleReadable(const ProximityPrompt&)
{
    if (mReadables.empty() || mPlayerEntity == kInvalidEntity) return false;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    int best = -1;
    float bestDistance = kReachTiles * tileSize;
    for (std::size_t i = 0; i < mReadables.size(); ++i)
    {
        const clues::Readable& r = mReadables[i];
        // Read once a life; after that the fridge, the desk, is itself again.
        if (std::find(mReadablesRead.begin(), mReadablesRead.end(), i) != mReadablesRead.end()) continue;
        const float distance = glm::length(tileCentre(mTileMap, r.tile) - here);
        if (distance <= bestDistance)
        {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    if (best < 0) return false;
    const clues::Readable& r = mReadables[static_cast<std::size_t>(best)];
    mProximityPrompt = std::string(clues::readableName(r.source)) + "   E: Read";
    if (!mInput.pressed(Action::Use)) return true;
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::ReadClue;
    mBaseAction.tile = r.tile;
    mBaseAction.slot = best;
    mBaseAction.total = gate::kReadSeconds;
    mBaseAction.label = "Reading";
    mBaseAction.startPos = here;
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    return true;
}

void Game::finishReadClue()
{
    const int index = mBaseAction.slot;
    mBaseAction = BaseAction{};
    if (index < 0 || index >= static_cast<int>(mReadables.size())) return;
    const clues::Readable r = mReadables[static_cast<std::size_t>(index)];
    mReadablesRead.push_back(static_cast<std::size_t>(index));
    const std::string text = clues::fill(clues::readableText(mClueContent, r.source), mClueNames);
    const bool knewHow = clues::knows(mLeads, clues::Fixable);
    learnLeads(clues::teaches(r.source), clues::sourceKey(r.source));
    // The manual teaches the hands as well as the head.
    if (r.source == clues::Source::Manual && !knewHow) awardXp(Skill::Mechanics, 100.0f);
    mInteractionMessage = text;
    mInteractionMessageTimer = 7.0f;
    std::cout << "[Clues] read " << clues::sourceKey(r.source) << " at " << r.tile.x << "," << r.tile.y << ": " << text << '\n';
}

void Game::updateBroadcast(float dtSeconds)
{
    if (mBroadcastHeard || !gatePlaced() || mClueContent.broadcast.empty()) return;
    mBroadcastTimer -= dtSeconds;
    if (mBroadcastTimer > 0.0f) return;
    mBroadcastTimer = kBroadcastSeconds;
    const std::size_t line = static_cast<std::size_t>(mBroadcastLine) % mClueContent.broadcast.size();
    const std::string text = clues::fill(mClueContent.broadcast[line], mClueNames);
    mInteractionMessage = "Radio: " + text;
    mInteractionMessageTimer = 6.0f;
    std::cout << "[Clues] radio: " << text << '\n';
    ++mBroadcastLine;
    if (mBroadcastLine >= static_cast<int>(mClueContent.broadcast.size()))
    {
        mBroadcastHeard = true;
        learnLeads(clues::teaches(clues::Source::Broadcast), "the radio");
    }
}

std::string Game::gateObjective() const
{
    if (mTitleGameModeIndex != 0 || !gatePlaced()) return "";
    if (mGate.open) return "The gate is open. Walk out.";
    if (mStand.running)
    {
        const int left = static_cast<int>(std::ceil(std::max(0.0f, gate::kOpenSeconds - mStand.opened) + mStand.grind));
        return "Hold the gate. " + std::to_string(left) + " s.";
    }
    clues::Facts facts;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        facts.hasFuse = InventorySystem::countItem(inv, mItems.gateFuse) > 0;
        facts.hasFuel = InventorySystem::countItem(inv, mItems.fuelCan) > 0;
        facts.keyInHand = InventorySystem::countItem(inv, mItems.gateKey) > 0;
        facts.hasToolbox = InventorySystem::countItem(inv, mItems.toolbox) > 0;
    }
    facts.canFree = clues::knows(mLeads, clues::Fixable) || skillLevel(Skill::Mechanics) >= gate::kMechanicsToFree;
    return clues::objective(mLeads, mGateSeen, mClueNames, facts);
}

bool Game::wayOutPageShown() const
{
    return gatePlaced() && (mLeads != 0 || mGateSeen.valid);
}

void Game::renderWayOutPage(float left, float textW, float top, float bottom)
{
    // A tab of the notebook (phase 10): what is known of the gate.
    constexpr float kBody = 2.0f;
    const float bodyLine = mFont.lineHeight(kBody) + 5.0f;
    const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, textW / (6.0f * kBody)));
    float y = top;
    if (!wayOutPageShown())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "Nothing yet. People talk of a way out of town.", kPanelDim, kBody);
        return;
    }
    const std::vector<std::string> page = clues::pageLines(mLeads, mGateSeen, mClueContent, mClueNames,
                                                           mSaveManager.runNumber(), mCurrentDay);
    const int known = clues::count(mLeads);
    mFont.drawText(mSpriteBatch, glm::vec2(left, y),
                   std::to_string(known) + (known == 1 ? " thing" : " things") + " known of " + std::to_string(clues::kLeadCount) + ".",
                   kPanelDim, 1.0f);
    y += 26.0f;
    for (const std::string& entry : page)
    {
        for (const std::string& text : wrapWords(entry, maxChars))
        {
            if (y + bodyLine > bottom) return;
            mFont.drawText(mSpriteBatch, glm::vec2(left, y), text, kPanelInk, kBody);
            y += bodyLine;
        }
    }
}

std::string Game::survivorGateLead(SurvivorRecord& record, float trust, float competence)
{
    if (!gatePlaced()) return "";
    clues::Mask told = 0;
    std::string line;
    if (record.retired)
    {
        // One of the bloodline tells what they knew.
        told = static_cast<clues::Mask>(static_cast<clues::Mask>(record.gateLeads) & static_cast<clues::Mask>(~mLeads));
        if (told == 0) return "";
        line = "I was writing it down. ";
    }
    else
    {
        told = static_cast<clues::Mask>(clues::fromSurvivor(mLeads, trust, competence) & static_cast<clues::Mask>(~mLeads));
        if (told == 0) return "";
        line = "Heard it on the radio, before. ";
    }
    for (int lead = 1; lead <= clues::kLeadCount; ++lead)
    {
        if ((told & clues::bit(lead)) == 0) continue;
        line += clues::fill(mClueContent.leads[static_cast<std::size_t>(lead - 1)], mClueNames);
        line += ' ';
        if (line.size() > 160) break; // the rest another day
    }
    while (!line.empty() && line.back() == ' ') line.pop_back();
    learnLeads(told, record.retired ? "the bloodline" : "a survivor");
    return line;
}

void Game::notebookTaken(Entity container)
{
    if (container == kInvalidEntity || !mWorld.hasComponent<LegacyLink>(container)) return;
    const LegacyLink& link = mWorld.getComponent<LegacyLink>(container);
    if (link.kind != LegacyLinkKind::Pack || link.graveIndex < 0 || link.graveIndex >= static_cast<int>(mLegacyGraves.size())) return;
    const GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(link.graveIndex)];
    if (!grave.notebook) return;
    const std::string name = grave.name;
    mInteractionMessage = learnFromGraveNotebook(link.graveIndex)
                              ? name + "'s notebook. Pages about a gate. J to read."
                              : name + "'s notebook. Nothing in it you did not know.";
    mInteractionMessageTimer = 4.0f;
}

std::vector<Game::WayOutMark> Game::wayOutMarks() const
{
    std::vector<WayOutMark> marks;
    if (!gatePlaced()) return marks;
    if (clues::knows(mLeads, clues::Where) || mGateSeen.valid) marks.push_back(WayOutMark{gateBoxWorld(), "The gate"});
    if (clues::knows(mLeads, clues::Fuse) && mFuseCabinet.x >= 0 && !mGate.fuseFitted)
    {
        marks.push_back(WayOutMark{tileCentre(mTileMap, mFuseCabinet), "Fuse"});
    }
    if (clues::knows(mLeads, clues::Sergeant) && mGate.key == gate::KeyPlace::Sergeant && !mSergeantLoop.empty())
    {
        glm::vec2 sum(0.0f);
        for (const glm::ivec2& t : mSergeantLoop) sum += tileCentre(mTileMap, t);
        marks.push_back(WayOutMark{sum / static_cast<float>(mSergeantLoop.size()), "Sgt. " + mClueNames.sergeant});
    }
    return marks;
}
