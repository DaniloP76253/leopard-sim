#include "combat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ammo.h"
#include "armor.h"
#include "projectile.h"
#include "terrain.h"

#define MAX_FLIGHT_TIME_S 15.0
#define HIT_RADIUS_M      2.0
#define SILHOUETTE_MAX_Z  3.2
#define HULL_MAX_Z        1.4
#define GUN_HEIGHT_M       2.2

static double frand_symmetric(double magnitude)
{
    return ((double)rand() / (double)RAND_MAX * 2.0 - 1.0) * magnitude;
}

static void apply_module_damage(Tank *target, const ArmorPlate *plate, const AmmoStats *ammo,
                                double effective_thickness, double penetration)
{
    double prev_engine = target->module_engine_hp;
    double prev_tracks = target->module_tracks_hp;
    double prev_turret = target->module_turret_hp;
    double prev_gun = target->module_gun_hp;
    double prev_optics = target->module_optics_hp;
    double margin = penetration - effective_thickness;
    double impact = 22.0 + ammo->he_filler_g / 30.0 + clampd(margin, 0.0, 220.0) * 0.08;

    if (plate == &target->armor.hull_front) {
        target->module_engine_hp -= impact * 1.2;
        target->module_tracks_hp -= impact * 0.7;
        target->module_optics_hp -= impact * 0.4;
    } else if (plate == &target->armor.hull_side) {
        target->module_tracks_hp -= impact * 1.0;
        target->module_engine_hp -= impact * 0.4;
    } else if (plate == &target->armor.hull_rear) {
        target->module_engine_hp -= impact * 1.3;
        target->module_tracks_hp -= impact * 0.4;
    } else if (plate == &target->armor.turret_front) {
        target->module_turret_hp -= impact * 0.9;
        target->module_gun_hp -= impact * 1.1;
        target->module_optics_hp -= impact * 0.8;
    } else if (plate == &target->armor.turret_side) {
        target->module_turret_hp -= impact * 0.7;
        target->module_gun_hp -= impact * 0.8;
    } else if (plate == &target->armor.turret_rear) {
        target->module_turret_hp -= impact * 1.0;
        target->module_gun_hp -= impact * 0.9;
        target->module_optics_hp -= impact * 0.6;
    } else if (plate == &target->armor.roof) {
        target->module_gun_hp -= impact * 1.2;
        target->module_optics_hp -= impact * 1.1;
        target->module_turret_hp -= impact * 0.4;
    }

    target->module_engine_hp = clampd(target->module_engine_hp, 0.0, 100.0);
    target->module_tracks_hp = clampd(target->module_tracks_hp, 0.0, 100.0);
    target->module_turret_hp  = clampd(target->module_turret_hp, 0.0, 100.0);
    target->module_gun_hp     = clampd(target->module_gun_hp, 0.0, 100.0);
    target->module_optics_hp  = clampd(target->module_optics_hp, 0.0, 100.0);

    if (prev_engine > 0.0 && target->module_engine_hp <= 0.0) {
        printf("*** %s engine destroyed\n", target->name);
    }
    if (prev_tracks > 0.0 && target->module_tracks_hp <= 0.0) {
        printf("*** %s tracks destroyed\n", target->name);
    }
    if (prev_turret > 0.0 && target->module_turret_hp <= 0.0) {
        printf("*** %s turret drive destroyed\n", target->name);
    }
    if (prev_gun > 0.0 && target->module_gun_hp <= 0.0) {
        printf("*** %s gun destroyed\n", target->name);
    }
    if (prev_optics > 0.0 && target->module_optics_hp <= 0.0) {
        printf("*** %s optics destroyed\n", target->name);
    }
}

static void apply_nonpenetration_damage(Tank *target, const ArmorPlate *plate, const AmmoStats *ammo)
{
    double splash = 2.5 + ammo->he_filler_g / 18.0;

    if (plate == &target->armor.hull_front) {
        target->module_engine_hp -= splash * 0.8;
        target->module_tracks_hp -= splash * 0.6;
        target->module_optics_hp -= splash * 0.3;
    } else if (plate == &target->armor.hull_side) {
        target->module_tracks_hp -= splash * 0.9;
        target->module_engine_hp -= splash * 0.3;
    } else if (plate == &target->armor.hull_rear) {
        target->module_engine_hp -= splash * 1.0;
        target->module_tracks_hp -= splash * 0.3;
    } else if (plate == &target->armor.turret_front) {
        target->module_turret_hp -= splash * 0.6;
        target->module_gun_hp -= splash * 0.7;
        target->module_optics_hp -= splash * 0.6;
    } else if (plate == &target->armor.turret_side) {
        target->module_turret_hp -= splash * 0.5;
        target->module_gun_hp -= splash * 0.5;
    } else if (plate == &target->armor.turret_rear) {
        target->module_turret_hp -= splash * 0.7;
        target->module_gun_hp -= splash * 0.6;
        target->module_optics_hp -= splash * 0.4;
    } else if (plate == &target->armor.roof) {
        target->module_gun_hp -= splash * 0.8;
        target->module_optics_hp -= splash * 0.9;
        target->module_turret_hp -= splash * 0.3;
    }

    target->module_engine_hp = clampd(target->module_engine_hp, 0.0, 100.0);
    target->module_tracks_hp = clampd(target->module_tracks_hp, 0.0, 100.0);
    target->module_turret_hp  = clampd(target->module_turret_hp, 0.0, 100.0);
    target->module_gun_hp     = clampd(target->module_gun_hp, 0.0, 100.0);
    target->module_optics_hp  = clampd(target->module_optics_hp, 0.0, 100.0);
}

void combat_fire_shot(Tank *shooter, Tank *target, CombatShotResult *out_result)
{
    CombatShotResult local_result = {0};
    CombatShotResult *r = out_result ? out_result : &local_result;
    r->outcome = SHOT_OUTCOME_INVALID;

    if (!target->alive) {
        printf("%s is already destroyed -- nothing to shoot at.\n", target->name);
        return;
    }
    if (!tank_can_fire(shooter)) {
        printf("%s gun system is disabled -- cannot fire.\n", shooter->name);
        return;
    }
    if (shooter->reload_timer_s > 0.0) {
        printf("Reloading... %.1fs remaining.\n", shooter->reload_timer_s);
        return;
    }
    if (shooter->ammo_count[shooter->current_ammo] <= 0) {
        printf("Out of %s rounds!\n", ammo_get(shooter->current_ammo)->short_name);
        return;
    }

    const AmmoStats *ammo = ammo_get(shooter->current_ammo);
    double shooter_ground_z = terrain_height_at(shooter->position_m);
    double target_ground_z = terrain_height_at(target->position_m);

    double dispersion_deg = 0.05;
    if (shooter->moved_this_tick)        dispersion_deg += 0.03 * fabs(tank_speed_kmh(shooter)) / 10.0;
    if (shooter->turret_moved_this_tick) dispersion_deg += 0.15;

    double az = tank_absolute_turret_azimuth_deg(shooter) + frand_symmetric(dispersion_deg);
    double el = shooter->gun_elevation_deg + frand_symmetric(dispersion_deg);

    Projectile proj;
    projectile_launch(&proj, shooter->position_m, shooter_ground_z + GUN_HEIGHT_M, az, el, ammo->muzzle_velocity_ms, shooter->current_ammo);

    double closest_dist = 1e9;
    int hit = 0;
    double hit_z = 0.0;
    double hit_range_m = 0.0;
    int terrain_blocked = 0;

    while (proj.active && proj.time_s < MAX_FLIGHT_TIME_S) {
        Vec2 prev_pos = proj.pos;
        double prev_z = proj.z;
        double prev_dist_traveled = proj.distance_traveled_m;

        projectile_step(&proj, ammo, 0.01);

        Vec2 seg = vec2_sub(proj.pos, prev_pos);
        Vec2 to_target = vec2_sub(target->position_m, prev_pos);
        double seg_len2 = seg.x * seg.x + seg.y * seg.y;
        double tparam = 0.0;
        if (seg_len2 > 1e-9) {
            tparam = (to_target.x * seg.x + to_target.y * seg.y) / seg_len2;
            tparam = clampd(tparam, 0.0, 1.0);
        }
        Vec2 closest_point = vec2_add(prev_pos, vec2_scale(seg, tparam));
        double d = vec2_dist(closest_point, target->position_m);
        double z_at_closest = prev_z + (proj.z - prev_z) * tparam;
        double local_z_at_closest = z_at_closest - target_ground_z;

        if (d < closest_dist) closest_dist = d;
        if (terrain_height_at(proj.pos) > proj.z) {
            terrain_blocked = 1;
            break;
        }
        if (d <= HIT_RADIUS_M && local_z_at_closest >= 0.0 && local_z_at_closest <= SILHOUETTE_MAX_Z) {
            hit = 1;
            hit_z = local_z_at_closest;
            hit_range_m = prev_dist_traveled + tparam * (proj.distance_traveled_m - prev_dist_traveled);
            break;
        }
    }

    shooter->ammo_count[shooter->current_ammo]--;
    shooter->reload_timer_s = shooter->reload_time_s;

    printf("\n--- FIRING %s ---\n", ammo->name);

    if (!hit) {
        r->range_m = proj.distance_traveled_m;
        r->impact_point = vec3_make(proj.pos.x, proj.pos.y, proj.z);
        if (terrain_blocked) {
            r->outcome = SHOT_OUTCOME_TERRAIN_BLOCKED;
            printf("MISS -- shell struck terrain cover after %.0fm of flight.\n", proj.distance_traveled_m);
        } else {
            r->outcome = SHOT_OUTCOME_MISS;
            printf("MISS -- closest approach %.1fm from target, shell flew %.0fm in %.2fs.\n",
                   closest_dist, proj.distance_traveled_m, proj.time_s);
        }
        return;
    }

    double vx = proj.vel.x, vy = proj.vel.y, vz = proj.vz;
    double horiz_speed = sqrt(vx * vx + vy * vy);
    double descent_deg = rad2deg(atan2(-vz, horiz_speed > 1e-6 ? horiz_speed : 1e-6));

    const ArmorPlate *plate;
    Vec3 normal;
    const char *face_label;

    if (descent_deg > 55.0) {
        plate = &target->armor.roof;
        normal = vec3_make(0.0, 0.0, 1.0);
        face_label = "Roof (top attack)";
    } else {
        int is_turret = (hit_z > HULL_MAX_Z);
        double parent_yaw = is_turret ? tank_absolute_turret_azimuth_deg(target) : target->heading_deg;

        double azimuth_travel_deg = rad2deg(atan2(vy, vx));
        double incoming_from_deg = norm_deg(azimuth_travel_deg + 180.0);
        double relative_deg = norm_deg(incoming_from_deg - parent_yaw);
        double abs_rel = fabs(relative_deg);

        double face_offset;
        if (abs_rel <= 45.0) {
            plate = is_turret ? &target->armor.turret_front : &target->armor.hull_front;
            face_offset = 0.0;
            face_label = is_turret ? "Turret front" : "Hull front (glacis)";
        } else if (abs_rel <= 135.0) {
            plate = is_turret ? &target->armor.turret_side : &target->armor.hull_side;
            face_offset = (relative_deg > 0.0) ? 90.0 : -90.0;
            face_label = is_turret ? "Turret side" : "Hull side";
        } else {
            plate = is_turret ? &target->armor.turret_rear : &target->armor.hull_rear;
            face_offset = 180.0;
            face_label = is_turret ? "Turret rear (bustle)" : "Hull rear";
        }
        normal = armor_plate_world_normal(parent_yaw, face_offset, plate->slope_deg);
    }

    Vec3 impact_dir = vec3_make(vx, vy, vz);
    double obliquity_deg = 0.0;
    double eff_thick = armor_effective_thickness(plate, normal, impact_dir, ammo->normalization_deg, &obliquity_deg);
    double pen = ammo_penetration_at_range(ammo, hit_range_m);

    printf("Range %.0fm | flight time %.2fs | impact: %s\n", hit_range_m, proj.time_s, face_label);
    printf("Obliquity %.0f deg | plate %.0fmm base -> %.0fmm effective | %s penetration here: %.0fmm\n",
           obliquity_deg, plate->thickness_mm, eff_thick, ammo->short_name, pen);

    r->range_m = hit_range_m;
    r->impact_point = vec3_make(proj.pos.x, proj.pos.y, proj.z);
    r->face_label = face_label;

    if (pen >= eff_thick) {
        double dmg = (ammo->he_filler_g > 0.0) ? 25.0 + ammo->he_filler_g / 25.0 : 45.0;
        int catastrophic = 0;
        int roll = rand() % 100;
        if (plate == &target->armor.turret_rear)      catastrophic = (roll < 40);
        else if (plate == &target->armor.roof)         catastrophic = (roll < 25);
        else if (plate == &target->armor.turret_front || plate == &target->armor.hull_front)
                                                        catastrophic = (roll < 8);
        else                                            catastrophic = (roll < 15);

        printf(">>> PENETRATION!");
        if (catastrophic) {
            target->hp = 0.0;
            printf(" Catastrophic ammo/crew loss -- %s is DESTROYED!\n", target->name);
            r->outcome = SHOT_OUTCOME_CATASTROPHIC_KILL;
            r->damage_dealt = 100.0;
        } else {
            target->hp -= dmg;
            if (target->hp < 0.0) target->hp = 0.0;
            printf(" %.0f damage. %s HP: %.0f/100\n", dmg, target->name, target->hp);
            r->outcome = SHOT_OUTCOME_PENETRATION;
            r->damage_dealt = dmg;
        }
        apply_module_damage(target, plate, ammo, eff_thick, pen);
        printf(" Modules -> engine %.0f, tracks %.0f, turret %.0f, gun %.0f, optics %.0f\n",
               target->module_engine_hp, target->module_tracks_hp,
               target->module_turret_hp, target->module_gun_hp, target->module_optics_hp);
        if (target->hp <= 0.0) target->alive = 0;
    } else {
        printf(">>> NO PENETRATION -- shell defeated (needed %.0fmm, had %.0fmm).\n", eff_thick, pen);
        r->outcome = SHOT_OUTCOME_NO_PENETRATION;
        if (ammo->he_filler_g > 0.0) {
            double splash_damage = 6.0 + ammo->he_filler_g / 22.0;
            target->hp -= splash_damage;
            if (target->hp < 0.0) target->hp = 0.0;
            r->damage_dealt = splash_damage;
            apply_nonpenetration_damage(target, plate, ammo);
            printf("Blast damage from %s. %s HP: %.0f/100\n", ammo->short_name, target->name, target->hp);
            printf(" Modules -> engine %.0f, tracks %.0f, turret %.0f, gun %.0f, optics %.0f\n",
                   target->module_engine_hp, target->module_tracks_hp,
                   target->module_turret_hp, target->module_gun_hp, target->module_optics_hp);
        } else if ((rand() % 100) < 10) {
            target->hp -= 2.0;
            if (target->hp < 0.0) target->hp = 0.0;
            r->damage_dealt = 2.0;
            printf("Minor spall/optics damage. %s HP: %.0f/100\n", target->name, target->hp);
        }
    }
}
