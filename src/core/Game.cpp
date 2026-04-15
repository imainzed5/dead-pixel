#include "core/Game.h"

#include <glm/gtc/matrix_transform.hpp>

#include "components/Animation.h"
#include "components/Bleeding.h"
#include "components/Combat.h"
#include "components/Facing.h"
#include "components/Health.h"
#include "components/Interactable.h"
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
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "components/SurvivorAI.h"

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

namespace
{
constexpr int kDefaultWindowWidth = 1280;
constexpr int kDefaultWindowHeight = 720;
constexpr float kSpriteSize = 32.0f;
constexpr float kPlayerFootOffsetY = 22.0f;
constexpr float kMaxFrameStepSeconds = 0.25f;
constexpr double kFixedTimeStepSeconds = 1.0 / 60.0;
constexpr double kGameHoursPerRealSecond = 1.0 / 60.0;
constexpr double kDeathRestartDelaySeconds = 3.0;

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

constexpr std::array<const char*, 3> kTitleModeNames = {
    "SURVIVAL",
    "SCAVENGER",
    "OVERRUN"
};

constexpr std::array<const char*, 3> kTitleModeDescriptions = {
    "Balanced pressure with standard supplies",
    "Lower pressure and extra resources",
    "High pressure and reduced resources"
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

constexpr std::array<const char*, 7> kTitleMenuItems = {
    "Continue Run",
    "Start New Run",
    "Save Slot",
    "Mode",
    "Options",
    "Credits",
    "Quit"
};

int wrapIndex(int value, int count)
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

std::string truncateHudText(const std::string& text, std::size_t maxChars)
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

TitlePanelLayout computeTitlePanelLayout(int windowWidth, int windowHeight)
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

std::size_t approximateCharBudget(float widthPixels, float glyphWidthPixels = 8.0f)
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

LatestLegacyEvent pickLatestLegacyEvent(const std::vector<GraveRecord>& graves, const std::vector<RetirementRecord>& retirements)
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

std::string formatTileLocation(float worldX, float worldY, const TileMap& tileMap)
{
    const int tileW = std::max(1, tileMap.tileWidth());
    const int tileH = std::max(1, tileMap.tileHeight());
    const int tileX = static_cast<int>(std::floor(worldX / static_cast<float>(tileW)));
    const int tileY = static_cast<int>(std::floor(worldY / static_cast<float>(tileH)));

    std::ostringstream out;
    out << "T(" << tileX << "," << tileY << ")";
    return out.str();
}

std::filesystem::path getExecutableDirectory()
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

glm::mat4 buildProjection(int width, int height)
{
    return glm::ortho(
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f,
        -1.0f,
        1.0f);
}

int surfaceNoiseModifier(SurfaceType surface)
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

float tierDurationSeconds(NoiseTier tier)
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

void advanceGameClock(double hours, double& gameHours, int& currentDay)
{
    gameHours += hours;
    while (gameHours >= 24.0)
    {
        gameHours -= 24.0;
        ++currentDay;
    }
}

bool hasInfectedWound(const Wounds& wounds)
{
    return std::any_of(
        wounds.active.begin(),
        wounds.active.end(),
        [](const Wound& wound)
        {
            return wound.infected;
        });
}

int treatWorstWounds(Wounds& wounds, int treatCount)
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

void syncHealthWithWounds(Health& health, const Wounds& wounds)
{
    constexpr float kMaxHpPenaltyPerWound = 5.0f;
    const float penalty = static_cast<float>(wounds.active.size()) * kMaxHpPenaltyPerWound;
    health.maximum = std::max(20.0f, 100.0f - penalty);
    health.current = std::min(health.current, health.maximum);
}

std::string generateCharacterName(std::uint32_t seed)
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

void hashValue(std::uint64_t& hash, std::uint64_t value)
{
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    hash ^= value;
    hash *= kFnvPrime;
}

void hashIntVector(std::uint64_t& hash, const std::vector<int>& values)
{
    for (int value : values)
    {
        hashValue(hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)));
    }
}

std::uint64_t computeLayoutFingerprint(const LayoutData& layout)
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

const char* districtTypeLabel(int districtType)
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

const char* craftingCategoryLabel(int category)
{
    switch (category)
    {
    case 1: return "TOOLS";
    case 2: return "MED";
    default: return "ALL";
    }
}

bool recipeMatchesCategory(const CraftingRecipe& recipe, int category)
{
    if (category == 0)
    {
        return true;
    }

    const char* label = craftingCategoryLabel(category);
    return recipe.category == label;
}

std::vector<std::size_t> buildVisibleRecipeList(
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

bool hasNearbyWorkbench(World& world, Entity playerEntity)
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

bool hasCraftTools(const Inventory& inv, const CraftingRecipe& recipe)
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

bool hasCraftIngredients(const Inventory& inv, const CraftingRecipe& recipe, int count)
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
}

Game::~Game()
{
    shutdown();
}

int Game::run()
{
    if (!initialize())
    {
        return 1;
    }

    using clock = std::chrono::steady_clock;
    auto previousTime = clock::now();
    double accumulator = 0.0;

    while (mIsRunning)
    {
        const auto currentTime = clock::now();
        double frameSeconds = std::chrono::duration<double>(currentTime - previousTime).count();
        previousTime = currentTime;

        if (frameSeconds > kMaxFrameStepSeconds)
        {
            frameSeconds = kMaxFrameStepSeconds;
        }

        accumulator += frameSeconds;
        processInputEvents();

        while (accumulator >= kFixedTimeStepSeconds)
        {
            snapshotPositions();
            update(static_cast<float>(kFixedTimeStepSeconds));
            accumulator -= kFixedTimeStepSeconds;
        }

        mInterpolationAlpha = static_cast<float>(accumulator / kFixedTimeStepSeconds);
        render();
        updateWindowTitle(frameSeconds);
        SDL_GL_SwapWindow(mWindow);
    }

    shutdown();
    return 0;
}

bool Game::initialize()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    mWindowWidth = kDefaultWindowWidth;
    mWindowHeight = kDefaultWindowHeight;

    mWindow = SDL_CreateWindow(
        "Dead Pixel Survival",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        mWindowWidth,
        mWindowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    if (mWindow == nullptr)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    mGlContext = SDL_GL_CreateContext(mWindow);
    if (mGlContext == nullptr)
    {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    if (SDL_GL_MakeCurrent(mWindow, mGlContext) != 0)
    {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    const GLenum glewResult = glewInit();
    glGetError();
    if (glewResult != GLEW_OK)
    {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewResult) << '\n';
        shutdown();
        return false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const std::filesystem::path assetsRoot = getExecutableDirectory() / "assets";

    mTitleSaveSlotIndex = 0;
    mActiveSaveSlot = kTitleSaveSlots[static_cast<std::size_t>(mTitleSaveSlotIndex)];
    if (!loadSlotAndWorld(mActiveSaveSlot))
    {
        std::cerr << "Failed to initialize save slot and world data." << '\n';
        shutdown();
        return false;
    }

    const std::string vertexShaderPath = (assetsRoot / "shaders" / "sprite.vert").string();
    const std::string fragmentShaderPath = (assetsRoot / "shaders" / "sprite.frag").string();

    const std::filesystem::path texturePath =
        (assetsRoot / "maps" / mTileMap.tileset().imagePath).lexically_normal();

    if (!mShader.loadFromFiles(vertexShaderPath, fragmentShaderPath))
    {
        shutdown();
        return false;
    }

    if (!mTexture.loadFromFile(texturePath.string()))
    {
        shutdown();
        return false;
    }

    if (!mSpriteBatch.initialize())
    {
        shutdown();
        return false;
    }

    const std::filesystem::path fontPng = assetsRoot / "fonts" / "pixel_font.png";
    const std::filesystem::path fontJson = assetsRoot / "fonts" / "pixel_font.json";
    if (!mFont.loadFromFiles(fontPng.string(), fontJson.string()))
    {
        std::cerr << "Failed to load bitmap font.\n";
        shutdown();
        return false;
    }

    const std::filesystem::path sheetPng = assetsRoot / "sprites" / "spritesheet.png";
    const std::filesystem::path sheetJson = assetsRoot / "sprites" / "spritesheet.json";
    if (!mSpriteSheet.loadFromFiles(sheetPng.string(), sheetJson.string()))
    {
        std::cerr << "Failed to load sprite sheet.\n";
        shutdown();
        return false;
    }

    if (!mAudioManager.initialize(assetsRoot.string()))
    {
        std::cerr << "Warning: Audio init failed, continuing without sound.\n";
    }
    mAudioManager.playAmbient(AmbientId::Wind, 0.15f);

    const std::filesystem::path itemsJson = assetsRoot / "data" / "items.json";
    if (!mItemDatabase.loadFromFile(itemsJson.string()))
    {
        std::cerr << "Failed to load item database.\n";
        shutdown();
        return false;
    }

    const std::filesystem::path recipesJson = assetsRoot / "data" / "recipes.json";
    if (!mCraftingSystem.loadRecipesFromFile(recipesJson.string()))
    {
        std::cerr << "Failed to load crafting recipes.\n";
        shutdown();
        return false;
    }

    mProjection = buildProjection(mWindowWidth, mWindowHeight);
    mCamera.setViewportSize(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight));
    mCamera.clampToBounds(mTileMap.pixelSize());
    // Boot to title screen — do not start a run yet
    mRunState = RunState::TitleScreen;
    mTitleMenuSelection = 0;
    mTitleModeIndex = static_cast<int>(TitleRunMode::Survival);
    mTitleHasCheckpoint = false;
    mTitleCheckpointDay = 1;
    mTitleCheckpointName.clear();
    mTitleCheckpointX = 0.0f;
    mTitleCheckpointY = 0.0f;
    mShowTitleCredits = false;
    mTitleStatusMessage.clear();
    mTitleStatusTimer = 0.0f;
    mTitlePulseTimer = 0.0f;
    mIgnoreCheckpointRestoreOnce = false;

    mIsRunning = true;
    mShowNoiseDebug = false;
    mShowDistrictDebug = false;
    return true;
}

bool Game::loadSlotAndWorld(const std::string& slotName)
{
    if (!mSaveManager.loadOrCreateSlot(slotName))
    {
        return false;
    }

    if (mSaveManager.hasWorldLayout())
    {
        LayoutData layout = mSaveManager.loadWorldLayout();
        if (layout.width > 0 && layout.height > 0)
        {
            mMapGenerator.loadLayout(layout);
        }
        else
        {
            LayoutData regenerated = mMapGenerator.generateLayout(mSaveManager.worldSeed());
            mSaveManager.saveWorldLayout(regenerated);
        }
    }
    else
    {
        LayoutData layout = mMapGenerator.generateLayout(mSaveManager.worldSeed());
        mSaveManager.saveWorldLayout(layout);
    }

    mMapGenerator.applyToTileMap(mTileMap);
    mLayoutFingerprint = computeLayoutFingerprint(mMapGenerator.layoutData());
    mActiveSaveSlot = slotName;
    mCamera.clampToBounds(mTileMap.pixelSize());

    std::cout << "[World] slot=" << slotName
              << " worldSeed=" << mSaveManager.worldSeed()
              << " runSeed=" << mSaveManager.currentRunSeed()
              << " layoutFingerprint=0x" << std::hex << mLayoutFingerprint << std::dec
              << '\n';

    return true;
}

void Game::registerComponents()
{
    mWorld.registerComponent<Transform>();
    mWorld.registerComponent<Sprite>();
    mWorld.registerComponent<Velocity>();
    mWorld.registerComponent<PlayerInput>();
    mWorld.registerComponent<Facing>();
    mWorld.registerComponent<Stamina>();
    mWorld.registerComponent<Combat>();
    mWorld.registerComponent<Health>();
    mWorld.registerComponent<ZombieAI>();
    mWorld.registerComponent<Needs>();
    mWorld.registerComponent<Interactable>();
    mWorld.registerComponent<Animation>();
    mWorld.registerComponent<Bleeding>();
    mWorld.registerComponent<Sleeping>();
    mWorld.registerComponent<Inventory>();
    mWorld.registerComponent<MentalState>();
    mWorld.registerComponent<Structure>();
    mWorld.registerComponent<Projectile>();
    mWorld.registerComponent<Wounds>();
    mWorld.registerComponent<Traits>();
    mWorld.registerComponent<SurvivorAI>();
}

void Game::startNewRun()
{
    mWorld = World{};
    registerComponents();

    mDebugEntities.clear();
    mNoiseModel = NoiseModel{};
    mNoiseEmitTimer = 0.0f;
    mMovementNoiseTier = NoiseTier::None;

    mRunState = RunState::Playing;
    mDeathStateTimer = 0.0;
    mDeathCause.clear();
    mDeathWorldPosition = glm::vec2(0.0f, 0.0f);

    mGameHours = 8.0;
    mCurrentDay = 1;
    mShowInventory = false;
    mShowCrafting = false;
    mCraftingRecipeCursor = 0;
    mCraftingQuantity = 1;
    mCraftingCategory = 0;
    mCraftingNotebookView = false;
    mSessionDiscoveredRecipes.clear();
    mCraftQueue.clear();
    mActiveCraftProgressSeconds = 0.0f;

    mRunStats = RunStats{};

    mCurrentCharacterName = generateCharacterName(mSaveManager.currentRunSeed());

    mInteractionMessage.clear();
    mInteractionMessageTimer = 0.0f;
    mContainerOpen = false;
    mOpenContainerEntity = kInvalidEntity;
    mContainerCursor = 0;

    // Generate run-variant spawns against the stable layout (map is NOT regenerated)
    mMapGenerator.generateSpawns(mSpawnData, mSaveManager.currentRunSeed());

    // Mode-specific spawn pressure tuning.
    if (mTitleModeIndex == static_cast<int>(TitleRunMode::Scavenger))
    {
        auto reduceTo = [](std::vector<glm::ivec2>& list, float ratio)
        {
            if (list.empty()) return;
            const std::size_t target = std::max<std::size_t>(1, static_cast<std::size_t>(std::round(static_cast<float>(list.size()) * ratio)));
            if (target < list.size())
            {
                list.resize(target);
            }
        };
        auto duplicateTo = [](std::vector<glm::ivec2>& list, float ratio)
        {
            if (list.empty() || ratio <= 1.0f) return;
            const std::size_t base = list.size();
            const std::size_t target = static_cast<std::size_t>(std::round(static_cast<float>(base) * ratio));
            for (std::size_t i = 0; list.size() < target; ++i)
            {
                list.push_back(list[i % base]);
            }
        };

        reduceTo(mSpawnData.zombieSpawns, 0.78f);
        duplicateTo(mSpawnData.foodSpawns, 1.20f);
        duplicateTo(mSpawnData.waterSpawns, 1.25f);
        duplicateTo(mSpawnData.bandageSpawns, 1.15f);
    }
    else if (mTitleModeIndex == static_cast<int>(TitleRunMode::Overrun))
    {
        auto reduceTo = [](std::vector<glm::ivec2>& list, float ratio)
        {
            if (list.empty()) return;
            const std::size_t target = std::max<std::size_t>(1, static_cast<std::size_t>(std::round(static_cast<float>(list.size()) * ratio)));
            if (target < list.size())
            {
                list.resize(target);
            }
        };
        auto duplicateTo = [](std::vector<glm::ivec2>& list, float ratio)
        {
            if (list.empty() || ratio <= 1.0f) return;
            const std::size_t base = list.size();
            const std::size_t target = static_cast<std::size_t>(std::round(static_cast<float>(base) * ratio));
            for (std::size_t i = 0; list.size() < target; ++i)
            {
                list.push_back(list[i % base]);
            }
        };

        duplicateTo(mSpawnData.zombieSpawns, 1.35f);
        reduceTo(mSpawnData.foodSpawns, 0.75f);
        reduceTo(mSpawnData.waterSpawns, 0.80f);
        reduceTo(mSpawnData.bandageSpawns, 0.70f);
    }

    const std::uint64_t runLayoutFingerprint = computeLayoutFingerprint(mMapGenerator.layoutData());
    if (mLayoutFingerprint != 0 && runLayoutFingerprint != mLayoutFingerprint)
    {
        std::cerr << "[World] Layout fingerprint changed between runs. expected=0x"
                  << std::hex << mLayoutFingerprint << " actual=0x" << runLayoutFingerprint << std::dec << '\n';
    }
    else if (mLayoutFingerprint == 0)
    {
        mLayoutFingerprint = runLayoutFingerprint;
    }

    mCamera.clampToBounds(mTileMap.pixelSize());

    setupScene();

    const bool allowCheckpointRestore = !mIgnoreCheckpointRestoreOnce;
    mIgnoreCheckpointRestoreOnce = false;
    bool restoredCheckpoint = false;

    // Attempt to restore from checkpoint
    RunCheckpoint cp;
    if (allowCheckpointRestore)
    {
        cp = mSaveManager.loadCheckpoint();
    }

    if (allowCheckpointRestore && cp.valid && mPlayerEntity != kInvalidEntity)
    {
        if (mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            auto& t = mWorld.getComponent<Transform>(mPlayerEntity);
            t.x = cp.playerX; t.y = cp.playerY;
        }
        if (mWorld.hasComponent<Health>(mPlayerEntity))
        {
            auto& h = mWorld.getComponent<Health>(mPlayerEntity);
            h.current = cp.healthCurrent; h.maximum = cp.healthMaximum;
        }
        if (mWorld.hasComponent<Needs>(mPlayerEntity))
            mWorld.getComponent<Needs>(mPlayerEntity) = cp.needs;
        if (mWorld.hasComponent<MentalState>(mPlayerEntity))
            mWorld.getComponent<MentalState>(mPlayerEntity) = cp.mental;
        if (mWorld.hasComponent<Wounds>(mPlayerEntity))
            mWorld.getComponent<Wounds>(mPlayerEntity).active = cp.wounds;
        if (cp.bleeding)
        {
            if (!mWorld.hasComponent<Bleeding>(mPlayerEntity))
                mWorld.addComponent<Bleeding>(mPlayerEntity, Bleeding{});
        }
        if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            mWorld.getComponent<Inventory>(mPlayerEntity) = cp.inventory;

        mCurrentDay = cp.currentDay;
        mGameHours = cp.gameHours;
        mCurrentCharacterName = cp.characterName;
        mRunStats.kills = cp.kills;
        mRunStats.itemsLooted = cp.itemsLooted;
        mRunStats.structuresBuilt = cp.structuresBuilt;
        mRunStats.distanceWalked = cp.distanceWalked;

        mInteractionMessage = "Resumed " + mCurrentCharacterName + "'s run.";
        mInteractionMessageTimer = 3.0f;
        restoredCheckpoint = true;
    }
    else if (!allowCheckpointRestore)
    {
        mSaveManager.deleteCheckpoint();
    }

    if (!restoredCheckpoint)
    {
        if (mTitleModeIndex >= 0 && mTitleModeIndex < static_cast<int>(kTitleModeNames.size()))
        {
            mInteractionMessage = std::string("Mode: ") + kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)];
            mInteractionMessageTimer = 2.5f;
        }
    }
}

void Game::returnToTitleScreen()
{
    // Navigation only: return to title without writing a new checkpoint/save.
    mWorld = World{};
    registerComponents();

    mDebugEntities.clear();
    mPlayerEntity = kInvalidEntity;

    mNoiseModel = NoiseModel{};
    mNoiseEmitTimer = 0.0f;
    mMovementNoiseTier = NoiseTier::None;

    mBuildSystem = BuildSystem{};
    mParticleSystem = ParticleSystem{};

    mRunState = RunState::TitleScreen;
    mPauseSelection = 0;

    mDeathStateTimer = 0.0;
    mDeathCause.clear();
    mDeathWorldPosition = glm::vec2(0.0f, 0.0f);

    mGameHours = 8.0;
    mCurrentDay = 1;
    mShowInventory = false;
    mInventoryCursor = 0;
    mShowCrafting = false;
    mCraftingRecipeCursor = 0;
    mCraftingQuantity = 1;
    mCraftingCategory = 0;
    mCraftingNotebookView = false;
    mSessionDiscoveredRecipes.clear();
    mCraftQueue.clear();
    mActiveCraftProgressSeconds = 0.0f;
    mContainerOpen = false;
    mOpenContainerEntity = kInvalidEntity;
    mContainerCursor = 0;
    mRunStats = RunStats{};

    mInteractionMessage.clear();
    mInteractionMessageTimer = 0.0f;
    mProximityPrompt.clear();
    mCurrentCharacterName.clear();

    mShowControls = false;
    mShowTitleCredits = false;
    mTitleMenuSelection = 0;
    mTitleStatusMessage = "Returned to title menu.";
    mTitleStatusTimer = 2.0f;

    const RunCheckpoint titleCheckpoint = mSaveManager.loadCheckpoint();
    mTitleHasCheckpoint = titleCheckpoint.valid;
    mTitleCheckpointDay = titleCheckpoint.valid ? titleCheckpoint.currentDay : 1;
    mTitleCheckpointName = titleCheckpoint.valid ? titleCheckpoint.characterName : std::string{};
    mTitleCheckpointX = titleCheckpoint.valid ? titleCheckpoint.playerX : 0.0f;
    mTitleCheckpointY = titleCheckpoint.valid ? titleCheckpoint.playerY : 0.0f;

    const auto& layout = mMapGenerator.layoutData();
    if (layout.width > 0 && layout.height > 0)
    {
        const glm::vec2 startCenter(
            (static_cast<float>(layout.playerStart.x) + 0.5f) * static_cast<float>(mTileMap.tileWidth()),
            (static_cast<float>(layout.playerStart.y) + 0.5f) * static_cast<float>(mTileMap.tileHeight()));
        mCamera.snapTo(startCenter);
    }
    mCamera.clampToBounds(mTileMap.pixelSize());
}

void Game::shutdown()
{
    mDebugEntities.clear();
    mPlayerEntity = kInvalidEntity;
    mMovementNoiseTier = NoiseTier::None;

    mSpriteBatch.shutdown();
    mTexture.unload();
    mAudioManager.shutdown();

    if (mGlContext != nullptr)
    {
        SDL_GL_DeleteContext(mGlContext);
        mGlContext = nullptr;
    }

    if (mWindow != nullptr)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
    {
        SDL_Quit();
    }

    mIsRunning = false;
}

void Game::setupScene()
{
    const GLuint sheetTex = mSpriteSheet.textureId();
    std::mt19937 runRng(mSaveManager.currentRunSeed());

    mPlayerEntity = mWorld.createEntity();

    Transform playerTransform{};
    playerTransform.x = static_cast<float>(mSpawnData.playerStart.x * mTileMap.tileWidth());
    playerTransform.y = static_cast<float>(mSpawnData.playerStart.y * mTileMap.tileHeight());

    Sprite playerSprite{};
    playerSprite.textureId = sheetTex;
    playerSprite.uvRect = mSpriteSheet.uvRect("player_idle");
    playerSprite.color = glm::vec4(1.0f);
    playerSprite.layer = 10;

    Animation playerAnim{};
    playerAnim.currentAnim = "player_idle";
    playerAnim.frameDuration = 0.15f;

    mWorld.addComponent<Transform>(mPlayerEntity, playerTransform);
    mWorld.addComponent<Sprite>(mPlayerEntity, playerSprite);
    mWorld.addComponent<Animation>(mPlayerEntity, playerAnim);
    mWorld.addComponent<Velocity>(mPlayerEntity, Velocity{});
    mWorld.addComponent<Facing>(mPlayerEntity, Facing{});
    mWorld.addComponent<PlayerInput>(mPlayerEntity, PlayerInput{});
    mWorld.addComponent<Stamina>(mPlayerEntity, Stamina{100.0f, 100.0f, 30.0f, 100.0f});
    mWorld.addComponent<Combat>(mPlayerEntity, Combat{});
    mWorld.addComponent<Health>(mPlayerEntity, Health{100.0f, 100.0f});
    mWorld.addComponent<Needs>(mPlayerEntity, Needs{});

    // Inventory with starting items
    {
        Inventory playerInv{};
        mWorld.addComponent<Inventory>(mPlayerEntity, playerInv);
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

        int startBandages = 2;
        int startFood = 3;
        int startWater = 2;
        if (mTitleModeIndex == static_cast<int>(TitleRunMode::Scavenger))
        {
            startBandages = 3;
            startFood = 4;
            startWater = 3;
        }
        else if (mTitleModeIndex == static_cast<int>(TitleRunMode::Overrun))
        {
            startBandages = 1;
            startFood = 2;
            startWater = 1;
        }

        InventorySystem::addItem(inv, mItemDatabase, 5, 1);  // knife
        InventorySystem::addItem(inv, mItemDatabase, 4, startBandages);
        InventorySystem::addItem(inv, mItemDatabase, 0, startFood);
        InventorySystem::addItem(inv, mItemDatabase, 3, startWater);
    }
    mWorld.addComponent<MentalState>(mPlayerEntity, MentalState{});
    mWorld.addComponent<Wounds>(mPlayerEntity, Wounds{});

    // Assign traits from seed
    {
        const std::uint32_t traitSeed = mSaveManager.currentRunSeed() ^ 0xDEADBEEFu;
        Traits traits{};
        traits.positive = static_cast<PositiveTrait>(1 + (traitSeed % 3));         // 1-3
        traits.negative = static_cast<NegativeTrait>(1 + ((traitSeed / 3) % 3));   // 1-3
        mWorld.addComponent<Traits>(mPlayerEntity, traits);

        // Apply trait effects
        if (traits.positive == PositiveTrait::Tough)
        {
            Health& h = mWorld.getComponent<Health>(mPlayerEntity);
            h.maximum += 20.0f;
            h.current = h.maximum;
        }
        // Quick and LightSleeper are applied dynamically in movement/sleep systems
    }

    // --- Zombies ---
    std::uniform_real_distribution<float> hpDist(30.0f, 70.0f);
    std::uniform_real_distribution<float> speedMul(0.8f, 1.2f);

    for (const auto& tile : mSpawnData.zombieSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;

        Entity zombie = mWorld.createEntity();
        Transform zt{}; zt.x = static_cast<float>(tile.x * mTileMap.tileWidth()); zt.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite zs{}; zs.textureId = sheetTex; zs.uvRect = mSpriteSheet.uvRect("zombie_idle"); zs.color = glm::vec4(1.0f); zs.layer = 8;
        Animation za{}; za.currentAnim = "zombie_idle"; za.frameDuration = 0.18f;
        ZombieAI zai{}; zai.targetWorld = glm::vec2(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
        const float hp = hpDist(runRng);
        zai.moveSpeed = 112.0f * speedMul(runRng);
        zai.spawnDay = mCurrentDay;
        mWorld.addComponent<Transform>(zombie, zt);
        mWorld.addComponent<Sprite>(zombie, zs);
        mWorld.addComponent<Animation>(zombie, za);
        mWorld.addComponent<Velocity>(zombie, Velocity{});
        mWorld.addComponent<ZombieAI>(zombie, zai);
        mWorld.addComponent<Health>(zombie, Health{hp, hp});
    }

    // --- NPC Survivors ---
    {
        static const char* kSurvivorNames[] = {
            "Alex", "Jordan", "Casey", "Morgan", "Riley",
            "Sam", "Quinn", "Avery", "Blake", "Cameron"
        };
        constexpr int kNameCount = 10;

        // Load persisted survivors or spawn fresh ones
        auto savedSurvivors = mSaveManager.loadSurvivors();
        if (!savedSurvivors.empty())
        {
            for (const auto& sr : savedSurvivors)
            {
                if (!sr.alive) continue;
                Entity npc = mWorld.createEntity();
                Transform nt{}; nt.x = sr.x; nt.y = sr.y;
                Sprite ns{}; ns.textureId = sheetTex; ns.uvRect = mSpriteSheet.uvRect("player_idle"); ns.color = glm::vec4(0.7f, 0.85f, 1.0f, 1.0f); ns.layer = 9;
                Animation na{}; na.currentAnim = "player_idle"; na.frameDuration = 0.2f;
                SurvivorAI sai{};
                sai.name = sr.name;
                sai.spawnDay = sr.spawnDay;
                sai.hunger = sr.hunger;
                sai.thirst = sr.thirst;
                sai.personality.aggression = sr.aggression;
                sai.personality.cooperation = sr.cooperation;
                sai.personality.competence = sr.competence;
                sai.homeDistrictId = sr.homeDistrictId;
                mWorld.addComponent<Transform>(npc, nt);
                mWorld.addComponent<Sprite>(npc, ns);
                mWorld.addComponent<Animation>(npc, na);
                mWorld.addComponent<Velocity>(npc, Velocity{});
                mWorld.addComponent<SurvivorAI>(npc, sai);
                mWorld.addComponent<Health>(npc, Health{60.0f, 60.0f});
            }
        }
        else
        {
            // Spawn 2-5 fresh survivors in non-wilderness districts
            std::uniform_int_distribution<int> npcCountDist(2, 5);
            const int npcCount = npcCountDist(runRng);

            const auto& layout = mMapGenerator.layoutData();
            std::vector<int> validDistricts;
            for (const auto& dd : layout.districts)
            {
                if (dd.type != static_cast<int>(DistrictType::Wilderness))
                    validDistricts.push_back(dd.id);
            }

            for (int i = 0; i < npcCount; ++i)
            {
                int districtId = 0;
                float spawnX = 200.0f;
                float spawnY = 200.0f;

                if (!validDistricts.empty())
                {
                    districtId = validDistricts[runRng() % validDistricts.size()];
                    for (const auto& dd : layout.districts)
                    {
                        if (dd.id == districtId)
                        {
                            std::uniform_int_distribution<int> xDist(dd.minX + 1, std::max(dd.minX + 1, dd.maxX - 1));
                            std::uniform_int_distribution<int> yDist(dd.minY + 1, std::max(dd.minY + 1, dd.maxY - 1));
                            int tx = xDist(runRng);
                            int ty = yDist(runRng);
                            // Find non-solid tile nearby
                            for (int attempts = 0; attempts < 10; ++attempts)
                            {
                                if (!mTileMap.isSolid(tx, ty)) break;
                                tx = xDist(runRng);
                                ty = yDist(runRng);
                            }
                            spawnX = static_cast<float>(tx * mTileMap.tileWidth());
                            spawnY = static_cast<float>(ty * mTileMap.tileHeight());
                            break;
                        }
                    }
                }

                Entity npc = mWorld.createEntity();
                Transform nt{}; nt.x = spawnX; nt.y = spawnY;
                Sprite ns{}; ns.textureId = sheetTex; ns.uvRect = mSpriteSheet.uvRect("player_idle"); ns.color = glm::vec4(0.7f, 0.85f, 1.0f, 1.0f); ns.layer = 9;
                Animation na{}; na.currentAnim = "player_idle"; na.frameDuration = 0.2f;

                SurvivorAI sai{};
                sai.name = kSurvivorNames[(runRng() % kNameCount)];
                sai.spawnDay = mCurrentDay;
                sai.homeDistrictId = districtId;

                std::uniform_real_distribution<float> traitDist(0.0f, 1.0f);
                sai.personality.aggression = traitDist(runRng);
                sai.personality.cooperation = traitDist(runRng);
                sai.personality.competence = traitDist(runRng);

                mWorld.addComponent<Transform>(npc, nt);
                mWorld.addComponent<Sprite>(npc, ns);
                mWorld.addComponent<Animation>(npc, na);
                mWorld.addComponent<Velocity>(npc, Velocity{});
                mWorld.addComponent<SurvivorAI>(npc, sai);
                mWorld.addComponent<Health>(npc, Health{60.0f, 60.0f});
            }
        }
    }

    // Apply Hardened bloodline trait if player has retirement history
    if (mSaveManager.hasRetirementHistory() && mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        traits.bloodline = BloodlineTrait::Hardened;
    }

    // Initialize district infection system
    mDistrictInfectionSystem.initialize(mMapGenerator.layoutData());

    const auto& layoutDistricts = mMapGenerator.layoutData().districts;
    auto getDistrictType = [&](int tileX, int tileY) -> DistrictType
    {
        const int distId = mTileMap.getDistrictId(tileX, tileY);
        if (distId == 0) return DistrictType::Wilderness;
        for (const auto& def : layoutDistricts)
        {
            if (def.id == distId) return static_cast<DistrictType>(def.type);
        }
        return DistrictType::Wilderness;
    };

    // --- Food ---
    static const std::array<const char*, 3> foodSprites = {"item_canned_food", "item_apple", "item_bread"};
    std::uniform_real_distribution<float> foodRestore(24.0f, 40.0f);

    for (const auto& tile : mSpawnData.foodSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity food = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(foodSprites[runRng() % foodSprites.size()]); s.color = glm::vec4(1.0f); s.layer = 6;
        Interactable inter{}; inter.type = InteractableType::FoodPickup; inter.hungerRestore = foodRestore(runRng); inter.prompt = "Scavenge food";
        mWorld.addComponent<Transform>(food, t);
        mWorld.addComponent<Sprite>(food, s);
        mWorld.addComponent<Interactable>(food, inter);
    }

    // --- Water ---
    std::uniform_real_distribution<float> waterRestoreDist(30.0f, 45.0f);
    for (const auto& tile : mSpawnData.waterSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity water = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("item_water_bottle"); s.color = glm::vec4(1.0f); s.layer = 6;
        Interactable inter{}; inter.type = InteractableType::WaterPickup; inter.thirstRestore = waterRestoreDist(runRng); inter.prompt = "Collect water";
        mWorld.addComponent<Transform>(water, t);
        mWorld.addComponent<Sprite>(water, s);
        mWorld.addComponent<Interactable>(water, inter);
    }

    // --- Weapons ---
    struct WeaponDef { WeaponCategory category; const char* sprite; };
    static const std::array<WeaponDef, 4> weaponDefs = {{
        {WeaponCategory::Bladed, "item_knife"},
        {WeaponCategory::Blunt, "item_bat"},
        {WeaponCategory::Blunt, "item_pipe"},
        {WeaponCategory::Bladed, "item_axe"},
    }};
    for (const auto& tile : mSpawnData.weaponSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        const auto& wd = weaponDefs[runRng() % weaponDefs.size()];
        Entity wpn = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(wd.sprite); s.color = glm::vec4(1.0f); s.layer = 6;
        Interactable inter{}; inter.type = InteractableType::WeaponPickup; inter.weaponCategory = static_cast<int>(wd.category); inter.prompt = "Take weapon";
        mWorld.addComponent<Transform>(wpn, t);
        mWorld.addComponent<Sprite>(wpn, s);
        mWorld.addComponent<Interactable>(wpn, inter);
    }

    // --- Bandages ---
    for (const auto& tile : mSpawnData.bandageSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity bandage = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("item_bandage"); s.color = glm::vec4(1.0f); s.layer = 6;
        Interactable inter{}; inter.type = InteractableType::BandagePickup; inter.prompt = "Take bandage";
        mWorld.addComponent<Transform>(bandage, t);
        mWorld.addComponent<Sprite>(bandage, s);
        mWorld.addComponent<Interactable>(bandage, inter);
    }

    // --- Medical caches ---
    {
        std::uniform_int_distribution<int> extraBandageDist(1, 2);
        std::uniform_int_distribution<int> extraAntibioticDist(1, 2);

        for (const auto& tile : mSpawnData.medicalCacheSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            Entity cache = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("env_shelf"); s.color = glm::vec4(0.85f, 0.9f, 0.95f, 1.0f); s.layer = 5;
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Raid med cache";
            Inventory cacheInv{};

            mWorld.addComponent<Transform>(cache, t);
            mWorld.addComponent<Sprite>(cache, s);
            mWorld.addComponent<Interactable>(cache, inter);
            mWorld.addComponent<Inventory>(cache, cacheInv);

            Inventory& inv = mWorld.getComponent<Inventory>(cache);
            InventorySystem::addItem(inv, mItemDatabase, 22, 1);
            InventorySystem::addItem(inv, mItemDatabase, 4, extraBandageDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 15, extraAntibioticDist(runRng));
        }
    }

    // --- Supply caches ---
    {
        std::uniform_int_distribution<int> foodQtyDist(2, 4);
        std::uniform_int_distribution<int> waterQtyDist(1, 3);
        std::uniform_int_distribution<int> materialQtyDist(1, 3);

        for (const auto& tile : mSpawnData.supplyCacheSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            Entity cache = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("env_crate"); s.color = glm::vec4(0.95f, 0.85f, 0.65f, 1.0f); s.layer = 5;
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Raid supply cache";
            Inventory cacheInv{};

            mWorld.addComponent<Transform>(cache, t);
            mWorld.addComponent<Sprite>(cache, s);
            mWorld.addComponent<Interactable>(cache, inter);
            mWorld.addComponent<Inventory>(cache, cacheInv);

            Inventory& inv = mWorld.getComponent<Inventory>(cache);
            InventorySystem::addItem(inv, mItemDatabase, 0, foodQtyDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 3, waterQtyDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 9, materialQtyDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 11, 1);
        }
    }

    // --- Sleeping bags ---
    for (const auto& tile : mSpawnData.sleepSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity bed = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("item_sleeping_bag"); s.color = glm::vec4(1.0f); s.layer = 5;
        Interactable inter{}; inter.type = InteractableType::SleepSpot; inter.sleepHours = 6.0f; inter.sleepQuality = 0.8f; inter.prompt = "Sleep";
        mWorld.addComponent<Transform>(bed, t);
        mWorld.addComponent<Sprite>(bed, s);
        mWorld.addComponent<Interactable>(bed, inter);
    }

    // --- Home furniture dressing pass (visual-first worldbuilding) ---
    {
        const std::array<const char*, 10> residentialFurniture = {
            "env_bed", "env_sleeping_area", "env_shelf", "env_crate", "env_shelf", "env_bed", "env_chair", "env_table", "env_cabinet", "env_couch"
        };
        const std::array<const char*, 7> commercialFurniture = {
            "env_shelf", "env_crate", "env_shelf", "env_barricade", "env_crate", "env_table", "env_chair"
        };
        const std::array<const char*, 5> industrialFurniture = {
            "env_crate", "env_shelf", "env_barricade", "env_noise_trap", "env_crate"
        };
        const std::array<const char*, 4> wildernessFurniture = {
            "env_sleeping_area", "env_crate", "env_shelf", "env_barricade"
        };

        const auto spawnFurniture = [&](int tileX, int tileY, const char* spriteName, const glm::vec4& color, int layer)
        {
            Entity decor = mWorld.createEntity();
            Transform t{};
            t.x = static_cast<float>(tileX * mTileMap.tileWidth());
            t.y = static_cast<float>(tileY * mTileMap.tileHeight());
            Sprite s{};
            s.textureId = sheetTex;
            s.uvRect = mSpriteSheet.uvRect(spriteName);
            s.color = color;
            s.layer = layer;
            mWorld.addComponent<Transform>(decor, t);
            mWorld.addComponent<Sprite>(decor, s);
        };

        int furnitureIdx = 0;
        for (const auto& tile : mSpawnData.homeFurnitureSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* spriteName = "env_crate";
            glm::vec4 tint = glm::vec4(0.88f, 0.84f, 0.78f, 1.0f);

            switch (dt)
            {
            case DistrictType::Residential:
                spriteName = residentialFurniture[furnitureIdx % residentialFurniture.size()];
                tint = glm::vec4(0.93f, 0.88f, 0.80f, 1.0f);
                break;
            case DistrictType::Commercial:
                spriteName = commercialFurniture[furnitureIdx % commercialFurniture.size()];
                tint = glm::vec4(0.82f, 0.86f, 0.90f, 1.0f);
                break;
            case DistrictType::Industrial:
                spriteName = industrialFurniture[furnitureIdx % industrialFurniture.size()];
                tint = glm::vec4(0.82f, 0.76f, 0.68f, 1.0f);
                break;
            default:
                spriteName = wildernessFurniture[furnitureIdx % wildernessFurniture.size()];
                tint = glm::vec4(0.78f, 0.76f, 0.72f, 0.95f);
                break;
            }

            spawnFurniture(tile.x, tile.y, spriteName, tint, 4);

            // Lightweight micro-encounter accents for visual storytelling.
            if ((furnitureIdx % 13) == 0)
            {
                const int nx = tile.x + (((furnitureIdx / 2) % 2 == 0) ? 1 : -1);
                const int ny = tile.y + (((furnitureIdx / 5) % 2 == 0) ? 0 : 1);
                if (!mTileMap.isSolid(nx, ny))
                {
                    const char* accentSprite = "env_crate";
                    if (dt == DistrictType::Commercial)
                    {
                        accentSprite = "env_barricade";
                    }
                    else if (dt == DistrictType::Industrial)
                    {
                        accentSprite = "env_noise_trap";
                    }
                    spawnFurniture(nx, ny, accentSprite, tint * glm::vec4(0.94f, 0.94f, 0.94f, 0.9f), 4);
                }
            }

            if ((furnitureIdx % 17) == 0)
            {
                const int sx = tile.x + (((furnitureIdx / 3) % 2 == 0) ? 0 : -1);
                const int sy = tile.y + (((furnitureIdx / 4) % 2 == 0) ? 1 : 0);
                if (!mTileMap.isSolid(sx, sy))
                {
                    const char* stain = (dt == DistrictType::Industrial) ? "env_blood_large" : "env_blood_small";
                    spawnFurniture(sx, sy, stain, glm::vec4(0.78f, 0.18f, 0.14f, 0.70f), 3);
                }
            }

            ++furnitureIdx;
        }
    }

    // --- Indoor decor (ambient clutter and distress cues inside buildings) ---
    {
        const std::array<const char*, 5> residentialIndoor = {
            "env_blood_small", "env_blood_large", "env_shelf", "env_crate", "env_sleeping_area"
        };
        const std::array<const char*, 5> commercialIndoor = {
            "env_shelf", "env_crate", "env_blood_small", "env_blood_large", "env_shelf"
        };
        const std::array<const char*, 5> industrialIndoor = {
            "env_crate", "env_noise_trap", "env_barricade", "env_blood_large", "env_shelf"
        };
        const std::array<const char*, 4> wildernessIndoor = {
            "env_crate", "env_blood_small", "env_blood_large", "env_shelf"
        };

        const auto spawnDecor = [&](int tileX, int tileY, const char* spriteName, const glm::vec4& color, int layer)
        {
            Entity decor = mWorld.createEntity();
            Transform t{};
            t.x = static_cast<float>(tileX * mTileMap.tileWidth());
            t.y = static_cast<float>(tileY * mTileMap.tileHeight());
            Sprite s{};
            s.textureId = sheetTex;
            s.uvRect = mSpriteSheet.uvRect(spriteName);
            s.color = color;
            s.layer = layer;
            mWorld.addComponent<Transform>(decor, t);
            mWorld.addComponent<Sprite>(decor, s);
        };

        int decorIdx = 0;
        for (const auto& tile : mSpawnData.decorIndoorSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* spriteName = "env_crate";
            glm::vec4 tint = glm::vec4(0.9f, 0.85f, 0.8f, 1.0f);

            switch (dt)
            {
            case DistrictType::Residential:
                spriteName = residentialIndoor[decorIdx % residentialIndoor.size()];
                tint = glm::vec4(0.92f, 0.88f, 0.82f, 1.0f);
                break;
            case DistrictType::Commercial:
                spriteName = commercialIndoor[decorIdx % commercialIndoor.size()];
                tint = glm::vec4(0.86f, 0.88f, 0.90f, 1.0f);
                break;
            case DistrictType::Industrial:
                spriteName = industrialIndoor[decorIdx % industrialIndoor.size()];
                tint = glm::vec4(0.84f, 0.80f, 0.74f, 1.0f);
                break;
            default:
                spriteName = wildernessIndoor[decorIdx % wildernessIndoor.size()];
                tint = glm::vec4(0.80f, 0.78f, 0.74f, 0.95f);
                break;
            }

            spawnDecor(tile.x, tile.y, spriteName, tint, 4);

            if ((decorIdx % 9) == 0)
            {
                const int nx = tile.x + (((decorIdx / 3) % 2 == 0) ? 1 : -1);
                const int ny = tile.y + (((decorIdx / 2) % 2 == 0) ? 0 : 1);
                if (!mTileMap.isSolid(nx, ny))
                {
                    const char* clusterSprite = (dt == DistrictType::Industrial) ? "env_crate" : "env_shelf";
                    spawnDecor(nx, ny, clusterSprite, tint * glm::vec4(0.95f, 0.95f, 0.95f, 0.9f), 4);
                }
            }

            ++decorIdx;
        }
    }

    // --- Outdoor decor (debris, abandoned props) ---
    {
        const std::array<const char*, 5> residentialOutdoor = {
            "env_grave", "env_grave2", "env_campfire", "env_crate", "env_barricade"
        };
        const std::array<const char*, 5> commercialOutdoor = {
            "env_barricade", "env_noise_trap", "env_crate", "env_grave2", "env_campfire"
        };
        const std::array<const char*, 5> industrialOutdoor = {
            "env_noise_trap", "env_barricade", "env_crate", "env_campfire", "env_grave"
        };
        const std::array<const char*, 4> wildernessOutdoor = {
            "env_grave", "env_campfire", "env_crate", "env_grave2"
        };

        int decorIdx = 0;
        for (const auto& tile : mSpawnData.decorOutdoorSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* spriteName = "env_crate";
            glm::vec4 tint = glm::vec4(0.7f, 0.65f, 0.6f, 0.9f);

            switch (dt)
            {
            case DistrictType::Residential:
                spriteName = residentialOutdoor[decorIdx % residentialOutdoor.size()];
                tint = glm::vec4(0.74f, 0.69f, 0.63f, 0.9f);
                break;
            case DistrictType::Commercial:
                spriteName = commercialOutdoor[decorIdx % commercialOutdoor.size()];
                tint = glm::vec4(0.68f, 0.70f, 0.74f, 0.92f);
                break;
            case DistrictType::Industrial:
                spriteName = industrialOutdoor[decorIdx % industrialOutdoor.size()];
                tint = glm::vec4(0.72f, 0.66f, 0.58f, 0.92f);
                break;
            default:
                spriteName = wildernessOutdoor[decorIdx % wildernessOutdoor.size()];
                tint = glm::vec4(0.62f, 0.66f, 0.60f, 0.88f);
                break;
            }

            Entity decor = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(spriteName);
            s.color = tint; s.layer = 5;
            mWorld.addComponent<Transform>(decor, t);
            mWorld.addComponent<Sprite>(decor, s);

            if ((decorIdx % 11) == 0)
            {
                const int nx = tile.x + (((decorIdx / 2) % 2 == 0) ? -1 : 1);
                const int ny = tile.y + (((decorIdx / 4) % 2 == 0) ? 1 : 0);
                if (!mTileMap.isSolid(nx, ny))
                {
                    Entity clusterDecor = mWorld.createEntity();
                    Transform ct{};
                    ct.x = static_cast<float>(nx * mTileMap.tileWidth());
                    ct.y = static_cast<float>(ny * mTileMap.tileHeight());
                    Sprite cs{};
                    cs.textureId = sheetTex;
                    cs.uvRect = mSpriteSheet.uvRect("env_crate");
                    cs.color = tint * glm::vec4(0.95f, 0.95f, 0.95f, 0.85f);
                    cs.layer = 4;
                    mWorld.addComponent<Transform>(clusterDecor, ct);
                    mWorld.addComponent<Sprite>(clusterDecor, cs);
                }
            }

            ++decorIdx;
        }
    }

    // --- Blood splatters ---
    {
        static const char* kBloodSprites[] = {"env_blood_small", "env_blood_large"};
        int bloodIdx = 0;
        for (const auto& tile : mSpawnData.bloodSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;
            Entity blood = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(kBloodSprites[bloodIdx % 2]);
            s.color = glm::vec4(0.8f, 0.15f, 0.1f, 0.7f); s.layer = 3;
            mWorld.addComponent<Transform>(blood, t);
            mWorld.addComponent<Sprite>(blood, s);
            ++bloodIdx;
        }
    }

    // --- Loot containers (district-aware loot profiles) ---
    {
        const int itemDbSize = static_cast<int>(mItemDatabase.allItems().size());
        std::uniform_int_distribution<int> lootCountDist(2, 4);
        std::uniform_int_distribution<int> lootQtyDist(1, 3);

        // District loot tables (item IDs)
        // Residential: food(0,1,2,21), drink(3), sleeping bag type items
        // Commercial: weapons(5-8,17), medical(4,15,16,22), general
        // Industrial: materials(9-14,18-20), bandages(4), tools
        const std::array<int, 7> residentialLoot = {0, 1, 2, 21, 3, 3, 23};
        const std::array<int, 7> commercialLoot  = {5, 6, 7, 8, 17, 22, 23};
        const std::array<int, 6> industrialLoot  = {9, 10, 11, 12, 18, 4};

        const auto isSpecialCacheTile = [&](const glm::ivec2& tile)
        {
            const auto matchesTile = [&](const glm::ivec2& other)
            {
                return other.x == tile.x && other.y == tile.y;
            };

            return std::any_of(mSpawnData.medicalCacheSpawns.begin(), mSpawnData.medicalCacheSpawns.end(), matchesTile) ||
                   std::any_of(mSpawnData.supplyCacheSpawns.begin(), mSpawnData.supplyCacheSpawns.end(), matchesTile);
        };

        // Build set of previously looted tile positions for fast lookup
        const auto& lootedPos = mSaveManager.lootedPositions();
        auto isLooted = [&](int tx, int ty) -> bool
        {
            for (const auto& [lx, ly] : lootedPos)
            {
                if (lx == tx && ly == ty) return true;
            }
            return false;
        };

        for (const auto& tile : mSpawnData.containerSpawns)
        {
            if (isSpecialCacheTile(tile)) continue;
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            // Skip containers at previously looted positions
            if (isLooted(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* containerSprite = "env_crate";
            glm::vec4 containerTint = glm::vec4(0.8f, 0.7f, 0.5f, 1.0f);
            const char* containerPrompt = "Search stash";

            switch (dt)
            {
            case DistrictType::Residential:
                containerSprite = "env_shelf";
                containerTint = glm::vec4(0.83f, 0.79f, 0.72f, 1.0f);
                containerPrompt = "Search pantry";
                break;
            case DistrictType::Commercial:
                containerSprite = "env_crate";
                containerTint = glm::vec4(0.72f, 0.76f, 0.80f, 1.0f);
                containerPrompt = "Search storefront";
                break;
            case DistrictType::Industrial:
                containerSprite = "env_crate";
                containerTint = glm::vec4(0.80f, 0.72f, 0.58f, 1.0f);
                containerPrompt = "Search workshop crate";
                break;
            default:
                containerSprite = "env_crate";
                containerTint = glm::vec4(0.72f, 0.69f, 0.62f, 1.0f);
                containerPrompt = "Search abandoned stash";
                break;
            }

            Entity container = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(containerSprite); s.color = containerTint; s.layer = 5;
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = containerPrompt;
            Inventory containerInv{};
            mWorld.addComponent<Transform>(container, t);
            mWorld.addComponent<Sprite>(container, s);
            mWorld.addComponent<Interactable>(container, inter);
            mWorld.addComponent<Inventory>(container, containerInv);
            Inventory& inv = mWorld.getComponent<Inventory>(container);

            const int lootTypes = lootCountDist(runRng);
            for (int li = 0; li < lootTypes; ++li)
            {
                int itemId;
                switch (dt)
                {
                case DistrictType::Residential:
                    itemId = residentialLoot[runRng() % residentialLoot.size()];
                    break;
                case DistrictType::Commercial:
                    itemId = commercialLoot[runRng() % commercialLoot.size()];
                    break;
                case DistrictType::Industrial:
                    itemId = industrialLoot[runRng() % industrialLoot.size()];
                    break;
                default: // Wilderness — random from full database
                    itemId = std::uniform_int_distribution<int>(0, std::max(0, itemDbSize - 1))(runRng);
                    break;
                }
                InventorySystem::addItem(inv, mItemDatabase, itemId, lootQtyDist(runRng));
            }
        }
    }

    // --- Ambient journal notes (district-authored flavor lines) ---
    {
        const std::array<const char*, 6> residentialNotes = {
            "The upstairs door sticks; kick low, not high.",
            "Left canned food under the sink. If it's gone, so am I.",
            "We marked each room before dark. Do the same.",
            "Our block looked safe until the sirens stopped.",
            "If you hear a baby monitor, leave immediately.",
            "Back fence has one loose board for escape.",
        };
        const std::array<const char*, 6> commercialNotes = {
            "Checkout lane still has meds taped underneath.",
            "The alarms died, but they still come to broken glass.",
            "Office stairwell is clear until sunset.",
            "Security shutters jam at the halfway point.",
            "Storage room had water on day four. Not on day seven.",
            "If the storefront is quiet, check the loading bay first.",
        };
        const std::array<const char*, 6> industrialNotes = {
            "North warehouse has tools, south warehouse has teeth.",
            "We welded one gate shut and regretted it.",
            "Forklift battery still works for floodlights.",
            "Machine room echoes hide footsteps until they're close.",
            "Look for painted arrows near the tanks.",
            "Keep one wrench. Doors fail more than people.",
        };
        const std::array<const char*, 5> wildernessNotes = {
            "Campfire ash is fresh. Stay alert.",
            "Water here tastes metallic after rain.",
            "Tracks circle this lot every dawn.",
            "We buried supplies under the fallen sign.",
            "If birds go silent, move now.",
        };

        const auto spawnAmbientNote = [&](int tileX, int tileY, const std::string& text)
        {
            Entity noteEntity = mWorld.createEntity();

            Transform nt{};
            nt.x = static_cast<float>(tileX * mTileMap.tileWidth()) + 5.0f;
            nt.y = static_cast<float>(tileY * mTileMap.tileHeight()) + 4.0f;

            Sprite ns{};
            ns.textureId = sheetTex;
            ns.uvRect = mSpriteSheet.uvRect("item_bandage");
            ns.color = glm::vec4(0.84f, 0.79f, 0.62f, 0.95f);
            ns.layer = 6;

            Interactable ni{};
            ni.type = InteractableType::JournalNote;
            ni.prompt = "Read note";
            ni.message = text;

            mWorld.addComponent<Transform>(noteEntity, nt);
            mWorld.addComponent<Sprite>(noteEntity, ns);
            mWorld.addComponent<Interactable>(noteEntity, ni);
        };

        int notesPlaced = 0;
        const int noteLimit = std::max(6, std::min(14, static_cast<int>(mSpawnData.decorOutdoorSpawns.size() / 6 + mSpawnData.decorIndoorSpawns.size() / 10)));
        int noteSeed = 0;

        for (const auto& tile : mSpawnData.decorOutdoorSpawns)
        {
            if (notesPlaced >= noteLimit) break;
            if ((noteSeed % 7) != 0)
            {
                ++noteSeed;
                continue;
            }
            if (mTileMap.isSolid(tile.x, tile.y))
            {
                ++noteSeed;
                continue;
            }

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* line = nullptr;
            switch (dt)
            {
            case DistrictType::Residential:
                line = residentialNotes[(noteSeed + notesPlaced) % residentialNotes.size()];
                break;
            case DistrictType::Commercial:
                line = commercialNotes[(noteSeed + notesPlaced) % commercialNotes.size()];
                break;
            case DistrictType::Industrial:
                line = industrialNotes[(noteSeed + notesPlaced) % industrialNotes.size()];
                break;
            default:
                line = wildernessNotes[(noteSeed + notesPlaced) % wildernessNotes.size()];
                break;
            }

            spawnAmbientNote(tile.x, tile.y, std::string("Scrawled note: ") + line);
            ++notesPlaced;
            ++noteSeed;
        }

        for (const auto& tile : mSpawnData.decorIndoorSpawns)
        {
            if (notesPlaced >= noteLimit) break;
            if ((noteSeed % 11) != 0)
            {
                ++noteSeed;
                continue;
            }
            if (mTileMap.isSolid(tile.x, tile.y))
            {
                ++noteSeed;
                continue;
            }

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* line = nullptr;
            switch (dt)
            {
            case DistrictType::Residential:
                line = residentialNotes[(noteSeed + notesPlaced) % residentialNotes.size()];
                break;
            case DistrictType::Commercial:
                line = commercialNotes[(noteSeed + notesPlaced) % commercialNotes.size()];
                break;
            case DistrictType::Industrial:
                line = industrialNotes[(noteSeed + notesPlaced) % industrialNotes.size()];
                break;
            default:
                line = wildernessNotes[(noteSeed + notesPlaced) % wildernessNotes.size()];
                break;
            }

            spawnAmbientNote(tile.x, tile.y, std::string("Journal scrap: ") + line);
            ++notesPlaced;
            ++noteSeed;
        }
    }

    // Restore persisted structures (degraded between runs)
    for (const StructureRecord& rec : mSaveManager.structures())
    {
        const int recipeIdx = rec.type;
        if (recipeIdx < 0 || recipeIdx >= BuildSystem::kRecipeCount) continue;
        const auto& recipe = BuildSystem::kRecipes[recipeIdx];

        Entity structEntity = mWorld.createEntity();
        Transform t{}; t.x = rec.x; t.y = rec.y;
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(recipe.sprite); s.color = glm::vec4(1.0f); s.layer = 4;
        Structure structure{}; structure.type = recipe.type; structure.maxHp = recipe.hp;
        structure.hp = recipe.hp * rec.hpRatio;
        structure.condition = static_cast<StructureCondition>(rec.condition);
        mWorld.addComponent<Transform>(structEntity, t);
        mWorld.addComponent<Sprite>(structEntity, s);
        mWorld.addComponent<Structure>(structEntity, structure);

        // Restore Supply Cache inventory
        if (recipe.type == StructureType::SupplyCache && !rec.items.empty())
        {
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Loot cache";
            mWorld.addComponent<Interactable>(structEntity, inter);
            Inventory inv{};
            for (const auto& [itemId, count] : rec.items)
            {
                InventorySystem::addItem(inv, mItemDatabase, itemId, count);
            }
            mWorld.addComponent<Inventory>(structEntity, inv);
        }
        // Rain Collector gets container but no items (regenerates fresh)
        else if (recipe.type == StructureType::RainCollector)
        {
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Collect water";
            mWorld.addComponent<Interactable>(structEntity, inter);
            mWorld.addComponent<Inventory>(structEntity, Inventory{});
        }
    }

    const auto spawnGraveMarker = [&](const GraveRecord& grave)
    {
        Entity graveEntity = mWorld.createEntity();

        Transform graveTransform{};
        graveTransform.x = grave.x;
        graveTransform.y = grave.y;

        Sprite graveSprite{};
        graveSprite.textureId = sheetTex;
        graveSprite.uvRect = mSpriteSheet.uvRect("env_grave");
        graveSprite.color = glm::vec4(1.0f);
        graveSprite.layer = 7;

        Interactable graveInteract{};
        graveInteract.type = InteractableType::GraveMarker;
        graveInteract.prompt = "Read epitaph";

        const int graveTileX = static_cast<int>(grave.x) / std::max(1, mTileMap.tileWidth());
        const int graveTileY = static_cast<int>(grave.y) / std::max(1, mTileMap.tileHeight());
        const DistrictType graveDistrict = getDistrictType(graveTileX, graveTileY);
        const char* districtLabel = "WILDERNESS";
        switch (graveDistrict)
        {
        case DistrictType::Residential: districtLabel = "RESIDENTIAL"; break;
        case DistrictType::Commercial: districtLabel = "COMMERCIAL"; break;
        case DistrictType::Industrial: districtLabel = "INDUSTRIAL"; break;
        default: districtLabel = "WILDERNESS"; break;
        }

        std::ostringstream message;
        message << "Here lies " << grave.name << " | Survived " << grave.day
            << " days | " << districtLabel << " | " << grave.cause;
        graveInteract.message = message.str();

        mWorld.addComponent<Transform>(graveEntity, graveTransform);
        mWorld.addComponent<Sprite>(graveEntity, graveSprite);
        mWorld.addComponent<Interactable>(graveEntity, graveInteract);

        // Spawn journal note beside the grave
        {
            Entity journalEntity = mWorld.createEntity();

            Transform jt{};
            jt.x = grave.x + 20.0f;
            jt.y = grave.y + 8.0f;

            Sprite js{};
            js.textureId = sheetTex;
            js.uvRect = mSpriteSheet.uvRect("item_bandage"); // reuse small item sprite
            js.color = glm::vec4(0.85f, 0.80f, 0.65f, 1.0f); // parchment tint
            js.layer = 6;

            // Generate procedural flavor text from the grave data
            const std::array<const char*, 5> residentialFlavor = {
                "The house alarms died, but the footsteps didn't.",
                "I barricaded the kitchen and still heard them upstairs.",
                "Family photos stayed on the wall longer than we did.",
                "Backyards connect faster than streets after dark.",
                "Every porch light looked like a signal.",
            };
            const std::array<const char*, 5> commercialFlavor = {
                "I slept in the stockroom between shipping pallets.",
                "The register drawers were empty before the shelves were.",
                "Reflections in storefront glass nearly got me killed.",
                "Loading docks are exits if you keep them clear.",
                "The mall speakers played static all night.",
            };
            const std::array<const char*, 5> industrialFlavor = {
                "The plant gates held until someone cut power.",
                "Tools mattered more than ammo by day three.",
                "Conveyors jammed with things that used to be people.",
                "The siren tower became a beacon for the dead.",
                "I trusted steel walls and forgot about roof access.",
            };
            const std::array<const char*, 4> wildernessFlavor = {
                "Open ground helps until exhaustion makes you loud.",
                "We kept moving camp but not fast enough.",
                "The quiet lots are where hope goes to thin out.",
                "Smoke carries farther than footsteps.",
            };

            const std::uint32_t flavorHash =
                (static_cast<std::uint32_t>(grave.day) * 2654435761u) ^
                static_cast<std::uint32_t>(grave.x + grave.y * 137.0f);

            const auto pickFlavor = [&](const auto& lines) -> const char*
            {
                const std::size_t idx = static_cast<std::size_t>(flavorHash % static_cast<std::uint32_t>(lines.size()));
                return lines[idx];
            };

            const char* flavorLine = nullptr;
            switch (graveDistrict)
            {
            case DistrictType::Residential: flavorLine = pickFlavor(residentialFlavor); break;
            case DistrictType::Commercial: flavorLine = pickFlavor(commercialFlavor); break;
            case DistrictType::Industrial: flavorLine = pickFlavor(industrialFlavor); break;
            default: flavorLine = pickFlavor(wildernessFlavor); break;
            }

            std::string causeHint;
            if (grave.cause.find("bite") != std::string::npos || grave.cause.find("Bite") != std::string::npos)
            {
                causeHint = "The bite went hot before dawn.";
            }
            else if (grave.cause.find("bleed") != std::string::npos || grave.cause.find("Bleed") != std::string::npos)
            {
                causeHint = "I ran out of clean wraps.";
            }
            else if (grave.cause.find("starv") != std::string::npos || grave.cause.find("Starv") != std::string::npos)
            {
                causeHint = "I traded too much food for one more day.";
            }
            else if (grave.cause.find("thirst") != std::string::npos || grave.cause.find("Thirst") != std::string::npos)
            {
                causeHint = "Dry taps ended us faster than the dead.";
            }
            else if (grave.cause.find("infection") != std::string::npos || grave.cause.find("Infection") != std::string::npos)
            {
                causeHint = "The fever came back worse each night.";
            }

            std::ostringstream journalMsg;
            journalMsg << grave.name << "'s journal (Day " << grave.day << "): "
                       << flavorLine;
            if (!causeHint.empty())
            {
                journalMsg << ' ' << causeHint;
            }

            Interactable ji{};
            ji.type = InteractableType::JournalNote;
            ji.prompt = "Read note";
            ji.message = journalMsg.str();

            mWorld.addComponent<Transform>(journalEntity, jt);
            mWorld.addComponent<Sprite>(journalEntity, js);
            mWorld.addComponent<Interactable>(journalEntity, ji);
        }
    };

    for (const GraveRecord& grave : mSaveManager.graves())
    {
        spawnGraveMarker(grave);
    }

    // Spawn retirement markers (visually distinct from graves)
    for (const RetirementRecord& ret : mSaveManager.retirements())
    {
        Entity retEntity = mWorld.createEntity();

        Transform retTransform{};
        retTransform.x = ret.x;
        retTransform.y = ret.y;

        Sprite retSprite{};
        retSprite.textureId = sheetTex;
        retSprite.uvRect = mSpriteSheet.uvRect("env_grave2");
        retSprite.color = glm::vec4(0.6f, 0.7f, 0.9f, 1.0f); // blue tint for retirement
        retSprite.layer = 7;

        Interactable retInteract{};
        retInteract.type = InteractableType::GraveMarker;
        retInteract.prompt = "Read";

        std::ostringstream message;
        message << ret.name << " walked away on Day " << ret.day << " | They couldn't go on.";
        retInteract.message = message.str();

        mWorld.addComponent<Transform>(retEntity, retTransform);
        mWorld.addComponent<Sprite>(retEntity, retSprite);
        mWorld.addComponent<Interactable>(retEntity, retInteract);
    }

    const Transform& playerTransformRef = mWorld.getComponent<Transform>(mPlayerEntity);
    const glm::vec2 playerCenter(
        playerTransformRef.x + kSpriteSize * 0.5f,
        playerTransformRef.y + kSpriteSize * 0.5f);
    mCamera.snapTo(playerCenter);
    mCamera.clampToBounds(mTileMap.pixelSize());

    mRunStats.prevX = playerTransformRef.x;
    mRunStats.prevY = playerTransformRef.y;

    // Initialize prev positions for interpolation
    snapshotPositions();
}

void Game::beginDeathState(const std::string& cause)
{
    if (mRunState == RunState::Dead)
    {
        return;
    }

    mRunState = RunState::Dead;
    mDeathStateTimer = 0.0;
    mDeathCause = cause;
    mAudioManager.playSound(SoundId::Death, 0.8f);

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& transform = mWorld.getComponent<Transform>(mPlayerEntity);
        mDeathWorldPosition = glm::vec2(transform.x, transform.y);
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
        velocity.dx = 0.0f;
        velocity.dy = 0.0f;
    }

    GraveRecord record;
    record.name = mCurrentCharacterName;
    record.x = mDeathWorldPosition.x;
    record.y = mDeathWorldPosition.y;
    record.day = mCurrentDay;
    record.runIndex = mSaveManager.runNumber();
    record.cause = cause;

    if (mSaveManager.recordDeath(record))
    {
        mInteractionMessage = "Their grave has been marked.";
    }
    else
    {
        mInteractionMessage = "Death recorded, but legacy write failed.";
    }

    // Save structures for next run
    {
        std::vector<StructureRecord> structRecords;
        mWorld.forEach<Structure, Transform>(
            [&](Entity e, Structure& s, Transform& t)
            {
                StructureRecord rec;
                rec.type = static_cast<int>(s.type);
                rec.x = t.x;
                rec.y = t.y;
                rec.condition = static_cast<int>(s.condition);
                rec.hpRatio = s.maxHp > 0.0f ? s.hp / s.maxHp : 0.0f;
                // Save Supply Cache inventory
                if (s.type == StructureType::SupplyCache && mWorld.hasComponent<Inventory>(e))
                {
                    const Inventory& inv = mWorld.getComponent<Inventory>(e);
                    for (const auto& slot : inv.slots)
                    {
                        if (slot.itemId >= 0 && slot.count > 0)
                        {
                            rec.items.emplace_back(slot.itemId, slot.count);
                        }
                    }
                }
                structRecords.push_back(rec);
            });
        mSaveManager.saveStructures(structRecords);
    }

    // Save cumulative stats
    mSaveManager.addRunStats(mRunStats.kills, mCurrentDay);

    // Delete mid-run checkpoint (permadeath)
    mSaveManager.deleteCheckpoint();
    mSaveManager.saveLootedPositions();

    // Save NPC survivors for next run
    {
        std::vector<SurvivorRecord> survivorRecords;
        mWorld.forEach<SurvivorAI, Transform, Health>(
            [&](Entity, SurvivorAI& ai, Transform& t, Health& h)
            {
                SurvivorRecord sr;
                sr.name = ai.name;
                sr.alive = ai.alive && h.current > 0.0f;
                sr.spawnDay = ai.spawnDay;
                sr.deathDay = sr.alive ? 0 : mCurrentDay;
                sr.x = t.x;
                sr.y = t.y;
                sr.hunger = ai.hunger;
                sr.thirst = ai.thirst;
                sr.aggression = ai.personality.aggression;
                sr.cooperation = ai.personality.cooperation;
                sr.competence = ai.personality.competence;
                sr.homeDistrictId = ai.homeDistrictId;
                survivorRecords.push_back(sr);
            });
        mSaveManager.saveSurvivors(survivorRecords);
    }

    // Balance instrumentation
    std::cout << "[Run End] Death - " << mCurrentCharacterName
              << " | Day " << mCurrentDay << " | Cause: " << cause
              << " | Kills: " << mRunStats.kills
              << " | Items: " << mRunStats.itemsLooted
              << " | Structures: " << mRunStats.structuresBuilt
              << " | Distance: " << static_cast<int>(mRunStats.distanceWalked) << "px"
              << '\n';

    mInteractionMessageTimer = 5.0f;
}

void Game::beginRetirementState()
{
    if (mRunState == RunState::Retired || mRunState == RunState::Dead)
    {
        return;
    }

    mRunState = RunState::Retired;
    mDeathStateTimer = 0.0;
    mDeathCause = "Gave up on Day " + std::to_string(mCurrentDay);

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& transform = mWorld.getComponent<Transform>(mPlayerEntity);
        mDeathWorldPosition = glm::vec2(transform.x, transform.y);
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
        velocity.dx = 0.0f;
        velocity.dy = 0.0f;
    }

    RetirementRecord retRecord;
    retRecord.name = mCurrentCharacterName;
    retRecord.x = mDeathWorldPosition.x;
    retRecord.y = mDeathWorldPosition.y;
    retRecord.day = mCurrentDay;
    retRecord.runIndex = mSaveManager.runNumber();
    mSaveManager.recordRetirement(retRecord);

    // Save structures for next run
    {
        std::vector<StructureRecord> structRecords;
        mWorld.forEach<Structure, Transform>(
            [&](Entity e, Structure& s, Transform& t)
            {
                StructureRecord rec;
                rec.type = static_cast<int>(s.type);
                rec.x = t.x;
                rec.y = t.y;
                rec.condition = static_cast<int>(s.condition);
                rec.hpRatio = s.maxHp > 0.0f ? s.hp / s.maxHp : 0.0f;
                if (s.type == StructureType::SupplyCache && mWorld.hasComponent<Inventory>(e))
                {
                    const Inventory& inv = mWorld.getComponent<Inventory>(e);
                    for (const auto& slot : inv.slots)
                    {
                        if (slot.itemId >= 0 && slot.count > 0)
                        {
                            rec.items.emplace_back(slot.itemId, slot.count);
                        }
                    }
                }
                structRecords.push_back(rec);
            });
        mSaveManager.saveStructures(structRecords);
    }

    // Save cumulative stats
    mSaveManager.addRunStats(mRunStats.kills, mCurrentDay);

    // Delete mid-run checkpoint (retirement ends the run)
    mSaveManager.deleteCheckpoint();
    mSaveManager.saveLootedPositions();

    // Save NPC survivors for next run
    {
        std::vector<SurvivorRecord> survivorRecords;
        mWorld.forEach<SurvivorAI, Transform, Health>(
            [&](Entity, SurvivorAI& ai, Transform& t, Health& h)
            {
                SurvivorRecord sr;
                sr.name = ai.name;
                sr.alive = ai.alive && h.current > 0.0f;
                sr.spawnDay = ai.spawnDay;
                sr.deathDay = sr.alive ? 0 : mCurrentDay;
                sr.x = t.x;
                sr.y = t.y;
                sr.hunger = ai.hunger;
                sr.thirst = ai.thirst;
                sr.aggression = ai.personality.aggression;
                sr.cooperation = ai.personality.cooperation;
                sr.competence = ai.personality.competence;
                sr.homeDistrictId = ai.homeDistrictId;
                survivorRecords.push_back(sr);
            });
        mSaveManager.saveSurvivors(survivorRecords);
    }

    // Balance instrumentation
    std::cout << "[Run End] Retirement - " << mCurrentCharacterName
              << " | Day " << mCurrentDay
              << " | Kills: " << mRunStats.kills
              << " | Items: " << mRunStats.itemsLooted
              << " | Structures: " << mRunStats.structuresBuilt
              << " | Distance: " << static_cast<int>(mRunStats.distanceWalked) << "px"
              << '\n';

    mInteractionMessage = "They couldn't go on.";
    mInteractionMessageTimer = 5.0f;
}

void Game::processInputEvents()
{
    mInput.beginFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        mInput.processEvent(event);

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            mWindowWidth = event.window.data1;
            mWindowHeight = event.window.data2;
            mProjection = buildProjection(mWindowWidth, mWindowHeight);
            mCamera.setViewportSize(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight));
            mCamera.clampToBounds(mTileMap.pixelSize());
        }
    }

    mInput.refreshMouseState();

    if (mInput.quitRequested())
    {
        mIsRunning = false;
    }

    // Mouse wheel zoom
    if (mInput.scrollY() != 0)
    {
        constexpr float kZoomStep = 0.15f;
        const float currentZoom = mCamera.zoom();
        const float newZoom = currentZoom * (1.0f + static_cast<float>(mInput.scrollY()) * kZoomStep);
        const glm::vec2 mouseScreen(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY()));
        mCamera.zoomToward(newZoom, mouseScreen);
        mCamera.clampToBounds(mTileMap.pixelSize());
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        if (mRunState == RunState::Playing && mShowCrafting)
        {
            mShowCrafting = false;
            mCraftingNotebookView = false;
        }
        else if (mRunState == RunState::Playing)
        {
            mRunState = RunState::Paused;
            mPauseSelection = 0;
        }
        else if (mRunState == RunState::Paused)
        {
            mRunState = RunState::Playing;
        }
        else if (mRunState == RunState::TitleScreen)
        {
            if (mShowTitleCredits)
            {
                mShowTitleCredits = false;
            }
            else if (mShowControls)
            {
                mShowControls = false;
            }
            else
            {
                mIsRunning = false;
            }
        }
    }
}

void Game::update(float dtSeconds)
{
    mMouseWorld = screenToWorld(glm::vec2(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY())));

    mInteractionMessageTimer = std::max(0.0f, mInteractionMessageTimer - dtSeconds);
    if (mInteractionMessageTimer <= 0.0f)
    {
        mInteractionMessage.clear();
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_F1))
    {
        mShowNoiseDebug = !mShowNoiseDebug;
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_F2))
    {
        mShowControls = !mShowControls;
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_F3))
    {
        mShowDistrictDebug = !mShowDistrictDebug;
    }

    // Title screen hub: briefing menu, mode, and save-slot selection.
    if (mRunState == RunState::TitleScreen)
    {
        mTitlePulseTimer += dtSeconds;
        mTitleStatusTimer = std::max(0.0f, mTitleStatusTimer - dtSeconds);
        if (mTitleStatusTimer <= 0.0f)
        {
            mTitleStatusMessage.clear();
        }

        const RunCheckpoint titleCheckpoint = mSaveManager.loadCheckpoint();
        mTitleHasCheckpoint = titleCheckpoint.valid;
        mTitleCheckpointDay = titleCheckpoint.valid ? titleCheckpoint.currentDay : 1;
        mTitleCheckpointName = titleCheckpoint.valid ? titleCheckpoint.characterName : std::string{};
        mTitleCheckpointX = titleCheckpoint.valid ? titleCheckpoint.playerX : 0.0f;
        mTitleCheckpointY = titleCheckpoint.valid ? titleCheckpoint.playerY : 0.0f;

        const int menuCount = static_cast<int>(kTitleMenuItems.size());
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            mTitleMenuSelection = wrapIndex(mTitleMenuSelection - 1, menuCount);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            mTitleMenuSelection = wrapIndex(mTitleMenuSelection + 1, menuCount);
        }

        auto adjustMenuValue = [&](int delta)
        {
            if (mTitleMenuSelection == 2) // Save Slot
            {
                const int oldIndex = mTitleSaveSlotIndex;
                const int newIndex = wrapIndex(mTitleSaveSlotIndex + delta, static_cast<int>(kTitleSaveSlots.size()));
                if (newIndex == oldIndex)
                {
                    return;
                }

                if (loadSlotAndWorld(kTitleSaveSlots[static_cast<std::size_t>(newIndex)]))
                {
                    mTitleSaveSlotIndex = newIndex;
                    mTitleStatusMessage = std::string("Active slot: ") + kTitleSaveLabels[static_cast<std::size_t>(newIndex)];
                    mTitleStatusTimer = 2.0f;
                }
                else
                {
                    mTitleStatusMessage = "Unable to load slot data.";
                    mTitleStatusTimer = 2.5f;
                }
                return;
            }

            if (mTitleMenuSelection == 3) // Mode
            {
                mTitleModeIndex = wrapIndex(mTitleModeIndex + delta, static_cast<int>(kTitleModeNames.size()));
                mTitleStatusMessage = std::string("Mode: ") + kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)];
                mTitleStatusTimer = 2.0f;
            }
        };

        if (mInput.wasKeyPressed(SDL_SCANCODE_LEFT) || mInput.wasKeyPressed(SDL_SCANCODE_A))
        {
            adjustMenuValue(-1);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_RIGHT) || mInput.wasKeyPressed(SDL_SCANCODE_D))
        {
            adjustMenuValue(1);
        }

        const bool activate =
            mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
            mInput.wasKeyPressed(SDL_SCANCODE_SPACE);

        if (activate)
        {
            switch (mTitleMenuSelection)
            {
            case 0: // Continue Run
                if (mTitleHasCheckpoint)
                {
                    mIgnoreCheckpointRestoreOnce = false;
                    startNewRun();
                }
                else
                {
                    mTitleStatusMessage = "No checkpoint found for this slot.";
                    mTitleStatusTimer = 2.5f;
                }
                break;

            case 1: // Start New Run
                mSaveManager.deleteCheckpoint();
                mIgnoreCheckpointRestoreOnce = true;
                startNewRun();
                break;

            case 2: // Save Slot
                adjustMenuValue(1);
                break;

            case 3: // Mode
                adjustMenuValue(1);
                break;

            case 4: // Options
                mShowControls = !mShowControls;
                if (mShowControls)
                {
                    mShowTitleCredits = false;
                }
                break;

            case 5: // Credits
                mShowTitleCredits = !mShowTitleCredits;
                if (mShowTitleCredits)
                {
                    mShowControls = false;
                }
                break;

            case 6: // Quit
                mIsRunning = false;
                break;

            default:
                break;
            }
        }
        return;
    }

    // Pause menu input
    if (mRunState == RunState::Paused)
    {
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            mPauseSelection = (mPauseSelection + 3) % 4;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            mPauseSelection = (mPauseSelection + 1) % 4;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_RETURN) || mInput.wasKeyPressed(SDL_SCANCODE_SPACE))
        {
            if (mPauseSelection == 0) // Resume
            {
                mRunState = RunState::Playing;
            }
            else if (mPauseSelection == 1) // Back to Menu
            {
                returnToTitleScreen();
            }
            else if (mPauseSelection == 2) // Restart
            {
                startNewRun();
            }
            else // Quit
            {
                mIsRunning = false;
            }
        }
        return;
    }

    if (mRunState == RunState::Dead || mRunState == RunState::Retired)
    {
        mDeathStateTimer += static_cast<double>(dtSeconds);

        const bool restartRequested =
            mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
            mInput.wasKeyPressed(SDL_SCANCODE_SPACE) ||
            mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT);

        if (restartRequested && mDeathStateTimer >= kDeathRestartDelaySeconds)
        {
            startNewRun();
        }

        return;
    }

    bool playerAlive = false;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        playerAlive = health.current > 0.0f;
    }

    if (playerAlive && !mShowCrafting)
    {
        mInputSystem.update(mWorld, mInput, mMouseWorld);
    }
    else if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
        velocity.dx = 0.0f;
        velocity.dy = 0.0f;
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_K) && !mDebugEntities.empty())
    {
        const Entity entity = mDebugEntities.back();
        mDebugEntities.pop_back();
        mWorld.destroyEntity(entity);
    }

    advanceGameClock(static_cast<double>(dtSeconds) * kGameHoursPerRealSecond, mGameHours, mCurrentDay);

    mNeedsSystem.update(mWorld, dtSeconds, mGameHours);
    mBleedingSystem.update(mWorld, dtSeconds);
    mWoundSystem.update(mWorld, mPlayerEntity, dtSeconds, mGameHours);

    // Campfire warmth: warm player body temperature when near a campfire
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Needs>(mPlayerEntity) &&
        mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 pc(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);
        bool nearCampfire = false;

        mWorld.forEach<Structure, Transform>(
            [&](Entity, Structure& structure, Transform& st)
            {
                if (nearCampfire) return;
                if (structure.type != StructureType::Campfire || structure.hp <= 0.0f) return;
                const glm::vec2 sc(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);
                if (glm::distance(pc, sc) < 96.0f)
                {
                    nearCampfire = true;
                }
            });

        if (nearCampfire)
        {
            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            constexpr float kCampfireWarmRate = 0.5f;
            constexpr float kCampfireTargetTemp = 37.5f;
            if (needs.bodyTemperature < kCampfireTargetTemp)
            {
                needs.bodyTemperature = std::min(kCampfireTargetTemp, needs.bodyTemperature + kCampfireWarmRate * dtSeconds);
            }
        }
    }

    // Inventory food spoilage
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        const float gameHoursThisTick = static_cast<float>(dtSeconds * kGameHoursPerRealSecond);
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        InventorySystem::degradeFood(inv, mItemDatabase, gameHoursThisTick);
    }

    // Mental state system
    mMentalStateSystem.update(mWorld, mNoiseModel, mPlayerEntity, dtSeconds, mGameHours, mCurrentDay);

    // Weather system
    mWeatherSystem.update(mWeatherState, dtSeconds, mGameHours, mCurrentDay);

    // Weather effects on player body temperature
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        // Push body temperature toward ambient weather temperature
        const float ambientTarget = std::clamp(mWeatherState.temperature, -10.0f, 45.0f);
        const float tempPressure = (ambientTarget < 15.0f) ? -0.1f : (ambientTarget > 35.0f ? 0.05f : 0.0f);
        needs.bodyTemperature += tempPressure * dtSeconds;
        needs.bodyTemperature = std::clamp(needs.bodyTemperature, 30.0f, 42.0f);
    }

    // Survivor AI system
    mSurvivorAISystem.update(mWorld, mTileMap, mNoiseModel, mPlayerEntity, dtSeconds, mGameHours, mCurrentDay);

    // Survivor interaction system tick
    mSurvivorInteractionSystem.update(mWorld, mPlayerEntity, dtSeconds);

    // District infection daily update
    mDistrictInfectionSystem.dailyUpdate(mWorld, mMapGenerator.layoutData(), mCurrentDay);

    // Build system
    {
        int structCountBefore = 0;
        mWorld.forEach<Structure>([&](Entity, Structure&) { ++structCountBefore; });
        const std::string buildMsg = mBuildSystem.update(mWorld, mInput, mTileMap, mNoiseModel, mItemDatabase, mSpriteSheet, mPlayerEntity, mMouseWorld, dtSeconds, mGameHours);
        int structCountAfter = 0;
        mWorld.forEach<Structure>([&](Entity, Structure&) { ++structCountAfter; });
        if (structCountAfter > structCountBefore)
        {
            mRunStats.structuresBuilt += (structCountAfter - structCountBefore);
        }
        if (!buildMsg.empty())
        {
            mInteractionMessage = buildMsg;
            mInteractionMessageTimer = 3.0f;
        }
    }

    // Crafting panel toggle (C) and panel controls.
    if (mInput.wasKeyPressed(SDL_SCANCODE_C) && mRunState == RunState::Playing)
    {
        mShowCrafting = !mShowCrafting;
        if (mShowCrafting)
        {
            mShowInventory = false;
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
            mCraftingRecipeCursor = 0;
            mCraftingQuantity = 1;
            mCraftingNotebookView = false;

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

        if (mInput.wasKeyPressed(SDL_SCANCODE_J))
        {
            mCraftingNotebookView = !mCraftingNotebookView;
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

        if (!visible.empty() && (mInput.wasKeyPressed(SDL_SCANCODE_RETURN) || mInput.wasKeyPressed(SDL_SCANCODE_SPACE)))
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

    // Inventory toggle (Tab)
    if (mInput.wasKeyPressed(SDL_SCANCODE_TAB))
    {
        if (mShowCrafting)
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
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
            mInteractionMessage = "Container empty.";
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

        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            mInventoryCursor = (mInventoryCursor + Inventory::kMaxSlots - 1) % Inventory::kMaxSlots;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
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

        // Use item (F key): use cursor slot when inventory is open, otherwise active hotbar slot
        if (mInput.wasKeyPressed(SDL_SCANCODE_F))
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
                        combat.weapon = makeWeaponStats(static_cast<WeaponCategory>(item->weaponCategory));
                        mInteractionMessage = "Equipped " + item->name + ".";
                        mInteractionMessageTimer = 3.0f;
                    }
                }
            }
        }

    }

    // Panic drift: add random velocity jitter proportional to panic level
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        if (mental.panic >= 25.0f)
        {
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            // Scale drift strength: 0 at 25 panic, full at 100
            const float t = (mental.panic - 25.0f) / 75.0f; // 0..1
            const float driftStrength = t * 80.0f; // up to 80 px/s drift
            vel.dx += std::cos(mental.panicDriftAngle) * driftStrength;
            vel.dy += std::sin(mental.panicDriftAngle) * driftStrength;
        }
    }

    // Depression effects: slow movement
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        if (mental.depression > 20.0f)
        {
            const float slowFactor = 1.0f - mental.depression * 0.003f;
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            vel.dx *= slowFactor;
            vel.dy *= slowFactor;
        }
    }

    // Depression crisis penalties: heavy movement slowdown while in crisis state.
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        if (mental.inCrisis)
        {
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            vel.dx *= 0.6f;
            vel.dy *= 0.6f;
        }
    }

    // Encumbrance slowdown
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Inventory>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const float weight = InventorySystem::totalWeight(inv, mItemDatabase);
        if (weight > Inventory::kMaxCarryWeight)
        {
            const float overRatio = std::min((weight - Inventory::kMaxCarryWeight) / 10.0f, 0.6f);
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            vel.dx *= (1.0f - overRatio);
            vel.dy *= (1.0f - overRatio);
        }
    }

    // Retirement: depression crisis window expired
    if (mMentalStateSystem.shouldForceRetire())
    {
        mMentalStateSystem.clearForceRetire();
        beginRetirementState();
        return;
    }

    // Sleep time-skip: if player is sleeping, fast-forward time and recover needs
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        Sleeping& sleeping = mWorld.getComponent<Sleeping>(mPlayerEntity);
        constexpr float kSleepTimeMultiplier = 8.0f;
        constexpr float kSleepWakeRange = 160.0f;
        constexpr float kSleepHealPerHour = 3.0f;
        const float extraDtSeconds = dtSeconds * (kSleepTimeMultiplier - 1.0f);
        const float gameHoursThisTick = static_cast<float>(dtSeconds * kGameHoursPerRealSecond * kSleepTimeMultiplier);
        sleeping.durationRemaining -= gameHoursThisTick;
        advanceGameClock(gameHoursThisTick, mGameHours, mCurrentDay);

        if (extraDtSeconds > 0.0f)
        {
            mNeedsSystem.update(mWorld, extraDtSeconds, mGameHours);
            mBleedingSystem.update(mWorld, extraDtSeconds);
            mWoundSystem.update(mWorld, mPlayerEntity, extraDtSeconds, mGameHours);

            if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                InventorySystem::degradeFood(inv, mItemDatabase, extraDtSeconds * static_cast<float>(kGameHoursPerRealSecond));
            }
        }

        if (mWorld.hasComponent<Needs>(mPlayerEntity))
        {
            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            needs.sleepDebt = std::max(0.0f, needs.sleepDebt - sleeping.debtRecoveryRate * sleeping.quality * gameHoursThisTick);
            needs.fatigue = std::max(0.0f, needs.fatigue - sleeping.fatigueRecoveryRate * sleeping.quality * gameHoursThisTick);

            const bool canRecoverHealth =
                needs.hunger > 45.0f &&
                needs.thirst > 45.0f &&
                needs.illness < 25.0f &&
                !mWorld.hasComponent<Bleeding>(mPlayerEntity) &&
                (!mWorld.hasComponent<Wounds>(mPlayerEntity) || !hasInfectedWound(mWorld.getComponent<Wounds>(mPlayerEntity)));

            if (canRecoverHealth && mWorld.hasComponent<Health>(mPlayerEntity))
            {
                Health& health = mWorld.getComponent<Health>(mPlayerEntity);
                health.current = std::min(health.maximum, health.current + kSleepHealPerHour * sleeping.quality * gameHoursThisTick);
            }
        }

        // Check for noise interruption (zombies nearby while sleeping)
        bool interrupted = false;
        float wakeRange = kSleepWakeRange;
        if (mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            if (traits.positive == PositiveTrait::LightSleeper) wakeRange *= 1.5f;
        }
        if (mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const glm::vec2 pc(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);
            mWorld.forEach<ZombieAI, Transform>(
                [&](Entity, ZombieAI&, Transform& zt)
                {
                    const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                    if (glm::distance(pc, zc) < wakeRange)
                    {
                        interrupted = true;
                    }
                });
        }

        if (sleeping.durationRemaining <= 0.0f || interrupted)
        {
            if (interrupted)
            {
                mInteractionMessage = "Woken up by nearby threat!";
            }
            else
            {
                mInteractionMessage = "Slept, but supplies are running down.";

                if (mWorld.hasComponent<MentalState>(mPlayerEntity))
                {
                    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                    const float depressionRelief = sleeping.quality >= 0.85f
                        ? 2.0f
                        : std::max(0.4f, sleeping.quality * 1.4f);
                    mental.depression = std::max(0.0f, mental.depression - depressionRelief);
                }

                // Checkpoint after a full sleep cycle
                if (mPlayerEntity != kInvalidEntity)
                {
                    RunCheckpoint cp;
                    cp.runSeed = mSaveManager.currentRunSeed();
                    cp.characterName = mCurrentCharacterName;
                    cp.currentDay = mCurrentDay;
                    cp.gameHours = mGameHours;
                    if (mWorld.hasComponent<Transform>(mPlayerEntity))
                    {
                        const auto& t = mWorld.getComponent<Transform>(mPlayerEntity);
                        cp.playerX = t.x; cp.playerY = t.y;
                    }
                    if (mWorld.hasComponent<Health>(mPlayerEntity))
                    {
                        const auto& h = mWorld.getComponent<Health>(mPlayerEntity);
                        cp.healthCurrent = h.current; cp.healthMaximum = h.maximum;
                    }
                    if (mWorld.hasComponent<Needs>(mPlayerEntity))
                        cp.needs = mWorld.getComponent<Needs>(mPlayerEntity);
                    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
                        cp.mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                    if (mWorld.hasComponent<Wounds>(mPlayerEntity))
                        cp.wounds = mWorld.getComponent<Wounds>(mPlayerEntity).active;
                    cp.bleeding = mWorld.hasComponent<Bleeding>(mPlayerEntity);
                    if (mWorld.hasComponent<Inventory>(mPlayerEntity))
                        cp.inventory = mWorld.getComponent<Inventory>(mPlayerEntity);
                    cp.kills = mRunStats.kills;
                    cp.itemsLooted = mRunStats.itemsLooted;
                    cp.structuresBuilt = mRunStats.structuresBuilt;
                    cp.distanceWalked = mRunStats.distanceWalked;
                    mSaveManager.saveCheckpoint(cp);
                    mSaveManager.saveLootedPositions();
                }
            }
            mInteractionMessageTimer = 3.0f;
            mWorld.removeComponent<Sleeping>(mPlayerEntity);
        }
    }

    // Sleep debt collapse: force sleep when exhaustion maxes out
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Needs>(mPlayerEntity) &&
        !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        if (needs.sleepDebt >= needs.maxSleepDebt)
        {
            Sleeping collapse{};
            collapse.durationRemaining = 3.0f;
            collapse.quality = 0.4f;
            mWorld.addComponent<Sleeping>(mPlayerEntity, collapse);
            mInteractionMessage = "Collapsed from exhaustion!";
            mInteractionMessageTimer = 4.0f;
        }
    }

    playerAlive = false;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        playerAlive = health.current > 0.0f;
    }

    if (!playerAlive)
    {
        std::string cause = "Killed by zombies";
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
        {
            const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            if (needs.thirst <= 0.0f)
            {
                cause = "Dehydrated";
            }
            else if (needs.hunger <= 0.0f)
            {
                cause = "Starved";
            }
        }
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Bleeding>(mPlayerEntity))
        {
            cause = "Bled out";
        }

        beginDeathState(cause + " on Day " + std::to_string(mCurrentDay));
        return;
    }

    mProximityPrompt.clear();

    if (playerAlive && !mShowCrafting && !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        // Update proximity prompt for HUD display
        const ProximityPrompt prompt = mInteractionSystem.getProximityPrompt(mWorld, mPlayerEntity);
        mProximityPrompt = prompt.text;

        // Intercept container interactions to open container view instead of auto-loot
        bool interceptedContainer = false;
        if (mInput.wasKeyPressed(SDL_SCANCODE_E) && prompt.entity != kInvalidEntity &&
            mWorld.hasComponent<Interactable>(prompt.entity))
        {
            const Interactable& inter = mWorld.getComponent<Interactable>(prompt.entity);
            if (inter.type == InteractableType::Container && mWorld.hasComponent<Inventory>(prompt.entity))
            {
                mContainerOpen = true;
                mOpenContainerEntity = prompt.entity;
                mContainerCursor = 0;
                mProximityPrompt.clear();
                interceptedContainer = true;
            }
        }

        if (!interceptedContainer)
        {
            const std::string interactionMessage = mInteractionSystem.update(mWorld, mInput, mPlayerEntity, &mItemDatabase);
            if (!interactionMessage.empty())
            {
                mInteractionMessage = interactionMessage;
                mInteractionMessageTimer = 4.0f;

                if (interactionMessage.find("Ate") != std::string::npos ||
                    interactionMessage.find("Drank") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::FoodEat, 0.5f);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("Picked up") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::WeaponHit, 0.4f);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("bandage") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::FoodEat, 0.4f);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("Looted") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::WeaponHit, 0.3f);
                    auto pos = interactionMessage.find("Looted ");
                    if (pos != std::string::npos)
                    {
                        int n = std::atoi(interactionMessage.c_str() + pos + 7);
                        mRunStats.itemsLooted += std::max(1, n);
                    }
                }
            }
        }
    }

    // --- NPC Survivor Interaction Keybinds ---
    if (playerAlive && !mShowCrafting && !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        // O key: observe nearest survivor
        if (mInput.wasKeyPressed(SDL_SCANCODE_O))
        {
            auto result = mSurvivorInteractionSystem.tryObserve(mWorld, mPlayerEntity);
            if (!result.message.empty())
            {
                mInteractionMessage = result.message;
                mInteractionMessageTimer = 4.0f;
            }
        }

        // G key: approach nearest survivor
        if (mInput.wasKeyPressed(SDL_SCANCODE_G))
        {
            auto result = mSurvivorInteractionSystem.tryApproach(mWorld, mPlayerEntity);
            if (!result.message.empty())
            {
                mInteractionMessage = result.message;
                mInteractionMessageTimer = 4.0f;
            }
        }

        // T key: trade with approached survivor (offers active hotbar slot)
        if (mInput.wasKeyPressed(SDL_SCANCODE_T) &&
            mSurvivorInteractionSystem.currentMode() == SurvivorInteractionMode::Approaching)
        {
            int slot = 0;
            if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                slot = mWorld.getComponent<Inventory>(mPlayerEntity).activeSlot;
            }
            auto result = mSurvivorInteractionSystem.tryTrade(mWorld, mPlayerEntity, mItemDatabase, slot);
            if (!result.message.empty())
            {
                mInteractionMessage = result.message;
                mInteractionMessageTimer = 4.0f;
            }

            if (result.tradeAccepted && mWorld.hasComponent<MentalState>(mPlayerEntity))
            {
                MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                mental.depression = std::max(0.0f, mental.depression - result.depressionRelief);
                mental.panic = std::max(0.0f, mental.panic - result.panicRelief);
            }
        }
    }

    // Track pre-combat state for audio triggers
    const float prevPlayerHp = (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
        ? mWorld.getComponent<Health>(mPlayerEntity).current
        : 0.0f;
    const AttackPhase prevPhase = (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Combat>(mPlayerEntity))
        ? mWorld.getComponent<Combat>(mPlayerEntity).phase
        : AttackPhase::Idle;

    if (!mShowCrafting)
    {
        mCombatSystem.update(mWorld, mInput, mTileMap, mNoiseModel, dtSeconds);
    }
    else if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
        combat.phase = AttackPhase::Idle;
        combat.phaseTimer = 0.0f;
        combat.recoveryDelayTimer = 0.0f;
        combat.hitProcessed = false;
        combat.grabState = GrabState::None;
        combat.grabTarget = kInvalidEntity;
        combat.grabTimer = 0.0f;
    }

    // Desensitisation gain: check for zombie deaths after combat
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        mWorld.forEach<ZombieAI, Health>(
            [&](Entity, ZombieAI&, Health& h)
            {
                // Zombie just died this frame (low HP after being hit)
                if (h.current <= 0.0f && h.current > -999.0f)
                {
                    mental.desensitisation = std::min(100.0f, mental.desensitisation + 1.5f);
                    ++mRunStats.kills;
                    h.current = -1000.0f; // mark as counted
                }
            });
    }

    // Combat audio
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        const Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
        if (combat.phase == AttackPhase::Windup && prevPhase == AttackPhase::Idle)
        {
            mAudioManager.playSound(SoundId::WeaponSwing, 0.5f);
        }
    }
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const float curHp = mWorld.getComponent<Health>(mPlayerEntity).current;
        if (curHp < prevPlayerHp && curHp > 0.0f)
        {
            mAudioManager.playSound(SoundId::PlayerHurt, 0.6f);
        }
    }

    mZombieAISystem.update(
        mWorld,
        mTileMap,
        mNoiseModel,
        mPlayerEntity,
        dtSeconds,
        mGameHours,
        mCurrentDay,
        &mDistrictInfectionSystem);

    // Zombie audio: growls from chasing zombies, attack sounds
    mZombieGrowlTimer = std::max(0.0f, mZombieGrowlTimer - dtSeconds);
    if (mZombieGrowlTimer <= 0.0f && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 pc(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);

        mWorld.forEach<ZombieAI, Transform, Health>(
            [&](Entity, ZombieAI& zai, Transform& zt, Health& zh)
            {
                if (mZombieGrowlTimer > 0.0f) return;
                if (zh.current <= 0.0f) return;
                if (zai.state != ZombieState::Chase && zai.state != ZombieState::Attack) return;

                const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                const float dist = glm::distance(pc, zc);
                if (dist < 256.0f)
                {
                    const SoundId sound = (zai.state == ZombieState::Attack) ? SoundId::ZombieAttack : SoundId::ZombieGrowl;
                    mAudioManager.playSoundAtWorld(sound, zc.x, pc.x, static_cast<float>(mWindowWidth), 0.5f);
                    mZombieGrowlTimer = 3.0f;
                }
            });
    }
    if (mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        vel.dx = 0.0f;
        vel.dy = 0.0f;
    }

    mMovementSystem.update(mWorld, dtSeconds);
    mProjectileSystem.update(mWorld, mTileMap, mNoiseModel, dtSeconds);
    mCollisionSystem.update(mWorld, mTileMap, dtSeconds);

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        if (health.current <= 0.0f)
        {
            beginDeathState("Killed by zombies on Day " + std::to_string(mCurrentDay));
            return;
        }
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 playerCenter(
            playerTransform.x + kSpriteSize * 0.5f,
            playerTransform.y + kSpriteSize * 0.5f);

        // Track distance walked
        const float dx = playerTransform.x - mRunStats.prevX;
        const float dy = playerTransform.y - mRunStats.prevY;
        mRunStats.distanceWalked += std::sqrt(dx * dx + dy * dy);
        mRunStats.prevX = playerTransform.x;
        mRunStats.prevY = playerTransform.y;

        mCamera.follow(playerCenter, dtSeconds, 7.5f);
        mCamera.clampToBounds(mTileMap.pixelSize());
    }

    // Update player animation based on movement and facing
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Animation>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        Animation& anim = mWorld.getComponent<Animation>(mPlayerEntity);
        const Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        const Facing& fac = mWorld.getComponent<Facing>(mPlayerEntity);

        const float speed = vel.dx * vel.dx + vel.dy * vel.dy;
        std::string desired = "player_idle";
        if (speed > 4.0f)
        {
            if (std::abs(vel.dx) > std::abs(vel.dy))
            {
                desired = "player_walk_right";
            }
            else if (vel.dy < 0.0f)
            {
                desired = "player_walk_up";
            }
            else
            {
                desired = "player_walk_down";
            }
        }
        else if (fac.crouching)
        {
            desired = "player_crouch";
        }

        if (anim.currentAnim != desired)
        {
            anim.currentAnim = desired;
            anim.currentFrame = 0;
            anim.frameTimer = 0.0f;
            anim.finished = false;
        }
    }

    // Update zombie animations based on movement
    for (const Entity entity : mWorld.livingEntities())
    {
        if (!mWorld.hasComponent<ZombieAI>(entity) ||
            !mWorld.hasComponent<Animation>(entity) ||
            !mWorld.hasComponent<Velocity>(entity))
        {
            continue;
        }

        Animation& anim = mWorld.getComponent<Animation>(entity);
        const Velocity& vel = mWorld.getComponent<Velocity>(entity);

        const float speed = vel.dx * vel.dx + vel.dy * vel.dy;
        std::string desired = "zombie_idle";
        if (speed > 1.0f)
        {
            if (std::abs(vel.dx) > std::abs(vel.dy))
            {
                desired = "zombie_walk_right";
            }
            else if (vel.dy < 0.0f)
            {
                desired = "zombie_walk_up";
            }
            else
            {
                desired = "zombie_walk_down";
            }
        }

        if (anim.currentAnim != desired)
        {
            anim.currentAnim = desired;
            anim.currentFrame = 0;
            anim.frameTimer = 0.0f;
            anim.finished = false;
        }
    }

    mAnimationSystem.update(mWorld, mSpriteSheet, dtSeconds);

    // Particle system update
    mParticleSystem.update(dtSeconds);

    // Spawn particles: blood on entity damage
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const float curHp = mWorld.getComponent<Health>(mPlayerEntity).current;
        if (curHp < prevPlayerHp && curHp > 0.0f)
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            mParticleSystem.spawnBlood(
                glm::vec2(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f), 10);
        }
    }

    // Spawn particles: dust when player moves
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        const float speed = vel.dx * vel.dx + vel.dy * vel.dy;
        if (speed > 100.0f)
        {
            static float dustTimer = 0.0f;
            dustTimer += dtSeconds;
            if (dustTimer > 0.15f)
            {
                dustTimer = 0.0f;
                const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
                mParticleSystem.spawnDust(
                    glm::vec2(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize), 2);
            }
        }
    }

    // Spawn particles: campfire embers
    {
        static float emberTimer = 0.0f;
        emberTimer += dtSeconds;
        if (emberTimer > 0.3f)
        {
            emberTimer = 0.0f;
            mWorld.forEach<Structure, Transform>(
                [&](Entity, Structure& structure, Transform& transform)
                {
                    if (structure.type != StructureType::Campfire || structure.hp <= 0.0f)
                    {
                        return;
                    }
                    mParticleSystem.spawnEmbers(
                        glm::vec2(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f), 2);
                });
        }
    }

    updateNoiseModel(dtSeconds);
}

void Game::render()
{
    glViewport(0, 0, mWindowWidth, mWindowHeight);
    glClearColor(0.08f, 0.11f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mShader.use();
    mShader.setMat4("uProjection", mProjection);
    mShader.setInt("uTexture", 0);
    setupLightingUniforms();

    // Pass 1: Map tiles (tileset texture)
    mSpriteBatch.begin();
    renderMap();
    mSpriteBatch.end();

    mTexture.bind(GL_TEXTURE0);
    mSpriteBatch.flush();

    // Pass 2: Entity sprites + particles (spritesheet texture, with lighting)
    mSpriteBatch.begin();
    mRenderSystem.render(mWorld, mSpriteBatch, mCamera, mInterpolationAlpha);

    // Particles (rendered in world space, affected by lighting)
    const glm::vec4 solidWhiteUv = mSpriteSheet.uvRect("solid_white");
    mParticleSystem.render(mSpriteBatch, mCamera, solidWhiteUv);

    // Build mode ghost preview
    if (mBuildSystem.isBuildMode())
    {
        const int recipeIdx = mBuildSystem.selectedRecipe();
        if (recipeIdx >= 0 && recipeIdx < BuildSystem::kRecipeCount)
        {
            const auto& recipe = BuildSystem::kRecipes[recipeIdx];
            const glm::vec4 ghostUv = mSpriteSheet.uvRect(recipe.sprite);
            const glm::vec2 ghostWorld = mBuildSystem.ghostPosition() + glm::vec2(kSpriteSize * 0.5f);
            const glm::vec2 ghostScreen = mCamera.worldToScreen(ghostWorld);
            const glm::vec4 ghostColor = mBuildSystem.ghostValid()
                ? glm::vec4(0.2f, 0.9f, 0.3f, 0.55f)
                : glm::vec4(0.9f, 0.2f, 0.2f, 0.55f);
            const float ghostSize = 32.0f * mCamera.zoom();
            mSpriteBatch.draw(
                glm::vec2(ghostScreen.x - ghostSize * 0.5f, ghostScreen.y - ghostSize * 0.5f),
                glm::vec2(ghostSize, ghostSize), ghostUv, ghostColor);
        }
    }

    mSpriteBatch.end();

    mSpriteSheet.bindTexture(GL_TEXTURE0);
    mSpriteBatch.flush();

    // Pass 2.5: HUD overlays (spritesheet texture, no lighting — zoom-independent)
    mShader.setFloat("uAmbientLight", 1.0f);
    mShader.setVec3("uAmbientColor", glm::vec3(1.0f));
    mShader.setInt("uLightCount", 0);

    mSpriteBatch.begin();
    renderTitleScreen();
    renderNeedsHudOverlay();
    renderAttackArcOverlay();
    renderNoiseDebugOverlay();
    renderDeathOverlay();
    renderRetirementOverlay();
    renderPauseOverlay();
    mCraftingRenderTextPass = false;
    renderCraftingOverlay();
    mSpriteBatch.end();

    mSpriteSheet.bindTexture(GL_TEXTURE0);
    mSpriteBatch.flush();

    // Pass 3: text overlays (font atlas texture, no lighting)
    mShader.setFloat("uAmbientLight", 1.0f);
    mShader.setVec3("uAmbientColor", glm::vec3(1.0f));
    mShader.setInt("uLightCount", 0);

    mSpriteBatch.begin();
    if (!mShowCrafting)
    {
        renderTextOverlays();
    }
    renderInventoryOverlay();
    renderContainerOverlay();
    renderBuildModeOverlay();
    mCraftingRenderTextPass = true;
    renderCraftingOverlay();
    if (!mShowCrafting)
    {
        renderControlsOverlay();
    }
    mSpriteBatch.end();

    mFont.bindTexture(GL_TEXTURE0);
    mSpriteBatch.flush();
}

void Game::renderNeedsHudOverlay()
{
    if (mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        return;
    }

    const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");

    // Panel background for all needs bars
    const glm::vec2 panelPos(12.0f, 12.0f);
    const glm::vec2 panelSize(180.0f, 142.0f);
    mSpriteBatch.draw(panelPos, panelSize, uiUv, glm::vec4(0.04f, 0.06f, 0.08f, 0.55f));

    struct NeedBar
    {
        const char* iconName;
        float ratio;
        glm::vec4 fillColor;
        glm::vec4 iconTint;
    };

    const float hungerRatio = needs.maxHunger > 0.0f ? std::clamp(needs.hunger / needs.maxHunger, 0.0f, 1.0f) : 0.0f;
    const float thirstRatio = needs.maxThirst > 0.0f ? std::clamp(needs.thirst / needs.maxThirst, 0.0f, 1.0f) : 0.0f;
    const float fatigueRatio = 1.0f - (needs.maxFatigue > 0.0f ? std::clamp(needs.fatigue / needs.maxFatigue, 0.0f, 1.0f) : 0.0f);
    const float sleepRatio = 1.0f - (needs.maxSleepDebt > 0.0f ? std::clamp(needs.sleepDebt / needs.maxSleepDebt, 0.0f, 1.0f) : 0.0f);
    const float tempRatio = std::clamp((needs.bodyTemperature - 33.0f) / 6.0f, 0.0f, 1.0f);
    const float wellnessRatio = 1.0f - std::clamp(needs.illness / 100.0f, 0.0f, 1.0f);

    const NeedBar bars[] = {
        {"ui_hunger", hungerRatio, glm::vec4(0.92f, 0.72f, 0.20f, 0.95f), glm::vec4(1.0f, 0.95f, 0.75f, 1.0f)},
        {"ui_thirst", thirstRatio, glm::vec4(0.30f, 0.65f, 0.95f, 0.95f), glm::vec4(0.75f, 0.90f, 1.0f, 1.0f)},
        {"ui_stamina", fatigueRatio, glm::vec4(0.35f, 0.85f, 0.45f, 0.95f), glm::vec4(0.70f, 1.0f, 0.75f, 1.0f)},
        {"ui_sleep", sleepRatio, glm::vec4(0.60f, 0.50f, 0.85f, 0.95f), glm::vec4(0.85f, 0.75f, 1.0f, 1.0f)},
        {"ui_heart", tempRatio, glm::vec4(0.85f, 0.45f, 0.30f, 0.95f), glm::vec4(1.0f, 0.75f, 0.65f, 1.0f)},
        {"ui_skull", wellnessRatio, glm::vec4(0.70f, 0.70f, 0.70f, 0.95f), glm::vec4(0.90f, 0.90f, 0.90f, 1.0f)},
    };

    constexpr float kBarHeight = 10.0f;
    constexpr float kBarWidth = 120.0f;
    constexpr float kRowSpacing = 20.0f;
    constexpr float kIconSize = 16.0f;

    for (int i = 0; i < 6; ++i)
    {
        const float yOff = panelPos.y + 10.0f + static_cast<float>(i) * kRowSpacing;
        const glm::vec4 iconUv = mSpriteSheet.uvRect(bars[i].iconName);
        mSpriteBatch.draw(glm::vec2(panelPos.x + 10.0f, yOff - 2.0f), glm::vec2(kIconSize, kIconSize), iconUv, bars[i].iconTint);

        const glm::vec2 barPos(panelPos.x + 34.0f, yOff);
        mSpriteBatch.draw(barPos, glm::vec2(kBarWidth, kBarHeight), uiUv, glm::vec4(0.08f, 0.08f, 0.08f, 0.85f));
        mSpriteBatch.draw(barPos, glm::vec2(kBarWidth * bars[i].ratio, kBarHeight), uiUv, bars[i].fillColor);
    }

    // Sleeping indicator
    if (mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        const float yOff = panelPos.y + panelSize.y + 4.0f;
        mSpriteBatch.draw(glm::vec2(panelPos.x, yOff), glm::vec2(180.0f, 18.0f), uiUv, glm::vec4(0.15f, 0.10f, 0.25f, 0.7f));
    }

    // Bleeding indicator
    if (mWorld.hasComponent<Bleeding>(mPlayerEntity))
    {
        const float yOff = panelPos.y + panelSize.y + 24.0f;
        mSpriteBatch.draw(glm::vec2(panelPos.x, yOff), glm::vec2(180.0f, 18.0f), uiUv, glm::vec4(0.4f, 0.05f, 0.05f, 0.7f));
    }
}

void Game::renderDeathOverlay()
{
    if (mRunState != RunState::Dead)
    {
        return;
    }

    const glm::vec4 uiUvRect = mSpriteSheet.uvRect("solid_white");

    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)),
        uiUvRect,
        glm::vec4(0.08f, 0.02f, 0.02f, 0.65f));

    const glm::vec2 panelSize(360.0f, 120.0f);
    const glm::vec2 panelPos(
        (static_cast<float>(mWindowWidth) - panelSize.x) * 0.5f,
        (static_cast<float>(mWindowHeight) - panelSize.y) * 0.5f);
    mSpriteBatch.draw(panelPos, panelSize, uiUvRect, glm::vec4(0.05f, 0.05f, 0.05f, 0.85f));

    const glm::vec4 graveUvRect = mSpriteSheet.uvRect("env_grave");
    mSpriteBatch.draw(panelPos + glm::vec2(20.0f, 28.0f), glm::vec2(64.0f, 64.0f), graveUvRect, glm::vec4(0.85f, 0.88f, 0.95f, 1.0f));

    if (mDeathStateTimer < kDeathRestartDelaySeconds)
    {
        mSpriteBatch.draw(panelPos + glm::vec2(104.0f, 36.0f), glm::vec2(220.0f, 14.0f), uiUvRect, glm::vec4(0.65f, 0.15f, 0.15f, 0.9f));
    }
    else
    {
        mSpriteBatch.draw(panelPos + glm::vec2(104.0f, 36.0f), glm::vec2(220.0f, 14.0f), uiUvRect, glm::vec4(0.25f, 0.55f, 0.25f, 0.9f));
    }
}

void Game::renderRetirementOverlay()
{
    if (mRunState != RunState::Retired)
    {
        return;
    }

    const glm::vec4 uiUvRect = mSpriteSheet.uvRect("solid_white");

    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)),
        uiUvRect,
        glm::vec4(0.05f, 0.05f, 0.12f, 0.60f));

    const glm::vec2 panelSize(360.0f, 120.0f);
    const glm::vec2 panelPos(
        (static_cast<float>(mWindowWidth) - panelSize.x) * 0.5f,
        (static_cast<float>(mWindowHeight) - panelSize.y) * 0.5f);
    mSpriteBatch.draw(panelPos, panelSize, uiUvRect, glm::vec4(0.06f, 0.06f, 0.10f, 0.85f));

    if (mDeathStateTimer >= kDeathRestartDelaySeconds)
    {
        mSpriteBatch.draw(panelPos + glm::vec2(20.0f, 90.0f), glm::vec2(320.0f, 14.0f), uiUvRect, glm::vec4(0.25f, 0.45f, 0.55f, 0.9f));
    }
}

void Game::renderDayNightOverlay()
{
    // Day/night is now handled by the shader lighting system.
    // This function is kept as a no-op for compatibility.
}

void Game::snapshotPositions()
{
    mWorld.forEach<Transform>(
        [](Entity, Transform& t)
        {
            t.prevX = t.x;
            t.prevY = t.y;
        });
}

void Game::setupLightingUniforms()
{
    // Compute ambient light level from time of day
    float ambientLevel = 1.0f;
    glm::vec3 ambientColor(1.0f, 1.0f, 1.0f);

    const double hour = mGameHours;
    if (hour >= 6.0 && hour < 18.0)
    {
        ambientLevel = 1.0f;
        ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
    }
    else if (hour >= 18.0 && hour < 20.0)
    {
        const float t = static_cast<float>((hour - 18.0) / 2.0);
        ambientLevel = 1.0f - t * 0.65f;
        ambientColor = glm::vec3(
            1.0f - t * 0.3f,
            1.0f - t * 0.35f,
            1.0f - t * 0.1f);
    }
    else if (hour >= 20.0 || hour < 5.0)
    {
        ambientLevel = 0.35f;
        ambientColor = glm::vec3(0.7f, 0.7f, 0.9f);
    }
    else // 5.0 <= hour < 6.0 (dawn)
    {
        const float t = static_cast<float>((hour - 5.0) / 1.0);
        ambientLevel = 0.35f + t * 0.65f;
        ambientColor = glm::vec3(
            0.7f + t * 0.3f,
            0.7f + t * 0.3f,
            0.9f + t * 0.1f);
    }

    mShader.setFloat("uAmbientLight", ambientLevel);
    mShader.setVec3("uAmbientColor", ambientColor);

    // Gather point lights from campfire structures
    constexpr int kMaxLights = 16;
    glm::vec2 lightPositions[kMaxLights];
    glm::vec3 lightColors[kMaxLights];
    float lightRadii[kMaxLights];
    int lightCount = 0;

    mWorld.forEach<Structure, Transform>(
        [&](Entity, Structure& structure, Transform& transform)
        {
            if (lightCount >= kMaxLights)
            {
                return;
            }

            if (structure.type != StructureType::Campfire || structure.hp <= 0.0f)
            {
                return;
            }

            const glm::vec2 worldCenter(
                transform.x + kSpriteSize * 0.5f,
                transform.y + kSpriteSize * 0.5f);
            const glm::vec2 screenPos = mCamera.worldToScreen(worldCenter);

            lightPositions[lightCount] = screenPos;
            lightColors[lightCount] = glm::vec3(1.0f, 0.7f, 0.3f);
            lightRadii[lightCount] = 150.0f * mCamera.zoom();
            ++lightCount;
        });

    mShader.setInt("uLightCount", lightCount);
    if (lightCount > 0)
    {
        mShader.setVec2Array("uLightPositions[0]", lightPositions, lightCount);
        mShader.setVec3Array("uLightColors[0]", lightColors, lightCount);
        mShader.setFloatArray("uLightRadii[0]", lightRadii, lightCount);
    }
}

void Game::renderTextOverlays()
{
    constexpr float kTextScale = 2.0f;
    constexpr float kSmallScale = 1.5f;

    // Needs status text labels (positioned below the HUD bar panel)
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        float statusY = 160.0f;
        const glm::vec4 warnColor(1.0f, 0.6f, 0.3f, 1.0f);

        if (needs.hunger <= 10.0f)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "STARVING", warnColor, kSmallScale);
            statusY += 16.0f;
        }
        if (needs.thirst <= 10.0f)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "DEHYDRATED", glm::vec4(0.4f, 0.7f, 1.0f, 1.0f), kSmallScale);
            statusY += 16.0f;
        }
        if (needs.fatigue > 80.0f)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "EXHAUSTED", glm::vec4(0.5f, 0.9f, 0.5f, 1.0f), kSmallScale);
            statusY += 16.0f;
        }
        if (needs.sleepDebt > 60.0f)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "SLEEP-DEPRIVED", glm::vec4(0.7f, 0.6f, 0.9f, 1.0f), kSmallScale);
            statusY += 16.0f;
        }
        if (needs.bodyTemperature < 35.5f)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "HYPOTHERMIC", glm::vec4(0.6f, 0.8f, 1.0f, 1.0f), kSmallScale);
            statusY += 16.0f;
        }
        if (needs.illness > 20.0f)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "SICK", glm::vec4(0.75f, 0.75f, 0.55f, 1.0f), kSmallScale);
            statusY += 16.0f;
        }
        if (mWorld.hasComponent<Sleeping>(mPlayerEntity))
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "SLEEPING...", glm::vec4(0.6f, 0.5f, 0.85f, 0.9f), kSmallScale);
            statusY += 16.0f;
        }
        if (mWorld.hasComponent<Bleeding>(mPlayerEntity))
        {
            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "BLEEDING", glm::vec4(0.9f, 0.2f, 0.2f, 1.0f), kSmallScale);
            statusY += 16.0f;
        }

        // Wound indicators
        if (mWorld.hasComponent<Wounds>(mPlayerEntity))
        {
            const Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
            int bites = 0, cuts = 0, bruises = 0;
            bool anyInfected = false;
            for (const auto& w : wounds.active)
            {
                if (w.type == WoundType::Bite) ++bites;
                else if (w.type == WoundType::Cut) ++cuts;
                else ++bruises;
                if (w.infected) anyInfected = true;
            }
            if (bites > 0)
            {
                std::string label = "BITE x" + std::to_string(bites);
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), label, glm::vec4(0.85f, 0.25f, 0.35f, 1.0f), kSmallScale);
                statusY += 16.0f;
            }
            if (cuts > 0)
            {
                std::string label = "CUT x" + std::to_string(cuts);
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), label, glm::vec4(0.9f, 0.5f, 0.3f, 1.0f), kSmallScale);
                statusY += 16.0f;
            }
            if (bruises > 0)
            {
                std::string label = "BRUISE x" + std::to_string(bruises);
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), label, glm::vec4(0.6f, 0.5f, 0.8f, 1.0f), kSmallScale);
                statusY += 16.0f;
            }
            if (anyInfected)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "INFECTED!", glm::vec4(0.4f, 0.9f, 0.2f, 1.0f), kSmallScale);
                statusY += 16.0f;
            }
        }

        // Mental state warnings
        if (mWorld.hasComponent<MentalState>(mPlayerEntity))
        {
            const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
            if (mental.panic >= 75.0f)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "PANICKING!", glm::vec4(1.0f, 0.3f, 0.1f, 1.0f), kSmallScale);
                statusY += 16.0f;
            }
            else if (mental.panic >= 25.0f)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "ANXIOUS", glm::vec4(0.9f, 0.7f, 0.3f, 0.9f), kSmallScale);
                statusY += 16.0f;
            }
            if (mental.depression >= 60.0f)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "DEPRESSED", glm::vec4(0.5f, 0.5f, 0.7f, 1.0f), kSmallScale);
                statusY += 16.0f;
            }
            if (mental.desensitisation >= 50.0f)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "NUMB", glm::vec4(0.6f, 0.6f, 0.6f, 0.85f), kSmallScale);
                statusY += 16.0f;
            }
        }

        // Encumbrance warning
        if (mWorld.hasComponent<Inventory>(mPlayerEntity))
        {
            const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
            const float weight = InventorySystem::totalWeight(inv, mItemDatabase);
            if (weight > Inventory::kMaxCarryWeight)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(12.0f, statusY), "OVERBURDENED", glm::vec4(0.9f, 0.6f, 0.2f, 1.0f), kSmallScale);
                statusY += 16.0f;
            }
        }
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        std::string hpStr = "HP " + std::to_string(static_cast<int>(health.current)) + "/" + std::to_string(static_cast<int>(health.maximum));
        mFont.drawText(mSpriteBatch, glm::vec2(200.0f, 12.0f), hpStr, glm::vec4(0.85f, 0.35f, 0.35f, 1.0f), kSmallScale);
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        const Stamina& stamina = mWorld.getComponent<Stamina>(mPlayerEntity);
        std::string staStr = "STA " + std::to_string(static_cast<int>(stamina.current)) + "/" + std::to_string(static_cast<int>(stamina.maximum));
        mFont.drawText(mSpriteBatch, glm::vec2(200.0f, 28.0f), staStr, glm::vec4(0.35f, 0.75f, 0.85f, 1.0f), kSmallScale);
    }

    // Day / time display
    {
        const int hour = static_cast<int>(mGameHours) % 24;
        const int minute = static_cast<int>((mGameHours - std::floor(mGameHours)) * 60.0);
        std::ostringstream timeStr;
        timeStr << "DAY " << mCurrentDay << "  "
                << std::setfill('0') << std::setw(2) << hour << ":"
                << std::setfill('0') << std::setw(2) << minute;
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(static_cast<float>(mWindowWidth) - 12.0f, 12.0f),
            timeStr.str(),
            glm::vec4(0.9f, 0.9f, 0.8f, 1.0f),
            kSmallScale,
            TextAlign::Right);
    }

    // Character name
    mFont.drawText(
        mSpriteBatch,
        glm::vec2(static_cast<float>(mWindowWidth) - 12.0f, 30.0f),
        mCurrentCharacterName,
        glm::vec4(0.7f, 0.7f, 0.65f, 0.85f),
        kSmallScale,
        TextAlign::Right);

    // Trait display below character name
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        const float traitX = static_cast<float>(mWindowWidth) - 12.0f;
        float traitY = 48.0f;
        if (traits.positive != PositiveTrait::None)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(traitX, traitY), traits.positiveName(),
                            glm::vec4(0.4f, 0.85f, 0.4f, 0.8f), 1.0f, TextAlign::Right);
            traitY += 14.0f;
        }
        if (traits.negative != NegativeTrait::None)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(traitX, traitY), traits.negativeName(),
                            glm::vec4(0.85f, 0.4f, 0.4f, 0.8f), 1.0f, TextAlign::Right);
        }
    }

    // Objective display
    if (mRunState == RunState::Playing)
    {
        const float objX = static_cast<float>(mWindowWidth) - 12.0f;
        float objY = 80.0f;

        std::string primaryObj = "Search buildings for supply caches";
        std::string secondaryObj = "Standing still burns food, water, and daylight";
        glm::vec4 primaryColor(0.9f, 0.8f, 0.3f, 0.9f);

        const bool bleeding = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Bleeding>(mPlayerEntity);
        const bool wounded = mPlayerEntity != kInvalidEntity &&
            mWorld.hasComponent<Wounds>(mPlayerEntity) &&
            !mWorld.getComponent<Wounds>(mPlayerEntity).active.empty();

        if (mPlayerEntity != kInvalidEntity &&
            mWorld.hasComponent<Health>(mPlayerEntity) &&
            mWorld.getComponent<Health>(mPlayerEntity).current < mWorld.getComponent<Health>(mPlayerEntity).maximum * 0.85f)
        {
            primaryObj = "Find a medical cache";
            secondaryObj = bleeding ? "Stop bleeding before pushing farther" : "Patch up before the next fight";
            primaryColor = glm::vec4(0.95f, 0.5f, 0.45f, 0.95f);
        }
        else if (bleeding || wounded)
        {
            primaryObj = "Find a medical cache";
            secondaryObj = bleeding ? "Stop bleeding before nightfall" : "Treat wounds before they turn into illness";
            primaryColor = glm::vec4(0.95f, 0.5f, 0.45f, 0.95f);
        }
        else if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
        {
            const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            if (needs.thirst < 55.0f)
            {
                primaryObj = "Find water or a supply cache";
                secondaryObj = "Thirst will kill a static run fast";
                primaryColor = glm::vec4(0.45f, 0.75f, 0.95f, 0.95f);
            }
            else if (needs.hunger < 55.0f)
            {
                primaryObj = "Scavenge food before night";
                secondaryObj = "Sleeping through the day now costs supplies";
            }
            else if (mRunStats.itemsLooted < 6)
            {
                primaryObj = "Sweep nearby interiors";
                secondaryObj = "Look for med and supply caches before dusk";
            }
        }

        mFont.drawText(mSpriteBatch, glm::vec2(objX, objY), primaryObj,
                        primaryColor, 1.0f, TextAlign::Right);
        objY += 14.0f;

        mFont.drawText(mSpriteBatch, glm::vec2(objX, objY), secondaryObj,
                        glm::vec4(0.6f, 0.7f, 0.6f, 0.7f), 1.0f, TextAlign::Right);
        objY += 14.0f;

        std::string scavengeObj = "Looted: " + std::to_string(mRunStats.itemsLooted) + " | Kills: " + std::to_string(mRunStats.kills);
        mFont.drawText(mSpriteBatch, glm::vec2(objX, objY), scavengeObj,
                        glm::vec4(0.7f, 0.5f, 0.5f, 0.7f), 1.0f, TextAlign::Right);
    }

    // Current district infection indicator (always visible while playing).
    if (mRunState == RunState::Playing &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
        const int tileX = static_cast<int>(pt.x) / std::max(1, mTileMap.tileWidth());
        const int tileY = static_cast<int>(pt.y) / std::max(1, mTileMap.tileHeight());
        const int districtId = mTileMap.getDistrictId(tileX, tileY);

        if (districtId > 0)
        {
            float infection = mDistrictInfectionSystem.getInfection(districtId);
            const bool overwhelmed = mDistrictInfectionSystem.isOverwhelmed(districtId);
            int districtType = static_cast<int>(DistrictType::Wilderness);
            for (const auto& def : mMapGenerator.layoutData().districts)
            {
                if (def.id == districtId)
                {
                    districtType = def.type;
                    break;
                }
            }

            std::ostringstream districtLine;
            districtLine << "District " << districtId << " (" << districtTypeLabel(districtType)
                         << ") Infection " << static_cast<int>(std::round(infection)) << "%";
            if (overwhelmed)
            {
                districtLine << " OVERWHELMED";
            }

            mFont.drawText(
                mSpriteBatch,
                glm::vec2(static_cast<float>(mWindowWidth) - 12.0f, 140.0f),
                districtLine.str(),
                overwhelmed ? glm::vec4(0.95f, 0.45f, 0.35f, 0.95f) : glm::vec4(0.85f, 0.7f, 0.35f, 0.85f),
                1.0f,
                TextAlign::Right);
        }
    }

    // F3 infection debug: top infected districts for tuning and verification.
    if (mShowDistrictDebug && mRunState == RunState::Playing)
    {
        std::vector<DistrictInfection> sortedDistricts = mDistrictInfectionSystem.districts();
        std::sort(
            sortedDistricts.begin(),
            sortedDistricts.end(),
            [](const DistrictInfection& a, const DistrictInfection& b)
            {
                return a.infection > b.infection;
            });

        float debugY = 230.0f;
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(12.0f, debugY),
            "INFECTION DEBUG (F3)",
            glm::vec4(0.95f, 0.85f, 0.55f, 1.0f),
            1.5f);
        debugY += 18.0f;

        const auto& defs = mMapGenerator.layoutData().districts;
        const std::size_t listCount = std::min<std::size_t>(sortedDistricts.size(), 8);
        for (std::size_t i = 0; i < listCount; ++i)
        {
            const auto& entry = sortedDistricts[i];
            int districtType = static_cast<int>(DistrictType::Wilderness);
            for (const auto& def : defs)
            {
                if (def.id == entry.districtId)
                {
                    districtType = def.type;
                    break;
                }
            }

            std::ostringstream row;
            row << "#" << entry.districtId << " " << districtTypeLabel(districtType)
                << " " << static_cast<int>(std::round(entry.infection)) << "%";
            if (entry.overwhelmed)
            {
                row << " OVERWHELMED";
            }

            const glm::vec4 rowColor = entry.overwhelmed
                ? glm::vec4(0.95f, 0.45f, 0.35f, 1.0f)
                : glm::vec4(0.75f, 0.75f, 0.68f, 0.92f);

            mFont.drawText(mSpriteBatch, glm::vec2(12.0f, debugY), row.str(), rowColor, 1.0f);
            debugY += 14.0f;
        }
    }

    // Hotbar display (bottom of screen)
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity) && mRunState == RunState::Playing)
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const float hotbarY = static_cast<float>(mWindowHeight) - 26.0f;
        const float startX = (static_cast<float>(mWindowWidth) - 8.0f * 72.0f) * 0.5f;
        for (int i = 0; i < 8; ++i)
        {
            const auto& slot = inv.slots[static_cast<std::size_t>(i)];
            const float slotX = startX + static_cast<float>(i) * 72.0f;
            const bool isActive = (i == inv.activeSlot);
            const glm::vec4 slotColor = isActive ? glm::vec4(1.0f, 1.0f, 0.6f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.55f, 0.8f);
            std::string label = std::to_string(i + 1) + ":";
            if (slot.itemId >= 0 && slot.count > 0)
            {
                const ItemDef* item = mItemDatabase.find(slot.itemId);
                if (item)
                {
                    label += item->name.substr(0, 6);
                    if (slot.count > 1)
                    {
                        label += "x" + std::to_string(slot.count);
                    }
                }
            }
            else
            {
                label += "---";
            }
            mFont.drawText(mSpriteBatch, glm::vec2(slotX, hotbarY), label, slotColor, 1.0f);
        }
    }

    // Interaction message
    if (!mInteractionMessage.empty() && mInteractionMessageTimer > 0.0f)
    {
        const float alpha = std::min(1.0f, mInteractionMessageTimer / 0.5f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(static_cast<float>(mWindowWidth) * 0.5f,
                       static_cast<float>(mWindowHeight) - 60.0f),
            mInteractionMessage,
            glm::vec4(1.0f, 1.0f, 0.8f, alpha),
            kTextScale,
            TextAlign::Center);
    }

    // Proximity prompt (E: action)
    if (!mProximityPrompt.empty() && mRunState == RunState::Playing && mInteractionMessageTimer <= 0.0f)
    {
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(static_cast<float>(mWindowWidth) * 0.5f,
                       static_cast<float>(mWindowHeight) - 60.0f),
            mProximityPrompt,
            glm::vec4(0.9f, 0.9f, 0.7f, 0.85f),
            kTextScale,
            TextAlign::Center);
    }

    // Death overlay text
    if (mRunState == RunState::Dead)
    {
        const float centerX = static_cast<float>(mWindowWidth) * 0.5f;
        const float centerY = static_cast<float>(mWindowHeight) * 0.5f;

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 40.0f), "YOU DIED",
                        glm::vec4(0.9f, 0.2f, 0.2f, 1.0f), 3.0f, TextAlign::Center);

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 4.0f), mCurrentCharacterName,
                        glm::vec4(0.8f, 0.82f, 0.9f, 1.0f), kTextScale, TextAlign::Center);

        // Traits on death screen
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            std::string traitLine;
            if (traits.positive != PositiveTrait::None) traitLine += traits.positiveName();
            if (traits.negative != NegativeTrait::None)
            {
                if (!traitLine.empty()) traitLine += " / ";
                traitLine += traits.negativeName();
            }
            if (!traitLine.empty())
            {
                mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 12.0f), traitLine,
                                glm::vec4(0.65f, 0.65f, 0.55f, 0.8f), 1.0f, TextAlign::Center);
            }
        }

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 26.0f), mDeathCause,
                        glm::vec4(0.75f, 0.55f, 0.55f, 1.0f), kSmallScale, TextAlign::Center);

        // Run stats
        {
            const int distMeters = static_cast<int>(mRunStats.distanceWalked / 32.0f);
            std::string statsLine = "Day " + std::to_string(mCurrentDay)
                + " | " + std::to_string(mRunStats.kills) + " kills"
                + " | " + std::to_string(distMeters) + "m walked";
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 42.0f), statsLine,
                            glm::vec4(0.65f, 0.65f, 0.7f, 0.9f), 1.0f, TextAlign::Center);
        }

        if (mDeathStateTimer >= kDeathRestartDelaySeconds)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 60.0f), "Press Enter to begin again",
                            glm::vec4(0.5f, 0.85f, 0.5f, 1.0f), kSmallScale, TextAlign::Center);
        }
        else
        {
            const int remaining = static_cast<int>(std::ceil(kDeathRestartDelaySeconds - mDeathStateTimer));
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 60.0f),
                            "Wait " + std::to_string(remaining) + "...",
                            glm::vec4(0.6f, 0.4f, 0.4f, 0.8f), kSmallScale, TextAlign::Center);
        }
    }

    // Retirement overlay text
    if (mRunState == RunState::Retired)
    {
        const float centerX = static_cast<float>(mWindowWidth) * 0.5f;
        const float centerY = static_cast<float>(mWindowHeight) * 0.5f;

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 40.0f), "GAVE UP",
                        glm::vec4(0.5f, 0.5f, 0.7f, 1.0f), 3.0f, TextAlign::Center);

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 4.0f), mCurrentCharacterName,
                        glm::vec4(0.7f, 0.7f, 0.8f, 1.0f), kTextScale, TextAlign::Center);

        // Traits on retirement screen
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            std::string traitLine;
            if (traits.positive != PositiveTrait::None) traitLine += traits.positiveName();
            if (traits.negative != NegativeTrait::None)
            {
                if (!traitLine.empty()) traitLine += " / ";
                traitLine += traits.negativeName();
            }
            if (!traitLine.empty())
            {
                mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 12.0f), traitLine,
                                glm::vec4(0.55f, 0.55f, 0.5f, 0.8f), 1.0f, TextAlign::Center);
            }
        }

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 26.0f), mDeathCause,
                        glm::vec4(0.5f, 0.5f, 0.65f, 1.0f), kSmallScale, TextAlign::Center);

        // Run stats
        {
            const int distMeters = static_cast<int>(mRunStats.distanceWalked / 32.0f);
            std::string statsLine = "Day " + std::to_string(mCurrentDay)
                + " | " + std::to_string(mRunStats.kills) + " kills"
                + " | " + std::to_string(distMeters) + "m walked";
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 42.0f), statsLine,
                            glm::vec4(0.55f, 0.55f, 0.65f, 0.9f), 1.0f, TextAlign::Center);
        }

        if (mDeathStateTimer >= kDeathRestartDelaySeconds)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 60.0f), "Press Enter to try again",
                            glm::vec4(0.45f, 0.65f, 0.75f, 1.0f), kSmallScale, TextAlign::Center);
        }
        else
        {
            const int remaining = static_cast<int>(std::ceil(kDeathRestartDelaySeconds - mDeathStateTimer));
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 60.0f),
                            "Wait " + std::to_string(remaining) + "...",
                            glm::vec4(0.4f, 0.4f, 0.5f, 0.8f), kSmallScale, TextAlign::Center);
        }
    }

    // Pause overlay text
    if (mRunState == RunState::Paused)
    {
        const float centerX = static_cast<float>(mWindowWidth) * 0.5f;
        const float centerY = static_cast<float>(mWindowHeight) * 0.5f;

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 50.0f), "PAUSED",
                        glm::vec4(0.9f, 0.9f, 0.85f, 1.0f), 3.0f, TextAlign::Center);

        static const char* options[] = {"Resume", "Back to Menu", "Restart", "Quit"};
        for (int i = 0; i < 4; ++i)
        {
            const bool selected = (i == mPauseSelection);
            const glm::vec4 color = selected
                ? glm::vec4(1.0f, 1.0f, 0.7f, 1.0f)
                : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f);
            mFont.drawText(mSpriteBatch,
                glm::vec2(centerX, centerY + 10.0f + static_cast<float>(i) * 28.0f),
                options[i], color, kTextScale, TextAlign::Center);
        }
    }

    // Title screen briefing hub text
    if (mRunState == RunState::TitleScreen)
    {
        const TitlePanelLayout layout = computeTitlePanelLayout(mWindowWidth, mWindowHeight);
        const float leftX = layout.leftTextX;
        const float rightX = layout.rightTextX;
        const std::size_t menuCharBudget = approximateCharBudget(layout.menuRowW - 20.0f, 8.0f);
        const std::size_t rightCharBudget = approximateCharBudget(layout.rightCardTopW - 24.0f, 8.0f);
        const std::size_t rightBottomBudget = approximateCharBudget(layout.rightCardBottomW - 24.0f, 8.0f);

        const auto& graveHistory = mSaveManager.graves();
        const auto& retirementHistory = mSaveManager.retirements();
        const GraveRecord* lastGrave = graveHistory.empty() ? nullptr : &graveHistory.back();
        const RetirementRecord* lastRetirement = retirementHistory.empty() ? nullptr : &retirementHistory.back();
        const LatestLegacyEvent latestEvent = pickLatestLegacyEvent(graveHistory, retirementHistory);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + 16.0f),
            "DEAD PIXEL SURVIVAL",
            glm::vec4(0.95f, 0.36f, 0.30f, 1.0f),
            3.0f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + 48.0f),
            "FIELD BRIEFING // PRE-RUN STAGING",
            glm::vec4(0.84f, 0.78f, 0.66f, 0.95f),
            1.2f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + 75.0f),
            "Set slot, review intel, then deploy.",
            glm::vec4(0.72f, 0.74f, 0.69f, 0.92f),
            1.15f);

        float menuY = layout.menuRowY + 3.0f;
        for (int i = 0; i < static_cast<int>(kTitleMenuItems.size()); ++i)
        {
            std::string itemText = kTitleMenuItems[static_cast<std::size_t>(i)];
            if (i == 2)
            {
                itemText += "  < " + std::string(kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)]) + " >";
            }
            else if (i == 3)
            {
                itemText += "  < " + std::string(kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)]) + " >";
            }

            glm::vec4 color(0.83f, 0.83f, 0.80f, 0.96f);
            if (i == 0 && !mTitleHasCheckpoint)
            {
                color = glm::vec4(0.72f, 0.44f, 0.44f, 0.95f);
                itemText += " [NO CHECKPOINT]";
            }

            if (i == mTitleMenuSelection)
            {
                color = glm::vec4(1.0f, 0.94f, 0.72f, 1.0f);
                itemText = ">> " + itemText;
            }
            else
            {
                itemText = "   " + itemText;
            }

            mFont.drawText(
                mSpriteBatch,
                glm::vec2(leftX, menuY),
                truncateHudText(itemText, menuCharBudget),
                color,
                i == mTitleMenuSelection ? 1.65f : 1.45f);
            menuY += layout.menuRowStep;
        }

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + layout.panelH - 44.0f),
            "W/S Move  A/D Change  Enter Select  Esc Close/Quit",
            glm::vec4(0.74f, 0.75f, 0.71f, 0.95f),
            1.15f);

        float topY = layout.rightCardTopY + 10.0f;

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY),
            "ACTIVE SLOT",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.15f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 19.0f),
            truncateHudText(std::string(kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)]) + " [" + mActiveSaveSlot + "]", rightCharBudget),
            glm::vec4(0.92f, 0.92f, 0.86f, 0.98f),
            1.25f);

        const int completedRuns = std::max(0, mSaveManager.runNumber() - 1);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 42.0f),
            "Runs Completed: " + std::to_string(completedRuns),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 58.0f),
            "Best Streak: Day " + std::to_string(mSaveManager.longestRunDays()),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 74.0f),
            truncateHudText("Total Kills: " + std::to_string(mSaveManager.totalKills()) +
                "  |  Days Survived: " + std::to_string(mSaveManager.totalDaysSurvived()), rightCharBudget),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 90.0f),
            truncateHudText("Graves: " + std::to_string(graveHistory.size()) +
                "  |  Retirements: " + std::to_string(retirementHistory.size()), rightCharBudget),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);

        std::string checkpointLine = "Checkpoint: none";
        std::string checkpointLoc = "Location: --";
        if (mTitleHasCheckpoint)
        {
            checkpointLine = "Checkpoint Day " + std::to_string(mTitleCheckpointDay) +
                " // " + truncateHudText(mTitleCheckpointName, 16);
            checkpointLoc = "Location: " + formatTileLocation(mTitleCheckpointX, mTitleCheckpointY, mTileMap);
        }

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 114.0f),
            truncateHudText(checkpointLine, rightCharBudget),
            mTitleHasCheckpoint ? glm::vec4(0.68f, 0.90f, 0.70f, 0.98f) : glm::vec4(0.62f, 0.62f, 0.62f, 0.90f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 130.0f),
            truncateHudText(checkpointLoc, rightCharBudget),
            mTitleHasCheckpoint ? glm::vec4(0.66f, 0.83f, 0.68f, 0.95f) : glm::vec4(0.60f, 0.60f, 0.60f, 0.85f),
            1.0f);

        float midY = layout.rightCardMidY + 10.0f;

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY),
            "MODE PROFILE",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.15f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY + 19.0f),
            kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)],
            glm::vec4(0.95f, 0.91f, 0.78f, 0.98f),
            1.25f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY + 37.0f),
            truncateHudText(kTitleModeDescriptions[static_cast<std::size_t>(mTitleModeIndex)], rightCharBudget),
            glm::vec4(0.74f, 0.77f, 0.71f, 0.95f),
            1.08f);

        const std::size_t briefingIndex =
            (static_cast<std::size_t>(mSaveManager.worldSeed()) +
             static_cast<std::size_t>(mSaveManager.runNumber()) +
             static_cast<std::size_t>(mTitlePulseTimer * 0.4f)) % kBriefingLines.size();

        float bottomY = layout.rightCardBottomY + 10.0f;

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY),
            "BRIEFING NOTE",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.1f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 18.0f),
            truncateHudText(kBriefingLines[briefingIndex], rightBottomBudget),
            glm::vec4(0.77f, 0.80f, 0.74f, 0.98f),
            1.05f);

        std::string latestHeader = "Latest Event: none";
        std::string latestLineA = "No completed runs in this slot.";
        std::string latestLineB = "Start or continue a run to build history.";
        glm::vec4 latestColor(0.63f, 0.63f, 0.63f, 0.9f);

        if (latestEvent.kind == LegacyEventKind::Death && latestEvent.death != nullptr)
        {
            latestHeader = "Latest Event: DEATH (D" + std::to_string(latestEvent.death->day) + ")";
            latestLineA = truncateHudText(
                latestEvent.death->name + " @ " + formatTileLocation(latestEvent.death->x, latestEvent.death->y, mTileMap),
                rightBottomBudget);
            latestLineB = "Cause: " + truncateHudText(latestEvent.death->cause, rightBottomBudget > 7 ? rightBottomBudget - 7 : rightBottomBudget);
            latestColor = glm::vec4(0.88f, 0.66f, 0.66f, 0.98f);
        }
        else if (latestEvent.kind == LegacyEventKind::Retirement && latestEvent.retirement != nullptr)
        {
            latestHeader = "Latest Event: RETIREMENT (D" + std::to_string(latestEvent.retirement->day) + ")";
            latestLineA = truncateHudText(
                latestEvent.retirement->name + " @ " + formatTileLocation(latestEvent.retirement->x, latestEvent.retirement->y, mTileMap),
                rightBottomBudget);
            latestLineB = "Status: Survivor exited run";
            latestColor = glm::vec4(0.76f, 0.86f, 0.74f, 0.98f);
        }

        std::string secondaryLine = "Previous Event: none";
        if (latestEvent.kind == LegacyEventKind::Death && lastRetirement != nullptr)
        {
            secondaryLine = "Previous: RETIRE D" + std::to_string(lastRetirement->day) +
                " " + truncateHudText(lastRetirement->name, 12);
        }
        else if (latestEvent.kind == LegacyEventKind::Retirement && lastGrave != nullptr)
        {
            secondaryLine = "Previous: DEATH D" + std::to_string(lastGrave->day) +
                " " + truncateHudText(lastGrave->name, 12);
        }

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 42.0f),
            "LATEST SLOT EVENT",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.1f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 60.0f),
            truncateHudText(latestHeader, rightBottomBudget),
            latestColor,
            1.05f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 76.0f),
            truncateHudText(latestLineA, rightBottomBudget),
            glm::vec4(0.76f, 0.80f, 0.74f, 0.95f),
            1.0f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 92.0f),
            truncateHudText(latestLineB, rightBottomBudget),
            glm::vec4(0.72f, 0.74f, 0.71f, 0.95f),
            1.0f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 108.0f),
            truncateHudText(secondaryLine, rightBottomBudget),
            glm::vec4(0.68f, 0.70f, 0.67f, 0.92f),
            1.0f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, layout.panelY + layout.panelH - 42.0f),
            truncateHudText("World Seed: " + std::to_string(mSaveManager.worldSeed()), rightCharBudget),
            glm::vec4(0.67f, 0.69f, 0.66f, 0.90f),
            1.05f);

        if (!mTitleStatusMessage.empty() && mTitleStatusTimer > 0.0f)
        {
            mFont.drawText(
                mSpriteBatch,
                glm::vec2(layout.panelX + layout.panelW * 0.5f, layout.panelY + layout.panelH - 16.0f),
                truncateHudText(mTitleStatusMessage, 64),
                glm::vec4(0.99f, 0.86f, 0.58f, 0.98f),
                1.25f,
                TextAlign::Center);
        }

        if (mShowTitleCredits)
        {
            const float cx = layout.panelX + layout.panelW * 0.5f;
            const float cy = layout.panelY + layout.panelH * 0.5f;
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy - 52.0f), "CREDITS",
                            glm::vec4(0.96f, 0.86f, 0.60f, 1.0f), 2.2f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy - 22.0f), "Dead Pixel Survival",
                            glm::vec4(0.92f, 0.92f, 0.86f, 1.0f), 1.4f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy - 4.0f), "Code + Design: In Progress Build",
                            glm::vec4(0.80f, 0.82f, 0.78f, 0.98f), 1.1f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy + 14.0f), "Engine: C++20 / SDL2 / OpenGL",
                            glm::vec4(0.80f, 0.82f, 0.78f, 0.98f), 1.1f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy + 34.0f), "Press Credits again or Esc to close",
                            glm::vec4(0.72f, 0.74f, 0.70f, 0.95f), 1.05f, TextAlign::Center);
        }
    }
}

void Game::renderMap()
{
    const int tileW = mTileMap.tileWidth();
    const int tileH = mTileMap.tileHeight();

    if (tileW <= 0 || tileH <= 0)
    {
        return;
    }

    const int mapW = mTileMap.width();
    const int mapH = mTileMap.height();
    const glm::vec2 viewportSize = mCamera.viewportSize();
    const float zoom = mCamera.zoom();

    // Iso tile visual size (at zoom 1.0)
    constexpr float kIsoTileW = 64.0f;
    constexpr float kIsoTileH = 32.0f;
    const float scaledTileW = kIsoTileW * zoom;
    const float scaledTileH = kIsoTileH * zoom;
    const float margin = scaledTileW;

    // Render tiles back-to-front by sum of (col + row)
    for (int sum = 0; sum < mapW + mapH - 1; ++sum)
    {
        const int colStart = std::max(0, sum - (mapH - 1));
        const int colEnd = std::min(sum, mapW - 1);

        for (int col = colStart; col <= colEnd; ++col)
        {
            const int row = sum - col;

            const int gid = mTileMap.groundGid(col, row);
            if (gid == 0)
            {
                continue;
            }

            glm::vec4 uvRect;
            if (!mTileMap.gidToUvRect(gid, uvRect))
            {
                continue;
            }

            // World center of tile
            const glm::vec2 worldCenter(
                static_cast<float>(col * tileW) + static_cast<float>(tileW) * 0.5f,
                static_cast<float>(row * tileH) + static_cast<float>(tileH) * 0.5f);

            const glm::vec2 screenCenter = mCamera.worldToScreen(worldCenter);

            // Frustum cull
            if (screenCenter.x + scaledTileW * 0.5f < -margin ||
                screenCenter.y + scaledTileH * 0.5f < -margin ||
                screenCenter.x - scaledTileW * 0.5f > viewportSize.x + margin ||
                screenCenter.y - scaledTileH * 0.5f > viewportSize.y + margin)
            {
                continue;
            }

            // Draw tile centered on its iso screen position
            mSpriteBatch.draw(
                glm::vec2(screenCenter.x - scaledTileW * 0.5f,
                           screenCenter.y - scaledTileH * 0.5f),
                glm::vec2(scaledTileW, scaledTileH),
                uvRect,
                glm::vec4(1.0f));
        }
    }
}

void Game::renderAttackArcOverlay()
{
    if (mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Transform>(mPlayerEntity) ||
        !mWorld.hasComponent<Facing>(mPlayerEntity) ||
        !mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        return;
    }

    const Transform& transform = mWorld.getComponent<Transform>(mPlayerEntity);
    const Facing& facing = mWorld.getComponent<Facing>(mPlayerEntity);
    const Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);

    if (combat.phase != AttackPhase::Windup)
    {
        return;
    }

    float arcDegrees = combat.weapon.arcDegrees;
    float rangePixels = combat.weapon.rangePixels;
    if (combat.weapon.condition == WeaponCondition::Broken)
    {
        arcDegrees *= 0.7f;
        rangePixels *= 0.6f;
    }
    else if (combat.weapon.condition == WeaponCondition::Damaged)
    {
        rangePixels *= 0.85f;
    }

    const glm::vec2 playerCenter(
        transform.x + kSpriteSize * 0.5f,
        transform.y + kSpriteSize * 0.5f);

    const glm::vec4 debugUvRect = mSpriteSheet.uvRect("solid_white");

    const float halfArcRadians = glm::radians(arcDegrees * 0.5f);
    constexpr int segments = 16;

    const glm::vec4 baseColor = combat.swingWillMiss
        ? glm::vec4(0.9f, 0.3f, 0.3f, 0.65f)
        : glm::vec4(1.0f, 0.95f, 0.2f, 0.75f);

    for (int index = 0; index <= segments; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(segments);
        const float angle = facing.angleRadians - halfArcRadians + (halfArcRadians * 2.0f * t);

        const glm::vec2 direction(std::cos(angle), std::sin(angle));
        const glm::vec2 arcPointWorld = playerCenter + direction * rangePixels;
        const glm::vec2 arcPointScreen = mCamera.worldToScreen(arcPointWorld);

        const float dotSize = 10.0f * mCamera.zoom();
        mSpriteBatch.draw(
            arcPointScreen - glm::vec2(dotSize * 0.5f, dotSize * 0.5f),
            glm::vec2(dotSize, dotSize),
            debugUvRect,
            baseColor);
    }
}

void Game::updateNoiseModel(float dtSeconds)
{
    mNoiseModel.update(dtSeconds);

    if (mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Transform>(mPlayerEntity) ||
        !mWorld.hasComponent<Velocity>(mPlayerEntity) ||
        !mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        mMovementNoiseTier = NoiseTier::None;
        mNoiseEmitTimer = 0.0f;
        return;
    }

    const Transform& transform = mWorld.getComponent<Transform>(mPlayerEntity);
    const Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
    const Facing& facing = mWorld.getComponent<Facing>(mPlayerEntity);

    const float speedSquared = velocity.dx * velocity.dx + velocity.dy * velocity.dy;
    if (speedSquared < 4.0f)
    {
        mMovementNoiseTier = NoiseTier::None;
        mNoiseEmitTimer = 0.0f;
        return;
    }

    NoiseTier noiseTier = facing.running ? NoiseTier::Medium : NoiseTier::Soft;
    if (facing.crouching)
    {
        noiseTier = NoiseModel::adjustTier(noiseTier, -1);
    }

    const glm::vec2 feetPosition(
        transform.x + kSpriteSize * 0.5f,
        transform.y + kPlayerFootOffsetY);

    const int tileX = static_cast<int>(std::floor(feetPosition.x / static_cast<float>(mTileMap.tileWidth())));
    const int tileY = static_cast<int>(std::floor(feetPosition.y / static_cast<float>(mTileMap.tileHeight())));
    const SurfaceType surface = mTileMap.getSurfaceType(tileX, tileY);

    noiseTier = NoiseModel::adjustTier(noiseTier, surfaceNoiseModifier(surface));

    // Clumsy trait: +1 noise tier on footsteps
    if (mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        if (traits.negative == NegativeTrait::Clumsy)
        {
            noiseTier = NoiseModel::adjustTier(noiseTier, 1);
        }
    }

    mMovementNoiseTier = noiseTier;

    if (noiseTier == NoiseTier::None)
    {
        mNoiseEmitTimer = 0.0f;
        return;
    }

    const float emissionInterval = facing.running ? 0.14f : (facing.crouching ? 0.28f : 0.22f);

    mNoiseEmitTimer += dtSeconds;
    if (mNoiseEmitTimer < emissionInterval)
    {
        return;
    }

    mNoiseEmitTimer = 0.0f;
    mNoiseModel.emitNoise(feetPosition, noiseTier, tierDurationSeconds(noiseTier), mPlayerEntity);

    // Footstep audio
    SoundId footstep = SoundId::FootstepNormal;
    if (noiseTier == NoiseTier::Soft || facing.crouching)
    {
        footstep = SoundId::FootstepSoft;
    }
    else if (noiseTier == NoiseTier::Loud || noiseTier == NoiseTier::Explosive || facing.running)
    {
        footstep = SoundId::FootstepLoud;
    }
    mAudioManager.playSound(footstep, 0.12f);
}

void Game::renderNoiseDebugOverlay()
{
    if (!mShowNoiseDebug)
    {
        return;
    }

    const auto& events = mNoiseModel.activeEvents();
    if (events.empty())
    {
        return;
    }

    const int tileWidth = mTileMap.tileWidth();
    const int tileHeight = mTileMap.tileHeight();
    if (tileWidth <= 0 || tileHeight <= 0)
    {
        return;
    }

    const glm::vec4 debugUvRect = mSpriteSheet.uvRect("solid_white");

    const glm::vec2 viewportSize = mCamera.viewportSize();

    for (const NoiseEvent& event : events)
    {
        const float centerTileX = event.worldPosition.x / static_cast<float>(tileWidth);
        const float centerTileY = event.worldPosition.y / static_cast<float>(tileHeight);

        const int minTileX = static_cast<int>(std::floor(centerTileX - static_cast<float>(event.radiusTiles) - 1.0f));
        const int maxTileX = static_cast<int>(std::ceil(centerTileX + static_cast<float>(event.radiusTiles) + 1.0f));
        const int minTileY = static_cast<int>(std::floor(centerTileY - static_cast<float>(event.radiusTiles) - 1.0f));
        const int maxTileY = static_cast<int>(std::ceil(centerTileY + static_cast<float>(event.radiusTiles) + 1.0f));

        glm::vec4 color = NoiseModel::tierColor(event.tier);
        const float lifeAlpha = event.totalDurationSeconds > 0.0f
            ? event.remainingSeconds / event.totalDurationSeconds
            : 0.0f;
        color.a *= lifeAlpha;

        for (int tileY = minTileY; tileY <= maxTileY; ++tileY)
        {
            for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
            {
                if (tileX < 0 || tileX >= mTileMap.width() || tileY < 0 || tileY >= mTileMap.height())
                {
                    continue;
                }

                const float dx = (static_cast<float>(tileX) + 0.5f) - centerTileX;
                const float dy = (static_cast<float>(tileY) + 0.5f) - centerTileY;
                const float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < static_cast<float>(event.radiusTiles) - 0.65f ||
                    distance > static_cast<float>(event.radiusTiles) + 0.35f)
                {
                    continue;
                }

                const glm::vec2 worldCenter(
                    static_cast<float>(tileX * tileWidth) + static_cast<float>(tileWidth) * 0.5f,
                    static_cast<float>(tileY * tileHeight) + static_cast<float>(tileHeight) * 0.5f);
                const glm::vec2 screenCenter = mCamera.worldToScreen(worldCenter);

                const float scaledW = static_cast<float>(tileWidth) * mCamera.zoom();
                const float scaledH = static_cast<float>(tileHeight) * mCamera.zoom();

                if (screenCenter.x + scaledW * 0.5f < 0.0f ||
                    screenCenter.y + scaledH * 0.5f < 0.0f ||
                    screenCenter.x - scaledW * 0.5f > viewportSize.x ||
                    screenCenter.y - scaledH * 0.5f > viewportSize.y)
                {
                    continue;
                }

                mSpriteBatch.draw(
                    glm::vec2(screenCenter.x - scaledW * 0.5f, screenCenter.y - scaledH * 0.5f),
                    glm::vec2(scaledW, scaledH),
                    debugUvRect,
                    color);
            }
        }
    }
}

void Game::updateWindowTitle(double frameSeconds)
{
    mFpsTimer += frameSeconds;
    mFramesThisSecond += 1;

    if (mFpsTimer < 1.0)
    {
        return;
    }

    const double fps = static_cast<double>(mFramesThisSecond) / mFpsTimer;

    std::size_t zombieCount = 0;
    for (const Entity entity : mWorld.livingEntities())
    {
        if (mWorld.hasComponent<ZombieAI>(entity))
        {
            ++zombieCount;
        }
    }

    std::ostringstream title;
    title << "Dead Pixel Survival | FPS: "
          << std::fixed << std::setprecision(1) << fps
          << " | Entities: " << mWorld.livingEntities().size()
          << " | Zombies: " << zombieCount;

    if (mRunState == RunState::Dead)
    {
        title << " | DEAD";
    }

    title << " | F1 Noise: " << (mShowNoiseDebug ? "ON" : "OFF");

    if (!mDebugEntities.empty())
    {
        title << " | K: remove test entity";
    }

    SDL_SetWindowTitle(mWindow, title.str().c_str());

    mFpsTimer = 0.0;
    mFramesThisSecond = 0;
}

glm::vec2 Game::screenToWorld(const glm::vec2& screenPosition) const
{
    return mCamera.screenToWorld(screenPosition);
}

void Game::renderInventoryOverlay()
{
    if (!mShowInventory || mShowCrafting || mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }

    constexpr float kSmallScale = 1.5f;
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

    const float panelX = static_cast<float>(mWindowWidth) * 0.5f - 140.0f;
    float y = 60.0f;

    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "INVENTORY", glm::vec4(1.0f, 0.95f, 0.7f, 1.0f), 2.0f);
    y += 24.0f;

    const float weight = InventorySystem::totalWeight(inv, mItemDatabase);
    std::ostringstream weightStr;
    weightStr << std::fixed << std::setprecision(1) << weight << "/" << Inventory::kMaxCarryWeight << " kg";
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), weightStr.str(), glm::vec4(0.7f, 0.7f, 0.65f, 0.9f), kSmallScale);
    y += 18.0f;

    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const auto& slot = inv.slots[static_cast<std::size_t>(i)];
        std::string line;
        if (i < 8)
        {
            line = "[" + std::to_string(i + 1) + "] ";
        }
        else
        {
            line = "[" + std::to_string(i + 1) + "] ";
        }

        if (slot.itemId >= 0 && slot.count > 0)
        {
            const ItemDef* item = mItemDatabase.find(slot.itemId);
            if (item)
            {
                line += item->name;
                if (slot.count > 1)
                {
                    line += " x" + std::to_string(slot.count);
                }
                if (item->spoilHours > 0.0f && slot.condition < 0.9f)
                {
                    const int pct = static_cast<int>(slot.condition * 100.0f);
                    line += " (" + std::to_string(pct) + "%)";
                }
            }
            else
            {
                line += "??? x" + std::to_string(slot.count);
            }
        }
        else
        {
            line += "---";
        }

        const bool isCursor = (i == mInventoryCursor);
        const bool active = (i == inv.activeSlot);
        glm::vec4 color;
        if (isCursor)
            color = glm::vec4(1.0f, 1.0f, 0.2f, 1.0f); // bright yellow for cursor
        else if (active)
            color = glm::vec4(1.0f, 1.0f, 0.6f, 1.0f);
        else
            color = glm::vec4(0.75f, 0.75f, 0.7f, 0.85f);

        std::string prefix = isCursor ? "> " : "  ";
        mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), prefix + line, color, kSmallScale);
        y += 14.0f;
    }

    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y + 4.0f), "W/S: Navigate | F: Use | C: Craft with cursor | Enter: Swap | Tab: Close",
                    glm::vec4(0.5f, 0.5f, 0.45f, 0.8f), 1.0f);
}

void Game::renderCraftingOverlay()
{
    if (!mShowCrafting || mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }

    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const auto visible = buildVisibleRecipeList(
        mCraftingSystem,
        mSaveManager,
        inv,
        mSessionDiscoveredRecipes,
        mCraftingCategory);

    const auto& recipes = mCraftingSystem.recipes();

    // Sample a single texel from the center of solid_white to avoid atlas edge bleeding
    // when drawing large stretched UI quads.
    const glm::vec4 uiUvRegion = mSpriteSheet.uvRect("solid_white");
    const glm::vec4 uiUv(
        uiUvRegion.x + uiUvRegion.z * 0.5f,
        uiUvRegion.y + uiUvRegion.w * 0.5f,
        0.0f,
        0.0f);

    const float pad = 0.0f;
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);

    const float listW = std::max(260.0f, w * 0.17f);
    const float invW = std::max(260.0f, w * 0.19f);
    const float detailX = listW + pad;
    const float detailW = w - listW - invW - pad * 2.0f;
    const float invX = w - invW;

    auto drawRect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        if (!mCraftingRenderTextPass)
        {
            mSpriteBatch.draw(pos, size, uiUv, color);
        }
    };

    auto drawText = [&](const glm::vec2& pos, const std::string& text, const glm::vec4& color, float scale, TextAlign align = TextAlign::Left)
    {
        if (mCraftingRenderTextPass)
        {
            const glm::vec4 shadowColor(0.0f, 0.0f, 0.0f, std::min(1.0f, color.a * 0.9f));
            mFont.drawText(mSpriteBatch, pos + glm::vec2(1.0f, 1.0f), text, shadowColor, scale, align);
            mFont.drawText(mSpriteBatch, pos, text, color, scale, align);
        }
    };

    auto isDiscovered = [&](const CraftingRecipe& recipe) -> bool
    {
        return recipe.startsKnown ||
            mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB) ||
            std::find(
                mSessionDiscoveredRecipes.begin(),
                mSessionDiscoveredRecipes.end(),
                CraftingSystem::normalizePair(recipe.inputA, recipe.inputB)) != mSessionDiscoveredRecipes.end();
    };

    // Dim whole scene and column backgrounds.
    drawRect(glm::vec2(0.0f, 0.0f), glm::vec2(w, h), glm::vec4(0.0f, 0.0f, 0.0f, 0.82f));
    drawRect(glm::vec2(0.0f, 0.0f), glm::vec2(listW, h), glm::vec4(0.035f, 0.042f, 0.05f, 1.0f));
    drawRect(glm::vec2(detailX, 0.0f), glm::vec2(detailW, h), glm::vec4(0.045f, 0.05f, 0.058f, 1.0f));
    drawRect(glm::vec2(invX, 0.0f), glm::vec2(invW, h), glm::vec4(0.052f, 0.057f, 0.064f, 1.0f));
    drawRect(glm::vec2(0.0f, 0.0f), glm::vec2(w, 40.0f), glm::vec4(0.01f, 0.01f, 0.015f, 0.75f));
    drawRect(glm::vec2(listW - 1.0f, 0.0f), glm::vec2(1.0f, h), glm::vec4(0.16f, 0.14f, 0.10f, 0.95f));
    drawRect(glm::vec2(invX, 0.0f), glm::vec2(1.0f, h), glm::vec4(0.16f, 0.14f, 0.10f, 0.95f));

    drawText(glm::vec2(12.0f, 12.0f), "CRAFTING", glm::vec4(0.92f, 0.84f, 0.58f, 1.0f), 1.95f);
    drawText(glm::vec2(invX + 12.0f, 12.0f), "INVENTORY", glm::vec4(0.84f, 0.86f, 0.88f, 1.0f), 1.95f);

    // Search strip (visual only for now).
    drawRect(glm::vec2(0.0f, 44.0f), glm::vec2(listW, 32.0f), glm::vec4(0.02f, 0.02f, 0.024f, 0.96f));
    drawText(glm::vec2(14.0f, 54.0f), "> search recipes...", glm::vec4(0.52f, 0.55f, 0.56f, 0.95f), 1.05f);

    // Category tabs.
    const float tabY = 78.0f;
    const float tabW = listW / 3.0f;
    const float tabH = 46.0f;
    const float tabTextScale = 1.28f;
    for (int i = 0; i < 3; ++i)
    {
        const float x0 = std::floor(tabW * static_cast<float>(i));
        const float x1 = (i == 2) ? listW : std::floor(tabW * static_cast<float>(i + 1));
        const float tabWidth = std::max(1.0f, x1 - x0);
        const bool active = (i == mCraftingCategory);
        drawRect(
            glm::vec2(x0, tabY),
            glm::vec2(tabWidth, tabH),
            active ? glm::vec4(0.13f, 0.11f, 0.08f, 0.95f) : glm::vec4(0.05f, 0.05f, 0.05f, 0.95f));
        drawText(
            glm::vec2(x0 + tabWidth * 0.5f, tabY + 15.0f),
            craftingCategoryLabel(i),
            active ? glm::vec4(0.90f, 0.78f, 0.45f, 0.95f) : glm::vec4(0.40f, 0.40f, 0.36f, 0.9f),
            tabTextScale,
            TextAlign::Center);
    }

    // Recipe list (name + craftability dot).
    float rowY = tabY + tabH + 8.0f;
    const float rowH = 58.0f;

    for (std::size_t listIndex = 0; listIndex < visible.size(); ++listIndex)
    {
        const std::size_t recipeIndex = visible[listIndex];
        const CraftingRecipe& recipe = recipes[recipeIndex];
        const bool selected = static_cast<int>(listIndex) == mCraftingRecipeCursor;
        const bool discovered = isDiscovered(recipe);
        const bool craftable =
            discovered &&
            hasCraftIngredients(inv, recipe, 1) &&
            hasCraftTools(inv, recipe) &&
            (!recipe.requiresWorkbench || hasNearbyWorkbench(mWorld, mPlayerEntity));

        drawRect(
            glm::vec2(0.0f, rowY),
            glm::vec2(listW, rowH),
            selected ? glm::vec4(0.165f, 0.12f, 0.07f, 0.98f) : glm::vec4(0.028f, 0.03f, 0.034f, 0.95f));

        drawRect(
            glm::vec2(0.0f, rowY),
            glm::vec2(3.0f, rowH),
            selected ? glm::vec4(0.90f, 0.72f, 0.30f, 0.95f) : glm::vec4(0.16f, 0.18f, 0.20f, 0.80f));

        drawText(
            glm::vec2(14.0f, rowY + 18.0f),
            discovered ? recipe.label : "???",
            discovered ? glm::vec4(0.94f, 0.88f, 0.74f, 1.0f) : glm::vec4(0.68f, 0.70f, 0.72f, 0.92f),
            1.75f);

        drawRect(
            glm::vec2(listW - 14.0f, rowY + 24.0f),
            glm::vec2(6.0f, 6.0f),
            craftable ? glm::vec4(0.25f, 0.75f, 0.30f, 1.0f) : glm::vec4(0.55f, 0.20f, 0.20f, 1.0f));

        rowY += rowH;
        if (rowY > h - 90.0f)
        {
            break;
        }
    }

    if (visible.empty())
    {
        drawText(
            glm::vec2(14.0f, 140.0f),
            "No known or nearby clues in this category.",
            glm::vec4(0.74f, 0.76f, 0.78f, 0.95f),
            1.15f);
    }

    if (mCraftingNotebookView)
    {
        drawRect(glm::vec2(detailX + 14.0f, 10.0f), glm::vec2(detailW - 28.0f, 46.0f), glm::vec4(0.065f, 0.07f, 0.08f, 0.98f));
        drawText(glm::vec2(detailX + 18.0f, 16.0f), "NOTEBOOK LOG", glm::vec4(0.86f, 0.74f, 0.44f, 0.95f), 2.0f);
        drawText(glm::vec2(detailX + 18.0f, 52.0f), "Discovered Recipes", glm::vec4(0.72f, 0.72f, 0.66f, 0.9f), 1.2f);

        float logY = 82.0f;
        const auto& discoveredPairs = mSaveManager.discoveredRecipes();
        if (discoveredPairs.empty())
        {
            drawText(glm::vec2(detailX + 18.0f, logY), "No preserved entries yet.", glm::vec4(0.55f, 0.55f, 0.52f, 0.9f), 1.2f);
        }
        else
        {
            for (const auto& pair : discoveredPairs)
            {
                const CraftingRecipe* recipe = mCraftingSystem.findRecipe(pair.first, pair.second);
                const std::string title = (recipe != nullptr) ? recipe->label : "Unknown Entry";
                drawText(glm::vec2(detailX + 18.0f, logY), "- " + title, glm::vec4(0.84f, 0.78f, 0.62f, 0.95f), 1.2f);
                logY += 18.0f;
                if (logY > h - 70.0f)
                {
                    break;
                }
            }
        }
    }
    else if (!visible.empty())
    {
        const int selectedRecipeListIndex = std::clamp(mCraftingRecipeCursor, 0, static_cast<int>(visible.size()) - 1);
        const std::size_t selectedRecipeIndex = visible[static_cast<std::size_t>(selectedRecipeListIndex)];
        const CraftingRecipe& selectedRecipe = recipes[selectedRecipeIndex];
        const bool discovered = isDiscovered(selectedRecipe);

        const ItemDef* aDef = mItemDatabase.find(selectedRecipe.inputA);
        const ItemDef* bDef = mItemDatabase.find(selectedRecipe.inputB);
        const ItemDef* outDef = mItemDatabase.find(selectedRecipe.outputItemId);

        drawRect(glm::vec2(detailX + 14.0f, 10.0f), glm::vec2(detailW - 28.0f, 46.0f), glm::vec4(0.065f, 0.07f, 0.08f, 0.98f));

        drawText(
            glm::vec2(detailX + 14.0f, 16.0f),
            discovered ? selectedRecipe.label : "???",
            glm::vec4(0.95f, 0.88f, 0.62f, 1.0f),
            2.7f);

        drawText(
            glm::vec2(detailX + 14.0f, 50.0f),
            discovered ? selectedRecipe.category + "  -  CRAFT ENTRY" : "UNKNOWN ENTRY",
            glm::vec4(0.72f, 0.76f, 0.78f, 0.96f),
            1.15f);

        if (selectedRecipe.requiresWorkbench)
        {
            const bool nearWorkbench = hasNearbyWorkbench(mWorld, mPlayerEntity);
            drawRect(
                glm::vec2(detailX + 14.0f, 76.0f),
                glm::vec2(detailW - 28.0f, 24.0f),
                nearWorkbench ? glm::vec4(0.10f, 0.22f, 0.12f, 0.95f) : glm::vec4(0.26f, 0.09f, 0.09f, 0.95f));
            drawText(
                glm::vec2(detailX + 20.0f, 83.0f),
                nearWorkbench ? "WORKBENCH IN RANGE" : "WORKBENCH REQUIRED (MOVE CLOSER)",
                nearWorkbench ? glm::vec4(0.45f, 0.86f, 0.50f, 0.95f) : glm::vec4(0.95f, 0.45f, 0.45f, 0.95f),
                1.1f);
        }

        const float ingredientTop = selectedRecipe.requiresWorkbench ? 116.0f : 104.0f;
        drawText(glm::vec2(detailX + 14.0f, ingredientTop), "INGREDIENTS", glm::vec4(0.82f, 0.84f, 0.86f, 0.98f), 1.34f);

        auto drawIngredientCard = [&](float x, const ItemDef* def, int needed)
        {
            const int have = (def != nullptr) ? InventorySystem::countItem(inv, def->id) : 0;
            const bool ok = discovered && have >= needed;
            drawRect(
                glm::vec2(x, ingredientTop + 22.0f),
                glm::vec2((detailW - 42.0f) * 0.5f, 60.0f),
                ok ? glm::vec4(0.07f, 0.18f, 0.08f, 0.92f) : glm::vec4(0.20f, 0.07f, 0.07f, 0.92f));

            drawText(
                glm::vec2(x + 10.0f, ingredientTop + 36.0f),
                (!discovered || def == nullptr) ? "???" : def->name,
                glm::vec4(0.86f, 0.80f, 0.68f, 0.95f),
                1.4f);

            std::ostringstream qty;
            qty << (discovered ? have : 0) << "/" << needed;
            drawText(
                glm::vec2(x + 10.0f, ingredientTop + 54.0f),
                qty.str(),
                ok ? glm::vec4(0.35f, 0.88f, 0.40f, 0.95f) : glm::vec4(0.92f, 0.45f, 0.45f, 0.95f),
                1.2f);
        };

        drawIngredientCard(detailX + 14.0f, aDef, 1);
        drawIngredientCard(detailX + 22.0f + (detailW - 42.0f) * 0.5f, bDef, 1);

        const float toolsY = ingredientTop + 96.0f;
        drawText(glm::vec2(detailX + 14.0f, toolsY), "REQUIRED TOOLS", glm::vec4(0.82f, 0.84f, 0.86f, 0.98f), 1.34f);

        if (selectedRecipe.requiredTools.empty())
        {
            drawRect(glm::vec2(detailX + 14.0f, toolsY + 22.0f), glm::vec2(140.0f, 30.0f), glm::vec4(0.10f, 0.20f, 0.07f, 0.92f));
            drawText(glm::vec2(detailX + 24.0f, toolsY + 31.0f), "NONE REQUIRED", glm::vec4(0.36f, 0.74f, 0.32f, 0.95f), 1.2f);
        }
        else
        {
            float toolX = detailX + 14.0f;
            for (int toolId : selectedRecipe.requiredTools)
            {
                const ItemDef* toolDef = mItemDatabase.find(toolId);
                const bool ok = toolDef != nullptr && InventorySystem::countItem(inv, toolId) > 0;

                float bestCondition = 0.0f;
                for (const auto& slot : inv.slots)
                {
                    if (slot.itemId == toolId && slot.count > 0)
                    {
                        bestCondition = std::max(bestCondition, slot.condition);
                    }
                }
                const int conditionPct = static_cast<int>(std::round(std::clamp(bestCondition, 0.0f, 1.0f) * 100.0f));

                drawRect(
                    glm::vec2(toolX, toolsY + 22.0f),
                    glm::vec2(170.0f, 30.0f),
                    ok ? glm::vec4(0.09f, 0.18f, 0.10f, 0.92f) : glm::vec4(0.20f, 0.07f, 0.07f, 0.92f));

                std::string toolLabel = toolDef ? toolDef->name : "Unknown tool";
                if (ok)
                {
                    toolLabel += " [" + std::to_string(conditionPct) + "%]";
                }

                drawText(
                    glm::vec2(toolX + 8.0f, toolsY + 31.0f),
                    toolLabel,
                    ok ? glm::vec4(0.40f, 0.86f, 0.48f, 0.95f) : glm::vec4(0.94f, 0.46f, 0.46f, 0.95f),
                    1.1f);
                toolX += 176.0f;
            }
        }

        const float outY = toolsY + 70.0f;
        drawText(glm::vec2(detailX + 14.0f, outY), "OUTPUT", glm::vec4(0.82f, 0.84f, 0.86f, 0.98f), 1.34f);
        drawRect(glm::vec2(detailX + 14.0f, outY + 20.0f), glm::vec2(detailW - 28.0f, 82.0f), glm::vec4(0.08f, 0.08f, 0.08f, 0.92f));
        drawText(
            glm::vec2(detailX + 24.0f, outY + 36.0f),
            discovered ? (outDef ? outDef->name : selectedRecipe.label) : "Unknown result",
            glm::vec4(0.90f, 0.82f, 0.65f, 0.95f),
            2.0f);
        if (discovered && !selectedRecipe.notes.empty())
        {
            drawText(glm::vec2(detailX + 24.0f, outY + 58.0f), selectedRecipe.notes, glm::vec4(0.50f, 0.50f, 0.48f, 0.90f), 1.1f);
        }

        const float timeY = outY + 120.0f;
        drawText(glm::vec2(detailX + 14.0f, timeY), "Craft time", glm::vec4(0.78f, 0.82f, 0.84f, 0.98f), 1.24f);
        drawRect(glm::vec2(detailX + 14.0f, timeY + 18.0f), glm::vec2(detailW - 28.0f, 10.0f), glm::vec4(0.12f, 0.13f, 0.14f, 0.95f));

        const float craftDuration = std::max(0.25f, static_cast<float>(selectedRecipe.craftTimeSeconds));
        float progressRatio = 0.0f;
        if (!mCraftQueue.empty() && mCraftQueue.front().recipeIndex == static_cast<int>(selectedRecipeIndex))
        {
            progressRatio = std::clamp(mActiveCraftProgressSeconds / craftDuration, 0.0f, 1.0f);
        }
        drawRect(
            glm::vec2(detailX + 14.0f, timeY + 18.0f),
            glm::vec2((detailW - 28.0f) * progressRatio, 10.0f),
            glm::vec4(0.60f, 0.50f, 0.25f, 0.95f));

        std::ostringstream timeText;
        timeText << std::fixed << std::setprecision(1) << craftDuration << "s";
        drawText(
            glm::vec2(detailX + detailW - 96.0f, timeY + 12.0f),
            timeText.str(),
            glm::vec4(0.42f, 0.42f, 0.40f, 0.9f),
            1.1f);

        int queuedTotal = 0;
        for (const CraftQueueEntry& entry : mCraftQueue)
        {
            queuedTotal += std::max(0, entry.remaining);
        }

        if (!mCraftQueue.empty())
        {
            const int activeRecipeIndex = mCraftQueue.front().recipeIndex;
            const int activeRemaining = mCraftQueue.front().remaining;
            const std::string activeName =
                (activeRecipeIndex >= 0 && activeRecipeIndex < static_cast<int>(recipes.size()))
                    ? recipes[static_cast<std::size_t>(activeRecipeIndex)].label
                    : "Unknown";
            drawText(
                glm::vec2(detailX + 14.0f, timeY + 38.0f),
                "Queue: " + activeName + " x" + std::to_string(std::max(0, activeRemaining)) +
                    " (total " + std::to_string(queuedTotal) + ")",
                glm::vec4(0.78f, 0.80f, 0.74f, 0.98f),
                1.15f);
        }
        else
        {
            drawText(glm::vec2(detailX + 14.0f, timeY + 38.0f), "Queue: idle", glm::vec4(0.70f, 0.72f, 0.73f, 0.96f), 1.15f);
        }

        const float footerY = h - 64.0f;
        drawRect(glm::vec2(detailX + 14.0f, footerY), glm::vec2(126.0f, 34.0f), glm::vec4(0.08f, 0.08f, 0.08f, 0.95f));
        drawText(glm::vec2(detailX + 22.0f, footerY + 9.0f), "-", glm::vec4(0.75f, 0.70f, 0.58f, 0.95f), 2.0f);
        drawText(glm::vec2(detailX + 64.0f, footerY + 9.0f), std::to_string(mCraftingQuantity), glm::vec4(0.88f, 0.82f, 0.66f, 0.95f), 1.8f);
        drawText(glm::vec2(detailX + 108.0f, footerY + 9.0f), "+", glm::vec4(0.75f, 0.70f, 0.58f, 0.95f), 2.0f);

        bool ready = discovered && hasCraftIngredients(inv, selectedRecipe, 1) && hasCraftTools(inv, selectedRecipe);
        if (selectedRecipe.requiresWorkbench)
        {
            ready = ready && hasNearbyWorkbench(mWorld, mPlayerEntity);
        }

        drawRect(
            glm::vec2(detailX + 150.0f, footerY),
            glm::vec2(detailW - 164.0f, 34.0f),
            ready ? glm::vec4(0.16f, 0.18f, 0.10f, 0.95f) : glm::vec4(0.12f, 0.12f, 0.12f, 0.95f));
        drawText(
            glm::vec2(detailX + detailW * 0.5f, footerY + 9.0f),
            ready ? "QUEUE CRAFT" : "MISSING ITEMS",
            ready ? glm::vec4(0.60f, 0.62f, 0.30f, 0.95f) : glm::vec4(0.45f, 0.45f, 0.42f, 0.95f),
            1.8f,
            TextAlign::Center);
    }

    float invY = 44.0f;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId < 0 || slot.count <= 0)
        {
            continue;
        }

        const ItemDef* item = mItemDatabase.find(slot.itemId);
        std::string line = item ? item->name : ("Item " + std::to_string(slot.itemId));
        line += " x" + std::to_string(slot.count);

        drawRect(glm::vec2(invX + 10.0f, invY), glm::vec2(invW - 20.0f, 34.0f), glm::vec4(0.11f, 0.12f, 0.13f, 0.96f));
        drawText(glm::vec2(invX + 18.0f, invY + 10.0f), line, glm::vec4(0.92f, 0.94f, 0.96f, 1.0f), 1.58f);

        invY += 40.0f;
        if (invY > h - 60.0f)
        {
            break;
        }
    }

    drawRect(glm::vec2(0.0f, h - 24.0f), glm::vec2(w, 24.0f), glm::vec4(0.015f, 0.018f, 0.022f, 0.98f));
    drawText(
        glm::vec2(12.0f, h - 18.0f),
        "C: Open/Close  W/S: Select  A/D: Category  +/-: Quantity  Enter: Queue  Shift+Enter: Append  X: Cancel  J: Notebook",
        glm::vec4(0.78f, 0.82f, 0.84f, 0.98f),
        1.06f);
}

void Game::renderContainerOverlay()
{
    if (!mContainerOpen || mOpenContainerEntity == kInvalidEntity || mRunState != RunState::Playing)
    {
        return;
    }

    if (!mWorld.hasComponent<Inventory>(mOpenContainerEntity))
    {
        return;
    }

    const Inventory& containerInv = mWorld.getComponent<Inventory>(mOpenContainerEntity);
    constexpr float kSmallScale = 1.5f;

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");

    // Semi-transparent panel background
    const float panelW = 300.0f;
    const float panelX = (static_cast<float>(mWindowWidth) - panelW) * 0.5f;
    const float panelY = 120.0f;

    // Count items
    int itemCount = 0;
    for (const auto& slot : containerInv.slots)
    {
        if (slot.itemId >= 0 && slot.count > 0) ++itemCount;
    }

    const float panelH = 40.0f + static_cast<float>(std::max(1, itemCount)) * 18.0f;
    mSpriteBatch.draw(glm::vec2(panelX, panelY), glm::vec2(panelW, panelH),
                       uiUv, glm::vec4(0.06f, 0.06f, 0.1f, 0.88f));

    // Header
    std::string header = "CONTAINER";
    if (mWorld.hasComponent<Interactable>(mOpenContainerEntity))
    {
        const Interactable& inter = mWorld.getComponent<Interactable>(mOpenContainerEntity);
        if (!inter.prompt.empty()) header = inter.prompt;
    }
    mFont.drawText(mSpriteBatch, glm::vec2(panelX + 10.0f, panelY + 8.0f), header,
                    glm::vec4(0.9f, 0.85f, 0.5f, 1.0f), 2.0f);

    float y = panelY + 32.0f;
    int idx = 0;
    for (const auto& slot : containerInv.slots)
    {
        if (slot.itemId < 0 || slot.count <= 0) continue;

        const bool selected = (idx == mContainerCursor);
        const ItemDef* item = mItemDatabase.find(slot.itemId);
        std::string line = item ? item->name : ("Item #" + std::to_string(slot.itemId));
        if (slot.count > 1) line += " x" + std::to_string(slot.count);

        if (selected)
        {
            mSpriteBatch.draw(glm::vec2(panelX + 4.0f, y - 1.0f), glm::vec2(panelW - 8.0f, 17.0f),
                               uiUv, glm::vec4(0.25f, 0.35f, 0.5f, 0.5f));
        }

        const glm::vec4 color = selected ? glm::vec4(1.0f, 1.0f, 0.7f, 1.0f) : glm::vec4(0.7f, 0.7f, 0.65f, 0.9f);
        mFont.drawText(mSpriteBatch, glm::vec2(panelX + 12.0f, y), line, color, kSmallScale);
        y += 18.0f;
        ++idx;
    }

    if (itemCount == 0)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(panelX + 12.0f, y), "Empty",
                        glm::vec4(0.5f, 0.5f, 0.5f, 0.7f), kSmallScale);
    }

    // Controls hint
    mFont.drawText(mSpriteBatch, glm::vec2(panelX + 10.0f, panelY + panelH + 4.0f),
                    "W/S: Select  E: Take  ESC: Close",
                    glm::vec4(0.5f, 0.5f, 0.45f, 0.7f), 1.0f);
}

void Game::renderBuildModeOverlay()
{
    if (mShowCrafting || !mBuildSystem.isBuildMode() || mRunState != RunState::Playing)
    {
        return;
    }

    constexpr float kSmallScale = 1.5f;
    const float panelX = 12.0f;
    float y = static_cast<float>(mWindowHeight) - 120.0f;

    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "BUILD MODE", glm::vec4(0.9f, 0.8f, 0.3f, 1.0f), 2.0f);
    y += 22.0f;

    for (int i = 0; i < BuildSystem::kRecipeCount; ++i)
    {
        const auto& recipe = BuildSystem::kRecipes[i];
        const bool selected = (i == mBuildSystem.selectedRecipe());

        // Check if player has enough materials
        bool canAfford = false;
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
        {
            const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
            canAfford = InventorySystem::countItem(inv, recipe.materialId) >= recipe.materialCost;
        }

        std::string line = std::to_string(i + 1) + ": " + std::string(recipe.name) + " (" + std::to_string(recipe.materialCost) + " mat)";
        glm::vec4 color;
        if (selected)
        {
            color = canAfford ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.5f, 0.4f, 1.0f);
        }
        else
        {
            color = canAfford ? glm::vec4(0.4f, 0.75f, 0.45f, 0.85f) : glm::vec4(0.5f, 0.5f, 0.5f, 0.6f);
        }
        mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), line, color, kSmallScale);
        y += 16.0f;
    }

    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y + 2.0f), "Click: Place  |  B/ESC: Cancel", glm::vec4(0.5f, 0.5f, 0.45f, 0.8f), 1.0f);
}

void Game::renderPauseOverlay()
{
    if (mRunState != RunState::Paused)
    {
        return;
    }

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");

    // Dim background
    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)),
        uiUv,
        glm::vec4(0.0f, 0.0f, 0.0f, 0.65f));

    // Panel
    const glm::vec2 panelSize(340.0f, 204.0f);
    const glm::vec2 panelPos(
        (static_cast<float>(mWindowWidth) - panelSize.x) * 0.5f,
        (static_cast<float>(mWindowHeight) - panelSize.y) * 0.5f);
    mSpriteBatch.draw(panelPos, panelSize, uiUv, glm::vec4(0.08f, 0.08f, 0.12f, 0.9f));

    // Highlight selected option
    const float optionStartY = panelPos.y + 58.0f;
    const float optionH = 28.0f;
    mSpriteBatch.draw(
        glm::vec2(panelPos.x + 10.0f, optionStartY + static_cast<float>(mPauseSelection) * optionH - 2.0f),
        glm::vec2(panelSize.x - 20.0f, optionH),
        uiUv,
        glm::vec4(0.25f, 0.4f, 0.55f, 0.5f));
}

void Game::renderTitleScreen()
{
    if (mRunState != RunState::TitleScreen)
    {
        return;
    }

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const float pulse = 0.5f + 0.5f * std::sin(mTitlePulseTimer * 1.2f);
    const TitlePanelLayout layout = computeTitlePanelLayout(mWindowWidth, mWindowHeight);

    // Global dim to push the title UI to the foreground.
    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(w, h),
        uiUv,
        glm::vec4(0.02f, 0.03f, 0.04f, 0.78f));

    // Main briefing panel.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX, layout.panelY),
        glm::vec2(layout.panelW, layout.panelH),
        uiUv,
        glm::vec4(0.07f, 0.08f, 0.09f, 0.95f));

    // Outer border.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX - 1.0f, layout.panelY - 1.0f),
        glm::vec2(layout.panelW + 2.0f, 1.0f),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.60f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelX - 1.0f, layout.panelY + layout.panelH),
        glm::vec2(layout.panelW + 2.0f, 1.0f),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.35f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelX - 1.0f, layout.panelY),
        glm::vec2(1.0f, layout.panelH),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.35f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelX + layout.panelW, layout.panelY),
        glm::vec2(1.0f, layout.panelH),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.60f));

    // Header stripe.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX, layout.panelY),
        glm::vec2(layout.panelW, 56.0f),
        uiUv,
        glm::vec4(0.20f, 0.08f, 0.06f, 0.94f));

    // Left interaction column.
    mSpriteBatch.draw(
        glm::vec2(layout.leftColX, layout.leftColY),
        glm::vec2(layout.leftColW, layout.leftColH),
        uiUv,
        glm::vec4(0.11f, 0.12f, 0.13f, 0.88f));

    // Right status column.
    mSpriteBatch.draw(
        glm::vec2(layout.rightColX, layout.rightColY),
        glm::vec2(layout.rightColW, layout.rightColH),
        uiUv,
        glm::vec4(0.10f, 0.11f, 0.12f, 0.90f));

    // Strong center divider between action and intel.
    mSpriteBatch.draw(
        glm::vec2(layout.rightColX - 6.0f, layout.leftColY),
        glm::vec2(2.0f, layout.leftColH),
        uiUv,
        glm::vec4(0.30f, 0.20f, 0.17f, 0.55f));

    // Right column section cards for readability.
    mSpriteBatch.draw(
        glm::vec2(layout.rightCardTopX, layout.rightCardTopY),
        glm::vec2(layout.rightCardTopW, layout.rightCardTopH),
        uiUv,
        glm::vec4(0.12f, 0.14f, 0.16f, 0.72f));
    mSpriteBatch.draw(
        glm::vec2(layout.rightCardMidX, layout.rightCardMidY),
        glm::vec2(layout.rightCardMidW, layout.rightCardMidH),
        uiUv,
        glm::vec4(0.12f, 0.14f, 0.16f, 0.68f));
    mSpriteBatch.draw(
        glm::vec2(layout.rightCardBottomX, layout.rightCardBottomY),
        glm::vec2(layout.rightCardBottomW, layout.rightCardBottomH),
        uiUv,
        glm::vec4(0.12f, 0.14f, 0.16f, 0.64f));

    // Menu row strips to make scanning options easier.
    for (int i = 0; i < static_cast<int>(kTitleMenuItems.size()); ++i)
    {
        const float rowY = layout.menuRowY + static_cast<float>(i) * layout.menuRowStep;
        glm::vec4 rowColor = (i % 2 == 0)
            ? glm::vec4(0.15f, 0.16f, 0.18f, 0.48f)
            : glm::vec4(0.12f, 0.13f, 0.15f, 0.40f);

        if (i == 0 && !mTitleHasCheckpoint)
        {
            rowColor = glm::vec4(0.20f, 0.12f, 0.12f, 0.44f);
        }
        if (i == mTitleMenuSelection)
        {
            rowColor = glm::vec4(0.40f, 0.26f, 0.16f, 0.70f);
        }

        mSpriteBatch.draw(
            glm::vec2(layout.menuRowX, rowY),
            glm::vec2(layout.menuRowW, layout.menuRowH),
            uiUv,
            rowColor);
    }

    // Accent pulse strip.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX + layout.panelW - 10.0f, layout.leftColY),
        glm::vec2(4.0f, layout.leftColH),
        uiUv,
        glm::vec4(0.85f, 0.25f + 0.25f * pulse, 0.18f, 0.65f));

    // Light scan lines for a briefing-terminal feel.
    for (int i = 0; i < 14; ++i)
    {
        const float lineY = layout.panelY + 74.0f + static_cast<float>(i) * ((layout.panelH - 100.0f) / 13.0f);
        mSpriteBatch.draw(
            glm::vec2(layout.panelX + 16.0f, lineY),
            glm::vec2(layout.panelW - 32.0f, 1.0f),
            uiUv,
            glm::vec4(0.35f, 0.35f, 0.35f, 0.07f));
    }

    if (mShowTitleCredits)
    {
        mSpriteBatch.draw(
            glm::vec2(layout.leftColX, layout.leftColY),
            glm::vec2(layout.panelW - 24.0f, layout.leftColH),
            uiUv,
            glm::vec4(0.03f, 0.04f, 0.05f, 0.84f));
    }
}

void Game::renderControlsOverlay()
{
    if (!mShowControls)
    {
        return;
    }

    constexpr float kSmallScale = 1.5f;
    const float panelX = static_cast<float>(mWindowWidth) - 260.0f;
    float y = 80.0f;
    const glm::vec4 headerColor(0.9f, 0.85f, 0.6f, 1.0f);
    const glm::vec4 textColor(0.7f, 0.7f, 0.65f, 0.9f);

    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "CONTROLS", headerColor, 2.0f);
    y += 24.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "WASD    Move", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "Mouse   Aim", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "LClick  Attack", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "RClick  Grab", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "E       Interact", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "F       Use Item", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "B       Build", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "Tab     Inventory", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "1-8     Hotbar", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "W/S     Navigate (Inv)", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "Enter   Swap to hotbar", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "Shift   Run", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "C       Crouch", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "Esc     Pause", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "F2      Controls", textColor, kSmallScale); y += 16.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "F3      Infection Debug", textColor, kSmallScale);
}
