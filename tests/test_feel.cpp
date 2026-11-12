// Characters and combat feel: the eight facings, what a landed blow does to a
// zombie, and that the atlas holds every animation the game asks for by name.
#include "TestFramework.h"

#include <fstream>
#include <string>

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include "components/Animation.h"
#include "components/ZombieAI.h"

DP_TEST(facing_follows_the_screen_not_the_world_axes)
{
    // World (x, y) lands on screen at (x - y, (x + y) / 2).
    DP_CHECK_EQ(facingDir(glm::vec2(1.0f, -1.0f), 2), 0);  // screen right
    DP_CHECK_EQ(facingDir(glm::vec2(1.0f, 0.0f), 4), 1);   // down-right
    DP_CHECK_EQ(facingDir(glm::vec2(1.0f, 1.0f), 6), 2);   // toward the viewer
    DP_CHECK_EQ(facingDir(glm::vec2(0.0f, 1.0f), 0), 3);   // down-left
    DP_CHECK_EQ(facingDir(glm::vec2(-1.0f, 1.0f), 0), 4);  // screen left
    DP_CHECK_EQ(facingDir(glm::vec2(-1.0f, 0.0f), 0), 5);  // up-left
    DP_CHECK_EQ(facingDir(glm::vec2(-1.0f, -1.0f), 2), 6); // away
    DP_CHECK_EQ(facingDir(glm::vec2(0.0f, -1.0f), 3), 7);  // up-right
}

DP_TEST(facing_holds_until_well_into_the_next_sector)
{
    // 26 degrees round from screen right: past the halfway line, but a
    // character already facing right keeps facing right.
    const glm::vec2 justPast(std::cos(0.45f) + std::sin(0.45f), std::sin(0.45f) - std::cos(0.45f));
    DP_CHECK_EQ(facingDir(justPast, 0), 0);
    DP_CHECK_EQ(facingDir(justPast, 4), 1);
    // No direction at all keeps the facing.
    DP_CHECK_EQ(facingDir(glm::vec2(0.0f, 0.0f), 5), 5);
}

DP_TEST(a_blow_throws_a_zombie_back_and_breaks_its_lunge)
{
    ZombieAI zombie{};
    zombie.attackWindup = 0.3f;
    zombie.strikeTimer = 0.2f;
    ZombieBlow::land(zombie, glm::vec2(1.0f, 0.0f), 24.0f);

    DP_CHECK(zombie.stagger > 0.0f);
    DP_CHECK(zombie.knock.x > 0.0f);
    DP_CHECK_EQ(zombie.knock.y, 0.0f);
    DP_CHECK_EQ(zombie.attackWindup, 0.0f);
    DP_CHECK_EQ(zombie.strikeTimer, 0.0f);
    DP_CHECK_EQ(zombie.hitFlash, ZombieBlow::kFreshHit);

    // Carried at a speed that dies away at kKnockDecay, the body travels
    // about the knockback distance in all.
    float travelled = 0.0f;
    glm::vec2 knock = zombie.knock;
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i)
    {
        travelled += glm::length(knock) * dt;
        knock *= 1.0f - ZombieBlow::kKnockDecay * dt;
    }
    DP_CHECK(travelled > 20.0f && travelled < 28.0f);

    // A heavier weapon staggers for longer.
    ZombieAI heavy{};
    ZombieBlow::land(heavy, glm::vec2(0.0f, 1.0f), 48.0f);
    DP_CHECK(heavy.stagger > zombie.stagger);
}

DP_TEST(the_atlas_has_every_character_animation_the_game_names)
{
    std::ifstream file(std::string(DEAD_PIXEL_ASSETS_DIR) + "/sprites/spritesheet.json");
    DP_CHECK(file.good());
    const nlohmann::json atlas = nlohmann::json::parse(file);
    const auto& sprites = atlas["sprites"];

    const auto frames = [&](const std::string& name) -> int
    {
        if (!sprites.contains(name))
        {
            return 0;
        }
        return sprites[name].contains("frames") ? static_cast<int>(sprites[name]["frames"].size()) : 1;
    };

    const char* views[] = {"_s", "_se", "_e", "_ne", "_n"};
    const char* loadouts[] = {"none", "club", "blade", "gun"};
    const char* moves[] = {"idle", "walk", "run", "crouch", "crouchwalk", "hurt"};
    for (const char* view : views)
    {
        for (const char* loadout : loadouts)
        {
            for (const char* move : moves)
            {
                DP_CHECK(frames(std::string("player_") + loadout + "_" + move + view) >= 1);
            }
        }
        // Attacks are shown frame by frame from the combat timers, so the
        // counts must cover wind-up, strike and recovery.
        DP_CHECK(frames(std::string("player_punch") + view) >= 5);
        DP_CHECK(frames(std::string("player_swing") + view) >= 6);
        DP_CHECK(frames(std::string("player_stab") + view) >= 5);
        DP_CHECK(frames(std::string("player_shoot") + view) >= 3);
        DP_CHECK(frames(std::string("player_death") + view) >= 2);
        for (int look = 0; look < 3; ++look)
        {
            const std::string set = "zombie" + std::to_string(look);
            DP_CHECK(frames(set + "_idle" + view) >= 1);
            DP_CHECK(frames(set + "_walk" + view) >= 4);
            DP_CHECK(frames(set + "_lunge" + view) >= 5);
            DP_CHECK(frames(set + "_stagger" + view) >= 2);
            DP_CHECK(frames(set + "_death" + view) >= 2);
        }
    }
}
