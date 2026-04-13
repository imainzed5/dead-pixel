#pragma once

#include <string>

enum class InteractableType
{
    FoodPickup,
    WaterPickup,
    GraveMarker,
    SleepSpot,
    WeaponPickup,
    BandagePickup,
    Container,
    JournalNote
};

struct Interactable
{
    InteractableType type = InteractableType::FoodPickup;
    float hungerRestore = 30.0f;
    float thirstRestore = 0.0f;
    float sleepHours = 6.0f;        // for SleepSpot
    float sleepQuality = 0.8f;      // for SleepSpot
    int weaponCategory = 0;         // for WeaponPickup (0=Improvised,1=Bladed,2=Blunt,3=Ranged,4=Unarmed)
    std::string prompt = "Eat";
    std::string message;
};
