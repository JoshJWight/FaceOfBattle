# Face of Battle - Architecture

## Overview

A historical battle simulator where morale, not health, determines victory. Based on the
observation that real ancient/medieval battles had ~10% casualties, mostly during the rout.

## Tech Stack

- **C++17** - Core language
- **SDL2** - Window management, input, rendering
- **EnTT** - Entity Component System (ECS)
- **CMake** - Build system

## Project Structure

```
src/
├── core/
│   ├── types.hpp        # Basic types (Vec2, FormationId)
│   └── constants.hpp    # Game constants (speeds, ranges, morale values)
├── components/
│   └── components.hpp   # All ECS components
├── systems/
│   ├── render_system.*  # Drawing units to screen
│   └── movement_system.*# Unit movement (see below)
├── simulation/
│   └── spatial_hash.hpp # O(1) spatial queries for nearby units
└── main.cpp             # Entry point, main loop
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
| `FormationMember` | Formation assignment and local position |
| `MovementTarget` | Where the unit wants to go |
| `InCombat` | Currently fighting (stores opponent) |
| `Routing` | Tag: unit is fleeing |
| `Dead` | Tag: unit is dead (kept for corpse rendering) |
| `Pursuing` | Tag: chasing routing enemies |

### Systems (execution order)

1. **CommandSystem** (TODO) - Officers issue orders
2. **BehaviorSystem** (TODO) - AI decision making
3. **FormationSystem** (TODO) - Maintain formation spacing
4. **MovementSystem** - Move units toward targets
5. **CombatSystem** (TODO) - Resolve melee combat
6. **MoraleSystem** (TODO) - Update morale from events
7. **StateSystem** (TODO) - Handle state transitions

## Movement System

The movement system handles all unit locomotion.

### Behavior by State

| State | Movement Behavior |
|-------|-------------------|
| Normal | Move toward `MovementTarget` at unit's speed |
| `InCombat` | No movement (held in place) |
| `Routing` | Flee away from nearest enemy at 1.5x speed |
| `Dead` | No movement |

### Speed by Unit Type

- Heavy Infantry: 5.0 units/sec
- Light Infantry: 8.0 units/sec
- Cavalry: 15.0 units/sec

### Implementation Notes

- Uses fixed timestep (60 Hz) for deterministic simulation
- Velocity is computed fresh each frame based on target, not accumulated
- Spatial hash used to find nearby enemies for routing direction
- Units stop when within `MELEE_RANGE` of target

## Main Loop

```
while running:
    handle input

    while accumulator >= FIXED_TIMESTEP:
        run systems in order
        rebuild spatial hash
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
