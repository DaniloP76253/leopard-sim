use super::vecmath::{clampd, Vec2, Vec3};

fn clamp_unit(value: f64) -> f64 {
    clampd(value, 0.0, 1.0)
}

pub fn terrain_height_at(pos: Vec2) -> f64 {
    let ridge = 1.6 * (pos.x * 0.0024).sin() + 1.2 * (pos.y * 0.0018).cos();
    let rolling = 1.0 * ((pos.x + pos.y) * 0.0012).sin() + 0.7 * ((pos.x - pos.y) * 0.0011).cos();

    let mut hill_a = 0.0;
    let mut hill_b = 0.0;
    let mut hill_c = 0.0;

    let a = Vec2::new(240.0, 120.0);
    let b = Vec2::new(520.0, -260.0);
    let c = Vec2::new(860.0, 260.0);

    let da = pos.dist(a);
    let db = pos.dist(b);
    let dc = pos.dist(c);

    if da < 180.0 {
        hill_a = (1.0 - da / 180.0) * 3.5;
    }
    if db < 220.0 {
        hill_b = (1.0 - db / 220.0) * 2.8;
    }
    if dc < 260.0 {
        hill_c = (1.0 - dc / 260.0) * 2.1;
    }

    let mut berm = 0.0;
    if pos.x > 420.0 && pos.x < 780.0 && pos.y > 40.0 && pos.y < 140.0 {
        berm = 1.4;
    }

    ridge + rolling + hill_a + hill_b + hill_c + berm
}

/// Height used by the Godot terrain mesh and all world-space objects.
/// The logical height is remapped to the terrain mesh's 0..20 m range.
pub fn terrain_world_height_at(pos: Vec2) -> f64 {
    let logical = terrain_height_at(pos);
    clampd((logical + 5.0) / 12.0, 0.0, 1.0) * 20.0
}

pub fn terrain_surface_slope_at(pos: Vec2) -> f64 {
    const STEP: f64 = 1.5;
    let h_l = terrain_height_at(Vec2::new(pos.x - STEP, pos.y));
    let h_r = terrain_height_at(Vec2::new(pos.x + STEP, pos.y));
    let h_d = terrain_height_at(Vec2::new(pos.x, pos.y - STEP));
    let h_u = terrain_height_at(Vec2::new(pos.x, pos.y + STEP));

    let dx = (h_r - h_l) / (2.0 * STEP);
    let dy = (h_u - h_d) / (2.0 * STEP);
    (dx * dx + dy * dy).sqrt()
}

pub fn terrain_grip_at(pos: Vec2) -> f64 {
    let slope = terrain_surface_slope_at(pos);
    let mut grip = 1.0 - slope * 0.9;
    let dist_to_mud = pos.dist(Vec2::new(300.0, -150.0));
    if dist_to_mud < 120.0 {
        grip *= 0.55;
    }
    clamp_unit(grip)
}

pub fn terrain_line_blocked(from: Vec3, to: Vec3) -> bool {
    const STEPS: i32 = 64;
    for i in 1..STEPS {
        let t = (i as f64) / (STEPS as f64);
        let xy = Vec2::new(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t);
        let z = from.z + (to.z - from.z) * t;
        if terrain_world_height_at(xy) + 0.05 > z {
            return true;
        }
    }
    false
}

