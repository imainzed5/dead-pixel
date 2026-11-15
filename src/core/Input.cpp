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
    mFirstPressed = bindings::kNone;
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
        noteFreshPress(static_cast<int>(key));
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
        noteFreshPress(bindings::kMouseBase + static_cast<int>(index));
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

// --- Actions (phase 13) ---

void Input::noteFreshPress(int code)
{
    if (mFirstPressed == bindings::kNone) mFirstPressed = code;
    // Crouch or run in toggle mode: each press switches it.
    for (int i = 0; i < kActionCount; ++i)
    {
        if (mToggle[static_cast<std::size_t>(i)] != 0 && mBindings.has(static_cast<Action>(i), code))
        {
            mToggled[static_cast<std::size_t>(i)] = mToggled[static_cast<std::size_t>(i)] != 0 ? 0 : 1;
        }
    }
}

void Input::setCodeState(int code, bool down)
{
    if (code == bindings::kNone) return;
    if (bindings::isMouse(code))
    {
        setMouseButtonState(static_cast<Uint8>(code - bindings::kMouseBase), down);
        return;
    }
    if (code < SDL_NUM_SCANCODES) setKeyState(static_cast<SDL_Scancode>(code), down);
}

void Input::setAction(Action action, bool down)
{
    setCodeState(mBindings.primary(action), down);
}

bool Input::codeDown(int code) const
{
    if (code == bindings::kNone) return false;
    if (bindings::isMouse(code)) return isMouseButtonDown(static_cast<Uint8>(code - bindings::kMouseBase));
    return isKeyDown(static_cast<SDL_Scancode>(code));
}

bool Input::codePressedNow(int code) const
{
    if (code == bindings::kNone) return false;
    if (bindings::isMouse(code)) return wasMouseButtonPressed(static_cast<Uint8>(code - bindings::kMouseBase));
    return wasKeyPressed(static_cast<SDL_Scancode>(code));
}

bool Input::codeReleasedNow(int code) const
{
    if (code == bindings::kNone) return false;
    if (bindings::isMouse(code)) return wasMouseButtonReleased(static_cast<Uint8>(code - bindings::kMouseBase));
    return wasKeyReleased(static_cast<SDL_Scancode>(code));
}

bool Input::pressed(Action action) const
{
    const auto& codes = mBindings.codes[static_cast<std::size_t>(action)];
    return codePressedNow(codes[0]) || codePressedNow(codes[1]);
}

bool Input::down(Action action) const
{
    const std::size_t i = static_cast<std::size_t>(action);
    if (mToggle[i] != 0) return mToggled[i] != 0;
    const auto& codes = mBindings.codes[i];
    return codeDown(codes[0]) || codeDown(codes[1]);
}

bool Input::released(Action action) const
{
    const auto& codes = mBindings.codes[static_cast<std::size_t>(action)];
    return codeReleasedNow(codes[0]) || codeReleasedNow(codes[1]);
}

bool Input::moving() const
{
    return down(Action::MoveUp) || down(Action::MoveDown) || down(Action::MoveLeft) || down(Action::MoveRight);
}

std::string Input::lettersPressed() const
{
    std::string letters;
    for (int key = SDL_SCANCODE_A; key <= SDL_SCANCODE_Z; ++key)
    {
        if (wasKeyPressed(static_cast<SDL_Scancode>(key))) letters.push_back(static_cast<char>('A' + (key - SDL_SCANCODE_A)));
    }
    return letters;
}

int Input::digitPressed() const
{
    for (int d = 1; d <= 9; ++d)
    {
        if (wasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + d - 1)) ||
            wasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + d - 1)))
        {
            return d;
        }
    }
    return 0;
}

int Input::digitTyped() const
{
    if (wasKeyPressed(SDL_SCANCODE_0) || wasKeyPressed(SDL_SCANCODE_KP_0)) return 0;
    const int d = digitPressed();
    return d > 0 ? d : -1;
}

void Input::clearToggle(Action action)
{
    mToggled[static_cast<std::size_t>(action)] = 0;
}

int Input::codePressed() const
{
    return mFirstPressed;
}

void Input::setToggle(Action action, bool toggle)
{
    const std::size_t i = static_cast<std::size_t>(action);
    mToggle[i] = toggle ? 1 : 0;
    mToggled[i] = 0;
}
