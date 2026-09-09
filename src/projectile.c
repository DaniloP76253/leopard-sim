#include "projectile.h"
#include <math.h>

#define GRAVITY_MSS 9.81

void projectile_launch(Projectile *p, Vec2 origin_xy, double origin_z,
                        double azimuth_deg, double elevation_deg,
                        double muzzle_velocity_ms, AmmoTypeId ammo_type)
{
    p->pos = origin_xy;
    p->z = origin_z;

    double az = deg2rad(azimuth_deg);
    double el = deg2rad(elevation_deg);
    double horiz_speed = muzzle_velocity_ms * cos(el);

    p->vel = vec2_make(horiz_speed * cos(az), horiz_speed * sin(az));
    p->vz  = muzzle_velocity_ms * sin(el);

    p->time_s = 0.0;
    p->distance_traveled_m = 0.0;
    p->ammo_type = ammo_type;
    p->active = 1;
}

int projectile_step(Projectile *p, const AmmoStats *ammo, double dt)
{
    if (!p->active) return 0;

    double speed = sqrt(p->vel.x * p->vel.x + p->vel.y * p->vel.y + p->vz * p->vz);
    double ds = speed * dt;

    /* aerodynamic drag: shave speed proportional to arc-length traveled */
    double drag_factor = exp(-ammo->drag_decel_per_m * ds);
    p->vel = vec2_scale(p->vel, drag_factor);
    p->vz *= drag_factor;

    /* gravity acts independently of drag */
    p->vz -= GRAVITY_MSS * dt;

    /* integrate */
    p->pos = vec2_add(p->pos, vec2_scale(p->vel, dt));
    p->z += p->vz * dt;

    p->distance_traveled_m += ds;
    p->time_s += dt;

    return 1;
}

Vec3 projectile_velocity3(const Projectile *p)
{
    return vec3_make(p->vel.x, p->vel.y, p->vz);
}
