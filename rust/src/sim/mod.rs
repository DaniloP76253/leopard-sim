pub mod ai;
pub mod ammo;
pub mod armor;
pub mod combat;
pub mod match_state;
pub mod projectile;
pub mod render_helpers;
pub mod tank;
pub mod terrain;
pub mod vecmath;

#[cfg(test)]
mod tests {
    use super::ammo::*;
    use super::armor::*;
    use super::combat::*;
    use super::tank::*;
    use super::vecmath::*;

    #[test]
    fn test_vecmath_basic() {
        let v1 = Vec2::new(3.0, 4.0);
        assert!((v1.length() - 5.0).abs() < 1e-6);

        let v2 = Vec2::new(0.0, 0.0);
        assert!((v1.dist(v2) - 5.0).abs() < 1e-6);

        let angle = norm_deg(270.0);
        assert!((angle - (-90.0)).abs() < 1e-6);
    }

    #[test]
    fn test_ammo_pen_curve() {
        let stats = AmmoTypeId::Apfsds.stats();
        let pen_0m = stats.penetration_at_range(0.0);
        assert_eq!(pen_0m, 660.0);

        let pen_500m = stats.penetration_at_range(500.0);
        assert_eq!(pen_500m, 620.0);

        let pen_250m = stats.penetration_at_range(250.0);
        assert!((pen_250m - 640.0).abs() < 1e-6);
    }

    #[test]
    fn test_armor_thickness() {
        let plate = ArmorPlate::new("Test", 100.0, 0.0);
        let normal = Vec3::new(1.0, 0.0, 0.0);
        let impact = Vec3::new(-1.0, 0.0, 0.0);
        let mut ob = 0.0;
        let eff = armor_effective_thickness(&plate, normal, impact, 0.0, Some(&mut ob));
        assert!((eff - 100.0).abs() < 1e-6);
        assert!(ob < 1e-6);
    }

    #[test]
    fn test_tank_init_and_movement() {
        let mut tank = Tank::new_leopard2a7("TestPlayer", Vec2::new(0.0, 0.0), 0.0);
        assert_eq!(tank.hp, 100.0);
        assert!(tank.can_fire());

        tank.update_movement(1.0, 0.0, 1.0, 0.5);
        assert!(tank.speed_ms > 0.0);
        assert!(tank.position_m.x > 0.0);
    }

    #[test]
    fn test_combat_fire_direct_hit() {
        let mut shooter = Tank::new_leopard2a7("Shooter", Vec2::new(0.0, 0.0), 0.0);
        let mut target = Tank::new_generic_mbt("Target", Vec2::new(100.0, 0.0), 180.0);

        let res = combat_fire_shot(&mut shooter, &mut target);
        assert_ne!(res.outcome, ShotOutcome::Invalid);
        assert_eq!(shooter.ammo_count[0], 19);
    }
}
