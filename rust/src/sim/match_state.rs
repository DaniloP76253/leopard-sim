use super::ammo::AMMO_TYPE_COUNT;
use super::tank::Tank;
use super::vecmath::{clampd, deg2rad, norm_deg, rad2deg, Vec2};

#[derive(Debug, Clone, Copy)]
pub struct MatchState {
    pub wave: i32,
    pub kills: i32,
    pub respawn_timer_s: f64,
}

impl Default for MatchState {
    fn default() -> Self {
        Self {
            wave: 1,
            kills: 0,
            respawn_timer_s: 0.0,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct SupplyDepot {
    pub position: Vec2,
    pub radius_m: f64,
    pub heal_per_s: f64,
}

impl MatchState {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn spawn_wave(
        &self,
        name: &str,
        origin: Vec2,
        min_range_m: f64,
        max_range_m: f64,
    ) -> Tank {
        let angle = deg2rad((fastrand::u32(0..360)) as f64);
        let range = min_range_m + fastrand::f64() * (max_range_m - min_range_m);
        let pos = origin.add(Vec2::new(angle.cos() * range, angle.sin() * range));
        let heading = norm_deg(rad2deg(angle) + 180.0);

        let mut enemy = Tank::new_generic_mbt(name, pos, heading);

        let mut reload = enemy.reload_time_s * 0.94f64.powf((self.wave - 1) as f64);
        if reload < 4.0 {
            reload = 4.0;
        }
        enemy.reload_time_s = reload;

        enemy
    }

    pub fn try_resupply(&self, t: &mut Tank, depot: &SupplyDepot, dt: f64) -> bool {
        if !t.alive {
            return false;
        }
        if t.position_m.dist(depot.position) > depot.radius_m {
            return false;
        }

        let mut was_full = t.hp >= 100.0;
        for i in 0..AMMO_TYPE_COUNT {
            if t.ammo_count[i] < t.ammo_max[i] {
                was_full = false;
            }
        }

        for i in 0..AMMO_TYPE_COUNT {
            if t.ammo_count[i] < t.ammo_max[i] {
                t.ammo_count[i] = t.ammo_max[i];
            }
        }
        t.hp = clampd(t.hp + depot.heal_per_s * dt, 0.0, 100.0);

        let mut is_full = t.hp >= 100.0;
        for i in 0..AMMO_TYPE_COUNT {
            if t.ammo_count[i] < t.ammo_max[i] {
                is_full = false;
            }
        }

        !was_full && is_full
    }
}

