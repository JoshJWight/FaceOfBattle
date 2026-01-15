#pragma once

#include "simulation/spatial_hash.hpp"
#include <entt/entt.hpp>
#include <random>

namespace fob {

/// Handles melee combat between soldiers.
///
/// Each tick:
/// 1. Soldiers look for enemies within ATTACK_RANGE
/// 2. If cooldown has elapsed, they attack
/// 3. Attack rolls for miss/light/heavy damage
/// 4. Damage is applied to target's health
/// 5. Units at 0 HP are marked Dead
class CombatSystem {
public:
    CombatSystem();

    /// Process combat for all units.
    /// @param registry The ECS registry
    /// @param spatialHash Spatial index for finding nearby enemies
    /// @param dt Delta time
    void update(entt::registry& registry, const SpatialHash& spatialHash, float dt);

private:
    /// Find the best target for a soldier to attack.
    /// Returns entt::null if no valid target in range.
    entt::entity findTarget(entt::registry& registry, const SpatialHash& spatialHash,
                            entt::entity attacker);

    /// Perform an attack from attacker to target.
    /// Rolls for damage and applies it.
    void performAttack(entt::registry& registry, entt::entity attacker, entt::entity target);

    /// Check if a unit should die and mark them Dead if so.
    void checkDeath(entt::registry& registry, entt::entity entity);

    std::mt19937 m_rng;
    std::vector<entt::entity> m_nearbyBuffer;
};

} // namespace fob
