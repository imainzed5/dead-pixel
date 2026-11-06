#include "core/Input.h"

#include <algorithm>

std::size_t Input::buttonToIndex(Uint8 button)
{
    const std::size_t index = static_cast<std::size_t>(button);
    if (index >= kMouseButtonCount)
    {
        return 0;
    }

    return index;
}

void Input::consumeTransitions()
{
    mKeyPressedLatch.fill(0);
    mKeyReleasedLatch.fill(0);
    mMousePressedLatch.fill(0);
    mMouseReleasedLatch.fill(0);
}

int Input::consumeScroll()
{
    const int scroll = mScrollY;
    mScrollY = 0;
    return scroll;
}

void Input::setKeyState(SDL_Scancode key, bool down)
{
    if (key >= SDL_NUM_SCANCODES)
    {
        return;
    }

    const bool wasDown = mCurrentKeys[key] != 0;
    if (down && !wasDown)
    {
        mKeyPressedLatch[key] = 1;
    }
    else if (!down && wasDown)
    {
        mKeyReleasedLatch[key] = 1;
    }
    mCurrentKeys[key] = down ? 1 : 0;
}

void Input::setMouseButtonState(Uint8 button, bool down)
{
    const std::size_t index = buttonToIndex(button);
    const bool wasDown = mCurrentMouseButtons[index] != 0;
    if (down && !wasDown)
    {
        mMousePressedLatch[index] = 1;
    }
    else if (!down && wasDown)
    {
        mMouseReleasedLatch[index] = 1;
    }
    mCurrentMouseButtons[index] = down ? 1 : 0;
}

void Input::setMousePosition(int x, int y)
{
    mMouseX = x;
    mMouseY = y;
}

void Input::processEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_QUIT:
        mQuitRequested = true;
        break;

    case SDL_KEYDOWN:
        if (event.key.repeat == 0)
        {
            setKeyState(event.key.keysym.scancode, true);
        }
        break;

    case SDL_KEYUP:
        setKeyState(event.key.keysym.scancode, false);
        break;

    case SDL_MOUSEBUTTONDOWN:
        setMouseButtonState(event.button.button, true);
        break;

    case SDL_MOUSEBUTTONUP:
        setMouseButtonState(event.button.button, false);
        break;

    case SDL_MOUSEMOTION:
        mMouseX = event.motion.x;
        mMouseY = event.motion.y;
        break;

    case SDL_MOUSEWHEEL:
        mScrollY += event.wheel.y;
        break;

    default:
        break;
    }
}

void Input::refreshMouseState()
{
    const Uint32 mouseState = SDL_GetMouseState(&mMouseX, &mMouseY);
    for (std::size_t button = 1; button < kMouseButtonCount; ++button)
    {
        const bool down = (mouseState & SDL_BUTTON(static_cast<Uint8>(button))) != 0;
        setMouseButtonState(static_cast<Uint8>(button), down);
    }
}

bool Input::isKeyDown(SDL_Scancode key) const
{
    if (key >= SDL_NUM_SCANCODES)
    {
        return false;
    }

    return mCurrentKeys[key] != 0;
}

bool Input::wasKeyPressed(SDL_Scancode key) const
{
    if (key >= SDL_NUM_SCANCODES)
    {
        return false;
    }

    return mKeyPressedLatch[key] != 0;
}

bool Input::wasKeyReleased(SDL_Scancode key) const
{
    if (key >= SDL_NUM_SCANCODES)
    {
        return false;
    }

    return mKeyReleasedLatch[key] != 0;
}

bool Input::isMouseButtonDown(Uint8 button) const
{
    return mCurrentMouseButtons[buttonToIndex(button)] != 0;
}

bool Input::wasMouseButtonPressed(Uint8 button) const
{
    return mMousePressedLatch[buttonToIndex(button)] != 0;
}

bool Input::wasMouseButtonReleased(Uint8 button) const
{
    return mMouseReleasedLatch[buttonToIndex(button)] != 0;
}

bool Input::anyKeyPressed() const
{
    const auto pressed = [](Uint8 v) { return v != 0; };
    return std::any_of(mKeyPressedLatch.begin(), mKeyPressedLatch.end(), pressed) ||
           std::any_of(mMousePressedLatch.begin(), mMousePressedLatch.end(), pressed);
}
