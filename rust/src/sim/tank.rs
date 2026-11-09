use super::ammo::{AmmoTypeId, AMMO_TYPE_COUNT};
use super::armor::{ArmorPlate, ArmorProfile};
use super::vecmath::{clampd, deg2rad, norm_deg, Vec2};

pub const TURN_POWER_SHARE_MAX: f64 = 0.50;
pub const TURN_SCRUB_DECEL_MSS: f64 = 0.45;

#[derive(Debug, Clone)]
pub struct Tank {
    pub name: String,

    pub position_m: Vec2,
    pub heading_deg: f64,
    pub speed_ms: f64,

    pub mass_kg: f64,
    pub engine_power_hp: f64,
    pub accel_fwd_mss: f64,
    pub accel_rev_mss: f64,
    pub brake_decel_mss: f64,
    pub roll_resist_mss: f64,
    pub max_speed_fwd_ms: f64,
    pub max_speed_rev_ms: f64,
    pub max_pivot_rate_dps: f64,

    pub turret_azimuth_deg: f64,
    pub turret_traverse_dps: f64,
    pub gun_elevation_deg: f64,
    pub gun_elevation_min_deg: f64,
    pub gun_elevation_max_deg: f64,
    pub gun_elevation_dps: f64,

    pub reload_time_s: f64,
    pub reload_timer_s: f64,

    pub module_engine_hp: f64,
    pub module_tracks_hp: f64,
    pub module_turret_hp: f64,
    pub module_gun_hp: f64,
    pub module_optics_hp: f64,

    pub hull_pitch_deg: f64,
    pub hull_roll_deg: f64,
    pub hull_bounce_m: f64,
    pub left_track_speed_ms: f64,
    pub right_track_speed_ms: f64,
    pub left_track_phase_m: f64,
    pub right_track_phase_m: f64,
    pub road_wheel_compression_m: [[f64; 5]; 2],

    pub current_ammo: AmmoTypeId,
    pub ammo_count: [i32; AMMO_TYPE_COUNT],
    pub ammo_max: [i32; AMMO_TYPE_COUNT],

    pub armor: ArmorProfile,

    pub hp: f64,
    pub alive: bool,

    pub moved_this_tick: bool,
    pub turret_moved_this_tick: bool,
}

impl Tank {
    pub fn new_leopard2a7(name: &str, pos: Vec2, heading_deg: f64) -> Self {
        Self {
            name: name.to_string(),
            position_m: pos,
            heading_deg,
            speed_ms: 0.0,

            mass_kg: 67000.0,
            engine_power_hp: 1500.0,
            accel_fwd_mss: 1.1,
            accel_rev_mss: 0.6,
            brake_decel_mss: 3.0,
            roll_resist_mss: 0.8,
            max_speed_fwd_ms: 70.0 / 3.6,
            max_speed_rev_ms: 25.0 / 3.6,
            max_pivot_rate_dps: 25.0,

            turret_azimuth_deg: 0.0,
            turret_traverse_dps: 42.0,
            gun_elevation_deg: 0.0,
            gun_elevation_min_deg: -9.0,
            gun_elevation_max_deg: 20.0,
            gun_elevation_dps: 10.0,

            reload_time_s: 6.0,
            reload_timer_s: 0.0,

            module_engine_hp: 100.0,
            module_tracks_hp: 100.0,
            module_turret_hp: 100.0,
            module_gun_hp: 100.0,
            module_optics_hp: 60.0,

            hull_pitch_deg: 0.0,
            hull_roll_deg: 0.0,
            hull_bounce_m: 0.0,
            left_track_speed_ms: 0.0,
            right_track_speed_ms: 0.0,
            left_track_phase_m: 0.0,
            right_track_phase_m: 0.0,
            road_wheel_compression_m: [[0.0; 5]; 2],

            current_ammo: AmmoTypeId::Apfsds,
            ammo_count: [20, 10, 12],
            ammo_max: [20, 10, 12],

            armor: ArmorProfile {
                hull_front: ArmorPlate::new("Hull upper glacis", 100.0, 82.0),
                hull_side: ArmorPlate::new("Hull side", 25.0, 0.0),
                hull_rear: ArmorPlate::new("Hull rear", 25.0, 10.0),
                turret_front: ArmorPlate::new("Turret front (cheek)", 250.0, 15.0),
                turret_side: ArmorPlate::new("Turret side", 60.0, 5.0),
                turret_rear: ArmorPlate::new("Turret bustle", 40.0, 0.0),
                roof: ArmorPlate::new("Roof", 20.0, 90.0),
            },

            hp: 100.0,
            alive: true,

            moved_this_tick: false,
            turret_moved_this_tick: false,
        }
    }

    pub fn new_generic_mbt(name: &str, pos: Vec2, heading_deg: f64) -> Self {
        Self {
            name: name.to_string(),
            position_m: pos,
            heading_deg,
            speed_ms: 0.0,

            mass_kg: 46000.0,
            engine_power_hp: 1000.0,
            accel_fwd_mss: 1.3,
            accel_rev_mss: 0.8,
            brake_decel_mss: 2.8,
            roll_resist_mss: 0.8,
            max_speed_fwd_ms: 60.0 / 3.6,
            max_speed_rev_ms: 15.0 / 3.6,
            max_pivot_rate_dps: 30.0,

            turret_azimuth_deg: 0.0,
            turret_traverse_dps: 35.0,
            gun_elevation_deg: 0.0,
            gun_elevation_min_deg: -6.0,
            gun_elevation_max_deg: 14.0,
            gun_elevation_dps: 8.0,

            reload_time_s: 7.0,
            reload_timer_s: 0.0,

            module_engine_hp: 100.0,
            module_tracks_hp: 100.0,
            module_turret_hp: 100.0,
            module_gun_hp: 100.0,
            module_optics_hp: 60.0,

            hull_pitch_deg: 0.0,
            hull_roll_deg: 0.0,
            hull_bounce_m: 0.0,
            left_track_speed_ms: 0.0,
            right_track_speed_ms: 0.0,
            left_track_phase_m: 0.0,
            right_track_phase_m: 0.0,
            road_wheel_compression_m: [[0.0; 5]; 2],

            current_ammo: AmmoTypeId::Apfsds,
            ammo_count: [20, 8, 8],
            ammo_max: [20, 8, 8],

            armor: ArmorProfile {
                hull_front: ArmorPlate::new("Hull upper glacis", 80.0, 68.0),
                hull_side: ArmorPlate::new("Hull side", 20.0, 0.0),
                hull_rear: ArmorPlate::new("Hull rear", 20.0, 0.0),
                turret_front: ArmorPlate::new("Turret front (cheek)", 230.0, 10.0),
                turret_side: ArmorPlate::new("Turret side", 45.0, 10.0),
                turret_rear: ArmorPlate::new("Turret bustle", 30.0, 0.0),
                roof: ArmorPlate::new("Roof", 20.0, 90.0),
            },

            hp: 100.0,
            alive: true,

            moved_this_tick: false,
            turret_moved_this_tick: false,
        }
    }

    pub fn update_movement(
        &mut self,
        mut throttle: f64,
        mut steer: f64,
        mut terrain_grip: f64,
        dt: f64,
    ) {
        throttle = clampd(throttle, -1.0, 1.0);
        steer = clampd(steer, -1.0, 1.0);
        if terrain_grip <= 0.0 {
            terrain_grip = 1.0;
        }

        let engine_destroyed = self.module_engine_hp <= 0.0;
        let tracks_destroyed = self.module_tracks_hp <= 0.0;

        let old_speed = self.speed_ms;
        let engine_factor = if engine_destroyed {
            0.0
        } else {
            clampd(self.module_engine_hp / 100.0, 0.0, 1.0)
        };
        let track_factor = if tracks_destroyed {
            0.0
        } else {
            clampd(self.module_tracks_hp / 100.0, 0.0, 1.0)
        };

        if tracks_destroyed {
            throttle = 0.0;
            steer = 0.0;
            self.speed_ms *= 0.35;
            if self.speed_ms.abs() < 0.02 {
                self.speed_ms = 0.0;
            }
        }

        let braking = (throttle > 0.0 && old_speed < -0.05) || (throttle < 0.0 && old_speed > 0.05);
        let turn_power_share = TURN_POWER_SHARE_MAX * steer.abs();

        let mut accel = 0.0;
        if !braking {
            if !engine_destroyed {
                if throttle > 0.0 {
                    accel = throttle
                        * self.accel_fwd_mss
                        * terrain_grip
                        * (0.25 + 0.75 * engine_factor)
                        * (1.0 - turn_power_share);
                } else if throttle < 0.0 {
                    accel = throttle
                        * self.accel_rev_mss
                        * terrain_grip
                        * (0.25 + 0.75 * engine_factor)
                        * (1.0 - turn_power_share);
                }
            }
        } else {
            accel = if old_speed > 0.0 {
                -self.brake_decel_mss * (0.5 + 0.5 * track_factor)
            } else {
                self.brake_decel_mss * (0.5 + 0.5 * track_factor)
            };
        }

        let mut resist = 0.0;
        if throttle.abs() < 1e-6 || braking {
            if old_speed > 0.0 {
                resist = -self.roll_resist_mss;
            } else if old_speed < 0.0 {
                resist = self.roll_resist_mss;
            }
        }

        let mut scrub_decel = 0.0;
        if old_speed.abs() > 0.05 {
            let scrub_mag = TURN_SCRUB_DECEL_MSS * steer.abs() * old_speed.abs() * terrain_grip;
            scrub_decel = if old_speed > 0.0 {
                -scrub_mag
            } else {
                scrub_mag
            };
        }

        let mut new_speed = old_speed + (accel + resist + scrub_decel) * dt;

        if (old_speed > 0.0 && new_speed < 0.0) || (old_speed < 0.0 && new_speed > 0.0) {
            if throttle.abs() < 1e-6 || braking || steer.abs() > 1e-6 {
                new_speed = 0.0;
            }
        }

        let mut max_fwd = self.max_speed_fwd_ms
            * terrain_grip
            * (0.2 + 0.8 * engine_factor)
            * (0.4 + 0.6 * track_factor);
        let mut max_rev = self.max_speed_rev_ms
            * terrain_grip
            * (0.2 + 0.8 * engine_factor)
            * (0.4 + 0.6 * track_factor);
        if engine_destroyed {
            max_fwd = 0.0;
            max_rev = 0.0;
        }
        self.speed_ms = clampd(new_speed, -max_rev, max_fwd);

        let speed_ratio = self.speed_ms.abs()
            / (if self.max_speed_fwd_ms > 0.0 {
                self.max_speed_fwd_ms
            } else {
                1.0
            });
        let mut turn_factor = clampd(1.0 - 0.5 * speed_ratio, 0.5, 1.0);
        if tracks_destroyed {
            turn_factor *= 0.15;
        }
        let turn_rate_dps = steer * self.max_pivot_rate_dps * turn_factor * (0.3 + 0.7 * track_factor);

        self.heading_deg = norm_deg(self.heading_deg + turn_rate_dps * dt);

        let track_half_gauge_m = 1.9;
        let angular_speed_rps = deg2rad(turn_rate_dps);
        let track_offset_ms = angular_speed_rps * track_half_gauge_m;
        if tracks_destroyed {
            self.left_track_speed_ms = 0.0;
            self.right_track_speed_ms = 0.0;
        } else {
            self.left_track_speed_ms = self.speed_ms - track_offset_ms;
            self.right_track_speed_ms = self.speed_ms + track_offset_ms;
        }
        self.left_track_phase_m += self.left_track_speed_ms * dt;
        self.right_track_phase_m += self.right_track_speed_ms * dt;

        let heading_rad = deg2rad(self.heading_deg);
        let forward_dir = Vec2::new(heading_rad.cos(), heading_rad.sin());
        self.position_m = self.position_m + forward_dir.scale(self.speed_ms * dt);

        self.moved_this_tick = (self.speed_ms.abs() > 0.05) || (steer.abs() > 0.01);
    }

    pub fn update_turret(&mut self, turret_input: f64, elevation_input: f64, dt: f64) {
        let turret_in = clampd(turret_input, -1.0, 1.0);
        let elev_in = clampd(elevation_input, -1.0, 1.0);

        let turret_factor = if self.module_turret_hp <= 0.0 {
            0.0
        } else {
            clampd(self.module_turret_hp / 100.0, 0.0, 1.0)
        };
        let gun_factor = if self.module_gun_hp <= 0.0 {
            0.0
        } else {
            clampd(self.module_gun_hp / 100.0, 0.0, 1.0)
        };

        let daz = turret_in * self.turret_traverse_dps * (0.2 + 0.8 * turret_factor) * dt;
        self.turret_azimuth_deg = norm_deg(self.turret_azimuth_deg + daz);

        let delev = elev_in * self.gun_elevation_dps * (0.2 + 0.8 * gun_factor) * dt;
        self.gun_elevation_deg = clampd(
            self.gun_elevation_deg + delev,
            self.gun_elevation_min_deg,
            self.gun_elevation_max_deg,
        );

        self.turret_moved_this_tick = (turret_in.abs() > 0.01) || (elev_in.abs() > 0.01);
    }

    pub fn tick_reload(&mut self, dt: f64) {
        if self.reload_timer_s > 0.0 {
            self.reload_timer_s -= dt;
            if self.reload_timer_s < 0.0 {
                self.reload_timer_s = 0.0;
            }
        }
    }

    pub fn absolute_turret_azimuth_deg(&self) -> f64 {
        norm_deg(self.heading_deg + self.turret_azimuth_deg)
    }

    pub fn speed_kmh(&self) -> f64 {
        self.speed_ms * 3.6
    }

    pub fn can_fire(&self) -> bool {
        self.alive && self.module_gun_hp > 5.0 && self.module_turret_hp > 5.0
    }
}

