#include "ui/Hud.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include <glm/common.hpp>

#include "rendering/BitmapFont.h"
#include "rendering/SpriteBatch.h"
#include "rendering/SpriteSheet.h"
#include "systems/NeedsSystem.h"

namespace
{
// Integer font scales keep the 5x7 pixel font crisp.
constexpr float kLabelScale = 2.0f;
constexpr float kSmallScale = 1.0f;

// Off-white on the world, amber for what needs attention, nothing boxed in.
const glm::vec4 kTextColor(0.96f, 0.95f, 0.93f, 1.0f);
const glm::vec4 kDimTextColor(0.72f, 0.71f, 0.68f, 1.0f);
const glm::vec4 kAmber(0.94f, 0.71f, 0.30f, 1.0f);
const glm::vec4 kTrackColor(0.04f, 0.05f, 0.06f, 0.62f);
const glm::vec4 kStaminaColor = kAmber;
const glm::vec4 kStaminaLostColor(0.46f, 0.16f, 0.14f, 0.95f);

// Conditions thresholds that live only in the HUD (early warnings shown before
// the needs model starts to bite).
constexpr float kPeckishBelow = 50.0f;
constexpr float kDrowsyAbove = 50.0f;
constexpr float kExhaustedAbove = 90.0f;
constexpr float kWindedAbove = 60.0f;
constexpr float kQueasyAbove = 20.0f;
constexpr float kSickAbove = 40.0f;
constexpr float kGravelyIllAbove = 80.0f;
constexpr float kAnxiousAt = 25.0f;
constexpr float kScaredAt = 50.0f;
constexpr float kPanickingAt = 75.0f;
constexpr float kLowSpiritsAt = 40.0f;
constexpr float kDepressedAt = 60.0f;
constexpr float kDespairAt = 85.0f;
constexpr float kNumbAt = 50.0f;

float ratio(float value, float maximum)
{
    return maximum > 0.0f ? std::clamp(value / maximum, 0.0f, 1.0f) : 0.0f;
}

glm::vec4 withAlpha(glm::vec4 color, float alpha)
{
    color.a *= alpha;
    return color;
}

std::string truncateText(const std::string& text, std::size_t maxChars)
{
    if (text.size() <= maxChars || maxChars < 3)
    {
        return text;
    }
    return text.substr(0, maxChars - 2) + "..";
}

glm::vec4 healthColor(float healthRatio)
{
    if (healthRatio > 0.6f)
    {
        return kTextColor;
    }
    if (healthRatio > 0.3f)
    {
        return {0.92f, 0.74f, 0.28f, 1.0f};
    }
    return {0.92f, 0.28f, 0.24f, 1.0f};
}

// Night is bluish, day is warm white.
glm::vec4 clockColor(double hours)
{
    const double h = std::fmod(std::max(0.0, hours), 24.0);
    if (h >= 21.0 || h < 5.0)
    {
        return {0.62f, 0.72f, 0.98f, 1.0f};
    }
    if (h >= 18.0 || h < 7.0)
    {
        return {0.96f, 0.70f, 0.46f, 1.0f};
    }
    return {0.96f, 0.94f, 0.82f, 1.0f};
}

bool hasSprite(const glm::vec4& uv)
{
    return uv.z > 0.0f && uv.w > 0.0f;
}

void drawFrame(SpriteBatch& batch, const glm::vec4& uv, const glm::vec2& pos, const glm::vec2& size,
               float thickness, const glm::vec4& color)
{
    batch.draw(pos, glm::vec2(size.x, thickness), uv, color);
    batch.draw(glm::vec2(pos.x, pos.y + size.y - thickness), glm::vec2(size.x, thickness), uv, color);
    batch.draw(glm::vec2(pos.x, pos.y + thickness), glm::vec2(thickness, size.y - thickness * 2.0f), uv, color);
    batch.draw(glm::vec2(pos.x + size.x - thickness, pos.y + thickness),
               glm::vec2(thickness, size.y - thickness * 2.0f), uv, color);
}

std::string clockText(double hours)
{
    const double h = std::fmod(std::max(0.0, hours), 24.0);
    const int hour = static_cast<int>(h);
    const int minute = std::clamp(static_cast<int>((h - static_cast<double>(hour)) * 60.0), 0, 59);
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    return buffer;
}

std::string weatherText(const HudState& state)
{
    const int temperature = static_cast<int>(std::lround(state.outdoorTemperature));
    std::string text = state.weather.empty() ? std::string("Clear") : state.weather;
    text += "  " + std::to_string(temperature) + "C";
    return text;
}

// Shared geometry so the shape pass and the text pass agree.
struct Layout
{
    glm::vec2 viewport{0.0f};
    bool docked = false;  // a panel is open down the right edge
    float centreX = 0.0f; // middle of the part of the screen still showing the world

    float barsX = 0.0f;
    float healthY = 0.0f;
    float staminaY = 0.0f;
    std::size_t conditionRows = 0;

    float rightX = 0.0f;
    float clockY = 0.0f;
    float dayY = 0.0f;
    float objectiveY = 0.0f;

    std::array<float, 8> slotX{};
    std::array<float, 8> slotW{};
    float iconBottom = 0.0f;
    float itemNameY = 0.0f;
    float messageY = 0.0f;

    float toastY = 0.0f;

    // Conditions stack upward from the bars, the worst at the top.
    [[nodiscard]] float conditionY(std::size_t row) const
    {
        return healthY - 12.0f - static_cast<float>(conditionRows - row) * Hud::kConditionRowHeight;
    }
};

Layout computeLayout(const HudState& state, const glm::vec2& viewport)
{
    Layout layout;
    layout.viewport = viewport;
    layout.docked = state.dockedWidth > 0.0f;
    layout.centreX = std::round((viewport.x - state.dockedWidth) * 0.5f);

    layout.barsX = Hud::kEdge;
    layout.staminaY = viewport.y - Hud::kEdge - Hud::kStaminaHeight;
    layout.healthY = layout.staminaY - 6.0f - Hud::kHealthHeight;
    layout.conditionRows = std::min(state.conditions.size(), Hud::kMaxConditionRows);

    layout.rightX = viewport.x - Hud::kEdge;
    layout.clockY = Hud::kEdge - 4.0f;
    layout.dayY = layout.clockY + 36.0f;
    layout.objectiveY = layout.dayY + (state.districtLine.empty() ? 22.0f : 44.0f) + 8.0f;

    const int active = std::clamp(state.activeSlot, 0, 7);
    const float total = 7.0f * Hud::kSlotSize + Hud::kActiveIcon + 7.0f * Hud::kSlotGap;
    float x = std::round(layout.centreX - total * 0.5f);
    for (int i = 0; i < 8; ++i)
    {
        const float width = i == active ? Hud::kActiveIcon : Hud::kSlotSize;
        layout.slotX[static_cast<std::size_t>(i)] = x;
        layout.slotW[static_cast<std::size_t>(i)] = width;
        x += width + Hud::kSlotGap;
    }
    layout.itemNameY = viewport.y - Hud::kEdge - 8.0f;
    layout.iconBottom = layout.itemNameY - 20.0f;
    layout.messageY = layout.docked ? viewport.y - Hud::kEdge - 40.0f : layout.iconBottom - Hud::kActiveIcon - 30.0f;

    layout.toastY = Hud::kEdge;
    return layout;
}

// The objective: title, detail line, then the plan's steps.
constexpr float kStepRow = 20.0f;
constexpr float kStepBox = 10.0f;
constexpr float kStepGap = 8.0f; // between a box and its text
constexpr std::size_t kStepChars = 30;

float objectiveStepsTop(const HudState& state)
{
    return state.objectiveDetail.empty() ? 26.0f : 40.0f;
}

constexpr float kDayCardTitleScale = 4.0f;
constexpr float kClockScale = 4.0f;

} // namespace

std::string Hud::slotLabel(const HudSlot& slot)
{
    std::string label = slot.name;
    if (slot.wear >= 0.0f)
    {
        if (slot.wear <= 0.0f) label += " - broken";
        else if (slot.wear <= 0.33f) label += " - damaged";
        else if (slot.wear <= 0.66f) label += " - worn";
    }
    if (slot.equipped)
    {
        label += " - in hand";
    }
    return label;
}

glm::vec4 Hud::severityColor(HudSeverity severity)
{
    switch (severity)
    {
    case HudSeverity::Info:
        return {0.68f, 0.76f, 0.86f, 1.0f};
    case HudSeverity::Mild:
        return {0.95f, 0.84f, 0.42f, 1.0f};
    case HudSeverity::Serious:
        return {1.0f, 0.56f, 0.26f, 1.0f};
    case HudSeverity::Critical:
        return {1.0f, 0.27f, 0.23f, 1.0f};
    }
    return {1.0f, 1.0f, 1.0f, 1.0f};
}

std::vector<HudStatus> Hud::buildConditions(const HudConditionInputs& in)
{
    std::vector<HudStatus> out;
    auto add = [&](const char* label, HudSeverity severity, const char* icon)
    {
        out.push_back(HudStatus{label, severity, icon});
    };

    // Wounds first: they are the most urgent things to fix.
    if (in.bleeding)
    {
        add("Bleeding", HudSeverity::Critical, "fx_blood");
    }
    if (in.infected)
    {
        add("Infected", HudSeverity::Critical, "ui_skull");
    }
    else if (in.bites > 0)
    {
        add(in.bites > 1 ? "Bitten x2+" : "Bitten", HudSeverity::Serious, "ui_skull");
    }
    if (in.cuts > 0 && !in.bleeding)
    {
        add("Cut", HudSeverity::Mild, "fx_blood");
    }

    if (in.thirst < NeedsSystem::kParchedBelow)
    {
        add("Parched", HudSeverity::Critical, "ui_thirst");
    }
    else if (in.thirst < NeedsSystem::kThirstyBelow)
    {
        add("Very thirsty", HudSeverity::Serious, "ui_thirst");
    }
    else if (in.thirst < kPeckishBelow)
    {
        add("Thirsty", HudSeverity::Mild, "ui_thirst");
    }

    if (in.hunger < NeedsSystem::kStarvingBelow)
    {
        add("Starving", HudSeverity::Critical, "ui_hunger");
    }
    else if (in.hunger < NeedsSystem::kHungryBelow)
    {
        add("Hungry", HudSeverity::Serious, "ui_hunger");
    }
    else if (in.hunger < kPeckishBelow)
    {
        add("Peckish", HudSeverity::Mild, "ui_hunger");
    }

    if (in.bodyTemperature < NeedsSystem::kHypothermicBelow)
    {
        add("Hypothermic", HudSeverity::Critical, "");
    }
    else if (in.bodyTemperature < NeedsSystem::kColdBelow)
    {
        add("Cold", HudSeverity::Serious, "");
    }
    else if (in.bodyTemperature < NeedsSystem::kChillyBelow)
    {
        add("Chilly", HudSeverity::Mild, "");
    }
    else if (in.bodyTemperature > NeedsSystem::kOverheatedAbove)
    {
        add("Overheated", HudSeverity::Serious, "");
    }

    if (in.illness >= kGravelyIllAbove)
    {
        add("Gravely ill", HudSeverity::Critical, "ui_skull");
    }
    else if (in.illness > kSickAbove)
    {
        add("Sick", HudSeverity::Serious, "ui_skull");
    }
    else if (in.illness > kQueasyAbove)
    {
        add("Queasy", HudSeverity::Mild, "ui_skull");
    }

    if (!in.sleeping)
    {
        if (in.sleepDebt > kExhaustedAbove)
        {
            add("Exhausted", HudSeverity::Critical, "ui_sleep");
        }
        else if (in.sleepDebt > NeedsSystem::kTiredAbove)
        {
            add("Tired", HudSeverity::Serious, "ui_sleep");
        }
        else if (in.sleepDebt > kDrowsyAbove)
        {
            add("Drowsy", HudSeverity::Mild, "ui_sleep");
        }
    }

    if (in.fatigue > kWindedAbove)
    {
        add("Winded", HudSeverity::Mild, "ui_stamina");
    }
    if (in.overburdened)
    {
        add("Overburdened", HudSeverity::Serious, "");
    }

    if (in.panic >= kPanickingAt)
    {
        add("Panicking", HudSeverity::Critical, "");
    }
    else if (in.panic >= kScaredAt)
    {
        add("Scared", HudSeverity::Serious, "");
    }
    else if (in.panic >= kAnxiousAt)
    {
        add("Anxious", HudSeverity::Mild, "");
    }

    if (in.inCrisis)
    {
        add("In crisis", HudSeverity::Critical, "");
    }
    else if (in.depression >= kDespairAt)
    {
        add("Despairing", HudSeverity::Critical, "");
    }
    else if (in.depression >= kDepressedAt)
    {
        add("Depressed", HudSeverity::Serious, "");
    }
    else if (in.depression >= kLowSpiritsAt)
    {
        add("Low spirits", HudSeverity::Mild, "");
    }

    if (in.desensitisation >= kNumbAt)
    {
        add("Numb", HudSeverity::Info, "");
    }
    if (in.sleeping)
    {
        add("Asleep", HudSeverity::Info, "ui_sleep");
    }

    // Most severe first; ties keep the order above.
    std::stable_sort(out.begin(), out.end(), [](const HudStatus& a, const HudStatus& b)
    {
        return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    });
    return out;
}

void Hud::drawShapes(SpriteBatch& batch, const SpriteSheet& sheet, const BitmapFont& font,
                     const HudState& state, const glm::vec2& viewport) const
{
    if (!state.visible)
    {
        return;
    }

    const Layout layout = computeLayout(state, viewport);
    const glm::vec4 white = sheet.uvRect("solid_white");
    const float pulse = 0.5f + 0.5f * std::sin(state.timeSeconds * 6.0f);

    // A line on the world: a dark edge keeps it readable over anything.
    const auto line = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        batch.draw(pos - glm::vec2(1.0f), size + glm::vec2(2.0f), white, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f * color.a));
        batch.draw(pos, size, white, color);
    };

    // Low-health vignette: red edges that close in as health drops.
    const float healthRatio = ratio(state.health, state.healthMax);
    if (healthRatio < 0.4f)
    {
        const float intensity = std::min(1.0f, 0.35f + (0.4f - healthRatio) / 0.4f) * (0.7f + 0.3f * pulse);
        constexpr int kBands = 8;
        constexpr float kBandThickness = 12.0f;
        for (int i = 0; i < kBands; ++i)
        {
            const float inset = static_cast<float>(i) * kBandThickness;
            const float falloff = 1.0f - static_cast<float>(i) / kBands;
            const float alpha = intensity * 0.42f * falloff * falloff;
            drawFrame(batch, white, glm::vec2(inset), viewport - glm::vec2(inset * 2.0f), kBandThickness,
                      glm::vec4(0.55f, 0.02f, 0.02f, alpha));
        }
    }

    // Body: health over stamina, two lines in the bottom-left corner.
    {
        const glm::vec2 pos(layout.barsX, layout.healthY);
        line(pos, glm::vec2(Hud::kBarsWidth, Hud::kHealthHeight), kTrackColor);
        glm::vec4 hpColor = healthColor(healthRatio);
        if (healthRatio <= 0.3f)
        {
            hpColor.a = 0.7f + 0.3f * pulse;
        }
        batch.draw(pos, glm::vec2(std::round(Hud::kBarsWidth * healthRatio), Hud::kHealthHeight), white, hpColor);
    }
    {
        const glm::vec2 pos(layout.barsX, layout.staminaY);
        const float baseMax = std::max(1.0f, std::max(state.staminaBaseMax, state.staminaMax));
        const float capRatio = ratio(state.staminaMax, baseMax);
        const float staminaRatio = ratio(state.stamina, baseMax);
        line(pos, glm::vec2(Hud::kBarsWidth, Hud::kStaminaHeight), kTrackColor);
        if (capRatio < 0.999f)
        {
            // The part that hunger, thirst, sleep and illness have taken away.
            const float capX = std::round(pos.x + Hud::kBarsWidth * capRatio);
            batch.draw(glm::vec2(capX, pos.y), glm::vec2(pos.x + Hud::kBarsWidth - capX, Hud::kStaminaHeight), white,
                       kStaminaLostColor);
        }
        // Out of breath (a fifth or less left): the line pulses.
        glm::vec4 staminaColor = kStaminaColor;
        if (ratio(state.stamina, state.staminaMax) <= 0.2f)
        {
            staminaColor.a = 0.45f + 0.55f * pulse;
        }
        batch.draw(pos, glm::vec2(std::round(Hud::kBarsWidth * staminaRatio), Hud::kStaminaHeight), white,
                   staminaColor);
    }

    // Held: the struggle meter as a row of ticks under the call to struggle.
    if (state.held && state.struggleTicksTotal > 0)
    {
        constexpr float kTick = 8.0f;
        constexpr float kGap = 4.0f;
        const float width = static_cast<float>(state.struggleTicksTotal) * (kTick + kGap) - kGap;
        const float x0 = std::round(layout.centreX - width * 0.5f);
        const float y = layout.messageY - 36.0f;
        for (int i = 0; i < state.struggleTicksTotal; ++i)
        {
            const bool filled = i < state.struggleTicks;
            line(glm::vec2(x0 + static_cast<float>(i) * (kTick + kGap), y), glm::vec2(kTick),
                 filled ? kAmber : kTrackColor);
        }
    }

    // Conditions: a dot for each, coloured by how bad it is.
    for (std::size_t i = 0; i < layout.conditionRows; ++i)
    {
        const HudStatus& status = state.conditions[i];
        glm::vec4 dot = severityColor(status.severity);
        if (status.severity == HudSeverity::Critical)
        {
            dot.a = 0.55f + 0.45f * pulse;
        }
        line(glm::vec2(layout.barsX, layout.conditionY(i) + 3.0f), glm::vec2(8.0f), dot);
    }

    if (layout.docked)
    {
        return; // the open panel carries the belt, and covers the clock
    }

    // Objective steps: a box before each, hollow until done.
    if (!state.objectiveTitle.empty())
    {
        for (std::size_t i = 0; i < state.objectiveSteps.size(); ++i)
        {
            const HudStep& step = state.objectiveSteps[i];
            const float textW = font.measureText(truncateText(step.text, kStepChars), kLabelScale).x;
            const glm::vec2 box(std::round(layout.rightX - textW - kStepGap - kStepBox),
                                layout.objectiveY + objectiveStepsTop(state) + static_cast<float>(i) * kStepRow + 2.0f);
            const glm::vec4 edge = step.done ? kDimTextColor : kTextColor;
            batch.draw(box - glm::vec2(1.0f), glm::vec2(kStepBox + 2.0f), white, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
            drawFrame(batch, white, box, glm::vec2(kStepBox), 2.0f, edge);
            if (step.done)
            {
                batch.draw(box + glm::vec2(3.0f), glm::vec2(kStepBox - 6.0f), white, edge);
            }
        }
    }

    // Belt: bare items, the one in use drawn large over a line.
    for (int i = 0; i < 8; ++i)
    {
        const HudSlot& slot = state.hotbar[static_cast<std::size_t>(i)];
        const bool active = (i == state.activeSlot);
        const float size = active ? Hud::kActiveIcon : Hud::kBeltIcon;
        const float x = layout.slotX[static_cast<std::size_t>(i)];
        const float width = layout.slotW[static_cast<std::size_t>(i)];
        const glm::vec2 iconPos(std::round(x + (width - size) * 0.5f), layout.iconBottom - size);

        glm::vec4 uv(0.0f);
        if (!slot.empty && !slot.icon.empty())
        {
            uv = sheet.uvRect(slot.icon);
        }
        if (hasSprite(uv))
        {
            batch.draw(iconPos + glm::vec2(0.0f, active ? 3.0f : 2.0f), glm::vec2(size), uv,
                       glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
            batch.draw(iconPos, glm::vec2(size), uv, active ? glm::vec4(1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.82f));
        }
        else if (slot.empty)
        {
            const glm::vec2 mark(std::round(x + width * 0.5f - 3.0f), layout.iconBottom - 19.0f);
            batch.draw(mark - glm::vec2(1.0f), glm::vec2(8.0f), white, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
            drawFrame(batch, white, mark, glm::vec2(6.0f), 1.0f, glm::vec4(kTextColor.r, kTextColor.g, kTextColor.b, 0.7f));
        }

        // Under the item in use: a line, which is also how worn a weapon is.
        const glm::vec4 wearColor = slot.wear > 0.66f   ? kTextColor
                                    : slot.wear > 0.33f ? kAmber
                                                        : glm::vec4(0.96f, 0.42f, 0.34f, 1.0f);
        if (active)
        {
            const glm::vec2 under(x, layout.iconBottom + 5.0f);
            if (!slot.empty && slot.wear >= 0.0f)
            {
                line(under, glm::vec2(width, 3.0f), kTrackColor);
                batch.draw(under, glm::vec2(std::max(2.0f, std::round(width * slot.wear)), 3.0f), white, wearColor);
            }
            else
            {
                line(under, glm::vec2(width, 3.0f), kTextColor);
            }
        }
        else if (!slot.empty && slot.wear >= 0.0f)
        {
            const glm::vec2 under(iconPos.x, layout.iconBottom + 1.0f);
            line(under, glm::vec2(size, 2.0f), kTrackColor);
            batch.draw(under, glm::vec2(std::max(1.0f, std::round(size * slot.wear)), 2.0f), white, wearColor);
        }
        // The weapon in hand, when it is not the slot in use.
        if (!slot.empty && slot.equipped && !active)
        {
            line(glm::vec2(std::round(x + width * 0.5f - 3.0f), iconPos.y - 9.0f), glm::vec2(6.0f), kAmber);
        }
    }
}

void Hud::drawText(SpriteBatch& batch, const BitmapFont& font, const HudState& state,
                   const glm::vec2& viewport) const
{
    if (!state.visible)
    {
        return;
    }

    const Layout layout = computeLayout(state, viewport);
    const float pulse = 0.5f + 0.5f * std::sin(state.timeSeconds * 6.0f);

    // Every label sits straight on the world, so each carries its own shadow.
    const auto put = [&](const glm::vec2& pos, const std::string& text, const glm::vec4& color, float scale,
                         TextAlign align = TextAlign::Left)
    {
        const float offset = scale >= 2.0f ? 2.0f : 1.0f;
        font.drawText(batch, pos + glm::vec2(offset), text, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f * color.a), scale, align);
        font.drawText(batch, pos, text, color, scale, align);
    };

    // Conditions.
    for (std::size_t i = 0; i < layout.conditionRows; ++i)
    {
        const HudStatus& status = state.conditions[i];
        glm::vec4 color = status.severity == HudSeverity::Info || status.severity == HudSeverity::Mild
                              ? kTextColor
                              : severityColor(status.severity);
        if (status.severity == HudSeverity::Critical)
        {
            color = glm::mix(color, glm::vec4(1.0f), 0.25f * pulse);
        }
        put(glm::vec2(layout.barsX + 16.0f, layout.conditionY(i)), status.label, color, kLabelScale);
    }
    if (state.conditions.size() > layout.conditionRows)
    {
        put(glm::vec2(layout.barsX, layout.conditionY(0) - 14.0f),
            "+" + std::to_string(state.conditions.size() - layout.conditionRows) + " more", kDimTextColor, kSmallScale);
    }

    if (!state.vehicleLine.empty())
    {
        put(glm::vec2(Hud::kEdge, Hud::kEdge), state.vehicleLine, state.driving ? kAmber : kTextColor, kLabelScale);
    }

    // Day card.
    if (!state.dayCardTitle.empty() && state.dayCardAlpha > 0.01f)
    {
        const float y = std::round(viewport.y * 0.22f);
        put(glm::vec2(layout.centreX, y), state.dayCardTitle, withAlpha(kTextColor, state.dayCardAlpha),
            kDayCardTitleScale, TextAlign::Center);
        if (!state.dayCardLine.empty())
        {
            put(glm::vec2(layout.centreX, y + 36.0f), state.dayCardLine, withAlpha(kDimTextColor, state.dayCardAlpha),
                kLabelScale, TextAlign::Center);
        }
    }

    // District toast.
    if (!state.districtToast.empty() && state.districtToastAlpha > 0.01f)
    {
        const glm::vec4 color = state.districtDanger ? glm::vec4(1.0f, 0.54f, 0.44f, 1.0f) : kTextColor;
        put(glm::vec2(layout.centreX, layout.toastY), state.districtToast, withAlpha(color, state.districtToastAlpha),
            kLabelScale, TextAlign::Center);
    }

    if (state.held)
    {
        put(glm::vec2(layout.centreX, layout.messageY - 62.0f), "HELD - SPACE",
            glm::mix(kAmber, glm::vec4(1.0f), 0.3f * pulse), kLabelScale, TextAlign::Center);
    }

    // Message (transient) wins over the standing interaction prompt.
    const bool showMessage = !state.message.empty() && state.messageAlpha > 0.01f;
    if (showMessage)
    {
        put(glm::vec2(layout.centreX, layout.messageY), truncateText(state.message, 72),
            withAlpha(kTextColor, state.messageAlpha), kLabelScale, TextAlign::Center);
    }
    else if (!state.prompt.empty())
    {
        put(glm::vec2(layout.centreX, layout.messageY), truncateText(state.prompt, 72), kTextColor, kLabelScale,
            TextAlign::Center);
    }

    if (layout.docked)
    {
        return;
    }

    // Time, then the day and the weather.
    put(glm::vec2(layout.rightX, layout.clockY), clockText(state.hours), clockColor(state.hours), kClockScale,
        TextAlign::Right);
    put(glm::vec2(layout.rightX, layout.dayY), "Day " + std::to_string(state.day) + "  " + weatherText(state),
        kTextColor, kLabelScale, TextAlign::Right);
    if (!state.districtLine.empty())
    {
        put(glm::vec2(layout.rightX, layout.dayY + 22.0f), truncateText(state.districtLine, 30),
            state.districtLineDanger ? glm::vec4(1.0f, 0.54f, 0.44f, 1.0f) : kDimTextColor, kLabelScale,
            TextAlign::Right);
    }

    // The thing to do, and the day's plan under it.
    if (!state.objectiveTitle.empty())
    {
        put(glm::vec2(layout.rightX, layout.objectiveY + 6.0f), truncateText(state.objectiveTitle, 40), kAmber,
            kLabelScale, TextAlign::Right);
        if (!state.objectiveDetail.empty())
        {
            put(glm::vec2(layout.rightX, layout.objectiveY + 28.0f), truncateText(state.objectiveDetail, 60),
                kDimTextColor, kSmallScale, TextAlign::Right);
        }
        for (std::size_t i = 0; i < state.objectiveSteps.size(); ++i)
        {
            const HudStep& step = state.objectiveSteps[i];
            put(glm::vec2(layout.rightX, layout.objectiveY + objectiveStepsTop(state) + static_cast<float>(i) * kStepRow),
                truncateText(step.text, kStepChars), step.done ? kDimTextColor : kTextColor, kLabelScale,
                TextAlign::Right);
        }
    }

    // Belt: a number under each item that is not in use, and how many.
    for (int i = 0; i < 8; ++i)
    {
        const HudSlot& slot = state.hotbar[static_cast<std::size_t>(i)];
        const bool active = (i == state.activeSlot);
        const float x = layout.slotX[static_cast<std::size_t>(i)];
        const float width = layout.slotW[static_cast<std::size_t>(i)];
        if (!active)
        {
            put(glm::vec2(std::round(x + width * 0.5f), layout.iconBottom + 6.0f), std::to_string(i + 1), kDimTextColor,
                kSmallScale, TextAlign::Center);
        }
        if (!slot.empty && slot.count > 1)
        {
            put(glm::vec2(x + width, layout.iconBottom - 10.0f), std::to_string(slot.count), kTextColor, kSmallScale,
                TextAlign::Right);
        }
    }

    const HudSlot& activeSlot = state.hotbar[static_cast<std::size_t>(std::clamp(state.activeSlot, 0, 7))];
    if (!activeSlot.empty && !activeSlot.name.empty())
    {
        put(glm::vec2(layout.centreX, layout.itemNameY), Hud::slotLabel(activeSlot), kTextColor, kLabelScale,
            TextAlign::Center);
    }
}
