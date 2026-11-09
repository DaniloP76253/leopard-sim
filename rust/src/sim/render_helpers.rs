use super::tank::Tank;
use super::terrain::{terrain_height_at, terrain_line_blocked};
use super::vecmath::{clampd, deg2rad, norm_deg, rad2deg, Vec2, Vec3};

pub const PLAYER_HEIGHT: f32 = 1.45;
pub const TURRET_MOUSE_SENS_DEG_PER_PX: f64 = 0.07;
pub const ELEVATION_MOUSE_SENS_DEG_PER_PX: f64 = 0.05;

pub fn lerpf(from: f32, to: f32, t: f32) -> f32 {
    from + (to - from) * t
}

pub fn terrain_visual_height_at(pos: Vec2) -> f32 {
    let logical = terrain_height_at(pos);
    let normalized = clampd((logical + 5.0) / 12.0, 0.0, 1.0);
    (normalized * 20.0) as f32
}

pub fn update_chassis_pose(t: &mut Tank) {
    let heading = deg2rad(t.heading_deg);
    let forward = Vec2::new(heading.cos(), heading.sin());
    let right = Vec2::new(-heading.sin(), heading.cos());

    let center = terrain_height_at(t.position_m);
    let front = terrain_height_at(t.position_m.add(forward.scale(3.0)));
    let back = terrain_height_at(t.position_m.add(forward.scale(-3.0)));
    let left = terrain_height_at(t.position_m.add(right.scale(-2.2)));
    let right_h = terrain_height_at(t.position_m.add(right.scale(2.2)));

    let target_pitch = rad2deg((front - back).atan2(6.0));
    let target_roll = rad2deg((right_h - left).atan2(4.4));

    t.hull_pitch_deg = lerpf(t.hull_pitch_deg as f32, target_pitch as f32, 0.12) as f64;
    t.hull_roll_deg = lerpf(t.hull_roll_deg as f32, target_roll as f32, 0.12) as f64;
    t.hull_bounce_m = lerpf(
        t.hull_bounce_m as f32,
        ((center - center.floor()) * 0.15) as f32,
        0.08,
    ) as f64;
}

pub fn turret_has_line_of_sight(shooter: &Tank, target: &Tank) -> bool {
    let shooter_gun = Vec3::new(
        shooter.position_m.x,
        shooter.position_m.y,
        terrain_height_at(shooter.position_m) + PLAYER_HEIGHT as f64 + 0.4,
    );
    let target_center = Vec3::new(
        target.position_m.x,
        target.position_m.y,
        terrain_height_at(target.position_m) + PLAYER_HEIGHT as f64 + 0.5,
    );
    !terrain_line_blocked(shooter_gun, target_center)
}

pub fn turret_input_toward(
    player: &Tank,
    desired_az_rel_deg: f64,
    desired_el_deg: f64,
    dt: f64,
) -> (f64, f64) {
    let az_error = norm_deg(desired_az_rel_deg - player.turret_azimuth_deg);
    let turret_input = clampd(
        az_error / (player.turret_traverse_dps * dt + 1e-6),
        -1.0,
        1.0,
    );

    let el_error = desired_el_deg - player.gun_elevation_deg;
    let elev_input = clampd(
        el_error / (player.gun_elevation_dps * dt + 1e-6),
        -1.0,
        1.0,
    );

    (turret_input, elev_input)
}

pub fn tank_boresight_dir(t: &Tank) -> Vec3 {
    let az = deg2rad(t.absolute_turret_azimuth_deg());
    let el = deg2rad(t.gun_elevation_deg);
    Vec3::new(el.cos() * az.cos(), el.sin(), el.cos() * az.sin())
}
