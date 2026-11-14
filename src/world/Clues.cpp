#include "world/Clues.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "world/BuildingKinds.h"

namespace
{
using K = FurnitureKind;

std::uint32_t hashMix(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

void replaceAll(std::string& text, const std::string& from, const std::string& to)
{
    std::size_t at = 0;
    while ((at = text.find(from, at)) != std::string::npos)
    {
        text.replace(at, from.size(), to);
        at += to.size();
    }
}
} // namespace

namespace clues
{
int count(Mask known)
{
    int n = 0;
    for (int lead = 1; lead <= kLeadCount; ++lead) n += knows(known, lead) ? 1 : 0;
    return n;
}

const char* sourceKey(Source source)
{
    switch (source)
    {
    case Source::FridgeNote: return "fridge_note";
    case Source::WallMap: return "wall_map";
    case Source::DutyBoard: return "duty_board";
    case Source::LogBook: return "log_book";
    case Source::OrderBook: return "order_book";
    case Source::GarageNote: return "garage_note";
    case Source::Manual: return "manual";
    case Source::ControlBox: return "control_box";
    case Source::Broadcast: return "broadcast";
    case Source::Nearby: return "nearby";
    case Source::Survivor: return "survivor";
    case Source::Notebook: return "notebook";
    }
    return "";
}

Mask teaches(Source source)
{
    switch (source)
    {
    case Source::FridgeNote: return bit(WayOut);
    case Source::WallMap: return bit(Where);
    case Source::DutyBoard: return bit(Sergeant);
    case Source::LogBook: return static_cast<Mask>(bit(Power) | bit(Sergeant));
    case Source::OrderBook: return bit(Fuse);
    case Source::GarageNote: return bit(Fuse);
    case Source::Manual: return bit(Fixable);
    case Source::ControlBox: return bit(Power);
    case Source::Broadcast: return bit(WayOut);
    case Source::Nearby: return bit(Where);
    case Source::Survivor:
    case Source::Notebook: break; // what they knew
    }
    return 0;
}

const char* readableName(Source source)
{
    switch (source)
    {
    case Source::FridgeNote: return "A note on the fridge";
    case Source::WallMap: return "A map on the wall";
    case Source::DutyBoard: return "The duty board";
    case Source::LogBook: return "The gate log book";
    case Source::OrderBook: return "The order book";
    case Source::GarageNote: return "A note by the tools";
    case Source::Manual: return "A generator manual";
    default: break;
    }
    return "Something written";
}

Mask fromSurvivor(Mask known, float trust, float competence)
{
    Mask taught = 0;
    if (trust >= kSurvivorTrust) taught |= bit(WayOut);
    if (knows(static_cast<Mask>(known | taught), WayOut) && competence > kSurvivorCompetence) taught |= bit(Where);
    return static_cast<Mask>(taught & ~known);
}

bool fridgeNoteIn(const LayoutData::BuildingRect& rect)
{
    return buildingkinds::roll(rect, 0xF0D3u) < 1.0f / 6.0f;
}

std::vector<Readable> place(const std::vector<FurniturePiece>& pieces, const LayoutData& layout, int startHome)
{
    std::vector<Readable> out;
    std::vector<glm::ivec2> used;
    const auto take = [&](int building, std::initializer_list<FurnitureKind> kinds, Source source)
    {
        // The first piece of the first kind that the building has; else any
        // piece there that nothing else lies on.
        for (const FurnitureKind kind : kinds)
        {
            for (const FurniturePiece& p : pieces)
            {
                if (p.building != building || p.kind != kind || p.armoury) continue;
                if (std::find(used.begin(), used.end(), p.tile) != used.end()) continue;
                used.push_back(p.tile);
                out.push_back(Readable{source, p.tile, building});
                return;
            }
        }
        for (const FurniturePiece& p : pieces)
        {
            if (p.building != building || p.armoury || p.kind == K::FloorLamp || p.kind == K::Plant) continue;
            if (std::find(used.begin(), used.end(), p.tile) != used.end()) continue;
            used.push_back(p.tile);
            out.push_back(Readable{source, p.tile, building});
            return;
        }
    };
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        const LayoutData::BuildingRect& rect = layout.buildings[i];
        const int building = static_cast<int>(i) + 1;
        switch (buildingkinds::kindOf(rect))
        {
        case BuildingKind::House:
            if (fridgeNoteIn(rect) || building == startHome) take(building, {K::Fridge, K::Counter, K::Table}, Source::FridgeNote);
            break;
        case BuildingKind::Police:
            take(building, {K::FilingCabinet, K::Desk}, Source::WallMap);
            take(building, {K::Locker, K::Desk}, Source::DutyBoard);
            break;
        case BuildingKind::GuardHut:
            take(building, {K::Desk, K::Locker}, Source::LogBook);
            break;
        case BuildingKind::Hardware:
            take(building, {K::Checkout, K::ToolRack}, Source::OrderBook);
            break;
        case BuildingKind::Garage:
            take(building, {K::Workbench, K::ToolRack}, Source::Manual);
            take(building, {K::ToolRack, K::Barrels, K::MetalRack}, Source::GarageNote);
            break;
        default:
            // A start home that is not a house still gets its note.
            if (building == startHome) take(building, {K::Fridge, K::Counter, K::Table}, Source::FridgeNote);
            break;
        }
    }
    return out;
}

glm::ivec2 fuseCabinet(const std::vector<FurniturePiece>& pieces, const LayoutData& layout)
{
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        if (buildingkinds::kindOf(layout.buildings[i]) != BuildingKind::Hardware) continue;
        const int building = static_cast<int>(i) + 1;
        // In the stock room, a cabinet or a rack; else anything that holds things.
        for (const bool stockOnly : {true, false})
        {
            for (const FurnitureKind kind : {K::MetalRack, K::Locker, K::FilingCabinet, K::Crates, K::ToolRack, K::Barrels})
            {
                for (const FurniturePiece& p : pieces)
                {
                    if (p.building != building || p.kind != kind || (stockOnly && !p.stock)) continue;
                    if (furniture::info(p.kind).prompt == nullptr) continue;
                    return p.tile;
                }
            }
        }
    }
    return glm::ivec2(-1);
}

Seen seenFrom(const gate::State& state, int run, int day)
{
    Seen seen;
    seen.valid = true;
    seen.run = run;
    seen.day = day;
    seen.fuse = state.fuseFitted;
    seen.fuel = state.fuel;
    seen.repairs = state.repairs;
    seen.freed = state.freed;
    seen.key = static_cast<int>(state.key);
    seen.open = state.open;
    return seen;
}

void transfer(Mask& known, Seen& seen, Mask written, const Seen& writtenSeen)
{
    known = static_cast<Mask>(known | written);
    if (!writtenSeen.valid) return;
    const bool newer = !seen.valid || writtenSeen.run > seen.run || (writtenSeen.run == seen.run && writtenSeen.day > seen.day);
    if (newer) seen = writtenSeen;
}

Content defaults()
{
    Content c;
    c.leads = {"They sealed the town. One gate still opens: the service gate, {side} side.",
               "The service gate is on the {side} side, where the road meets the wall.",
               "Gate motor: fuse blown, generator seized, tank dry.",
               "Sgt. {sergeant} has the gate key. He walks {street}.",
               "Gate fuses are kept at the hardware store, in the stock room cabinet.",
               "It can be freed with tools and patience."};
    c.readables = {
        {"fridge_note", "Radio says they sealed the town. The service gate on the {side} side still opens. We go tonight."},
        {"wall_map", "A town map. The service gate on the {side} side is circled in red."},
        {"duty_board", "Gate detail. Key held by Sgt. {sergeant}. Off post since day 3."},
        {"log_book", "Motor dead. Fuse blown, generator seized, tank dry. Sgt. {sergeant} took the key when he went to find his kid. {street}."},
        {"order_book", "Gate fuses, two. Stock room, locked cabinet."},
        {"garage_note", "Gate fuse went back to the hardware store. Stock room."},
        {"manual", "Seized generator: work the flywheel loose with tools, an hour at a time. It can be freed with tools and patience."}};
    c.broadcast = {"This is a recorded message for the town.", "The town is under quarantine. Stay away from the wall.",
                   "One gate still opens: the service gate, {side} side."};
    c.sergeants = {"Hale", "Moran", "Pike", "Doyle", "Kerr", "Novak"};
    c.streets = {"Mill Street", "Station Road", "Chapel Lane", "Orchard Street", "Canal Road", "Market Street"};
    return c;
}

bool loadContent(const std::string& path, Content& out, std::string* error)
{
    out = defaults();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    const nlohmann::json json = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (json.is_discarded() || !json.is_object())
    {
        if (error != nullptr) *error = "not JSON: " + path;
        return false;
    }
    if (json.contains("leads") && json["leads"].is_array())
    {
        for (std::size_t i = 0; i < json["leads"].size() && i < out.leads.size(); ++i)
        {
            if (json["leads"][i].is_string()) out.leads[i] = json["leads"][i].get<std::string>();
        }
    }
    if (json.contains("readables") && json["readables"].is_object())
    {
        for (auto& [key, text] : out.readables)
        {
            if (json["readables"].contains(key) && json["readables"][key].is_string()) text = json["readables"][key].get<std::string>();
        }
    }
    const auto strings = [&](const char* key, std::vector<std::string>& into)
    {
        if (!json.contains(key) || !json[key].is_array()) return;
        std::vector<std::string> read;
        for (const auto& item : json[key])
        {
            if (item.is_string()) read.push_back(item.get<std::string>());
        }
        if (!read.empty()) into = std::move(read);
    };
    strings("broadcast", out.broadcast);
    strings("sergeants", out.sergeants);
    strings("streets", out.streets);
    return true;
}

std::string readableText(const Content& content, Source source)
{
    const std::string key = sourceKey(source);
    for (const auto& [k, text] : content.readables)
    {
        if (k == key) return text;
    }
    return "";
}

Names names(const Content& content, std::uint32_t worldSeed, int side)
{
    Names n;
    n.side = gate::sideName(side);
    if (!content.sergeants.empty()) n.sergeant = content.sergeants[hashMix(worldSeed, 0x5E26u) % content.sergeants.size()];
    if (!content.streets.empty()) n.street = content.streets[hashMix(worldSeed, 0x57EEu) % content.streets.size()];
    return n;
}

std::string fill(const std::string& text, const Names& names)
{
    std::string out = text;
    replaceAll(out, "{side}", names.side);
    replaceAll(out, "{sergeant}", names.sergeant);
    replaceAll(out, "{street}", names.street);
    return out;
}

std::vector<std::string> pageLines(Mask known, const Seen& seen, const Content& content, const Names& names, int currentRun,
                                   int currentDay)
{
    std::vector<std::string> lines;
    for (int lead = 1; lead <= kLeadCount; ++lead)
    {
        if (knows(known, lead)) lines.push_back(fill(content.leads[static_cast<std::size_t>(lead - 1)], names));
    }
    if (!knows(known, Power) && !seen.valid) return lines;
    if (!seen.valid)
    {
        lines.emplace_back("The box itself: not seen yet.");
        return lines;
    }
    if (seen.open)
    {
        lines.emplace_back("The gate stands open.");
        return lines;
    }
    std::string when;
    if (seen.run < currentRun) when = ", as of day " + std::to_string(seen.day) + " of an earlier life";
    else if (seen.day < currentDay) when = ", as of day " + std::to_string(seen.day);
    lines.push_back(std::string("Fuse: ") + (seen.fuse ? "fitted" : "blown") + when + ".");
    lines.push_back("Fuel: " + std::to_string(seen.fuel) + " of " + std::to_string(gate::kFuelNeeded) + when + ".");
    std::string generator = seen.freed ? "freed" : "seized";
    if (!seen.freed && seen.repairs > 0)
    {
        generator += ", " + std::to_string(seen.repairs) + " of " + std::to_string(gate::kRepairSessions) + " sessions done";
    }
    lines.push_back("Generator: " + generator + when + ".");
    const char* key = seen.key == static_cast<int>(gate::KeyPlace::Sergeant) ? "with the Sergeant"
                    : seen.key == static_cast<int>(gate::KeyPlace::Turned)   ? "in the box"
                                                                             : "not with the Sergeant any more";
    lines.push_back(std::string("Key: ") + key + when + ".");
    return lines;
}

std::string objective(Mask known, const Seen& seen, const Names& names, const Facts& facts)
{
    if (seen.valid && seen.open) return "The gate stands open.";
    if (!knows(known, WayOut) && !knows(known, Where)) return "Find out why nobody has left.";
    if (!knows(known, Where)) return "Find the service gate, " + names.side + " side.";
    if (!knows(known, Power) && !seen.valid) return "Look at the gate's control box.";
    const bool fuse = seen.valid && seen.fuse;
    const bool freed = seen.valid && seen.freed;
    const int fuel = seen.valid ? seen.fuel : 0;
    if (!fuse)
    {
        if (facts.hasFuse) return "Fit the fuse in the gate box.";
        if (knows(known, Fuse)) return "Get a fuse from the hardware store.";
        return "Find a gate fuse.";
    }
    if (!freed)
    {
        if (!facts.canFree) return "Find out how to free the generator.";
        return facts.hasToolbox ? "Free the generator." : "Bring a toolbox to the generator.";
    }
    if (fuel < gate::kFuelNeeded)
    {
        const std::string count = std::to_string(fuel) + " of " + std::to_string(gate::kFuelNeeded);
        return facts.hasFuel ? "Pour fuel into the generator: " + count + "." : "Bring fuel for the generator: " + count + ".";
    }
    if (!facts.keyInHand)
    {
        const bool withHim = !seen.valid || seen.key == static_cast<int>(gate::KeyPlace::Sergeant);
        if (knows(known, Sergeant) && withHim) return "Find the Sergeant, " + names.street + ".";
        return "Find the gate key.";
    }
    return "Start the gate motor.";
}
} // namespace clues
