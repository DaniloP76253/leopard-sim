pub const PEN_TABLE_POINTS: usize = 6;
pub const AMMO_TYPE_COUNT: usize = 3;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AmmoTypeId {
    Apfsds = 0,
    Heat = 1,
    He = 2,
}

impl AmmoTypeId {
    pub fn from_index(idx: usize) -> Self {
        match idx {
            1 => AmmoTypeId::Heat,
            2 => AmmoTypeId::He,
            _ => AmmoTypeId::Apfsds,
        }
    }

    pub fn to_index(self) -> usize {
        self as usize
    }

    pub fn stats(self) -> &'static AmmoStats {
        match self {
            AmmoTypeId::Apfsds => &AMMO_APFSDS_STATS,
            AmmoTypeId::Heat => &AMMO_HEAT_STATS,
            AmmoTypeId::He => &AMMO_HE_STATS,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct AmmoStats {
    pub name: &'static str,
    pub short_name: &'static str,
    pub muzzle_velocity_ms: f64,
    pub drag_decel_per_m: f64,
    pub range_points_m: [f64; PEN_TABLE_POINTS],
    pub pen_points_mm: [f64; PEN_TABLE_POINTS],
    pub he_filler_g: f64,
    pub normalization_deg: f64,
    pub blast_radius_m: f64,
}

pub static AMMO_APFSDS_STATS: AmmoStats = AmmoStats {
    name: "DM53 APFSDS",
    short_name: "APFSDS",
    muzzle_velocity_ms: 1750.0,
    drag_decel_per_m: 0.00006,
    range_points_m: [0.0, 500.0, 1000.0, 1500.0, 2000.0, 3000.0],
    pen_points_mm: [660.0, 620.0, 590.0, 560.0, 530.0, 480.0],
    he_filler_g: 0.0,
    normalization_deg: 0.0,
    blast_radius_m: 0.0,
};

pub static AMMO_HEAT_STATS: AmmoStats = AmmoStats {
    name: "DM12 HEAT-MP",
    short_name: "HEAT",
    muzzle_velocity_ms: 1140.0,
    drag_decel_per_m: 0.00012,
    range_points_m: [0.0, 500.0, 1000.0, 1500.0, 2000.0, 3000.0],
    pen_points_mm: [800.0, 800.0, 800.0, 800.0, 800.0, 800.0],
    he_filler_g: 450.0,
    normalization_deg: 5.0,
    blast_radius_m: 2.0,
};

pub static AMMO_HE_STATS: AmmoStats = AmmoStats {
    name: "DM11 HE-PFF",
    short_name: "HE",
    muzzle_velocity_ms: 1140.0,
    drag_decel_per_m: 0.00012,
    range_points_m: [0.0, 500.0, 1000.0, 1500.0, 2000.0, 3000.0],
    pen_points_mm: [30.0, 30.0, 30.0, 30.0, 30.0, 30.0],
    he_filler_g: 1015.0,
    normalization_deg: 0.0,
    blast_radius_m: 6.0,
};

impl AmmoStats {
    pub fn penetration_at_range(&self, range_m: f64) -> f64 {
        if range_m <= self.range_points_m[0] {
            return self.pen_points_mm[0];
        }
        for i in 0..(PEN_TABLE_POINTS - 1) {
            let r0 = self.range_points_m[i];
            let r1 = self.range_points_m[i + 1];
            if range_m <= r1 {
                let t = (range_m - r0) / (r1 - r0);
                let p0 = self.pen_points_mm[i];
                let p1 = self.pen_points_mm[i + 1];
                return p0 + t * (p1 - p0);
            }
        }
        self.pen_points_mm[PEN_TABLE_POINTS - 1]
    }

    pub fn velocity_at_range(&self, range_m: f64) -> f64 {
        self.muzzle_velocity_ms * (-self.drag_decel_per_m * range_m).exp()
    }
}

