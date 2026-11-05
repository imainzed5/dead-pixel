#pragma once

// What each tone preset changes. GRIM HUMAN is the intended balance; HOPELESS
// leans towards scarcity and dread, BRUTAL ARCADE towards fast, forgiving
// action against more (but weaker) dead.

#include "save/SaveManager.h"

struct ToneTuning
{
    float suppliesScale = 1.0f;     // food, water, bandage, medical and supply spawns
    float weaponScale = 1.0f;       // weapon spawns
    float zombieCountScale = 1.0f;  // zombies placed at run start
    float zombieHealthScale = 1.0f; // every zombie that spawns
    float nightDarkness = 0.80f;    // how far ambient light falls at full night
    float saturation = 1.0f;        // world colour: below 1 drains it, above 1 punches it up
    float staminaRegenScale = 1.0f;
};

[[nodiscard]] inline ToneTuning toneTuning(SaveTonePreset preset)
{
    ToneTuning tuning{};
    switch (preset)
    {
    case SaveTonePreset::Hopeless:
        tuning.suppliesScale = 0.70f;
        tuning.weaponScale = 0.80f;
        tuning.zombieHealthScale = 1.15f;
        tuning.nightDarkness = 0.88f;
        tuning.saturation = 0.45f;
        tuning.staminaRegenScale = 0.90f;
        break;
    case SaveTonePreset::GrimHuman:
        break;
    case SaveTonePreset::BrutalArcade:
        tuning.suppliesScale = 1.15f;
        tuning.weaponScale = 1.50f;
        tuning.zombieCountScale = 1.35f;
        tuning.zombieHealthScale = 0.70f;
        tuning.nightDarkness = 0.65f;
        tuning.saturation = 1.12f;
        tuning.staminaRegenScale = 1.30f;
        break;
    }
    return tuning;
}
