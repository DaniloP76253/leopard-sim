# Leopard 2A7 — Drive & Gunnery Simulator (C)

A text-console tank sim with War Thunder–flavored mechanics: real
differential-steering driving physics, independent turret/gun control,
gravity+drag ballistic flight for every round (not hitscan), and 3D
armor-obliquity penetration resolution.

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
needed, then builds the 3D frontend. The 3D frontend uses the same tank,
armor, ballistics, and combat logic as the console version, but renders
the action in a Raylib window.

3D controls are slightly different from the console version: hold the
right mouse button and move the mouse to orbit the camera, use the mouse
wheel to zoom, and left click or Space to fire.

## Important note on the numbers

The armor thicknesses, penetration tables, speeds, traverse rates and
reload times are **clearly-labeled editable placeholders** — ballpark
figures in the right neighborhood, not verified exact War Thunder
datamine values (those shift between patches) and not real classified
vehicle specifications (actual composite armor performance is not
public). Everything you'd want to tune lives in two places:

- `src/ammo.c` → `ammo_init_table()` — muzzle velocity, penetration-vs-range
  table, HE filler, normalization angle, per ammo type.
- `src/tank.c` → `tank_init_leopard2a7()` / `tank_init_generic_mbt()` —
  mass, engine, speeds, turret/gun rates, reload time, and every armor
  plate's thickness + slope.

Pull whatever exact numbers you want from the War Thunder Wiki (or make
up your own balance) and drop them straight into those structs — the
physics/engine code doesn't know or care where the numbers came from.

## What's actually simulated

- **Driving physics** (`tank_update_movement`): throttle/steer →
  acceleration → speed, with rolling resistance, active braking, a
  top-speed cap, and a terrain grip multiplier (there's a demo "mud
  patch" zone in `main.c` at `MUD_CENTER_X/Y` that cuts grip to 0.55 —
  easy to extend into a real terrain map).
- **Turret & gun** (`tank_update_turret`): independent azimuth/elevation
  with real slew-rate limits (deg/s) and elevation limits, decoupled
  from hull heading.
- **Ballistics** (`projectile.c`): every round is launched with its own
  muzzle velocity and flies a real parabolic arc — gravity pulls it
  down every step, and drag bleeds velocity with range. Long shots
  genuinely need you to lead/elevate; there's no auto-aim.
- **Armor & penetration** (`armor.c` + the firing logic in `main.c`):
  each hull/turret face is a plate with a thickness and a slope. The
  code builds the plate's outward-facing 3D normal from the vehicle's
  heading/turret angle and compares it against the incoming round's
  actual 3D flight vector at the moment of impact — so a shot's
  effective armor depends on *both* your firing angle around the tank
  *and* the plate's built-in slope *and* how much the round has arced
  downward by the time it arrives. Penetration is looked up from the
  ammo's range table and compared against that effective thickness.
  APFSDS/HEAT/HE behave differently (HEAT's penetration barely drops
  with range but gets a small "normalization" angle allowance; HE has
  almost no penetration but a big blast-damage number).
- **Gun handling**: a small dispersion cone that widens if you fire
  while the hull or turret is actively moving — encourages settling
  the aim rather than snap-shooting on the move, like a modern
  stabilizer that's good but not magic.
- **Damage model**: penetrating hits reduce a simplified 0–100 HP pool,
  with a per-hit chance of a catastrophic kill that's much higher on
  the turret bustle (ammo-storage area) than on well-protected frontal
  armor — a common, non-sensitive design generalization in defense
  writeups, applied here purely as a gameplay flourish with made-up
  probabilities.

## Controls

Run the program and type `help` any time, or see the banner it prints
on startup. Quick summary:

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
| `t ai on\|off` | let the target drive itself randomly |
| `reset` | restart the scenario |
| `quit` | exit |

Heading convention: 0° = East (+X), 90° = North (+Y), counter-clockwise
positive — standard math angles, shown on the ASCII map.

## Extending it

The code is deliberately modular so you can grow it:

- `ammo.h/.c` — add more rounds, or a real drag-coefficient curve.
- `armor.h/.c` — swap the single-thickness plates for layered
  composite/ERA models, or add explicit weak-spot zones.
- `tank.h/.c` — add engine torque curves, track damage, module
  (engine/gun/tracks) knock-out states instead of one HP pool.
- `main.c` — add the target shooting back (it already has a full
  `Tank` with its own reload/ammo fields, just unused for firing), a
  proper terrain heightmap, or swap the text UI for an SDL2/ncurses
  front end using the same physics/ballistics/armor modules untouched.
- `src/main_raylib.c` — optional real-time 3D frontend built with
  Raylib, using the same shared combat logic as the console simulator.
