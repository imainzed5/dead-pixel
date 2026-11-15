#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

#include "components/Vehicle.h"
#include "map/LayoutData.h"

// Vehicles as rules (overhaul phase 11): what each kind is, starting one
// (the key, the wire, the crank), wear and the condition bands, a hit on a
// wall or a body, the arcade drive, and where the town parks them. Pure:
// GameVehicles.cpp and VehicleSystem do the rest.
namespace vehiclerules
{
// --- The kinds ----------------------------------------------------------------
struct KindInfo
{
    const char* name = "sedan";
    float topSpeed = 250.0f;        // px/s
    float reverseSpeed = 80.0f;     // px/s
    float accel = 120.0f;           // px/s per second
    float brake = 320.0f;
    float drag = 70.0f;             // coasting
    float turnRate = 2.4f;          // rad/s at low speed
    float length = 72.0f;           // world px
    float width = 28.0f;
    float radius = 14.0f;           // the three collision circles
    float circleOffset = 22.0f;     // of the end circles from the middle
    int trunkSlots = 12;
    float trunkKg = 60.0f;
    float maxFuel = 30.0f;
    float fuelPerGameHour = 6.0f;
    int seats = 2;
    float hearing = 1.0f;           // how far its engine carries (x)
    bool engine = true;
};
[[nodiscard]] const KindInfo& info(VehicleKind kind);
[[nodiscard]] const char* kindName(VehicleKind kind); // "sedan"
// By name, any case. False when unknown.
[[nodiscard]] bool parseKind(const char* text, VehicleKind& out);

// Paints: a sedan comes in four, a pickup in two (the sprites are drawn per paint).
inline constexpr int kSedanColors = 4;
inline constexpr int kPickupColors = 2;
[[nodiscard]] int colorCount(VehicleKind kind);
[[nodiscard]] const char* colorName(VehicleKind kind, int color); // "red"
// "the red sedan", "the green pickup", "the bicycle"
[[nodiscard]] const char* describe(VehicleKind kind, int color, char* buffer, std::size_t size);

// --- Noise ----------------------------------------------------------------------
inline constexpr float kEngineNoiseSeconds = 0.3f; // a running engine, each this often
inline constexpr float kIdleNoiseSeconds = 1.0f;   // idling (Medium)
inline constexpr float kAlarmSeconds = 45.0f;      // Loud each second
inline constexpr int kAlarmDays = 10;              // its battery lasts the first days of a life
inline constexpr float kCrankSeconds = 1.0f;

// --- Starting it ----------------------------------------------------------------
inline constexpr float kHotwireSeconds = 20.0f;
inline constexpr float kHotwireFailChance = 0.40f;
inline constexpr float kHotwireFailPerLevel = 0.10f;
inline constexpr float kKeyInsideChance = 1.0f / 3.0f;
inline constexpr float kAlarmChance = 0.2f;
[[nodiscard]] float hotwireSeconds(int mechanics);
[[nodiscard]] float hotwireFailChance(int mechanics);
// `roll` in [0, 1): true when the wire fails at the end (a Loud crank).
[[nodiscard]] bool hotwireFails(float roll, int mechanics);
// Each start of a car under 50: (50 - condition) x 2 % to fail.
[[nodiscard]] float startFailChance(float condition);
[[nodiscard]] bool startFails(float roll, float condition);

// --- Condition --------------------------------------------------------------------
enum class Band
{
    Good,    // 70 and up
    Worn,    // 40 and up: top speed x0.86
    Damaged, // 10 and up: x0.72, the engine Explosive, starts fail
    Broken   // under 10: will not run
};
[[nodiscard]] Band band(float condition);
[[nodiscard]] const char* bandName(Band band); // "Good"
[[nodiscard]] float bandSpeedScale(Band band);
[[nodiscard]] bool runs(float condition);

// Wear: 0.5 a game hour driven; rain and snow x1.5; off the road x2.
inline constexpr float kWearPerGameHour = 0.5f;
[[nodiscard]] float wearPerGameHour(bool wetOrSnow, bool offRoad);
inline constexpr float kOffRoadSpeedScale = 0.7f;
inline constexpr float kFlatTyreSpeedScale = 0.7f; // each flat
inline constexpr float kFlatChance = 0.10f;        // running down scenery
inline constexpr float kBetweenLivesWear = 15.0f;  // a car used in a life, next life

// Everything that caps how fast it goes now.
struct SpeedInput
{
    float condition = 100.0f;
    int tyres = 4;
    bool offRoad = false;
};
[[nodiscard]] float topSpeed(VehicleKind kind, const SpeedInput& in);
// Turning falls with speed; snow halves it. Nothing turns standing still
// (a car rolls to turn), but a bicycle turns at walking pace.
[[nodiscard]] float turnRate(VehicleKind kind, float speed, bool snow);
// Idle, Slow, Fast: the HUD's word.
[[nodiscard]] const char* speedWord(float speed);
inline constexpr float kZoomOutSpeed = 150.0f; // the view pulls back 15 % above it
inline constexpr float kCameraLeadTiles = 3.0f;

// Fuel used over `gameHours` of driving.
[[nodiscard]] float fuelUsed(VehicleKind kind, float gameHours);
inline constexpr float kCanFuel = 12.0f;       // one Fuel Can
inline constexpr float kRefuelSeconds = 6.0f;

// --- Repair ---------------------------------------------------------------------
inline constexpr float kRepairSeconds = 60.0f;
inline constexpr float kRepairAmount = 25.0f;
// +25 (Mechanics adds 10 % a level); doubled on a garage's lift.
[[nodiscard]] float repairAmount(int mechanics, bool lift);

// --- Hits ------------------------------------------------------------------------
inline constexpr float kWallDamageSpeed = 120.0f;   // px/s: above it a wall hurts
inline constexpr float kRunDownSceneryAbove = 150.0f;
inline constexpr float kRunDownBodyAbove = 140.0f;
struct WallHit
{
    bool hurts = false;
    float vehicleDamage = 0.0f; // speed / 20
    float driverDamage = 0.0f;  // speed / 40
};
[[nodiscard]] WallHit wallHit(float speed);

struct BodyHit
{
    bool thrown = false;          // 65 damage, on the floor
    float damage = 0.0f;
    float conditionLoss = 0.0f;
    float speedKeep = 1.0f;       // of the car's speed after
    bool blocked = false;         // too slow: it stops against them
};
inline constexpr float kBodyDamage = 65.0f;
inline constexpr float kBodySpeedLoss = 0.35f;
inline constexpr float kBodyConditionLoss = 5.0f;
inline constexpr float kBruteConditionLoss = 15.0f;
inline constexpr float kThrownDownSeconds = 2.5f;
[[nodiscard]] BodyHit bodyHit(float speed, bool brute);

// Battered by the dead: 3 a blow; after this long by three or more, a window goes.
inline constexpr float kBlowDamage = 3.0f;
inline constexpr float kWindowSeconds = 20.0f;
inline constexpr int kWindowCrowd = 3;
inline constexpr float kHiddenSightScale = 0.2f;   // crouched in a parked car, engine off
inline constexpr float kHeadlightSeenTiles = 14.0f;

// --- The bicycle -------------------------------------------------------------------
inline constexpr float kPedalStaminaPerSecond = 2.0f;
inline constexpr float kFallDamage = 5.0f;
inline constexpr float kFallDownSeconds = 1.2f;
inline constexpr float kBodyStopsBikeAbove = 60.0f;
inline constexpr float kRainBrakeScale = 0.5f;

// --- The drive ---------------------------------------------------------------------
// The arcade model. The keys give a direction on the screen, turned into the
// world as the player's walk is; the vehicle turns toward it at its turn rate
// and drives on, or brakes and backs toward it when it lies behind.
struct DriveState
{
    float heading = 0.0f;
    float speed = 0.0f;
};
struct DriveInput
{
    glm::vec2 want{0.0f};   // world direction wanted (unit), or zero: coast
    float top = 250.0f;      // the cap now (topSpeed)
    bool canThrottle = true; // fuel and a running engine, or legs with stamina
    bool snow = false;
    bool rain = false;       // a bicycle brakes long in the rain
};
// Where the want lies past this, it brakes and then reverses.
inline constexpr float kReverseAngle = 2.0f; // radians (~115 degrees)
void drive(VehicleKind kind, DriveState& state, const DriveInput& in, float dt);
[[nodiscard]] glm::vec2 forward(float heading);

// The body as three circles along its length (world px).
[[nodiscard]] std::array<glm::vec2, 3> circles(VehicleKind kind, const glm::vec2& centre, float heading);
// The tailgate and trunk: a point just behind the body.
[[nodiscard]] glm::vec2 rear(VehicleKind kind, const glm::vec2& centre, float heading);
// The driver's door (left of the heading) and the other side.
[[nodiscard]] glm::vec2 side(VehicleKind kind, const glm::vec2& centre, float heading, bool driver);

// Which of eight facings a heading draws as (0 = +x, clockwise in world
// space, so 1 = +x+y, straight down the screen), and whether that facing is
// a mirror of a drawn one; `drawn` is the facing whose sprite is used.
struct Facing8
{
    int dir = 0;
    int drawn = 0;
    bool mirrored = false;
};
[[nodiscard]] Facing8 facing8(float heading);

// --- Where the town parks them ------------------------------------------------------
struct Spot
{
    VehicleKind kind = VehicleKind::Sedan;
    glm::vec2 centre{0.0f};  // world px
    float heading = 0.0f;
    int color = 0;
    float condition = 70.0f;
    float fuel = 10.0f;
    bool keyInside = false;
    bool alarm = false;
    int building = 0;        // what it was parked for (index + 1)
};
// Counts per town: sedans 3-5, pickups 1-2, bicycles 4-6.
inline constexpr int kSedansMin = 3;
inline constexpr int kSedansMax = 5;
inline constexpr int kPickupsMin = 1;
inline constexpr int kPickupsMax = 2;
inline constexpr int kBicyclesMin = 4;
inline constexpr int kBicyclesMax = 6;
// Fixed at world creation from the layout and the world seed: sedans by
// houses (and the police lot), pickups at the garage (then a warehouse,
// workshop or hardware store), bicycles by houses and the biggest
// building in town. Each on open ground where its body fits.
[[nodiscard]] std::vector<Spot> place(const LayoutData& layout, std::uint32_t worldSeed);
// The body fits at `centre`: every tile its circles touch is open ground
// outside, not a doorway.
[[nodiscard]] bool fits(const LayoutData& layout, VehicleKind kind, const glm::vec2& centre, float heading);
} // namespace vehiclerules
