# Leopard 2A7 — Tank Simulator (Godot 4 + Rust)

A modern tank simulator featuring realistic War Thunder–style mechanics, built with **Godot 4** for 3D visual rendering/world construction and **Rust (GDExtension)** for physics, ballistics, and damage simulation.

## Architecture

- **`rust/`** — Pure Rust simulation core and GDExtension bindings:
  - `sim/tank.rs`: Skid-steering driving physics, track scrub deceleration, module damage.
  - `sim/projectile.rs`: Real parabolic ballistic flight with aerodynamic drag and gravity.
  - `sim/armor.rs`: 3D plate obliquity, normalization angle, line-of-sight thickness.
  - `sim/combat.rs`: 3D silhouette hit detection, penetration calculation, catastrophic kill rolls.
  - `sim/ammo.rs`: APFSDS, HEAT-MP, and HE-PFF ballistic and penetration curves.
  - `sim/ai.rs`: Enemy target leading, ballistic elevation solution, and circling tactics.
  - `sim/match_state.rs`: Wave progression, respawn timers, and supply depot resupply.
  - `godot_nodes/game_manager.rs`: `TankGame` GDExtension node exposed directly to Godot.
- **`godot/`** — Godot 4 project:
  - `scenes/main.tscn`: 3D battlefield scene (terrain, lighting, environment, depot, tanks, HUD).
  - `scenes/main.gd`: Input handling, chase camera, and gunner optics.
  - `scenes/sight_overlay.gd`: Procedural 2D mil-dot optics, stadia ticks, elevation tape, and chase reticle.
  - `assets/`: 3D models (`hull.obj`, `turret.obj`, `barrel.obj`, etc.) and PBR camo textures.

## Requirements

- **Rust toolchain** (`cargo` and `rustc`).
- **Godot 4.3+** (Godot 4.7 supported).

## Quick Start

```bash
# Build the Rust GDExtension and run in Godot
make run-godot

# Open the project in the Godot 4 Editor
make editor

# Run the Rust simulation unit test suite
make test-rust
```

## Controls

| Key / Input | Action |
|---|---|
| `W` / `S` | Throttle forward / reverse |
| `A` / `D` | Steer left / right (differential steering) |
| Mouse | Aim turret & gun (full 360° traversal) |
| `RMB` (Hold) | Free-look camera around tank |
| Scroll Wheel | Zoom camera in / out |
| `V` | Toggle Gunner Sight optics (magnified view + reticle) |
| `J` / `L` | Trim turret traverse left / right |
| `I` / `K` | Trim gun elevation up / down |
| `LMB` / `Space` | Fire loaded round |
| `1` / `2` / `3` | Select ammo: APFSDS / HEAT-MP / HE-PFF |
| `R` | Reset match / scenario |
| `Esc` | Toggle mouse capture |

## Preserved Versions

- The previous C / Raylib implementation is archived in the **`c-version`** Git branch.
