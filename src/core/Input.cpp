#include "core/Input.h"

std::size_t Input::buttonToIndex(Uint8 button)
{
    const std::size_t index = static_cast<std::size_t>(button);
    if (index >= kMouseButtonCount)
    {
        return 0;
    }

    return index;
}

void Input::beginFrame()
{
    mPreviousKeys = mCurrentKeys;
    mPreviousMouseButtons = mCurrentMouseButtons;
    mQuitRequested = false;
    mScrollY = 0;
}

void Input::processEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_QUIT:
        mQuitRequested = true;
        break;

    case SDL_KEYDOWN:
        if (event.key.keysym.scancode < SDL_NUM_SCANCODES)
        {
            mCurrentKeys[event.key.keysym.scancode] = 1;
        }
        break;

    case SDL_KEYUP:
        if (event.key.keysym.scancode < SDL_NUM_SCANCODES)
        {
            mCurrentKeys[event.key.keysym.scancode] = 0;
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
        mCurrentMouseButtons[buttonToIndex(event.button.button)] = 1;
        break;

    case SDL_MOUSEBUTTONUP:
        mCurrentMouseButtons[buttonToIndex(event.button.button)] = 0;
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
        mCurrentMouseButtons[button] = (mouseState & SDL_BUTTON(static_cast<Uint8>(button))) != 0 ? 1 : 0;
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

    return mCurrentKeys[key] != 0 && mPreviousKeys[key] == 0;
}

bool Input::wasKeyReleased(SDL_Scancode key) const
{
    if (key >= SDL_NUM_SCANCODES)
    {
        return false;
    }

    return mCurrentKeys[key] == 0 && mPreviousKeys[key] != 0;
}

bool Input::isMouseButtonDown(Uint8 button) const
{
    const std::size_t index = buttonToIndex(button);
    return mCurrentMouseButtons[index] != 0;
}

bool Input::wasMouseButtonPressed(Uint8 button) const
{
    const std::size_t index = buttonToIndex(button);
    return mCurrentMouseButtons[index] != 0 && mPreviousMouseButtons[index] == 0;
}

bool Input::wasMouseButtonReleased(Uint8 button) const
{
    const std::size_t index = buttonToIndex(button);
    return mCurrentMouseButtons[index] == 0 && mPreviousMouseButtons[index] != 0;
}
