#include "TestFramework.h"

#include "core/Bindings.h"
#include "core/Input.h"

#include <nlohmann/json.hpp>

DP_TEST(default_bindings_are_the_keys_the_game_always_had)
{
    const bindings::Table table = bindings::defaults();
    DP_CHECK_EQ(table.primary(Action::Use), static_cast<int>(SDL_SCANCODE_E));
    DP_CHECK_EQ(table.primary(Action::Shove), static_cast<int>(SDL_SCANCODE_SPACE));
    DP_CHECK_EQ(table.primary(Action::Attack), bindings::kMouseLeft);
    DP_CHECK(table.has(Action::Run, SDL_SCANCODE_RSHIFT));
    DP_CHECK_EQ(bindings::label(table, Action::Shove), std::string("Space"));
    DP_CHECK_EQ(bindings::label(table, Action::Attack), std::string("LClick"));
}

DP_TEST(rebinding_to_a_taken_key_swaps_the_two_actions)
{
    bindings::Table table = bindings::defaults();
    const bindings::RebindResult result = bindings::rebind(table, Action::Shove, SDL_SCANCODE_F);
    DP_CHECK(result.ok);
    DP_CHECK(result.swappedWith.has_value());
    DP_CHECK(*result.swappedWith == Action::UseItem);
    DP_CHECK_EQ(table.primary(Action::Shove), static_cast<int>(SDL_SCANCODE_F));
    DP_CHECK_EQ(table.primary(Action::UseItem), static_cast<int>(SDL_SCANCODE_SPACE));
}

DP_TEST(rebinding_to_a_free_key_swaps_nothing)
{
    bindings::Table table = bindings::defaults();
    const bindings::RebindResult result = bindings::rebind(table, Action::Throw, SDL_SCANCODE_H);
    DP_CHECK(result.ok);
    DP_CHECK(!result.swappedWith.has_value());
    DP_CHECK_EQ(table.primary(Action::Throw), static_cast<int>(SDL_SCANCODE_H));
}

DP_TEST(escape_enter_and_function_keys_cannot_be_bound)
{
    bindings::Table table = bindings::defaults();
    DP_CHECK(!bindings::rebind(table, Action::Use, SDL_SCANCODE_ESCAPE).ok);
    DP_CHECK(!bindings::rebind(table, Action::Use, SDL_SCANCODE_RETURN).ok);
    DP_CHECK(!bindings::rebind(table, Action::Use, SDL_SCANCODE_F2).ok);
    DP_CHECK(table == bindings::defaults());
}

DP_TEST(mouse_buttons_can_be_bound)
{
    bindings::Table table = bindings::defaults();
    DP_CHECK(bindings::rebind(table, Action::Shove, bindings::kMouseMiddle).ok);
    DP_CHECK_EQ(table.primary(Action::Shove), bindings::kMouseMiddle);
    Input input;
    input.setBindings(table);
    input.setMouseButtonState(SDL_BUTTON_MIDDLE, true);
    DP_CHECK(input.pressed(Action::Shove));
}

DP_TEST(bindings_survive_a_json_round_trip)
{
    bindings::Table table = bindings::defaults();
    (void)bindings::rebind(table, Action::Shove, SDL_SCANCODE_F);
    (void)bindings::rebind(table, Action::Attack, bindings::kMouseRight);
    const bindings::Table back = bindings::fromJson(bindings::toJson(table));
    DP_CHECK(back == table);
}

DP_TEST(unknown_names_in_the_keys_file_keep_their_defaults)
{
    nlohmann::json json = {{"shove", "Not A Key"}, {"teleport", "T"}};
    const bindings::Table back = bindings::fromJson(json);
    DP_CHECK(back == bindings::defaults());
}

DP_TEST(prompts_name_the_players_keys)
{
    bindings::Table table = bindings::defaults();
    DP_CHECK_EQ(bindings::rebindText("Space to shove.", table), std::string("Space to shove."));
    (void)bindings::rebind(table, Action::Shove, SDL_SCANCODE_F);
    DP_CHECK_EQ(bindings::rebindText("Space to shove.", table), std::string("F to shove."));
    // A word that is not a key is left alone.
    DP_CHECK_EQ(bindings::rebindText("A door. Space to shove.", table), std::string("A door. F to shove."));
}

DP_TEST(an_action_reads_its_bound_key_not_the_default)
{
    bindings::Table table = bindings::defaults();
    (void)bindings::rebind(table, Action::Shove, SDL_SCANCODE_F);
    Input input;
    input.setBindings(table);
    input.setKeyState(SDL_SCANCODE_SPACE, true);
    DP_CHECK(!input.pressed(Action::Shove));
    DP_CHECK(input.pressed(Action::UseItem));
    input.consumeTransitions();
    input.setKeyState(SDL_SCANCODE_F, true);
    DP_CHECK(input.pressed(Action::Shove));
}

DP_TEST(toggle_crouch_stays_down_until_pressed_again)
{
    Input input;
    input.setToggle(Action::Crouch, true);
    input.setKeyState(SDL_SCANCODE_C, true);
    input.consumeTransitions();
    input.setKeyState(SDL_SCANCODE_C, false);
    input.consumeTransitions();
    DP_CHECK(input.down(Action::Crouch));
    input.setKeyState(SDL_SCANCODE_C, true);
    input.consumeTransitions();
    DP_CHECK(!input.down(Action::Crouch));
}
