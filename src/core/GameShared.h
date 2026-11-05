#pragma once

// Constants, layouts and small helpers shared by the Game*.cpp files.
// Everything here was file-local to Game.cpp before the split.

#include "core/Game.h"

#include "map/TileIds.h"
#include "systems/ToneTuning.h"

#include <glm/gtc/matrix_transform.hpp>

#include "components/Animation.h"
#include "components/Bleeding.h"
#include "components/Combat.h"
#include "components/Fire.h"
#include "components/Facing.h"
#include "components/Health.h"
#include "components/Interactable.h"
#include "components/LegacyLink.h"
#include "components/Inventory.h"
#include "components/MentalState.h"
#include "components/Needs.h"
#include "components/PlayerInput.h"
#include "components/Projectile.h"
#include "components/Sleeping.h"
#include "components/Wounds.h"
#include "components/Traits.h"
#include "components/Sprite.h"
#include "components/Stamina.h"
#include "components/Structure.h"
#include "components/Transform.h"
#include "components/Vehicle.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "components/SurvivorAI.h"
#include "dev/DevHarness.h"
#include "world/EntityFactory.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>

namespace gameshared
{
constexpr int kDefaultWindowWidth = 1280;
constexpr int kDefaultWindowHeight = 720;
constexpr float kSpriteSize = 32.0f;
constexpr float kPlayerFootOffsetY = 22.0f;
constexpr float kMaxFrameStepSeconds = 0.25f;
constexpr double kFixedTimeStepSeconds = 1.0 / 60.0;
constexpr double kGameHoursPerRealSecond = 1.0 / 60.0;
constexpr double kDeathRestartDelaySeconds = 3.0;
constexpr float kVehicleInteractRangePixels = 64.0f;
constexpr float kVehicleCollisionRangePixels = 26.0f;
constexpr float kVehicleFuelPerSecond = 0.28f;
constexpr float kVehicleRefuelAmount = 12.0f;
constexpr float kVehicleBaseSpeed = 248.0f;
constexpr float kMinimapZoomMin = 0.70f;
constexpr float kMinimapZoomMax = 4.20f;
constexpr float kMinimapZoomStep = 0.20f;

constexpr std::array<const char*, 3> kTitleSaveSlots = {
    "default_slot",
    "frontier_slot",
    "ironman_slot"
};

constexpr std::array<const char*, 3> kTitleSaveLabels = {
    "Default",
    "Frontier",
    "Ironman"
};

constexpr std::array<const char*, 2> kTitleGameModeNames = {
    "CAMPAIGN",
    "ENDLESS"
};

constexpr std::array<const char*, 2> kTitleGameModeDescriptions = {
    "Objective-driven run with an infection deadline",
    "No win condition; survive and shape world history"
};

constexpr std::array<const char*, 6> kTitleModeNames = {
    "OVERWHELMING SWARM",
    "STALKER",
    "MUTATING THREAT",
    "INFECTED SOCIETY",
    "UNKNOWN",
    "HYBRID"
};

constexpr std::array<const char*, 6> kTitleModeDescriptions = {
    "Huge populations; pressure through volume",
    "Fewer but persistent hunters",
    "Threat escalates as days pass",
    "Human hostility dominates district danger",
    "Unstable behavior patterns and uncertainty",
    "Swarm pressure plus late-game mutation"
};

constexpr std::array<const char*, 3> kTitleToneNames = {
    "HOPELESS",
    "GRIM HUMAN",
    "BRUTAL ARCADE"
};

constexpr std::array<const char*, 3> kTitleToneDescriptions = {
    "Scarce supplies, tougher dead, darker nights, drained colour",
    "The intended balance: dark, scarce, but fair",
    "More dead but weaker, more weapons, quicker breath"
};

constexpr std::array<const char*, 16> kBriefingLines = {
    "Noise travels farther than courage.",
    "One bad hallway can end a week-long run.",
    "Water first. Heroics later.",
    "You are not clearing the city. You are outlasting it.",
    "Night rewards discipline, not speed.",
    "Every grave was someone who thought they had one more fight.",
    "Caches buy time. Time buys options.",
    "When panic rises, decisions collapse.",
    "If a district feels quiet, assume it is listening.",
    "Residential blocks hide hunger better than danger.",
    "Storefront glass means lines of sight for you and for them.",
    "Factories keep supplies and echoes in equal measure.",
    "A safe route at noon can be a trap by dusk.",
    "Fight less. Return alive more.",
    "The city does not forgive routine.",
    "Leave one exit clear before you open a door."
};

constexpr std::array<const char*, 10> kTitleMenuItems = {
    "Continue Run",
    "Start New Run",
    "Start New World",
    "Save Slot",
    "Game Mode",
    "Threat",
    "Tone",
    "Controls",
    "Credits",
    "Quit"
};

inline int wrapIndex(int value, int count)
{
    if (count <= 0)
    {
        return 0;
    }
    int wrapped = value % count;
    if (wrapped < 0)
    {
        wrapped += count;
    }
    return wrapped;
}

// Marker colour for a legacy lead: the minimap, the town map and the notebook
// all use these, so a colour means the same thing everywhere.
inline glm::vec4 legacyLeadColor(legacy::LeadKind kind)
{
    switch (kind)
    {
    case legacy::LeadKind::Risen: return glm::vec4(0.94f, 0.34f, 0.28f, 1.0f);
    case legacy::LeadKind::Pack: return glm::vec4(0.96f, 0.72f, 0.30f, 1.0f);
    case legacy::LeadKind::Survivor: return glm::vec4(0.50f, 0.71f, 0.96f, 1.0f);
    case legacy::LeadKind::Grave: break;
    }
    return glm::vec4(0.71f, 0.71f, 0.73f, 1.0f);
}

inline std::string truncateHudText(const std::string& text, std::size_t maxChars)
{
    if (text.size() <= maxChars)
    {
        return text;
    }
    if (maxChars <= 3)
    {
        return text.substr(0, maxChars);
    }
    return text.substr(0, maxChars - 3) + "...";
}

inline void addUniqueTilePair(std::vector<std::pair<int, int>>& tiles, int x, int y)
{
    for (const auto& [tx, ty] : tiles)
    {
        if (tx == x && ty == y)
        {
            return;
        }
    }
    tiles.emplace_back(x, y);
}

inline bool hasTilePair(const std::vector<std::pair<int, int>>& tiles, int x, int y)
{
    for (const auto& [tx, ty] : tiles)
    {
        if (tx == x && ty == y)
        {
            return true;
        }
    }
    return false;
}

inline glm::ivec2 worldToTile(const TileMap& tileMap, const glm::vec2& world)
{
    const int tw = std::max(1, tileMap.tileWidth());
    const int th = std::max(1, tileMap.tileHeight());
    return glm::ivec2(
        static_cast<int>(std::floor(world.x / static_cast<float>(tw))),
        static_cast<int>(std::floor(world.y / static_cast<float>(th))));
}

inline const char* vehicleConditionLabel(float condition)
{
    if (condition >= 70.0f) return "GOOD";
    if (condition >= 40.0f) return "WORN";
    if (condition >= 10.0f) return "DAMAGED";
    return "BROKEN";
}

inline float vehicleSpeedFromCondition(float condition)
{
    if (condition >= 70.0f) return kVehicleBaseSpeed;
    if (condition >= 40.0f) return kVehicleBaseSpeed * 0.86f;
    if (condition >= 10.0f) return kVehicleBaseSpeed * 0.72f;
    return 0.0f;
}

inline float fireBaseFlammability(SurfaceType surface)
{
    switch (surface)
    {
    case SurfaceType::Carpet:
        return 0.62f;
    case SurfaceType::Grass:
        return 0.74f;
    case SurfaceType::Default:
        return 0.36f;
    case SurfaceType::Gravel:
        return 0.08f;
    case SurfaceType::Metal:
        return 0.03f;
    }
    return 0.30f;
}

inline float deterministic01(int x, int y, int salt)
{
    std::uint32_t n = static_cast<std::uint32_t>(x * 73856093) ^
                      static_cast<std::uint32_t>(y * 19349663) ^
                      static_cast<std::uint32_t>(salt * 83492791);
    n ^= (n >> 13);
    n *= 1274126177u;
    n ^= (n >> 16);
    return static_cast<float>(n & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
}

inline SaveGameMode saveGameModeFromIndex(int index)
{
    return index == 0 ? SaveGameMode::Campaign : SaveGameMode::Endless;
}

inline int indexFromSaveGameMode(SaveGameMode mode)
{
    return mode == SaveGameMode::Campaign ? 0 : 1;
}

inline SaveThreatMode saveThreatModeFromIndex(int index)
{
    switch (index)
    {
    case 0: return SaveThreatMode::OverwhelmingSwarm;
    case 1: return SaveThreatMode::Stalker;
    case 2: return SaveThreatMode::MutatingThreat;
    case 3: return SaveThreatMode::InfectedSociety;
    case 4: return SaveThreatMode::Unknown;
    case 5: return SaveThreatMode::Hybrid;
    default: return SaveThreatMode::OverwhelmingSwarm;
    }
}

inline int indexFromSaveThreatMode(SaveThreatMode mode)
{
    switch (mode)
    {
    case SaveThreatMode::OverwhelmingSwarm: return 0;
    case SaveThreatMode::Stalker: return 1;
    case SaveThreatMode::MutatingThreat: return 2;
    case SaveThreatMode::InfectedSociety: return 3;
    case SaveThreatMode::Unknown: return 4;
    case SaveThreatMode::Hybrid: return 5;
    default: return 0;
    }
}

inline SaveTonePreset saveTonePresetFromIndex(int index)
{
    switch (index)
    {
    case 0: return SaveTonePreset::Hopeless;
    case 2: return SaveTonePreset::BrutalArcade;
    case 1:
    default:
        return SaveTonePreset::GrimHuman;
    }
}

inline int indexFromSaveTonePreset(SaveTonePreset preset)
{
    switch (preset)
    {
    case SaveTonePreset::Hopeless: return 0;
    case SaveTonePreset::GrimHuman: return 1;
    case SaveTonePreset::BrutalArcade: return 2;
    default: return 1;
    }
}

struct ThreatBehaviorTuning
{
    float speedScale = 1.0f;
    float visionScale = 1.0f;
};

inline ThreatDirectorSystem::Tuning threatDirectorTuning(SaveThreatMode mode)
{
    ThreatDirectorSystem::Tuning tuning{};
    switch (mode)
    {
    case SaveThreatMode::OverwhelmingSwarm: tuning.migrationScale = 1.3f; tuning.populationScale = 1.45f; break;
    case SaveThreatMode::Stalker:           tuning.migrationScale = 1.6f; tuning.populationScale = 0.7f;  break;
    case SaveThreatMode::MutatingThreat:    tuning.migrationScale = 1.0f; tuning.populationScale = 1.15f; break;
    case SaveThreatMode::InfectedSociety:   tuning.migrationScale = 0.8f; tuning.populationScale = 0.9f;  break;
    case SaveThreatMode::Unknown:           tuning.migrationScale = 1.1f; tuning.populationScale = 1.1f;  break;
    case SaveThreatMode::Hybrid:            tuning.migrationScale = 1.25f; tuning.populationScale = 1.3f; break;
    }
    return tuning;
}

inline ThreatBehaviorTuning computeThreatBehaviorTuning(
    SaveThreatMode mode,
    int currentDay,
    Entity zombieEntity,
    std::uint64_t runSeed)
{
    ThreatBehaviorTuning tuning{};

    switch (mode)
    {
    case SaveThreatMode::OverwhelmingSwarm:
        tuning.speedScale = 0.94f;
        tuning.visionScale = 0.96f;
        break;

    case SaveThreatMode::Stalker:
        tuning.speedScale = 1.18f;
        tuning.visionScale = 1.30f;
        break;

    case SaveThreatMode::MutatingThreat:
    {
        const float dayFactor = std::clamp((static_cast<float>(currentDay) - 1.0f) / 30.0f, 0.0f, 1.0f);
        tuning.speedScale = 1.0f + dayFactor * 0.42f;
        tuning.visionScale = 1.0f + dayFactor * 0.24f;
        break;
    }

    case SaveThreatMode::InfectedSociety:
        tuning.speedScale = 0.82f;
        tuning.visionScale = 0.84f;
        break;

    case SaveThreatMode::Unknown:
    {
        const int seedA = static_cast<int>(runSeed & 0x7FFFFFFFu);
        const int seedB = static_cast<int>((runSeed >> 17) & 0x7FFFFFFFu);
        const float jitterA = deterministic01(static_cast<int>(zombieEntity), currentDay, seedA);
        const float jitterB = deterministic01(currentDay, static_cast<int>(zombieEntity), seedB);
        tuning.speedScale = 0.74f + jitterA * 0.78f;
        tuning.visionScale = 0.78f + jitterB * 0.56f;
        break;
    }

    case SaveThreatMode::Hybrid:
    {
        const float dayFactor = std::clamp((static_cast<float>(currentDay) - 1.0f) / 40.0f, 0.0f, 1.0f);
        tuning.speedScale = 1.05f + dayFactor * 0.28f;
        tuning.visionScale = 1.04f + dayFactor * 0.20f;
        break;
    }
    }

    return tuning;
}

// Inventory grid geometry shared by input hit-testing and both render passes.
// Slots 1-8 (the first two rows) are the hotbar.
struct InventoryLayout
{
    glm::vec2 panelPos{0.0f};
    glm::vec2 panelSize{0.0f};
    glm::vec2 gridPos{0.0f};
    float titleY = 0.0f;
    float weightY = 0.0f;
    float detailY = 0.0f;
    float footerY = 0.0f;
    static constexpr int kColumns = 4;
    static constexpr int kRows = 5;
    static constexpr float kSlot = 48.0f;
    static constexpr float kGap = 6.0f;

    [[nodiscard]] glm::vec2 slotPos(int index) const
    {
        const int column = index % kColumns;
        const int row = index / kColumns;
        // A little extra space separates the hotbar rows from the backpack.
        const float hotbarGap = row >= 2 ? 10.0f : 0.0f;
        return gridPos + glm::vec2(static_cast<float>(column) * (kSlot + kGap),
                                   static_cast<float>(row) * (kSlot + kGap) + hotbarGap);
    }

    [[nodiscard]] int slotAt(const glm::vec2& point) const
    {
        for (int i = 0; i < kColumns * kRows; ++i)
        {
            const glm::vec2 pos = slotPos(i);
            if (point.x >= pos.x && point.x < pos.x + kSlot && point.y >= pos.y && point.y < pos.y + kSlot)
            {
                return i;
            }
        }
        return -1;
    }
};

inline InventoryLayout computeInventoryLayout(int windowWidth, int windowHeight)
{
    InventoryLayout layout;
    const float gridW = InventoryLayout::kColumns * InventoryLayout::kSlot + (InventoryLayout::kColumns - 1) * InventoryLayout::kGap;
    const float gridH = InventoryLayout::kRows * InventoryLayout::kSlot + (InventoryLayout::kRows - 1) * InventoryLayout::kGap + 10.0f;
    layout.panelSize = glm::vec2(340.0f, 20.0f + 26.0f + 22.0f + 12.0f + gridH + 14.0f + 44.0f + 18.0f + 12.0f);
    layout.panelPos = glm::vec2(
        std::round((static_cast<float>(windowWidth) - layout.panelSize.x) * 0.5f),
        std::round(std::max(96.0f, (static_cast<float>(windowHeight) - layout.panelSize.y) * 0.5f - 40.0f)));
    layout.titleY = layout.panelPos.y + 20.0f;
    layout.weightY = layout.titleY + 26.0f;
    layout.gridPos = glm::vec2(std::round(layout.panelPos.x + (layout.panelSize.x - gridW) * 0.5f),
                               layout.weightY + 22.0f + 12.0f);
    layout.detailY = layout.gridPos.y + gridH + 14.0f;
    layout.footerY = layout.detailY + 44.0f;
    return layout;
}

// One-line summary of what an item does, for the inventory details.
inline std::string itemEffectSummary(const ItemDef& item)
{
    std::string summary;
    auto add = [&](const std::string& part)
    {
        if (!summary.empty()) summary += ", ";
        summary += part;
    };
    if (item.hungerRestore > 0.0f) add("+" + std::to_string(static_cast<int>(item.hungerRestore)) + " food");
    if (item.thirstRestore > 0.0f) add("+" + std::to_string(static_cast<int>(item.thirstRestore)) + " water");
    if (item.healthRestore > 0.0f) add("+" + std::to_string(static_cast<int>(item.healthRestore)) + " health");
    if (item.stopsBleeding) add("stops bleeding");
    if (item.woundTreatment > 0) add("treats wounds");
    if (item.curesInfection) add("fights infection");
    if (item.illnessRelief > 0.0f) add("eases illness");
    if (item.panicRelief > 0.0f) add("calms nerves");
    if (item.weaponCategory >= 0) add("weapon");
    return summary;
}

struct ControlBinding
{
    const char* key;
    const char* action;
};

constexpr std::array<ControlBinding, 24> kControlBindings = {{
    {"WASD", "Move"},
    {"Shift", "Run"},
    {"C", "Crouch"},
    {"Mouse", "Aim"},
    {"LClick", "Attack"},
    {"RClick", "Grab"},
    {"E", "Interact"},
    {"F", "Use item"},
    {"Q", "Flashlight"},
    {"1-8", "Hotbar slot"},
    {"Tab", "Inventory"},
    {"Arrows", "Inventory cursor"},
    {"Enter", "Swap to hotbar"},
    {"N", "Crafting"},
    {"B", "Build"},
    {"L", "Fire action"},
    {"V", "Enter/exit car"},
    {"Shift+V", "Refuel car"},
    {"M", "World map"},
    {"J", "Notebook"},
    {"+/-", "Minimap zoom"},
    {"Esc", "Pause"},
    {"F2", "Controls"},
    {"F3", "Infection debug"},
}};

struct ControlsLayout
{
    glm::vec2 panelPos{0.0f};
    glm::vec2 panelSize{0.0f};
    float rowsY = 0.0f;
    float columnWidth = 0.0f;
    static constexpr float kRowH = 22.0f;
    static constexpr float kKeyColumn = 108.0f;
    static constexpr std::size_t kRowsPerColumn = 12;
};

inline ControlsLayout computeControlsLayout(int windowWidth, int windowHeight)
{
    ControlsLayout layout;
    layout.columnWidth = 320.0f;
    layout.panelSize = glm::vec2(layout.columnWidth * 2.0f + 48.0f,
                                 64.0f + ControlsLayout::kRowH * static_cast<float>(ControlsLayout::kRowsPerColumn) + 20.0f);
    layout.panelPos = glm::vec2(
        std::round((static_cast<float>(windowWidth) - layout.panelSize.x) * 0.5f),
        std::round((static_cast<float>(windowHeight) - layout.panelSize.y) * 0.5f));
    layout.rowsY = layout.panelPos.y + 64.0f;
    return layout;
}

// Pause menu geometry shared by the panel pass and the text pass.
struct PauseLayout
{
    glm::vec2 panelPos{0.0f};
    glm::vec2 panelSize{0.0f};
    float titleY = 0.0f;
    float nameY = 0.0f;
    float runLineY = 0.0f;
    float statsLineY = 0.0f;
    float traitsLineY = 0.0f;
    float optionsY = 0.0f;
    static constexpr float kOptionH = 30.0f;
    static constexpr int kOptionCount = 5;
};

inline PauseLayout computePauseLayout(int windowWidth, int windowHeight)
{
    PauseLayout layout;
    layout.panelSize = glm::vec2(440.0f, 312.0f);
    layout.panelPos = glm::vec2(
        std::round((static_cast<float>(windowWidth) - layout.panelSize.x) * 0.5f),
        std::round((static_cast<float>(windowHeight) - layout.panelSize.y) * 0.5f));
    layout.titleY = layout.panelPos.y + 18.0f;
    layout.nameY = layout.panelPos.y + 60.0f;
    layout.runLineY = layout.panelPos.y + 86.0f;
    layout.statsLineY = layout.panelPos.y + 100.0f;
    layout.traitsLineY = layout.panelPos.y + 114.0f;
    layout.optionsY = layout.panelPos.y + 140.0f;
    return layout;
}

struct TitlePanelLayout
{
    float panelX = 0.0f;
    float panelY = 0.0f;
    float panelW = 0.0f;
    float panelH = 0.0f;

    float leftColX = 0.0f;
    float leftColY = 0.0f;
    float leftColW = 0.0f;
    float leftColH = 0.0f;

    float rightColX = 0.0f;
    float rightColY = 0.0f;
    float rightColW = 0.0f;
    float rightColH = 0.0f;

    float menuRowX = 0.0f;
    float menuRowY = 0.0f;
    float menuRowW = 0.0f;
    float menuRowH = 22.0f;
    float menuRowStep = 24.0f;

    float rightCardTopX = 0.0f;
    float rightCardTopY = 0.0f;
    float rightCardTopW = 0.0f;
    float rightCardTopH = 170.0f;

    float rightCardMidX = 0.0f;
    float rightCardMidY = 0.0f;
    float rightCardMidW = 0.0f;
    float rightCardMidH = 86.0f;

    float rightCardBottomX = 0.0f;
    float rightCardBottomY = 0.0f;
    float rightCardBottomW = 0.0f;
    float rightCardBottomH = 0.0f;

    float leftTextX = 0.0f;
    float rightTextX = 0.0f;
};

inline TitlePanelLayout computeTitlePanelLayout(int windowWidth, int windowHeight)
{
    TitlePanelLayout layout;
    const float w = static_cast<float>(windowWidth);
    const float h = static_cast<float>(windowHeight);

    layout.panelW = std::floor(std::max(820.0f, std::min(1060.0f, w - 32.0f)));
    layout.panelH = std::floor(std::max(520.0f, std::min(610.0f, h - 36.0f)));
    layout.panelX = std::floor((w - layout.panelW) * 0.5f);
    layout.panelY = std::floor((h - layout.panelH) * 0.5f);

    constexpr float kOuterPadding = 14.0f;
    constexpr float kColumnTopInset = 70.0f;
    constexpr float kColumnBottomInset = 16.0f;
    constexpr float kColumnGap = 14.0f;

    const float contentW = layout.panelW - kOuterPadding * 2.0f;
    const float contentH = layout.panelH - kColumnTopInset - kColumnBottomInset;

    layout.leftColX = layout.panelX + kOuterPadding;
    layout.leftColY = layout.panelY + kColumnTopInset;
    layout.leftColW = std::floor(contentW * 0.52f);
    layout.leftColH = contentH;

    layout.rightColX = std::floor(layout.leftColX + layout.leftColW + kColumnGap);
    layout.rightColY = layout.leftColY;
    layout.rightColW = std::floor(contentW - layout.leftColW - kColumnGap);
    layout.rightColH = contentH;

    layout.menuRowX = layout.leftColX + 8.0f;
    layout.menuRowY = layout.panelY + 108.0f;
    layout.menuRowW = layout.leftColW - 16.0f;

    layout.rightCardTopX = layout.rightColX + 8.0f;
    layout.rightCardTopY = layout.rightColY + 8.0f;
    layout.rightCardTopW = layout.rightColW - 16.0f;
    layout.rightCardTopH = std::min(200.0f, layout.rightColH * 0.38f);

    layout.rightCardMidX = layout.rightColX + 8.0f;
    layout.rightCardMidY = layout.rightCardTopY + layout.rightCardTopH + 8.0f;
    layout.rightCardMidW = layout.rightColW - 16.0f;
    layout.rightCardMidH = std::min(106.0f, layout.rightColH * 0.18f);

    layout.rightCardBottomX = layout.rightColX + 8.0f;
    layout.rightCardBottomY = layout.rightCardMidY + layout.rightCardMidH + 8.0f;
    layout.rightCardBottomW = layout.rightColW - 16.0f;
    layout.rightCardBottomH = std::max(96.0f, layout.rightColH - (layout.rightCardBottomY - layout.rightColY) - 8.0f);

    layout.leftTextX = layout.menuRowX + 8.0f;
    layout.rightTextX = layout.rightCardTopX + 10.0f;

    return layout;
}

inline std::size_t approximateCharBudget(float widthPixels, float glyphWidthPixels = 8.0f)
{
    if (glyphWidthPixels <= 0.0f)
    {
        return 12;
    }
    return static_cast<std::size_t>(std::max(12.0f, std::floor(widthPixels / glyphWidthPixels)));
}

enum class LegacyEventKind
{
    None,
    Death,
    Retirement
};

struct LatestLegacyEvent
{
    LegacyEventKind kind = LegacyEventKind::None;
    const GraveRecord* death = nullptr;
    const RetirementRecord* retirement = nullptr;
};

inline LatestLegacyEvent pickLatestLegacyEvent(const std::vector<GraveRecord>& graves, const std::vector<RetirementRecord>& retirements)
{
    LatestLegacyEvent output;

    const GraveRecord* lastDeath = graves.empty() ? nullptr : &graves.back();
    const RetirementRecord* lastRetirement = retirements.empty() ? nullptr : &retirements.back();

    if (lastDeath == nullptr && lastRetirement == nullptr)
    {
        return output;
    }
    if (lastDeath != nullptr && lastRetirement == nullptr)
    {
        output.kind = LegacyEventKind::Death;
        output.death = lastDeath;
        return output;
    }
    if (lastDeath == nullptr)
    {
        output.kind = LegacyEventKind::Retirement;
        output.retirement = lastRetirement;
        return output;
    }

    if (lastDeath->runIndex != lastRetirement->runIndex)
    {
        if (lastDeath->runIndex > lastRetirement->runIndex)
        {
            output.kind = LegacyEventKind::Death;
            output.death = lastDeath;
        }
        else
        {
            output.kind = LegacyEventKind::Retirement;
            output.retirement = lastRetirement;
        }
        return output;
    }

    if (lastDeath->day >= lastRetirement->day)
    {
        output.kind = LegacyEventKind::Death;
        output.death = lastDeath;
    }
    else
    {
        output.kind = LegacyEventKind::Retirement;
        output.retirement = lastRetirement;
    }
    return output;
}

inline std::string formatTileLocation(float worldX, float worldY, const TileMap& tileMap)
{
    const int tileW = std::max(1, tileMap.tileWidth());
    const int tileH = std::max(1, tileMap.tileHeight());
    const int tileX = static_cast<int>(std::floor(worldX / static_cast<float>(tileW)));
    const int tileY = static_cast<int>(std::floor(worldY / static_cast<float>(tileH)));

    std::ostringstream out;
    out << "T(" << tileX << "," << tileY << ")";
    return out.str();
}

inline std::filesystem::path getExecutableDirectory()
{
    char* basePath = SDL_GetBasePath();
    if (basePath == nullptr)
    {
        return std::filesystem::current_path();
    }

    std::filesystem::path output(basePath);
    SDL_free(basePath);
    return output;
}

inline glm::mat4 buildProjection(int width, int height)
{
    return glm::ortho(
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f,
        -1.0f,
        1.0f);
}

inline int surfaceNoiseModifier(SurfaceType surface)
{
    switch (surface)
    {
    case SurfaceType::Carpet:
    case SurfaceType::Grass:
        return -2;
    case SurfaceType::Gravel:
    case SurfaceType::Metal:
        return 1;
    case SurfaceType::Default:
    default:
        return 0;
    }
}

inline float tierDurationSeconds(NoiseTier tier)
{
    switch (tier)
    {
    case NoiseTier::Whisper:
        return 0.35f;
    case NoiseTier::Soft:
        return 0.45f;
    case NoiseTier::Medium:
        return 0.55f;
    case NoiseTier::Loud:
        return 0.70f;
    case NoiseTier::Explosive:
        return 0.90f;
    case NoiseTier::None:
    default:
        return 0.0f;
    }
}

inline void advanceGameClock(double hours, double& gameHours, int& currentDay)
{
    gameHours += hours;
    while (gameHours >= 24.0)
    {
        gameHours -= 24.0;
        ++currentDay;
    }
}

inline bool hasInfectedWound(const Wounds& wounds)
{
    return std::any_of(
        wounds.active.begin(),
        wounds.active.end(),
        [](const Wound& wound)
        {
            return wound.infected;
        });
}

inline int treatWorstWounds(Wounds& wounds, int treatCount)
{
    int treated = 0;
    while (treated < treatCount && !wounds.active.empty())
    {
        int bestIdx = 0;
        for (int i = 1; i < static_cast<int>(wounds.active.size()); ++i)
        {
            const Wound& current = wounds.active[i];
            const Wound& best = wounds.active[bestIdx];

            if (current.type == WoundType::Bite && best.type != WoundType::Bite)
            {
                bestIdx = i;
            }
            else if (current.infected && !best.infected)
            {
                bestIdx = i;
            }
            else if (current.severity > best.severity)
            {
                bestIdx = i;
            }
        }

        wounds.active.erase(wounds.active.begin() + bestIdx);
        ++treated;
    }

    return treated;
}

inline void syncHealthWithWounds(Health& health, const Wounds& wounds)
{
    constexpr float kMaxHpPenaltyPerWound = 5.0f;
    const float penalty = static_cast<float>(wounds.active.size()) * kMaxHpPenaltyPerWound;
    health.maximum = std::max(20.0f, 100.0f - penalty);
    health.current = std::min(health.current, health.maximum);
}

inline std::string generateCharacterName(std::uint32_t seed)
{
    static constexpr std::array<const char*, 12> firstNames = {
        "Alex", "Sam", "Jordan", "Casey", "Taylor", "Riley",
        "Morgan", "Quinn", "Avery", "Blake", "Hayden", "Parker"};
    static constexpr std::array<const char*, 12> lastNames = {
        "Mason", "Reed", "Cross", "Hale", "Brooks", "Voss",
        "Turner", "Sloan", "Graves", "Frost", "Mercer", "Stone"};

    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::size_t> firstDist(0, firstNames.size() - 1);
    std::uniform_int_distribution<std::size_t> lastDist(0, lastNames.size() - 1);

    return std::string(firstNames[firstDist(rng)]) + " " + std::string(lastNames[lastDist(rng)]);
}

inline void hashValue(std::uint64_t& hash, std::uint64_t value)
{
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    hash ^= value;
    hash *= kFnvPrime;
}

inline void hashIntVector(std::uint64_t& hash, const std::vector<int>& values)
{
    for (int value : values)
    {
        hashValue(hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)));
    }
}

inline std::uint64_t computeLayoutFingerprint(const LayoutData& layout)
{
    std::uint64_t hash = 1469598103934665603ull;

    hashValue(hash, static_cast<std::uint64_t>(layout.width));
    hashValue(hash, static_cast<std::uint64_t>(layout.height));
    hashValue(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(layout.playerStart.x)));
    hashValue(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(layout.playerStart.y)));

    hashIntVector(hash, layout.ground);
    hashIntVector(hash, layout.collision);
    hashIntVector(hash, layout.surface);
    hashIntVector(hash, layout.district);

    for (const auto& building : layout.buildings)
    {
        hashValue(hash, static_cast<std::uint64_t>(building.x));
        hashValue(hash, static_cast<std::uint64_t>(building.y));
        hashValue(hash, static_cast<std::uint64_t>(building.w));
        hashValue(hash, static_cast<std::uint64_t>(building.h));
    }

    for (const auto& door : layout.doors)
    {
        hashValue(hash, static_cast<std::uint64_t>(door.x));
        hashValue(hash, static_cast<std::uint64_t>(door.y));
    }

    for (const auto& district : layout.districts)
    {
        hashValue(hash, static_cast<std::uint64_t>(district.id));
        hashValue(hash, static_cast<std::uint64_t>(district.type));
        hashValue(hash, static_cast<std::uint64_t>(district.minX));
        hashValue(hash, static_cast<std::uint64_t>(district.minY));
        hashValue(hash, static_cast<std::uint64_t>(district.maxX));
        hashValue(hash, static_cast<std::uint64_t>(district.maxY));
        for (int idx : district.buildingIndices)
        {
            hashValue(hash, static_cast<std::uint64_t>(idx));
        }
        for (int idx : district.adjacentDistricts)
        {
            hashValue(hash, static_cast<std::uint64_t>(idx));
        }
    }

    return hash;
}

inline const char* districtTypeLabel(int districtType)
{
    switch (static_cast<DistrictType>(districtType))
    {
    case DistrictType::Residential:
        return "Residential";
    case DistrictType::Commercial:
        return "Commercial";
    case DistrictType::Industrial:
        return "Industrial";
    case DistrictType::Wilderness:
    default:
        return "Wilderness";
    }
}

inline const char* weatherConditionLabel(WeatherCondition condition)
{
    switch (condition)
    {
    case WeatherCondition::Clear: return "Clear";
    case WeatherCondition::Cloudy: return "Cloudy";
    case WeatherCondition::Rain: return "Rain";
    case WeatherCondition::HeavyRain: return "Heavy rain";
    case WeatherCondition::Thunderstorm: return "Storm";
    case WeatherCondition::Fog: return "Fog";
    case WeatherCondition::Blizzard: return "Blizzard";
    case WeatherCondition::Heatwave: return "Heatwave";
    }
    return "Clear";
}

inline const char* craftingCategoryLabel(int category)
{
    switch (category)
    {
    case 1: return "TOOLS";
    case 2: return "MED";
    default: return "ALL";
    }
}

inline bool recipeMatchesCategory(const CraftingRecipe& recipe, int category)
{
    if (category == 0)
    {
        return true;
    }

    const char* label = craftingCategoryLabel(category);
    return recipe.category == label;
}

inline std::vector<std::size_t> buildVisibleRecipeList(
    const CraftingSystem& craftingSystem,
    const SaveManager& saveManager,
    const Inventory& inventory,
    const std::vector<std::pair<int, int>>& sessionDiscovered,
    int category)
{
    std::vector<std::size_t> visible;
    const auto& recipes = craftingSystem.recipes();
    visible.reserve(recipes.size());

    for (std::size_t i = 0; i < recipes.size(); ++i)
    {
        const CraftingRecipe& recipe = recipes[i];
        if (!recipeMatchesCategory(recipe, category))
        {
            continue;
        }

        bool discovered = recipe.startsKnown || saveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB);
        if (!discovered)
        {
            const auto normalized = CraftingSystem::normalizePair(recipe.inputA, recipe.inputB);
            for (const auto& pair : sessionDiscovered)
            {
                if (pair.first == normalized.first && pair.second == normalized.second)
                {
                    discovered = true;
                    break;
                }
            }
        }
        if (discovered)
        {
            visible.push_back(i);
            continue;
        }

        const bool hasA = InventorySystem::countItem(inventory, recipe.inputA) > 0;
        const bool hasB = InventorySystem::countItem(inventory, recipe.inputB) > 0;
        if (hasA || hasB)
        {
            visible.push_back(i);
        }
    }

    return visible;
}

inline bool hasNearbyWorkbench(World& world, Entity playerEntity)
{
    if (playerEntity == kInvalidEntity || !world.hasComponent<Transform>(playerEntity))
    {
        return false;
    }

    const Transform& playerTransform = world.getComponent<Transform>(playerEntity);
    const glm::vec2 playerCenter(playerTransform.x + kSpriteSize * 0.5f, playerTransform.y + kSpriteSize * 0.5f);
    constexpr float kWorkbenchRange = 96.0f;

    bool found = false;
    world.forEach<Structure, Transform>(
        [&](Entity, Structure& structure, Transform& st)
        {
            if (found) return;
            if (structure.type != StructureType::SupplyCache) return;
            const glm::vec2 center(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);
            if (glm::distance(playerCenter, center) <= kWorkbenchRange)
            {
                found = true;
            }
        });

    return found;
}

inline bool hasCraftTools(const Inventory& inv, const CraftingRecipe& recipe)
{
    for (int toolId : recipe.requiredTools)
    {
        if (InventorySystem::countItem(inv, toolId) <= 0)
        {
            return false;
        }
    }
    return true;
}

inline bool hasCraftIngredients(const Inventory& inv, const CraftingRecipe& recipe, int count)
{
    if (count <= 0)
    {
        return true;
    }

    const int needA = count;
    const int needB = count;
    return InventorySystem::countItem(inv, recipe.inputA) >= needA &&
           InventorySystem::countItem(inv, recipe.inputB) >= needB;
}
} // namespace gameshared
