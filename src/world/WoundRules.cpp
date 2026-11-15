#include "world/WoundRules.h"

#include <algorithm>
#include <cctype>

namespace woundrules
{
namespace
{
bool open(const Wound& w) { return w.type == WoundType::Cut || w.type == WoundType::Bite; }
bool deepOpen(const Wound& w) { return open(w) && w.depth == WoundDepth::Deep && !w.stitched; }
} // namespace

bool isArm(BodyPart part) { return part == BodyPart::LeftArm || part == BodyPart::RightArm; }
bool isLeg(BodyPart part) { return part == BodyPart::LeftLeg || part == BodyPart::RightLeg; }

const char* partName(BodyPart part)
{
    switch (part)
    {
    case BodyPart::Head: return "head";
    case BodyPart::Torso: return "torso";
    case BodyPart::LeftArm: return "left arm";
    case BodyPart::RightArm: return "right arm";
    case BodyPart::LeftLeg: return "left leg";
    case BodyPart::RightLeg: return "right leg";
    }
    return "";
}

BodyPart clawPart(float roll)
{
    if (roll < 0.25f) return BodyPart::LeftArm;
    if (roll < 0.50f) return BodyPart::RightArm;
    if (roll < 0.80f) return BodyPart::Torso;
    if (roll < 0.90f) return BodyPart::Head;
    return roll < 0.95f ? BodyPart::LeftLeg : BodyPart::RightLeg;
}

BodyPart heldBitePart(float roll)
{
    if (roll < 0.20f) return BodyPart::LeftArm;
    if (roll < 0.40f) return BodyPart::RightArm;
    if (roll < 0.70f) return BodyPart::Torso;
    return BodyPart::Head;
}

BodyPart crawlerPart(float roll) { return legPart(roll); }

BodyPart limbPart(float roll)
{
    if (roll < 0.25f) return BodyPart::LeftArm;
    if (roll < 0.50f) return BodyPart::RightArm;
    if (roll < 0.75f) return BodyPart::LeftLeg;
    return BodyPart::RightLeg;
}

BodyPart legPart(float roll) { return roll < 0.5f ? BodyPart::LeftLeg : BodyPart::RightLeg; }

Wound make(WoundType type, BodyPart part, WoundDepth depth, bool contaminated)
{
    Wound wound;
    wound.type = type;
    wound.part = part;
    if (type == WoundType::Fracture && !isArm(part) && !isLeg(part)) wound.part = BodyPart::LeftLeg;
    wound.depth = (type == WoundType::Cut || type == WoundType::Bite) ? depth : WoundDepth::Shallow;
    wound.contaminated = contaminated;
    return wound;
}

BodyEffects effects(const std::vector<Wound>& wounds)
{
    BodyEffects fx;
    for (const Wound& w : wounds)
    {
        const bool arm = isArm(w.part);
        const bool leg = isLeg(w.part);
        if (open(w) && !w.dressed)
        {
            if (arm) fx.swingStaminaScale = std::max(fx.swingStaminaScale, 1.15f);
            if (leg) fx.moveScale = std::min(fx.moveScale, 0.92f);
            if (w.part == BodyPart::Torso) fx.maxStaminaPenalty = std::max(fx.maxStaminaPenalty, 10.0f);
            if (w.part == BodyPart::Head)
            {
                fx.sightPenaltyTiles = std::max(fx.sightPenaltyTiles, 1);
                fx.headVignette = true;
            }
        }
        if (deepOpen(w))
        {
            if (arm) fx.rebleedOnSwing = true;
            if (leg) fx.rebleedOnRun = true;
            if (w.part == BodyPart::Torso || w.part == BodyPart::Head)
            {
                fx.slowBleedPerHour = std::max(fx.slowBleedPerHour, w.dressed ? kSlowBleedDressedPerHour : kSlowBleedPerHour);
            }
        }
        if (w.type == WoundType::Fracture)
        {
            if (arm)
            {
                fx.armHeldIn = true;
                if (w.splinted) fx.windupScale = std::max(fx.windupScale, 1.2f);
                else
                {
                    fx.noTwoHanded = true;
                    fx.missWearScale = 2.0f;
                }
            }
            if (leg)
            {
                if (w.splinted)
                {
                    fx.moveScale = std::min(fx.moveScale, 0.80f);
                    fx.runAtWalkSpeed = true;
                }
                else
                {
                    fx.moveScale = std::min(fx.moveScale, 0.55f);
                    fx.canRun = false;
                }
            }
        }
        if (leg && w.type != WoundType::Bruise && (w.type == WoundType::Fracture || !w.dressed)) fx.limp = true;
    }
    return fx;
}

float maxHealthPenalty(const std::vector<Wound>& wounds)
{
    float penalty = 0.0f;
    for (const Wound& w : wounds) penalty += w.type == WoundType::Fracture ? 10.0f : 5.0f;
    return penalty;
}

float maxHealth(float baseMaximum, const std::vector<Wound>& wounds)
{
    return std::max(20.0f, baseMaximum - maxHealthPenalty(wounds));
}

float healHours(const Wound& w)
{
    if (w.infected || w.turning) return -1.0f;
    switch (w.type)
    {
    case WoundType::Bruise: return 2.0f;
    case WoundType::Cut:
        if (w.depth == WoundDepth::Shallow) return 6.0f;
        return w.stitched ? 12.0f : 30.0f;
    case WoundType::Bite: return 18.0f;
    case WoundType::Fracture: return w.splinted ? 96.0f : -1.0f;
    }
    return 6.0f;
}

float healRate(bool bedRest, int medicineLevel, float healTimeScale)
{
    const float medicine = 1.0f + 0.08f * static_cast<float>(std::clamp(medicineLevel, 0, 5));
    return (bedRest ? kBedRestHealScale : 1.0f) * medicine / std::max(0.1f, healTimeScale);
}

bool festers(const Wound& w) { return w.type == WoundType::Bite && w.contaminated && !w.infected; }

bool mayInfect(const Wound& w)
{
    if (w.type != WoundType::Cut || w.infected || w.dressed) return false;
    return (w.depth == WoundDepth::Deep && !w.stitched) || w.contaminated;
}

float feverDegrees(float illness) { return 0.3f * std::max(0.0f, illness) / 20.0f; }

float turnDeathHours(float roll) { return 12.0f + 12.0f * std::clamp(roll, 0.0f, 1.0f); }

Treatment treatmentFromName(const std::string& name)
{
    std::string n;
    for (char c : name) n.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (n == "dress") return Treatment::Dress;
    if (n == "dress_clean") return Treatment::DressClean;
    if (n == "stitch") return Treatment::Stitch;
    if (n == "splint") return Treatment::Splint;
    if (n == "cure") return Treatment::CureInfection;
    if (n == "disinfect") return Treatment::Disinfect;
    return Treatment::None;
}

bool canTreat(Treatment treatment, const Wound& w)
{
    switch (treatment)
    {
    case Treatment::Dress:
        return open(w) && (!w.dressed || w.dressingHours >= kDressingDirtyHours * 0.5f);
    case Treatment::DressClean:
        return open(w) && (!w.dressed || w.contaminated || w.dressingHours >= kDressingDirtyHours * 0.5f);
    case Treatment::Stitch: return deepOpen(w);
    case Treatment::Splint: return w.type == WoundType::Fracture && !w.splinted;
    case Treatment::CureInfection: return w.infected && !w.turning;
    case Treatment::Disinfect: return open(w) && w.contaminated && !w.infected;
    case Treatment::None: break;
    }
    return false;
}

int worstFor(Treatment treatment, const std::vector<Wound>& wounds)
{
    int best = -1;
    int bestScore = -1;
    for (std::size_t i = 0; i < wounds.size(); ++i)
    {
        const Wound& w = wounds[i];
        if (!canTreat(treatment, w)) continue;
        int score = urgency(w);
        if ((treatment == Treatment::Dress || treatment == Treatment::DressClean) && !w.dressed) score += 1000;
        if (score > bestScore)
        {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void apply(Treatment treatment, Wound& w)
{
    switch (treatment)
    {
    case Treatment::Dress:
        w.dressed = true;
        w.dressingHours = 0.0f;
        break;
    case Treatment::DressClean:
        w.dressed = true;
        w.dressingHours = 0.0f;
        w.contaminated = false;
        w.infectionTimer = 0.0f;
        break;
    case Treatment::Stitch: w.stitched = true; break;
    case Treatment::Splint: w.splinted = true; break;
    case Treatment::CureInfection:
        w.infected = false;
        w.contaminated = false;
        w.infectionTimer = 0.0f;
        w.infectedHours = 0.0f;
        break;
    case Treatment::Disinfect:
        w.contaminated = false;
        w.infectionTimer = 0.0f;
        break;
    case Treatment::None: break;
    }
}

float treatSeconds(Treatment treatment, int medicineLevel)
{
    const float scale = 1.0f - 0.10f * static_cast<float>(std::clamp(medicineLevel, 0, 5));
    switch (treatment)
    {
    case Treatment::Stitch: return kStitchSeconds * scale;
    case Treatment::Splint: return kSplintSeconds * scale;
    default: return 0.0f;
    }
}

const char* treatmentVerb(Treatment treatment)
{
    switch (treatment)
    {
    case Treatment::Dress: return "dress";
    case Treatment::DressClean: return "dress clean";
    case Treatment::Stitch: return "stitch";
    case Treatment::Splint: return "splint";
    case Treatment::CureInfection: return "treat the infection";
    case Treatment::Disinfect: return "disinfect";
    case Treatment::None: break;
    }
    return "";
}

int urgency(const Wound& w)
{
    if (w.turning) return 100;
    if (w.infected) return 90;
    if (w.type == WoundType::Fracture) return w.splinted ? 40 : 80;
    if (deepOpen(w)) return w.dressed ? 65 : 70;
    if (w.type == WoundType::Bite) return w.contaminated ? 60 : 55;
    if (w.type == WoundType::Cut)
    {
        if (w.depth == WoundDepth::Deep) return 35;
        return w.dressed ? 20 : 30;
    }
    return 10;
}

namespace
{
std::string typeName(const Wound& w)
{
    const bool deep = w.depth == WoundDepth::Deep;
    switch (w.type)
    {
    case WoundType::Bruise: return "Bruise";
    case WoundType::Cut: return deep ? "Deep cut" : "Cut";
    case WoundType::Bite: return deep ? "Deep bite" : "Bite";
    case WoundType::Fracture: return "Broken";
    }
    return "Wound";
}
} // namespace

std::string label(const Wound& w)
{
    if (w.type == WoundType::Fracture) return std::string("Broken ") + partName(w.part);
    return typeName(w) + ", " + partName(w.part);
}

std::string need(const Wound& w, bool seesInfection)
{
    if (w.turning) return "turning: nothing helps";
    if (w.infected) return "infected: antibiotics";
    if (w.type == WoundType::Fracture) return w.splinted ? "splinted, mending" : "needs a splint";
    if (deepOpen(w)) return "needs stitches";
    if (w.type == WoundType::Bruise) return "mends on its own";
    if (seesInfection && w.contaminated) return "dirty: will fester";
    if (!w.dressed) return "needs a dressing";
    if (w.dressingHours >= kDressingDirtyHours * 0.75f) return "dressing needs changing";
    return w.stitched ? "stitched, dressed" : "dressed";
}

std::vector<std::string> hudLabels(const std::vector<Wound>& wounds, bool bleeding, float illness)
{
    std::vector<std::string> out;
    auto add = [&](const std::string& text)
    {
        if (std::find(out.begin(), out.end(), text) == out.end()) out.push_back(text);
    };
    if (bleeding) add("Bleeding");
    std::vector<const Wound*> sorted;
    for (const Wound& w : wounds) sorted.push_back(&w);
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Wound* a, const Wound* b) { return urgency(*a) > urgency(*b); });
    bool infected = false;
    for (const Wound* w : sorted)
    {
        infected = infected || w->infected;
        if (w->turning) add("Turning");
        else if (w->infected) add(w->type == WoundType::Bite ? "Infected bite" : "Infected cut");
        else if (w->type == WoundType::Fracture)
            add(std::string(w->splinted ? "Splinted " : "Broken ") + (isArm(w->part) ? "arm" : "leg"));
        else add(typeName(*w));
    }
    if (infected && feverDegrees(illness) >= 0.3f) add("Fever");
    return out;
}
} // namespace woundrules
