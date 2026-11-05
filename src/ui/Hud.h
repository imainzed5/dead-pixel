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
// Layout (anchored to window edges):
//   top-left      health and stamina bars, then active conditions
//   top-center    district toast when entering a new district
//   top-right     day, time, weather; objective below
//   bottom-center hotbar with item icons, selected item name, messages
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
};

struct HudSlot
{
    bool empty = true;
    std::string icon;
    std::string name;
    int count = 0;
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

    // Layout constants (pixels at 1x).
    static constexpr float kMargin = 16.0f;
    static constexpr float kVitalsWidth = 260.0f;
    static constexpr float kBarHeight = 12.0f;
    static constexpr float kConditionRowHeight = 24.0f;
    static constexpr std::size_t kMaxConditionRows = 9;
    static constexpr float kSlotSize = 48.0f;
    static constexpr float kSlotGap = 6.0f;
    static constexpr float kClockWidth = 236.0f;
};
