#include "systems/combat_system.hpp"
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

} // anonymous namespace

CombatSystem::CombatSystem()
    : m_rng(std::random_device{}()) {}

void CombatSystem::update(entt::registry& registry, const SpatialHash& spatialHash, float dt) {
    // Decay flash effects
    auto flashView = registry.view<FlashEffect>();
    for (auto entity : flashView) {
        auto& flash = flashView.get<FlashEffect>(entity);
        if (flash.timer > 0.0f) {
            flash.timer -= dt;
            if (flash.timer <= 0.0f) {
                flash.type = FlashEffect::None;
            }
        }
    }

    // Update attack cooldowns and process attacks
    auto combatantView = registry.view<Position, Team, Stats>(entt::exclude<Dead, Routing>);

    for (auto entity : combatantView) {
        // Update cooldown timer if in combat
        auto* inCombat = registry.try_get<InCombat>(entity);
        if (inCombat) {
            inCombat->combatTimer += dt;
        }

        // Try to find a target and attack
        entt::entity target = findTarget(registry, spatialHash, entity);

        if (target != entt::null) {
            // We have a valid target
            if (!inCombat) {
                // Enter combat with randomized initial cooldown
                registry.emplace<InCombat>(entity, target);
                inCombat = registry.try_get<InCombat>(entity);
                std::uniform_real_distribution<float> initialDelay(0.0f, ATTACK_COOLDOWN);
                inCombat->combatTimer = initialDelay(m_rng);
            } else {
                // Update target if changed
                inCombat->opponent = target;
            }

            // Attack if cooldown has elapsed
            if (inCombat->combatTimer >= ATTACK_COOLDOWN) {
                performAttack(registry, entity, target);
                // Randomize next cooldown (1x to 2x base) to stagger attacks
                std::uniform_real_distribution<float> cooldownVariance(0.0f, ATTACK_COOLDOWN);
                inCombat->combatTimer = -cooldownVariance(m_rng);
            }
        } else {
            // No target in range - leave combat
            if (inCombat) {
                registry.remove<InCombat>(entity);
            }
        }
    }
}

entt::entity CombatSystem::findTarget(entt::registry& registry, const SpatialHash& spatialHash,
                                       entt::entity attacker) {
    const auto& attackerPos = registry.get<Position>(attacker);
    const auto& attackerTeam = registry.get<Team>(attacker);

    spatialHash.queryRadius(attackerPos.x, attackerPos.y, ATTACK_RANGE, m_nearbyBuffer);

    entt::entity bestTarget = entt::null;
    float bestDist = ATTACK_RANGE + 1.0f;

    for (auto other : m_nearbyBuffer) {
        if (other == attacker) continue;
        if (!registry.valid(other)) continue;
        if (registry.all_of<Dead>(other)) continue;

        const auto* otherTeam = registry.try_get<Team>(other);
        if (!otherTeam || otherTeam->value == attackerTeam.value) continue;  // Same team

        // Must have health to be a valid target
        if (!registry.all_of<Stats>(other)) continue;

        const auto& otherPos = registry.get<Position>(other);
        float dist = distance(attackerPos.x, attackerPos.y, otherPos.x, otherPos.y);

        if (dist <= ATTACK_RANGE && dist < bestDist) {
            bestTarget = other;
            bestDist = dist;
        }
    }

    return bestTarget;
}

void CombatSystem::performAttack(entt::registry& registry, entt::entity attacker, entt::entity target) {
    if (!registry.valid(target)) return;
    if (registry.all_of<Dead>(target)) return;

    auto* targetStats = registry.try_get<Stats>(target);
    if (!targetStats) return;

    // Flash white on attacker to show they're attacking
    registry.emplace_or_replace<FlashEffect>(attacker, FlashEffect::Attack);

    // Roll for hit type
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    float hitRoll = roll(m_rng);

    float damage = 0.0f;

    if (hitRoll < MISS_CHANCE) {
        // Miss - no damage
        damage = 0.0f;
    } else {
        // Hit - determine if light or heavy
        float damageRoll = roll(m_rng);
        if (damageRoll < HEAVY_HIT_CHANCE) {
            damage = HEAVY_DAMAGE;
        } else {
            damage = LIGHT_DAMAGE;
        }
    }

    // Apply damage
    if (damage > 0.0f) {
        // Factor in defense (simple reduction)
        float actualDamage = std::max(1.0f, damage - targetStats->defense * 0.5f);
        targetStats->health -= actualDamage;

        // Flash yellow on target to show they got hit
        registry.emplace_or_replace<FlashEffect>(target, FlashEffect::Hit);

        // Check for death
        checkDeath(registry, target);
    }
}

void CombatSystem::checkDeath(entt::registry& registry, entt::entity entity) {
    auto* stats = registry.try_get<Stats>(entity);
    if (!stats) return;

    if (stats->health <= 0.0f) {
        stats->health = 0.0f;

        // Mark as dead
        if (!registry.all_of<Dead>(entity)) {
            registry.emplace<Dead>(entity);
        }

        // Remove combat-related components
        registry.remove<InCombat>(entity);
        registry.remove<Routing>(entity);
        registry.remove<Pursuing>(entity);
    }
}

} // namespace fob
