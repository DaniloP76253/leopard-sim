# Leopard 2A7 — Drive & Gunnery Simulator (3D)

A 3D tank sim built with Raylib and War Thunder–flavored mechanics: real
differential-steering driving physics, independent turret/gun control,
gravity+drag ballistic flight for every round, 3D armor-obliquity
penetration resolution, procedural terrain with slope-based grip, and a
gunner-sight camera system.

## Build & run

```
make
./leopard_sim_3d
# or: make run
```

`make` bootstraps a local Raylib install inside the workspace if needed,
then builds the simulator. Needs `gcc` (or `clang`), `cmake`, and the
usual X11/GL development headers.

## What's actually simulated

- **Driving physics** (`tank_update_movement`): throttle/steer ->
  acceleration -> speed, with rolling resistance, active braking, a
  top-speed cap, and a terrain grip multiplier. Steering costs real
  speed the way a tracked vehicle's differential steering actually
  does: part of the engine's power is diverted into swinging the tracks
  at different speeds instead of pushing you forward, and the tracks
  scrub sideways against the ground while turning. Crank a hard turn at
  70 km/h and you'll bleed down to roughly 50 km/h in about 3 seconds;
  pivoting in place from a standstill is unaffected.
  The full `terrain.c` slope-based grip model cuts grip on steep hills
  and mud patches.
- **Turret & gun** (`tank_update_turret`): independent azimuth/elevation
  with real slew-rate limits (deg/s) and elevation limits, decoupled
  from hull heading.
- **Ballistics** (`projectile.c`): every round is launched with its own
  muzzle velocity and flies a real parabolic arc — gravity pulls it
  down every step, and drag bleeds velocity with range. Long shots
  genuinely need you to lead/elevate; there's no auto-aim.
- **Armor & penetration** (`armor.c` + `combat.c`):
  each hull/turret face is a plate with a thickness and a slope. The
  code builds the plate's outward-facing 3D normal from the vehicle's
  heading/turret angle and compares it against the incoming round's
  actual 3D flight vector at the moment of impact. `combat_fire_shot()`
  returns a `CombatShotResult` (hit/miss/penetration outcome, impact
  point, range, damage) for hit markers/tracers.
- **Enemy AI** (`ai.c`): leads the target using its current velocity,
  solves the low-angle ballistic elevation equation for whatever ammo it
  has loaded, retreats and keeps the gun on you when hurt ("shoot and
  scoot"), and holds a preferred engagement range while circling instead
  of parking nose-on.
- **Wave/resupply loop** (`match.c`): destroying the target starts a
  short countdown before a fresh one rolls in at a random
  bearing/range, reloading a little faster each wave. Drive over the
  marked supply point to top off ammo and slowly repair HP.
- **Gun handling**: a small dispersion cone that widens if you fire
  while the hull or turret is actively moving.
- **Damage model**: penetrating hits reduce a simplified 0–100 HP pool,
  with a per-hit chance of a catastrophic kill that's much higher on
  the turret bustle (ammo-storage area) than on well-protected frontal
  armor.

## Camera and aiming

Mouse motion (and the `J`/`L`/`I`/`K` trim keys) both drive one
persistent "desired aim" state, which the turret servos toward every
tick. The turret can traverse the full 360 degrees regardless of where
the camera is looking, including straight behind you.

- **`V`** toggles a real gunner-sight view: the camera locks to the
  barrel with a narrow (magnified) FOV, and the HUD swaps to a proper
  sight picture — center reticle, mil-dot stadia ticks, and an
  elevation tape showing where the gun sits between its limits.
- The default chase camera has terrain collision, smooth FOV transitions,
  and a small recoil "kick" on firing.
- In chase view, if the gun is aimed off-screen, a `<< GUN` / `GUN >>`
  hint shows which way to look.

## Controls

| Key | Action |
|---|---|
| `W`/`S` | Drive forward / reverse |
| `A`/`D` | Steer left / right |
| Mouse | Aim gun (works full 360°) |
| `J`/`L` | Trim turret left / right |
| `I`/`K` | Trim elevation up / down |
| `V` | Toggle gunner sight |
| Right-mouse drag | Free-look chase camera |
| Scroll wheel | Zoom |
| Left click / Space | Fire |
| `1`/`2`/`3` | Select ammo (APFSDS / HEAT / HE) |
| `P` | Dump status to console |
| `R` | Reset match |

## Important note on the numbers

The armor thicknesses, penetration tables, speeds, traverse rates and
reload times are **clearly-labeled editable placeholders** — ballpark
figures in the right neighborhood, not verified exact War Thunder
datamine values and not real classified vehicle specifications. Everything
you'd want to tune lives in:

- `src/ammo.c` -> `ammo_init_table()` — muzzle velocity, penetration-vs-range
  table, HE filler, normalization angle, per ammo type.
- `src/tank.c` -> `tank_init_leopard2a7()` / `tank_init_generic_mbt()` —
  mass, engine, speeds, turret/gun rates, reload time, ammo loadout,
  and every armor plate's thickness + slope.
- `src/tank.c` -> `tank_update_movement()` — `TURN_POWER_SHARE_MAX` and
  `TURN_SCRUB_DECEL_MSS` control how hard turning bleeds speed.
- `src/ai.c` — `AI_HP_RETREAT_THRESHOLD`, `AI_RANGE_MARGIN_M`,
  `AI_FLANK_PERIOD_S` control the enemy's behavior thresholds.
- `src/match.c` — wave difficulty ramp and supply depot position/radius/heal-rate.
- `src/main_raylib_fixed.c` — `TURRET_MOUSE_SENS_DEG_PER_PX` /
  `ELEVATION_MOUSE_SENS_DEG_PER_PX` control mouse-aim sensitivity.

## Extending it

- `ammo.h/.c` — add more rounds, or a real drag-coefficient curve.
- `armor.h/.c` — swap the single-thickness plates for layered
  composite/ERA models, or add explicit weak-spot zones.
- `ai.c` — add real pathfinding or per-wave behavior presets.
- `main_raylib_fixed.c` — add procedural camo/terrain/rock textures,
  use the `CombatShotResult` struct to drive tracer/hit-marker effects,
  and support multiple simultaneous enemies.
