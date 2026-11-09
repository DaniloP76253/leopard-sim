use super::tank::Tank;
use super::vecmath::{clampd, deg2rad, norm_deg, rad2deg, Vec2, GRAVITY_MSS};

pub const AI_HP_RETREAT_THRESHOLD: f64 = 35.0;
pub const AI_RANGE_MARGIN_M: f64 = 60.0;
pub const AI_FLANK_PERIOD_S: f64 = 6.0;
pub const AI_AIM_FIRE_AZ_TOL_DEG: f64 = 1.5;
pub const AI_AIM_FIRE_EL_TOL_DEG: f64 = 1.0;

#[derive(Debug, Clone, Copy, Default)]
pub struct AiCommand {
    pub throttle: f64,
    pub steer: f64,
    pub turret_input: f64,
    pub elevation_input: f64,
    pub wants_to_fire: bool,
}

#[derive(Debug, Clone, Copy)]
pub struct AiState {
    pub preferred_range_m: f64,
    pub flank_sign: f64,
    pub flank_retime_s: f64,
}

impl AiState {
    pub fn new(preferred_range_m: f64) -> Self {
        Self {
            preferred_range_m,
            flank_sign: if fastrand::bool() { 1.0 } else { -1.0 },
            flank_retime_s: AI_FLANK_PERIOD_S,
        }
    }
}

fn estimate_flight_time(range_m: f64, muzzle_speed_ms: f64, gun_elev_deg: f64) -> f64 {
    let mut horiz_speed = muzzle_speed_ms * deg2rad(gun_elev_deg).cos();
    if horiz_speed < 1.0 {
        horiz_speed = 1.0;
    }
    range_m / horiz_speed
}

pub fn ai_compute_command(
    state: &mut AiState,
    self_tank: &Tank,
    enemy: &Tank,
    dt: f64,
) -> AiCommand {
    let mut cmd = AiCommand::default();
    if !self_tank.alive || !enemy.alive {
        return cmd;
    }

    // 1. Gunnery: lead the target, solve elevation
    let range_now = self_tank.position_m.dist(enemy.position_m);

    let enemy_heading_rad = deg2rad(enemy.heading_deg);
    let enemy_vel = Vec2::new(
        enemy_heading_rad.cos() * enemy.speed_ms,
        enemy_heading_rad.sin() * enemy.speed_ms,
    );

    let ammo = self_tank.current_ammo.stats();
    let lead_time = estimate_flight_time(
        range_now,
        ammo.muzzle_velocity_ms,
        self_tank.gun_elevation_deg,
    );
    let aim_point = enemy.position_m.add(enemy_vel.scale(lead_time));

    let aim_flat = aim_point.sub(self_tank.position_m);
    let range_m = aim_flat.length();
    let desired_azimuth = norm_deg(rad2deg(aim_flat.y.atan2(aim_flat.x)) - self_tank.heading_deg);
    let azimuth_error = norm_deg(desired_azimuth - self_tank.turret_azimuth_deg);
    cmd.turret_input = clampd(
        azimuth_error / (self_tank.turret_traverse_dps * dt + 1e-6),
        -1.0,
        1.0,
    );

    let v = ammo.muzzle_velocity_ms;
    let sin2theta = clampd((range_m * GRAVITY_MSS) / (v * v), -1.0, 1.0);
    let mut desired_elev = 0.5 * rad2deg(sin2theta.asin());
    desired_elev = clampd(
        desired_elev,
        self_tank.gun_elevation_min_deg,
        self_tank.gun_elevation_max_deg,
    );
    let elev_error = desired_elev - self_tank.gun_elevation_deg;
    cmd.elevation_input = clampd(
        elev_error / (self_tank.gun_elevation_dps * dt + 1e-6),
        -1.0,
        1.0,
    );

    cmd.wants_to_fire = (azimuth_error.abs() < AI_AIM_FIRE_AZ_TOL_DEG)
        && (elev_error.abs() < AI_AIM_FIRE_EL_TOL_DEG);

    // 2. Driving: retreat low, hold range, flank otherwise
    let heading_to_enemy = rad2deg(
        (enemy.position_m.y - self_tank.position_m.y)
            .atan2(enemy.position_m.x - self_tank.position_m.x),
    );
    let heading_error = norm_deg(heading_to_enemy - self_tank.heading_deg);

    state.flank_retime_s -= dt;
    if state.flank_retime_s <= 0.0 {
        state.flank_sign = -state.flank_sign;
        state.flank_retime_s = AI_FLANK_PERIOD_S;
    }

    if self_tank.hp < AI_HP_RETREAT_THRESHOLD {
        cmd.throttle = -0.8;
        cmd.steer = clampd(-heading_error / 90.0, -1.0, 1.0);
    } else if range_now > state.preferred_range_m + AI_RANGE_MARGIN_M {
        cmd.throttle = 0.7;
        cmd.steer = clampd(heading_error / 60.0, -1.0, 1.0);
    } else if range_now < state.preferred_range_m - AI_RANGE_MARGIN_M {
        cmd.throttle = -0.5;
        cmd.steer = clampd(-heading_error / 60.0, -1.0, 1.0);
    } else {
        let tangent_heading = heading_to_enemy + 90.0 * state.flank_sign;
        let tangent_error = norm_deg(tangent_heading - self_tank.heading_deg);
        cmd.throttle = 0.5;
        cmd.steer = clampd(tangent_error / 60.0, -1.0, 1.0);
    }

    cmd
}

