# Face of Battle - Architecture

## Overview

A historical battle simulator where morale, not health, determines victory. Based on the
observation that real ancient/medieval battles had ~10% casualties, mostly during the rout.

## Tech Stack

- **C++20** - Core language
- **SDL2** - Window management, input, rendering
- **EnTT** - Entity Component System (ECS)
- **CMake** - Build system

## Project Structure

```
src/
├── core/
│   ├── types.hpp          # Basic types (Vec2)
│   └── constants.hpp      # Game constants (speeds, ranges, morale values)
├── components/
│   └── components.hpp     # All ECS components
├── systems/
│   ├── render_system.*    # Drawing units to screen
│   ├── formation_system.* # Formation-level movement and state
│   └── movement_system.*  # Individual unit movement
├── simulation/
│   └── spatial_hash.hpp   # O(1) spatial queries for nearby units
└── main.cpp               # Entry point, main loop
```

## ECS Architecture

### Components

| Component | Purpose |
|-----------|---------|
| `Position` | World coordinates (x, y) |
| `Velocity` | Current movement vector |
| `Team` | Red or Blue army |
| `Stats` | Health, stamina, attack, defense, speed |
| `Morale` | 0.0 (routing) to 1.0 (full morale) |
| `UnitType` | Light/Heavy Infantry, Cavalry |
| `Officer` | Leadership unit with rank |
| `Formation` | Formation entity: target, facing, state |
| `FormationMember` | Links soldier to formation with local offset |
| `MovementTarget` | Where a free unit wants to go |
| `InCombat` | Currently fighting (stores opponent) |
| `Routing` | Tag: unit is fleeing |
| `Dead` | Tag: unit is dead (kept for corpse rendering) |
| `Pursuing` | Tag: chasing routing enemies |

### Systems (execution order)

1. **FormationSystem** - Advance formations, detect enemy contact
2. **MovementSystem** - Move individual units (formation-relative or free)
3. **CombatSystem** (TODO) - Resolve melee combat
4. **MoraleSystem** (TODO) - Update morale from events

## Formation System

Formations are higher-level units that soldiers belong to. The formation advances as a whole,
and individual soldiers maintain their position within it.

### Formation States

| State | Behavior |
|-------|----------|
| `Advancing` | Formation moves toward target, soldiers maintain positions |
| `Engaged` | Front line in contact, formation holds, soldiers hold positions |
| `Withdrawing` | (TODO) Formation pulls back |
| `Broken` | Formation collapsed, soldiers act independently |

### State Transitions

- **Advancing → Engaged**: When any front-line soldier (rank 0) contacts an enemy
- **Engaged → Broken**: (TODO) When morale collapses

### Soldier Position Calculation

Each soldier has a `localOffset` relative to their formation center:
- `file` determines X position (left/right)
- `rank` determines Y position (front/back)

World position = formation.position + localOffset (rotated by formation.facing)

## Movement System

The movement system handles individual unit locomotion.

### Unit Categories

1. **Formation Members**: Move toward their position in formation
   - When `Advancing`: Actively seek formation position
   - When `Engaged`: Gentle drift toward position, mostly hold

2. **Free Units**: Move toward `MovementTarget` (for units without formation)

3. **Routing Units**: Flee from enemies at 1.5x speed, ignore formation

### Collision Avoidance

- **Enemy repulsion**: Strong push away from enemies within `ENEMY_STOP_RADIUS` (3.0)
- **Ally separation**: Push apart from allies within `ALLY_SEPARATION_RADIUS` (2.0)

### Speed by Unit Type

- Heavy Infantry: 5.0 units/sec
- Light Infantry: 8.0 units/sec
- Cavalry: 15.0 units/sec

## Main Loop

```
while running:
    handle input

    while accumulator >= FIXED_TIMESTEP:
        rebuild spatial hash
        formationSystem.update()
        movementSystem.update()
        accumulator -= FIXED_TIMESTEP

    render
```

## Future Systems (from vision_design.txt)

- **Stamina**: Depletes on attack/block, regenerates when out of combat
- **Morale cascade**: Nearby routs cause chain reactions
- **Officers**: Encourage attacks, push orders, bigger morale impact on death
- **Terrain**: Mud, rivers, hills with high ground advantage
- **Multi-line formations**: Roman triplex acies style
- **Pursuit phase**: Chase routing enemies, faster units catch more
