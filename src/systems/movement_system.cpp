#include "systems/movement_system.hpp"
#include "components/components.hpp"
#include "core/constants.hpp"
#include <cmath>

namespace fob {

namespace {

float getBaseSpeed(UnitType::Type type) {
    switch (type) {
        case UnitType::LightInfantry: return LIGHT_INFANTRY_SPEED;
        case UnitType::Cavalry:       return CAVALRY_SPEED;
        case UnitType::HeavyInfantry:
        default:                      return HEAVY_INFANTRY_SPEED;
    }
}

float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

Vec2 normalize(Vec2 v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < 0.0001f) return Vec2(0.0f, 0.0f);
    return Vec2(v.x / len, v.y / len);
}

Vec2 clampMagnitude(Vec2 v, float maxLen) {
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len <= maxLen || len < 0.0001f) return v;
    float scale = maxLen / len;
    return Vec2(v.x * scale, v.y * scale);
}

} // anonymous namespace

void MovementSystem::update(entt::registry& registry, const SpatialHash& spatialHash, float dt) {
    // Process routing units first (they flee from enemies, ignore formation)
    auto routingView = registry.view<Position, Velocity, UnitType, Routing>(entt::exclude<Dead, InCombat>);
    for (auto entity : routingView) {
        const auto& unitType = routingView.get<UnitType>(entity);
        float speed = getBaseSpeed(unitType.type) * 1.5f;
        fleeFromEnemies(registry, entity, spatialHash, speed, dt);
    }

    // Process formation members
    auto formationMemberView = registry.view<Position, Velocity, FormationMember, Team, UnitType>(
        entt::exclude<Dead, InCombat, Routing>);

    for (auto entity : formationMemberView) {
        const auto& member = formationMemberView.get<FormationMember>(entity);
        const auto& unitType = formationMemberView.get<UnitType>(entity);
        float speed = getBaseSpeed(unitType.type);

        // Get formation state
        if (!registry.valid(member.formation)) continue;
        const auto* formation = registry.try_get<Formation>(member.formation);
        const auto* formationPos = registry.try_get<Position>(member.formation);
        if (!formation || !formationPos) continue;

        moveFormationMember(registry, entity, spatialHash, *formation, *formationPos, speed, dt);
    }

    // Process units with MovementTarget but no formation (free units)
    auto freeUnitView = registry.view<Position, Velocity, MovementTarget, Team, UnitType>(
        entt::exclude<Dead, InCombat, Routing, FormationMember>);

    for (auto entity : freeUnitView) {
        const auto& target = freeUnitView.get<MovementTarget>(entity);
        if (!target.hasTarget) continue;

        const auto& unitType = freeUnitView.get<UnitType>(entity);
        float speed = getBaseSpeed(unitType.type);
        moveFreeUnit(registry, entity, spatialHash, speed, dt);
    }
}

void MovementSystem::moveFormationMember(entt::registry& registry, entt::entity entity,
                                          const SpatialHash& spatialHash,
                                          const Formation& formation, const Position& formationPos,
                                          float speed, float dt) {
    auto& pos = registry.get<Position>(entity);
    auto& vel = registry.get<Velocity>(entity);
    const auto& member = registry.get<FormationMember>(entity);
    const auto& team = registry.get<Team>(entity);

    // Calculate target position in world space
    // Local offset is relative to formation center, rotated by facing
    Vec2 targetWorld;
    targetWorld.x = formationPos.x + member.localOffset.x;
    targetWorld.y = formationPos.y + member.localOffset.y * formation.facing.y;

    // Query nearby units for collision
    float queryRadius = std::max(ENEMY_STOP_RADIUS, ALLY_SEPARATION_RADIUS);
    spatialHash.queryRadius(pos.x, pos.y, queryRadius, m_nearbyBuffer);

    // Calculate forces from nearby units
    Vec2 enemyRepulsion(0.0f, 0.0f);
    Vec2 allyRepulsion(0.0f, 0.0f);
    bool enemyContact = false;

    for (auto other : m_nearbyBuffer) {
        if (other == entity) continue;
        if (!registry.valid(other)) continue;
        if (registry.all_of<Dead>(other)) continue;

        const auto* otherTeam = registry.try_get<Team>(other);
        if (!otherTeam) continue;

        const auto& otherPos = registry.get<Position>(other);
        float dist = distance(pos.x, pos.y, otherPos.x, otherPos.y);
        if (dist < 0.01f) continue;

        Vec2 away((pos.x - otherPos.x) / dist, (pos.y - otherPos.y) / dist);

        if (otherTeam->value != team.value) {
            // Enemy
            if (dist < ENEMY_STOP_RADIUS) {
                enemyContact = true;
                float strength = (ENEMY_STOP_RADIUS - dist) / ENEMY_STOP_RADIUS;
                enemyRepulsion.x += away.x * strength * 2.0f;
                enemyRepulsion.y += away.y * strength * 2.0f;
            }
        } else {
            // Ally
            if (dist < ALLY_SEPARATION_RADIUS) {
                float strength = (ALLY_SEPARATION_RADIUS - dist) / ALLY_SEPARATION_RADIUS;
                allyRepulsion.x += away.x * strength;
                allyRepulsion.y += away.y * strength;
            }
        }
    }

    // Build movement vector based on formation state
    Vec2 movement(0.0f, 0.0f);

    if (formation.state == FormationState::Advancing && !enemyContact) {
        // Move toward formation position
        float distToTarget = distance(pos.x, pos.y, targetWorld.x, targetWorld.y);
        if (distToTarget > 0.5f) {
            Vec2 toTarget = normalize(Vec2(targetWorld.x - pos.x, targetWorld.y - pos.y));
            // Move faster if far from position, slower if close
            float urgency = std::min(distToTarget / FORMATION_SPACING, 1.0f);
            movement.x += toTarget.x * speed * urgency;
            movement.y += toTarget.y * speed * urgency;
        }
    } else if (formation.state == FormationState::Engaged || enemyContact) {
        // Check if there's a gap in front to fill (replacement behavior)
        bool allyInFront = false;
        Vec2 frontCheckPos(pos.x, pos.y + formation.facing.y * FORMATION_SPACING);

        // Query for allies directly in front of us (same file, one rank ahead)
        // Use larger radius to account for spatial hash cell boundaries
        float queryRadius = FORMATION_SPACING * 1.5f;
        spatialHash.queryRadius(frontCheckPos.x, frontCheckPos.y, queryRadius, m_nearbyBuffer);

        for (auto other : m_nearbyBuffer) {
            if (other == entity) continue;
            if (!registry.valid(other)) continue;
            if (registry.all_of<Dead>(other)) continue;

            const auto* otherTeam = registry.try_get<Team>(other);
            if (!otherTeam || otherTeam->value != team.value) continue;

            const auto& otherPos = registry.get<Position>(other);

            // Check actual distance to frontCheckPos (spatial hash returns all in cells)
            float dx = otherPos.x - frontCheckPos.x;
            float dy = otherPos.y - frontCheckPos.y;
            float distSq = dx * dx + dy * dy;
            float checkRadius = FORMATION_SPACING * 0.7f;  // Slightly larger than half spacing

            if (distSq < checkRadius * checkRadius) {
                allyInFront = true;
                break;
            }
        }

        if (!allyInFront) {
            // No ally in front - advance to fill the gap
            // Enemy repulsion will still prevent walking directly into enemies
            movement.x += formation.facing.x * speed * 0.5f;
            movement.y += formation.facing.y * speed * 0.5f;
        }

        if (enemyContact || allyInFront) {
            // Hold position - only apply minor drift toward formation spot
            float distToTarget = distance(pos.x, pos.y, targetWorld.x, targetWorld.y);
            if (distToTarget > FORMATION_SPACING * 0.5f) {
                Vec2 toTarget = normalize(Vec2(targetWorld.x - pos.x, targetWorld.y - pos.y));
                movement.x += toTarget.x * speed * 0.3f;  // Gentle drift
                movement.y += toTarget.y * speed * 0.3f;
            }
        }
    }

    // Apply enemy repulsion (highest priority)
    enemyRepulsion = normalize(enemyRepulsion);
    movement.x += enemyRepulsion.x * speed * 1.5f;
    movement.y += enemyRepulsion.y * speed * 1.5f;

    // Apply ally separation
    allyRepulsion = normalize(allyRepulsion);
    movement.x += allyRepulsion.x * ALLY_SEPARATION_STRENGTH;
    movement.y += allyRepulsion.y * ALLY_SEPARATION_STRENGTH;

    // Clamp and apply
    movement = clampMagnitude(movement, speed);
    vel.dx = movement.x;
    vel.dy = movement.y;

    pos.x += vel.dx * dt;
    pos.y += vel.dy * dt;
}

void MovementSystem::moveFreeUnit(entt::registry& registry, entt::entity entity,
                                   const SpatialHash& spatialHash, float speed, float dt) {
    auto& pos = registry.get<Position>(entity);
    auto& vel = registry.get<Velocity>(entity);
    const auto& target = registry.get<MovementTarget>(entity);
    const auto& team = registry.get<Team>(entity);

    // Query nearby units
    float queryRadius = std::max(ENEMY_STOP_RADIUS, ALLY_SEPARATION_RADIUS);
    spatialHash.queryRadius(pos.x, pos.y, queryRadius, m_nearbyBuffer);

    Vec2 enemyRepulsion(0.0f, 0.0f);
    Vec2 allyRepulsion(0.0f, 0.0f);
    bool enemyInRange = false;

    for (auto other : m_nearbyBuffer) {
        if (other == entity) continue;
        if (!registry.valid(other)) continue;
        if (registry.all_of<Dead>(other)) continue;

        const auto* otherTeam = registry.try_get<Team>(other);
        if (!otherTeam) continue;

        const auto& otherPos = registry.get<Position>(other);
        float dist = distance(pos.x, pos.y, otherPos.x, otherPos.y);
        if (dist < 0.01f) continue;

        Vec2 away((pos.x - otherPos.x) / dist, (pos.y - otherPos.y) / dist);

        if (otherTeam->value != team.value) {
            if (dist < ENEMY_STOP_RADIUS) {
                enemyInRange = true;
                float strength = (ENEMY_STOP_RADIUS - dist) / ENEMY_STOP_RADIUS;
                enemyRepulsion.x += away.x * strength * 2.0f;
                enemyRepulsion.y += away.y * strength * 2.0f;
            }
        } else {
            if (dist < ALLY_SEPARATION_RADIUS) {
                float strength = (ALLY_SEPARATION_RADIUS - dist) / ALLY_SEPARATION_RADIUS;
                allyRepulsion.x += away.x * strength;
                allyRepulsion.y += away.y * strength;
            }
        }
    }

    Vec2 movement(0.0f, 0.0f);

    if (!enemyInRange) {
        float distToTarget = distance(pos.x, pos.y, target.position.x, target.position.y);
        if (distToTarget > MELEE_RANGE) {
            Vec2 toTarget = normalize(Vec2(target.position.x - pos.x, target.position.y - pos.y));
            movement.x += toTarget.x * speed;
            movement.y += toTarget.y * speed;
        }
    }

    enemyRepulsion = normalize(enemyRepulsion);
    movement.x += enemyRepulsion.x * speed * 1.5f;
    movement.y += enemyRepulsion.y * speed * 1.5f;

    allyRepulsion = normalize(allyRepulsion);
    movement.x += allyRepulsion.x * ALLY_SEPARATION_STRENGTH;
    movement.y += allyRepulsion.y * ALLY_SEPARATION_STRENGTH;

    movement = clampMagnitude(movement, speed);
    vel.dx = movement.x;
    vel.dy = movement.y;

    pos.x += vel.dx * dt;
    pos.y += vel.dy * dt;
}

void MovementSystem::fleeFromEnemies(entt::registry& registry, entt::entity entity,
                                      const SpatialHash& spatialHash, float speed, float dt) {
    auto& pos = registry.get<Position>(entity);
    auto& vel = registry.get<Velocity>(entity);
    const auto& team = registry.get<Team>(entity);

    spatialHash.queryRadius(pos.x, pos.y, MORALE_EFFECT_RADIUS, m_nearbyBuffer);

    Vec2 fleeDir(0.0f, 0.0f);
    int enemyCount = 0;

    for (auto other : m_nearbyBuffer) {
        if (other == entity) continue;
        if (!registry.valid(other)) continue;
        if (registry.all_of<Dead>(other)) continue;

        const auto* otherTeam = registry.try_get<Team>(other);
        if (!otherTeam || otherTeam->value == team.value) continue;

        const auto& otherPos = registry.get<Position>(other);
        float dist = distance(pos.x, pos.y, otherPos.x, otherPos.y);
        if (dist < 0.1f) continue;

        float weight = 1.0f / dist;
        fleeDir.x += (pos.x - otherPos.x) * weight;
        fleeDir.y += (pos.y - otherPos.y) * weight;
        enemyCount++;
    }

    if (enemyCount == 0) {
        fleeDir = (team.value == Team::Red) ? Vec2(0.0f, -1.0f) : Vec2(0.0f, 1.0f);
    }

    Vec2 dir = normalize(fleeDir);
    vel.dx = dir.x * speed;
    vel.dy = dir.y * speed;

    pos.x += vel.dx * dt;
    pos.y += vel.dy * dt;
}

} // namespace fob
