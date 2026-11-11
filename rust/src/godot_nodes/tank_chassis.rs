use godot::classes::{IRigidBody3D, RayCast3D, RigidBody3D};
use godot::prelude::*;

const WHEEL_COUNT_PER_SIDE: usize = 7;
const WHEEL_COUNT: usize = WHEEL_COUNT_PER_SIDE * 2;
const TRACK_HALF_GAUGE: f32 = 1.95;
// Ray origin is at y=0.25; wheel center at full extension is y=-0.62.
const REST_LENGTH: f32 = 1.29;
const MAX_LENGTH: f32 = 1.55;
const MAX_COMPRESSION: f32 = 0.45;
// At 67 t, 14 springs at this rate settle about 0.13 m into their travel.
const SPRING_RATE: f32 = 350000.0;
const DAMPING_RATE: f32 = 50000.0;
const MAX_SUSPENSION_FORCE: f32 = 80000.0;
const LATERAL_GRIP: f32 = 8.0;
const MAX_LATERAL_GRIP: f32 = 1.1;
const TRACK_RADIUS: f32 = 0.42;
const MAX_GROUND_SPEED_FWD: f32 = 60.0 / 3.6;
const MAX_GROUND_SPEED_REV: f32 = 25.0 / 3.6;
const MAX_TURN_RATE_RAD_S: f32 = 45.0_f32.to_radians();
const DRIVE_ACCELERATION: f32 = 3.0;
const STEER_ACCELERATION: f32 = 6.0;
const DRIVE_BRAKE_ACCELERATION: f32 = 4.0;

#[derive(GodotClass)]
#[class(base=RigidBody3D)]
pub struct TankChassis {
    base: Base<RigidBody3D>,
    throttle: f32,
    steer: f32,
    wheel_lengths: [f32; WHEEL_COUNT],
    contact_count: i32,
}

#[godot_api]
impl IRigidBody3D for TankChassis {
    fn init(base: Base<RigidBody3D>) -> Self {
        Self {
            base,
            throttle: 0.0,
            steer: 0.0,
            wheel_lengths: [MAX_LENGTH; WHEEL_COUNT],
            contact_count: 0,
        }
    }

    fn physics_process(&mut self, _delta: f64) {}
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
        let forward_speed = if self.throttle >= 0.0 {
            self.throttle * MAX_GROUND_SPEED_FWD
        } else {
            self.throttle * MAX_GROUND_SPEED_REV
        };
        forward_speed - direction * self.steer * TRACK_HALF_GAUGE * MAX_TURN_RATE_RAD_S
    }

    #[func]
    pub fn get_suspension_contact_count(&self) -> i32 {
        self.contact_count
    }

    fn step_suspension(&mut self, delta: f32) {
        let velocity = self.base().get_linear_velocity();
        let transform = self.base().get_global_transform();
        // `rows` are matrix rows, not local axes. Use the rotated basis columns.
        let forward = transform.basis.col_a();
        let right = transform.basis.col_c();

        let mut contacts = [false; WHEEL_COUNT];
        let mut contact_positions = [Vector3::ZERO; WHEEL_COUNT];
        let mut contact_normals = [Vector3::UP; WHEEL_COUNT];
        for index in 0..WHEEL_COUNT {
            let side = if index < WHEEL_COUNT_PER_SIDE {
                -1.0
            } else {
                1.0
            };
            let wheel_index = index % WHEEL_COUNT_PER_SIDE;
            let ray_name = if side < 0.0 {
                "Suspension/Left"
            } else {
                "Suspension/Right"
            };
            let path = format!("{}/{}", ray_name, wheel_index);
            self.wheel_lengths[index] = MAX_LENGTH;
            let Some(node) = self.base().get_node_or_null(path.as_str()) else {
                continue;
            };
            let Ok(mut ray) = node.try_cast::<RayCast3D>() else {
                continue;
            };
            ray.force_raycast_update();
            if ray.is_colliding() {
                contacts[index] = true;
                contact_positions[index] = ray.get_collision_point();
                contact_normals[index] = ray.get_collision_normal().normalized();
                self.wheel_lengths[index] = ray
                    .get_global_position()
                    .distance_to(contact_positions[index])
                    .clamp(REST_LENGTH - MAX_COMPRESSION, MAX_LENGTH);
            }
        }
        self.contact_count = contacts.iter().filter(|contact| **contact).count() as i32;
        let angular_velocity = self.base().get_angular_velocity();
        let mass = self.base().get_mass();
        let forward_target = if self.throttle >= 0.0 {
            self.throttle * MAX_GROUND_SPEED_FWD
        } else {
            self.throttle * MAX_GROUND_SPEED_REV
        };
        for side_index in 0..2 {
            let first_wheel = side_index * WHEEL_COUNT_PER_SIDE;
            let side_sign = if side_index == 0 { -1.0 } else { 1.0 };
            // Left faster than right turns toward +Z for this +X-forward hull.
            let target_speed =
                forward_target - side_sign * self.steer * TRACK_HALF_GAUGE * MAX_TURN_RATE_RAD_S;
            let track_offset =
                transform.basis * Vector3::new(0.0, -0.84, side_sign * TRACK_HALF_GAUGE);
            let track_velocity = velocity + angular_velocity.cross(track_offset);
            let track_speed = track_velocity.dot(forward);
            let speed_error = target_speed - track_speed;
            let acceleration_limit = if self.steer.abs() > 0.01 {
                STEER_ACCELERATION
            } else if target_speed.abs() > 0.01 {
                DRIVE_ACCELERATION
            } else {
                DRIVE_BRAKE_ACCELERATION
            };
            let drive_acceleration =
                (speed_error / delta.max(0.0001)).clamp(-acceleration_limit, acceleration_limit);
            let grounded_wheels = contacts[first_wheel..first_wheel + WHEEL_COUNT_PER_SIDE]
                .iter()
                .filter(|contact| **contact)
                .count() as f32;
            let contact_ratio = grounded_wheels / WHEEL_COUNT_PER_SIDE as f32;
            let drive_force = forward * (drive_acceleration * mass * 0.5 * contact_ratio);
            self.base_mut()
                .apply_force_ex(drive_force)
                .position(track_offset)
                .done();
        }

        for index in 0..WHEEL_COUNT {
            if !contacts[index] {
                continue;
            }

            let contact = contact_positions[index];
            let force_offset = contact - transform.origin;
            let contact_velocity = velocity + angular_velocity.cross(force_offset);
            let normal = contact_normals[index];
            let compression = (REST_LENGTH - self.wheel_lengths[index]).max(0.0);
            let compression_speed = -contact_velocity.dot(normal);
            let spring_force = SPRING_RATE * compression;
            let damping_force = DAMPING_RATE * compression_speed;
            let suspension_force = (spring_force + damping_force).clamp(0.0, MAX_SUSPENSION_FORCE);
            self.base_mut()
                .apply_force_ex(normal * suspension_force)
                .position(force_offset)
                .done();

            let lateral_axis = right - normal * right.dot(normal);
            if lateral_axis.length_squared() <= 0.0001 {
                continue;
            }
            let lateral_axis = lateral_axis.normalized();
            // Resist chassis side-slip without cancelling the differential yaw motion.
            let lateral_speed = velocity.dot(lateral_axis);
            let max_lateral_force = suspension_force * MAX_LATERAL_GRIP;
            let lateral_force_magnitude =
                (-lateral_speed * (mass / WHEEL_COUNT as f32) * LATERAL_GRIP)
                    .clamp(-max_lateral_force, max_lateral_force);
            let lateral_force = lateral_axis * lateral_force_magnitude;
            self.base_mut()
                .apply_force_ex(lateral_force)
                .position(force_offset)
                .done();
        }
    }
}

pub fn track_radius() -> f32 {
    TRACK_RADIUS
}
