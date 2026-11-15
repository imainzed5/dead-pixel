// The binding table: every key the game knows by name. Nothing outside this
// file and Input.cpp names a raw key (phase 13).
#include "core/Bindings.h"

#include <nlohmann/json.hpp>

#include <SDL.h>

#include <algorithm>
#include <cctype>

namespace bindings
{
namespace
{
struct ActionInfo
{
    const char* key;
    const char* label;
    int first;
    int second;
};

// In Action's order.
const std::array<ActionInfo, kActionCount>& infoTable()
{
    static const std::array<ActionInfo, kActionCount> kInfo = {{
        {"move_up", "Move up", SDL_SCANCODE_W, kNone},
        {"move_down", "Move down", SDL_SCANCODE_S, kNone},
        {"move_left", "Move left", SDL_SCANCODE_A, kNone},
        {"move_right", "Move right", SDL_SCANCODE_D, kNone},
        {"run", "Run", SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT},
        {"crouch", "Crouch", SDL_SCANCODE_C, SDL_SCANCODE_LCTRL},
        {"attack", "Attack", kMouseLeft, kNone},
        {"take_down", "Take down", kMouseRight, kNone},
        {"shove", "Shove", SDL_SCANCODE_SPACE, kNone},
        {"throw", "Throw", SDL_SCANCODE_G, kNone},
        {"use", "Use", SDL_SCANCODE_E, kNone},
        {"board", "Board, tap, reload", SDL_SCANCODE_R, kNone},
        {"pry", "Pry, break up", SDL_SCANCODE_X, kNone},
        {"use_item", "Use item", SDL_SCANCODE_F, kNone},
        {"flashlight", "Flashlight", SDL_SCANCODE_Q, kNone},
        {"drop", "Drop", SDL_SCANCODE_BACKSPACE, kNone},
        {"pack", "Pack", SDL_SCANCODE_TAB, kNone},
        {"make", "Make", SDL_SCANCODE_N, kNone},
        {"build", "Build wheel", SDL_SCANCODE_B, kNone},
        {"vehicle", "Vehicle in, out", SDL_SCANCODE_V, kNone},
        {"map", "Town map", SDL_SCANCODE_M, kNone},
        {"notebook", "Notebook", SDL_SCANCODE_J, kNone},
        {"zoom_in", "Minimap in", SDL_SCANCODE_EQUALS, SDL_SCANCODE_KP_PLUS},
        {"zoom_out", "Minimap out", SDL_SCANCODE_MINUS, SDL_SCANCODE_KP_MINUS},
        {"belt_1", "Belt 1", SDL_SCANCODE_1, kNone},
        {"belt_2", "Belt 2", SDL_SCANCODE_2, kNone},
        {"belt_3", "Belt 3", SDL_SCANCODE_3, kNone},
        {"belt_4", "Belt 4", SDL_SCANCODE_4, kNone},
        {"belt_5", "Belt 5", SDL_SCANCODE_5, kNone},
        {"belt_6", "Belt 6", SDL_SCANCODE_6, kNone},
        {"belt_7", "Belt 7", SDL_SCANCODE_7, kNone},
        {"belt_8", "Belt 8", SDL_SCANCODE_8, kNone},
        {"controls", "Controls", SDL_SCANCODE_F2, kNone},
        {"ui_back", "Back", SDL_SCANCODE_ESCAPE, kNone},
        {"ui_up", "Up", SDL_SCANCODE_UP, kNone},
        {"ui_down", "Down", SDL_SCANCODE_DOWN, kNone},
        {"ui_left", "Left", SDL_SCANCODE_LEFT, kNone},
        {"ui_right", "Right", SDL_SCANCODE_RIGHT, kNone},
        {"ui_confirm", "Confirm", SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER},
        {"ui_space", "Space", SDL_SCANCODE_SPACE, kNone},
        {"ui_erase", "Erase", SDL_SCANCODE_BACKSPACE, kNone},
        {"ui_stack", "Stack", SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT},
        {"ui_ctrl", "Ctrl", SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL},
        {"ui_click", "Click", kMouseLeft, kNone},
        {"ui_alt_click", "Right click", kMouseRight, kNone},
        {"ui_minus", "Minus", SDL_SCANCODE_MINUS, SDL_SCANCODE_KP_MINUS},
        {"ui_plus", "Plus", SDL_SCANCODE_EQUALS, SDL_SCANCODE_KP_PLUS},
        {"debug_noise", "Noise debug", SDL_SCANCODE_F1, kNone},
        {"debug_district", "District debug", SDL_SCANCODE_F3, kNone},
        {"debug_despawn", "Despawn test entity", SDL_SCANCODE_K, kNone},
    }};
    return kInfo;
}

const ActionInfo& info(Action action)
{
    return infoTable()[static_cast<std::size_t>(action)];
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Short labels where SDL's name is long; everything else uses SDL's.
struct Short
{
    int code;
    const char* label;
};
constexpr Short kShort[] = {
    {SDL_SCANCODE_LSHIFT, "Shift"},     {SDL_SCANCODE_RSHIFT, "RShift"},   {SDL_SCANCODE_LCTRL, "Ctrl"},
    {SDL_SCANCODE_RCTRL, "RCtrl"},      {SDL_SCANCODE_LALT, "Alt"},        {SDL_SCANCODE_RALT, "RAlt"},
    {SDL_SCANCODE_BACKSPACE, "Bksp"},   {SDL_SCANCODE_RETURN, "Enter"},    {SDL_SCANCODE_ESCAPE, "Esc"},
    {SDL_SCANCODE_KP_PLUS, "Num +"},    {SDL_SCANCODE_KP_MINUS, "Num -"},  {SDL_SCANCODE_KP_ENTER, "Num Enter"},
    {SDL_SCANCODE_EQUALS, "+"},         {SDL_SCANCODE_MINUS, "-"},         {SDL_SCANCODE_CAPSLOCK, "Caps"},
    {kMouseLeft, "LClick"},             {kMouseRight, "RClick"},           {kMouseMiddle, "MClick"},
    {kMouseBase + 4, "Mouse 4"},        {kMouseBase + 5, "Mouse 5"},
};

// Words the game prints for a key in its prompts, and the action each stands for.
struct Word
{
    const char* word;
    Action action;
};
constexpr Word kWords[] = {
    {"E", Action::Use},        {"R", Action::Board},    {"X", Action::Pry},     {"F", Action::UseItem},
    {"G", Action::Throw},      {"Q", Action::Flashlight}, {"V", Action::Vehicle}, {"Space", Action::Shove},
    {"Tab", Action::Pack},
};

bool alnum(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

// A key word in a sentence: "E: Search", "Hold E", "Press Space", "E to".
bool keyContext(const std::string& text, std::size_t begin, std::size_t end)
{
    if (end < text.size() && text[end] == ':') return true;
    if (text.compare(end, 4, " to ") == 0 || text.compare(end, 6, " again") == 0) return true;
    std::size_t p = begin;
    while (p > 0 && text[p - 1] == ' ') --p;
    std::size_t q = p;
    while (q > 0 && alnum(text[q - 1])) --q;
    const std::string before = lower(text.substr(q, p - q));
    return before == "hold" || before == "press" || before == "tap";
}
} // namespace

Table defaults()
{
    Table table;
    for (int i = 0; i < kActionCount; ++i)
    {
        const ActionInfo& a = infoTable()[static_cast<std::size_t>(i)];
        table.codes[static_cast<std::size_t>(i)] = {a.first, a.second};
    }
    return table;
}

bool rebindable(Action action)
{
    return static_cast<int>(action) < kRebindableCount;
}

const char* actionKey(Action action)
{
    return info(action).key;
}

const char* actionLabel(Action action)
{
    return info(action).label;
}

std::string codeLabel(int code)
{
    if (code == kNone) return "-";
    for (const Short& s : kShort)
    {
        if (s.code == code) return s.label;
    }
    if (isMouse(code)) return "Mouse " + std::to_string(code - kMouseBase);
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
    return (name != nullptr && name[0] != '\0') ? std::string(name) : "Key " + std::to_string(code);
}

std::string codeName(int code)
{
    if (code == kNone) return "";
    if (isMouse(code)) return "Mouse " + std::to_string(code - kMouseBase);
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
    return (name != nullptr && name[0] != '\0') ? std::string(name) : "Key " + std::to_string(code);
}

int codeFromName(const std::string& rawName)
{
    const std::string name = lower(rawName);
    if (name.empty()) return kNone;
    if (name == "enter") return SDL_SCANCODE_RETURN;
    if (name == "esc") return SDL_SCANCODE_ESCAPE;
    if (name == "lshift" || name == "shift") return SDL_SCANCODE_LSHIFT;
    if (name == "lctrl" || name == "ctrl") return SDL_SCANCODE_LCTRL;
    if (name == "lclick" || name == "mouse left") return kMouseLeft;
    if (name == "rclick" || name == "mouse right") return kMouseRight;
    if (name == "mclick" || name == "mouse middle") return kMouseMiddle;
    if (name.rfind("mouse ", 0) == 0)
    {
        const std::string number = name.substr(6);
        if (!number.empty() && std::all_of(number.begin(), number.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
        {
            const int button = std::stoi(number);
            if (button >= 1 && button <= 7) return kMouseBase + button;
        }
    }
    for (const Short& s : kShort)
    {
        if (lower(s.label) == name) return s.code;
    }
    std::string sdlName = rawName;
    std::replace(sdlName.begin(), sdlName.end(), '_', ' ');
    const SDL_Scancode code = SDL_GetScancodeFromName(sdlName.c_str());
    return code == SDL_SCANCODE_UNKNOWN ? kNone : static_cast<int>(code);
}

bool bindableCode(int code)
{
    if (code == kNone) return false;
    if (isMouse(code)) return code != kMouseBase; // button 0 is not a button
    switch (code)
    {
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_F1:
    case SDL_SCANCODE_F2:
    case SDL_SCANCODE_F3:
        return false;
    default:
        return code > SDL_SCANCODE_UNKNOWN && code < SDL_NUM_SCANCODES;
    }
}

RebindResult rebind(Table& table, Action action, int code)
{
    RebindResult result;
    if (!rebindable(action) || !bindableCode(code)) return result;
    auto& mine = table.codes[static_cast<std::size_t>(action)];
    result.ok = true;
    if (mine[0] == code && mine[1] == kNone) return result;
    const int old = mine[0] != code ? mine[0] : mine[1];
    for (int i = 0; i < kRebindableCount; ++i)
    {
        if (i == static_cast<int>(action)) continue;
        auto& other = table.codes[static_cast<std::size_t>(i)];
        for (int slot = 0; slot < 2; ++slot)
        {
            if (other[static_cast<std::size_t>(slot)] != code) continue;
            // The other action takes the old key; a second key is simply dropped.
            other[static_cast<std::size_t>(slot)] = slot == 0 ? old : kNone;
            if (other[0] == kNone && other[1] != kNone) std::swap(other[0], other[1]);
            result.swappedWith = static_cast<Action>(i);
        }
    }
    mine = {code, kNone};
    return result;
}

std::string label(const Table& table, Action action, bool both)
{
    const auto& c = table.codes[static_cast<std::size_t>(action)];
    std::string out = codeLabel(c[0]);
    if (both && c[1] != kNone) out += " / " + codeLabel(c[1]);
    return out;
}

std::string rebindText(const std::string& text, const Table& table)
{
    static const Table kDefault = defaults();
    if (table == kDefault) return text;
    std::string out;
    out.reserve(text.size() + 8);
    std::size_t i = 0;
    while (i < text.size())
    {
        if (!alnum(text[i]) || (i > 0 && alnum(text[i - 1])))
        {
            out += text[i++];
            continue;
        }
        std::size_t end = i;
        while (end < text.size() && alnum(text[end])) ++end;
        const std::string word = text.substr(i, end - i);
        std::string replaced = word;
        if (word == "WASD")
        {
            const Action moves[4] = {Action::MoveUp, Action::MoveLeft, Action::MoveDown, Action::MoveRight};
            std::string joined;
            bool single = true;
            for (Action a : moves) single = single && label(table, a).size() == 1;
            for (int m = 0; m < 4; ++m)
            {
                if (!single && m > 0) joined += "/";
                joined += label(table, moves[m]);
            }
            replaced = joined;
        }
        else if (keyContext(text, i, end))
        {
            for (const Word& w : kWords)
            {
                if (word == w.word) replaced = label(table, w.action);
            }
        }
        out += replaced;
        i = end;
    }
    return out;
}

nlohmann::json toJson(const Table& table)
{
    nlohmann::json json = nlohmann::json::object();
    for (int i = 0; i < kRebindableCount; ++i)
    {
        const auto& c = table.codes[static_cast<std::size_t>(i)];
        nlohmann::json keys = nlohmann::json::array();
        for (int code : c)
        {
            if (code != kNone) keys.push_back(codeName(code));
        }
        json[actionKey(static_cast<Action>(i))] = keys;
    }
    return json;
}

Table fromJson(const nlohmann::json& json)
{
    Table table = defaults();
    if (!json.is_object()) return table;
    for (int i = 0; i < kRebindableCount; ++i)
    {
        const char* key = actionKey(static_cast<Action>(i));
        if (!json.contains(key)) continue;
        const nlohmann::json& value = json[key];
        std::array<int, 2> codes{kNone, kNone};
        int n = 0;
        const auto take = [&](const nlohmann::json& name)
        {
            if (!name.is_string() || n >= 2) return;
            const int code = codeFromName(name.get<std::string>());
            if (bindableCode(code)) codes[static_cast<std::size_t>(n++)] = code;
        };
        if (value.is_array())
        {
            for (const nlohmann::json& name : value) take(name);
        }
        else
        {
            take(value);
        }
        if (n > 0) table.codes[static_cast<std::size_t>(i)] = codes;
    }
    return table;
}
} // namespace bindings
