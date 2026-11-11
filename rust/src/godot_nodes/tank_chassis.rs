use godot::classes::{IRigidBody3D, RayCast3D, RigidBody3D};
use godot::prelude::*;

const WHEEL_COUNT_PER_SIDE: usize = 7;
const WHEEL_COUNT: usize = WHEEL_COUNT_PER_SIDE * 2;
const WHEEL_X: [f32; WHEEL_COUNT_PER_SIDE] = [-2.4, -1.6, -0.8, 0.0, 0.8, 1.6, 2.4];
const TRACK_HALF_GAUGE: f32 = 1.95;
const REST_LENGTH: f32 = 1.1;
const MAX_LENGTH: f32 = 1.55;
const SPRING_RATE: f32 = 60000.0;
const DAMPING_RATE: f32 = 10000.0;
const MAX_SUSPENSION_FORCE: f32 = 45000.0;
const LATERAL_GRIP: f32 = 14.0;
const TRACK_RADIUS: f32 = 0.42;
const MAX_GROUND_SPEED: f32 = 12.0;
const DRIVE_ACCELERATION: f32 = 2.2;
const DRIVE_BRAKE_ACCELERATION: f32 = 4.0;

#[derive(GodotClass)]
#[class(base=RigidBody3D)]
pub struct TankChassis {
    base: Base<RigidBody3D>,
    throttle: f32,
    steer: f32,
    previous_lengths: [f32; WHEEL_COUNT],
    wheel_lengths: [f32; WHEEL_COUNT],
    lengths_initialized: bool,
    contact_count: i32,
    startup_remaining: f32,
}

#[godot_api]
impl IRigidBody3D for TankChassis {
    fn init(base: Base<RigidBody3D>) -> Self {
        Self {
            base,
            throttle: 0.0,
            steer: 0.0,
            previous_lengths: [REST_LENGTH; WHEEL_COUNT],
            wheel_lengths: [REST_LENGTH; WHEEL_COUNT],
            lengths_initialized: false,
            contact_count: 0,
            startup_remaining: 0.5,
        }
    }

    fn physics_process(&mut self, _delta: f64) {
    }
}

#[godot_api]
impl TankChassis {
    #[func]
    pub fn simulate_physics_step(&mut self, delta: f64) {
        self.step_suspension(delta as f32);
    }

    #[func]
    pub fn set_drive_input(&mut self, throttle: f32, steer: f32) {
        self.throttle = throttle.clamp(-1.0, 1.0);
        self.steer = steer.clamp(-1.0, 1.0);
    }

    #[func]
    pub fn get_wheel_compression(&self, index: i32) -> f32 {
        let index = index.clamp(0, (WHEEL_COUNT - 1) as i32) as usize;
        (REST_LENGTH - self.wheel_lengths[index]).max(0.0)
    }

    #[func]
    pub fn get_track_speed(&self, side: i32) -> f32 {
        let direction = if side <= 0 { -1.0 } else { 1.0 };
        (self.throttle + direction * self.steer).clamp(-1.0, 1.0)
    }

    #[func]
    pub fn get_suspension_contact_count(&self) -> i32 {
        self.contact_count
    }

    fn step_suspension(&mut self, delta: f32) {
        if self.startup_remaining > 0.0 {
            self.startup_remaining -= delta;
            self.base_mut().set_linear_velocity(Vector3::ZERO);
            self.base_mut().set_angular_velocity(Vector3::ZERO);
            if self.startup_remaining <= 0.0 {
                self.base_mut().set_freeze_enabled(false);
            }
            return;
        }

        let velocity = self.base().get_linear_velocity();
        let transform = self.base().get_global_transform();
        let forward = transform.basis.rows[0];
        let up = transform.basis.rows[1];
        let right = -transform.basis.rows[2];

        let mut contacts = [false; WHEEL_COUNT];
        let mut contact_positions = [Vector3::ZERO; WHEEL_COUNT];
        for index in 0..WHEEL_COUNT {
            let side = if index < WHEEL_COUNT_PER_SIDE { -1.0 } else { 1.0 };
            let wheel_index = index % WHEEL_COUNT_PER_SIDE;
            let local_origin = Vector3::new(WHEEL_X[wheel_index], 0.25, side * TRACK_HALF_GAUGE);
            let ray_name = if side < 0.0 { "Suspension/Left" } else { "Suspension/Right" };
            let path = format!("{}/{}", ray_name, wheel_index);
            let Some(node) = self.base().get_node_or_null(path.as_str()) else {
                continue;
            };
            let Ok(ray) = node.try_cast::<RayCast3D>() else {
                continue;
            };
            if ray.is_colliding() {
                contacts[index] = true;
                contact_positions[index] = ray.get_collision_point();
            }
            let origin = transform * local_origin;
            self.wheel_lengths[index] = if contacts[index] {
                origin.distance_to(contact_positions[index]).clamp(0.05, MAX_LENGTH)
            } else {
                MAX_LENGTH
            };
        }
        self.contact_count = contacts.iter().filter(|contact| **contact).count() as i32;
        let forward_speed = velocity.dot(forward);
        let target_speed = self.throttle * MAX_GROUND_SPEED;
        let speed_error = target_speed - forward_speed;
        let acceleration_limit = if self.throttle.abs() > 0.01 {
            DRIVE_ACCELERATION
        } else {
            DRIVE_BRAKE_ACCELERATION
        };
        let drive_acceleration = (speed_error / delta.max(0.0001))
            .clamp(-acceleration_limit, acceleration_limit);
        let drive_force = forward * (drive_acceleration * self.base().get_mass());
        self.base_mut().apply_central_force(drive_force);

        for index in 0..WHEEL_COUNT {
            let length = self.wheel_lengths[index];
            let previous = self.previous_lengths[index];
            let compression_speed = (previous - length) / delta.max(0.0001);
            self.previous_lengths[index] = length;
			if !self.lengths_initialized {
				continue;
			}
            if !contacts[index] {
                continue;
            }

            let spring_force = SPRING_RATE * (REST_LENGTH - length).max(0.0);
            let damping_force = DAMPING_RATE * compression_speed;
            let suspension_force = (spring_force - damping_force).clamp(0.0, MAX_SUSPENSION_FORCE);
            let contact = contact_positions[index];
            let force_offset = contact - transform.origin;
            self.base_mut()
                .apply_force_ex(up * suspension_force)
                .position(force_offset)
                .done();

            let contact_velocity = velocity;
            let lateral_speed = contact_velocity.dot(right);
            let lateral_force = -right * (lateral_speed * LATERAL_GRIP * self.base().get_mass());
            self.base_mut()
                .apply_force_ex(lateral_force)
                .position(force_offset)
                .done();
        }
        		self.lengths_initialized = true;
    }
}

pub fn track_radius() -> f32 {
    TRACK_RADIUS
}
