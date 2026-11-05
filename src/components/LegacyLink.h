#pragma once

// Ties a world entity to a past character's grave record (index into the
// save's graves): their risen body, or the pack they left behind.
enum class LegacyLinkKind
{
    Risen,
    Pack,
    Grave
};

struct LegacyLink
{
    LegacyLinkKind kind = LegacyLinkKind::Grave;
    int graveIndex = -1;
};
