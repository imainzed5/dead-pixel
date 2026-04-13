#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>

class Input
{
public:
    void beginFrame();
    void processEvent(const SDL_Event& event);
    void refreshMouseState();

    [[nodiscard]] bool isKeyDown(SDL_Scancode key) const;
    [[nodiscard]] bool wasKeyPressed(SDL_Scancode key) const;
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
    std::array<Uint8, SDL_NUM_SCANCODES> mPreviousKeys{};

    std::array<Uint8, kMouseButtonCount> mCurrentMouseButtons{};
    std::array<Uint8, kMouseButtonCount> mPreviousMouseButtons{};

    int mMouseX = 0;
    int mMouseY = 0;
    int mScrollY = 0;
    bool mQuitRequested = false;
};
