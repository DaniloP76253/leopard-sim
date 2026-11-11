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
    use super::ai::*;
    use super::ammo::*;
    use super::armor::*;
    use super::combat::*;
    use super::render_helpers::*;
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
    fn test_player_turret_input_changes_angles() {
        let mut tank = Tank::new_leopard2a7("TestPlayer", Vec2::new(0.0, 0.0), 0.0);
        let (turret_input, elevation_input) = turret_input_toward(&tank, 90.0, 10.0, 0.1);

        tank.update_turret(turret_input, elevation_input, 0.1);

        assert!(tank.turret_azimuth_deg > 0.0);
        assert!(tank.gun_elevation_deg > 0.0);
    }

    #[test]
    fn test_tracks_animate_with_differential_motion() {
        let mut tank = Tank::new_leopard2a7("TestPlayer", Vec2::new(0.0, 0.0), 0.0);

        tank.update_movement(1.0, 1.0, 1.0, 0.5);

        assert!(tank.right_track_phase_m > tank.left_track_phase_m);
        assert!(tank.left_track_speed_ms < tank.right_track_speed_ms);
        assert!(tank.left_track_phase_m.abs() > 0.0 || tank.right_track_phase_m.abs() > 0.0);
    }

    #[test]
    fn test_ai_turret_input_changes_angles() {
        let mut ai_state = AiState::new(350.0);
        let mut bot = Tank::new_generic_mbt("Bot", Vec2::new(0.0, 0.0), 0.0);
        let player = Tank::new_leopard2a7("Player", Vec2::new(0.0, 100.0), 0.0);
        let command = ai_compute_command(&mut ai_state, &bot, &player, 0.1);

        bot.update_turret(command.turret_input, command.elevation_input, 0.1);

        assert!(bot.turret_azimuth_deg > 0.0);
        assert!(bot.gun_elevation_deg >= 0.0);
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
