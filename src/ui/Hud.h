#pragma once

#include <array>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

class BitmapFont;
class SpriteBatch;
class SpriteSheet;

// The in-play HUD: vitals, status conditions, clock, objective, hotbar and
// messages. Game builds a HudState snapshot each frame; Hud only draws it.
//
// Nothing is boxed in: text and thin lines sit straight on the world, each
// with a dark edge so it reads over anything.
//
// Layout (anchored to window edges):
//   bottom-left   health and stamina lines, active conditions stacked above
//   top-center    district toast when entering a new district
//   top-right     time, day and weather; objective below
//   bottom-center belt of bare item icons, the one in use drawn large
//   top-left      vehicle readout
//
// Conditions follow the "only show problems" rule: nothing is listed while
// the character is fine, and each entry is colored by severity.

enum class HudSeverity
{
    Info,
    Mild,
    Serious,
    Critical
};

struct HudStatus
{
    std::string label;
    HudSeverity severity = HudSeverity::Mild;
    std::string icon; // spritesheet name, may be empty
};

// Raw values the conditions list is derived from.
struct HudConditionInputs
{
    float hunger = 100.0f;
    float thirst = 100.0f;
    float sleepDebt = 0.0f;
    float fatigue = 0.0f;
    float bodyTemperature = 37.0f;
    float illness = 0.0f;
    float panic = 0.0f;
    float depression = 0.0f;
    float desensitisation = 0.0f;
    bool inCrisis = false;
    bool bleeding = false;
    int bites = 0;
    int cuts = 0;
    bool infected = false;
    bool overburdened = false;
    bool sleeping = false;
    // Phase 5: each wound by name and place, most urgent first (from
    // woundrules::hudLabels: "Bleeding", "Infected bite", "Broken left leg",
    // "Fever"). When set, it replaces bites / cuts / infected above.
    std::vector<std::string> woundLabels;
    bool rested = false;
};

struct HudStep
{
    std::string text;
    bool done = false;
};

struct HudSlot
{
    bool empty = true;
    std::string icon;
    std::string name;
    int count = 0;
    bool equipped = false; // the weapon in hand
    float wear = -1.0f;    // weapon durability left, 0..1; -1 for other items
    bool dim = false;      // a flashlight or radio with a flat battery
};

struct HudState
{
    bool visible = false;
    float timeSeconds = 0.0f; // drives pulsing warnings

    float health = 100.0f;
    float healthMax = 100.0f;
    float stamina = 100.0f;
    float staminaMax = 100.0f;
    float staminaBaseMax = 100.0f; // cap before needs shave it; the gap is drawn as "lost"
    std::vector<HudStatus> conditions;

    int day = 1;
    double hours = 8.0;
    std::string weather;           // e.g. "Rain"
    float outdoorTemperature = 18.0f;
    std::string characterName;
    std::string districtLine;      // infection of the district you stand in
    bool districtLineDanger = false;

    std::string objectiveTitle;
    std::string objectiveDetail;
    glm::vec4 objectiveColor{0.9f, 0.8f, 0.3f, 1.0f};
    // The day's plan under the objective, each step with a check box.
    std::vector<HudStep> objectiveSteps;

    // A card at the top of the screen at dawn ("Day 2" and a line).
    std::string dayCardTitle;
    std::string dayCardLine;
    float dayCardAlpha = 0.0f;

    std::array<HudSlot, 8> hotbar{};
    int activeSlot = 0;

    std::string message;  // transient feedback ("Flashlight on.")
    float messageAlpha = 0.0f;
    std::string prompt;   // context action ("E: Search shelf")

    std::string vehicleLine;
    bool driving = false;

    std::string districtToast;
    float districtToastAlpha = 0.0f;
    bool districtDanger = false;

    // Held by the dead: a call to struggle, and the meter as ticks to fill.
    bool held = false;
    int struggleTicks = 0;
    int struggleTicksTotal = 0;

    // Width of a panel open down the right edge (pack, crafting), or 0. The
    // panel carries the belt and covers the clock, so the HUD leaves them out.
    float dockedWidth = 0.0f;
};

class Hud
{
public:
    // Spritesheet pass: panels, bars, icons. The font is only used to measure.
    void drawShapes(SpriteBatch& batch, const SpriteSheet& sheet, const BitmapFont& font,
                    const HudState& state, const glm::vec2& viewport) const;
    // Font pass: every label and number.
    void drawText(SpriteBatch& batch, const BitmapFont& font, const HudState& state, const glm::vec2& viewport) const;

    [[nodiscard]] static std::vector<HudStatus> buildConditions(const HudConditionInputs& in);
    [[nodiscard]] static glm::vec4 severityColor(HudSeverity severity);
    // Name shown above the hotbar: "Knife - in hand", "Bat - worn", "Pipe - broken".
    [[nodiscard]] static std::string slotLabel(const HudSlot& slot);

    // Layout constants (pixels at 1x).
    static constexpr float kEdge = 24.0f;          // gap to the window edge
    static constexpr float kBarsWidth = 220.0f;    // health and stamina lines
    static constexpr float kHealthHeight = 6.0f;
    static constexpr float kStaminaHeight = 3.0f;
    static constexpr float kConditionRowHeight = 22.0f;
    static constexpr std::size_t kMaxConditionRows = 9;
    static constexpr float kSlotSize = 40.0f;      // a belt slot not in use
    static constexpr float kBeltIcon = 32.0f;
    static constexpr float kActiveIcon = 64.0f;    // the item in use, drawn double size
    static constexpr float kSlotGap = 8.0f;
};
