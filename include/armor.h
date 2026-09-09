#ifndef ARMOR_H
#define ARMOR_H

#include "vecmath.h"

/*
 * A single flat armor plate: a base (vertical-equivalent) thickness in mm
 * of RHA-equivalent, plus how far the plate is sloped back from vertical.
 * slope_deg = 0   -> plate stands straight up (like a side skirt)
 * slope_deg = 90  -> plate lies flat/horizontal (like a roof)
 *
 * NOTE ON NUMBERS: real composite armor is not a single steel-equivalent
 * number and exact values are classified / disputed even in enthusiast
 * circles. These are illustrative placeholders in the right ballpark --
 * edit them in tank.c where each ArmorProfile is built.
 */
typedef struct {
    const char *name;
    double thickness_mm;
    double slope_deg;
} ArmorPlate;

typedef struct {
    ArmorPlate hull_front;   /* upper glacis                */
    ArmorPlate hull_side;
    ArmorPlate hull_rear;
    ArmorPlate turret_front;
    ArmorPlate turret_side;
    ArmorPlate turret_rear;
    ArmorPlate roof;         /* hull/turret roof, top attack */
} ArmorProfile;

/* World-space outward normal of a plate given the facing (yaw) of the
 * hull/turret it belongs to, and an angular offset for which face
 * (0 = front, 90 = right side, -90 = left side, 180 = rear). */
Vec3 armor_plate_world_normal(double parent_yaw_deg, double face_offset_deg, double slope_deg);

/* Effective (line-of-sight) thickness for a projectile hitting a plate
 * whose outward normal is plate_normal, with incoming travel direction
 * impact_dir (does not need to be normalized), given how much "free"
 * normalization angle (degrees) the round gets before the angle counts
 * fully against it. Returns effective thickness in mm and writes the
 * resulting obliquity angle (post-normalization, degrees from normal)
 * into *out_obliquity_deg if non-NULL. */
double armor_effective_thickness(const ArmorPlate *plate, Vec3 plate_normal,
                                  Vec3 impact_dir, double normalization_deg,
                                  double *out_obliquity_deg);

#endif /* ARMOR_H */
