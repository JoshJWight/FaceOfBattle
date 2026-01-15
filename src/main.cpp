#include "core/types.hpp"
#include "core/constants.hpp"
#include "components/components.hpp"
#include "systems/render_system.hpp"
#include "systems/movement_system.hpp"
#include "systems/formation_system.hpp"
#include "systems/combat_system.hpp"
#include "simulation/spatial_hash.hpp"

#include <entt/entt.hpp>
#include <SDL2/SDL.h>

#include <iostream>
#include <chrono>

using namespace fob;

/// Spawn a formation of soldiers.
/// Creates a formation entity and populates it with soldiers.
/// @param registry The ECS registry
/// @param team Which team this formation belongs to
/// @param center Starting center position of the formation
/// @param rows Number of ranks (depth)
/// @param cols Number of files (width)
/// @param spacing Distance between soldiers
/// @param targetPos Where the formation should advance toward
/// @param facing Direction the formation faces (should point toward enemy)
/// @return The formation entity
entt::entity spawnFormation(entt::registry& registry, Team::Value team,
                            Vec2 center, int rows, int cols, float spacing,
                            Vec2 targetPos, Vec2 facing) {
    // Create the formation entity
    auto formationEntity = registry.create();
    registry.emplace<Position>(formationEntity, center);
    registry.emplace<Formation>(formationEntity, targetPos, facing, HEAVY_INFANTRY_SPEED);
    registry.emplace<Team>(formationEntity, team);

    // Spawn soldiers in the formation
    // Rank 0 is the front line, facing the enemy
    // Local coordinates: X = left/right, Y = forward/back relative to facing
    for (int rank = 0; rank < rows; ++rank) {
        for (int file = 0; file < cols; ++file) {
            auto soldier = registry.create();

            // Calculate local offset from formation center
            // Files spread left-right (X), ranks go back from front (Y)
            float localX = (file - (cols - 1) * 0.5f) * spacing;
            float localY = -rank * spacing;  // Negative because rank 0 is front

            Vec2 localOffset(localX, localY);

            // Calculate world position
            // For axis-aligned facing: X is always left/right, Y flips based on facing
            float worldX = center.x + localX;
            float worldY = center.y + localY * facing.y;

            registry.emplace<Position>(soldier, worldX, worldY);
            registry.emplace<Velocity>(soldier, 0.0f, 0.0f);
            registry.emplace<Team>(soldier, team);
            registry.emplace<Stats>(soldier, 100.0f, 100.0f, 10.0f, 5.0f, HEAVY_INFANTRY_SPEED);
            registry.emplace<Morale>(soldier, 1.0f, 0.0f);
            registry.emplace<UnitType>(soldier, UnitType::HeavyInfantry);
            registry.emplace<FormationMember>(soldier, formationEntity, localOffset, rank, file);

            // Make some units officers (center of each rank)
            if (file == cols / 2 && rank % 3 == 0) {
                registry.emplace<Officer>(soldier, 1);
            }
        }
    }

    return formationEntity;
}

int main(int /*argc*/, char* /*argv*/[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Face of Battle",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create ECS registry and systems
    entt::registry registry;
    RenderSystem renderSystem(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
    FormationSystem formationSystem;
    MovementSystem movementSystem;
    CombatSystem combatSystem;
    SpatialHash spatialHash;

    // Spawn two opposing armies
    // Each army: 10 rows x 50 cols = 500 units
    // Total: 1,000 units + 2 formation entities
    std::cout << "Spawning armies..." << std::endl;
    auto spawnStart = std::chrono::high_resolution_clock::now();

    // Red army at bottom, facing up (toward Blue)
    spawnFormation(registry, Team::Red, Vec2(0.0f, -30.0f), 10, 50, FORMATION_SPACING,
                   Vec2(0.0f, 30.0f), Vec2(0.0f, 1.0f));

    // Blue army at top, facing down (toward Red)
    spawnFormation(registry, Team::Blue, Vec2(0.0f, 30.0f), 10, 50, FORMATION_SPACING,
                   Vec2(0.0f, -30.0f), Vec2(0.0f, -1.0f));

    auto spawnEnd = std::chrono::high_resolution_clock::now();
    auto spawnMs = std::chrono::duration_cast<std::chrono::milliseconds>(spawnEnd - spawnStart).count();
    auto& storage = registry.storage<entt::entity>();
    std::cout << "Spawned " << (storage.size() - storage.free_list())
              << " entities in " << spawnMs << "ms" << std::endl;

    // Center camera on battlefield
    renderSystem.camera().position = Vec2(0.0f, 0.0f);
    renderSystem.camera().zoom = 2.0f;

    // Main loop
    bool running = true;
    auto lastTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;

    // For FPS display
    int frameCount = 0;
    float fpsTimer = 0.0f;

    while (running) {
        // Calculate delta time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Cap delta time to avoid spiral of death
        if (dt > 0.25f) dt = 0.25f;

        accumulator += dt;
        fpsTimer += dt;
        frameCount++;

        // FPS display
        if (fpsTimer >= 1.0f) {
            std::cout << "FPS: " << frameCount << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // Event handling
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_PLUS:
                    case SDLK_EQUALS:
                        renderSystem.camera().zoom *= 1.2f;
                        if (renderSystem.camera().zoom > MAX_ZOOM)
                            renderSystem.camera().zoom = MAX_ZOOM;
                        break;
                    case SDLK_MINUS:
                        renderSystem.camera().zoom /= 1.2f;
                        if (renderSystem.camera().zoom < MIN_ZOOM)
                            renderSystem.camera().zoom = MIN_ZOOM;
                        break;
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                if (event.wheel.y > 0) {
                    renderSystem.camera().zoom *= 1.1f;
                    if (renderSystem.camera().zoom > MAX_ZOOM)
                        renderSystem.camera().zoom = MAX_ZOOM;
                } else if (event.wheel.y < 0) {
                    renderSystem.camera().zoom /= 1.1f;
                    if (renderSystem.camera().zoom < MIN_ZOOM)
                        renderSystem.camera().zoom = MIN_ZOOM;
                }
            }
        }

        // Camera panning with arrow keys
        const uint8_t* keys = SDL_GetKeyboardState(nullptr);
        float panSpeed = 500.0f / renderSystem.camera().zoom;
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
            renderSystem.camera().position.x -= panSpeed * dt;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            renderSystem.camera().position.x += panSpeed * dt;
        }
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
            renderSystem.camera().position.y += panSpeed * dt;
        }
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
            renderSystem.camera().position.y -= panSpeed * dt;
        }

        // Fixed timestep simulation updates
        while (accumulator >= FIXED_TIMESTEP) {
            // Rebuild spatial hash (needed by systems for queries)
            spatialHash.clear();
            auto posView = registry.view<Position>(entt::exclude<Dead, Formation>);
            for (auto entity : posView) {
                const auto& pos = posView.get<Position>(entity);
                spatialHash.insert(entity, pos.x, pos.y);
            }

            // Run simulation systems in order
            formationSystem.update(registry, spatialHash, FIXED_TIMESTEP);
            movementSystem.update(registry, spatialHash, FIXED_TIMESTEP);
            combatSystem.update(registry, spatialHash, FIXED_TIMESTEP);
            // TODO: moraleSystem.update(registry, FIXED_TIMESTEP);

            accumulator -= FIXED_TIMESTEP;
        }

        // Render
        renderSystem.render(registry);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
