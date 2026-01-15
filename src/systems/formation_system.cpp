#include "systems/formation_system.hpp"
#include "components/components.hpp"
#include "core/constants.hpp"
#include <cmath>

namespace fob {

namespace {

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

} // anonymous namespace

void FormationSystem::update(entt::registry& registry, const SpatialHash& spatialHash, float dt) {
    auto formationView = registry.view<Position, Formation>();

    for (auto entity : formationView) {
        auto& pos = formationView.get<Position>(entity);
        auto& formation = formationView.get<Formation>(entity);

        switch (formation.state) {
            case FormationState::Advancing: {
                // Check if we've made contact with the enemy
                if (checkEnemyContact(registry, spatialHash, entity)) {
                    formation.state = FormationState::Engaged;
                    break;
                }

                // Move formation toward target
                float dist = distance(pos.x, pos.y, formation.targetPosition.x, formation.targetPosition.y);
                if (dist > 1.0f) {
                    Vec2 dir = normalize(Vec2(formation.targetPosition.x - pos.x,
                                              formation.targetPosition.y - pos.y));
                    pos.x += dir.x * formation.speed * dt;
                    pos.y += dir.y * formation.speed * dt;
                }
                break;
            }

            case FormationState::Engaged:
                // Formation holds position - soldiers handle their own micro-movement
                // Could add logic here to detect if enemies have retreated
                break;

            case FormationState::Withdrawing:
                // TODO: Pull back from enemy
                break;

            case FormationState::Broken:
                // Formation no longer functions - soldiers act independently
                break;
        }
    }
}

bool FormationSystem::checkEnemyContact(entt::registry& registry, const SpatialHash& spatialHash,
                                         entt::entity formationEntity) {
    const auto& formation = registry.get<Formation>(formationEntity);

    // Get the team of this formation by checking one of its members
    Team::Value formationTeam = Team::Red;  // default
    bool foundTeam = false;

    // Find front-line soldiers (rank 0) in this formation
    auto memberView = registry.view<Position, FormationMember, Team>(entt::exclude<Dead, Routing>);

    for (auto soldier : memberView) {
        const auto& member = memberView.get<FormationMember>(soldier);
        if (member.formation != formationEntity) continue;

        if (!foundTeam) {
            formationTeam = memberView.get<Team>(soldier).value;
            foundTeam = true;
        }

        // Only check front-line soldiers
        if (member.rank != formation.frontRank) continue;

        const auto& soldierPos = memberView.get<Position>(soldier);

        // Check for nearby enemies
        spatialHash.queryRadius(soldierPos.x, soldierPos.y, ENEMY_STOP_RADIUS, m_nearbyBuffer);

        for (auto other : m_nearbyBuffer) {
            if (other == soldier) continue;
            if (!registry.valid(other)) continue;
            if (registry.all_of<Dead>(other)) continue;

            const auto* otherTeam = registry.try_get<Team>(other);
            if (!otherTeam || otherTeam->value == formationTeam) continue;

            // Found an enemy near a front-line soldier
            return true;
        }
    }

    return false;
}

} // namespace fob
