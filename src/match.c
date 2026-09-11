#include "match.h"
#include <math.h>
#include <stdlib.h>

void match_state_init(MatchState *m)
{
    m->wave = 1;
    m->kills = 0;
    m->respawn_timer_s = 0.0;
}

void match_spawn_wave(MatchState *m, Tank *enemy, const char *name, Vec2 origin,
                       double min_range_m, double max_range_m)
{
    double angle = deg2rad((double)(rand() % 360));
    double range = min_range_m + ((double)rand() / (double)RAND_MAX) * (max_range_m - min_range_m);
    Vec2 pos = vec2_add(origin, vec2_make(cos(angle) * range, sin(angle) * range));
    double heading = norm_deg(rad2deg(angle) + 180.0); /* roughly facing back towards origin */

    tank_init_generic_mbt(enemy, name, pos, heading);

    /* modest difficulty ramp: reload a little faster each wave, floor at 4s */
    double reload = enemy->reload_time_s * pow(0.94, (double)(m->wave - 1));
    if (reload < 4.0) reload = 4.0;
    enemy->reload_time_s = reload;
}

int match_try_resupply(Tank *t, const SupplyDepot *depot, double dt)
{
    if (!t->alive) return 0;
    if (vec2_dist(t->position_m, depot->position) > depot->radius_m) return 0;

    int was_full = (t->hp >= 100.0);
    for (int i = 0; i < AMMO_TYPE_COUNT; i++) {
        if (t->ammo_count[i] < t->ammo_max[i]) was_full = 0;
    }

    for (int i = 0; i < AMMO_TYPE_COUNT; i++) {
        if (t->ammo_count[i] < t->ammo_max[i]) t->ammo_count[i] = t->ammo_max[i];
    }
    t->hp = clampd(t->hp + depot->heal_per_s * dt, 0.0, 100.0);

    int is_full = (t->hp >= 100.0);
    for (int i = 0; i < AMMO_TYPE_COUNT; i++) {
        if (t->ammo_count[i] < t->ammo_max[i]) is_full = 0;
    }

    return (!was_full && is_full);
}
