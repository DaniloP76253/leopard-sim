#include "ai.h"
#include <math.h>
#include <stdlib.h>

#define AI_HP_RETREAT_THRESHOLD  35.0
#define AI_RANGE_MARGIN_M        60.0
#define AI_FLANK_PERIOD_S         6.0
#define AI_AIM_FIRE_AZ_TOL_DEG    1.5
#define AI_AIM_FIRE_EL_TOL_DEG    1.0

void ai_state_init(AiState *state, double preferred_range_m)
{
    state->preferred_range_m = preferred_range_m;
    state->flank_sign = (rand() % 2) ? 1.0 : -1.0;
    state->flank_retime_s = AI_FLANK_PERIOD_S;
}

/* Rough time-of-flight estimate for a lead calculation: horizontal speed
 * component of the current muzzle velocity at the tank's present gun
 * elevation, ignoring drag. Good enough to lead a moving target; the
 * actual flight (with drag) is simulated properly by projectile.c. */
static double estimate_flight_time(double range_m, double muzzle_speed_ms, double gun_elev_deg)
{
    double horiz_speed = muzzle_speed_ms * cos(deg2rad(gun_elev_deg));
    if (horiz_speed < 1.0) horiz_speed = 1.0;
    return range_m / horiz_speed;
}

AiCommand ai_compute_command(AiState *state, const Tank *self, const Tank *enemy, double dt)
{
    AiCommand cmd = { 0 };
    if (!self->alive || !enemy->alive) return cmd;

    /* ---------------- gunnery: lead the target, solve elevation ---------------- */
    double range_now = vec2_dist(self->position_m, enemy->position_m);

    double enemy_heading_rad = deg2rad(enemy->heading_deg);
    Vec2 enemy_vel = vec2_make(cos(enemy_heading_rad) * enemy->speed_ms,
                                sin(enemy_heading_rad) * enemy->speed_ms);

    const AmmoStats *ammo = ammo_get(self->current_ammo);
    double lead_time = estimate_flight_time(range_now, ammo->muzzle_velocity_ms, self->gun_elevation_deg);
    Vec2 aim_point = vec2_add(enemy->position_m, vec2_scale(enemy_vel, lead_time));

    Vec2 aim_flat = vec2_sub(aim_point, self->position_m);
    double range_m = vec2_length(aim_flat);
    double desired_azimuth = norm_deg(rad2deg(atan2(aim_flat.y, aim_flat.x)) - self->heading_deg);
    double azimuth_error = norm_deg(desired_azimuth - self->turret_azimuth_deg);
    cmd.turret_input = clampd(azimuth_error / (self->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    /* Low-angle solution to the projectile range equation (no drag, flat
     * target height): sin(2*theta) = range * g / v^2. This is exactly the
     * "quick fire solution" simplification real ballistic computers use
     * before applying finer correction -- basic classical mechanics, not
     * anything ammo- or vehicle-specific. */
    double v = ammo->muzzle_velocity_ms;
    double sin2theta = clampd((range_m * GRAVITY_MSS) / (v * v), -1.0, 1.0);
    double desired_elev = 0.5 * rad2deg(asin(sin2theta));
    desired_elev = clampd(desired_elev, self->gun_elevation_min_deg, self->gun_elevation_max_deg);
    double elev_error = desired_elev - self->gun_elevation_deg;
    cmd.elevation_input = clampd(elev_error / (self->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);

    cmd.wants_to_fire = (fabs(azimuth_error) < AI_AIM_FIRE_AZ_TOL_DEG) &&
                        (fabs(elev_error) < AI_AIM_FIRE_EL_TOL_DEG);

    /* ---------------- driving: retreat low, hold range, flank otherwise ---------------- */
    double heading_to_enemy = rad2deg(atan2(enemy->position_m.y - self->position_m.y,
                                             enemy->position_m.x - self->position_m.x));
    double heading_error = norm_deg(heading_to_enemy - self->heading_deg);

    state->flank_retime_s -= dt;
    if (state->flank_retime_s <= 0.0) {
        state->flank_sign = -state->flank_sign;
        state->flank_retime_s = AI_FLANK_PERIOD_S;
    }

    if (self->hp < AI_HP_RETREAT_THRESHOLD) {
        /* shoot-and-scoot: back off while keeping the gun on target */
        cmd.throttle = -0.8;
        cmd.steer = clampd(-heading_error / 90.0, -1.0, 1.0);
    } else if (range_now > state->preferred_range_m + AI_RANGE_MARGIN_M) {
        cmd.throttle = 0.7;
        cmd.steer = clampd(heading_error / 60.0, -1.0, 1.0);
    } else if (range_now < state->preferred_range_m - AI_RANGE_MARGIN_M) {
        cmd.throttle = -0.5;
        cmd.steer = clampd(-heading_error / 60.0, -1.0, 1.0);
    } else {
        /* orbit at range instead of parking nose-on -- a moving target
         * is harder to hit and this reads as far more "alive" */
        double tangent_heading = heading_to_enemy + 90.0 * state->flank_sign;
        double tangent_error = norm_deg(tangent_heading - self->heading_deg);
        cmd.throttle = 0.5;
        cmd.steer = clampd(tangent_error / 60.0, -1.0, 1.0);
    }

    return cmd;
}
