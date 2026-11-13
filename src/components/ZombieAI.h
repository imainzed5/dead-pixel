#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

#include <algorithm>

#include "ecs/Types.h"

enum class ZombieState
{
    Wander,
    Investigate,
    Chase,
    Attack
};

enum class DecompStage
{
    Fresh = 0,    // Day 1-3:  fastest, highest threat
    Bloated = 1,  // Day 4-10: slower, durable, bio-hazard on death
    Decayed = 2,  // Day 11-20: slow, low individual threat, disease risk
    Skeletal = 3  // Day 20+: barely mobile, minimal threat
};

struct ZombieAI
{
    ZombieState state = ZombieState::Wander;
    glm::vec2 targetWorld{0.0f, 0.0f};
    float stateTimer = 0.0f;
    float pathRepathTimer = 0.0f;
    float attackCooldown = 0.0f;
    float attackWindup = 0.0f; // > 0: lunging; the hit resolves when it reaches 0
    float strikeTimer = 0.0f;  // > 0: just struck; the body is still thrown forward
    glm::vec2 strikeDir{0.0f, 0.0f};

    // Taking a blow: the zombie reels for `stagger` seconds, carried by
    // `knock` (world px/s, dying away), and cannot act until it recovers.
    float stagger = 0.0f;
    glm::vec2 knock{0.0f, 0.0f};
    float hitFlash = 0.0f;     // > 0: drawn bright for a moment after a hit

    // Which of the zombie looks (clothes, hair) this one wears.
    int look = 0;

    // Per-zombie base speed (rolled at spawn); threat tuning scales it into moveSpeed.
    float baseMoveSpeed = 112.0f;
    float moveSpeed = 112.0f;
    float visionRangeTiles = 8.0f;

    // Decomposition / aging
    DecompStage decomp = DecompStage::Fresh;
    int spawnDay = 1; // in-game day this zombie was created

    // Wandering: per-zombie RNG and a persistent heading (correlated random walk)
    // so zombies drift across the map instead of jittering in place.
    std::uint32_t rngState = 0; // 0 = seed lazily from the entity id
    float heading = 0.0f;       // radians, world space
    float idleTimer = 0.0f;     // > 0: standing still for a moment

    // Long-range moves (migration, distant noise) follow an A* path.
    std::vector<glm::ivec2> path;
    std::size_t pathIndex = 0;
    glm::vec2 pathGoal{0.0f, 0.0f};
    bool migrating = false; // sent toward the player's area by the threat director
    bool nightCaller = false; // sent to the player's door after dark; drifts off at dawn
    // A caller near the house has picked its way in (phase 2).
    bool entryChosen = false;
    // Seconds left limping off a spike board (half speed).
    float slowTimer = 0.0f;
    float boredTimer = 0.0f;  // > 0: just gave up on a goal, and noise does not turn it back

    // Who dealt the most recent damage (player, projectile shooter, fire,
    // survivor, vehicle). Kills are credited from this.
    Entity lastDamagedBy = kInvalidEntity;

    // Leash: a wandering zombie with leashTiles > 0 drifts back toward
    // leashHome when it strays further than that (the risen dead stay near
    // where they fell).
    glm::vec2 leashHome{0.0f, 0.0f};
    float leashTiles = 0.0f;
};

namespace ZombieBlow
{
// Knockback dies away at this rate (1/s), so a blow of N pixels carries the
// body about N pixels in all.
inline constexpr float kKnockDecay = 9.0f;
// hitFlash is set to this by a blow; the game sees it once, reacts (blood,
// shake, a held frame) and starts the flash proper.
inline constexpr float kFreshHit = 1.0f;
inline constexpr float kFlashSeconds = 0.12f;

// Land a blow on a zombie: it is thrown along `direction` (unit, world space)
// and reels, and whatever lunge it was winding up is broken off.
inline void land(ZombieAI& zombie, const glm::vec2& direction, float knockbackPixels)
{
    zombie.knock = direction * (knockbackPixels * kKnockDecay);
    zombie.stagger = std::clamp(0.14f + knockbackPixels * 0.006f, 0.18f, 0.45f);
    zombie.hitFlash = kFreshHit;
    zombie.attackWindup = 0.0f;
    zombie.strikeTimer = 0.0f;
}
} // namespace ZombieBlow
