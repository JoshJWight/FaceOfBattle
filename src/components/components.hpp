#pragma once

#include "core/types.hpp"
#include <entt/entt.hpp>

namespace fob {

// ============================================================================
// Identity & Spatial
// ============================================================================

struct Position {
    float x = 0.0f;
    float y = 0.0f;

    Position() = default;
    Position(float x_, float y_) : x(x_), y(y_) {}
    Position(Vec2 v) : x(v.x), y(v.y) {}

    Vec2 toVec2() const { return Vec2(x, y); }
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;

    Velocity() = default;
    Velocity(float dx_, float dy_) : dx(dx_), dy(dy_) {}
    Velocity(Vec2 v) : dx(v.x), dy(v.y) {}

    Vec2 toVec2() const { return Vec2(dx, dy); }
};

struct Team {
    enum Value : uint8_t { Red, Blue };
    Value value = Red;

    Team() = default;
    Team(Value v) : value(v) {}
};

// ============================================================================
// Combat Stats
// ============================================================================

struct Stats {
    float health = 100.0f;
    float maxHealth = 100.0f;
    float stamina = 100.0f;
    float maxStamina = 100.0f;
    float attackPower = 10.0f;
    float defense = 5.0f;
    float speed = 5.0f;

    Stats() = default;
    Stats(float hp, float stam, float atk, float def, float spd)
        : health(hp), maxHealth(hp), stamina(stam), maxStamina(stam),
          attackPower(atk), defense(def), speed(spd) {}
};

struct Morale {
    float value = 1.0f;         // 0.0 = routing, 1.0 = full morale
    float baseModifier = 0.0f;  // from army size, terrain, etc.

    Morale() = default;
    Morale(float v, float mod = 0.0f) : value(v), baseModifier(mod) {}
};

// ============================================================================
// Unit Classification
// ============================================================================

struct UnitType {
    enum Type : uint8_t { LightInfantry, HeavyInfantry, Cavalry };
    Type type = HeavyInfantry;

    UnitType() = default;
    UnitType(Type t) : type(t) {}
};

struct Officer {
    int rank = 1;  // higher rank = more important

    Officer() = default;
    Officer(int r) : rank(r) {}
};

struct General {};  // tag component

// ============================================================================
// Formation
// ============================================================================

struct FormationMember {
    FormationId formationId = 0;
    Vec2 localTargetPos = {0.0f, 0.0f};  // desired pos relative to formation center
    int rank = 0;  // row in formation: 0 = front, 1 = second, etc.

    FormationMember() = default;
    FormationMember(FormationId id, Vec2 target, int r)
        : formationId(id), localTargetPos(target), rank(r) {}
};

// ============================================================================
// State Tags (presence/absence indicates state)
// ============================================================================

struct InCombat {
    entt::entity opponent = entt::null;
    float combatTimer = 0.0f;  // time since last attack

    InCombat() = default;
    InCombat(entt::entity opp) : opponent(opp), combatTimer(0.0f) {}
};

struct Routing {};   // unit is fleeing
struct Dead {};      // unit is dead (kept for rendering corpses, etc.)

struct Pursuing {
    entt::entity target = entt::null;

    Pursuing() = default;
    Pursuing(entt::entity t) : target(t) {}
};

} // namespace fob
