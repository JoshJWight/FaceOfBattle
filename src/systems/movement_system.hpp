#pragma once

#include "core/types.hpp"
#include "simulation/spatial_hash.hpp"
#include <entt/entt.hpp>

namespace fob {

/// Handles unit locomotion based on current state and targets.
///
/// Behavior varies by unit state:
/// - Normal units: Move toward MovementTarget at their speed
/// - InCombat units: Held in place, no movement
/// - Routing units: Flee away from nearest enemy at 1.5x speed
/// - Dead units: No movement
///
/// Speed is determined by UnitType (cavalry > light > heavy infantry).
/// Movement stops when within MELEE_RANGE of target.
class MovementSystem {
public:
    MovementSystem() = default;

    /// Update all unit positions for one simulation tick.
    /// @param registry The ECS registry
    /// @param spatialHash Spatial index for finding nearby enemies (used for routing)
    /// @param dt Delta time (should be FIXED_TIMESTEP)
    void update(entt::registry& registry, const SpatialHash& spatialHash, float dt);

private:
    /// Move a formation member toward their position in formation.
    void moveFormationMember(entt::registry& registry, entt::entity entity,
                             const SpatialHash& spatialHash,
                             const struct Formation& formation, const struct Position& formationPos,
                             float speed, float dt);

    /// Move a free unit (no formation) toward their movement target.
    void moveFreeUnit(entt::registry& registry, entt::entity entity,
                      const SpatialHash& spatialHash, float speed, float dt);

    /// Move a routing unit away from enemies.
    void fleeFromEnemies(entt::registry& registry, entt::entity entity,
                         const SpatialHash& spatialHash, float speed, float dt);

    // Scratch buffer for spatial queries (avoids per-frame allocation)
    std::vector<entt::entity> m_nearbyBuffer;
};

} // namespace fob
