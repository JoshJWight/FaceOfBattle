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
#include <cstring>

using namespace fob;

/// Spawn a formation of soldiers.
entt::entity spawnFormation(entt::registry& registry, Team::Value team,
                            Vec2 center, int rows, int cols, float spacing,
                            Vec2 targetPos, Vec2 facing) {
    auto formationEntity = registry.create();
    registry.emplace<Position>(formationEntity, center);
    registry.emplace<Formation>(formationEntity, targetPos, facing, HEAVY_INFANTRY_SPEED);
    registry.emplace<Team>(formationEntity, team);

    for (int rank = 0; rank < rows; ++rank) {
        for (int file = 0; file < cols; ++file) {
            auto soldier = registry.create();

            float localX = (file - (cols - 1) * 0.5f) * spacing;
            float localY = -rank * spacing;

            Vec2 localOffset(localX, localY);

            float worldX = center.x + localX;
            float worldY = center.y + localY * facing.y;

            registry.emplace<Position>(soldier, worldX, worldY);
            registry.emplace<Velocity>(soldier, 0.0f, 0.0f);
            registry.emplace<Team>(soldier, team);
            registry.emplace<Stats>(soldier, 100.0f, 100.0f, 10.0f, 5.0f, HEAVY_INFANTRY_SPEED);
            registry.emplace<Morale>(soldier, 1.0f, 0.0f);
            registry.emplace<UnitType>(soldier, UnitType::HeavyInfantry);
            registry.emplace<FormationMember>(soldier, formationEntity, localOffset, rank, file);

            if (file == cols / 2 && rank % 3 == 0) {
                registry.emplace<Officer>(soldier, 1);
            }
        }
    }

    return formationEntity;
}

void runHeadless(int maxTicks) {
    std::cout << "Running headless simulation for " << maxTicks << " ticks..." << std::endl;

    entt::registry registry;
    FormationSystem formationSystem;
    MovementSystem movementSystem;
    CombatSystem combatSystem;
    SpatialHash spatialHash;

    // Spawn armies
    spawnFormation(registry, Team::Red, Vec2(0.0f, -30.0f), 10, 50, FORMATION_SPACING,
                   Vec2(0.0f, 30.0f), Vec2(0.0f, 1.0f));
    spawnFormation(registry, Team::Blue, Vec2(0.0f, 30.0f), 10, 50, FORMATION_SPACING,
                   Vec2(0.0f, -30.0f), Vec2(0.0f, -1.0f));

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int tick = 0; tick < maxTicks; ++tick) {
        // Rebuild spatial hash
        spatialHash.clear();
        auto posView = registry.view<Position>(entt::exclude<Dead, Formation>);
        for (auto entity : posView) {
            const auto& pos = posView.get<Position>(entity);
            spatialHash.insert(entity, pos.x, pos.y);
        }

        // Run systems
        formationSystem.update(registry, spatialHash, FIXED_TIMESTEP);
        movementSystem.update(registry, spatialHash, FIXED_TIMESTEP);
        combatSystem.update(registry, spatialHash, FIXED_TIMESTEP);

        // Print stats every simulated second (60 ticks)
        if (tick % 60 == 0) {
            int redAlive = 0, blueAlive = 0, dead = 0;
            auto statsView = registry.view<Team, Stats>();
            for (auto entity : statsView) {
                if (registry.all_of<Dead>(entity)) {
                    dead++;
                } else if (statsView.get<Team>(entity).value == Team::Red) {
                    redAlive++;
                } else {
                    blueAlive++;
                }
            }
            float simTime = tick * FIXED_TIMESTEP;
            std::cout << "t=" << simTime << "s: Red=" << redAlive
                      << " Blue=" << blueAlive << " Dead=" << dead << std::endl;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    float simSeconds = maxTicks * FIXED_TIMESTEP;

    std::cout << "\nSimulated " << simSeconds << "s in " << elapsedMs << "ms ("
              << (simSeconds / (elapsedMs / 1000.0f)) << "x realtime)" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check for headless mode
    bool headless = false;
    int headlessTicks = 6000;  // Default: 100 seconds of simulation

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            headlessTicks = std::atoi(argv[++i]);
        }
    }

    if (headless) {
        runHeadless(headlessTicks);
        return 0;
    }

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
    std::cout << "Spawning armies..." << std::endl;
    spawnFormation(registry, Team::Red, Vec2(0.0f, -30.0f), 10, 50, FORMATION_SPACING,
                   Vec2(0.0f, 30.0f), Vec2(0.0f, 1.0f));
    spawnFormation(registry, Team::Blue, Vec2(0.0f, 30.0f), 10, 50, FORMATION_SPACING,
                   Vec2(0.0f, -30.0f), Vec2(0.0f, -1.0f));

    // Center camera on battlefield
    renderSystem.camera().position = Vec2(0.0f, 0.0f);
    renderSystem.camera().zoom = 2.0f;

    // Main loop
    bool running = true;
    auto lastTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;
    int frameCount = 0;
    float fpsTimer = 0.0f;

    while (running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        if (dt > 0.25f) dt = 0.25f;

        accumulator += dt;
        fpsTimer += dt;
        frameCount++;

        if (fpsTimer >= 1.0f) {
            std::cout << "FPS: " << frameCount << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

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

        while (accumulator >= FIXED_TIMESTEP) {
            spatialHash.clear();
            auto posView = registry.view<Position>(entt::exclude<Dead, Formation>);
            for (auto entity : posView) {
                const auto& pos = posView.get<Position>(entity);
                spatialHash.insert(entity, pos.x, pos.y);
            }

            formationSystem.update(registry, spatialHash, FIXED_TIMESTEP);
            movementSystem.update(registry, spatialHash, FIXED_TIMESTEP);
            combatSystem.update(registry, spatialHash, FIXED_TIMESTEP);

            accumulator -= FIXED_TIMESTEP;
        }

        renderSystem.render(registry);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
