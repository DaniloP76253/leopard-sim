extends TankGame

@onready var player_tank: Node3D = $PlayerTank
@onready var player_hull: Node3D = $PlayerTank/Hull
@onready var player_turret: Node3D = $PlayerTank/Turret
@onready var player_gun: Node3D = $PlayerTank/Turret/Gun

@onready var enemy_tank: Node3D = $EnemyTank
@onready var enemy_hull: Node3D = $EnemyTank/Hull
@onready var enemy_turret: Node3D = $EnemyTank/Turret
@onready var enemy_gun: Node3D = $EnemyTank/Turret/Gun

@onready var chase_cam: Camera3D = $ChaseCamera
@onready var gunner_cam: Camera3D = $GunnerCamera

@onready var depot_node: Node3D = $Depot
@onready var hud_label: Label = $HUD/HUDPanel/Margin/VBox/InfoLabel
@onready var combat_label: Label = $HUD/HUDPanel/Margin/VBox/CombatLabel
@onready var sight_overlay: Control = $HUD/SightOverlay

var mouse_aim_delta: Vector2 = Vector2.ZERO
var orbit_yaw_deg: float = 0.0
var orbit_pitch_deg: float = -18.0
var orbit_distance: float = 18.0
var camera_kick: float = 0.0
var is_rmb_down: bool = false

func _ready() -> void:
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	var depot_pos = get_depot_position()
	if depot_node:
		depot_node.position = depot_pos

	chase_cam.current = true
	gunner_cam.current = false

func _input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		if is_rmb_down and not is_gunner_sight_active():
			# Free-look chase camera
			orbit_yaw_deg = wrapf(orbit_yaw_deg + event.relative.x * 0.18, -180.0, 180.0)
			orbit_pitch_deg = clamp(orbit_pitch_deg - event.relative.y * 0.12, -62.0, -8.0)
		else:
			# Gun aiming
			mouse_aim_delta += event.relative

	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT:
			is_rmb_down = event.pressed
		elif event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			orbit_distance = clamp(orbit_distance - 1.2, 8.0, 32.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			orbit_distance = clamp(orbit_distance + 1.2, 8.0, 32.0)
		elif event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			var shot = fire_player_shot()
			if shot.get("success", false):
				camera_kick = 1.0

	elif event is InputEventKey and event.pressed:
		if event.keycode == KEY_ESCAPE:
			if Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
				Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
			else:
				Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
		elif event.keycode == KEY_V:
			var sight = toggle_gunner_sight()
			if sight:
				gunner_cam.current = true
				chase_cam.current = false
			else:
				chase_cam.current = true
				gunner_cam.current = false
		elif event.keycode == KEY_SPACE:
			var shot = fire_player_shot()
			if shot.get("success", false):
				camera_kick = 1.0
		elif event.keycode == KEY_1:
			select_ammo(0)
		elif event.keycode == KEY_2:
			select_ammo(1)
		elif event.keycode == KEY_3:
			select_ammo(2)
		elif event.keycode == KEY_R:
			reset_game()
			camera_kick = 0.0

func _process(delta: float) -> void:
	# Driving controls
	var throttle := 0.0
	var steer := 0.0
	if Input.is_key_pressed(KEY_W):
		throttle += 1.0
	if Input.is_key_pressed(KEY_S):
		throttle -= 1.0
	if Input.is_key_pressed(KEY_D):
		steer += 1.0
	if Input.is_key_pressed(KEY_A):
		steer -= 1.0

	# Trim controls
	var trim_turret := 0.0
	var trim_elev := 0.0
	if Input.is_key_pressed(KEY_L):
		trim_turret += 1.0
	if Input.is_key_pressed(KEY_J):
		trim_turret -= 1.0
	if Input.is_key_pressed(KEY_I):
		trim_elev += 1.0
	if Input.is_key_pressed(KEY_K):
		trim_elev -= 1.0

	apply_player_input(throttle, steer, mouse_aim_delta.x, mouse_aim_delta.y, trim_turret, trim_elev, delta)
	mouse_aim_delta = Vector2.ZERO

func _physics_process(delta: float) -> void:
	# 1. Update Player Tank Visuals
	var p_pos = get_player_position()
	player_tank.position = p_pos

	var p_hdg_rad = deg_to_rad(-get_player_heading())
	var p_pr = get_player_pitch_roll()
	player_hull.rotation = Vector3(deg_to_rad(-p_pr.x), p_hdg_rad, deg_to_rad(p_pr.y))

	var p_turret_abs = deg_to_rad(-get_player_absolute_turret_azimuth())
	player_turret.position = Vector3(0.0, 0.8, 0.0)
	player_turret.rotation = Vector3(0.0, p_turret_abs, 0.0)

	var p_gun_el = deg_to_rad(get_player_gun_elevation())
	player_gun.rotation = Vector3(0.0, 0.0, -p_gun_el)

	# 2. Update Enemy Tank Visuals
	var t_alive = get_target_alive()
	enemy_tank.visible = t_alive
	if t_alive:
		var t_pos = get_target_position()
		enemy_tank.position = t_pos

		var t_hdg_rad = deg_to_rad(-get_target_heading())
		var t_pr = get_target_pitch_roll()
		enemy_hull.rotation = Vector3(deg_to_rad(-t_pr.x), t_hdg_rad, deg_to_rad(t_pr.y))

		var t_turret_abs = deg_to_rad(-get_target_absolute_turret_azimuth())
		enemy_turret.position = Vector3(0.0, 0.8, 0.0)
		enemy_turret.rotation = Vector3(0.0, t_turret_abs, 0.0)

		var t_gun_el = deg_to_rad(get_target_gun_elevation())
		enemy_gun.rotation = Vector3(0.0, 0.0, -t_gun_el)

	# 3. Update Camera Systems
	if is_gunner_sight_active():
		# --- Gunner View Optics ---
		var eye_pos = get_gunner_eye_pos()
		var aim_dir = get_player_boresight_dir()
		gunner_cam.global_position = gunner_cam.global_position.lerp(eye_pos, 0.35)
		gunner_cam.look_at(gunner_cam.global_position + aim_dir, Vector3.UP)
		gunner_cam.fov = lerp(gunner_cam.fov, 20.0 + camera_kick * 3.0, 0.15)
	else:
		# --- Chase Camera ---
		if not is_rmb_down:
			var desired_yaw = get_player_heading() + get_desired_turret_azimuth()
			orbit_yaw_deg = lerp_angle(deg_to_rad(orbit_yaw_deg), deg_to_rad(desired_yaw), 0.35)
			orbit_yaw_deg = rad_to_deg(orbit_yaw_deg)
			orbit_pitch_deg = lerp(orbit_pitch_deg, -18.0, 0.04)

		var yaw_rad = deg_to_rad(orbit_yaw_deg)
		var pitch_rad = deg_to_rad(orbit_pitch_deg)

		var forward_cam = Vector3(
			cos(pitch_rad) * cos(yaw_rad),
			sin(pitch_rad),
			cos(pitch_rad) * sin(yaw_rad)
		)

		var desired_cam_pos = p_pos - forward_cam * orbit_distance + Vector3(0.0, 3.0, 0.0)
		var ground_h = get_terrain_visual_height(desired_cam_pos.x, desired_cam_pos.z)
		if desired_cam_pos.y < ground_h + 1.2:
			desired_cam_pos.y = ground_h + 1.2

		chase_cam.global_position = chase_cam.global_position.lerp(desired_cam_pos, 0.20)
		chase_cam.look_at(p_pos + Vector3(0.0, 1.1, 0.0), Vector3.UP)
		chase_cam.fov = lerp(chase_cam.fov, 55.0 + camera_kick * 1.5, 0.15)

	camera_kick = max(0.0, camera_kick - delta * 4.0)

	# 4. Update HUD
	var hud = get_hud_data()
	var spd: float = hud.get("speed_kmh", 0.0)
	var rng: float = hud.get("range_to_target", 0.0)
	var los: bool = hud.get("has_los", false)
	var php: float = hud.get("player_hp", 0.0)
	var thp: float = hud.get("target_hp", 0.0)
	var ammo: String = hud.get("ammo_name", "")
	var count: int = hud.get("ammo_count", 0)
	var rld: float = hud.get("reload_timer", 0.0)
	var wave: int = hud.get("wave", 1)
	var kills: int = hud.get("kills", 0)
	var resup: bool = hud.get("is_resupplying", false)

	var text = "Leopard 2A7 Simulator (Godot + Rust)\n"
	text += "W/S: Drive | A/D: Steer | Mouse: Aim | V: Gunner Sight | RMB: Free Look | Scroll: Zoom\n"
	text += "LMB / Space: Fire | 1/2/3: Ammo | R: Reset match\n"
	text += "Speed: %.1f km/h | Range: %.0fm | LOS: %s\n" % [spd, rng, "CLEAR" if los else "BLOCKED"]
	text += "Ammo: %s (x%d) | Reload: %s | Player HP: %.0f/100 | Target HP: %.0f/100\n" % [
		ammo, count, "READY" if rld <= 0.0 else "%.1fs" % rld, php, thp
	]
	text += "Wave: %d | Kills: %d %s" % [wave, kills, " | [RESUPPLYING AT DEPOT]" if resup else ""]
	hud_label.text = text

	var shot_msg: String = hud.get("last_player_shot", "")
	combat_label.text = shot_msg
