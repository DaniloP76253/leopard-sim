use super::ammo::{AmmoStats, AmmoTypeId};
use super::vecmath::{deg2rad, Vec2, Vec3, GRAVITY_MSS};

#[derive(Debug, Clone, Copy)]
pub struct Projectile {
    pub pos: Vec2,
    pub z: f64,
    pub vel: Vec2,
    pub vz: f64,
    pub time_s: f64,
    pub distance_traveled_m: f64,
    pub ammo_type: AmmoTypeId,
    pub active: bool,
}

impl Projectile {
    pub fn new() -> Self {
        Self {
            pos: Vec2::new(0.0, 0.0),
            z: 0.0,
            vel: Vec2::new(0.0, 0.0),
            vz: 0.0,
            time_s: 0.0,
            distance_traveled_m: 0.0,
            ammo_type: AmmoTypeId::Apfsds,
            active: false,
        }
    }

    pub fn launch(
        &mut self,
        origin_xy: Vec2,
        origin_z: f64,
        azimuth_deg: f64,
        elevation_deg: f64,
        muzzle_velocity_ms: f64,
        ammo_type: AmmoTypeId,
    ) {
        self.pos = origin_xy;
        self.z = origin_z;

        let az = deg2rad(azimuth_deg);
        let el = deg2rad(elevation_deg);
        let horiz_speed = muzzle_velocity_ms * el.cos();

        self.vel = Vec2::new(horiz_speed * az.cos(), horiz_speed * az.sin());
        self.vz = muzzle_velocity_ms * el.sin();

        self.time_s = 0.0;
        self.distance_traveled_m = 0.0;
        self.ammo_type = ammo_type;
        self.active = true;
    }

    pub fn step(&mut self, ammo: &AmmoStats, dt: f64) -> bool {
        if !self.active {
            return false;
        }

        let speed = (self.vel.x * self.vel.x + self.vel.y * self.vel.y + self.vz * self.vz).sqrt();
        let ds = speed * dt;

        let drag_factor = (-ammo.drag_decel_per_m * ds).exp();
        self.vel = self.vel.scale(drag_factor);
        self.vz *= drag_factor;

        self.vz -= GRAVITY_MSS * dt;

        self.pos = self.pos + self.vel.scale(dt);
        self.z += self.vz * dt;

        self.distance_traveled_m += ds;
        self.time_s += dt;

        true
    }

    pub fn velocity3(&self) -> Vec3 {
        Vec3::new(self.vel.x, self.vel.y, self.vz)
    }
}

