#ifndef AMMO_H
#define AMMO_H

/*
 * Ammo data for the Leopard 2A7's 120mm L/55 smoothbore.
 *
 * NOTE ON NUMBERS: the muzzle velocities / penetration curves below are
 * illustrative placeholders in the general ballpark of what's publicly
 * discussed for these rounds -- they are NOT exact War Thunder datamined
 * values (those change between patches and I don't have them memorized
 * reliably) and they are NOT real classified performance figures either.
 * Everything you need to tune is in ammo_init_table() in ammo.c -- pull
 * the exact numbers you want from the War Thunder Wiki and drop them in
 * there; the rest of the engine doesn't care where the numbers came from.
 */

#define PEN_TABLE_POINTS 6

typedef enum {
    AMMO_APFSDS = 0,   /* DM53-style long-rod kinetic penetrator   */
    AMMO_HEAT   = 1,   /* DM12-style HEAT-MP shaped charge          */
    AMMO_HE     = 2,   /* DM11-style HE / programmable HE round     */
    AMMO_TYPE_COUNT
} AmmoTypeId;

typedef struct {
    const char *name;         /* full name                                   */
    const char *short_name;   /* short label for HUD/status lines            */
    double muzzle_velocity_ms;
    double drag_decel_per_m;  /* fractional velocity loss per meter traveled */

    /* penetration (mm of vertical RHA-equivalent) vs. range (m) */
    double range_points_m[PEN_TABLE_POINTS];
    double pen_points_mm[PEN_TABLE_POINTS];

    double he_filler_g;       /* TNT-equivalent filler, for post-pen effect  */
    double normalization_deg; /* angle "forgiveness" before obliquity bites  */
    double blast_radius_m;    /* soft-target / module-damage radius (HE)     */
} AmmoStats;

void ammo_init_table(void);
const AmmoStats *ammo_get(AmmoTypeId id);

/* linear interpolation of the penetration table at a given range */
double ammo_penetration_at_range(const AmmoStats *a, double range_m);

/* velocity remaining after traveling the given range (simple exponential-ish decay) */
double ammo_velocity_at_range(const AmmoStats *a, double range_m);

#endif /* AMMO_H */
