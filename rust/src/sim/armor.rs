use super::vecmath::{clampd, deg2rad, rad2deg, Vec3};

#[derive(Debug, Clone, Copy)]
pub struct ArmorPlate {
    pub name: &'static str,
    pub thickness_mm: f64,
    pub slope_deg: f64,
}

impl ArmorPlate {
    pub const fn new(name: &'static str, thickness_mm: f64, slope_deg: f64) -> Self {
        Self {
            name,
            thickness_mm,
            slope_deg,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ArmorProfile {
    pub hull_front: ArmorPlate,
    pub hull_side: ArmorPlate,
    pub hull_rear: ArmorPlate,
    pub turret_front: ArmorPlate,
    pub turret_mantlet: ArmorPlate,
    pub turret_side: ArmorPlate,
    pub turret_rear: ArmorPlate,
    pub roof: ArmorPlate,
}

pub fn armor_plate_world_normal(
    parent_yaw_deg: f64,
    face_offset_deg: f64,
    slope_deg: f64,
) -> Vec3 {
    let yaw = deg2rad(parent_yaw_deg + face_offset_deg);
    let slope = deg2rad(slope_deg);

    let nx_local = slope.cos();
    let nz_local = slope.sin();

    Vec3::new(nx_local * yaw.cos(), nx_local * yaw.sin(), nz_local)
}

pub fn armor_effective_thickness(
    plate: &ArmorPlate,
    plate_normal: Vec3,
    impact_dir: Vec3,
    normalization_deg: f64,
    out_obliquity_deg: Option<&mut f64>,
) -> f64 {
    let dir = impact_dir.normalize();
    let n = plate_normal.normalize();

    let neg_dir = Vec3::new(-dir.x, -dir.y, -dir.z);
    let cos_theta = clampd(neg_dir.dot(n), -1.0, 1.0);
    let obliquity_deg = rad2deg(cos_theta.acos());

    let mut effective_angle = obliquity_deg - normalization_deg;
    if effective_angle < 0.0 {
        effective_angle = 0.0;
    }
    if effective_angle > 89.0 {
        effective_angle = 89.0;
    }

    if let Some(ob) = out_obliquity_deg {
        *ob = obliquity_deg;
    }

    let mut cos_eff = deg2rad(effective_angle).cos();
    if cos_eff < 0.02 {
        cos_eff = 0.02;
    }

    plate.thickness_mm / cos_eff
}

