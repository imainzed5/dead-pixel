#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"
#include "world/Furniture.h"
#include "world/Gate.h"

// The way out's clues (overhaul phase 9): a chain of six leads, each a fact
// a character knows or not. Where they can be learned follows from the
// layout and the furniture; the words are in assets/data/clues.json. Pure.
namespace clues
{
inline constexpr int kLeadCount = 6;
enum Lead : int
{
    WayOut = 1,   // there is a way out
    Where = 2,    // where the gate is
    Power = 3,    // it needs power
    Sergeant = 4, // the Sergeant has the key
    Fuse = 5,     // where a fuse is
    Fixable = 6   // the generator can be freed
};

// Known leads, one bit each (lead 1 is bit 0).
using Mask = std::uint8_t;
[[nodiscard]] constexpr Mask bit(int lead)
{
    return lead >= 1 && lead <= kLeadCount ? static_cast<Mask>(1u << (lead - 1)) : static_cast<Mask>(0);
}
[[nodiscard]] constexpr bool knows(Mask known, int lead) { return (known & bit(lead)) != 0; }
[[nodiscard]] int count(Mask known);
[[nodiscard]] constexpr Mask all() { return static_cast<Mask>((1u << kLeadCount) - 1u); }

// Where a lead can be learned.
enum class Source : int
{
    FridgeNote,
    WallMap,
    DutyBoard,
    LogBook,
    OrderBook,
    GarageNote,
    Manual,
    ControlBox, // E on the gate's box
    Broadcast,  // a radio with a battery
    Nearby,     // within 15 tiles of the gate
    Survivor,
    Notebook
};
// "fridge_note", "wall_map", ... (the keys in clues.json).
[[nodiscard]] const char* sourceKey(Source source);
// The leads a source teaches.
[[nodiscard]] Mask teaches(Source source);
// The prompt line's name for a readable: "A note on the fridge".
[[nodiscard]] const char* readableName(Source source);

// A survivor's "Say hello": lead 1 at trust 10 or more, and lead 2 as well
// when the survivor is competent (over 0.6) and lead 1 is known by then.
inline constexpr float kSurvivorTrust = 10.0f;
inline constexpr float kSurvivorCompetence = 0.6f;
[[nodiscard]] Mask fromSurvivor(Mask known, float trust, float competence);

// A thing to read, standing in the world for ever.
struct Readable
{
    Source source = Source::FridgeNote;
    glm::ivec2 tile{0};  // the furniture piece it lies on
    int building = 0;    // index + 1
};
// One house in six has a note on its fridge.
[[nodiscard]] bool fridgeNoteIn(const LayoutData::BuildingRect& rect);
// Every readable in town. `startHome` (index + 1, or 0): a fridge note is
// guaranteed there (the first life of a world on the way out).
[[nodiscard]] std::vector<Readable> place(const std::vector<FurniturePiece>& pieces, const LayoutData& layout, int startHome);
// The hardware store's locked stock-room cabinet that keeps the gate fuse,
// or (-1, -1) when the town has no hardware store.
[[nodiscard]] glm::ivec2 fuseCabinet(const std::vector<FurniturePiece>& pieces, const LayoutData& layout);

// What a character knows of the gate's state: seen at the box (this life)
// or read in a notebook (an earlier one). `run` and `day` say when.
struct Seen
{
    bool valid = false;
    int run = 0;
    int day = 0;
    bool fuse = false;
    int fuel = 0;
    int repairs = 0;
    bool freed = false;
    int key = 0;   // gate::KeyPlace
    bool open = false;
};
[[nodiscard]] Seen seenFrom(const gate::State& state, int run, int day);
// Reading an earlier character's notebook: every lead in it is learned, and
// its picture of the gate is taken when it is newer than ours.
void transfer(Mask& known, Seen& seen, Mask written, const Seen& writtenSeen);

// The words.
struct Content
{
    std::array<std::string, kLeadCount> leads;
    std::vector<std::pair<std::string, std::string>> readables; // source key, what it says
    std::vector<std::string> broadcast;  // three lines
    std::vector<std::string> sergeants;  // surnames
    std::vector<std::string> streets;
};
// The built-in words (the same as clues.json ships).
[[nodiscard]] Content defaults();
// Reads clues.json over the defaults. False (and `error`) when it cannot.
bool loadContent(const std::string& path, Content& out, std::string* error = nullptr);
// What a readable says.
[[nodiscard]] std::string readableText(const Content& content, Source source);

// The names a world gives: the gate's side, the Sergeant and his street.
struct Names
{
    std::string side = "east";
    std::string sergeant = "Hale";
    std::string street = "Mill Street";
};
[[nodiscard]] Names names(const Content& content, std::uint32_t worldSeed, int side);
// Replaces {side}, {sergeant} and {street}.
[[nodiscard]] std::string fill(const std::string& text, const Names& names);

// The notebook's page "The way out": the known leads as sentences, then the
// four needs as far as this character knows them ("Fuel: 2 of 4, as of day
// 6 of an earlier life").
[[nodiscard]] std::vector<std::string> pageLines(Mask known, const Seen& seen, const Content& content, const Names& names,
                                                 int currentRun, int currentDay);
// The objective line: the next unmet step this character knows about.
struct Facts
{
    bool hasFuse = false;    // a gate fuse in the pack
    bool hasFuel = false;    // a fuel can in the pack
    bool keyInHand = false;
    bool canFree = false;    // lead 6, or Mechanics 3
    bool hasToolbox = false;
};
[[nodiscard]] std::string objective(Mask known, const Seen& seen, const Names& names, const Facts& facts);
} // namespace clues
