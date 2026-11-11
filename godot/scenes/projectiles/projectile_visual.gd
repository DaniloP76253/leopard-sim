extends Node3D

@export var projectile_mesh: Mesh
@export var projectile_material: Material
@export var tracer_color := Color(1.0, 0.72, 0.28, 0.9)
@export var model_center_forward := 0.4
@export_range(0.1, 1.0, 0.05) var minimum_display_time := 0.45
@export_range(0.05, 0.5, 0.05) var tracer_linger_time := 0.2

var _path := PackedVector3Array()
var _duration := 0.1
var _elapsed := 0.0
var _finished_time := 0.0
var _finished := false
var _model: MeshInstance3D
var _trail: MeshInstance3D
var _trail_mesh := ImmediateMesh.new()
var _trail_material: StandardMaterial3D

func _ready() -> void:
	_model = MeshInstance3D.new()
	_model.mesh = projectile_mesh
	_model.material_override = projectile_material
	_model.position.x = model_center_forward
	add_child(_model)

	_trail_material = StandardMaterial3D.new()
	_trail_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_trail_material.albedo_color = tracer_color
	_trail_material.emission_enabled = true
	_trail_material.emission = tracer_color
	_trail_material.emission_energy_multiplier = 2.5
	_trail_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_trail = MeshInstance3D.new()
	_trail.mesh = _trail_mesh
	_trail.material_override = _trail_material
	add_child(_trail)

func configure(points: Array, flight_time: float) -> void:
	_path.clear()
	for point in points:
		if point is Vector3:
			_path.append(point)
	if _path.size() < 2:
		queue_free()
		return
	_duration = maxf(flight_time, minimum_display_time)
	_elapsed = 0.0
	_update_pose(0.0)

func _sample_path(progress: float) -> Vector3:
	var scaled := clampf(progress, 0.0, 1.0) * float(_path.size() - 1)
	var index := mini(int(floor(scaled)), _path.size() - 2)
	return _path[index].lerp(_path[index + 1], scaled - float(index))

func _update_pose(progress: float) -> void:
	var position := _sample_path(progress)
	global_position = position
	var ahead := _sample_path(minf(progress + 0.01, 1.0))
	var direction := ahead - position
	if direction.length_squared() < 0.000001 and _path.size() > 1:
		direction = position - _sample_path(maxf(progress - 0.01, 0.0))
	if direction.length_squared() > 0.000001:
		var axis_x := direction.normalized()
		var axis_z := axis_x.cross(Vector3.UP)
		if axis_z.length_squared() < 0.000001:
			axis_z = axis_x.cross(Vector3.RIGHT)
		axis_z = axis_z.normalized()
		var axis_y := axis_z.cross(axis_x).normalized()
		global_basis = Basis(axis_x, axis_y, axis_z)
	_update_trail(progress, position)

func _update_trail(progress: float, current: Vector3) -> void:
	_trail_mesh.clear_surfaces()
	var trail_span := minf(tracer_linger_time / _duration, 1.0)
	var start_progress := maxf(0.0, progress - trail_span)
	_trail_mesh.surface_begin(Mesh.PRIMITIVE_LINE_STRIP, _trail_material)
	_trail_mesh.surface_add_vertex(to_local(_sample_path(start_progress)))
	var first_index := int(ceil(start_progress * float(_path.size() - 1)))
	var last_index := int(floor(progress * float(_path.size() - 1)))
	for index in range(first_index, last_index + 1):
		_trail_mesh.surface_add_vertex(to_local(_path[index]))
	_trail_mesh.surface_add_vertex(to_local(current))
	_trail_mesh.surface_end()

func _process(delta: float) -> void:
	if _path.size() < 2:
		return
	if not _finished:
		_elapsed += delta
		var progress := clampf(_elapsed / _duration, 0.0, 1.0)
		_update_pose(progress)
		if progress >= 1.0:
			_finished = true
			_finished_time = 0.0
	else:
		_finished_time += delta
		if _finished_time >= tracer_linger_time:
			queue_free()
