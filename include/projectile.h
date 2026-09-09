#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "vecmath.h"
#include "ammo.h"

typedef struct {
    Vec2 pos;      /* horizontal position, meters   */
    double z;      /* height above ground, meters   */
    Vec2 vel;      /* horizontal velocity, m/s       */
    double vz;     /* vertical velocity, m/s (+up)   */
    double time_s;
    double distance_traveled_m;
    AmmoTypeId ammo_type;
    int active;
} Projectile;

void projectile_launch(Projectile *p, Vec2 origin_xy, double origin_z,
                        double azimuth_deg, double elevation_deg,
                        double muzzle_velocity_ms, AmmoTypeId ammo_type);

/* advances the projectile by dt (gravity + simple drag). Returns 1 while
 * still airborne, 0 the step it touches the ground (z <= 0). */
int projectile_step(Projectile *p, const AmmoStats *ammo, double dt);

Vec3 projectile_velocity3(const Projectile *p);

#endif /* PROJECTILE_H */
