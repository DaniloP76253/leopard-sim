#include "ammo.h"
#include <math.h>

static AmmoStats g_ammo[AMMO_TYPE_COUNT];
static int g_ammo_ready = 0;

void ammo_init_table(void)
{
    /* ============================================================
     *  EDIT ME: drop exact numbers from the War Thunder Wiki here.
     * ============================================================ */

    /* --- DM53-style APFSDS (kinetic long-rod penetrator) --- */
    g_ammo[AMMO_APFSDS] = (AmmoStats){
        .name               = "DM53 APFSDS",
        .short_name         = "APFSDS",
        .muzzle_velocity_ms = 1750.0,
        .drag_decel_per_m   = 0.00006, /* slow velocity bleed over range */
        .range_points_m     = {   0,  500, 1000, 1500, 2000, 3000 },
        .pen_points_mm      = { 660,  620,  590,  560,  530,  480 },
        .he_filler_g        = 0.0,
        .normalization_deg  = 0.0,   /* rigid rod: negligible normalization */
        .blast_radius_m     = 0.0,
    };

    /* --- DM12-style HEAT-MP (shaped charge, chemical energy) --- */
    g_ammo[AMMO_HEAT] = (AmmoStats){
        .name               = "DM12 HEAT-MP",
        .short_name         = "HEAT",
        .muzzle_velocity_ms = 1140.0,
        .drag_decel_per_m   = 0.00012, /* lighter/slower round, sheds speed faster */
        .range_points_m     = {   0,  500, 1000, 1500, 2000, 3000 },
        .pen_points_mm      = { 800,  800,  800,  800,  800,  800 }, /* chemical energy: ~range-independent */
        .he_filler_g        = 450.0,
        .normalization_deg  = 5.0,   /* small forgiving cone before jet loses efficiency */
        .blast_radius_m     = 2.0,
    };

    /* --- DM11-style HE / programmable airburst round --- */
    g_ammo[AMMO_HE] = (AmmoStats){
        .name               = "DM11 HE-PFF",
        .short_name         = "HE",
        .muzzle_velocity_ms = 1140.0,
        .drag_decel_per_m   = 0.00012,
        .range_points_m     = {   0,  500, 1000, 1500, 2000, 3000 },
        .pen_points_mm      = {  30,   30,   30,   30,   30,   30 }, /* thin-walled, not meant for armor */
        .he_filler_g        = 1015.0,
        .normalization_deg  = 0.0,
        .blast_radius_m     = 6.0,
    };

    g_ammo_ready = 1;
}

const AmmoStats *ammo_get(AmmoTypeId id)
{
    if (!g_ammo_ready) ammo_init_table();
    if (id < 0 || id >= AMMO_TYPE_COUNT) id = AMMO_APFSDS;
    return &g_ammo[id];
}

double ammo_penetration_at_range(const AmmoStats *a, double range_m)
{
    if (range_m <= a->range_points_m[0]) return a->pen_points_mm[0];
    for (int i = 0; i < PEN_TABLE_POINTS - 1; i++) {
        double r0 = a->range_points_m[i];
        double r1 = a->range_points_m[i + 1];
        if (range_m <= r1) {
            double t = (range_m - r0) / (r1 - r0);
            double p0 = a->pen_points_mm[i];
            double p1 = a->pen_points_mm[i + 1];
            return p0 + t * (p1 - p0);
        }
    }
    return a->pen_points_mm[PEN_TABLE_POINTS - 1];
}

double ammo_velocity_at_range(const AmmoStats *a, double range_m)
{
    double v = a->muzzle_velocity_ms * exp(-a->drag_decel_per_m * range_m);
    return v;
}
