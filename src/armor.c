#include "armor.h"
#include <math.h>

Vec3 armor_plate_world_normal(double parent_yaw_deg, double face_offset_deg, double slope_deg)
{
    double yaw = deg2rad(parent_yaw_deg + face_offset_deg);
    double slope = deg2rad(slope_deg);

    /* local normal: at slope=0 it points straight out horizontally,
     * at slope=90 it points straight up (roof-like) */
    double nx_local = cos(slope);
    double nz_local = sin(slope);

    Vec3 n;
    n.x = nx_local * cos(yaw);
    n.y = nx_local * sin(yaw);
    n.z = nz_local;
    return n;
}

double armor_effective_thickness(const ArmorPlate *plate, Vec3 plate_normal,
                                  Vec3 impact_dir, double normalization_deg,
                                  double *out_obliquity_deg)
{
    Vec3 dir = vec3_normalize(impact_dir);
    Vec3 n = vec3_normalize(plate_normal);

    /* angle between the *reversed* travel direction and the plate normal:
     * a round arriving head-on into the plate has dir roughly opposite n,
     * so -dir roughly equals n, giving obliquity ~ 0. */
    Vec3 neg_dir = vec3_make(-dir.x, -dir.y, -dir.z);
    double cos_theta = vec3_dot(neg_dir, n);
    cos_theta = clampd(cos_theta, -1.0, 1.0);
    double obliquity_deg = rad2deg(acos(cos_theta));

    /* apply normalization: shave the angle down before it counts */
    double effective_angle = obliquity_deg - normalization_deg;
    if (effective_angle < 0.0) effective_angle = 0.0;
    if (effective_angle > 89.0) effective_angle = 89.0; /* avoid blow-up at grazing */

    if (out_obliquity_deg) *out_obliquity_deg = obliquity_deg;

    double cos_eff = cos(deg2rad(effective_angle));
    if (cos_eff < 0.02) cos_eff = 0.02; /* cap line-of-sight multiplier at 50x */

    return plate->thickness_mm / cos_eff;
}
