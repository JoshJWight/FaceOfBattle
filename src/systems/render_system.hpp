#pragma once

#include "core/types.hpp"
#include <entt/entt.hpp>
#include <SDL2/SDL.h>

namespace fob {

struct Camera {
    Vec2 position = {0.0f, 0.0f};  // world position of camera center
    float zoom = 1.0f;

    // Convert world coords to screen coords
    Vec2 worldToScreen(Vec2 world, int screenWidth, int screenHeight) const {
        Vec2 relative = (world - position) * zoom;
        return Vec2(
            relative.x + screenWidth * 0.5f,
            -relative.y + screenHeight * 0.5f  // flip Y for screen coords
        );
    }

    // Convert screen coords to world coords
    Vec2 screenToWorld(Vec2 screen, int screenWidth, int screenHeight) const {
        Vec2 relative(
            screen.x - screenWidth * 0.5f,
            -(screen.y - screenHeight * 0.5f)
        );
        return relative / zoom + position;
    }
};

class RenderSystem {
public:
    RenderSystem(SDL_Renderer* renderer, int width, int height);

    void render(entt::registry& registry);

    Camera& camera() { return m_camera; }
    const Camera& camera() const { return m_camera; }

private:
    SDL_Renderer* m_renderer;
    int m_width;
    int m_height;
    Camera m_camera;

    void drawUnit(float screenX, float screenY, uint8_t r, uint8_t g, uint8_t b, float size);
};

} // namespace fob
