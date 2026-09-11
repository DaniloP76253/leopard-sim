#ifndef MATCH_H
#define MATCH_H

#include "tank.h"
#include "vecmath.h"

/* Tracks the wave-based encounter loop: how many enemies you've put down,
 * which wave you're on, and (while the current enemy is dead) a countdown
 * to the next one rolling in. Shared by both frontends so console and 3D
 * play the same match structure. */
typedef struct {
    int wave;
    int kills;
    double respawn_timer_s;
} MatchState;

typedef struct {
    Vec2 position;
    double radius_m;
    double heal_per_s;
} SupplyDepot;

void match_state_init(MatchState *m);

/* Called once an enemy has died and its respawn timer has elapsed: gives
 * it a fresh loadout, a new position at a random bearing/range from
 * `origin`, and a small difficulty bump per wave (faster reload, floored
 * at 4s so it never becomes unfair). `name` should be a string literal or
 * otherwise independent buffer -- do not pass enemy->name, since the
 * struct is memset before the name is copied. */
void match_spawn_wave(MatchState *m, Tank *enemy, const char *name, Vec2 origin,
                       double min_range_m, double max_range_m);

/* Tops up ammo (to each type's recorded max) and slowly repairs HP while
 * `t` is inside `depot`'s radius. Returns 1 on the tick it first reaches
 * full health *and* full ammo (handy for a one-shot "resupplied!"
 * message), 0 otherwise. */
int match_try_resupply(Tank *t, const SupplyDepot *depot, double dt);

#endif /* MATCH_H */
