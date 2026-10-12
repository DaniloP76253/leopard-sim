#include "tank.h"
#include <string.h>
#include <math.h>

/* ===================================================================
 * All the numeric stats in the two init functions below are editable
 * placeholders (ballpark / illustrative), not verified exact War
 * Thunder datamine values and not real classified specifications.
 * Swap in whatever numbers you want -- the physics code doesn't care.
 * =================================================================== */

void tank_init_leopard2a7(Tank *t, const char *name, Vec2 pos, double heading_deg)
{
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name, sizeof(t->name) - 1);

    t->position_m = pos;
    t->heading_deg = heading_deg;
    t->speed_ms = 0.0;

    t->mass_kg            = 67000.0;   /* combat weight, approx      */
    t->engine_power_hp    = 1500.0;    /* MTU MB 873-style powerpack */
    t->accel_fwd_mss      = 1.1;
    t->accel_rev_mss      = 0.6;
    t->brake_decel_mss    = 3.0;
    t->roll_resist_mss    = 0.8;
    t->max_speed_fwd_ms   = 70.0 / 3.6;
    t->max_speed_rev_ms   = 25.0 / 3.6;
    t->max_pivot_rate_dps = 25.0;

    t->turret_azimuth_deg     = 0.0;
    t->turret_traverse_dps    = 42.0;
    t->gun_elevation_deg      = 0.0;
    t->gun_elevation_min_deg  = -9.0;
    t->gun_elevation_max_deg  = 20.0;
    t->gun_elevation_dps      = 10.0;

    t->reload_time_s  = 6.0;
    t->reload_timer_s = 0.0;

    t->module_engine_hp = 100.0;
    t->module_tracks_hp = 100.0;
    t->module_turret_hp  = 100.0;
    t->module_gun_hp     = 100.0;
    t->module_optics_hp  = 60.0;

    t->hull_pitch_deg = 0.0;
    t->hull_roll_deg  = 0.0;
    t->hull_bounce_m  = 0.0;
    t->left_track_speed_ms = 0.0;
    t->right_track_speed_ms = 0.0;
    t->left_track_phase_m = 0.0;
    t->right_track_phase_m = 0.0;
    memset(t->road_wheel_compression_m, 0, sizeof(t->road_wheel_compression_m));

    t->current_ammo = AMMO_APFSDS;
    t->ammo_count[AMMO_APFSDS] = 20; t->ammo_max[AMMO_APFSDS] = 20;
    t->ammo_count[AMMO_HEAT]   = 10; t->ammo_max[AMMO_HEAT]   = 10;
    t->ammo_count[AMMO_HE]     = 12; t->ammo_max[AMMO_HE]     = 12;

    t->armor.hull_front   = (ArmorPlate){ "Hull upper glacis", 100.0, 82.0 };
    t->armor.hull_side    = (ArmorPlate){ "Hull side",          25.0,  0.0 };
    t->armor.hull_rear    = (ArmorPlate){ "Hull rear",          25.0, 10.0 };
    t->armor.turret_front = (ArmorPlate){ "Turret front (cheek)", 250.0, 15.0 };
    t->armor.turret_side  = (ArmorPlate){ "Turret side",         60.0,  5.0 };
    t->armor.turret_rear  = (ArmorPlate){ "Turret bustle",       40.0,  0.0 };
    t->armor.roof         = (ArmorPlate){ "Roof",                20.0, 90.0 };

    t->hp = 100.0;
    t->alive = 1;
}

void tank_init_generic_mbt(Tank *t, const char *name, Vec2 pos, double heading_deg)
{
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name, sizeof(t->name) - 1);

    t->position_m = pos;
    t->heading_deg = heading_deg;
    t->speed_ms = 0.0;

    t->mass_kg            = 46000.0;
    t->engine_power_hp    = 1000.0;
    t->accel_fwd_mss      = 1.3;
    t->accel_rev_mss      = 0.8;
    t->brake_decel_mss    = 2.8;
    t->roll_resist_mss    = 0.8;
    t->max_speed_fwd_ms   = 60.0 / 3.6;
    t->max_speed_rev_ms   = 15.0 / 3.6;
    t->max_pivot_rate_dps = 30.0;

    t->turret_azimuth_deg     = 0.0;
    t->turret_traverse_dps    = 35.0;
    t->gun_elevation_deg      = 0.0;
    t->gun_elevation_min_deg  = -6.0;
    t->gun_elevation_max_deg  = 14.0;
    t->gun_elevation_dps      = 8.0;

    t->reload_time_s  = 7.0;
    t->reload_timer_s = 0.0;

    t->module_engine_hp = 100.0;
    t->module_tracks_hp = 100.0;
    t->module_turret_hp  = 100.0;
    t->module_gun_hp     = 100.0;
    t->module_optics_hp  = 60.0;

    t->hull_pitch_deg = 0.0;
    t->hull_roll_deg  = 0.0;
    t->hull_bounce_m  = 0.0;
    t->left_track_speed_ms = 0.0;
    t->right_track_speed_ms = 0.0;
    t->left_track_phase_m = 0.0;
    t->right_track_phase_m = 0.0;
    memset(t->road_wheel_compression_m, 0, sizeof(t->road_wheel_compression_m));

    t->current_ammo = AMMO_APFSDS;
    t->ammo_count[AMMO_APFSDS] = 20; t->ammo_max[AMMO_APFSDS] = 20;
    t->ammo_count[AMMO_HEAT]   = 8;  t->ammo_max[AMMO_HEAT]   = 8;
    t->ammo_count[AMMO_HE]     = 8;  t->ammo_max[AMMO_HE]     = 8;

    t->armor.hull_front   = (ArmorPlate){ "Hull upper glacis", 80.0, 68.0 };
    t->armor.hull_side    = (ArmorPlate){ "Hull side",         20.0,  0.0 };
    t->armor.hull_rear    = (ArmorPlate){ "Hull rear",         20.0,  0.0 };
    t->armor.turret_front = (ArmorPlate){ "Turret front (cheek)", 230.0, 10.0 };
    t->armor.turret_side  = (ArmorPlate){ "Turret side",         45.0, 10.0 };
    t->armor.turret_rear  = (ArmorPlate){ "Turret bustle",       30.0,  0.0 };
    t->armor.roof         = (ArmorPlate){ "Roof",                20.0, 90.0 };

    t->hp = 100.0;
    t->alive = 1;
}

/* Differential/skid steering: a track vehicle turns by running its two
 * tracks at different speeds, not by "steering wheels" the way a wheeled
 * vehicle does. That costs a moving tank speed two ways, both modeled
 * below:
 *   1. engine power that goes into the speed *differential* between the
 *      tracks isn't available to push the vehicle forward, so harder
 *      steering directly eats into your acceleration budget.
 *   2. the tracks scrub sideways against the ground while turning, and
 *      that scrub drag grows with both how hard you're turning and how
 *      fast you're already going -- crank the stick over at speed and
 *      you'll visibly bleed off speed, just like the real thing.
 * At a standstill neither term does anything (nothing to divert power
 * from, nothing to scrub), so neutral/pivot turns are still full-rate. */
#define TURN_POWER_SHARE_MAX   0.50 /* fraction of forward accel diverted to the differential at full steer lock */
#define TURN_SCRUB_DECEL_MSS   0.45 /* extra decel (m/s^2) per unit |steer| * |speed_ms| while turning */

void tank_update_movement(Tank *t, double throttle, double steer, double terrain_grip, double dt)
{
    throttle = clampd(throttle, -1.0, 1.0);
    steer    = clampd(steer, -1.0, 1.0);
    if (terrain_grip <= 0.0) terrain_grip = 1.0;

    int engine_destroyed = t->module_engine_hp <= 0.0;
    int tracks_destroyed = t->module_tracks_hp <= 0.0;

    double old_speed = t->speed_ms;
    double engine_factor = engine_destroyed ? 0.0 : clampd(t->module_engine_hp / 100.0, 0.0, 1.0);
    double track_factor = tracks_destroyed ? 0.0 : clampd(t->module_tracks_hp / 100.0, 0.0, 1.0);

    if (tracks_destroyed) {
        throttle = 0.0;
        steer = 0.0;
        t->speed_ms *= 0.35;
        if (fabs(t->speed_ms) < 0.02) {
            t->speed_ms = 0.0;
        }
    }

    int braking = (throttle > 0.0 && old_speed < -0.05) ||
                  (throttle < 0.0 && old_speed >  0.05);

    double turn_power_share = TURN_POWER_SHARE_MAX * fabs(steer);

    double accel = 0.0;
    if (!braking) {
        if (!engine_destroyed) {
            if (throttle > 0.0)      accel = throttle * t->accel_fwd_mss * terrain_grip * (0.25 + 0.75 * engine_factor) * (1.0 - turn_power_share);
            else if (throttle < 0.0) accel = throttle * t->accel_rev_mss * terrain_grip * (0.25 + 0.75 * engine_factor) * (1.0 - turn_power_share);
        }
    } else {
        accel = (old_speed > 0.0) ? -t->brake_decel_mss * (0.5 + 0.5 * track_factor)
                                  :  t->brake_decel_mss * (0.5 + 0.5 * track_factor);
    }

    double resist = 0.0;
    if (fabs(throttle) < 1e-6 || braking) {
        if (old_speed > 0.0)      resist = -t->roll_resist_mss;
        else if (old_speed < 0.0) resist =  t->roll_resist_mss;
    }

    /* track scrub drag: opposes whichever way the tank is already
     * moving (it's drag, not a directional push), scales with speed so
     * it's negligible while crawling but real once you're moving fast */
    double scrub_decel = 0.0;
    if (fabs(old_speed) > 0.05) {
        double scrub_mag = TURN_SCRUB_DECEL_MSS * fabs(steer) * fabs(old_speed) * terrain_grip;
        scrub_decel = (old_speed > 0.0) ? -scrub_mag : scrub_mag;
    }

    double new_speed = old_speed + (accel + resist + scrub_decel) * dt;

    /* don't let resistance/braking/scrub fling the tank into reverse on its own */
    if ((old_speed > 0.0 && new_speed < 0.0) || (old_speed < 0.0 && new_speed > 0.0)) {
        if (fabs(throttle) < 1e-6 || braking || fabs(steer) > 1e-6) new_speed = 0.0;
    }

    double max_fwd = t->max_speed_fwd_ms * terrain_grip * (0.2 + 0.8 * engine_factor) * (0.4 + 0.6 * track_factor);
    double max_rev = t->max_speed_rev_ms * terrain_grip * (0.2 + 0.8 * engine_factor) * (0.4 + 0.6 * track_factor);
    if (engine_destroyed) {
        max_fwd = 0.0;
        max_rev = 0.0;
    }
    t->speed_ms = clampd(new_speed, -max_rev, max_fwd);

    double speed_ratio = fabs(t->speed_ms) / (t->max_speed_fwd_ms > 0 ? t->max_speed_fwd_ms : 1.0);
    double turn_factor = clampd(1.0 - 0.5 * speed_ratio, 0.5, 1.0);
    if (tracks_destroyed) {
        turn_factor *= 0.15;
    }
    double turn_rate_dps = steer * t->max_pivot_rate_dps * turn_factor * (0.3 + 0.7 * track_factor);

    t->heading_deg = norm_deg(t->heading_deg + turn_rate_dps * dt);

    /* Approximate the two track speeds from the hull speed plus the
     * current turn rate. This gives the visual belt motion a real link
     * to the skid-steer physics: when the tank pivots, the inside track
     * slows down and the outside track speeds up. */
    const double track_half_gauge_m = 1.9;
    double angular_speed_rps = deg2rad(turn_rate_dps);
    double track_offset_ms = angular_speed_rps * track_half_gauge_m;
    if (tracks_destroyed) {
        t->left_track_speed_ms = 0.0;
        t->right_track_speed_ms = 0.0;
    } else {
        t->left_track_speed_ms = t->speed_ms - track_offset_ms;
        t->right_track_speed_ms = t->speed_ms + track_offset_ms;
    }
    t->left_track_phase_m += t->left_track_speed_ms * dt;
    t->right_track_phase_m += t->right_track_speed_ms * dt;

    double heading_rad = deg2rad(t->heading_deg);
    Vec2 forward_dir = vec2_make(cos(heading_rad), sin(heading_rad));
    t->position_m = vec2_add(t->position_m, vec2_scale(forward_dir, t->speed_ms * dt));

    t->moved_this_tick = (fabs(t->speed_ms) > 0.05) || (fabs(steer) > 0.01);
}

void tank_update_turret(Tank *t, double turret_input, double elevation_input, double dt)
{
    turret_input    = clampd(turret_input, -1.0, 1.0);
    elevation_input = clampd(elevation_input, -1.0, 1.0);

    double turret_factor = (t->module_turret_hp <= 0.0) ? 0.0 : clampd(t->module_turret_hp / 100.0, 0.0, 1.0);
    double gun_factor = (t->module_gun_hp <= 0.0) ? 0.0 : clampd(t->module_gun_hp / 100.0, 0.0, 1.0);

    double daz = turret_input * t->turret_traverse_dps * (0.2 + 0.8 * turret_factor) * dt;
    t->turret_azimuth_deg = norm_deg(t->turret_azimuth_deg + daz);

    double delev = elevation_input * t->gun_elevation_dps * (0.2 + 0.8 * gun_factor) * dt;
    t->gun_elevation_deg = clampd(t->gun_elevation_deg + delev,
                                   t->gun_elevation_min_deg,
                                   t->gun_elevation_max_deg);

    t->turret_moved_this_tick = (fabs(turret_input) > 0.01) || (fabs(elevation_input) > 0.01);
}

void tank_tick_reload(Tank *t, double dt)
{
    if (t->reload_timer_s > 0.0) {
        t->reload_timer_s -= dt;
        if (t->reload_timer_s < 0.0) t->reload_timer_s = 0.0;
    }
}

double tank_absolute_turret_azimuth_deg(const Tank *t)
{
    return norm_deg(t->heading_deg + t->turret_azimuth_deg);
}

double tank_speed_kmh(const Tank *t)
{
    return t->speed_ms * 3.6;
}

int tank_can_fire(const Tank *t)
{
    return t->alive && t->module_gun_hp > 5.0 && t->module_turret_hp > 5.0;
}
