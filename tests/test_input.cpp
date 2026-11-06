#include "TestFramework.h"

#include "core/Input.h"

namespace
{
SDL_Event keyEvent(Uint32 type, SDL_Scancode code, Uint8 repeat = 0)
{
    SDL_Event event{};
    event.type = type;
    event.key.keysym.scancode = code;
    event.key.repeat = repeat;
    return event;
}
} // namespace

DP_TEST(input_press_survives_frames_without_a_tick)
{
    Input input;
    input.processEvent(keyEvent(SDL_KEYDOWN, SDL_SCANCODE_E));
    // A render frame with no simulation tick must not lose the press.
    input.processEvent(SDL_Event{});
    DP_CHECK(input.wasKeyPressed(SDL_SCANCODE_E));
    input.consumeTransitions();
    DP_CHECK(!input.wasKeyPressed(SDL_SCANCODE_E));
    DP_CHECK(input.isKeyDown(SDL_SCANCODE_E));
}

DP_TEST(input_press_fires_once_across_two_ticks)
{
    Input input;
    input.processEvent(keyEvent(SDL_KEYDOWN, SDL_SCANCODE_TAB));
    DP_CHECK(input.wasKeyPressed(SDL_SCANCODE_TAB));
    input.consumeTransitions(); // first tick of the frame
    DP_CHECK(!input.wasKeyPressed(SDL_SCANCODE_TAB)); // second tick sees nothing new
}

DP_TEST(input_tap_shorter_than_a_tick_registers)
{
    Input input;
    input.processEvent(keyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE));
    input.processEvent(keyEvent(SDL_KEYUP, SDL_SCANCODE_SPACE));
    DP_CHECK(input.wasKeyPressed(SDL_SCANCODE_SPACE));
    DP_CHECK(input.wasKeyReleased(SDL_SCANCODE_SPACE));
    DP_CHECK(!input.isKeyDown(SDL_SCANCODE_SPACE));
}

DP_TEST(input_key_repeat_is_not_a_new_press)
{
    Input input;
    input.processEvent(keyEvent(SDL_KEYDOWN, SDL_SCANCODE_W));
    input.consumeTransitions();
    input.processEvent(keyEvent(SDL_KEYDOWN, SDL_SCANCODE_W, 1));
    DP_CHECK(!input.wasKeyPressed(SDL_SCANCODE_W));
    DP_CHECK(input.isKeyDown(SDL_SCANCODE_W));
}

DP_TEST(input_scroll_is_consumed_once)
{
    Input input;
    input.addScroll(2);
    input.addScroll(1);
    DP_CHECK_EQ(input.consumeScroll(), 3);
    DP_CHECK_EQ(input.consumeScroll(), 0);
}

DP_TEST(input_any_key_pressed_tracks_latches)
{
    Input input;
    DP_CHECK(!input.anyKeyPressed());
    input.setKeyState(SDL_SCANCODE_Q, true);
    DP_CHECK(input.anyKeyPressed());
    input.consumeTransitions();
    DP_CHECK(!input.anyKeyPressed()); // still held, but no new press
    input.setMouseButtonState(SDL_BUTTON_LEFT, true);
    DP_CHECK(input.anyKeyPressed());
}
