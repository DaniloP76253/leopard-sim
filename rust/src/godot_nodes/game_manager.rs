use godot::classes::{INode3D, Node3D};
use godot::prelude::*;

use crate::sim::ai::{ai_compute_command, AiState};
use crate::sim::ammo::AmmoTypeId;
use crate::sim::combat::{combat_fire_shot, CombatShotResult};
use crate::sim::match_state::{MatchState, SupplyDepot};
use crate::sim::render_helpers::{
    tank_boresight_dir, terrain_visual_height_at, turret_has_line_of_sight, turret_input_toward,
    update_chassis_pose, ELEVATION_MOUSE_SENS_DEG_PER_PX, PLAYER_HEIGHT,
    TURRET_MOUSE_SENS_DEG_PER_PX,
};
use crate::sim::tank::Tank;
use crate::sim::terrain::terrain_grip_at;
use crate::sim::vecmath::{clampd, norm_deg, Vec2};

const SIM_DT_MAX: f64 = 0.05;
const AI_PREFERRED_RANGE_M: f64 = 350.0;
const WAVE_ORIGIN_X: f64 = 0.0;
const WAVE_ORIGIN_Y: f64 = 0.0;
const WAVE_MIN_RANGE_M: f64 = 500.0;
const WAVE_MAX_RANGE_M: f64 = 900.0;
const WAVE_RESPAWN_DELAY_S: f64 = 4.0;
const SUPPLY_DEPOT_X: f64 = 0.0;
const SUPPLY_DEPOT_Y: f64 = 220.0;
const SUPPLY_RADIUS_M: f64 = 20.0;
const SUPPLY_HEAL_PER_S: f64 = 8.0;

#[derive(GodotClass)]
#[class(base=Node3D)]
pub struct TankGame {
    base: Base<Node3D>,

    player: Tank,
    target: Tank,

    ai_state: AiState,
    wave_state: MatchState,
    depot: SupplyDepot,

    desired_turret_azimuth_deg: f64,
    desired_gun_elevation_deg: f64,

    gunner_sight_active: bool,
    sim_time_s: f64,

    last_player_shot: String,
    last_enemy_shot: String,
    was_resupplied_notification: bool,
}

#[godot_api]
impl INode3D for TankGame {
    fn init(base: Base<Node3D>) -> Self {
        let player = Tank::new_leopard2a7("Player", Vec2::new(0.0, 0.0), 0.0);
        let target = Tank::new_generic_mbt("Target", Vec2::new(700.0, 150.0), 200.0);
        let ai_state = AiState::new(AI_PREFERRED_RANGE_M);
        let wave_state = MatchState::new();
        let depot = SupplyDepot {
            position: Vec2::new(SUPPLY_DEPOT_X, SUPPLY_DEPOT_Y),
            radius_m: SUPPLY_RADIUS_M,
            heal_per_s: SUPPLY_HEAL_PER_S,
        };

        Self {
            base,
            player,
            target,
            ai_state,
            wave_state,
            depot,
            desired_turret_azimuth_deg: 0.0,
            desired_gun_elevation_deg: 0.0,
            gunner_sight_active: false,
            sim_time_s: 0.0,
            last_player_shot: String::new(),
            last_enemy_shot: String::new(),
            was_resupplied_notification: false,
        }
    }

    fn physics_process(&mut self, mut delta: f64) {
        if delta > SIM_DT_MAX {
            delta = SIM_DT_MAX;
        }

        let (p_turret, p_elev) = turret_input_toward(
            &self.player,
            self.desired_turret_azimuth_deg,
            self.desired_gun_elevation_deg,
            delta,
        );

        self.player.update_turret(p_turret, p_elev, delta);
        self.player.tick_reload(delta);
        update_chassis_pose(&mut self.player);

        if self.target.alive {
            let cmd = ai_compute_command(
                &mut self.ai_state,
                &self.target,
                &self.player,
                delta,
            );

            self.target.update_movement(
                cmd.throttle,
                cmd.steer,
                terrain_grip_at(self.target.position_m),
                delta,
            );
            update_chassis_pose(&mut self.target);
            self.target.update_turret(cmd.turret_input, cmd.elevation_input, delta);
            self.target.tick_reload(delta);

            let ammo_idx = self.target.current_ammo.to_index();
            if cmd.wants_to_fire
                && self.target.can_fire()
                && self.target.reload_timer_s <= 0.0
                && self.target.ammo_count[ammo_idx] > 0
                && turret_has_line_of_sight(&self.target, &self.player)
            {
                let res = combat_fire_shot(&mut self.target, &mut self.player);
                self.last_enemy_shot = res.message;
            }
        } else {
            self.wave_state.respawn_timer_s -= delta;
            if self.wave_state.respawn_timer_s <= 0.0 {
                self.wave_state.wave += 1;
                self.target = self.wave_state.spawn_wave(
                    "Target",
                    Vec2::new(WAVE_ORIGIN_X, WAVE_ORIGIN_Y),
                    WAVE_MIN_RANGE_M,
                    WAVE_MAX_RANGE_M,
                );
                self.ai_state = AiState::new(AI_PREFERRED_RANGE_M);
            }
        }

        if self.wave_state.try_resupply(&mut self.player, &self.depot, delta) {
            self.was_resupplied_notification = true;
        }

        self.sim_time_s += delta;
    }
}

#[godot_api]
impl TankGame {
    #[signal]
    fn shot_result(outcome: i32, impact_x: f32, impact_y: f32, impact_z: f32, message: GString);

    #[func]
    pub fn reset_game(&mut self) {
        self.player = Tank::new_leopard2a7("Player", Vec2::new(0.0, 0.0), 0.0);
        self.target = Tank::new_generic_mbt("Target", Vec2::new(700.0, 150.0), 200.0);
        self.wave_state = MatchState::new();
        self.ai_state = AiState::new(AI_PREFERRED_RANGE_M);
        self.sim_time_s = 0.0;
        self.desired_turret_azimuth_deg = 0.0;
        self.desired_gun_elevation_deg = 0.0;
        self.gunner_sight_active = false;
        self.last_player_shot.clear();
        self.last_enemy_shot.clear();
        self.was_resupplied_notification = false;
    }

    #[func]
    pub fn apply_player_input(
        &mut self,
        throttle: f64,
        steer: f64,
        mouse_dx: f64,
        mouse_dy: f64,
        trim_turret: f64,
        trim_elev: f64,
        dt: f64,
    ) {
        self.player.update_movement(
            throttle,
            steer,
            terrain_grip_at(self.player.position_m),
            dt,
        );

        self.desired_turret_azimuth_deg = norm_deg(
            self.desired_turret_azimuth_deg
                + mouse_dx * TURRET_MOUSE_SENS_DEG_PER_PX
                + trim_turret * self.player.turret_traverse_dps * dt,
        );

        self.desired_gun_elevation_deg = clampd(
            self.desired_gun_elevation_deg
                - mouse_dy * ELEVATION_MOUSE_SENS_DEG_PER_PX
                + trim_elev * self.player.gun_elevation_dps * dt,
            self.player.gun_elevation_min_deg,
            self.player.gun_elevation_max_deg,
        );
    }

    #[func]
    pub fn fire_player_shot(&mut self) -> Dictionary<Variant, Variant> {
        let mut dict: Dictionary<Variant, Variant> = Dictionary::new();
        if !self.player.can_fire() {
            dict.set("success", false);
            dict.set("message", "Cannot fire (gun damaged or dead)");
            return dict;
        }

        let res: CombatShotResult = combat_fire_shot(&mut self.player, &mut self.target);
        self.last_player_shot = res.message.clone();

        dict.set("success", true);
        dict.set("outcome", res.outcome as i32);
        dict.set(
            "impact_point",
            Vector3::new(
                res.impact_point.x as f32,
                res.impact_point.z as f32,
                res.impact_point.y as f32,
            ),
        );
        dict.set("range_m", res.range_m);
        dict.set("damage", res.damage_dealt);
        dict.set("message", res.message.as_str());

        if !self.target.alive && self.wave_state.respawn_timer_s <= 0.0 {
            self.wave_state.kills += 1;
            self.wave_state.respawn_timer_s = WAVE_RESPAWN_DELAY_S;
        }

        dict
    }

    #[func]
    pub fn select_ammo(&mut self, ammo_idx: i32) {
        self.player.current_ammo = AmmoTypeId::from_index(ammo_idx as usize);
    }

    #[func]
    pub fn toggle_gunner_sight(&mut self) -> bool {
        self.gunner_sight_active = !self.gunner_sight_active;
        self.gunner_sight_active
    }

    #[func]
    pub fn is_gunner_sight_active(&self) -> bool {
        self.gunner_sight_active
    }

    #[func]
    pub fn get_desired_turret_azimuth(&self) -> f64 {
        self.desired_turret_azimuth_deg
    }

    #[func]
    pub fn get_desired_gun_elevation(&self) -> f64 {
        self.desired_gun_elevation_deg
    }

    #[func]
    pub fn get_player_position(&self) -> Vector3 {
        let visual_y = terrain_visual_height_at(self.player.position_m);
        Vector3::new(
            self.player.position_m.x as f32,
            visual_y + PLAYER_HEIGHT + self.player.hull_bounce_m as f32,
            self.player.position_m.y as f32,
        )
    }

    #[func]
    pub fn get_player_heading(&self) -> f64 {
        self.player.heading_deg
    }

    #[func]
    pub fn get_player_turret_azimuth(&self) -> f64 {
        self.player.turret_azimuth_deg
    }

    #[func]
    pub fn get_player_absolute_turret_azimuth(&self) -> f64 {
        self.player.absolute_turret_azimuth_deg()
    }

    #[func]
    pub fn get_player_gun_elevation(&self) -> f64 {
        self.player.gun_elevation_deg
    }

    #[func]
    pub fn get_player_pitch_roll(&self) -> Vector2 {
        Vector2::new(self.player.hull_pitch_deg as f32, self.player.hull_roll_deg as f32)
    }

    #[func]
    pub fn get_player_turret_world_pos(&self) -> Vector3 {
        let visual_y = terrain_visual_height_at(self.player.position_m);
        Vector3::new(
            self.player.position_m.x as f32,
            visual_y + PLAYER_HEIGHT + 0.8 + self.player.hull_bounce_m as f32,
            self.player.position_m.y as f32,
        )
    }

    #[func]
    pub fn get_player_boresight_dir(&self) -> Vector3 {
        let dir = tank_boresight_dir(&self.player);
        Vector3::new(dir.x as f32, dir.y as f32, dir.z as f32)
    }

    #[func]
    pub fn get_boresight_target_point(&self) -> Vector3 {
        let turret_pos = self.get_player_turret_world_pos();
        let dir = self.get_player_boresight_dir();
        turret_pos + dir * 260.0
    }

    #[func]
    pub fn get_gunner_eye_pos(&self) -> Vector3 {
        let turret_pos = self.get_player_turret_world_pos();
        let dir = self.get_player_boresight_dir();
        turret_pos + dir * 3.5
    }

    #[func]
    pub fn get_target_position(&self) -> Vector3 {
        let visual_y = terrain_visual_height_at(self.target.position_m);
        Vector3::new(
            self.target.position_m.x as f32,
            visual_y + PLAYER_HEIGHT + self.target.hull_bounce_m as f32,
            self.target.position_m.y as f32,
        )
    }

    #[func]
    pub fn get_target_heading(&self) -> f64 {
        self.target.heading_deg
    }

    #[func]
    pub fn get_target_turret_azimuth(&self) -> f64 {
        self.target.turret_azimuth_deg
    }

    #[func]
    pub fn get_target_absolute_turret_azimuth(&self) -> f64 {
        self.target.absolute_turret_azimuth_deg()
    }

    #[func]
    pub fn get_target_gun_elevation(&self) -> f64 {
        self.target.gun_elevation_deg
    }

    #[func]
    pub fn get_target_pitch_roll(&self) -> Vector2 {
        Vector2::new(self.target.hull_pitch_deg as f32, self.target.hull_roll_deg as f32)
    }

    #[func]
    pub fn get_target_alive(&self) -> bool {
        self.target.alive
    }

    #[func]
    pub fn get_depot_position(&self) -> Vector3 {
        let ground = terrain_visual_height_at(self.depot.position);
        Vector3::new(
            self.depot.position.x as f32,
            ground + 0.3,
            self.depot.position.y as f32,
        )
    }

    #[func]
    pub fn get_depot_radius(&self) -> f64 {
        self.depot.radius_m
    }

    #[func]
    pub fn is_player_in_depot(&self) -> bool {
        self.player.position_m.dist(self.depot.position) <= self.depot.radius_m
    }

    #[func]
    pub fn has_line_of_sight(&self) -> bool {
        turret_has_line_of_sight(&self.player, &self.target)
    }

    #[func]
    pub fn get_terrain_visual_height(&self, x: f32, z: f32) -> f32 {
        terrain_visual_height_at(Vec2::new(x as f64, z as f64))
    }

    #[func]
    pub fn get_elevation_tape_info(&self) -> Vector3 {
        Vector3::new(
            self.player.gun_elevation_deg as f32,
            self.player.gun_elevation_min_deg as f32,
            self.player.gun_elevation_max_deg as f32,
        )
    }

    #[func]
    pub fn get_hud_data(&self) -> Dictionary<Variant, Variant> {
        let mut d: Dictionary<Variant, Variant> = Dictionary::new();
        d.set("speed_kmh", self.player.speed_kmh());
        d.set(
            "range_to_target",
            self.player.position_m.dist(self.target.position_m),
        );
        d.set("has_los", self.has_line_of_sight());
        d.set("player_hp", self.player.hp);
        d.set("player_alive", self.player.alive);
        d.set("target_hp", self.target.hp);
        d.set("target_alive", self.target.alive);

        d.set("ammo_name", self.player.current_ammo.stats().short_name);
        d.set(
            "ammo_count",
            self.player.ammo_count[self.player.current_ammo.to_index()],
        );
        d.set("reload_timer", self.player.reload_timer_s);

        d.set("wave", self.wave_state.wave);
        d.set("kills", self.wave_state.kills);
        d.set("respawn_timer", self.wave_state.respawn_timer_s);
        d.set("is_resupplying", self.is_player_in_depot());

        d.set("module_engine", self.player.module_engine_hp);
        d.set("module_tracks", self.player.module_tracks_hp);
        d.set("module_gun", self.player.module_gun_hp);

        d.set("last_player_shot", self.last_player_shot.as_str());
        d.set("last_enemy_shot", self.last_enemy_shot.as_str());

        d
    }
}
