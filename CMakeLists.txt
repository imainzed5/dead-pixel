cmake_minimum_required(VERSION 3.25)

project(dead_pixel_survival VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_mixer REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(glew REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_executable(dead_pixel_survival
    src/main.cpp
    src/core/Game.cpp
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
    src/systems/ProjectileSystem.cpp
    src/systems/WoundSystem.cpp
    src/systems/BuildSystem.cpp
    src/systems/SurvivorAISystem.cpp
    src/systems/SurvivorInteractionSystem.cpp
    src/systems/WeatherSystem.cpp
    src/systems/DistrictInfectionSystem.cpp
    src/items/ItemDatabase.cpp
    src/rendering/Camera.cpp
    src/rendering/Shader.cpp
    src/rendering/Texture.cpp
    src/rendering/SpriteBatch.cpp
    src/rendering/BitmapFont.cpp
    src/rendering/SpriteSheet.cpp
    src/systems/AnimationSystem.cpp
    src/systems/ParticleSystem.cpp
    src/audio/AudioManager.cpp
)

target_include_directories(dead_pixel_survival PRIVATE src)

if(TARGET SDL2::SDL2main)
    target_link_libraries(dead_pixel_survival PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(dead_pixel_survival PRIVATE
    SDL2::SDL2
    SDL2_mixer::SDL2_mixer
    OpenGL::GL
    GLEW::GLEW
    glm::glm
)

add_custom_command(TARGET dead_pixel_survival POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:dead_pixel_survival>/assets
)

if(MSVC)
    target_compile_options(dead_pixel_survival PRIVATE /W4 /permissive-)
else()
    target_compile_options(dead_pixel_survival PRIVATE -Wall -Wextra -Wpedantic)
endif()
