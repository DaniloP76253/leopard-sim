@tool
extends TankGame

const SETTINGS_PATH := "user://settings.cfg"
const DEFAULT_MOUSE_SENSITIVITY := 3.2
const DEFAULT_CAMERA_SENSITIVITY := 1.0
const TRACK_LINK_COUNT := 56
const TRACK_HALF_GAUGE := 1.95
const PLAYER_TANK_RENDER_LAYER := 2
const ROAD_WHEEL_X := [-2.4, -1.6, -0.8, 0.0, 0.8, 1.6, 2.4]
const RETURN_ROLLER_X := [-1.6, -0.5, 0.5, 1.6]
const IDLER_X := 2.78
const SPROCKET_X := -2.78
const END_WHEEL_RADIUS := 0.70
const SPROCKET_RADIUS := END_WHEEL_RADIUS
const IDLER_RADIUS := END_WHEEL_RADIUS
const END_WHEEL_Y := -0.34          # centro de sprocket e idler
const ROAD_WHEEL_BASE_Y := -0.62    # centro de las ruedas de carga
const ROAD_WHEEL_RADIUS := 0.42
const RETURN_ROLLER_BASE_Y := 0.0   # centro real = BASE_Y + 0.12
const LINK_HALF_THICKNESS := 0.054
const TRACK_MESH_LENGTH := 5.6      # largo de track.obj en X
const TRACK_MESH = preload("res://assets/models/tank/leopard_2a7/models/track.obj")
const PLAYER_CAMO = preload("res://assets/models/tank/leopard_2a7/materials/player_camo.tres")
const ENEMY_CAMO = preload("res://assets/models/tank/leopard_2a7/materials/enemy_camo.tres")
const TANK_METAL = preload("res://assets/models/tank/leopard_2a7/materials/metal.tres")
const PROJECTILE_VISUAL_SCENES := [
	preload("res://scenes/projectiles/apfsds.tscn"),
	preload("res://scenes/projectiles/heat.tscn"),
	preload("res://scenes/projectiles/he.tscn")
]

func _editor_track_state() -> Dictionary:
	return {
		"left_phase": 0.0,
		"right_phase": 0.0,
		"left_compression": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
		"right_compression": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
	}

@onready var player_tank: Node3D = $PlayerTank
@onready var player_chassis: TankChassis = $PlayerTank
@onready var player_hull: Node3D = $PlayerTank/Visual/Hull
@onready var player_hull_mesh: MeshInstance3D = $PlayerTank/Visual/Hull/HullMesh
@onready var player_turret: Node3D = $PlayerTank/Visual/Turret
@onready var player_turret_mesh: MeshInstance3D = $PlayerTank/Visual/Turret/TurretMesh
@onready var player_gun: Node3D = $PlayerTank/Visual/Turret/Gun
@onready var player_mantlet_mesh: MeshInstance3D = $PlayerTank/Visual/Turret/Gun/MantletMesh
@onready var player_bore_evacuator: MeshInstance3D = $PlayerTank/Visual/Turret/Gun/BoreEvacuator
@onready var player_tracks_root: Node3D = $PlayerTank/Visual/Tracks
@onready var player_track_left: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/Left
@onready var player_track_right: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/Right
@onready var player_wheels_left: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/WheelsLeft
@onready var player_wheels_right: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/WheelsRight
@onready var player_sprocket_left: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/SprocketLeft
@onready var player_sprocket_right: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/SprocketRight
@onready var player_idler_left: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/IdlerLeft
@onready var player_idler_right: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/IdlerRight
@onready var player_return_left: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/ReturnLeft
@onready var player_return_right: MultiMeshInstance3D = $PlayerTank/Visual/Tracks/ReturnRight

@onready var enemy_tank: Node3D = $EnemyTank
@onready var enemy_hull: Node3D = $EnemyTank/Visual/Hull
@onready var enemy_hull_mesh: MeshInstance3D = $EnemyTank/Visual/Hull/HullMesh
@onready var enemy_turret: Node3D = $EnemyTank/Visual/Turret
@onready var enemy_turret_mesh: MeshInstance3D = $EnemyTank/Visual/Turret/TurretMesh
@onready var enemy_gun: Node3D = $EnemyTank/Visual/Turret/Gun
@onready var enemy_mantlet_mesh: MeshInstance3D = $EnemyTank/Visual/Turret/Gun/MantletMesh
@onready var enemy_bore_evacuator: MeshInstance3D = $EnemyTank/Visual/Turret/Gun/BoreEvacuator
@onready var enemy_tracks_root: Node3D = $EnemyTank/Visual/Tracks
@onready var enemy_track_left: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/Left
@onready var enemy_track_right: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/Right
@onready var enemy_wheels_left: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/WheelsLeft
@onready var enemy_wheels_right: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/WheelsRight
@onready var enemy_sprocket_left: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/SprocketLeft
@onready var enemy_sprocket_right: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/SprocketRight
@onready var enemy_idler_left: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/IdlerLeft
@onready var enemy_idler_right: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/IdlerRight
@onready var enemy_return_left: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/ReturnLeft
@onready var enemy_return_right: MultiMeshInstance3D = $EnemyTank/Visual/Tracks/ReturnRight

@onready var chase_cam: Camera3D = $ChaseCamera
@onready var gunner_cam: Camera3D = $GunnerCamera

@onready var depot_node: Node3D = $Depot
@onready var terrain_mesh: MeshInstance3D = $TerrainMesh
@onready var hud_label: Label = $HUD/HUDPanel/Margin/VBox/InfoLabel
@onready var combat_label: Label = $HUD/HUDPanel/Margin/VBox/CombatLabel
@onready var sight_overlay: Control = $HUD/SightOverlay
@onready var pause_menu: PanelContainer = $HUD/PauseMenu
@onready var sensitivity_slider: HSlider = $HUD/PauseMenu/Margin/VBox/SensitivitySlider
@onready var sensitivity_value_label: Label = $HUD/PauseMenu/Margin/VBox/SensitivityValue
@onready var camera_sensitivity_slider: HSlider = $HUD/PauseMenu/Margin/VBox/CameraSensitivitySlider
@onready var camera_sensitivity_value_label: Label = $HUD/PauseMenu/Margin/VBox/CameraSensitivityValue
@onready var invert_check: CheckButton = $HUD/PauseMenu/Margin/VBox/InvertMouse
@onready var resume_button: Button = $HUD/PauseMenu/Margin/VBox/ResumeButton
@onready var quit_button: Button = $HUD/PauseMenu/Margin/VBox/QuitButton

var mouse_aim_delta: Vector2 = Vector2.ZERO
var orbit_yaw_deg: float = 0.0
var orbit_pitch_deg: float = -18.0
var orbit_distance: float = 18.0
var camera_kick: float = 0.0
var gunner_camera_mount_in_gun: Vector3 = Vector3.ZERO
var is_rmb_down: bool = false
var mouse_sensitivity: float = DEFAULT_MOUSE_SENSITIVITY
var camera_sensitivity: float = DEFAULT_CAMERA_SENSITIVITY
var invert_mouse_y: bool = false

func _load_settings() -> void:
	var settings = ConfigFile.new()
	if settings.load(SETTINGS_PATH) == OK:
		mouse_sensitivity = settings.get_value("mouse", "sensitivity", DEFAULT_MOUSE_SENSITIVITY)
		camera_sensitivity = settings.get_value("mouse", "camera_sensitivity", DEFAULT_CAMERA_SENSITIVITY)
		invert_mouse_y = settings.get_value("mouse", "invert_y", false)

func _save_settings() -> void:
	var settings = ConfigFile.new()
	settings.set_value("mouse", "sensitivity", mouse_sensitivity)
	settings.set_value("mouse", "camera_sensitivity", camera_sensitivity)
	settings.set_value("mouse", "invert_y", invert_mouse_y)
	settings.save(SETTINGS_PATH)

func _make_menu_label(text: String) -> Label:
	var label = Label.new()
	label.text = text
	return label

func _build_pause_menu() -> void:
	pause_menu = PanelContainer.new()
	pause_menu.name = "PauseMenu"
	pause_menu.visible = false
	pause_menu.mouse_filter = Control.MOUSE_FILTER_STOP
	pause_menu.custom_minimum_size = Vector2(420.0, 0.0)
	$HUD.add_child(pause_menu)
	pause_menu.set_anchors_preset(Control.PRESET_CENTER)
	call_deferred("_center_pause_menu")

	var margin = MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 24)
	margin.add_theme_constant_override("margin_top", 20)
	margin.add_theme_constant_override("margin_right", 24)
	margin.add_theme_constant_override("margin_bottom", 20)
	pause_menu.add_child(margin)
	var layout = VBoxContainer.new()
	layout.add_theme_constant_override("separation", 10)
	margin.add_child(layout)

	var title = _make_menu_label("PAUSED")
	title.add_theme_font_size_override("font_size", 26)
	layout.add_child(title)
	layout.add_child(_make_menu_label("Mouse sensitivity (CS2 style, m_yaw 0.022)"))

	var sensitivity_slider = HSlider.new()
	sensitivity_slider.min_value = 0.1
	sensitivity_slider.max_value = 10.0
	sensitivity_slider.step = 0.1
	sensitivity_slider.value = mouse_sensitivity
	sensitivity_slider.value_changed.connect(_on_sensitivity_changed)
	layout.add_child(sensitivity_slider)
	sensitivity_value_label = _make_menu_label("")
	layout.add_child(sensitivity_value_label)
	_update_sensitivity_label()

	layout.add_child(_make_menu_label("Free-look sensitivity"))
	var camera_slider = HSlider.new()
	camera_slider.min_value = 0.1
	camera_slider.max_value = 3.0
	camera_slider.step = 0.1
	camera_slider.value = camera_sensitivity
	camera_slider.value_changed.connect(_on_camera_sensitivity_changed)
	layout.add_child(camera_slider)
	camera_sensitivity_value_label = _make_menu_label("")
	layout.add_child(camera_sensitivity_value_label)
	_update_camera_sensitivity_label()

	var invert_check = CheckButton.new()
	invert_check.text = "Invert vertical mouse"
	invert_check.button_pressed = invert_mouse_y
	invert_check.toggled.connect(_on_invert_mouse_toggled)
	resume_button.pressed.connect(_toggle_pause_menu)
	quit_button.pressed.connect(_quit_game)
	layout.add_child(invert_check)

	var resume_button = Button.new()
	resume_button.text = "Resume"
	resume_button.pressed.connect(_toggle_pause_menu)
	layout.add_child(resume_button)
	var quit_button = Button.new()
	quit_button.text = "Exit"
	quit_button.pressed.connect(_quit_game)
	layout.add_child(quit_button)

func _center_pause_menu() -> void:
	if pause_menu:
		pause_menu.position = -pause_menu.size * 0.5

func _update_sensitivity_label() -> void:
	if sensitivity_value_label:
		sensitivity_value_label.text = "%.1f  (%.3f degrees/count)" % [mouse_sensitivity, mouse_sensitivity * 0.022]

func _update_camera_sensitivity_label() -> void:
	if camera_sensitivity_value_label:
		camera_sensitivity_value_label.text = "%.1fx" % camera_sensitivity

func _on_sensitivity_changed(value: float) -> void:
	mouse_sensitivity = value
	_update_sensitivity_label()
	_save_settings()

func _on_camera_sensitivity_changed(value: float) -> void:
	camera_sensitivity = value
	_update_camera_sensitivity_label()
	_save_settings()

func _on_invert_mouse_toggled(enabled: bool) -> void:
	invert_mouse_y = enabled
	_save_settings()

func _toggle_pause_menu() -> void:
	pause_menu.visible = not pause_menu.visible
	if pause_menu.visible:
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
		is_rmb_down = false
		mouse_aim_delta = Vector2.ZERO
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _quit_game() -> void:
	_save_settings()
	get_tree().quit()

func _configure_track_instance(track_instance: MultiMeshInstance3D) -> void:
	var multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.instance_count = TRACK_LINK_COUNT
	multimesh.mesh = TRACK_MESH
	track_instance.multimesh = multimesh

func _configure_wheel_instance(wheel_instance: MultiMeshInstance3D, radius: float, height: float, count: int) -> void:
	var wheel_mesh = CylinderMesh.new()
	wheel_mesh.top_radius = radius
	wheel_mesh.bottom_radius = radius
	wheel_mesh.height = height
	wheel_mesh.radial_segments = 16
	var multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.instance_count = count
	multimesh.mesh = wheel_mesh
	wheel_instance.multimesh = multimesh

# --- Recorrido de la oruga -------------------------------------------------

func _bottom_y(x: float, ys: Array) -> float:
	# xs = [SPROCKET_X, ROAD_WHEEL_X..., IDLER_X] (ascendente), ys del mismo tamaño
	var count := ROAD_WHEEL_X.size() + 2
	var prev_x: float = SPROCKET_X
	for i in range(1, count):
		var next_x: float = IDLER_X if i == count - 1 else ROAD_WHEEL_X[i - 1]
		if x <= next_x:
			return lerpf(ys[i - 1], ys[i], inverse_lerp(prev_x, next_x, x))
		prev_x = next_x
	return ys[ys.size() - 1]

func _track_point(d: float, total: float, bottom_ys: Array) -> Vector2:
	var r := END_WHEEL_RADIUS + LINK_HALF_THICKNESS
	var run := IDLER_X - SPROCKET_X
	var arc := PI * r
	d = fposmod(d, total)
	if d < arc:                       # arco trasero: abajo -> arriba
		var a := 1.5 * PI - PI * (d / arc)
		return Vector2(SPROCKET_X + r * cos(a), END_WHEEL_Y + r * sin(a))
	d -= arc
	if d < run:                       # tramo superior (sobre los rodillos)
		return Vector2(SPROCKET_X + d, END_WHEEL_Y + r)
	d -= run
	if d < arc:                       # arco delantero: arriba -> abajo
		var a2 := 0.5 * PI - PI * (d / arc)
		return Vector2(IDLER_X + r * cos(a2), END_WHEEL_Y + r * sin(a2))
	d -= arc
	var x := IDLER_X - d              # tramo inferior (sigue las ruedas)
	return Vector2(x, _bottom_y(x, bottom_ys))

func _update_track_side(track_instance: MultiMeshInstance3D, phase: float, compression: Array, side_sign: float) -> void:
	var multimesh := track_instance.multimesh
	var r := END_WHEEL_RADIUS + LINK_HALF_THICKNESS
	var total := 2.0 * ((IDLER_X - SPROCKET_X) + PI * r)
	var step := total / float(TRACK_LINK_COUNT)

	var base_y := ROAD_WHEEL_BASE_Y - ROAD_WHEEL_RADIUS - LINK_HALF_THICKNESS
	var bottom_ys: Array = [base_y]
	for c in compression:
		bottom_ys.append(base_y + c)
	bottom_ys.append(base_y)

	var link_scale := Vector3(step * 1.05 / TRACK_MESH_LENGTH, 0.18, 0.36)
	for index in range(TRACK_LINK_COUNT):
		var d := phase + (index + 0.5) * step
		var p := _track_point(d, total, bottom_ys)
		var tangent := _track_point(d + 0.03, total, bottom_ys) - _track_point(d - 0.03, total, bottom_ys)
		var link_basis := Basis(Vector3.BACK, atan2(tangent.y, tangent.x)).scaled_local(link_scale)
		multimesh.set_instance_transform(index, Transform3D(link_basis, Vector3(p.x, p.y, TRACK_HALF_GAUGE * side_sign)))

# --- Ruedas -----------------------------------------------------------------

func _update_wheels(wheel_instance: MultiMeshInstance3D, compression: Array, side_sign: float, phase: float) -> void:
	var multimesh = wheel_instance.multimesh
	for index in range(ROAD_WHEEL_X.size()):
		var wheel_basis = Basis(Vector3(0.0, 0.0, 1.0), -phase / ROAD_WHEEL_RADIUS) * Basis(Vector3.RIGHT, PI * 0.5)
		var wheel_position = Vector3(
			ROAD_WHEEL_X[index],
			ROAD_WHEEL_BASE_Y + compression[index],
			TRACK_HALF_GAUGE * side_sign
		)
		multimesh.set_instance_transform(index, Transform3D(wheel_basis, wheel_position))

func _update_role_wheel(wheel_instance: MultiMeshInstance3D, x: float, y: float, side_sign: float, phase: float, radius: float) -> void:
	var wheel_basis = Basis(Vector3(0.0, 0.0, 1.0), -phase / radius) * Basis(Vector3.RIGHT, PI * 0.5)
	wheel_instance.multimesh.set_instance_transform(0, Transform3D(wheel_basis, Vector3(x, y, TRACK_HALF_GAUGE * side_sign)))

func _update_return_rollers(wheel_instance: MultiMeshInstance3D, side_sign: float, phase: float) -> void:
	var wheel_basis = Basis(Vector3(0.0, 0.0, 1.0), -phase / 0.24) * Basis(Vector3.RIGHT, PI * 0.5)
	for index in range(RETURN_ROLLER_X.size()):
		var roller_position = Vector3(RETURN_ROLLER_X[index], RETURN_ROLLER_BASE_Y + 0.12, TRACK_HALF_GAUGE * side_sign)
		wheel_instance.multimesh.set_instance_transform(index, Transform3D(wheel_basis, roller_position))

func _update_tracks(root: Node3D, left: MultiMeshInstance3D, right: MultiMeshInstance3D, wheels_left: MultiMeshInstance3D, wheels_right: MultiMeshInstance3D, sprocket_left: MultiMeshInstance3D, sprocket_right: MultiMeshInstance3D, idler_left: MultiMeshInstance3D, idler_right: MultiMeshInstance3D, return_left: MultiMeshInstance3D, return_right: MultiMeshInstance3D, state: Dictionary, hull_rotation: Vector3) -> void:
	root.rotation = hull_rotation
	var left_compression = state.get("left_compression", [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
	var right_compression = state.get("right_compression", [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
	var left_phase = state.get("left_phase", 0.0)
	var right_phase = state.get("right_phase", 0.0)
	_update_track_side(left, left_phase, left_compression, -1.0)
	_update_track_side(right, right_phase, right_compression, 1.0)
	_update_wheels(wheels_left, left_compression, -1.0, left_phase)
	_update_wheels(wheels_right, right_compression, 1.0, right_phase)
	_update_role_wheel(sprocket_left, SPROCKET_X, END_WHEEL_Y, -1.0, left_phase, SPROCKET_RADIUS)
	_update_role_wheel(sprocket_right, SPROCKET_X, END_WHEEL_Y, 1.0, right_phase, SPROCKET_RADIUS)
	_update_role_wheel(idler_left, IDLER_X, END_WHEEL_Y, -1.0, left_phase, IDLER_RADIUS)
	_update_role_wheel(idler_right, IDLER_X, END_WHEEL_Y, 1.0, right_phase, IDLER_RADIUS)
	_update_return_rollers(return_left, -1.0, left_phase)
	_update_return_rollers(return_right, 1.0, right_phase)

func _player_track_state_from_suspension() -> Dictionary:
	var state: Dictionary = get_player_track_state()
	var left_compression: Array = []
	var right_compression: Array = []
	for index in range(ROAD_WHEEL_X.size()):
		left_compression.append(player_chassis.get_wheel_compression(index))
		right_compression.append(player_chassis.get_wheel_compression(index + ROAD_WHEEL_X.size()))
	state["left_compression"] = left_compression
	state["right_compression"] = right_compression
	return state

func _ready() -> void:
	set_process_input(true)
	if Engine.is_editor_hint():
		_configure_track_instance(player_track_left)
		_configure_track_instance(player_track_right)
		_configure_wheel_instance(player_wheels_left, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
		_configure_wheel_instance(player_wheels_right, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
		_configure_wheel_instance(player_sprocket_left, SPROCKET_RADIUS, 0.22, 1)
		_configure_wheel_instance(player_sprocket_right, SPROCKET_RADIUS, 0.22, 1)
		_configure_wheel_instance(player_idler_left, IDLER_RADIUS, 0.20, 1)
		_configure_wheel_instance(player_idler_right, IDLER_RADIUS, 0.20, 1)
		_configure_wheel_instance(player_return_left, 0.24, 0.12, RETURN_ROLLER_X.size())
		_configure_wheel_instance(player_return_right, 0.24, 0.12, RETURN_ROLLER_X.size())
		_configure_track_instance(enemy_track_left)
		_configure_track_instance(enemy_track_right)
		_configure_wheel_instance(enemy_wheels_left, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
		_configure_wheel_instance(enemy_wheels_right, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
		_configure_wheel_instance(enemy_sprocket_left, SPROCKET_RADIUS, 0.22, 1)
		_configure_wheel_instance(enemy_sprocket_right, SPROCKET_RADIUS, 0.22, 1)
		_configure_wheel_instance(enemy_idler_left, IDLER_RADIUS, 0.20, 1)
		_configure_wheel_instance(enemy_idler_right, IDLER_RADIUS, 0.20, 1)
		_configure_wheel_instance(enemy_return_left, 0.24, 0.12, RETURN_ROLLER_X.size())
		_configure_wheel_instance(enemy_return_right, 0.24, 0.12, RETURN_ROLLER_X.size())
		_update_tracks(player_tracks_root, player_track_left, player_track_right, player_wheels_left, player_wheels_right, player_sprocket_left, player_sprocket_right, player_idler_left, player_idler_right, player_return_left, player_return_right, _editor_track_state(), Vector3.ZERO)
		_update_tracks(enemy_tracks_root, enemy_track_left, enemy_track_right, enemy_wheels_left, enemy_wheels_right, enemy_sprocket_left, enemy_sprocket_right, enemy_idler_left, enemy_idler_right, enemy_return_left, enemy_return_right, _editor_track_state(), Vector3.ZERO)
		return
	_load_settings()
	player_chassis.rotation = Vector3.ZERO
	# La caja del chasis queda a ~0.2 m del suelo; los rayos alcanzan la altura de reposo.
	player_chassis.global_position = get_player_position() + Vector3(0.0, -0.35, 0.0)
	# La posición puesta en el editor se interpreta relativa al casco. Convertirla
	# a coordenadas del pivote del cañón para que siga el giro y la elevación.
	player_turret.position = Vector3(0.0, 0.8, 0.0)
	player_turret.rotation = Vector3.ZERO
	player_gun.rotation = Vector3.ZERO
	var bore_evacuator_offset := float(get_bore_evacuator_offset_m())
	player_bore_evacuator.position.x = bore_evacuator_offset
	enemy_bore_evacuator.position.x = bore_evacuator_offset
	var camera_mount_hull_local := gunner_cam.position
	var camera_mount_world := player_chassis.to_global(camera_mount_hull_local)
	gunner_camera_mount_in_gun = player_gun.to_local(camera_mount_world)
	for visual in player_tank.find_children("*", "VisualInstance3D", true, false):
		visual.layers = PLAYER_TANK_RENDER_LAYER
	chase_cam.cull_mask = 1 | PLAYER_TANK_RENDER_LAYER
	gunner_cam.cull_mask = 1
	terrain_mesh.create_trimesh_collision()
	player_hull_mesh.material_override = PLAYER_CAMO
	player_turret_mesh.material_override = PLAYER_CAMO
	player_mantlet_mesh.material_override = PLAYER_CAMO
	player_bore_evacuator.material_override = TANK_METAL
	enemy_hull_mesh.material_override = ENEMY_CAMO
	enemy_turret_mesh.material_override = ENEMY_CAMO
	enemy_mantlet_mesh.material_override = ENEMY_CAMO
	enemy_bore_evacuator.material_override = TANK_METAL
	pause_menu.visible = false
	sensitivity_slider.value = mouse_sensitivity
	camera_sensitivity_slider.value = camera_sensitivity
	invert_check.button_pressed = invert_mouse_y
	sensitivity_slider.value_changed.connect(_on_sensitivity_changed)
	camera_sensitivity_slider.value_changed.connect(_on_camera_sensitivity_changed)
	invert_check.toggled.connect(_on_invert_mouse_toggled)
	_update_sensitivity_label()
	_update_camera_sensitivity_label()
	_configure_track_instance(player_track_left)
	_configure_track_instance(player_track_right)
	_configure_wheel_instance(player_wheels_left, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
	_configure_wheel_instance(player_wheels_right, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
	_configure_wheel_instance(player_sprocket_left, SPROCKET_RADIUS, 0.22, 1)
	_configure_wheel_instance(player_sprocket_right, SPROCKET_RADIUS, 0.22, 1)
	_configure_wheel_instance(player_idler_left, IDLER_RADIUS, 0.20, 1)
	_configure_wheel_instance(player_idler_right, IDLER_RADIUS, 0.20, 1)
	_configure_wheel_instance(player_return_left, 0.24, 0.12, RETURN_ROLLER_X.size())
	_configure_wheel_instance(player_return_right, 0.24, 0.12, RETURN_ROLLER_X.size())
	_configure_track_instance(enemy_track_left)
	_configure_track_instance(enemy_track_right)
	_configure_wheel_instance(enemy_wheels_left, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
	_configure_wheel_instance(enemy_wheels_right, ROAD_WHEEL_RADIUS, 0.18, ROAD_WHEEL_X.size())
	_configure_wheel_instance(enemy_sprocket_left, SPROCKET_RADIUS, 0.22, 1)
	_configure_wheel_instance(enemy_sprocket_right, SPROCKET_RADIUS, 0.22, 1)
	_configure_wheel_instance(enemy_idler_left, IDLER_RADIUS, 0.20, 1)
	_configure_wheel_instance(enemy_idler_right, IDLER_RADIUS, 0.20, 1)
	_configure_wheel_instance(enemy_return_left, 0.24, 0.12, RETURN_ROLLER_X.size())
	_configure_wheel_instance(enemy_return_right, 0.24, 0.12, RETURN_ROLLER_X.size())
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	var depot_pos = get_depot_position()
	if depot_node:
		depot_node.position = depot_pos

	chase_cam.current = true
	gunner_cam.current = false

func _input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		if pause_menu and pause_menu.visible:
			return
		var is_free_look = is_rmb_down and not is_gunner_sight_active()
		if not is_free_look:
			var mouse_y = -event.relative.y if invert_mouse_y else event.relative.y
			mouse_aim_delta += Vector2(event.relative.x, mouse_y) * mouse_sensitivity
		if is_free_look:
			# Free-look chase camera
			orbit_yaw_deg = wrapf(orbit_yaw_deg + event.relative.x * 0.18 * camera_sensitivity, -180.0, 180.0)
			orbit_pitch_deg = clamp(orbit_pitch_deg - event.relative.y * 0.12 * camera_sensitivity, -65.0, 30.0)

	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT:
			is_rmb_down = event.pressed
		elif event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			orbit_distance = clamp(orbit_distance - 1.2, 8.0, 32.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			orbit_distance = clamp(orbit_distance + 1.2, 8.0, 32.0)
		elif event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			var shot = fire_player_shot()
			if shot.get("fired", false):
				camera_kick = 1.0
				_show_projectile(shot)

	elif event is InputEventKey and event.pressed:
		if event.keycode == KEY_ESCAPE:
			_toggle_pause_menu()
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
			if shot.get("fired", false):
				camera_kick = 1.0
				_show_projectile(shot)
		elif event.keycode == KEY_1:
			select_ammo(0)
		elif event.keycode == KEY_2:
			select_ammo(1)
		elif event.keycode == KEY_3:
			select_ammo(2)
		elif event.keycode == KEY_R:
			reset_game()
			camera_kick = 0.0

func _show_projectile(shot: Dictionary) -> void:
	var trajectory: Array = shot.get("trajectory", [])
	if trajectory.size() < 2:
		return
	var ammo_type := clampi(int(shot.get("ammo_type", 0)), 0, PROJECTILE_VISUAL_SCENES.size() - 1)
	var projectile = PROJECTILE_VISUAL_SCENES[ammo_type].instantiate()
	add_child(projectile)
	projectile.configure(trajectory, float(shot.get("flight_time_s", 0.1)))

func _process(delta: float) -> void:
	if pause_menu and pause_menu.visible:
		mouse_aim_delta = Vector2.ZERO
		return

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
	player_chassis.set_drive_input(throttle, steer)

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
	if Engine.is_editor_hint():
		_update_tracks(player_tracks_root, player_track_left, player_track_right, player_wheels_left, player_wheels_right, player_sprocket_left, player_sprocket_right, player_idler_left, player_idler_right, player_return_left, player_return_right, _editor_track_state(), Vector3.ZERO)
		_update_tracks(enemy_tracks_root, enemy_track_left, enemy_track_right, enemy_wheels_left, enemy_wheels_right, enemy_sprocket_left, enemy_sprocket_right, enemy_idler_left, enemy_idler_right, enemy_return_left, enemy_return_right, _editor_track_state(), Vector3.ZERO)
		return
	if pause_menu and pause_menu.visible:
		return

	var player_forward := player_chassis.global_transform.basis.x
	var player_forward_speed := player_chassis.linear_velocity.dot(player_forward)
	var player_heading := rad_to_deg(atan2(player_forward.z, player_forward.x))
	var player_pitch := rad_to_deg(atan2(player_forward.y, Vector2(player_forward.x, player_forward.z).length()))
	sync_player_chassis_state(player_chassis.global_position, player_heading, player_forward_speed, player_pitch, delta)
	player_chassis.simulate_physics_step(delta)
	advance_simulation_from_godot(delta)

	# 1. Update Player Tank Visuals
	# El Visual del jugador es hijo del RigidBody, que ya se inclina/gira:
	# casco y orugas no llevan rotación propia, y la torreta es relativa al casco.
	var p_pos = get_player_position()

	player_hull.rotation = Vector3.ZERO
	_update_tracks(player_tracks_root, player_track_left, player_track_right, player_wheels_left, player_wheels_right, player_sprocket_left, player_sprocket_right, player_idler_left, player_idler_right, player_return_left, player_return_right, _player_track_state_from_suspension(), Vector3.ZERO)

	var p_turret_rel = deg_to_rad(-get_player_turret_azimuth())
	player_turret.position = Vector3(0.0, 0.8, 0.0)
	player_turret.rotation = Vector3(0.0, p_turret_rel, 0.0)

	var p_gun_el = deg_to_rad(get_player_gun_elevation())
	player_gun.rotation = Vector3(0.0, 0.0, p_gun_el)
	var bore_direction := player_gun.global_transform.basis.x.normalized()
	sync_player_gun_bore(player_gun.global_position, bore_direction)

	# 2. Update Enemy Tank Visuals
	var t_alive = get_target_alive()
	enemy_tank.visible = t_alive
	if t_alive:
		var t_pos = get_target_position()
		enemy_tank.position = t_pos

		var t_hdg_rad = deg_to_rad(-get_target_heading())
		var t_pr = get_target_pitch_roll()
		enemy_hull.rotation = Vector3(deg_to_rad(-t_pr.x), t_hdg_rad, deg_to_rad(t_pr.y))
		_update_tracks(enemy_tracks_root, enemy_track_left, enemy_track_right, enemy_wheels_left, enemy_wheels_right, enemy_sprocket_left, enemy_sprocket_right, enemy_idler_left, enemy_idler_right, enemy_return_left, enemy_return_right, get_target_track_state(), enemy_hull.rotation)

		var t_turret_abs = deg_to_rad(-get_target_absolute_turret_azimuth())
		enemy_turret.position = Vector3(0.0, 0.8, 0.0)
		enemy_turret.rotation = Vector3(0.0, t_turret_abs, 0.0)

		var t_gun_el = deg_to_rad(get_target_gun_elevation())
		enemy_gun.rotation = Vector3(0.0, 0.0, t_gun_el)

	# 3. Update Camera Systems
	if is_gunner_sight_active():
		# --- Gunner View Optics ---
		var gun_direction := player_gun.global_transform.basis.x
		var eye_pos := player_gun.to_global(gunner_camera_mount_in_gun)
		gunner_cam.global_position = eye_pos
		gunner_cam.look_at(eye_pos + gun_direction * 260.0, player_chassis.global_transform.basis.y)
		gunner_cam.fov = lerp(gunner_cam.fov, 20.0 + camera_kick * 3.0, 0.15)
	else:
		# --- Chase Camera ---
		if not is_rmb_down:
			var desired_yaw = get_player_heading() + get_desired_turret_azimuth()
			orbit_yaw_deg = lerp_angle(deg_to_rad(orbit_yaw_deg), deg_to_rad(desired_yaw), 0.35)
			orbit_yaw_deg = rad_to_deg(orbit_yaw_deg)
			var desired_pitch = clamp(-18.0 + get_player_gun_elevation() * 1.9, -65.0, 30.0)
			orbit_pitch_deg = lerp(orbit_pitch_deg, desired_pitch, 0.12)

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

		var camera_anchor = p_pos + Vector3(0.0, 1.1, 0.0)
		var camera_query = PhysicsRayQueryParameters3D.create(camera_anchor, desired_cam_pos)
		camera_query.exclude = [player_chassis.get_rid()]
		camera_query.collision_mask = 1  # solo terreno, nunca el propio tanque
		var camera_hit = get_world_3d().direct_space_state.intersect_ray(camera_query)
		if not camera_hit.is_empty():
			desired_cam_pos = camera_hit.position + camera_hit.normal * 0.35

		chase_cam.global_position = chase_cam.global_position.lerp(desired_cam_pos, 0.20)
		var chase_focus = p_pos + Vector3(0.0, 1.1, 0.0)
		if not is_rmb_down:
			chase_focus = get_desired_boresight_target_point()
		chase_cam.look_at(chase_focus, Vector3.UP)
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
	var player_turret_azimuth: float = hud.get("player_turret_azimuth", 0.0)
	var player_gun_elevation: float = hud.get("player_gun_elevation", 0.0)
	var target_turret_azimuth: float = hud.get("target_turret_azimuth", 0.0)
	var target_gun_elevation: float = hud.get("target_gun_elevation", 0.0)
	var desired_turret_azimuth: float = hud.get("desired_turret_azimuth", 0.0)
	var desired_gun_elevation: float = hud.get("desired_gun_elevation", 0.0)
	var sim_time: float = hud.get("sim_time", 0.0)
	var suspension_contacts: int = player_chassis.get_suspension_contact_count()

	var text = "Leopard 2A7 Simulator (Godot + Rust)\n"
	text += "W/S: Drive | A/D: Steer | Mouse: Aim | V: Gunner Sight | RMB: Free Look | Scroll: Zoom\n"
	text += "LMB / Space: Fire | 1/2/3: Ammo | R: Reset match\n"
	text += "Speed: %.1f km/h | Range: %.0fm | LOS: %s\n" % [spd, rng, "CLEAR" if los else "BLOCKED"]
	text += "Ammo: %s (x%d) | Reload: %s | Player HP: %.0f/100 | Target HP: %.0f/100\n" % [
		ammo, count, "READY" if rld <= 0.0 else "%.1fs" % rld, php, thp
	]
	text += "Player turret/gun: %.1f° / %.1f° | Enemy turret/gun: %.1f° / %.1f°\n" % [
		player_turret_azimuth, player_gun_elevation, target_turret_azimuth, target_gun_elevation
	]
	text += "Desired: %.1f° / %.1f° | Sim: %.1fs\n" % [
		desired_turret_azimuth, desired_gun_elevation, sim_time
	]
	text += "Suspension contacts: %d/14\n" % suspension_contacts
	text += "Wave: %d | Kills: %d %s" % [wave, kills, " | [RESUPPLYING AT DEPOT]" if resup else ""]
	hud_label.text = text

	var shot_msg: String = hud.get("last_player_shot", "")
	combat_label.text = shot_msg
