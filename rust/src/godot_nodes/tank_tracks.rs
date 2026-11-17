use std::collections::HashMap;
use std::f32::consts::TAU;

use godot::classes::{Engine, Mesh, MultiMesh, MultiMeshInstance3D, Node3D};
use godot::prelude::*;

const OUTLINE_SAMPLES_PER_WHEEL: usize = 48;
const ROAD_WHEELS_PER_SIDE: usize = 7;
const MAX_SUSPENSION_TRAVEL: f32 = 0.45;

#[derive(Clone, Copy)]
struct WheelCircle {
    center: Vector2,
    radius: f32,
}

#[derive(GodotClass)]
#[class(tool, base=Node3D)]
pub struct TankTracks {
    base: Base<Node3D>,
    wheel_rest_transforms: HashMap<String, Vec<Transform3D>>,

    #[export]
    pub track_mesh: Option<Gd<Mesh>>,
    #[export]
    pub link_count: i32,
    #[export]
    pub track_mesh_length: f32,
    #[export]
    pub link_half_thickness: f32,
    #[export]
    pub link_height: f32,
    #[export]
    pub link_width: f32,
    #[export]
    pub link_length_ratio: f32,
    #[export]
    pub track_slack: f32,
}

#[godot_api]
impl INode3D for TankTracks {
    fn init(base: Base<Node3D>) -> Self {
        Self {
            base,
            wheel_rest_transforms: HashMap::new(),
            track_mesh: None,
            link_count: 56,
            track_mesh_length: 5.6,
            link_half_thickness: 0.054,
            link_height: 0.18,
            link_width: 0.36,
            link_length_ratio: 0.92,
            track_slack: 0.08,
        }
    }

    fn ready(&mut self) {
        self.create_track_multimeshes();
        self.capture_wheel_transforms();
        self.render_tracks(0.0, 0.0, &[]);

        if Engine::singleton().is_editor_hint() {
            self.base_mut().set_process(true);
        }
    }

    fn process(&mut self, _delta: f64) {
        if Engine::singleton().is_editor_hint() {
            // Rebuild the contour in the editor when wheels are repositioned.
            self.update_track_side("Left", 0.0);
            self.update_track_side("Right", 0.0);
        }
    }
}

#[godot_api]
impl TankTracks {
    #[func]
    pub fn update_player_track_state(
        &mut self,
        state: Dictionary<Variant, Variant>,
        wheel_compressions: PackedFloat32Array,
    ) {
        self.render_tracks(
            Self::state_float(&state, "left_phase"),
            Self::state_float(&state, "right_phase"),
            wheel_compressions.as_slice(),
        );
    }

    #[func]
    pub fn update_sim_track_state(&mut self, state: Dictionary<Variant, Variant>) {
        self.render_tracks(
            Self::state_float(&state, "left_phase"),
            Self::state_float(&state, "right_phase"),
            &[],
        );
    }
}

impl TankTracks {
    fn state_float(state: &Dictionary<Variant, Variant>, name: &str) -> f32 {
        let key = name.to_variant();
        let default = 0.0_f64.to_variant();
        state
            .get(&key)
            .unwrap_or(default)
            .try_to_relaxed::<f64>()
            .unwrap_or(0.0) as f32
    }

    fn create_track_multimeshes(&self) {
        for path in ["Left", "Right"] {
            let Some(mut multimesh) = self.multimesh(path) else {
                continue;
            };
            if let Some(mesh) = self.track_mesh.as_ref() {
                if multimesh.get_mesh().is_none() {
                    multimesh.set_mesh(mesh);
                }
            }
            let count = self.link_count.max(1);
            if multimesh.get_instance_count() != count {
                multimesh.set_instance_count(count);
            }
        }
    }

    fn multimesh(&self, path: &str) -> Option<Gd<MultiMesh>> {
        let node = self.base().get_node_or_null(path)?;
        let instance = node.try_cast::<MultiMeshInstance3D>().ok()?;
        instance.get_multimesh()
    }

    fn capture_wheel_transforms(&mut self) {
        self.wheel_rest_transforms.clear();

        for path in [
            "WheelsLeft",
            "WheelsRight",
            "SprocketLeft",
            "SprocketRight",
            "IdlerLeft",
            "IdlerRight",
            "ReturnLeft",
            "ReturnRight",
        ] {
            let Some(multimesh) = self.multimesh(path) else {
                continue;
            };
            let transforms = (0..multimesh.get_instance_count())
                .map(|index| multimesh.get_instance_transform(index))
                .collect();
            self.wheel_rest_transforms
                .insert(path.to_string(), transforms);
        }
    }

    fn render_tracks(&self, left_phase: f32, right_phase: f32, wheel_compressions: &[f32]) {
        self.update_wheels("WheelsLeft", left_phase, wheel_compressions, 0);
        self.update_wheels(
            "WheelsRight",
            right_phase,
            wheel_compressions,
            ROAD_WHEELS_PER_SIDE,
        );
        self.update_wheels("SprocketLeft", left_phase, &[], 0);
        self.update_wheels("SprocketRight", right_phase, &[], 0);
        self.update_wheels("IdlerLeft", left_phase, &[], 0);
        self.update_wheels("IdlerRight", right_phase, &[], 0);
        self.update_wheels("ReturnLeft", left_phase, &[], 0);
        self.update_wheels("ReturnRight", right_phase, &[], 0);

        // The belt follows the road wheels' current suspension positions.
        self.update_track_side("Left", left_phase);
        self.update_track_side("Right", right_phase);
    }

    fn append_wheel_circles(&self, path: &str, circles: &mut Vec<WheelCircle>) {
        let Some(node) = self.base().get_node_or_null(path) else {
            return;
        };
        let Ok(instance) = node.try_cast::<MultiMeshInstance3D>() else {
            return;
        };
        let Some(multimesh) = instance.get_multimesh() else {
            return;
        };
        let Some(mesh) = multimesh.get_mesh() else {
            return;
        };

        let mesh_size = mesh.get_aabb().size;
        let local_radius = mesh_size.x.max(mesh_size.z) * 0.5;
        if local_radius <= f32::EPSILON {
            return;
        }

        let node_transform = instance.get_transform();
        for index in 0..multimesh.get_instance_count() {
            let transform = node_transform * multimesh.get_instance_transform(index);
            let x_axis = transform.basis.col_a();
            let z_axis = transform.basis.col_c();
            let x_scale = (x_axis.x * x_axis.x + x_axis.y * x_axis.y).sqrt();
            let z_scale = (z_axis.x * z_axis.x + z_axis.y * z_axis.y).sqrt();
            let radius = local_radius * x_scale.max(z_scale) + self.link_half_thickness.max(0.0);

            circles.push(WheelCircle {
                center: Vector2::new(transform.origin.x, transform.origin.y),
                radius,
            });
        }
    }

    fn track_outline(&self, side: &str) -> Vec<Vector2> {
        let suffix = if side == "Left" { "Left" } else { "Right" };
        let mut circles = Vec::new();
        self.append_wheel_circles(&format!("Wheels{suffix}"), &mut circles);
        self.append_wheel_circles(&format!("Sprocket{suffix}"), &mut circles);
        self.append_wheel_circles(&format!("Idler{suffix}"), &mut circles);

        let mut points = Vec::with_capacity(circles.len() * OUTLINE_SAMPLES_PER_WHEEL);
        for circle in circles {
            for index in 0..OUTLINE_SAMPLES_PER_WHEEL {
                let angle = TAU * index as f32 / OUTLINE_SAMPLES_PER_WHEEL as f32;
                points.push(Vector2::new(
                    circle.center.x + angle.cos() * circle.radius,
                    circle.center.y + angle.sin() * circle.radius,
                ));
            }
        }

        points.sort_by(|a, b| a.x.total_cmp(&b.x).then(a.y.total_cmp(&b.y)));
        points.dedup_by(|a, b| (a.x - b.x).abs() < 0.00001 && (a.y - b.y).abs() < 0.00001);
        if points.len() < 3 {
            return Vec::new();
        }

        let mut lower = Vec::new();
        for point in &points {
            while lower.len() >= 2
                && Self::cross(lower[lower.len() - 2], lower[lower.len() - 1], *point) <= 0.0
            {
                lower.pop();
            }
            lower.push(*point);
        }

        let mut upper = Vec::new();
        for point in points.iter().rev() {
            while upper.len() >= 2
                && Self::cross(upper[upper.len() - 2], upper[upper.len() - 1], *point) <= 0.0
            {
                upper.pop();
            }
            upper.push(*point);
        }

        lower.pop();
        upper.pop();
        lower.extend(upper);

        // Positive phase runs along the upper span toward the front of the tank.
        if Self::signed_area(&lower) > 0.0 {
            lower.reverse();
        }

        // A small parabolic drop models slack in the unsupported upper run.
        let slack = self.track_slack.clamp(0.0, 0.3);
        if slack > 0.0 {
            let min_x = lower
                .iter()
                .map(|point| point.x)
                .fold(f32::INFINITY, f32::min);
            let max_x = lower
                .iter()
                .map(|point| point.x)
                .fold(f32::NEG_INFINITY, f32::max);
            let min_y = lower
                .iter()
                .map(|point| point.y)
                .fold(f32::INFINITY, f32::min);
            let max_y = lower
                .iter()
                .map(|point| point.y)
                .fold(f32::NEG_INFINITY, f32::max);
            let mid_y = (min_y + max_y) * 0.5;
            let width = (max_x - min_x).max(f32::EPSILON);

            for point in &mut lower {
                if point.y <= mid_y {
                    continue;
                }
                let along = ((point.x - min_x) / width).clamp(0.0, 1.0);
                point.y -= 4.0 * slack * along * (1.0 - along);
            }
        }
        lower
    }

    fn cross(origin: Vector2, a: Vector2, b: Vector2) -> f32 {
        (a.x - origin.x) * (b.y - origin.y) - (a.y - origin.y) * (b.x - origin.x)
    }

    fn signed_area(points: &[Vector2]) -> f32 {
        (0..points.len())
            .map(|index| {
                let current = points[index];
                let next = points[(index + 1) % points.len()];
                current.x * next.y - next.x * current.y
            })
            .sum::<f32>()
            * 0.5
    }

    fn outline_length(points: &[Vector2]) -> f32 {
        (0..points.len())
            .map(|index| points[index].distance_to(points[(index + 1) % points.len()]))
            .sum()
    }

    fn point_on_outline(points: &[Vector2], distance: f32, total: f32) -> Vector2 {
        if points.len() < 2 || total <= f32::EPSILON {
            return points.first().copied().unwrap_or(Vector2::ZERO);
        }

        let mut remaining = distance.rem_euclid(total);
        for index in 0..points.len() {
            let start = points[index];
            let end = points[(index + 1) % points.len()];
            let edge_length = start.distance_to(end);
            if edge_length <= f32::EPSILON {
                continue;
            }
            if remaining <= edge_length {
                return start + (end - start) * (remaining / edge_length);
            }
            remaining -= edge_length;
        }

        points[0]
    }

    fn update_track_side(&self, path: &str, phase: f32) {
        let outline = self.track_outline(path);
        if outline.len() < 3 {
            return;
        }

        let Some(mut multimesh) = self.multimesh(path) else {
            return;
        };
        let link_count = multimesh.get_instance_count().min(self.link_count.max(0));
        if link_count == 0 {
            return;
        }

        let total = Self::outline_length(&outline);
        if total <= f32::EPSILON {
            return;
        }
        let step = total / link_count as f32;
        let Some(mesh) = multimesh.get_mesh() else {
            return;
        };
        let bounds = mesh.get_aabb();
        let mesh_size = bounds.size;
        let mesh_length = self.track_mesh_length.abs().max(0.0001);
        let mesh_height = mesh_size.y.abs().max(0.0001);
        let mesh_width = mesh_size.z.abs().max(0.0001);
        let scale = Vector3::new(
            step * self.link_length_ratio.clamp(0.5, 1.0) / mesh_length,
            self.link_height.max(0.0001) / mesh_height,
            self.link_width.max(0.0001) / mesh_width,
        );
        let mesh_center = bounds.position + mesh_size * 0.5;

        for index in 0..link_count {
            let distance = phase + (index as f32 + 0.5) * step;
            let point = Self::point_on_outline(&outline, distance, total);
            let before = Self::point_on_outline(&outline, distance - 0.025, total);
            let after = Self::point_on_outline(&outline, distance + 0.025, total);
            let tangent = after - before;
            let angle = tangent.y.atan2(tangent.x);
            let rotation = Basis::from_axis_angle(Vector3::BACK, angle);
            let basis = Basis::from_cols(
                rotation.col_a() * scale.x,
                rotation.col_b() * scale.y,
                rotation.col_c() * scale.z,
            );
            let origin = Vector3::new(point.x, point.y, 0.0) - basis * mesh_center;
            multimesh.set_instance_transform(index, Transform3D::new(basis, origin));
        }
    }

    fn wheel_radius(&self, path: &str, index: i32) -> Option<f32> {
        let node = self.base().get_node_or_null(path)?;
        let instance = node.try_cast::<MultiMeshInstance3D>().ok()?;
        let multimesh = instance.get_multimesh()?;
        let mesh = multimesh.get_mesh()?;
        if index >= multimesh.get_instance_count() {
            return None;
        }

        let mesh_size = mesh.get_aabb().size;
        let local_radius = mesh_size.x.max(mesh_size.z) * 0.5;
        let transform = instance.get_transform() * multimesh.get_instance_transform(index);
        let x_axis = transform.basis.col_a();
        let z_axis = transform.basis.col_c();
        let x_scale = (x_axis.x * x_axis.x + x_axis.y * x_axis.y).sqrt();
        let z_scale = (z_axis.x * z_axis.x + z_axis.y * z_axis.y).sqrt();
        Some(local_radius * x_scale.max(z_scale))
    }

    fn update_wheels(
        &self,
        path: &str,
        phase: f32,
        wheel_compressions: &[f32],
        compression_offset: usize,
    ) {
        let Some(rest_transforms) = self.wheel_rest_transforms.get(path) else {
            return;
        };
        let Some(mut multimesh) = self.multimesh(path) else {
            return;
        };

        let rotation_axis = Vector3::BACK;
        for (index, rest_transform) in rest_transforms.iter().enumerate() {
            let radius = self
                .wheel_radius(path, index as i32)
                .unwrap_or(0.4)
                .max(0.0001);
            let angle = -phase / radius;
            let mut transform = *rest_transform;
            let compression = wheel_compressions
                .get(compression_offset + index)
                .copied()
                .unwrap_or(0.0)
                .clamp(0.0, MAX_SUSPENSION_TRAVEL);
            transform.origin.y += compression;
            transform.basis = Basis::from_axis_angle(rotation_axis, angle) * rest_transform.basis;
            multimesh.set_instance_transform(index as i32, transform);
        }
    }
}
