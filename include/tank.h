#ifndef TANK_H
#define TANK_H

#include "vecmath.h"
#include "ammo.h"
#include "armor.h"

typedef struct {
    char name[64];

    /* --- hull kinematics (world space) --- */
    Vec2 position_m;     /* x = east, y = north, meters       */
    double heading_deg;  /* 0 = facing +x, CCW positive         */
    double speed_ms;     /* signed: + forward, - reverse        */

    /* --- mobility stats (editable placeholders, see tank.c) --- */
    double mass_kg;
    double engine_power_hp;
    double accel_fwd_mss;     /* effective forward acceleration      */
    double accel_rev_mss;     /* effective reverse acceleration      */
    double brake_decel_mss;
    double roll_resist_mss;
    double max_speed_fwd_ms;
    double max_speed_rev_ms;
    double max_pivot_rate_dps; /* turn rate at ~0 speed (neutral steer) */

    /* --- turret / gun --- */
    double turret_azimuth_deg;   /* relative to hull                 */
    double turret_traverse_dps;
    double gun_elevation_deg;    /* relative to hull deck, + = up    */
    double gun_elevation_min_deg;
    double gun_elevation_max_deg;
    double gun_elevation_dps;

    double reload_time_s;
    double reload_timer_s;

    /* --- module damage --- */
    double module_engine_hp;
    double module_tracks_hp;
    double module_turret_hp;
    double module_gun_hp;
    double module_optics_hp;

    /* --- suspension / visible chassis pose --- */
    double hull_pitch_deg;
    double hull_roll_deg;
    double hull_bounce_m;
    double left_track_speed_ms;
    double right_track_speed_ms;
    double left_track_phase_m;
    double right_track_phase_m;
    double road_wheel_compression_m[2][5];

    AmmoTypeId current_ammo;
    int ammo_count[AMMO_TYPE_COUNT];
    int ammo_max[AMMO_TYPE_COUNT];   /* starting loadout; what a resupply tops back up to */

    ArmorProfile armor;

    double hp;      /* abstracted crew/systems health, 100 = fully combat capable */
    int alive;

    /* bookkeeping used for gun-handling / dispersion mechanics */
    int moved_this_tick;
    int turret_moved_this_tick;
} Tank;

void tank_init_leopard2a7(Tank *t, const char *name, Vec2 pos, double heading_deg);
void tank_init_generic_mbt(Tank *t, const char *name, Vec2 pos, double heading_deg);

/* throttle in [-1,1] (reverse..forward), steer in [-1,1] (left..right) */
void tank_update_movement(Tank *t, double throttle, double steer, double terrain_grip, double dt);
void tank_update_turret(Tank *t, double turret_input, double elevation_input, double dt);
void tank_tick_reload(Tank *t, double dt);

double tank_absolute_turret_azimuth_deg(const Tank *t);
double tank_speed_kmh(const Tank *t);
int tank_can_fire(const Tank *t);

#endif /* TANK_H */
