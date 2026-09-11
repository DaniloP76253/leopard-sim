# Leopard 2A7 — Drive & Gunnery Simulator (C)

A text-console tank sim with War Thunder–flavored mechanics: real
differential-steering driving physics (turning costs real speed, like an
actual tracked vehicle), independent turret/gun control, gravity+drag
ballistic flight for every round (not hitscan), 3D armor-obliquity
penetration resolution, a smart-ish enemy gunner, and a wave/resupply
combat loop.

## Build & run

```
make
./leopard_sim
# or: make run
```

Needs only a C compiler and libm (`gcc`/`clang`, `-lm`). No external
libraries.

You can also build the optional 3D frontend:

```
make raylib
./leopard_sim_3d
# or: make raylib-run
```

`make raylib` bootstraps a local Raylib install inside the workspace if
needed, then builds the 3D frontend.

## Current state of the two frontends (read this first)

Both frontends share the same physics/ballistics/armor/ammo code, so the
driving, gunnery, and penetration feel identical either way. They do
**not** currently have identical feature sets:

- **Console (`leopard_sim`)** has everything below: the shared smart-AI
  module, the wave/kill loop, and the supply depot.
- **3D (`leopard_sim_3d`)** has the new camera + aiming + gunner-sight
  rework (see below), but still uses its own simple random-walk target
  driver and has no wave/resupply loop yet, and the terrain/tanks are
  still plain colored shapes rather than textured. Wiring the shared
  `ai.c`/`match.c` modules and adding procedural textures into the 3D
  frontend is the natural next step and hasn't been done yet.

## What's actually simulated

- **Driving physics** (`tank_update_movement`): throttle/steer ->
  acceleration -> speed, with rolling resistance, active braking, a
  top-speed cap, and a terrain grip multiplier. Steering now costs real
  speed the way a tracked vehicle's differential steering actually
  does: part of the engine's power is diverted into swinging the tracks
  at different speeds instead of pushing you forward, and the tracks
  scrub sideways against the ground while turning. Crank a hard turn at
  70 km/h and you'll bleed down to roughly 50 km/h in about 3 seconds;
  pivoting in place from a standstill is unaffected (nothing to divert
  power from or scrub off yet), same as a real neutral-steer turn.
  There's a demo "mud patch" zone (`MUD_CENTER_X/Y` in `main.c`, or the
  full `terrain.c` slope-based grip model in the 3D build) that cuts
  grip further.
- **Turret & gun** (`tank_update_turret`): independent azimuth/elevation
  with real slew-rate limits (deg/s) and elevation limits, decoupled
  from hull heading.
- **Ballistics** (`projectile.c`): every round is launched with its own
  muzzle velocity and flies a real parabolic arc — gravity pulls it
  down every step, and drag bleeds velocity with range. Long shots
  genuinely need you to lead/elevate; there's no auto-aim.
- **Armor & penetration** (`armor.c` + the firing logic in `combat.c`):
  each hull/turret face is a plate with a thickness and a slope. The
  code builds the plate's outward-facing 3D normal from the vehicle's
  heading/turret angle and compares it against the incoming round's
  actual 3D flight vector at the moment of impact. `combat_fire_shot()`
  also returns a `CombatShotResult` (hit/miss/penetration outcome,
  impact point, range, damage) so a GUI frontend can react to a shot
  without scraping stdout — the 3D frontend doesn't consume this yet,
  but the plumbing is there for hit markers/tracers.
- **Enemy AI** (`ai.c`, console only so far): leads the target using its
  current velocity, solves the low-angle ballistic elevation equation
  for whatever ammo it has loaded (same physics a real quick-fire
  solution uses, minus drag), retreats and keeps the gun on you when
  hurt ("shoot and scoot"), and otherwise holds a preferred engagement
  range while circling instead of parking nose-on. Toggle it with
  `t ai on` in the console build.
- **Wave/resupply loop** (`match.c`, console only so far): destroying
  the target starts a short countdown before a fresh one rolls in at a
  random bearing/range, reloading a little faster each wave (floored so
  it stays fair). Drive over the marked supply point to top off ammo and
  slowly repair HP.
- **Gun handling**: a small dispersion cone that widens if you fire
  while the hull or turret is actively moving.
- **Damage model**: penetrating hits reduce a simplified 0–100 HP pool,
  with a per-hit chance of a catastrophic kill that's much higher on
  the turret bustle (ammo-storage area) than on well-protected frontal
  armor — a common, non-sensitive design generalization in defense
  writeups, applied here purely as a gameplay flourish with made-up
  probabilities.

## 3D frontend: camera and aiming

The 3D build's mouse control was reworked from "raycast wherever the
chase camera happens to point" to a proper gunner control model:

- Mouse motion (and the `J`/`L`/`I`/`K` trim keys) both drive one
  persistent "desired aim" state, which the turret servos toward every
  tick — the same error/rate math the AI already used. This means the
  turret can traverse the full 360 degrees regardless of where the
  camera is looking, including straight behind you.
- **`V`** toggles a real gunner-sight view: the camera locks to the
  barrel with a narrow (magnified) FOV, and the HUD swaps to a proper
  sight picture — center reticle, mil-dot stadia ticks, and an
  elevation tape showing where the gun sits between its limits.
- The default chase camera now has terrain collision (it won't sink
  into a hillside behind you), smooth FOV transitions between the two
  view modes, and a small recoil "kick" on firing.
- In chase view, if the gun is aimed somewhere off-screen (behind you,
  or past the edge of the camera's view), a `<< GUN` / `GUN >>` hint
  shows which way to look instead of the reticle just vanishing.

## Important note on the numbers

The armor thicknesses, penetration tables, speeds, traverse rates and
reload times are **clearly-labeled editable placeholders** — ballpark
figures in the right neighborhood, not verified exact War Thunder
datamine values (those shift between patches) and not real classified
vehicle specifications (actual composite armor performance is not
public). Everything you'd want to tune lives in:

- `src/ammo.c` -> `ammo_init_table()` — muzzle velocity, penetration-vs-range
  table, HE filler, normalization angle, per ammo type.
- `src/tank.c` -> `tank_init_leopard2a7()` / `tank_init_generic_mbt()` —
  mass, engine, speeds, turret/gun rates, reload time, ammo loadout
  (which also sets the resupply cap), and every armor plate's thickness
  + slope.
- `src/tank.c` -> `tank_update_movement()` — `TURN_POWER_SHARE_MAX` and
  `TURN_SCRUB_DECEL_MSS` at the top control how hard turning bleeds speed.
- `src/ai.c` — `AI_HP_RETREAT_THRESHOLD`, `AI_RANGE_MARGIN_M`,
  `AI_FLANK_PERIOD_S` control the enemy's behavior thresholds.
- `src/match.c` / `main.c` — wave difficulty ramp and supply depot
  position/radius/heal-rate.
- `src/main_raylib_fixed.c` — `TURRET_MOUSE_SENS_DEG_PER_PX` /
  `ELEVATION_MOUSE_SENS_DEG_PER_PX` control mouse-aim sensitivity.

## Controls (console)

| Command | Effect |
|---|---|
| `w`/`s` [n] | throttle forward/reverse for n ticks |
| `a`/`d` [n] | steer left/right for n ticks |
| `b` [n] | brake |
| `n` [n] | do nothing / let time pass (e.g. to wait out a reload) |
| `drive <throttle> <steer>` | one tick of custom analog input |
| `j`/`l` [n] | turret left/right |
| `i`/`k` [n] | gun elevation up/down |
| `aim <turret> <elev>` | one tick of custom analog turret/gun input |
| `1`/`2`/`3` | select APFSDS / HEAT / HE |
| `f` | fire |
| `p` | status | `m` | ASCII map |
| `t pos x y heading` | reposition the target (scenario tool) |
| `t turret deg` | set the target's turret angle |
| `t ai on\|off` | enemy AI: drives, leads you, and shoots back |
| `reset` | restart the scenario (new match, wave 1) |
| `quit` | exit |

## Controls (3D)

`W`/`S` drive, `A`/`D` steer, mouse aims the gun (works all the way
round), `J`/`L`/`I`/`K` trim the turret/elevation, `V` toggles the
gunner sight, right-mouse-drag free-looks the chase camera, wheel
zooms, left click/space fires, `1`/`2`/`3` select ammo, `P` dumps status
to the console, `R` resets.

## Extending it

- `ammo.h/.c` — add more rounds, or a real drag-coefficient curve.
- `armor.h/.c` — swap the single-thickness plates for layered
  composite/ERA models, or add explicit weak-spot zones.
- `ai.c` — the AI is deliberately simple (no path-planning around
  terrain/obstacles); add real pathfinding or per-wave behavior presets.
- `main_raylib_fixed.c` — the biggest open items are: wire in `ai.c` +
  `match.c` (the console build's smarter AI and wave/resupply loop) so
  both frontends play the same match structure, add procedural
  camo/terrain/rock textures using the `CombatShotResult` struct to
  drive tracer/hit-marker effects, and support multiple simultaneous
  enemies instead of one at a time.
- `main.c` — add a proper terrain heightmap to the console build (it
  currently uses a flat-ground + mud-patch model instead of the full
  `terrain.c` slope system that the 3D build uses).
