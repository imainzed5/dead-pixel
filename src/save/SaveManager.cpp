#include "save/SaveManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>

namespace
{
std::optional<std::uint32_t> gSeedOverride;

std::uint32_t randomSeed()
{
    if (gSeedOverride)
    {
        return *gSeedOverride;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::uint32_t> dist(1u, 0xFFFFFFFEu);
    return dist(gen);
}

std::string readFileToString(const std::string& filePath)
{
    std::ifstream input(filePath, std::ios::binary);
    if (!input)
    {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::pair<int, int> normalizeRecipePair(int itemA, int itemB)
{
    if (itemA <= itemB)
    {
        return {itemA, itemB};
    }
    return {itemB, itemA};
}

void addUniqueTile(std::vector<std::pair<int, int>>& tiles, int x, int y)
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

std::string toLowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
    return value;
}

const char* saveGameModeToString(SaveGameMode mode)
{
    switch (mode)
    {
    case SaveGameMode::Campaign:
        return "campaign";
    case SaveGameMode::Endless:
    default:
        return "endless";
    }
}

SaveGameMode parseSaveGameMode(const nlohmann::json& value)
{
    if (value.is_string())
    {
        const std::string lowered = toLowerCopy(value.get<std::string>());
        if (lowered == "campaign")
        {
            return SaveGameMode::Campaign;
        }
        return SaveGameMode::Endless;
    }

    if (value.is_number_integer())
    {
        const int mode = value.get<int>();
        return mode == 0 ? SaveGameMode::Campaign : SaveGameMode::Endless;
    }

    return SaveGameMode::Endless;
}

const char* saveThreatModeToString(SaveThreatMode mode)
{
    switch (mode)
    {
    case SaveThreatMode::OverwhelmingSwarm:
        return "overwhelming_swarm";
    case SaveThreatMode::Stalker:
        return "stalker";
    case SaveThreatMode::MutatingThreat:
        return "mutating_threat";
    case SaveThreatMode::InfectedSociety:
        return "infected_society";
    case SaveThreatMode::Unknown:
        return "unknown";
    case SaveThreatMode::Hybrid:
        return "hybrid";
    default:
        return "overwhelming_swarm";
    }
}

SaveThreatMode parseSaveThreatMode(const nlohmann::json& value)
{
    if (value.is_string())
    {
        const std::string lowered = toLowerCopy(value.get<std::string>());
        if (lowered == "stalker") return SaveThreatMode::Stalker;
        if (lowered == "mutating_threat") return SaveThreatMode::MutatingThreat;
        if (lowered == "infected_society") return SaveThreatMode::InfectedSociety;
        if (lowered == "unknown") return SaveThreatMode::Unknown;
        if (lowered == "hybrid") return SaveThreatMode::Hybrid;
        return SaveThreatMode::OverwhelmingSwarm;
    }

    if (value.is_number_integer())
    {
        const int mode = value.get<int>();
        switch (mode)
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

    return SaveThreatMode::OverwhelmingSwarm;
}

const char* saveTonePresetToString(SaveTonePreset preset)
{
    switch (preset)
    {
    case SaveTonePreset::Hopeless:
        return "hopeless";
    case SaveTonePreset::GrimHuman:
        return "grim_human";
    case SaveTonePreset::BrutalArcade:
        return "brutal_arcade";
    default:
        return "grim_human";
    }
}

SaveTonePreset parseSaveTonePreset(const nlohmann::json& value)
{
    if (value.is_string())
    {
        const std::string lowered = toLowerCopy(value.get<std::string>());
        if (lowered == "hopeless") return SaveTonePreset::Hopeless;
        if (lowered == "brutal_arcade") return SaveTonePreset::BrutalArcade;
        return SaveTonePreset::GrimHuman;
    }

    if (value.is_number_integer())
    {
        const int preset = value.get<int>();
        switch (preset)
        {
        case 0: return SaveTonePreset::Hopeless;
        case 1: return SaveTonePreset::GrimHuman;
        case 2: return SaveTonePreset::BrutalArcade;
        default: return SaveTonePreset::GrimHuman;
        }
    }

    return SaveTonePreset::GrimHuman;
}

const char* worldMemoryEventTypeToString(WorldMemoryEventType type)
{
    switch (type)
    {
    case WorldMemoryEventType::LootedContainer:
        return "looted_container";
    case WorldMemoryEventType::BurnedTile:
        return "burned_tile";
    case WorldMemoryEventType::VehicleAbandoned:
        return "vehicle_abandoned";
    case WorldMemoryEventType::StructureAbandoned:
        return "structure_abandoned";
    case WorldMemoryEventType::GravePlaced:
        return "grave_placed";
    case WorldMemoryEventType::RetirementPlaced:
        return "retirement_placed";
    case WorldMemoryEventType::Other:
    default:
        return "other";
    }
}

WorldMemoryEventType parseWorldMemoryEventType(const nlohmann::json& value)
{
    if (value.is_string())
    {
        const std::string lowered = toLowerCopy(value.get<std::string>());
        if (lowered == "looted_container") return WorldMemoryEventType::LootedContainer;
        if (lowered == "burned_tile") return WorldMemoryEventType::BurnedTile;
        if (lowered == "vehicle_abandoned") return WorldMemoryEventType::VehicleAbandoned;
        if (lowered == "structure_abandoned") return WorldMemoryEventType::StructureAbandoned;
        if (lowered == "grave_placed") return WorldMemoryEventType::GravePlaced;
        if (lowered == "retirement_placed") return WorldMemoryEventType::RetirementPlaced;
        return WorldMemoryEventType::Other;
    }

    if (value.is_number_integer())
    {
        const int type = value.get<int>();
        switch (type)
        {
        case 0: return WorldMemoryEventType::LootedContainer;
        case 1: return WorldMemoryEventType::BurnedTile;
        case 2: return WorldMemoryEventType::VehicleAbandoned;
        case 3: return WorldMemoryEventType::StructureAbandoned;
        case 4: return WorldMemoryEventType::GravePlaced;
        case 5: return WorldMemoryEventType::RetirementPlaced;
        default: return WorldMemoryEventType::Other;
        }
    }

    return WorldMemoryEventType::Other;
}
}

bool SaveManager::loadOrCreateSlot(const std::string& slotName)
{
    mSlotName = slotName;
    mSaveDir = "save";
    mMetaPath = mSaveDir + "/" + mSlotName + ".json";
    mMetaBackupPath = mMetaPath + ".backup";
    mLegacyPath = mSaveDir + "/" + mSlotName + "_legacy.json";
    mLegacyBackupPath = mSaveDir + "/" + mSlotName + "_legacy.backup.json";
    mStructurePath = mSaveDir + "/" + mSlotName + "_structures.json";
    mStructureBackupPath = mSaveDir + "/" + mSlotName + "_structures.backup.json";
    mLayoutPath = mSaveDir + "/" + mSlotName + "_layout.json";
    mLayoutBackupPath = mSaveDir + "/" + mSlotName + "_layout.backup.json";
    mCheckpointPath = mSaveDir + "/" + mSlotName + "_checkpoint.json";
    mWorldMemoryPath = mSaveDir + "/" + mSlotName + "_memory.json";
    mSurvivorPath = mSaveDir + "/" + mSlotName + "_survivors.json";

    std::error_code ec;
    std::filesystem::create_directories(mSaveDir, ec);
    if (ec)
    {
        return false;
    }

    if (!loadMeta())
    {
        return false;
    }

    if (!loadLegacy())
    {
        return false;
    }

    if (!loadStructures())
    {
        return false;
    }

    loadLootedPositions();

    return true;
}

bool SaveManager::resetCurrentSlotWorld(bool keepPresentationSettings)
{
    if (mSlotName.empty())
    {
        return false;
    }

    const RunPresentationSettings preserved = mPresentationSettings;

    mSaveVersion = kCurrentSaveVersion;
    mWorldSeed = randomSeed();
    mRunNumber = 1;
    mTotalKills = 0;
    mLongestRunDays = 0;
    mTotalDaysSurvived = 0;

    mGraves.clear();
    mRetirements.clear();
    mStructures.clear();
    mLootedPositions.clear();
    mDiscoveredRecipes.clear();
    mVehicles.clear();
    mFireTiles.clear();
    mBurnedTiles.clear();
    mBrokenDoors.clear();
    mBoardedDoors.clear();
    mCharacterHistory.clear();
    mWorldEvents.clear();

    mPresentationSettings = keepPresentationSettings ? preserved : RunPresentationSettings{};

    std::error_code ec;
    std::filesystem::remove(mLayoutPath, ec);
    ec.clear();
    std::filesystem::remove(mLayoutBackupPath, ec);
    ec.clear();
    std::filesystem::remove(mCheckpointPath, ec);
    ec.clear();
    std::filesystem::remove(mSurvivorPath, ec);
    ec.clear();

    if (!writeLegacy())
    {
        return false;
    }

    if (!writeStructures())
    {
        return false;
    }

    if (!saveLootedPositions())
    {
        return false;
    }

    if (!writeMeta())
    {
        return false;
    }

    return true;
}

std::uint32_t SaveManager::currentRunSeed() const
{
    return mWorldSeed ^ static_cast<std::uint32_t>(mRunNumber);
}

bool SaveManager::recordDeath(const GraveRecord& record)
{
    mGraves.push_back(record);
    ++mRunNumber;

    if (!writeLegacy())
    {
        return false;
    }

    if (!writeMeta())
    {
        return false;
    }

    return true;
}

void SaveManager::updateGrave(std::size_t index, const GraveRecord& record)
{
    if (index < mGraves.size())
    {
        mGraves[index] = record;
    }
}

bool SaveManager::recordRetirement(const RetirementRecord& record)
{
    mRetirements.push_back(record);
    ++mRunNumber;

    if (!writeLegacy())
    {
        return false;
    }

    if (!writeMeta())
    {
        return false;
    }

    return true;
}

bool SaveManager::addCharacterHistoryRecord(const CharacterHistoryRecord& record)
{
    mCharacterHistory.push_back(record);
    return writeLegacy();
}

BloodlineTrait SaveManager::selectBloodlineForNewRun() const
{
    // Preserve existing behavior: any retirement history guarantees Hardened.
    if (!mRetirements.empty())
    {
        return BloodlineTrait::Hardened;
    }

    if (!mCharacterHistory.empty())
    {
        const CharacterHistoryRecord& latest = mCharacterHistory.back();
        const std::string causeLower = toLowerCopy(latest.cause);

        if (causeLower.find("burn") != std::string::npos ||
            causeLower.find("fire") != std::string::npos)
        {
            return BloodlineTrait::Firewise;
        }

        if (causeLower.find("starv") != std::string::npos ||
            causeLower.find("thirst") != std::string::npos)
        {
            return BloodlineTrait::Scavenger;
        }
    }

    if (mGraves.size() >= 4)
    {
        return BloodlineTrait::Vigilant;
    }

    return BloodlineTrait::None;
}

bool SaveManager::addWorldMemoryEvent(const WorldMemoryEventRecord& record)
{
    for (const WorldMemoryEventRecord& existing : mWorldEvents)
    {
        if (existing.type == record.type &&
            existing.tileX == record.tileX &&
            existing.tileY == record.tileY &&
            existing.day == record.day &&
            existing.runIndex == record.runIndex &&
            existing.detail == record.detail)
        {
            return true;
        }
    }

    mWorldEvents.push_back(record);
    return saveLootedPositions();
}

bool SaveManager::savePresentationSettings()
{
    return writeMeta();
}

bool SaveManager::loadMeta()
{
    auto parseMeta = [&](const std::string& contents) -> bool
    {
        nlohmann::json json = nlohmann::json::parse(contents, nullptr, false);
        if (json.is_discarded())
        {
            return false;
        }

        const int version = json.value("saveVersion", 0);
        if (version < kCurrentSaveVersion)
        {
            // Legacy save — incompatible. Create fresh slot.
            std::cerr << "Save version " << version << " is outdated (current: "
                      << kCurrentSaveVersion << "). Creating fresh slot.\n";
            return false;
        }

        mSaveVersion = version;
        mWorldSeed = json.value("worldSeed", randomSeed());
        mRunNumber = std::max(1, json.value("runNumber", 1));
        mTotalKills = std::max(0, json.value("totalKills", 0));
        mLongestRunDays = std::max(0, json.value("longestRunDays", 0));
        mTotalDaysSurvived = std::max(0, json.value("totalDaysSurvived", 0));

        mPresentationSettings = RunPresentationSettings{};
        if (json.contains("presentation") && json["presentation"].is_object())
        {
            const nlohmann::json& presentationJson = json["presentation"];
            mPresentationSettings.gameMode = parseSaveGameMode(presentationJson.value("gameMode", nlohmann::json("endless")));
            mPresentationSettings.threatMode = parseSaveThreatMode(presentationJson.value("threatMode", nlohmann::json("overwhelming_swarm")));
            mPresentationSettings.tonePreset = parseSaveTonePreset(presentationJson.value("tonePreset", nlohmann::json("grim_human")));

            if (presentationJson.contains("campaignObjective") && presentationJson["campaignObjective"].is_object())
            {
                const nlohmann::json& objectiveJson = presentationJson["campaignObjective"];
                mPresentationSettings.campaignObjective.active = objectiveJson.value("active", false);
                mPresentationSettings.campaignObjective.tileX = objectiveJson.value("tileX", 0);
                mPresentationSettings.campaignObjective.tileY = objectiveJson.value("tileY", 0);
                mPresentationSettings.campaignObjective.districtId = objectiveJson.value("districtId", 0);
                mPresentationSettings.campaignObjective.infectionDeadlineDay = objectiveJson.value("infectionDeadlineDay", 0);
                mPresentationSettings.campaignObjective.completed = objectiveJson.value("completed", false);
            }
        }
        return true;
    };

    std::string contents = readFileToString(mMetaPath);
    if (!contents.empty() && parseMeta(contents))
    {
        return true;
    }

    contents = readFileToString(mMetaBackupPath);
    if (!contents.empty() && parseMeta(contents))
    {
        std::error_code ec;
        std::filesystem::remove(mMetaPath, ec);
        return writeMeta();
    }

    // No valid save found — create fresh slot
    mSaveVersion = kCurrentSaveVersion;
    mWorldSeed = randomSeed();
    mRunNumber = 1;
    mTotalKills = 0;
    mLongestRunDays = 0;
    mTotalDaysSurvived = 0;
    mPresentationSettings = RunPresentationSettings{};
    mGraves.clear();
    mRetirements.clear();
    mStructures.clear();

    // Write clean files for all layers so old legacy/structure data is replaced
    writeLegacy();
    writeStructures();

    return writeMeta();
}

bool SaveManager::loadLegacy()
{
    auto parseLegacy = [&](const std::string& contents)
    {
        nlohmann::json json = nlohmann::json::parse(contents, nullptr, false);
        if (json.is_discarded())
        {
            return false;
        }

        mGraves.clear();
        if (json.contains("graves") && json["graves"].is_array())
        {
            for (const auto& graveJson : json["graves"])
            {
                GraveRecord record;
                record.name = graveJson.value("name", std::string("Unknown"));
                record.x = graveJson.value("x", 0.0f);
                record.y = graveJson.value("y", 0.0f);
                record.day = std::max(1, graveJson.value("day", 1));
                record.runIndex = std::max(0, graveJson.value("runIndex", 0));
                record.cause = graveJson.value("cause", std::string("Unknown"));
                record.risen = graveJson.value("risen", false);
                record.laidToRest = graveJson.value("laidToRest", false);
                if (graveJson.contains("items") && graveJson["items"].is_array())
                {
                    for (const auto& itemJson : graveJson["items"])
                    {
                        const int id = itemJson.value("id", -1);
                        const int count = itemJson.value("count", 0);
                        if (id >= 0 && count > 0)
                        {
                            record.items.emplace_back(id, count);
                        }
                    }
                }
                mGraves.push_back(record);
            }
        }

        mRetirements.clear();
        if (json.contains("retirements") && json["retirements"].is_array())
        {
            for (const auto& retJson : json["retirements"])
            {
                RetirementRecord record;
                record.name = retJson.value("name", std::string("Unknown"));
                record.x = retJson.value("x", 0.0f);
                record.y = retJson.value("y", 0.0f);
                record.day = std::max(1, retJson.value("day", 1));
                record.runIndex = std::max(0, retJson.value("runIndex", 0));
                mRetirements.push_back(record);
            }
        }

        mCharacterHistory.clear();
        if (json.contains("characters") && json["characters"].is_array())
        {
            for (const auto& charJson : json["characters"])
            {
                CharacterHistoryRecord record;
                record.name = charJson.value("name", std::string("Unknown"));
                record.runIndex = std::max(0, charJson.value("runIndex", 0));
                record.dayStarted = std::max(1, charJson.value("dayStarted", 1));
                record.dayEnded = std::max(record.dayStarted, charJson.value("dayEnded", 1));
                record.retired = charJson.value("retired", false);
                record.cause = charJson.value("cause", std::string{});
                record.locationTag = charJson.value("locationTag", std::string{});
                record.positiveTrait = charJson.value("positiveTrait", std::string{});
                record.negativeTrait = charJson.value("negativeTrait", std::string{});
                record.bloodlineTrait = charJson.value("bloodlineTrait", std::string{});
                record.finalNote = charJson.value("finalNote", std::string{});
                mCharacterHistory.push_back(record);
            }
        }

        return true;
    };

    std::string contents = readFileToString(mLegacyPath);
    if (!contents.empty())
    {
        if (parseLegacy(contents))
        {
            return true;
        }
    }

    contents = readFileToString(mLegacyBackupPath);
    if (!contents.empty())
    {
        if (parseLegacy(contents))
        {
            return writeLegacy();
        }
    }

    mGraves.clear();
    mRetirements.clear();
    mCharacterHistory.clear();
    return writeLegacy();
}

bool SaveManager::writeMeta() const
{
    nlohmann::json json;
    json["saveVersion"] = kCurrentSaveVersion;
    json["slot"] = mSlotName;
    json["worldSeed"] = mWorldSeed;
    json["runNumber"] = mRunNumber;
    json["totalKills"] = mTotalKills;
    json["longestRunDays"] = mLongestRunDays;
    json["totalDaysSurvived"] = mTotalDaysSurvived;

    nlohmann::json presentationJson;
    presentationJson["gameMode"] = saveGameModeToString(mPresentationSettings.gameMode);
    presentationJson["threatMode"] = saveThreatModeToString(mPresentationSettings.threatMode);
    presentationJson["tonePreset"] = saveTonePresetToString(mPresentationSettings.tonePreset);
    presentationJson["campaignObjective"] = {
        {"active", mPresentationSettings.campaignObjective.active},
        {"tileX", mPresentationSettings.campaignObjective.tileX},
        {"tileY", mPresentationSettings.campaignObjective.tileY},
        {"districtId", mPresentationSettings.campaignObjective.districtId},
        {"infectionDeadlineDay", mPresentationSettings.campaignObjective.infectionDeadlineDay},
        {"completed", mPresentationSettings.campaignObjective.completed}
    };
    json["presentation"] = presentationJson;

    return writeJsonAtomic(mMetaPath, mMetaBackupPath, json.dump(2));
}

bool SaveManager::writeLegacy() const
{
    nlohmann::json json;
    json["graves"] = nlohmann::json::array();
    json["retirements"] = nlohmann::json::array();
    json["characters"] = nlohmann::json::array();

    for (const GraveRecord& grave : mGraves)
    {
        nlohmann::json graveJson;
        graveJson["name"] = grave.name;
        graveJson["x"] = grave.x;
        graveJson["y"] = grave.y;
        graveJson["day"] = grave.day;
        graveJson["runIndex"] = grave.runIndex;
        graveJson["cause"] = grave.cause;
        if (grave.risen)
        {
            graveJson["risen"] = true;
        }
        if (grave.laidToRest)
        {
            graveJson["laidToRest"] = true;
        }
        if (!grave.items.empty())
        {
            graveJson["items"] = nlohmann::json::array();
            for (const auto& [itemId, count] : grave.items)
            {
                graveJson["items"].push_back({{"id", itemId}, {"count", count}});
            }
        }
        json["graves"].push_back(graveJson);
    }

    for (const RetirementRecord& ret : mRetirements)
    {
        nlohmann::json retJson;
        retJson["name"] = ret.name;
        retJson["x"] = ret.x;
        retJson["y"] = ret.y;
        retJson["day"] = ret.day;
        retJson["runIndex"] = ret.runIndex;
        json["retirements"].push_back(retJson);
    }

    for (const CharacterHistoryRecord& character : mCharacterHistory)
    {
        nlohmann::json characterJson;
        characterJson["name"] = character.name;
        characterJson["runIndex"] = character.runIndex;
        characterJson["dayStarted"] = character.dayStarted;
        characterJson["dayEnded"] = character.dayEnded;
        characterJson["retired"] = character.retired;
        characterJson["cause"] = character.cause;
        characterJson["locationTag"] = character.locationTag;
        characterJson["positiveTrait"] = character.positiveTrait;
        characterJson["negativeTrait"] = character.negativeTrait;
        characterJson["bloodlineTrait"] = character.bloodlineTrait;
        characterJson["finalNote"] = character.finalNote;
        json["characters"].push_back(characterJson);
    }

    return writeJsonAtomic(mLegacyPath, mLegacyBackupPath, json.dump(2));
}

bool SaveManager::hasWorldLayout() const
{
    std::error_code ec;
    if (std::filesystem::exists(mLayoutPath, ec))
    {
        return true;
    }
    if (std::filesystem::exists(mLayoutBackupPath, ec))
    {
        return true;
    }
    return false;
}

LayoutData SaveManager::loadWorldLayout() const
{
    auto parseLayout = [](const std::string& contents, LayoutData& out) -> bool
    {
        nlohmann::json json = nlohmann::json::parse(contents, nullptr, false);
        if (json.is_discarded())
        {
            return false;
        }

        out.width = json.value("width", 0);
        out.height = json.value("height", 0);
        if (out.width <= 0 || out.height <= 0)
        {
            return false;
        }

        const std::size_t total = static_cast<std::size_t>(out.width * out.height);

        if (json.contains("ground") && json["ground"].is_array() && json["ground"].size() == total)
        {
            out.ground.resize(total);
            for (std::size_t i = 0; i < total; ++i)
            {
                out.ground[i] = json["ground"][i].get<int>();
            }
        }
        else
        {
            return false;
        }

        if (json.contains("collision") && json["collision"].is_array() && json["collision"].size() == total)
        {
            out.collision.resize(total);
            for (std::size_t i = 0; i < total; ++i)
            {
                out.collision[i] = json["collision"][i].get<int>();
            }
        }
        else
        {
            return false;
        }

        if (json.contains("surface") && json["surface"].is_array() && json["surface"].size() == total)
        {
            out.surface.resize(total);
            for (std::size_t i = 0; i < total; ++i)
            {
                out.surface[i] = json["surface"][i].get<int>();
            }
        }
        else
        {
            return false;
        }

        if (json.contains("district") && json["district"].is_array() && json["district"].size() == total)
        {
            out.district.resize(total);
            for (std::size_t i = 0; i < total; ++i)
            {
                out.district[i] = json["district"][i].get<int>();
            }
        }
        else
        {
            out.district.assign(total, 0);
        }

        out.buildings.clear();
        if (json.contains("buildings") && json["buildings"].is_array())
        {
            for (const auto& bJson : json["buildings"])
            {
                LayoutData::BuildingRect b;
                b.x = bJson.value("x", 0);
                b.y = bJson.value("y", 0);
                b.w = bJson.value("w", 0);
                b.h = bJson.value("h", 0);
                out.buildings.push_back(b);
            }
        }

        out.doors.clear();
        if (json.contains("doors") && json["doors"].is_array())
        {
            for (const auto& dJson : json["doors"])
            {
                LayoutData::DoorPos d;
                d.x = dJson.value("x", 0);
                d.y = dJson.value("y", 0);
                out.doors.push_back(d);
            }
        }

        out.districts.clear();
        if (json.contains("districts") && json["districts"].is_array())
        {
            for (const auto& distJson : json["districts"])
            {
                LayoutData::DistrictDef def;
                def.id = distJson.value("id", 0);
                def.type = distJson.value("type", 0);
                def.minX = distJson.value("minX", 0);
                def.minY = distJson.value("minY", 0);
                def.maxX = distJson.value("maxX", 0);
                def.maxY = distJson.value("maxY", 0);
                if (distJson.contains("buildingIndices") && distJson["buildingIndices"].is_array())
                {
                    for (const auto& bi : distJson["buildingIndices"])
                    {
                        def.buildingIndices.push_back(bi.get<int>());
                    }
                }
                if (distJson.contains("adjacent") && distJson["adjacent"].is_array())
                {
                    for (const auto& ai : distJson["adjacent"])
                    {
                        def.adjacentDistricts.push_back(ai.get<int>());
                    }
                }
                out.districts.push_back(def);
            }
        }

        if (json.contains("playerStart") && json["playerStart"].is_object())
        {
            out.playerStart.x = json["playerStart"].value("x", 0);
            out.playerStart.y = json["playerStart"].value("y", 0);
        }

        return true;
    };

    LayoutData layout;

    std::string contents = readFileToString(mLayoutPath);
    if (!contents.empty() && parseLayout(contents, layout))
    {
        return layout;
    }

    contents = readFileToString(mLayoutBackupPath);
    if (!contents.empty() && parseLayout(contents, layout))
    {
        return layout;
    }

    return {};
}

bool SaveManager::saveWorldLayout(const LayoutData& layout)
{
    nlohmann::json json;
    json["version"] = 1;
    json["width"] = layout.width;
    json["height"] = layout.height;
    json["ground"] = layout.ground;
    json["collision"] = layout.collision;
    json["surface"] = layout.surface;
    json["district"] = layout.district;

    json["buildings"] = nlohmann::json::array();
    for (const auto& b : layout.buildings)
    {
        json["buildings"].push_back({{"x", b.x}, {"y", b.y}, {"w", b.w}, {"h", b.h}});
    }

    json["doors"] = nlohmann::json::array();
    for (const auto& d : layout.doors)
    {
        json["doors"].push_back({{"x", d.x}, {"y", d.y}});
    }

    json["districts"] = nlohmann::json::array();
    for (const auto& dist : layout.districts)
    {
        nlohmann::json dJson;
        dJson["id"] = dist.id;
        dJson["type"] = dist.type;
        dJson["minX"] = dist.minX;
        dJson["minY"] = dist.minY;
        dJson["maxX"] = dist.maxX;
        dJson["maxY"] = dist.maxY;
        dJson["buildingIndices"] = dist.buildingIndices;
        dJson["adjacent"] = dist.adjacentDistricts;
        json["districts"].push_back(dJson);
    }

    json["playerStart"] = {{"x", layout.playerStart.x}, {"y", layout.playerStart.y}};

    return writeJsonAtomic(mLayoutPath, mLayoutBackupPath, json.dump());
}

bool SaveManager::writeJsonAtomic(const std::string& targetPath, const std::string& backupPath, const std::string& jsonText) const
{
    const std::string tempPath = targetPath + ".tmp";

    {
        std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << jsonText;
        output.flush();
        if (!output.good())
        {
            return false;
        }
    }

    std::error_code ec;
    if (std::filesystem::exists(targetPath, ec))
    {
        std::filesystem::copy_file(targetPath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
        ec.clear();
        std::filesystem::remove(targetPath, ec);
        ec.clear();
    }

    std::filesystem::rename(tempPath, targetPath, ec);
    if (ec)
    {
        std::filesystem::remove(tempPath, ec);
        return false;
    }

    return true;
}

bool SaveManager::saveStructures(const std::vector<StructureRecord>& structures)
{
    mStructures = structures;
    return writeStructures();
}

bool SaveManager::loadStructures()
{
    auto parseStructures = [&](const std::string& contents)
    {
        nlohmann::json json = nlohmann::json::parse(contents, nullptr, false);
        if (json.is_discarded())
        {
            return false;
        }

        mStructures.clear();
        if (json.contains("structures") && json["structures"].is_array())
        {
            for (const auto& sJson : json["structures"])
            {
                StructureRecord rec;
                rec.type = sJson.value("type", 0);
                rec.x = sJson.value("x", 0.0f);
                rec.y = sJson.value("y", 0.0f);
                rec.condition = sJson.value("condition", 0);
                rec.hpRatio = std::clamp(sJson.value("hpRatio", 1.0f), 0.0f, 1.0f);

                // Parse cached items (for supply caches)
                if (sJson.contains("items") && sJson["items"].is_array())
                {
                    for (const auto& itemJson : sJson["items"])
                    {
                        int id = itemJson.value("id", -1);
                        int count = itemJson.value("count", 0);
                        if (id >= 0 && count > 0)
                        {
                            rec.items.emplace_back(id, count);
                        }
                    }
                }

                // Degrade between runs: Intact->Damaged, Damaged->Breaking, Breaking->removed
                if (rec.condition == 0) // Intact
                {
                    rec.condition = 1; // Damaged
                    rec.hpRatio = std::min(rec.hpRatio, 0.5f);
                }
                else if (rec.condition == 1) // Damaged
                {
                    rec.condition = 2; // Breaking
                    rec.hpRatio = std::min(rec.hpRatio, 0.25f);
                }
                else
                {
                    continue; // Breaking -> destroyed, skip
                }

                mStructures.push_back(rec);
            }
        }

        return true;
    };

    std::string contents = readFileToString(mStructurePath);
    if (!contents.empty())
    {
        if (parseStructures(contents))
        {
            return true;
        }
    }

    contents = readFileToString(mStructureBackupPath);
    if (!contents.empty())
    {
        if (parseStructures(contents))
        {
            return writeStructures();
        }
    }

    mStructures.clear();
    return true;
}

bool SaveManager::writeStructures() const
{
    nlohmann::json json;
    json["structures"] = nlohmann::json::array();

    for (const StructureRecord& s : mStructures)
    {
        nlohmann::json sJson;
        sJson["type"] = s.type;
        sJson["x"] = s.x;
        sJson["y"] = s.y;
        sJson["condition"] = s.condition;
        sJson["hpRatio"] = s.hpRatio;
        if (!s.items.empty())
        {
            sJson["items"] = nlohmann::json::array();
            for (const auto& [itemId, count] : s.items)
            {
                nlohmann::json itemJson;
                itemJson["id"] = itemId;
                itemJson["count"] = count;
                sJson["items"].push_back(itemJson);
            }
        }
        json["structures"].push_back(sJson);
    }

    return writeJsonAtomic(mStructurePath, mStructureBackupPath, json.dump(2));
}

void SaveManager::addRunStats(int kills, int daysSurvived)
{
    mTotalKills += kills;
    mTotalDaysSurvived += daysSurvived;
    if (daysSurvived > mLongestRunDays)
    {
        mLongestRunDays = daysSurvived;
    }
    writeMeta();
}

bool SaveManager::saveCheckpoint(const RunCheckpoint& cp)
{
    nlohmann::json json;
    json["runSeed"] = cp.runSeed;
    json["characterName"] = cp.characterName;
    json["currentDay"] = cp.currentDay;
    json["gameHours"] = cp.gameHours;
    json["playerX"] = cp.playerX;
    json["playerY"] = cp.playerY;
    json["healthCurrent"] = cp.healthCurrent;
    json["healthMaximum"] = cp.healthMaximum;

    // Needs
    json["hunger"] = cp.needs.hunger;
    json["thirst"] = cp.needs.thirst;
    json["fatigue"] = cp.needs.fatigue;
    json["sleepDebt"] = cp.needs.sleepDebt;
    json["bodyTemperature"] = cp.needs.bodyTemperature;
    json["illness"] = cp.needs.illness;

    // Mental state
    json["panic"] = cp.mental.panic;
    json["depression"] = cp.mental.depression;
    json["desensitisation"] = cp.mental.desensitisation;

    // Wounds
    json["wounds"] = nlohmann::json::array();
    for (const auto& w : cp.wounds)
    {
        nlohmann::json wj;
        wj["type"] = static_cast<int>(w.type);
        wj["severity"] = w.severity;
        wj["infected"] = w.infected;
        wj["infectionTimer"] = w.infectionTimer;
        wj["contaminated"] = w.contaminated;
        wj["healTimer"] = w.healTimer;
        json["wounds"].push_back(wj);
    }
    json["bleeding"] = cp.bleeding;

    // Inventory
    json["inventory"] = nlohmann::json::array();
    for (const auto& slot : cp.inventory.slots)
    {
        json["inventory"].push_back({{"id", slot.itemId}, {"count", slot.count}, {"condition", slot.condition}});
    }
    json["activeSlot"] = cp.inventory.activeSlot;
    json["equippedSlot"] = cp.equippedSlot;

    // Stats
    json["kills"] = cp.kills;
    json["itemsLooted"] = cp.itemsLooted;
    json["structuresBuilt"] = cp.structuresBuilt;
    json["distanceWalked"] = cp.distanceWalked;

    // The town as this run has changed it.
    const auto tilesToJson = [](const std::vector<std::pair<int,int>>& tiles)
    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& [x, y] : tiles)
        {
            arr.push_back({x, y});
        }
        return arr;
    };
    json["openDoors"] = tilesToJson(cp.openDoors);
    json["brokenDoors"] = tilesToJson(cp.brokenDoors);
    json["boardedDoors"] = nlohmann::json::array();
    for (const BoardedDoorRecord& door : cp.boardedDoors)
    {
        json["boardedDoors"].push_back({{"x", door.x}, {"y", door.y}, {"hp", door.boardHp}});
    }
    json["burned"] = tilesToJson(cp.burnedTiles);
    json["fires"] = nlohmann::json::array();
    for (const FireTileRecord& fire : cp.fires)
    {
        json["fires"].push_back(
            {{"tileX", fire.tileX}, {"tileY", fire.tileY}, {"intensity", fire.intensity}, {"lifeRemaining", fire.lifeRemaining}});
    }
    json["zombies"] = nlohmann::json::array();
    for (const ZombieRecord& z : cp.zombies)
    {
        json["zombies"].push_back({{"x", z.x}, {"y", z.y}, {"hp", z.health}, {"maxHp", z.maxHealth},
                                   {"speed", z.baseMoveSpeed}, {"day", z.spawnDay}});
    }
    json["corpses"] = nlohmann::json::array();
    for (const CorpseRecord& c : cp.corpses)
    {
        json["corpses"].push_back({{"x", c.x}, {"y", c.y}, {"flip", c.flipX}});
    }

    const std::string tempPath = mCheckpointPath + ".tmp";
    {
        std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output << json.dump();
        output.flush();
        if (!output.good()) return false;
    }

    std::error_code ec;
    if (std::filesystem::exists(mCheckpointPath, ec))
    {
        std::filesystem::remove(mCheckpointPath, ec);
        ec.clear();
    }
    std::filesystem::rename(tempPath, mCheckpointPath, ec);
    return !ec;
}

RunCheckpoint SaveManager::loadCheckpoint() const
{
    RunCheckpoint cp;
    std::string contents = readFileToString(mCheckpointPath);
    if (contents.empty()) return cp;

    nlohmann::json json = nlohmann::json::parse(contents, nullptr, false);
    if (json.is_discarded()) return cp;

    cp.runSeed = json.value("runSeed", 0u);
    if (cp.runSeed != currentRunSeed()) return cp; // Checkpoint belongs to a different run

    cp.valid = true;
    cp.characterName = json.value("characterName", std::string("Unknown"));
    cp.currentDay = json.value("currentDay", 1);
    cp.gameHours = json.value("gameHours", 8.0);
    cp.playerX = json.value("playerX", 0.0f);
    cp.playerY = json.value("playerY", 0.0f);
    cp.healthCurrent = json.value("healthCurrent", 100.0f);
    cp.healthMaximum = json.value("healthMaximum", 100.0f);

    cp.needs.hunger = json.value("hunger", 80.0f);
    cp.needs.thirst = json.value("thirst", 80.0f);
    cp.needs.fatigue = json.value("fatigue", 0.0f);
    cp.needs.sleepDebt = json.value("sleepDebt", 0.0f);
    cp.needs.bodyTemperature = json.value("bodyTemperature", 37.0f);
    cp.needs.illness = json.value("illness", 0.0f);

    cp.mental.panic = json.value("panic", 0.0f);
    cp.mental.depression = json.value("depression", 0.0f);
    cp.mental.desensitisation = json.value("desensitisation", 0.0f);

    if (json.contains("wounds") && json["wounds"].is_array())
    {
        for (const auto& wj : json["wounds"])
        {
            Wound w;
            w.type = static_cast<WoundType>(wj.value("type", 0));
            w.severity = wj.value("severity", 1.0f);
            w.infected = wj.value("infected", false);
            w.infectionTimer = wj.value("infectionTimer", 0.0f);
            w.contaminated = wj.value("contaminated", w.type == WoundType::Bite);
            w.healTimer = wj.value("healTimer", 0.0f);
            cp.wounds.push_back(w);
        }
    }
    cp.bleeding = json.value("bleeding", false);

    if (json.contains("inventory") && json["inventory"].is_array())
    {
        const auto& arr = json["inventory"];
        for (std::size_t i = 0; i < arr.size() && i < Inventory::kMaxSlots; ++i)
        {
            cp.inventory.slots[i].itemId = arr[i].value("id", -1);
            cp.inventory.slots[i].count = arr[i].value("count", 0);
            cp.inventory.slots[i].condition = arr[i].value("condition", 1.0f);
        }
    }
    cp.inventory.activeSlot = json.value("activeSlot", 0);
    cp.equippedSlot = json.value("equippedSlot", -1);

    cp.kills = json.value("kills", 0);
    cp.itemsLooted = json.value("itemsLooted", 0);
    cp.structuresBuilt = json.value("structuresBuilt", 0);
    cp.distanceWalked = json.value("distanceWalked", 0.0f);

    const auto tilesFromJson = [&json](const char* key)
    {
        std::vector<std::pair<int,int>> tiles;
        if (json.contains(key) && json[key].is_array())
        {
            for (const auto& tj : json[key])
            {
                if (tj.is_array() && tj.size() == 2 && tj[0].is_number_integer() && tj[1].is_number_integer())
                {
                    tiles.emplace_back(tj[0].get<int>(), tj[1].get<int>());
                }
            }
        }
        return tiles;
    };
    cp.openDoors = tilesFromJson("openDoors");
    cp.brokenDoors = tilesFromJson("brokenDoors");
    if (json.contains("boardedDoors") && json["boardedDoors"].is_array())
    {
        cp.boardsSaved = true;
        for (const auto& dj : json["boardedDoors"])
        {
            const BoardedDoorRecord door{dj.value("x", 0), dj.value("y", 0), dj.value("hp", 0.0f)};
            if (door.boardHp > 0.0f) cp.boardedDoors.push_back(door);
        }
    }
    cp.burnedTiles = tilesFromJson("burned");
    if (json.contains("fires") && json["fires"].is_array())
    {
        for (const auto& fj : json["fires"])
        {
            FireTileRecord fire;
            fire.tileX = fj.value("tileX", 0);
            fire.tileY = fj.value("tileY", 0);
            fire.intensity = std::clamp(fj.value("intensity", 0.0f), 0.0f, 2.0f);
            fire.lifeRemaining = std::max(0.0f, fj.value("lifeRemaining", 0.0f));
            if (fire.lifeRemaining > 0.0f && fire.intensity > 0.0f)
            {
                cp.fires.push_back(fire);
            }
        }
    }
    if (json.contains("zombies") && json["zombies"].is_array())
    {
        cp.zombiesSaved = true;
        for (const auto& zj : json["zombies"])
        {
            ZombieRecord z;
            z.x = zj.value("x", 0.0f);
            z.y = zj.value("y", 0.0f);
            z.maxHealth = std::max(1.0f, zj.value("maxHp", 50.0f));
            z.health = std::clamp(zj.value("hp", z.maxHealth), 0.0f, z.maxHealth);
            z.baseMoveSpeed = std::clamp(zj.value("speed", 112.0f), 10.0f, 400.0f);
            z.spawnDay = std::max(1, zj.value("day", 1));
            if (z.health > 0.0f)
            {
                cp.zombies.push_back(z);
            }
        }
    }
    if (json.contains("corpses") && json["corpses"].is_array())
    {
        for (const auto& cj : json["corpses"])
        {
            CorpseRecord c;
            c.x = cj.value("x", 0.0f);
            c.y = cj.value("y", 0.0f);
            c.flipX = cj.value("flip", false);
            cp.corpses.push_back(c);
        }
    }

    return cp;
}

void SaveManager::deleteCheckpoint()
{
    std::error_code ec;
    std::filesystem::remove(mCheckpointPath, ec);
}

bool SaveManager::addLootedPosition(int tileX, int tileY)
{
    for (const auto& [x, y] : mLootedPositions)
    {
        if (x == tileX && y == tileY) return false; // already tracked
    }
    mLootedPositions.emplace_back(tileX, tileY);
    return true;
}

bool SaveManager::addDiscoveredRecipe(int itemA, int itemB)
{
    if (itemA < 0 || itemB < 0)
    {
        return false;
    }

    const auto normalized = normalizeRecipePair(itemA, itemB);
    for (const auto& [a, b] : mDiscoveredRecipes)
    {
        if (a == normalized.first && b == normalized.second)
        {
            return false;
        }
    }

    mDiscoveredRecipes.push_back(normalized);
    return true;
}

bool SaveManager::isRecipeDiscovered(int itemA, int itemB) const
{
    if (itemA < 0 || itemB < 0)
    {
        return false;
    }

    const auto normalized = normalizeRecipePair(itemA, itemB);
    for (const auto& [a, b] : mDiscoveredRecipes)
    {
        if (a == normalized.first && b == normalized.second)
        {
            return true;
        }
    }

    return false;
}

void SaveManager::setVehicleRecords(std::vector<VehicleRecord> vehicles)
{
    mVehicles = std::move(vehicles);
}

void SaveManager::setFireTileRecords(std::vector<FireTileRecord> fireTiles)
{
    mFireTiles = std::move(fireTiles);
}

void SaveManager::setBoardedDoors(std::vector<BoardedDoorRecord> boardedDoors)
{
    mBoardedDoors.clear();
    for (const BoardedDoorRecord& door : boardedDoors)
    {
        if (door.boardHp > 0.0f) mBoardedDoors.push_back(door);
    }
}

void SaveManager::setBrokenDoors(std::vector<std::pair<int,int>> brokenDoors)
{
    mBrokenDoors.clear();
    for (const auto& [x, y] : brokenDoors)
    {
        addUniqueTile(mBrokenDoors, x, y);
    }
}

void SaveManager::setBurnedTiles(std::vector<std::pair<int,int>> burnedTiles)
{
    mBurnedTiles.clear();
    mBurnedTiles.reserve(burnedTiles.size());
    for (const auto& [x, y] : burnedTiles)
    {
        addUniqueTile(mBurnedTiles, x, y);
    }
}

bool SaveManager::saveLootedPositions()
{
    nlohmann::json json;
    json["looted"] = nlohmann::json::array();
    for (const auto& [x, y] : mLootedPositions)
    {
        json["looted"].push_back({{"x", x}, {"y", y}});
    }

    json["recipes"] = nlohmann::json::array();
    for (const auto& [a, b] : mDiscoveredRecipes)
    {
        json["recipes"].push_back({{"a", a}, {"b", b}});
    }

    json["vehicles"] = nlohmann::json::array();
    for (const auto& vehicle : mVehicles)
    {
        nlohmann::json vehicleJson;
        vehicleJson["x"] = vehicle.x;
        vehicleJson["y"] = vehicle.y;
        vehicleJson["condition"] = vehicle.condition;
        vehicleJson["fuel"] = vehicle.fuel;
        vehicleJson["maxFuel"] = vehicle.maxFuel;
        vehicleJson["engineOn"] = vehicle.engineOn;

        if (!vehicle.items.empty())
        {
            vehicleJson["items"] = nlohmann::json::array();
            for (const auto& [itemId, count] : vehicle.items)
            {
                vehicleJson["items"].push_back({{"id", itemId}, {"count", count}});
            }
        }

        json["vehicles"].push_back(vehicleJson);
    }

    json["fires"] = nlohmann::json::array();
    for (const auto& fire : mFireTiles)
    {
        json["fires"].push_back(
            {{"tileX", fire.tileX}, {"tileY", fire.tileY}, {"intensity", fire.intensity}, {"lifeRemaining", fire.lifeRemaining}});
    }

    json["burned"] = nlohmann::json::array();
    for (const auto& [x, y] : mBurnedTiles)
    {
        json["burned"].push_back({{"x", x}, {"y", y}});
    }

    json["brokenDoors"] = nlohmann::json::array();
    for (const auto& [x, y] : mBrokenDoors)
    {
        json["brokenDoors"].push_back({{"x", x}, {"y", y}});
    }
    json["boardedDoors"] = nlohmann::json::array();
    for (const BoardedDoorRecord& door : mBoardedDoors)
    {
        json["boardedDoors"].push_back({{"x", door.x}, {"y", door.y}, {"hp", door.boardHp}});
    }

    json["events"] = nlohmann::json::array();
    for (const WorldMemoryEventRecord& event : mWorldEvents)
    {
        json["events"].push_back(
            {
                {"type", worldMemoryEventTypeToString(event.type)},
                {"tileX", event.tileX},
                {"tileY", event.tileY},
                {"day", event.day},
                {"runIndex", event.runIndex},
                {"detail", event.detail}
            });
    }

    const std::string tempPath = mWorldMemoryPath + ".tmp";
    {
        std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output << json.dump();
        output.flush();
        if (!output.good()) return false;
    }
    std::error_code ec;
    if (std::filesystem::exists(mWorldMemoryPath, ec))
    {
        std::filesystem::remove(mWorldMemoryPath, ec);
        ec.clear();
    }
    std::filesystem::rename(tempPath, mWorldMemoryPath, ec);
    return !ec;
}

bool SaveManager::saveDiscoveredRecipes()
{
    return saveLootedPositions();
}

bool SaveManager::loadLootedPositions()
{
    mLootedPositions.clear();
    mDiscoveredRecipes.clear();
    mVehicles.clear();
    mFireTiles.clear();
    mBurnedTiles.clear();
    mBrokenDoors.clear();
    mBoardedDoors.clear();
    mWorldEvents.clear();
    std::string contents = readFileToString(mWorldMemoryPath);
    if (contents.empty()) return true;

    nlohmann::json json = nlohmann::json::parse(contents, nullptr, false);
    if (json.is_discarded()) return false;

    if (json.contains("looted") && json["looted"].is_array())
    {
        for (const auto& lj : json["looted"])
        {
            int x = lj.value("x", 0);
            int y = lj.value("y", 0);
            mLootedPositions.emplace_back(x, y);
        }
    }

    if (json.contains("recipes") && json["recipes"].is_array())
    {
        for (const auto& rj : json["recipes"])
        {
            const int a = rj.value("a", -1);
            const int b = rj.value("b", -1);
            if (a < 0 || b < 0)
            {
                continue;
            }

            const auto normalized = normalizeRecipePair(a, b);
            bool seen = false;
            for (const auto& [ea, eb] : mDiscoveredRecipes)
            {
                if (ea == normalized.first && eb == normalized.second)
                {
                    seen = true;
                    break;
                }
            }

            if (!seen)
            {
                mDiscoveredRecipes.push_back(normalized);
            }
        }
    }

    if (json.contains("vehicles") && json["vehicles"].is_array())
    {
        for (const auto& vj : json["vehicles"])
        {
            VehicleRecord record;
            record.x = vj.value("x", 0.0f);
            record.y = vj.value("y", 0.0f);
            record.condition = std::clamp(vj.value("condition", 100.0f), 0.0f, 100.0f);
            record.fuel = std::max(0.0f, vj.value("fuel", 0.0f));
            record.maxFuel = std::max(1.0f, vj.value("maxFuel", 30.0f));
            record.engineOn = vj.value("engineOn", false);

            if (vj.contains("items") && vj["items"].is_array())
            {
                for (const auto& ij : vj["items"])
                {
                    const int itemId = ij.value("id", -1);
                    const int count = ij.value("count", 0);
                    if (itemId >= 0 && count > 0)
                    {
                        record.items.emplace_back(itemId, count);
                    }
                }
            }

            mVehicles.push_back(record);
        }
    }

    if (json.contains("fires") && json["fires"].is_array())
    {
        for (const auto& fj : json["fires"])
        {
            FireTileRecord fire;
            fire.tileX = fj.value("tileX", 0);
            fire.tileY = fj.value("tileY", 0);
            fire.intensity = std::clamp(fj.value("intensity", 0.0f), 0.0f, 2.0f);
            fire.lifeRemaining = std::max(0.0f, fj.value("lifeRemaining", 0.0f));
            if (fire.lifeRemaining > 0.0f && fire.intensity > 0.0f)
            {
                mFireTiles.push_back(fire);
            }
        }
    }

    if (json.contains("burned") && json["burned"].is_array())
    {
        for (const auto& bj : json["burned"])
        {
            const int x = bj.value("x", 0);
            const int y = bj.value("y", 0);
            addUniqueTile(mBurnedTiles, x, y);
        }
    }

    if (json.contains("brokenDoors") && json["brokenDoors"].is_array())
    {
        for (const auto& dj : json["brokenDoors"])
        {
            addUniqueTile(mBrokenDoors, dj.value("x", 0), dj.value("y", 0));
        }
    }
    if (json.contains("boardedDoors") && json["boardedDoors"].is_array())
    {
        for (const auto& dj : json["boardedDoors"])
        {
            const BoardedDoorRecord door{dj.value("x", 0), dj.value("y", 0), dj.value("hp", 0.0f)};
            if (door.boardHp > 0.0f) mBoardedDoors.push_back(door);
        }
    }

    if (json.contains("events") && json["events"].is_array())
    {
        for (const auto& ej : json["events"])
        {
            WorldMemoryEventRecord event;
            event.type = parseWorldMemoryEventType(ej.value("type", nlohmann::json("other")));
            event.tileX = ej.value("tileX", 0);
            event.tileY = ej.value("tileY", 0);
            event.day = std::max(1, ej.value("day", 1));
            event.runIndex = std::max(0, ej.value("runIndex", 0));
            event.detail = ej.value("detail", std::string{});
            mWorldEvents.push_back(event);
        }
    }

    return true;
}

bool SaveManager::saveSurvivors(const std::vector<SurvivorRecord>& survivors)
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : survivors)
    {
        nlohmann::json j;
        j["name"] = s.name;
        j["alive"] = s.alive;
        j["spawnDay"] = s.spawnDay;
        j["deathDay"] = s.deathDay;
        j["x"] = s.x;
        j["y"] = s.y;
        j["hunger"] = s.hunger;
        j["thirst"] = s.thirst;
        j["aggression"] = s.aggression;
        j["cooperation"] = s.cooperation;
        j["competence"] = s.competence;
        j["homeDistrictId"] = s.homeDistrictId;
        arr.push_back(j);
    }
    nlohmann::json root;
    root["survivors"] = arr;

    const std::string text = root.dump(2);
    const std::string tempPath = mSurvivorPath + ".tmp";
    std::filesystem::path parentDir = std::filesystem::path(mSurvivorPath).parent_path();
    std::error_code ec;
    std::filesystem::create_directories(parentDir, ec);
    std::ofstream ofs(tempPath);
    if (!ofs) return false;
    ofs << text;
    ofs.close();
    std::filesystem::rename(tempPath, mSurvivorPath, ec);
    return !ec;
}

std::vector<SurvivorRecord> SaveManager::loadSurvivors() const
{
    std::vector<SurvivorRecord> result;
    std::string contents = readFileToString(mSurvivorPath);
    if (contents.empty()) return result;

    nlohmann::json json = nlohmann::json::parse(contents, nullptr, false);
    if (json.is_discarded()) return result;

    if (json.contains("survivors") && json["survivors"].is_array())
    {
        for (const auto& sj : json["survivors"])
        {
            SurvivorRecord r;
            r.name = sj.value("name", "");
            r.alive = sj.value("alive", true);
            r.spawnDay = sj.value("spawnDay", 1);
            r.deathDay = sj.value("deathDay", 0);
            r.x = sj.value("x", 0.0f);
            r.y = sj.value("y", 0.0f);
            r.hunger = sj.value("hunger", 80.0f);
            r.thirst = sj.value("thirst", 80.0f);
            r.aggression = sj.value("aggression", 0.5f);
            r.cooperation = sj.value("cooperation", 0.5f);
            r.competence = sj.value("competence", 0.5f);
            r.homeDistrictId = sj.value("homeDistrictId", 0);
            result.push_back(r);
        }
    }
    return result;
}

void SaveManager::setSeedOverride(std::uint32_t seed)
{
    gSeedOverride = seed;
}
