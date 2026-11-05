#include "ui/Hud.h"

#include <algorithm>
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

constexpr float kPanelPadding = 10.0f;
constexpr float kIconSize = 20.0f;
constexpr float kBarRowHeight = 18.0f;
constexpr float kBarRowGap = 8.0f;
constexpr float kValueColumnWidth = 48.0f;

const glm::vec4 kPanelColor(0.03f, 0.04f, 0.06f, 0.72f);
const glm::vec4 kPanelEdgeColor(0.72f, 0.62f, 0.40f, 0.55f);
const glm::vec4 kBarTrackColor(0.10f, 0.10f, 0.11f, 0.95f);
const glm::vec4 kStaminaColor(0.36f, 0.74f, 0.86f, 1.0f);
const glm::vec4 kStaminaLostColor(0.42f, 0.16f, 0.16f, 0.95f);
const glm::vec4 kTextColor(0.92f, 0.91f, 0.84f, 1.0f);
const glm::vec4 kDimTextColor(0.70f, 0.72f, 0.70f, 0.90f);

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
        return {0.44f, 0.80f, 0.40f, 1.0f};
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

// The UI icons are drawn in the middle of 32px cells; crop to the drawn part so
// they show at 1:1 instead of being shrunk.
glm::vec4 iconUv(const glm::vec4& uv)
{
    constexpr float kCrop = 6.0f / 32.0f;
    return {uv.x + uv.z * kCrop, uv.y + uv.w * kCrop, uv.z * (1.0f - 2.0f * kCrop), uv.w * (1.0f - 2.0f * kCrop)};
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

void drawPanel(SpriteBatch& batch, const glm::vec4& uv, const glm::vec2& pos, const glm::vec2& size)
{
    batch.draw(pos, size, uv, kPanelColor);
    batch.draw(pos, glm::vec2(size.x, 1.0f), uv, kPanelEdgeColor);
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

    glm::vec2 vitalsPos{0.0f};
    glm::vec2 vitalsSize{0.0f};
    float healthRowY = 0.0f;
    float staminaRowY = 0.0f;
    float barX = 0.0f;
    float barWidth = 0.0f;

    float conditionsY = 0.0f;
    std::size_t conditionRows = 0;

    glm::vec2 clockPos{0.0f};
    glm::vec2 clockSize{0.0f};
    float objectiveY = 0.0f;

    glm::vec2 hotbarPos{0.0f};
    float hotbarWidth = 0.0f;
    float itemNameY = 0.0f;
    float messageY = 0.0f;

    float toastY = 0.0f;
};

Layout computeLayout(const HudState& state, const glm::vec2& viewport)
{
    Layout layout;
    layout.viewport = viewport;

    layout.vitalsPos = glm::vec2(Hud::kMargin, Hud::kMargin);
    layout.healthRowY = layout.vitalsPos.y + kPanelPadding;
    layout.staminaRowY = layout.healthRowY + kBarRowHeight + kBarRowGap;
    layout.vitalsSize = glm::vec2(Hud::kVitalsWidth, kPanelPadding * 2.0f + kBarRowHeight * 2.0f + kBarRowGap);
    layout.barX = layout.vitalsPos.x + kPanelPadding + kIconSize + 8.0f;
    layout.barWidth = layout.vitalsPos.x + layout.vitalsSize.x - kPanelPadding - kValueColumnWidth - layout.barX;

    layout.conditionsY = layout.vitalsPos.y + layout.vitalsSize.y + 8.0f;
    layout.conditionRows = std::min(state.conditions.size(), Hud::kMaxConditionRows);

    const float clockRows = state.districtLine.empty() ? 2.0f : 3.0f;
    layout.clockSize = glm::vec2(Hud::kClockWidth, kPanelPadding * 2.0f + clockRows * 18.0f + (clockRows - 1.0f) * 8.0f);
    layout.clockPos = glm::vec2(viewport.x - Hud::kMargin - layout.clockSize.x, Hud::kMargin);
    layout.objectiveY = layout.clockPos.y + layout.clockSize.y + 10.0f;

    layout.hotbarWidth = 8.0f * Hud::kSlotSize + 7.0f * Hud::kSlotGap;
    layout.hotbarPos = glm::vec2(std::round((viewport.x - layout.hotbarWidth) * 0.5f),
                                 viewport.y - Hud::kMargin - Hud::kSlotSize);
    layout.itemNameY = layout.hotbarPos.y - 26.0f;
    layout.messageY = layout.itemNameY - 30.0f;

    layout.toastY = Hud::kMargin + 4.0f;
    return layout;
}

void drawTextStrip(SpriteBatch& batch, const glm::vec4& uv, float centerX, float y, float width, float height,
                   float alpha)
{
    const float padX = 10.0f;
    batch.draw(glm::vec2(std::round(centerX - width * 0.5f - padX), y - 5.0f),
               glm::vec2(width + padX * 2.0f, height + 10.0f), uv,
               glm::vec4(0.02f, 0.03f, 0.04f, 0.62f * alpha));
}

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

    // Vitals panel.
    drawPanel(batch, white, layout.vitalsPos, layout.vitalsSize);
    const float iconX = layout.vitalsPos.x + kPanelPadding;

    const glm::vec4 heartUv = iconUv(sheet.uvRect("ui_heart"));
    batch.draw(glm::vec2(iconX, layout.healthRowY - 1.0f), glm::vec2(kIconSize), heartUv,
               glm::vec4(1.0f, 0.78f, 0.74f, 1.0f));
    const float barY0 = layout.healthRowY + 3.0f;
    const float barH = kBarHeight;
    batch.draw(glm::vec2(layout.barX, barY0), glm::vec2(layout.barWidth, barH), white, kBarTrackColor);
    glm::vec4 hpColor = healthColor(healthRatio);
    if (healthRatio <= 0.3f)
    {
        hpColor.a = 0.7f + 0.3f * pulse;
    }
    batch.draw(glm::vec2(layout.barX, barY0), glm::vec2(std::round(layout.barWidth * healthRatio), barH), white,
               hpColor);

    const glm::vec4 staminaUv = iconUv(sheet.uvRect("ui_stamina"));
    batch.draw(glm::vec2(iconX, layout.staminaRowY - 1.0f), glm::vec2(kIconSize), staminaUv,
               glm::vec4(0.74f, 0.92f, 1.0f, 1.0f));
    const float barY1 = layout.staminaRowY + 3.0f;
    const float baseMax = std::max(1.0f, std::max(state.staminaBaseMax, state.staminaMax));
    const float capRatio = ratio(state.staminaMax, baseMax);
    const float staminaRatio = ratio(state.stamina, baseMax);
    batch.draw(glm::vec2(layout.barX, barY1), glm::vec2(layout.barWidth, barH), white, kBarTrackColor);
    if (capRatio < 0.999f)
    {
        // The part of the bar that hunger, thirst, sleep and illness have taken away.
        const float capX = std::round(layout.barX + layout.barWidth * capRatio);
        batch.draw(glm::vec2(capX, barY1), glm::vec2(layout.barX + layout.barWidth - capX, barH), white,
                   kStaminaLostColor);
    }
    batch.draw(glm::vec2(layout.barX, barY1), glm::vec2(std::round(layout.barWidth * staminaRatio), barH), white,
               kStaminaColor);

    // Condition rows: severity chip, icon, label backing.
    for (std::size_t i = 0; i < layout.conditionRows; ++i)
    {
        const HudStatus& status = state.conditions[i];
        const float rowY = layout.conditionsY + static_cast<float>(i) * kConditionRowHeight;
        const float labelW = font.measureText(status.label, kLabelScale).x;
        const float rowW = 4.0f + 6.0f + kIconSize + 6.0f + labelW + 10.0f;
        batch.draw(glm::vec2(Hud::kMargin, rowY), glm::vec2(rowW, kConditionRowHeight - 4.0f), white, kPanelColor);

        glm::vec4 chip = severityColor(status.severity);
        if (status.severity == HudSeverity::Critical)
        {
            chip.a = 0.55f + 0.45f * pulse;
        }
        batch.draw(glm::vec2(Hud::kMargin, rowY), glm::vec2(4.0f, kConditionRowHeight - 4.0f), white, chip);

        if (!status.icon.empty())
        {
            const glm::vec4 uv = sheet.uvRect(status.icon);
            if (hasSprite(uv))
            {
                batch.draw(glm::vec2(Hud::kMargin + 8.0f, rowY), glm::vec2(kIconSize), iconUv(uv),
                           glm::vec4(1.0f));
            }
        }
    }

    // Clock panel.
    drawPanel(batch, white, layout.clockPos, layout.clockSize);
    const glm::vec4 clockUv = iconUv(sheet.uvRect("ui_clock"));
    batch.draw(glm::vec2(layout.clockPos.x + kPanelPadding, layout.clockPos.y + kPanelPadding - 1.0f),
               glm::vec2(kIconSize), clockUv, glm::vec4(1.0f));

    // Objective backing.
    if (!state.objectiveTitle.empty())
    {
        const float titleW = font.measureText(truncateText(state.objectiveTitle, 40), kLabelScale).x;
        const float detailW = font.measureText(truncateText(state.objectiveDetail, 60), kSmallScale).x;
        const float w = std::max(titleW, detailW) + 20.0f;
        const float h = state.objectiveDetail.empty() ? 28.0f : 42.0f;
        batch.draw(glm::vec2(viewport.x - Hud::kMargin - w, layout.objectiveY), glm::vec2(w, h), white,
                   glm::vec4(0.02f, 0.03f, 0.04f, 0.55f));
        batch.draw(glm::vec2(viewport.x - Hud::kMargin - 3.0f, layout.objectiveY), glm::vec2(3.0f, h), white,
                   state.objectiveColor);
    }

    // District toast backing.
    if (!state.districtToast.empty() && state.districtToastAlpha > 0.01f)
    {
        const float w = font.measureText(state.districtToast, kLabelScale).x;
        drawTextStrip(batch, white, viewport.x * 0.5f, layout.toastY, w, 18.0f, state.districtToastAlpha);
    }

    // Hotbar.
    for (int i = 0; i < 8; ++i)
    {
        const HudSlot& slot = state.hotbar[static_cast<std::size_t>(i)];
        const glm::vec2 pos(layout.hotbarPos.x + static_cast<float>(i) * (Hud::kSlotSize + Hud::kSlotGap),
                            layout.hotbarPos.y);
        const bool active = (i == state.activeSlot);
        batch.draw(pos, glm::vec2(Hud::kSlotSize), white,
                   active ? glm::vec4(0.10f, 0.09f, 0.05f, 0.85f) : glm::vec4(0.03f, 0.04f, 0.06f, 0.70f));
        drawFrame(batch, white, pos, glm::vec2(Hud::kSlotSize), active ? 2.0f : 1.0f,
                  active ? glm::vec4(0.98f, 0.84f, 0.38f, 1.0f) : glm::vec4(0.55f, 0.52f, 0.44f, 0.55f));

        if (!slot.empty && !slot.icon.empty())
        {
            const glm::vec4 uv = sheet.uvRect(slot.icon);
            if (hasSprite(uv))
            {
                constexpr float kItemIcon = 32.0f;
                const float inset = (Hud::kSlotSize - kItemIcon) * 0.5f;
                batch.draw(pos + glm::vec2(inset, inset + 2.0f), glm::vec2(kItemIcon), uv,
                           active ? glm::vec4(1.0f) : glm::vec4(0.88f, 0.88f, 0.88f, 1.0f));
            }
        }

        // Weapons: wear along the bottom, a gold corner on the one in hand.
        if (!slot.empty && slot.wear >= 0.0f)
        {
            const glm::vec2 barPos = pos + glm::vec2(5.0f, Hud::kSlotSize - 7.0f);
            const float barW = Hud::kSlotSize - 10.0f;
            batch.draw(barPos, glm::vec2(barW, 3.0f), white, glm::vec4(0.0f, 0.0f, 0.0f, 0.65f));
            const glm::vec4 wearColor = slot.wear > 0.66f   ? glm::vec4(0.47f, 0.80f, 0.42f, 1.0f)
                                        : slot.wear > 0.33f ? glm::vec4(0.95f, 0.74f, 0.28f, 1.0f)
                                                            : glm::vec4(0.92f, 0.33f, 0.27f, 1.0f);
            batch.draw(barPos, glm::vec2(std::max(1.0f, std::round(barW * slot.wear)), 3.0f), white, wearColor);
        }
        if (!slot.empty && slot.equipped)
        {
            const glm::vec4 gold(0.98f, 0.84f, 0.38f, 1.0f);
            const glm::vec2 corner = pos + glm::vec2(Hud::kSlotSize - 3.0f, 3.0f);
            for (int k = 0; k < 6; ++k)
            {
                batch.draw(corner + glm::vec2(-static_cast<float>(6 - k), static_cast<float>(k)),
                           glm::vec2(static_cast<float>(6 - k), 1.0f), white, gold);
            }
        }
    }

    // Selected item name and message backings.
    const HudSlot& activeSlot = state.hotbar[static_cast<std::size_t>(std::clamp(state.activeSlot, 0, 7))];
    if (!activeSlot.empty && !activeSlot.name.empty())
    {
        const float w = font.measureText(Hud::slotLabel(activeSlot), kLabelScale).x;
        drawTextStrip(batch, white, viewport.x * 0.5f, layout.itemNameY, w, 18.0f, 0.8f);
    }

    const bool showMessage = !state.message.empty() && state.messageAlpha > 0.01f;
    const std::string& line = showMessage ? state.message : state.prompt;
    if (!line.empty())
    {
        const float w = font.measureText(truncateText(line, 72), kLabelScale).x;
        drawTextStrip(batch, white, viewport.x * 0.5f, layout.messageY, w, 18.0f,
                      showMessage ? state.messageAlpha : 0.9f);
    }

    // Vehicle readout backing (bottom-left).
    if (!state.vehicleLine.empty())
    {
        const float w = font.measureText(state.vehicleLine, kLabelScale).x;
        batch.draw(glm::vec2(Hud::kMargin, viewport.y - Hud::kMargin - 28.0f), glm::vec2(w + 20.0f, 28.0f), white,
                   kPanelColor);
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

    // Vitals values, right-aligned in the value column.
    const float valueX = layout.vitalsPos.x + layout.vitalsSize.x - kPanelPadding;
    const float healthRatio = ratio(state.health, state.healthMax);
    font.drawText(batch, glm::vec2(valueX, layout.healthRowY),
                  std::to_string(static_cast<int>(std::ceil(std::max(0.0f, state.health)))),
                  healthColor(healthRatio), kLabelScale, TextAlign::Right);
    font.drawText(batch, glm::vec2(valueX, layout.staminaRowY),
                  std::to_string(static_cast<int>(std::max(0.0f, state.stamina))), kStaminaColor, kLabelScale,
                  TextAlign::Right);

    // Conditions.
    for (std::size_t i = 0; i < layout.conditionRows; ++i)
    {
        const HudStatus& status = state.conditions[i];
        const float rowY = layout.conditionsY + static_cast<float>(i) * kConditionRowHeight;
        glm::vec4 color = severityColor(status.severity);
        if (status.severity == HudSeverity::Critical)
        {
            color = glm::mix(color, glm::vec4(1.0f), 0.25f * pulse);
        }
        font.drawText(batch, glm::vec2(Hud::kMargin + 10.0f + kIconSize + 6.0f, rowY + 2.0f), status.label, color,
                      kLabelScale);
    }
    if (state.conditions.size() > layout.conditionRows)
    {
        const float rowY = layout.conditionsY + static_cast<float>(layout.conditionRows) * kConditionRowHeight;
        font.drawText(batch, glm::vec2(Hud::kMargin + 4.0f, rowY),
                      "+" + std::to_string(state.conditions.size() - layout.conditionRows) + " more",
                      kDimTextColor, kSmallScale);
    }

    // Clock panel: day and time, then weather.
    const float clockTextX = layout.clockPos.x + kPanelPadding + kIconSize + 8.0f;
    const float clockRight = layout.clockPos.x + layout.clockSize.x - kPanelPadding;
    const float clockRow0 = layout.clockPos.y + kPanelPadding;
    const float clockRow1 = clockRow0 + 18.0f + 8.0f;
    font.drawText(batch, glm::vec2(clockTextX, clockRow0), "DAY " + std::to_string(state.day), kTextColor,
                  kLabelScale);
    font.drawText(batch, glm::vec2(clockRight, clockRow0), clockText(state.hours), clockColor(state.hours),
                  kLabelScale, TextAlign::Right);
    font.drawText(batch, glm::vec2(clockRight, clockRow1), truncateText(weatherText(state), 17), kDimTextColor,
                  kLabelScale, TextAlign::Right);
    if (!state.districtLine.empty())
    {
        const glm::vec4 color = state.districtLineDanger ? glm::vec4(1.0f, 0.48f, 0.36f, 1.0f)
                                                         : glm::vec4(0.86f, 0.74f, 0.46f, 0.95f);
        font.drawText(batch, glm::vec2(clockRight, clockRow1 + 26.0f), truncateText(state.districtLine, 17), color,
                      kLabelScale, TextAlign::Right);
    }

    // Objective under the clock.
    if (!state.objectiveTitle.empty())
    {
        const float rightX = viewport.x - Hud::kMargin - 10.0f;
        font.drawText(batch, glm::vec2(rightX, layout.objectiveY + 6.0f), truncateText(state.objectiveTitle, 40),
                      state.objectiveColor, kLabelScale, TextAlign::Right);
        if (!state.objectiveDetail.empty())
        {
            font.drawText(batch, glm::vec2(rightX, layout.objectiveY + 28.0f),
                          truncateText(state.objectiveDetail, 60), kDimTextColor, kSmallScale, TextAlign::Right);
        }
    }

    // District toast.
    if (!state.districtToast.empty() && state.districtToastAlpha > 0.01f)
    {
        const glm::vec4 color = state.districtDanger ? glm::vec4(1.0f, 0.48f, 0.36f, 1.0f)
                                                     : glm::vec4(0.94f, 0.86f, 0.60f, 1.0f);
        font.drawText(batch, glm::vec2(viewport.x * 0.5f, layout.toastY), state.districtToast,
                      withAlpha(color, state.districtToastAlpha), kLabelScale, TextAlign::Center);
    }

    // Hotbar labels: key numbers, counts, and short names for items without art.
    for (int i = 0; i < 8; ++i)
    {
        const HudSlot& slot = state.hotbar[static_cast<std::size_t>(i)];
        const glm::vec2 pos(layout.hotbarPos.x + static_cast<float>(i) * (Hud::kSlotSize + Hud::kSlotGap),
                            layout.hotbarPos.y);
        const bool active = (i == state.activeSlot);
        font.drawText(batch, pos + glm::vec2(4.0f, 4.0f), std::to_string(i + 1),
                      active ? glm::vec4(0.98f, 0.84f, 0.38f, 1.0f) : glm::vec4(0.70f, 0.68f, 0.60f, 0.9f),
                      kSmallScale);
        if (!slot.empty && slot.count > 1)
        {
            font.drawText(batch, pos + glm::vec2(Hud::kSlotSize - 4.0f, Hud::kSlotSize - 12.0f),
                          std::to_string(slot.count), kTextColor, kSmallScale, TextAlign::Right);
        }
    }

    const HudSlot& activeSlot = state.hotbar[static_cast<std::size_t>(std::clamp(state.activeSlot, 0, 7))];
    if (!activeSlot.empty && !activeSlot.name.empty())
    {
        font.drawText(batch, glm::vec2(viewport.x * 0.5f, layout.itemNameY), Hud::slotLabel(activeSlot),
                      glm::vec4(0.98f, 0.90f, 0.62f, 0.95f), kLabelScale, TextAlign::Center);
    }

    // Message (transient) wins over the standing interaction prompt.
    const bool showMessage = !state.message.empty() && state.messageAlpha > 0.01f;
    if (showMessage)
    {
        font.drawText(batch, glm::vec2(viewport.x * 0.5f, layout.messageY), truncateText(state.message, 72),
                      glm::vec4(1.0f, 0.97f, 0.84f, state.messageAlpha), kLabelScale, TextAlign::Center);
    }
    else if (!state.prompt.empty())
    {
        font.drawText(batch, glm::vec2(viewport.x * 0.5f, layout.messageY), truncateText(state.prompt, 72),
                      glm::vec4(0.86f, 0.92f, 0.72f, 0.95f), kLabelScale, TextAlign::Center);
    }

    if (!state.vehicleLine.empty())
    {
        font.drawText(batch, glm::vec2(Hud::kMargin + 10.0f, viewport.y - Hud::kMargin - 23.0f), state.vehicleLine,
                      state.driving ? glm::vec4(0.94f, 0.90f, 0.56f, 1.0f) : glm::vec4(0.66f, 0.80f, 0.86f, 0.95f),
                      kLabelScale);
    }
}
