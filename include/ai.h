#ifndef AI_H
#define AI_H

#include "tank.h"

/* Driving + turret/gun inputs the AI wants applied this tick, in the same
 * -1..1 ranges tank_update_movement()/tank_update_turret() already expect. */
typedef struct {
    double throttle;
    double steer;
    double turret_input;
    double elevation_input;
    int    wants_to_fire;   /* 1 once the gun is roughly on target */
} AiCommand;

/* Small amount of state kept between ticks so the AI reads as "a driver
 * and gunner with intent" rather than re-rolling random inputs every
 * frame (which is what the old target_ai coin-flip did). */
typedef struct {
    double preferred_range_m;
    double flank_sign;      /* +1 or -1: which way it's currently circling */
    double flank_retime_s;  /* counts down to 0, then flips flank_sign     */
} AiState;

void ai_state_init(AiState *state, double preferred_range_m);

/* Decide this tick's driving + aiming for `self`, engaging `enemy`.
 * Leads the target using its current velocity, solves the low-angle
 * ballistic elevation for the current ammo's muzzle velocity (ignoring
 * drag -- a "quick solution" a real ballistic computer would refine from),
 * retreats and keeps the gun on target when badly hurt, and otherwise
 * holds a preferred engagement range while circling instead of parking
 * nose-on. It does not path-plan around terrain or obstacles. */
AiCommand ai_compute_command(AiState *state, const Tank *self, const Tank *enemy, double dt);

#endif /* AI_H */
