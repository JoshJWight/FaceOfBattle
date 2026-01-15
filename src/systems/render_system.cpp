#include "systems/render_system.hpp"
#include "components/components.hpp"
#include "core/constants.hpp"
#include <algorithm>
#include <cmath>

namespace fob {

RenderSystem::RenderSystem(SDL_Renderer* renderer, int width, int height)
    : m_renderer(renderer), m_width(width), m_height(height) {}

void RenderSystem::render(entt::registry& registry) {
    // Clear screen with dark background
    SDL_SetRenderDrawColor(m_renderer, 20, 20, 30, 255);
    SDL_RenderClear(m_renderer);

    // Unit size scales with zoom but stays smaller than formation spacing
    // to prevent overlap. At formation spacing of 2.5, we use ~80% of that.
    float spacingPx = FORMATION_SPACING * m_camera.zoom;
    float baseSize = spacingPx * 0.7f;
    baseSize = std::clamp(baseSize, 1.0f, 20.0f);

    // Render all living units
    auto view = registry.view<Position, Team>(entt::exclude<Dead>);

    for (auto entity : view) {
        const auto& pos = view.get<Position>(entity);
        const auto& team = view.get<Team>(entity);

        Vec2 screenPos = m_camera.worldToScreen(pos.toVec2(), m_width, m_height);

        // Skip if off-screen
        if (screenPos.x < -baseSize || screenPos.x > m_width + baseSize ||
            screenPos.y < -baseSize || screenPos.y > m_height + baseSize) {
            continue;
        }

        // Color based on team and state
        uint8_t r, g, b;
        if (team.value == Team::Red) {
            r = 220; g = 60; b = 60;
        } else {
            r = 60; g = 100; b = 220;
        }

        // Modify color based on state
        if (registry.all_of<Routing>(entity)) {
            // Routing units are darker/desaturated
            r = r / 2;
            g = g / 2;
            b = b / 2;
        } else if (registry.all_of<InCombat>(entity)) {
            // Units in combat are brighter
            r = std::min(255, r + 30);
            g = std::min(255, g + 30);
            b = std::min(255, b + 30);
        }

        // Officers are rendered slightly larger
        float size = baseSize;
        if (registry.all_of<Officer>(entity)) {
            size *= 1.5f;
        }

        // Morale affects brightness
        if (auto* morale = registry.try_get<Morale>(entity)) {
            float factor = 0.5f + 0.5f * morale->value;
            r = static_cast<uint8_t>(r * factor);
            g = static_cast<uint8_t>(g * factor);
            b = static_cast<uint8_t>(b * factor);
        }

        // Flash effects override color
        if (auto* flash = registry.try_get<FlashEffect>(entity)) {
            if (flash->isActive()) {
                if (flash->type == FlashEffect::Attack) {
                    // White flash for attacking
                    r = 255; g = 255; b = 255;
                } else if (flash->type == FlashEffect::Hit) {
                    // Yellow flash for getting hit
                    r = 255; g = 255; b = 0;
                }
            }
        }

        // Formation markers are circles, soldiers are squares
        if (registry.all_of<Formation>(entity)) {
            drawCircle(screenPos.x, screenPos.y, r, g, b, size * 0.6f);
        } else {
            drawUnit(screenPos.x, screenPos.y, r, g, b, size);
        }
    }

    // Render dead units as gray (optional, could be toggled)
    auto deadView = registry.view<Position, Dead>();
    SDL_SetRenderDrawColor(m_renderer, 50, 50, 50, 255);
    for (auto entity : deadView) {
        const auto& pos = deadView.get<Position>(entity);
        Vec2 screenPos = m_camera.worldToScreen(pos.toVec2(), m_width, m_height);

        if (screenPos.x < 0 || screenPos.x > m_width ||
            screenPos.y < 0 || screenPos.y > m_height) {
            continue;
        }

        SDL_Rect rect{
            static_cast<int>(screenPos.x - 1),
            static_cast<int>(screenPos.y - 1),
            2, 2
        };
        SDL_RenderFillRect(m_renderer, &rect);
    }
}

void RenderSystem::drawUnit(float screenX, float screenY, uint8_t r, uint8_t g, uint8_t b, float size) {
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);

    int halfSize = static_cast<int>(size * 0.5f);
    int x = static_cast<int>(screenX);
    int y = static_cast<int>(screenY);

    SDL_Rect rect{x - halfSize, y - halfSize, static_cast<int>(size), static_cast<int>(size)};
    SDL_RenderFillRect(m_renderer, &rect);
}

void RenderSystem::drawCircle(float screenX, float screenY, uint8_t r, uint8_t g, uint8_t b, float radius) {
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);

    int cx = static_cast<int>(screenX);
    int cy = static_cast<int>(screenY);
    int rad = static_cast<int>(radius);

    // Midpoint circle algorithm for filled circle
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = static_cast<int>(std::sqrt(rad * rad - dy * dy));
        SDL_RenderDrawLine(m_renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

} // namespace fob
