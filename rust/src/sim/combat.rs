use super::ammo::AmmoStats;
use super::armor::{armor_effective_thickness, armor_plate_world_normal, ArmorPlate};
use super::projectile::Projectile;
use super::tank::Tank;
use super::terrain::terrain_world_height_at;
use super::vecmath::{clampd, deg2rad, norm_deg, rad2deg, Vec3};

pub const MAX_FLIGHT_TIME_S: f64 = 15.0;
pub const HIT_RADIUS_M: f64 = 2.0;
pub const SILHOUETTE_MAX_Z: f64 = 3.2;
pub const HULL_MAX_Z: f64 = 1.4;
pub const GUN_HEIGHT_M: f64 = 2.2;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ShotOutcome {
    Invalid = 0,
    Miss,
    TerrainBlocked,
    NoPenetration,
    Penetration,
    CatastrophicKill,
}

#[derive(Debug, Clone)]
pub struct CombatShotResult {
    pub outcome: ShotOutcome,
    pub impact_point: Vec3,
    pub range_m: f64,
    pub flight_time_s: f64,
    pub trajectory_points: Vec<Vec3>,
    pub damage_dealt: f64,
    pub face_label: Option<&'static str>,
    pub message: String,
}

impl Default for CombatShotResult {
    fn default() -> Self {
        Self {
            outcome: ShotOutcome::Invalid,
            impact_point: Vec3::new(0.0, 0.0, 0.0),
            range_m: 0.0,
            flight_time_s: 0.0,
            trajectory_points: Vec::new(),
            damage_dealt: 0.0,
            face_label: None,
            message: String::new(),
        }
    }
}

fn frand_symmetric(magnitude: f64) -> f64 {
    (fastrand::f64() * 2.0 - 1.0) * magnitude
}

enum PlateTarget {
    HullFront,
    HullSide,
    HullRear,
    TurretFront,
    GunMantlet,
    TurretSide,
    TurretRear,
    Roof,
}

fn apply_module_damage(
    target: &mut Tank,
    plate_target: PlateTarget,
    ammo: &AmmoStats,
    effective_thickness: f64,
    penetration: f64,
) {
    let margin = penetration - effective_thickness;
    let impact = 22.0 + ammo.he_filler_g / 30.0 + clampd(margin, 0.0, 220.0) * 0.08;

    match plate_target {
        PlateTarget::HullFront => {
            target.module_engine_hp -= impact * 1.2;
            target.module_tracks_hp -= impact * 0.7;
            target.module_optics_hp -= impact * 0.4;
        }
        PlateTarget::HullSide => {
            target.module_tracks_hp -= impact * 1.0;
            target.module_engine_hp -= impact * 0.4;
        }
        PlateTarget::HullRear => {
            target.module_engine_hp -= impact * 1.3;
            target.module_tracks_hp -= impact * 0.4;
        }
        PlateTarget::TurretFront => {
            target.module_turret_hp -= impact * 0.9;
            target.module_gun_hp -= impact * 1.1;
            target.module_optics_hp -= impact * 0.8;
        }
        PlateTarget::GunMantlet => {
            target.module_turret_hp -= impact * 0.8;
            target.module_gun_hp -= impact * 1.2;
            target.module_optics_hp -= impact * 0.4;
        }
        PlateTarget::TurretSide => {
            target.module_turret_hp -= impact * 0.7;
            target.module_gun_hp -= impact * 0.8;
        }
        PlateTarget::TurretRear => {
            target.module_turret_hp -= impact * 1.0;
            target.module_gun_hp -= impact * 0.9;
            target.module_optics_hp -= impact * 0.6;
        }
        PlateTarget::Roof => {
            target.module_gun_hp -= impact * 1.2;
            target.module_optics_hp -= impact * 1.1;
            target.module_turret_hp -= impact * 0.4;
        }
    }

    target.module_engine_hp = clampd(target.module_engine_hp, 0.0, 100.0);
    target.module_tracks_hp = clampd(target.module_tracks_hp, 0.0, 100.0);
    target.module_turret_hp = clampd(target.module_turret_hp, 0.0, 100.0);
    target.module_gun_hp = clampd(target.module_gun_hp, 0.0, 100.0);
    target.module_optics_hp = clampd(target.module_optics_hp, 0.0, 100.0);
}

fn apply_nonpenetration_damage(
    target: &mut Tank,
    plate_target: PlateTarget,
    ammo: &AmmoStats,
) {
    let splash = 2.5 + ammo.he_filler_g / 18.0;

    match plate_target {
        PlateTarget::HullFront => {
            target.module_engine_hp -= splash * 0.8;
            target.module_tracks_hp -= splash * 0.6;
            target.module_optics_hp -= splash * 0.3;
        }
        PlateTarget::HullSide => {
            target.module_tracks_hp -= splash * 0.9;
            target.module_engine_hp -= splash * 0.3;
        }
        PlateTarget::HullRear => {
            target.module_engine_hp -= splash * 1.0;
            target.module_tracks_hp -= splash * 0.3;
        }
        PlateTarget::TurretFront => {
            target.module_turret_hp -= splash * 0.6;
            target.module_gun_hp -= splash * 0.7;
            target.module_optics_hp -= splash * 0.6;
        }
        PlateTarget::GunMantlet => {
            target.module_turret_hp -= splash * 0.5;
            target.module_gun_hp -= splash * 0.8;
            target.module_optics_hp -= splash * 0.3;
        }
        PlateTarget::TurretSide => {
            target.module_turret_hp -= splash * 0.5;
            target.module_gun_hp -= splash * 0.5;
        }
        PlateTarget::TurretRear => {
            target.module_turret_hp -= splash * 0.7;
            target.module_gun_hp -= splash * 0.6;
            target.module_optics_hp -= splash * 0.4;
        }
        PlateTarget::Roof => {
            target.module_gun_hp -= splash * 0.8;
            target.module_optics_hp -= splash * 0.9;
            target.module_turret_hp -= splash * 0.3;
        }
    }

    target.module_engine_hp = clampd(target.module_engine_hp, 0.0, 100.0);
    target.module_tracks_hp = clampd(target.module_tracks_hp, 0.0, 100.0);
    target.module_turret_hp = clampd(target.module_turret_hp, 0.0, 100.0);
    target.module_gun_hp = clampd(target.module_gun_hp, 0.0, 100.0);
    target.module_optics_hp = clampd(target.module_optics_hp, 0.0, 100.0);
}

pub fn combat_fire_shot(shooter: &mut Tank, target: &mut Tank) -> CombatShotResult {
    combat_fire_shot_with_bore(shooter, target, None)
}

pub fn combat_fire_shot_with_bore(
    shooter: &mut Tank,
    target: &mut Tank,
    physical_bore: Option<(Vec3, Vec3)>,
) -> CombatShotResult {
    let mut r = CombatShotResult::default();

    if !target.alive {
        r.message = format!("{} is already destroyed -- nothing to shoot at.", target.name);
        return r;
    }
    if !shooter.can_fire() {
        r.message = format!("{} gun system is disabled -- cannot fire.", shooter.name);
        return r;
    }
    if shooter.reload_timer_s > 0.0 {
        r.message = format!("Reloading... {:.1}s remaining.", shooter.reload_timer_s);
        return r;
    }
    let ammo_idx = shooter.current_ammo.to_index();
    if shooter.ammo_count[ammo_idx] <= 0 {
        r.message = format!("Out of {} rounds!", shooter.current_ammo.stats().short_name);
        return r;
    }

    let ammo = shooter.current_ammo.stats();
    let shooter_ground_z = terrain_world_height_at(shooter.position_m);
    let target_ground_z = terrain_world_height_at(target.position_m);

    let mut dispersion_deg = 0.05;
    if shooter.moved_this_tick {
        dispersion_deg += 0.03 * shooter.speed_kmh().abs() / 10.0;
    }
    if shooter.turret_moved_this_tick {
        dispersion_deg += 0.15;
    }

    let (origin, base_az, base_el) = if let Some((origin, direction)) = physical_bore {
        let direction = direction.normalize();
        let horizontal = (direction.x * direction.x + direction.y * direction.y).sqrt();
        (
            origin,
            rad2deg(direction.y.atan2(direction.x)),
            rad2deg(direction.z.atan2(horizontal)),
        )
    } else {
        (
            Vec3::new(
                shooter.position_m.x,
                shooter.position_m.y,
                shooter_ground_z + GUN_HEIGHT_M,
            ),
            shooter.absolute_turret_azimuth_deg(),
            shooter.gun_elevation_deg,
        )
    };
    let az = base_az + frand_symmetric(dispersion_deg);
    let el = base_el + frand_symmetric(dispersion_deg);

    let mut proj = Projectile::new();
    proj.launch(
        super::vecmath::Vec2::new(origin.x, origin.y),
        origin.z,
        az,
        el,
        ammo.muzzle_velocity_ms,
        shooter.current_ammo,
    );

    let mut trajectory_points = vec![origin];
    let mut trajectory_step = 0_u32;

    let mut closest_dist = 1e9;
    let mut hit = false;
    let mut hit_z = 0.0;
    let mut hit_position = super::vecmath::Vec2::new(0.0, 0.0);
    let mut hit_range_m = 0.0;
    let mut terrain_blocked = false;

    while proj.active && proj.time_s < MAX_FLIGHT_TIME_S {
        let prev_pos = proj.pos;
        let prev_z = proj.z;
        let prev_dist_traveled = proj.distance_traveled_m;

        proj.step(ammo, 0.01);
        trajectory_step += 1;
        if trajectory_step % 2 == 0 {
            trajectory_points.push(Vec3::new(proj.pos.x, proj.pos.y, proj.z));
        }

        let seg = proj.pos.sub(prev_pos);
        let to_target = target.position_m.sub(prev_pos);
        let seg_len2 = seg.x * seg.x + seg.y * seg.y;
        let mut tparam = 0.0;
        if seg_len2 > 1e-9 {
            tparam = (to_target.x * seg.x + to_target.y * seg.y) / seg_len2;
            tparam = clampd(tparam, 0.0, 1.0);
        }
        let closest_point = prev_pos.add(seg.scale(tparam));
        let d = closest_point.dist(target.position_m);
        let z_at_closest = prev_z + (proj.z - prev_z) * tparam;
        let local_z_at_closest = z_at_closest - target_ground_z;

        if d < closest_dist {
            closest_dist = d;
        }
        if terrain_world_height_at(proj.pos) > proj.z {
            terrain_blocked = true;
            break;
        }
        if d <= HIT_RADIUS_M && local_z_at_closest >= 0.0 && local_z_at_closest <= SILHOUETTE_MAX_Z {
            hit = true;
            hit_z = local_z_at_closest;
            hit_position = closest_point;
            hit_range_m = prev_dist_traveled + tparam * (proj.distance_traveled_m - prev_dist_traveled);
            break;
        }
    }

    let final_position = Vec3::new(proj.pos.x, proj.pos.y, proj.z);
    if trajectory_points.last().copied() != Some(final_position) {
        trajectory_points.push(final_position);
    }
    r.flight_time_s = proj.time_s;
    r.trajectory_points = trajectory_points;

    shooter.ammo_count[ammo_idx] -= 1;
    shooter.reload_timer_s = shooter.reload_time_s;

    if !hit {
        r.range_m = proj.distance_traveled_m;
        r.impact_point = Vec3::new(proj.pos.x, proj.pos.y, proj.z);
        if terrain_blocked {
            r.outcome = ShotOutcome::TerrainBlocked;
            r.message = format!(
                "MISS -- shell struck terrain cover after {:.0}m of flight.",
                proj.distance_traveled_m
            );
        } else {
            r.outcome = ShotOutcome::Miss;
            r.message = format!(
                "MISS -- closest approach {:.1}m from target, shell flew {:.0}m in {:.2}s.",
                closest_dist, proj.distance_traveled_m, proj.time_s
            );
        }
        return r;
    }

    let vx = proj.vel.x;
    let vy = proj.vel.y;
    let vz = proj.vz;
    let horiz_speed = (vx * vx + vy * vy).sqrt();
    let descent_deg = rad2deg((-vz).atan2(if horiz_speed > 1e-6 { horiz_speed } else { 1e-6 }));

    let plate: ArmorPlate;
    let normal: Vec3;
    let face_label: &'static str;
    let plate_target: PlateTarget;

    if descent_deg > 55.0 {
        plate = target.armor.roof;
        normal = Vec3::new(0.0, 0.0, 1.0);
        face_label = "Roof (top attack)";
        plate_target = PlateTarget::Roof;
    } else {
        let is_turret = hit_z > HULL_MAX_Z;
        let parent_yaw = if is_turret {
            target.absolute_turret_azimuth_deg()
        } else {
            target.heading_deg
        };

        let azimuth_travel_deg = rad2deg(vy.atan2(vx));
        let incoming_from_deg = norm_deg(azimuth_travel_deg + 180.0);
        let relative_deg = norm_deg(incoming_from_deg - parent_yaw);
        let abs_rel = relative_deg.abs();

        let face_offset;
        if abs_rel <= 45.0 {
            if is_turret {
                let turret_yaw = deg2rad(parent_yaw);
                let turret_right = super::vecmath::Vec2::new(-turret_yaw.sin(), turret_yaw.cos());
                let hit_offset = hit_position.sub(target.position_m);
                let mantlet_lateral = hit_offset.x * turret_right.x + hit_offset.y * turret_right.y;
                let hits_mantlet = (2.25..=2.95).contains(&hit_z) && mantlet_lateral.abs() <= 0.32;
                if hits_mantlet {
                    plate = target.armor.turret_mantlet;
                    face_label = "Gun mantlet";
                    plate_target = PlateTarget::GunMantlet;
                } else {
                    plate = target.armor.turret_front;
                    face_label = "Turret front";
                    plate_target = PlateTarget::TurretFront;
                }
            } else {
                plate = target.armor.hull_front;
                face_label = "Hull front (glacis)";
                plate_target = PlateTarget::HullFront;
            }
            face_offset = 0.0;
        } else if abs_rel <= 135.0 {
            if is_turret {
                plate = target.armor.turret_side;
                face_label = "Turret side";
                plate_target = PlateTarget::TurretSide;
            } else {
                plate = target.armor.hull_side;
                face_label = "Hull side";
                plate_target = PlateTarget::HullSide;
            }
            face_offset = if relative_deg > 0.0 { 90.0 } else { -90.0 };
        } else {
            if is_turret {
                plate = target.armor.turret_rear;
                face_label = "Turret rear (bustle)";
                plate_target = PlateTarget::TurretRear;
            } else {
                plate = target.armor.hull_rear;
                face_label = "Hull rear";
                plate_target = PlateTarget::HullRear;
            }
            face_offset = 180.0;
        }
        normal = armor_plate_world_normal(parent_yaw, face_offset, plate.slope_deg);
    }

    let impact_dir = Vec3::new(vx, vy, vz);
    let mut obliquity_deg = 0.0;
    let eff_thick = armor_effective_thickness(
        &plate,
        normal,
        impact_dir,
        ammo.normalization_deg,
        Some(&mut obliquity_deg),
    );
    let pen = ammo.penetration_at_range(hit_range_m);

    r.range_m = hit_range_m;
    r.impact_point = Vec3::new(proj.pos.x, proj.pos.y, proj.z);
    r.face_label = Some(face_label);

    if pen >= eff_thick {
        let dmg = if ammo.he_filler_g > 0.0 {
            25.0 + ammo.he_filler_g / 25.0
        } else {
            45.0
        };

        let roll = fastrand::u32(0..100);
        let catastrophic = match plate_target {
            PlateTarget::TurretRear => roll < 40,
            PlateTarget::Roof => roll < 25,
            PlateTarget::TurretFront | PlateTarget::GunMantlet | PlateTarget::HullFront => roll < 8,
            _ => roll < 15,
        };

        if catastrophic {
            target.hp = 0.0;
            target.alive = false;
            r.outcome = ShotOutcome::CatastrophicKill;
            r.damage_dealt = 100.0;
            r.message = format!(
                ">>> PENETRATION! Catastrophic ammo/crew loss -- {} is DESTROYED!",
                target.name
            );
        } else {
            target.hp -= dmg;
            if target.hp <= 0.0 {
                target.hp = 0.0;
                target.alive = false;
            }
            r.outcome = ShotOutcome::Penetration;
            r.damage_dealt = dmg;
            r.message = format!(
                ">>> PENETRATION! {:.0} damage. {} HP: {:.0}/100",
                dmg, target.name, target.hp
            );
        }
        apply_module_damage(target, plate_target, ammo, eff_thick, pen);
    } else {
        r.outcome = ShotOutcome::NoPenetration;
        if ammo.he_filler_g > 0.0 {
            let splash_damage = 6.0 + ammo.he_filler_g / 22.0;
            target.hp -= splash_damage;
            if target.hp <= 0.0 {
                target.hp = 0.0;
                target.alive = false;
            }
            r.damage_dealt = splash_damage;
            apply_nonpenetration_damage(target, plate_target, ammo);
            r.message = format!(
                ">>> NO PENETRATION -- Blast damage from {}. {} HP: {:.0}/100",
                ammo.short_name, target.name, target.hp
            );
        } else if fastrand::u32(0..100) < 10 {
            target.hp -= 2.0;
            if target.hp <= 0.0 {
                target.hp = 0.0;
                target.alive = false;
            }
            r.damage_dealt = 2.0;
            r.message = format!(
                ">>> NO PENETRATION -- Minor spall/optics damage. {} HP: {:.0}/100",
                target.name, target.hp
            );
        } else {
            r.message = format!(
                ">>> NO PENETRATION -- shell defeated (needed {:.0}mm, had {:.0}mm).",
                eff_thick, pen
            );
        }
    }

    r
}
