#pragma once

#include <array>
#include <optional>
#include <string>

#include <nlohmann/json_fwd.hpp>

// What the keys do, by name (phase 13). Code asks Input for an action, never
// for a raw key; this table says which keys (or mouse buttons) make each one.
// The rebindable actions come first, in the order the Keys list shows them;
// the rest are fixed (menu keys, the controls panel, the dev keys).
enum class Action : int
{
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Run,
    Crouch,
    Attack,
    TakeDown,
    Shove,
    Throw,
    Use,
    Board,
    Pry,
    UseItem,
    Flashlight,
    Drop,
    Pack,
    Make,
    Build,
    Vehicle,
    Map,
    Notebook,
    ZoomIn,
    ZoomOut,
    Belt1,
    Belt2,
    Belt3,
    Belt4,
    Belt5,
    Belt6,
    Belt7,
    Belt8,
    // Fixed from here on.
    Controls,
    UiBack,    // Esc: close, pause
    UiUp,      // the arrows
    UiDown,
    UiLeft,
    UiRight,
    UiConfirm, // Enter
    UiSpace,   // Space in menus (also confirms there)
    UiErase,   // Backspace in text and readers
    UiStack,   // Shift in panels: the whole stack
    UiCtrl,    // Ctrl with the wheel: minimap zoom
    UiClick,   // left button on a panel
    UiAltClick,
    UiMinus,   // a panel's count, a name's hyphen
    UiPlus,
    DebugNoise,    // F1, dev builds only
    DebugDistrict, // F3, dev builds only
    DebugDespawn,  // K, dev builds only
    Count
};

constexpr int kActionCount = static_cast<int>(Action::Count);
constexpr int kRebindableCount = static_cast<int>(Action::Belt8) + 1;

namespace bindings
{
// A key is its SDL scancode; a mouse button is kMouseBase + its number.
constexpr int kNone = -1;
constexpr int kMouseBase = 1000;
constexpr int kMouseLeft = kMouseBase + 1;
constexpr int kMouseMiddle = kMouseBase + 2;
constexpr int kMouseRight = kMouseBase + 3;
[[nodiscard]] inline bool isMouse(int code) { return code >= kMouseBase; }

// Each action has a first key and an optional second (Shift on either side).
struct Table
{
    std::array<std::array<int, 2>, kActionCount> codes{};
    [[nodiscard]] int primary(Action a) const { return codes[static_cast<std::size_t>(a)][0]; }
    [[nodiscard]] bool has(Action a, int code) const
    {
        const auto& c = codes[static_cast<std::size_t>(a)];
        return code != kNone && (c[0] == code || c[1] == code);
    }
    bool operator==(const Table& other) const { return codes == other.codes; }
};

[[nodiscard]] Table defaults();
[[nodiscard]] bool rebindable(Action action);
// The settings file's name for the action ("shove") and the Keys list's ("Shove").
[[nodiscard]] const char* actionKey(Action action);
[[nodiscard]] const char* actionLabel(Action action);
// The short name shown in prompts and panels ("E", "Space", "LClick", "Shift").
[[nodiscard]] std::string codeLabel(int code);
// The name written to the settings file (SDL's key name, "Mouse 1").
[[nodiscard]] std::string codeName(int code);
// Either of the above, a harness word ("enter", "esc", "shift", "left_shift"),
// or "left"/"right" for the mouse. kNone when nothing matches.
[[nodiscard]] int codeFromName(const std::string& name);
// Esc, Enter and the function keys stay the menus' and the panels'.
[[nodiscard]] bool bindableCode(int code);

// Binds `action` to `code`. A rebindable action that had the key takes the
// action's old key instead (a swap), and is returned so it can be said.
struct RebindResult
{
    bool ok = false;
    std::optional<Action> swappedWith;
};
RebindResult rebind(Table& table, Action action, int code);

// The label of an action's first key ("E"); with `both`, "Shift / RShift".
[[nodiscard]] std::string label(const Table& table, Action action, bool both = false);

// Writes the key names the game prints into `text` as the player's keys:
// a standalone E, R, X, F, G, Q, V, C, B, M, J, N, Space, Tab, WASD or 1-8
// becomes what that action is bound to now. Unchanged when nothing is rebound.
[[nodiscard]] std::string rebindText(const std::string& text, const Table& table);

// Only rebindable actions are written; anything missing or unknown on
// reading keeps its default.
[[nodiscard]] nlohmann::json toJson(const Table& table);
[[nodiscard]] Table fromJson(const nlohmann::json& json);
} // namespace bindings
