# Dead Pixel Survival

Dead Pixel Survival is an isometric, systems-driven survival prototype built in C++20 with SDL2 and OpenGL.

Current state: playable early alpha with procedural town generation, combat, needs simulation, structure building, save metadata, and legacy grave persistence.

## Current Highlights

- C++20 ECS architecture with fixed-step update loop
- Isometric world generation with A* pathfinding
- Player needs simulation (hunger, thirst, fatigue, temperature, illness)
- Melee and projectile combat, stamina pressure, and wound systems
- Zombie AI with sight/noise response
- Buildable structures and degraded inter-run persistence
- Audio, particles, inventory, and UI overlays

## Build Requirements (Windows, MSYS2 UCRT64)

Install MSYS2 UCRT64 and these packages:

- mingw-w64-ucrt-x86_64-toolchain
- mingw-w64-ucrt-x86_64-cmake
- mingw-w64-ucrt-x86_64-ninja
- mingw-w64-ucrt-x86_64-SDL2
- mingw-w64-ucrt-x86_64-SDL2_mixer
- mingw-w64-ucrt-x86_64-glew
- mingw-w64-ucrt-x86_64-glm

## Build And Run (PowerShell)

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
.\build\dead_pixel_survival.exe
```

## Project Structure

- src: gameplay and engine source code
- assets: runtime game assets (sprites, sounds, shaders, maps, data)
- docs: GDD, audit reports, and milestone backlog
- tools: asset generation scripts
- save: local runtime saves (ignored by git)

## Documentation

- docs/dead_pixel_current_state_audit.md
- docs/dead_pixel_survival_gdd_v0.3.md
- docs/milestone_backlog.md

## CI

GitHub Actions CI is defined in `.github/workflows/ci.yml` and runs CMake + Ninja builds on Windows using MSYS2 UCRT64.

## License

This project is licensed under the MIT License. See LICENSE for details.
