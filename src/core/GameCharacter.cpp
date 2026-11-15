// The character screen (phase 5): between "New Run" and the run, over the
// dimmed town. Name, two columns of traits, the points and what they make.
#include "core/GameShared.h"

using namespace gameshared;

namespace
{
constexpr int kNameColumn = 0;
constexpr int kGoodColumn = 1;
constexpr int kBadColumn = 2;
constexpr int kBeginColumn = 3;
constexpr std::size_t kMaxNameLength = 16;

const char* bloodlineLine(BloodlineTrait line)
{
    switch (line)
    {
    case BloodlineTrait::Hardened: return "Hardened. Their people did not break easily.";
    case BloodlineTrait::Scavenger: return "Scavenger. They always knew where to look.";
    case BloodlineTrait::Firewise: return "Firewise. Fire runs in the family.";
    case BloodlineTrait::Vigilant: return "Vigilant. Light sleepers, all of them.";
    case BloodlineTrait::Wayfinder: return "Wayfinder. One of theirs got out. They know every street.";
    case BloodlineTrait::Gravedigger: return "Gravedigger. They put their own to rest.";
    case BloodlineTrait::None: break;
    }
    return "";
}
} // namespace

void Game::beginCharacterCreation()
{
    mDraft = CharacterDraft{};
    // Start from nothing chosen; Space takes the seed's pick.
    mDraft.name = mCurrentCharacterName;
    mDraft.column = kGoodColumn;
    mRunState = RunState::CharacterCreation;
    mShowInventory = false;
    mShowCrafting = false;
}

void Game::updateCharacterCreation()
{
    mDraft.refusedTimer = std::max(0.0f, mDraft.refusedTimer - 1.0f / 60.0f);

    if (mInput.pressed(Action::UiBack))
    {
        returnToTitleScreen();
        return;
    }
    // Quick start: the seed's pick, as before, and straight in.
    if (mInput.pressed(Action::UiSpace))
    {
        mDraft.traits = traitrules::quickStart(mSaveManager.currentRunSeed());
        finishCharacterCreation();
        return;
    }

    // Typing replaces the name (letters; "-" for a space), Backspace takes one off.
    for (const char typed : mInput.lettersPressed())
    {
        if (!mDraft.typed)
        {
            mDraft.name.clear();
            mDraft.typed = true;
        }
        if (mDraft.name.size() >= kMaxNameLength) continue;
        const bool startOfWord = mDraft.name.empty() || mDraft.name.back() == ' ';
        const char letter = static_cast<char>((startOfWord ? 'A' : 'a') + (typed - 'A'));
        mDraft.name.push_back(letter);
    }
    if (mInput.pressed(Action::UiMinus) && mDraft.typed && !mDraft.name.empty() &&
        mDraft.name.back() != ' ' && mDraft.name.size() < kMaxNameLength)
    {
        mDraft.name.push_back(' ');
    }
    if (mInput.pressed(Action::UiErase) && !mDraft.name.empty())
    {
        if (!mDraft.typed)
        {
            mDraft.name.clear();
            mDraft.typed = true;
        }
        else
        {
            mDraft.name.pop_back();
        }
    }

    // Left/Right: name, good at, bad at, Begin. Up/Down along a column.
    if (mInput.pressed(Action::UiLeft)) mDraft.column = std::max(kNameColumn, mDraft.column - 1);
    if (mInput.pressed(Action::UiRight)) mDraft.column = std::min(kBeginColumn, mDraft.column + 1);
    const int rows = static_cast<int>(traitrules::allPositive().size());
    if (mDraft.column == kGoodColumn || mDraft.column == kBadColumn)
    {
        if (mInput.pressed(Action::UiUp)) mDraft.row = (mDraft.row + rows - 1) % rows;
        if (mInput.pressed(Action::UiDown)) mDraft.row = (mDraft.row + 1) % rows;
    }

    if (!mInput.pressed(Action::UiConfirm)) return;
    switch (mDraft.column)
    {
    case kNameColumn:
        // Another name from the same town.
        ++mDraft.rerolls;
        mDraft.name = generateCharacterName(mSaveManager.currentRunSeed() ^ (static_cast<std::uint32_t>(mDraft.rerolls) * 0x9E3779B9u));
        mDraft.typed = false;
        break;
    case kGoodColumn:
    case kBadColumn:
    {
        const bool done = mDraft.column == kGoodColumn
                              ? traitrules::toggle(mDraft.traits, traitrules::allPositive()[static_cast<std::size_t>(mDraft.row)])
                              : traitrules::toggle(mDraft.traits, traitrules::allNegative()[static_cast<std::size_t>(mDraft.row)]);
        if (!done)
        {
            mDraft.refusedTimer = 2.0f;
            mDraft.refusedWhy = "Three of each, at most.";
        }
        else
        {
            mDraft.refusedTimer = 0.0f; // a change answers the refusal
        }
        break;
    }
    case kBeginColumn:
        if (traitrules::canBegin(mDraft.traits))
        {
            finishCharacterCreation();
        }
        else
        {
            mDraft.refusedTimer = 2.0f;
            mDraft.refusedWhy = "Too much. Give something up.";
        }
        break;
    default:
        break;
    }
}

void Game::finishCharacterCreation()
{
    applyCharacter(mDraft.traits, true);
    std::string name = mDraft.name;
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (!name.empty()) mCurrentCharacterName = name;
    mRunState = RunState::Playing;
    mInteractionMessage = "You are " + mCurrentCharacterName + ".";
    mInteractionMessageTimer = 3.5f;
    // The checkpoint knows who this is from the first minute.
    if (mPlayerEntity != kInvalidEntity) mSaveManager.saveCheckpoint(buildRunCheckpoint());
}

void Game::renderCharacterCreation()
{
    if (mRunState != RunState::CharacterCreation) return;
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    mSpriteBatch.draw(glm::vec2(0.0f), glm::vec2(w, h), white, glm::vec4(0.02f, 0.025f, 0.03f, 0.86f));

    const auto rect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        mSpriteBatch.draw(pos, size, white, color);
    };
    const auto outline = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        rect(pos, glm::vec2(size.x, 2.0f), color);
        rect(pos + glm::vec2(0.0f, size.y - 2.0f), glm::vec2(size.x, 2.0f), color);
        rect(pos, glm::vec2(2.0f, size.y), color);
        rect(pos + glm::vec2(size.x - 2.0f, 0.0f), glm::vec2(2.0f, size.y), color);
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

    const float margin = std::max(24.0f, std::floor(w * 0.03f));
    const float top = std::max(28.0f, std::floor(h * 0.07f));
    const float gap = 24.0f;
    const float nameW = std::floor(std::clamp(w * 0.20f, 200.0f, 280.0f));
    const float rightW = std::floor(std::clamp(w * 0.19f, 200.0f, 260.0f));
    const float colW = std::floor((w - margin * 2.0f - nameW - rightW - gap * 3.0f) * 0.5f);
    const float nameX = margin;
    const float goodX = nameX + nameW + gap;
    const float badX = goodX + colW + gap;
    const float rightX = badX + colW + gap;
    const float titleScale = w >= 1100.0f ? 4.0f : 3.0f;

    text(glm::vec2(nameX, top), "WHO WAKES UP", kPanelInk, titleScale);
    const float bodyTop = top + mFont.lineHeight(titleScale) + 22.0f;
    rect(glm::vec2(nameX, bodyTop - 10.0f), glm::vec2(w - margin * 2.0f, 1.0f), kPanelRule);

    // Left: the name.
    {
        float y = bodyTop + 6.0f;
        text(glm::vec2(nameX, y), "NAME", kPanelDim, 1.0f);
        y += 16.0f;
        const bool focus = mDraft.column == kNameColumn;
        if (focus) rect(glm::vec2(nameX - 6.0f, y - 6.0f), glm::vec2(nameW + 6.0f, 30.0f), kPanelInk);
        const std::string shown = mDraft.name.empty() ? "_" : mDraft.name + (mDraft.typed ? "_" : "");
        text(glm::vec2(nameX, y), fit(shown, nameW, 2.0f), focus ? kPanelOnInk : kPanelInk, 2.0f);
        y += 34.0f;
        text(glm::vec2(nameX, y), fit("Enter: another name", nameW, 1.0f), kPanelDim, 1.0f);
        y += 14.0f;
        text(glm::vec2(nameX, y), fit("Type to replace it", nameW, 1.0f), kPanelDim, 1.0f);
        y += 30.0f;
        // Wrapped to the column.
        const auto wrapped = [&](const std::string& line)
        {
            const std::size_t perLine = static_cast<std::size_t>(std::max(8.0f, nameW / 6.0f));
            std::size_t start = 0;
            while (start < line.size())
            {
                std::size_t end = std::min(line.size(), start + perLine);
                if (end < line.size())
                {
                    const std::size_t space = line.rfind(' ', end);
                    if (space != std::string::npos && space > start) end = space;
                }
                text(glm::vec2(nameX, y), line.substr(start, end - start), kPanelInk, 1.0f);
                y += 14.0f;
                start = end + (end < line.size() && line[end] == ' ' ? 1 : 0);
            }
        };
        const Traits* own = playerTraits();
        if (own != nullptr && !own->bloodlines.empty())
        {
            text(glm::vec2(nameX, y), "BLOODLINE", kPanelDim, 1.0f);
            y += 16.0f;
            for (BloodlineTrait line : own->bloodlines)
            {
                wrapped(bloodlineLine(line));
                y += 4.0f;
            }
            y += 10.0f;
        }
        // Phase 10: what the dead of the line knew.
        bool echoes = false;
        for (std::size_t i = 0; i < kSkillCount; ++i)
        {
            if (mSkillEchoes[i] <= 0) continue;
            if (!echoes)
            {
                text(glm::vec2(nameX, y), "IN THE FAMILY", kPanelDim, 1.0f);
                y += 16.0f;
            }
            echoes = true;
            wrapped(legacy::echoLine(skillrules::name(static_cast<Skill>(i)), mSkillEchoes[i]));
            y += 4.0f;
        }
    }

    // Middle: good at, bad at.
    const float rowH = std::clamp((h - bodyTop - 120.0f) / 10.0f, 34.0f, 44.0f);
    const auto column = [&](float x, const char* heading, int columnIndex, auto traitsList, auto chosenOf, auto costOf,
                            auto nameOf, auto effectOf)
    {
        float y = bodyTop + 6.0f;
        text(glm::vec2(x, y), heading, kPanelDim, 1.0f);
        y += 18.0f;
        for (int i = 0; i < static_cast<int>(traitsList.size()); ++i)
        {
            const auto trait = traitsList[static_cast<std::size_t>(i)];
            const bool chosen = chosenOf(trait);
            const bool focus = mDraft.column == columnIndex && mDraft.row == i;
            if (chosen) rect(glm::vec2(x - 6.0f, y - 5.0f), glm::vec2(colW + 6.0f, rowH - 4.0f), kPanelInk);
            // The cursor: a ring; around a chosen (filled) row it stands off by a gap.
            if (focus && !chosen) outline(glm::vec2(x - 8.0f, y - 7.0f), glm::vec2(colW + 10.0f, rowH), kPanelInk);
            if (focus && chosen) outline(glm::vec2(x - 10.0f, y - 9.0f), glm::vec2(colW + 14.0f, rowH + 4.0f), kPanelInk);
            const glm::vec4 ink = chosen ? kPanelOnInk : kPanelInk;
            text(glm::vec2(x, y), fit(nameOf(trait), colW - 40.0f, 2.0f), ink, 2.0f);
            text(glm::vec2(x + colW - 6.0f, y), costOf(trait), ink, 2.0f, TextAlign::Right);
            text(glm::vec2(x, y + 19.0f), fit(effectOf(trait), colW - 6.0f, 1.0f), chosen ? kPanelOnInk : kPanelDim, 1.0f);
            y += rowH;
        }
    };
    column(goodX, "GOOD AT", kGoodColumn, traitrules::allPositive(),
           [&](PositiveTrait t) { return mDraft.traits.has(t); },
           [](PositiveTrait t) { return "-" + std::to_string(traitrules::cost(t)); },
           [](PositiveTrait t) { return std::string(traitName(t)); },
           [](PositiveTrait t) { return std::string(traitrules::effect(t)); });
    column(badX, "BAD AT", kBadColumn, traitrules::allNegative(),
           [&](NegativeTrait t) { return mDraft.traits.has(t); },
           [](NegativeTrait t) { return "+" + std::to_string(traitrules::gain(t)); },
           [](NegativeTrait t) { return std::string(traitName(t)); },
           [](NegativeTrait t) { return std::string(traitrules::effect(t)); });

    // Right: points, what it makes, Begin.
    {
        float y = bodyTop + 6.0f;
        const int points = traitrules::points(mDraft.traits);
        text(glm::vec2(rightX, y), "Points: " + std::to_string(points), points < 0 ? kPanelAmber : kPanelInk, 3.0f);
        y += 40.0f;
        text(glm::vec2(rightX, y), "STARTS WITH", kPanelDim, 1.0f);
        y += 18.0f;
        const auto line = [&](const std::string& label, const std::string& value)
        {
            text(glm::vec2(rightX, y), label, kPanelDim, 2.0f);
            text(glm::vec2(rightX + rightW, y), value, kPanelInk, 2.0f, TextAlign::Right);
            y += 24.0f;
        };
        line("Health", std::to_string(static_cast<int>(std::round(traitrules::maxHealth(mDraft.traits)))));
        const auto levels = traitrules::startingLevels(mDraft.traits);
        {
            std::ostringstream carry;
            carry << std::fixed << std::setprecision(0)
                  << traitrules::carryLimitKg(mDraft.traits, levels[static_cast<std::size_t>(Skill::Fitness)]) << " kg";
            line("Carry", carry.str());
        }
        for (std::size_t i = 0; i < kSkillCount; ++i)
        {
            if (levels[i] > 0) line(fit(skillrules::name(static_cast<Skill>(i)), rightW - 30.0f, 2.0f), std::to_string(levels[i]));
        }

        y += 16.0f;
        const bool canBegin = traitrules::canBegin(mDraft.traits);
        const bool focus = mDraft.column == kBeginColumn;
        const glm::vec2 buttonPos(rightX - 6.0f, y - 6.0f);
        const glm::vec2 buttonSize(rightW + 6.0f, 34.0f);
        if (focus && canBegin) rect(buttonPos, buttonSize, kPanelInk);
        else outline(buttonPos, buttonSize, canBegin ? kPanelInk : kPanelTrack);
        text(glm::vec2(rightX, y), "Begin", focus && canBegin ? kPanelOnInk : (canBegin ? kPanelInk : kPanelDim), 3.0f);
        y += 44.0f;
        if (mDraft.refusedTimer > 0.0f)
        {
            const std::string& why = mDraft.refusedWhy;
            text(glm::vec2(rightX, y), fit(why, rightW, 1.0f), kPanelBad, 1.0f);
            y += 18.0f;
        }
        text(glm::vec2(rightX, y), fit("Space: quick start", rightW, 1.0f), kPanelDim, 1.0f);
        y += 14.0f;
        text(glm::vec2(rightX, y), fit("Esc: back to the title", rightW, 1.0f), kPanelDim, 1.0f);
    }

    text(glm::vec2(margin, h - 28.0f),
         fit("Left/Right column   Up/Down move   Enter choose   Type a name (- for a space)", w - margin * 2.0f, 2.0f),
         kPanelDim, 2.0f);
}
