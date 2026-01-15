#pragma once

#include "core/types.hpp"
#include "simulation/spatial_hash.hpp"
#include <entt/entt.hpp>

namespace fob {

/// Manages formation-level movement and state transitions.
///
/// Formations are higher-level units that soldiers belong to. The formation
/// advances as a whole, and individual soldiers maintain their position
/// within it.
///
/// State transitions:
/// - Advancing → Engaged: When front-line soldiers contact enemies
/// - Engaged → Advancing: (TODO) When ordered to push or enemies retreat
/// - Any → Broken: (TODO) When morale collapses
class FormationSystem {
public:
    FormationSystem() = default;

    /// Update all formations for one simulation tick.
    /// @param registry The ECS registry
    /// @param spatialHash Spatial index for finding nearby enemies
    /// @param dt Delta time (should be FIXED_TIMESTEP)
    void update(entt::registry& registry, const SpatialHash& spatialHash, float dt);

private:
    /// Check if any front-line soldiers in this formation are in contact with enemies.
    bool checkEnemyContact(entt::registry& registry, const SpatialHash& spatialHash,
                           entt::entity formationEntity);

    // Scratch buffer for spatial queries
    std::vector<entt::entity> m_nearbyBuffer;
};

} // namespace fob
