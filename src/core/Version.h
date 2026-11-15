#pragma once

// The game's version (CMakeLists.txt's project version), shown bottom-right on
// the title screen and written in saves, and whether this is a packaged build
// (tools/package.py builds with DEAD_PIXEL_PACKAGED: the dev keys are hidden
// and saves go to the user's folder).
#ifndef DEAD_PIXEL_VERSION
#define DEAD_PIXEL_VERSION "0.0.0"
#endif

inline constexpr const char* kGameVersion = DEAD_PIXEL_VERSION;

#ifdef DEAD_PIXEL_PACKAGED
inline constexpr bool kPackagedBuild = true;
#else
inline constexpr bool kPackagedBuild = false;
#endif
