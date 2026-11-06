#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>

// Keyboard/mouse state with press/release transitions latched per simulation tick.
//
// Events can arrive at any rate relative to the fixed-step simulation. A press is
// latched when it happens and stays visible to wasKeyPressed() until the
// simulation calls consumeTransitions() after a tick. This means a press is never
// dropped (frame with zero ticks) or double-counted (frame with two ticks), and
// a tap shorter than one tick still registers.
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

    [[nodiscard]] bool isKeyDown(SDL_Scancode key) const;
    [[nodiscard]] bool wasKeyPressed(SDL_Scancode key) const;
    // Any key or mouse button pressed since the last tick.
    [[nodiscard]] bool anyKeyPressed() const;
    [[nodiscard]] bool wasKeyReleased(SDL_Scancode key) const;

    [[nodiscard]] bool isMouseButtonDown(Uint8 button) const;
    [[nodiscard]] bool wasMouseButtonPressed(Uint8 button) const;
    [[nodiscard]] bool wasMouseButtonReleased(Uint8 button) const;

    [[nodiscard]] int mouseX() const { return mMouseX; }
    [[nodiscard]] int mouseY() const { return mMouseY; }
    [[nodiscard]] int scrollY() const { return mScrollY; }
    [[nodiscard]] bool quitRequested() const { return mQuitRequested; }

private:
    static constexpr std::size_t kMouseButtonCount = 8;

    [[nodiscard]] static std::size_t buttonToIndex(Uint8 button);

    std::array<Uint8, SDL_NUM_SCANCODES> mCurrentKeys{};
    std::array<Uint8, SDL_NUM_SCANCODES> mKeyPressedLatch{};
    std::array<Uint8, SDL_NUM_SCANCODES> mKeyReleasedLatch{};

    std::array<Uint8, kMouseButtonCount> mCurrentMouseButtons{};
    std::array<Uint8, kMouseButtonCount> mMousePressedLatch{};
    std::array<Uint8, kMouseButtonCount> mMouseReleasedLatch{};

    int mMouseX = 0;
    int mMouseY = 0;
    int mScrollY = 0;
    bool mQuitRequested = false;
};
