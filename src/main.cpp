#include "core/types.hpp"
#include "core/constants.hpp"
#include "components/components.hpp"
#include "systems/render_system.hpp"
#include "simulation/spatial_hash.hpp"

#include <entt/entt.hpp>
#include <SDL2/SDL.h>

#include <iostream>
#include <random>
#include <chrono>

using namespace fob;

// Spawn a line of soldiers
void spawnFormation(entt::registry& registry, Team::Value team,
                    Vec2 center, int rows, int cols, float spacing) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.3f, 0.3f);

    float startX = center.x - (cols - 1) * spacing * 0.5f;
    float startY = center.y - (rows - 1) * spacing * 0.5f;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto entity = registry.create();

            float x = startX + col * spacing + jitter(gen);
            float y = startY + row * spacing + jitter(gen);

            registry.emplace<Position>(entity, x, y);
            registry.emplace<Velocity>(entity, 0.0f, 0.0f);
            registry.emplace<Team>(entity, team);
            registry.emplace<Stats>(entity, 100.0f, 100.0f, 10.0f, 5.0f, HEAVY_INFANTRY_SPEED);
            registry.emplace<Morale>(entity, 1.0f, 0.0f);
            registry.emplace<UnitType>(entity, UnitType::HeavyInfantry);

            // Make some units officers (every 50th unit)
            if ((row * cols + col) % 50 == 25) {
                registry.emplace<Officer>(entity, 1);
            }
        }
    }
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
    SpatialHash spatialHash;

    // Spawn two opposing armies
    // Each army: 100 rows x 500 cols = 50,000 units
    // Total: 100,000 units for testing
    std::cout << "Spawning armies..." << std::endl;
    auto spawnStart = std::chrono::high_resolution_clock::now();

    spawnFormation(registry, Team::Red, Vec2(0.0f, -150.0f), 100, 500, FORMATION_SPACING);
    spawnFormation(registry, Team::Blue, Vec2(0.0f, 150.0f), 100, 500, FORMATION_SPACING);

    auto spawnEnd = std::chrono::high_resolution_clock::now();
    auto spawnMs = std::chrono::duration_cast<std::chrono::milliseconds>(spawnEnd - spawnStart).count();
    auto& storage = registry.storage<entt::entity>();
    std::cout << "Spawned " << (storage.size() - storage.free_list())
              << " entities in " << spawnMs << "ms" << std::endl;

    // Center camera on battlefield
    renderSystem.camera().position = Vec2(0.0f, 0.0f);
    renderSystem.camera().zoom = 0.5f;

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
            // TODO: Run simulation systems here
            // commandSystem.update(registry, FIXED_TIMESTEP);
            // behaviorSystem.update(registry, FIXED_TIMESTEP);
            // formationSystem.update(registry, FIXED_TIMESTEP);
            // movementSystem.update(registry, FIXED_TIMESTEP);
            // combatSystem.update(registry, FIXED_TIMESTEP);
            // moraleSystem.update(registry, FIXED_TIMESTEP);
            // stateSystem.update(registry, FIXED_TIMESTEP);

            // Rebuild spatial hash
            spatialHash.clear();
            auto posView = registry.view<Position>(entt::exclude<Dead>);
            for (auto entity : posView) {
                const auto& pos = posView.get<Position>(entity);
                spatialHash.insert(entity, pos.x, pos.y);
            }

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
