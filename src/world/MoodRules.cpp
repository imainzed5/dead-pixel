#include "world/MoodRules.h"

#include <algorithm>
#include <cmath>

namespace moodrules
{
namespace
{
float ramp(float value, float from, float to)
{
    if (to <= from) return value >= from ? 1.0f : 0.0f;
    return std::clamp((value - from) / (to - from), 0.0f, 1.0f);
}
} // namespace

bool Params::neutral() const
{
    return saturation >= 0.999f && vignette <= 0.0f && blur <= 0.0f && blink <= 0.0f && coldEdge <= 0.0f &&
           warmCast <= 0.0f && shimmer <= 0.0f && doubleImage <= 0.0f && haze <= 0.0f && zoom <= 0.0f &&
           dimEdges <= 0.0f && shakePx <= 0.0f && redPulse <= 0.0f;
}

float saturationFor(float depression)
{
    if (depression >= 90.0f) return 0.45f;
    if (depression >= 70.0f) return 0.6f;
    if (depression >= 50.0f) return 0.8f;
    return 1.0f;
}

float vignetteFor(float panic)
{
    if (panic < kPanicFrom) return 0.0f;
    return kVignetteMin + (kVignetteMax - kVignetteMin) * ramp(panic, kPanicFrom, 100.0f);
}

float nextBlinkSeconds(float roll)
{
    return kBlinkMinSeconds + (kBlinkMaxSeconds - kBlinkMinSeconds) * std::clamp(roll, 0.0f, 1.0f);
}

float blinkShut(float t)
{
    if (t <= 0.0f || t >= kBlinkSeconds) return 0.0f;
    const float half = kBlinkSeconds * 0.5f;
    return t < half ? t / half : (kBlinkSeconds - t) / half;
}

Params params(const Facts& f, float level)
{
    Params p;
    level = std::clamp(level, 0.0f, 1.0f);
    if (level <= 0.0f) return p;

    p.saturation = 1.0f - (1.0f - saturationFor(f.depression)) * level;
    p.vignette = vignetteFor(f.panic) * level;
    if (f.panic >= kPanicShakeFrom) p.shakePx = std::round(f.heartbeat) * level; // 1 px on the beat
    p.blur = ramp(f.sleepDebt, kDebtBlurFrom, 100.0f) * level;
    if (f.sleepDebt >= kDebtBlurFrom) p.blur = std::max(p.blur, 0.35f * level);
    if (f.sleepDebt >= kDebtBlinkFrom) p.blink = f.blink * kBlinkDepth * level;
    if (f.bodyTemperature < kColdBelow) p.coldEdge = (0.45f + 0.55f * ramp(kColdBelow - f.bodyTemperature, 0.0f, 2.0f)) * level;
    const bool fever = f.illness >= kFeverIllness;
    if (f.bodyTemperature > kOverheatedAbove && !fever)
    {
        const float heat = 0.5f + 0.5f * ramp(f.bodyTemperature - kOverheatedAbove, 0.0f, 1.5f);
        p.warmCast = heat * level;
        p.shimmer = heat * level;
    }
    if (fever) p.doubleImage = (0.5f + 0.5f * ramp(f.illness, kFeverIllness, 100.0f)) * level;
    if (f.smoke) p.haze = 0.55f * level;
    if (f.held) p.zoom = kHeldZoom * level;
    if (f.listening) p.dimEdges = kListeningDim * level; // the share of light taken at the rim
    if (f.health < kPulseBelowHealth) p.redPulse = (0.4f + 0.6f * f.heartbeat) * level;
    return p;
}

Idle idleFor(float depression, float bodyTemperature, float sleepDebt, float stamina01)
{
    if (sleepDebt >= kDebtBlinkFrom || stamina01 < 0.15f) return Idle::Knee;
    if (bodyTemperature < kColdBelow) return Idle::Cold;
    if (depression >= 50.0f) return Idle::Low;
    return Idle::None;
}

const char* idleName(Idle idle)
{
    switch (idle)
    {
    case Idle::Low: return "low";
    case Idle::Cold: return "cold";
    case Idle::Knee: return "knee";
    default: return "";
    }
}

Sun sunAt(double hour, double dawn, double dusk)
{
    Sun sun;
    if (dusk <= dawn || hour <= dawn || hour >= dusk) return sun; // night: none but lamps
    const float t = static_cast<float>((hour - dawn) / (dusk - dawn)); // 0 sunrise, 1 sunset
    constexpr float kPi = 3.14159265f;
    // Morning sun in the east throws shadows west; evening the other way;
    // always a little north, up the screen.
    sun.dirX = -std::cos(kPi * t);
    sun.dirY = -0.35f;
    const float low = 1.0f - std::sin(kPi * t); // 1 at the horizon, 0 at noon
    sun.length = 0.35f + 2.4f * low * low;
    // The very ends of the day fade the shadow out as the light goes.
    const float edge = std::min(t, 1.0f - t);
    sun.length *= std::clamp(edge / 0.03f, 0.0f, 1.0f);
    return sun;
}
} // namespace moodrules
