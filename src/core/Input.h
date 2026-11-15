#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>
#include <string>

#include "core/Bindings.h"

// Keyboard/mouse state with press/release transitions latched per simulation tick.
//
// Events can arrive at any rate relative to the fixed-step simulation. A press is
// latched when it happens and stays visible to wasKeyPressed() until the
// simulation calls consumeTransitions() after a tick. This means a press is never
// dropped (frame with zero ticks) or double-counted (frame with two ticks), and
// a tap shorter than one tick still registers.
//
// The game asks for actions (phase 13): pressed(Action::Shove), never a raw
// key. The binding table says which keys and buttons make each action; the
// raw key calls are for this class, its tests and the binding table.
class Input
{
public:
    void processEvent(const SDL_Event& event);
    void refreshMouseState();

    // Call once after every simulation tick.
    void consumeTransitions();

    // Scroll is consumed by the frame-level camera zoom, not by ticks.
    [[nodiscard]] int consumeScroll();

    // Direct state injection (used by the dev harness for scripted input).
    void setKeyState(SDL_Scancode key, bool down);
    void setMouseButtonState(Uint8 button, bool down);
    void setMousePosition(int x, int y);
    void addScroll(int amount) { mScrollY += amount; }
    // A key or button by its binding code (bindings::kMouseBase + n for buttons).
    void setCodeState(int code, bool down);
    // Presses or lets go of an action's first key (the bot plays this way).
    void setAction(Action action, bool down);

    [[nodiscard]] bool isKeyDown(SDL_Scancode key) const;
    [[nodiscard]] bool wasKeyPressed(SDL_Scancode key) const;
    // Any key or mouse button pressed since the last tick.
    [[nodiscard]] bool anyKeyPressed() const;
    [[nodiscard]] bool wasKeyReleased(SDL_Scancode key) const;

    [[nodiscard]] bool isMouseButtonDown(Uint8 button) const;
    [[nodiscard]] bool wasMouseButtonPressed(Uint8 button) const;
    [[nodiscard]] bool wasMouseButtonReleased(Uint8 button) const;

    // --- Actions ---
    [[nodiscard]] bool pressed(Action action) const;
    // Held, or (crouch and run in toggle mode) switched on.
    [[nodiscard]] bool down(Action action) const;
    [[nodiscard]] bool released(Action action) const;
    // Any of the four movement actions held.
    [[nodiscard]] bool moving() const;
    // The letters A-Z pressed this tick, in that order ("" for none). For typing a name.
    [[nodiscard]] std::string lettersPressed() const;
    // A digit 1-9 pressed this tick (the number row or the keypad), else 0.
    [[nodiscard]] int digitPressed() const;
    // A digit 0-9 typed this tick, else -1 (a seed).
    [[nodiscard]] int digitTyped() const;
    // The first key or button pressed this tick, for rebinding; kNone if none.
    [[nodiscard]] int codePressed() const;

    [[nodiscard]] const bindings::Table& bindings() const { return mBindings; }
    void setBindings(const bindings::Table& table) { mBindings = table; }
    // Crouch and run can be pressed once to switch on and again to stop.
    void setToggle(Action action, bool toggle);
    [[nodiscard]] bool toggle(Action action) const { return mToggle[static_cast<std::size_t>(action)] != 0; }
    void clearToggles() { mToggled.fill(0); }
    void clearToggle(Action action); // a press-once hold is over
    // "E", "Space", "LClick": what the action is bound to now.
    [[nodiscard]] std::string keyLabel(Action action) const { return bindings::label(mBindings, action); }
    // A prompt with the player's keys in place of the defaults.
    [[nodiscard]] std::string text(const std::string& line) const { return bindings::rebindText(line, mBindings); }

    [[nodiscard]] int mouseX() const { return mMouseX; }
    [[nodiscard]] int mouseY() const { return mMouseY; }
    [[nodiscard]] int scrollY() const { return mScrollY; }
    [[nodiscard]] bool quitRequested() const { return mQuitRequested; }

private:
    static constexpr std::size_t kMouseButtonCount = 8;

    [[nodiscard]] static std::size_t buttonToIndex(Uint8 button);
    [[nodiscard]] bool codeDown(int code) const;
    [[nodiscard]] bool codePressedNow(int code) const;
    [[nodiscard]] bool codeReleasedNow(int code) const;
    void noteFreshPress(int code);

    std::array<Uint8, SDL_NUM_SCANCODES> mCurrentKeys{};
    std::array<Uint8, SDL_NUM_SCANCODES> mKeyPressedLatch{};
    std::array<Uint8, SDL_NUM_SCANCODES> mKeyReleasedLatch{};

    std::array<Uint8, kMouseButtonCount> mCurrentMouseButtons{};
    std::array<Uint8, kMouseButtonCount> mMousePressedLatch{};
    std::array<Uint8, kMouseButtonCount> mMouseReleasedLatch{};

    bindings::Table mBindings = bindings::defaults();
    std::array<Uint8, kActionCount> mToggle{};  // the action switches rather than holds
    std::array<Uint8, kActionCount> mToggled{}; // switched on now
    int mFirstPressed = bindings::kNone;        // this tick's first fresh press

    int mMouseX = 0;
    int mMouseY = 0;
    int mScrollY = 0;
    bool mQuitRequested = false;
};
