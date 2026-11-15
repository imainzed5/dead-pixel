#pragma once

// Mood on the screen (phase 12): the character's state, carried by the
// picture instead of numbers. Pure: facts in, the full-screen pass's
// parameters out. GameMood.cpp gathers the facts and draws.
//
//   Panic 50+        the view closes 15 to 35 %; at 75+ a 1 px shake at heartbeat rate
//   Tired (debt 70+) the edges blur; at 85+ a blink to 60 % black every 20 to 40 s
//   Depression 50+   saturation x0.8, x0.6 at 70+, x0.45 at 90+
//   Cold             a blue cast at the edges
//   Overheated       a slow shimmer near the horizon, a warm cast
//   Fever            a slight double image at the edges
//   Smoke            a grey haze
//   Held             zoom in 6 %
//   Listening        the edges dim 10 %
//   Health under 20% a red edge pulsing with the heartbeat
//
// Everything scales with the "screen effects" level (1 full, 0.5 reduced,
// 0 off); at 0 nothing here applies and the pass is skipped.
namespace moodrules
{
inline constexpr float kPanicFrom = 50.0f;
inline constexpr float kPanicShakeFrom = 75.0f;
inline constexpr float kVignetteMin = 0.15f;
inline constexpr float kVignetteMax = 0.35f;
inline constexpr float kDebtBlurFrom = 70.0f;
inline constexpr float kDebtBlinkFrom = 85.0f;
inline constexpr float kBlinkMinSeconds = 20.0f;
inline constexpr float kBlinkMaxSeconds = 40.0f;
inline constexpr float kBlinkSeconds = 0.35f;   // closed and open again
inline constexpr float kBlinkDepth = 0.6f;      // to 60 % black
inline constexpr float kHeldZoom = 0.06f;
inline constexpr float kListeningDim = 0.10f;
inline constexpr float kPulseBelowHealth = 0.20f;
inline constexpr float kColdBelow = 35.5f;      // body temperature
inline constexpr float kOverheatedAbove = 38.3f;
inline constexpr float kFeverIllness = 50.0f;
inline constexpr float kBreathPuffBelow = 5.0f; // air, outdoors

struct Facts
{
    float panic = 0.0f;        // 0-100
    float sleepDebt = 0.0f;    // shown debt, 0-100
    float depression = 0.0f;   // 0-100
    float bodyTemperature = 37.0f;
    float illness = 0.0f;      // 0-100
    bool smoke = false;        // breathing smoke now
    bool held = false;
    bool listening = false;
    float health = 1.0f;       // 0-1
    float heartbeat = 0.0f;    // 0-1, the phase of the current beat (1 on the beat)
    float blink = 0.0f;        // 0-1, how shut the eyes are (from the blink timer)
};

struct Params
{
    float saturation = 1.0f;   // multiplies the tone's
    float vignette = 0.0f;     // 0-1 share of the view closed
    float blur = 0.0f;         // 0-1 edge blur
    float blink = 0.0f;        // 0-1 black over everything
    float coldEdge = 0.0f;     // 0-1 blue at the edges
    float warmCast = 0.0f;     // 0-1
    float shimmer = 0.0f;      // 0-1
    float doubleImage = 0.0f;  // 0-1 (px offset scale)
    float haze = 0.0f;         // 0-1 grey
    float zoom = 0.0f;         // fraction to zoom in
    float dimEdges = 0.0f;     // 0-1
    float shakePx = 0.0f;      // pixels
    float redPulse = 0.0f;     // 0-1

    [[nodiscard]] bool neutral() const;
};

// The parameters for these facts at this level (0, 0.5, 1).
[[nodiscard]] Params params(const Facts& facts, float level);

// Saturation by depression (before the level).
[[nodiscard]] float saturationFor(float depression);
// The view closed by panic: 0 under 50, 15 % at 50 to 35 % at 100.
[[nodiscard]] float vignetteFor(float panic);
// Seconds to the next blink, from a 0-1 roll.
[[nodiscard]] float nextBlinkSeconds(float roll);
// How shut the eyes are `t` seconds into a blink (0 to 1 and back).
[[nodiscard]] float blinkShut(float t);

// The idle a body falls into: 0 none, 1 low (depressed), 2 cold (shiver),
// 3 knee (exhausted). The strongest wins: knee, then cold, then low.
enum class Idle
{
    None,
    Low,
    Cold,
    Knee
};
[[nodiscard]] Idle idleFor(float depression, float bodyTemperature, float sleepDebt, float stamina01);
[[nodiscard]] const char* idleName(Idle idle); // "", "low", "cold", "knee"

// Long shadows: the sun's direction and how long a shadow is (1 = its
// height) at this hour, or 0 length at night. Dawn and dusk stretch them.
struct Sun
{
    float dirX = 0.0f;  // screen direction a shadow falls
    float dirY = 0.0f;
    float length = 0.0f;
};
[[nodiscard]] Sun sunAt(double hour, double dawn, double dusk);
} // namespace moodrules
