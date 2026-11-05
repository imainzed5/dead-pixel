cmake_minimum_required(VERSION 3.25)

project(dead_pixel_survival VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEAD_PIXEL_BUILD_TESTS "Build the unit test runner" ON)

find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_mixer REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(glew REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)

# stb ships headers only (no CMake package); fail at configure time if missing.
find_path(STB_INCLUDE_DIR stb/stb_image.h REQUIRED)

function(dead_pixel_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Everything except main(): shared by the game and the test runner.
add_library(dead_pixel_core STATIC
    src/core/Game.cpp
    src/core/GameScene.cpp
    src/core/GameRunEnd.cpp
    src/core/GameUpdate.cpp
    src/core/GameRender.cpp
    src/core/GameUi.cpp
    src/core/GameMenus.cpp
    src/core/GameVehicles.cpp
    src/core/GameFire.cpp
    src/core/GameInventory.cpp
    src/core/GameLegacy.cpp
    src/core/Input.cpp
    src/ai/Pathfinder.cpp
    src/ecs/EntityManager.cpp
    src/ecs/World.cpp
    src/map/TileMap.cpp
    src/map/TiledLoader.cpp
    src/map/MapGenerator.cpp
    src/noise/NoiseModel.cpp
    src/save/SaveManager.cpp
    src/systems/InputSystem.cpp
    src/systems/CombatSystem.cpp
    src/systems/CollisionSystem.cpp
    src/systems/InteractionSystem.cpp
    src/systems/MovementSystem.cpp
    src/systems/NeedsSystem.cpp
    src/systems/BleedingSystem.cpp
    src/systems/ZombieAISystem.cpp
    src/systems/RenderSystem.cpp
    src/systems/InventorySystem.cpp
    src/systems/MentalStateSystem.cpp
    src/systems/CraftingSystem.cpp
    src/systems/ProjectileSystem.cpp
    src/systems/WoundSystem.cpp
    src/systems/BuildSystem.cpp
    src/systems/SurvivorAISystem.cpp
    src/systems/SurvivorInteractionSystem.cpp
    src/systems/WeatherSystem.cpp
    src/systems/DistrictInfectionSystem.cpp
    src/systems/ThreatDirectorSystem.cpp
    src/world/EntityFactory.cpp
    src/items/ItemDatabase.cpp
    src/rendering/Camera.cpp
    src/rendering/Shader.cpp
    src/rendering/Texture.cpp
    src/rendering/SpriteBatch.cpp
    src/rendering/BitmapFont.cpp
    src/rendering/SpriteSheet.cpp
    src/rendering/StructureView.cpp
    src/legacy/LegacyRules.cpp
    src/ui/Hud.cpp
    src/systems/AnimationSystem.cpp
    src/systems/ParticleSystem.cpp
    src/audio/AudioManager.cpp
    src/dev/DevHarness.cpp
    src/dev/LaunchOptions.cpp
)

target_include_directories(dead_pixel_core PUBLIC src)
target_include_directories(dead_pixel_core SYSTEM PUBLIC ${STB_INCLUDE_DIR})

target_link_libraries(dead_pixel_core PUBLIC
    SDL2::SDL2
    SDL2_mixer::SDL2_mixer
    OpenGL::GL
    GLEW::GLEW
    glm::glm
    nlohmann_json::nlohmann_json
)
dead_pixel_warnings(dead_pixel_core)

add_executable(dead_pixel_survival src/main.cpp)

if(TARGET SDL2::SDL2main)
    target_link_libraries(dead_pixel_survival PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(dead_pixel_survival PRIVATE dead_pixel_core)
dead_pixel_warnings(dead_pixel_survival)

add_custom_command(TARGET dead_pixel_survival POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:dead_pixel_survival>/assets
)

if(DEAD_PIXEL_BUILD_TESTS)
    enable_testing()

    add_executable(dead_pixel_tests
        tests/test_main.cpp
        tests/test_input.cpp
        tests/test_mapgen.cpp
        tests/test_save.cpp
        tests/test_simulation.cpp
        tests/test_survival.cpp
        tests/test_hud.cpp
        tests/test_structures.cpp
        tests/test_legacy.cpp
    )
    target_link_libraries(dead_pixel_tests PRIVATE dead_pixel_core)
    target_compile_definitions(dead_pixel_tests PRIVATE
        DEAD_PIXEL_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
        SDL_MAIN_HANDLED)
    dead_pixel_warnings(dead_pixel_tests)

    add_test(NAME unit COMMAND dead_pixel_tests)
endif()
