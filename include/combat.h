#ifndef COMBAT_H
#define COMBAT_H

#include "tank.h"
#include "vecmath.h"

typedef enum {
    SHOT_OUTCOME_INVALID = 0,   /* could not fire: dead target, no ammo, reloading, gun destroyed */
    SHOT_OUTCOME_MISS,
    SHOT_OUTCOME_TERRAIN_BLOCKED,
    SHOT_OUTCOME_NO_PENETRATION,
    SHOT_OUTCOME_PENETRATION,
    SHOT_OUTCOME_CATASTROPHIC_KILL
} ShotOutcome;

/* Structured result of a fired shot, so a GUI frontend can draw a tracer /
 * impact effect / hit marker without having to scrape stdout. The console
 * frontend can just pass NULL if it doesn't need this -- all the same
 * information is still printed to stdout exactly as before. */
typedef struct {
    ShotOutcome outcome;
    Vec3 impact_point;      /* world-space (x, y, z-up), meters */
    double range_m;
    double damage_dealt;
    const char *face_label; /* NULL unless the shot actually reached the target's silhouette */
} CombatShotResult;

void combat_fire_shot(Tank *shooter, Tank *target, CombatShotResult *out_result);

#endif /* COMBAT_H */
