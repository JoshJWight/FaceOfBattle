#pragma once

namespace fob {

// Simulation
constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;

// Spatial
constexpr float MELEE_RANGE = 2.0f;
constexpr float FORMATION_SPACING = 2.5f;
constexpr float MORALE_EFFECT_RADIUS = 20.0f;
constexpr float SPATIAL_HASH_CELL_SIZE = 10.0f;

// Separation / Collision avoidance
constexpr float ALLY_SEPARATION_RADIUS = 2.0f;    // Start separating when closer than this
constexpr float ALLY_SEPARATION_STRENGTH = 8.0f;  // How strongly to push apart
constexpr float ENEMY_STOP_RADIUS = 3.0f;         // Stop advancing when enemy within this range

// Combat
constexpr float BASE_ATTACK_STAMINA_COST = 10.0f;
constexpr float BASE_BLOCK_STAMINA_COST = 5.0f;
constexpr float STAMINA_REGEN_RATE = 5.0f;

// Morale
constexpr float ALLY_KILL_MORALE_BOOST = 0.05f;
constexpr float ALLY_DEATH_MORALE_HIT = 0.08f;
constexpr float NEARBY_ROUT_MORALE_HIT = 0.15f;
constexpr float OFFICER_DEATH_MORALE_HIT = 0.20f;
constexpr float ROUT_THRESHOLD = 0.0f;
constexpr float FRONT_LINE_MORALE_BONUS = 0.1f;

// Movement speeds (units per second)
constexpr float LIGHT_INFANTRY_SPEED = 8.0f;
constexpr float HEAVY_INFANTRY_SPEED = 5.0f;
constexpr float CAVALRY_SPEED = 15.0f;

// Rendering
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr float DEFAULT_ZOOM = 1.0f;
constexpr float MIN_ZOOM = 0.02f;   // Can see entire battlefield
constexpr float MAX_ZOOM = 20.0f;   // Can see individual units clearly

} // namespace fob
