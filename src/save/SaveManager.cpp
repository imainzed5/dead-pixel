#include "save/SaveManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace
{
std::uint32_t randomSeed()
{
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
                record.cause = graveJson.value("cause", std::string("Unknown"));
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
                mRetirements.push_back(record);
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

    return writeJsonAtomic(mMetaPath, mMetaBackupPath, json.dump(2));
}

bool SaveManager::writeLegacy() const
{
    nlohmann::json json;
    json["graves"] = nlohmann::json::array();
    json["retirements"] = nlohmann::json::array();

    for (const GraveRecord& grave : mGraves)
    {
        nlohmann::json graveJson;
        graveJson["name"] = grave.name;
        graveJson["x"] = grave.x;
        graveJson["y"] = grave.y;
        graveJson["day"] = grave.day;
        graveJson["cause"] = grave.cause;
        json["graves"].push_back(graveJson);
    }

    for (const RetirementRecord& ret : mRetirements)
    {
        nlohmann::json retJson;
        retJson["name"] = ret.name;
        retJson["x"] = ret.x;
        retJson["y"] = ret.y;
        retJson["day"] = ret.day;
        json["retirements"].push_back(retJson);
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

    // Stats
    json["kills"] = cp.kills;
    json["itemsLooted"] = cp.itemsLooted;
    json["structuresBuilt"] = cp.structuresBuilt;
    json["distanceWalked"] = cp.distanceWalked;

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

    cp.kills = json.value("kills", 0);
    cp.itemsLooted = json.value("itemsLooted", 0);
    cp.structuresBuilt = json.value("structuresBuilt", 0);
    cp.distanceWalked = json.value("distanceWalked", 0.0f);

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

bool SaveManager::saveLootedPositions()
{
    nlohmann::json json;
    json["looted"] = nlohmann::json::array();
    for (const auto& [x, y] : mLootedPositions)
    {
        json["looted"].push_back({{"x", x}, {"y", y}});
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

bool SaveManager::loadLootedPositions()
{
    mLootedPositions.clear();
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
