use godot::classes::{IRigidBody3D, RayCast3D, RigidBody3D};
use godot::prelude::*;

const WHEEL_COUNT_PER_SIDE: usize = 7;
const WHEEL_COUNT: usize = WHEEL_COUNT_PER_SIDE * 2;
const TRACK_HALF_GAUGE: f32 = 1.95;
// Ray origin is at y=0.25; wheel center at full extension is y=-0.62.
const MAX_LENGTH: f32 = 1.75;
const MAX_COMPRESSION: f32 = 0.45;
const LATERAL_GRIP: f32 = 8.0;
const MAX_LATERAL_GRIP: f32 = 1.0;
const TRACK_RADIUS: f32 = 0.42;
const MAX_GROUND_SPEED_FWD: f32 = 60.0 / 3.6;
const MAX_GROUND_SPEED_REV: f32 = 25.0 / 3.6;
const MAX_TURN_RATE_RAD_S: f32 = 45.0_f32.to_radians();
const DRIVE_ACCELERATION: f32 = 4.5;
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

    #[export]
    pub track_friction: f32,

    #[export]
    pub suspension_spring_rate: f32,
    #[export]
    pub suspension_damping_rate: f32,
    #[export]
    pub suspension_max_force: f32,
    #[export]
    pub suspension_rest_length: f32,
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
            track_friction: 1.0,
            // 14 springs at this rate support 67 t after about 0.13 m of travel.
            suspension_spring_rate: 350000.0,
            suspension_damping_rate: 50000.0,
            suspension_max_force: 80000.0,
            // Matches the wheel radius plus tread thickness at full extension.
            suspension_rest_length: 1.544,
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
        let rest_length = self
            .suspension_rest_length
            .clamp(MAX_COMPRESSION, MAX_LENGTH + MAX_COMPRESSION);
        (rest_length - self.wheel_lengths[index]).clamp(0.0, MAX_COMPRESSION)
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
    pub fn get_actual_track_speeds(&self) -> Vector2 {
        if self.throttle.abs() <= 0.01 && self.steer.abs() <= 0.01 {
            return Vector2::ZERO;
        }

        let transform = self.base().get_global_transform();
        let velocity = self.base().get_linear_velocity();
        let angular_velocity = self.base().get_angular_velocity();
        let forward = transform.basis.col_a().normalized();
        let up = transform.basis.col_b().normalized();
        let forward_speed = velocity.dot(forward);

        // Apply steering input to the tread animation immediately. If steering
        // is released, keep using the measured hull rotation until it settles.
        let yaw_speed = if self.steer.abs() > 0.01 {
            -self.steer * MAX_TURN_RATE_RAD_S
        } else {
            angular_velocity.dot(up)
        };

        let left_speed = forward_speed - yaw_speed * TRACK_HALF_GAUGE;
        let right_speed = forward_speed + yaw_speed * TRACK_HALF_GAUGE;
        let stop_noise = |speed: f32| if speed.abs() < 0.03 { 0.0 } else { speed };

        Vector2::new(stop_noise(left_speed), stop_noise(right_speed))
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
        let rest_length = self
            .suspension_rest_length
            .clamp(MAX_COMPRESSION, MAX_LENGTH + MAX_COMPRESSION);

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
                    .clamp(rest_length - MAX_COMPRESSION, MAX_LENGTH);
            }
        }
        self.contact_count = contacts.iter().filter(|contact| **contact).count() as i32;
        let angular_velocity = self.base().get_angular_velocity();
        let mass = self.base().get_mass();
        let friction = self.track_friction.max(0.0);
        let spring_rate = self.suspension_spring_rate.max(0.0);
        let damping_rate = self.suspension_damping_rate.max(0.0);
        let max_suspension_force = self.suspension_max_force.max(0.0);
        let mut spring_loads = [0.0_f32; WHEEL_COUNT];
        let mut normal_loads = [0.0_f32; WHEEL_COUNT];
        let mut lateral_forces = [Vector3::ZERO; WHEEL_COUNT];
        for index in 0..WHEEL_COUNT {
            if !contacts[index] {
                continue;
            }

            let contact = contact_positions[index];
            let force_offset = contact - transform.origin;
            let contact_velocity = velocity + angular_velocity.cross(force_offset);
            let normal = contact_normals[index];
            let compression = (rest_length - self.wheel_lengths[index]).clamp(0.0, MAX_COMPRESSION);
            let compression_speed = -contact_velocity.dot(normal);
            let spring_force = spring_rate * compression;
            let damping_force = damping_rate * compression_speed;
            let suspension_force = (spring_force + damping_force).clamp(0.0, max_suspension_force);
            spring_loads[index] = suspension_force;
            self.base_mut()
                .apply_force_ex(normal * suspension_force)
                .position(force_offset)
                .done();
        }

        // Estimate contact load from vehicle weight if the suspension rays
        // are at full extension; use spring loads when they are higher.
        for side_index in 0..2 {
            let first_wheel = side_index * WHEEL_COUNT_PER_SIDE;
            let side_range = first_wheel..first_wheel + WHEEL_COUNT_PER_SIDE;
            let mut side_contacts = 0;
            let mut normal_y_sum = 0.0;
            let mut spring_support = 0.0;
            for index in side_range.clone() {
                if contacts[index] {
                    side_contacts += 1;
                    normal_y_sum += contact_normals[index].y.max(0.0);
                    spring_support += spring_loads[index];
                }
            }
            if side_contacts == 0 {
                continue;
            }

            let average_normal_y = normal_y_sum / side_contacts as f32;
            let estimated_support = mass * 9.81 * average_normal_y * 0.5;
            let side_support = estimated_support.max(spring_support);
            for index in side_range {
                if !contacts[index] {
                    continue;
                }
                normal_loads[index] = if spring_support > estimated_support {
                    side_support * spring_loads[index] / spring_support
                } else {
                    side_support / side_contacts as f32
                };
            }
        }

        for index in 0..WHEEL_COUNT {
            if !contacts[index] {
                continue;
            }

            let contact = contact_positions[index];
            let force_offset = contact - transform.origin;
            let contact_velocity = velocity + angular_velocity.cross(force_offset);
            let normal = contact_normals[index];
            let lateral_axis = right - normal * right.dot(normal);
            if lateral_axis.length_squared() <= 0.0001 {
                continue;
            }
            let lateral_axis = lateral_axis.normalized();
            // Side slip and drive share the friction available at each contact.
            let lateral_speed = contact_velocity.dot(lateral_axis);
            let max_lateral_force = normal_loads[index] * friction * MAX_LATERAL_GRIP;
            let lateral_force_magnitude =
                (-lateral_speed * (mass / WHEEL_COUNT as f32) * LATERAL_GRIP)
                    .clamp(-max_lateral_force, max_lateral_force);
            let lateral_force = lateral_axis * lateral_force_magnitude;
            lateral_forces[index] = lateral_force;
            self.base_mut()
                .apply_force_ex(lateral_force)
                .position(force_offset)
                .done();
        }

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
            let side_load: f32 = normal_loads[first_wheel..first_wheel + WHEEL_COUNT_PER_SIDE]
                .iter()
                .sum();
            if side_load <= 0.0 {
                continue;
            }

            let requested_side_force = drive_acceleration * mass * 0.5;
            for wheel_index in first_wheel..first_wheel + WHEEL_COUNT_PER_SIDE {
                let normal_load = normal_loads[wheel_index];
                if normal_load <= 0.0 {
                    continue;
                }

                let normal = contact_normals[wheel_index];
                let drive_axis = forward - normal * forward.dot(normal);
                if drive_axis.length_squared() <= 0.0001 {
                    continue;
                }
                let drive_axis = drive_axis.normalized();
                let friction_capacity = normal_load * friction;
                let lateral_capacity = lateral_forces[wheel_index].length();
                let max_drive_force = (friction_capacity * friction_capacity
                    - lateral_capacity * lateral_capacity)
                    .max(0.0)
                    .sqrt();
                let requested_wheel_force = requested_side_force * normal_load / side_load;
                let drive_force =
                    drive_axis * requested_wheel_force.clamp(-max_drive_force, max_drive_force);
                let force_offset = contact_positions[wheel_index] - transform.origin;
                self.base_mut()
                    .apply_force_ex(drive_force)
                    .position(force_offset)
                    .done();
            }
        }
    }
}

pub fn track_radius() -> f32 {
    TRACK_RADIUS
}
