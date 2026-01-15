#include "systems/movement_system.hpp"
#include "components/components.hpp"
#include "core/constants.hpp"
#include <cmath>

namespace fob {

namespace {

/// Get the base movement speed for a unit type.
float getBaseSpeed(UnitType::Type type) {
    switch (type) {
        case UnitType::LightInfantry: return LIGHT_INFANTRY_SPEED;
        case UnitType::Cavalry:       return CAVALRY_SPEED;
        case UnitType::HeavyInfantry:
        default:                      return HEAVY_INFANTRY_SPEED;
    }
}

/// Calculate distance between two points.
float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

/// Normalize a vector, returning zero vector if length is too small.
Vec2 normalize(Vec2 v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < 0.0001f) return Vec2(0.0f, 0.0f);
    return Vec2(v.x / len, v.y / len);
}

} // anonymous namespace

void MovementSystem::update(entt::registry& registry, const SpatialHash& spatialHash, float dt) {
    // Process routing units first (they flee from enemies)
    auto routingView = registry.view<Position, Velocity, UnitType, Routing>(entt::exclude<Dead, InCombat>);
    for (auto entity : routingView) {
        const auto& unitType = routingView.get<UnitType>(entity);
        float speed = getBaseSpeed(unitType.type) * 1.5f;  // Routing units run faster (panic!)
        fleeFromEnemies(registry, entity, spatialHash, speed, dt);
    }

    // Process normal units with movement targets
    auto normalView = registry.view<Position, Velocity, MovementTarget, Stats, UnitType>(
        entt::exclude<Dead, InCombat, Routing>);
    for (auto entity : normalView) {
        const auto& target = normalView.get<MovementTarget>(entity);
        if (!target.hasTarget) continue;

        const auto& unitType = normalView.get<UnitType>(entity);
        float speed = getBaseSpeed(unitType.type);
        moveTowardTarget(registry, entity, speed, dt);
    }
}

void MovementSystem::moveTowardTarget(entt::registry& registry, entt::entity entity,
                                       float speed, float dt) {
    auto& pos = registry.get<Position>(entity);
    auto& vel = registry.get<Velocity>(entity);
    const auto& target = registry.get<MovementTarget>(entity);

    float dist = distance(pos.x, pos.y, target.position.x, target.position.y);

    // Stop if we're close enough to target
    if (dist < MELEE_RANGE) {
        vel.dx = 0.0f;
        vel.dy = 0.0f;
        return;
    }

    // Calculate direction to target
    Vec2 dir = normalize(Vec2(target.position.x - pos.x, target.position.y - pos.y));

    // Set velocity
    vel.dx = dir.x * speed;
    vel.dy = dir.y * speed;

    // Update position
    pos.x += vel.dx * dt;
    pos.y += vel.dy * dt;
}

void MovementSystem::fleeFromEnemies(entt::registry& registry, entt::entity entity,
                                      const SpatialHash& spatialHash, float speed, float dt) {
    auto& pos = registry.get<Position>(entity);
    auto& vel = registry.get<Velocity>(entity);
    const auto& team = registry.get<Team>(entity);

    // Find nearby units to flee from
    spatialHash.queryRadius(pos.x, pos.y, MORALE_EFFECT_RADIUS, m_nearbyBuffer);

    // Calculate average direction away from enemies
    Vec2 fleeDir(0.0f, 0.0f);
    int enemyCount = 0;

    for (auto other : m_nearbyBuffer) {
        if (other == entity) continue;
        if (!registry.valid(other)) continue;
        if (registry.all_of<Dead>(other)) continue;

        const auto* otherTeam = registry.try_get<Team>(other);
        if (!otherTeam || otherTeam->value == team.value) continue;  // Same team, ignore

        const auto& otherPos = registry.get<Position>(other);
        float dist = distance(pos.x, pos.y, otherPos.x, otherPos.y);
        if (dist < 0.1f) continue;  // Too close, avoid division issues

        // Weight by inverse distance (closer enemies are scarier)
        float weight = 1.0f / dist;
        fleeDir.x += (pos.x - otherPos.x) * weight;
        fleeDir.y += (pos.y - otherPos.y) * weight;
        enemyCount++;
    }

    // If no enemies nearby, flee "backward" (positive Y for Red, negative for Blue)
    if (enemyCount == 0) {
        fleeDir = (team.value == Team::Red) ? Vec2(0.0f, -1.0f) : Vec2(0.0f, 1.0f);
    }

    // Normalize and apply
    Vec2 dir = normalize(fleeDir);
    vel.dx = dir.x * speed;
    vel.dy = dir.y * speed;

    pos.x += vel.dx * dt;
    pos.y += vel.dy * dt;
}

} // namespace fob
