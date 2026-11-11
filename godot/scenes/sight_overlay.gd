extends Control

@export var game_node: NodePath
var _game: TankGame

func _ready() -> void:
	if game_node:
		_game = get_node(game_node) as TankGame
	if not _game:
		_game = get_parent().get_parent() as TankGame

func _process(_delta: float) -> void:
	queue_redraw()

func _draw() -> void:
	if not _game:
		return

	var viewport_size = get_viewport_rect().size
	var w = viewport_size.x
	var h = viewport_size.y
	var cx = w * 0.5
	var cy = h * 0.5

	if _game.is_gunner_sight_active():
		# --- Gunner Sight Overlay ---
		var reticle_color = Color(0.24, 1.0, 0.47, 0.86)

		# The circle follows the mouse; the lines follow the physical gun.
		draw_arc(Vector2(cx, cy), 18.0, 0, TAU, 24, reticle_color, 1.5)
		var boresight_world = _game.get_boresight_target_point()
		if not get_viewport().get_camera_3d().is_position_behind(boresight_world):
			var gun_screen_pos = get_viewport().get_camera_3d().unproject_position(boresight_world)
			var line_length = 140.0
			draw_line(Vector2(gun_screen_pos.x - line_length, gun_screen_pos.y), Vector2(gun_screen_pos.x - 20.0, gun_screen_pos.y), reticle_color, 1.5)
			draw_line(Vector2(gun_screen_pos.x + 20.0, gun_screen_pos.y), Vector2(gun_screen_pos.x + line_length, gun_screen_pos.y), reticle_color, 1.5)
			draw_line(Vector2(gun_screen_pos.x, gun_screen_pos.y - line_length), Vector2(gun_screen_pos.x, gun_screen_pos.y - 20.0), reticle_color, 1.5)
			draw_line(Vector2(gun_screen_pos.x, gun_screen_pos.y + 20.0), Vector2(gun_screen_pos.x, gun_screen_pos.y + line_length), reticle_color, 1.5)

		# Stadia mil-ticks
		var tick_color = Color(reticle_color.r, reticle_color.g, reticle_color.b, 0.7)
		for i in range(-6, 7):
			if i == 0:
				continue
			var x = cx + i * 22.0
			var tick_h = 10.0 if (i % 3 == 0) else 5.0
			draw_line(Vector2(x, cy - tick_h), Vector2(x, cy + tick_h), tick_color, 1.0)

		# Elevation Tape on right side
		var tape_info = _game.get_elevation_tape_info()
		var gun_el = tape_info.x
		var min_el = tape_info.y
		var max_el = tape_info.z

		var tape_x = w - 60.0
		var tape_top = cy - 160.0
		var tape_bottom = cy + 160.0
		draw_line(Vector2(tape_x, tape_top), Vector2(tape_x, tape_bottom), Color(1.0, 1.0, 1.0, 0.5), 1.5)

		var span = max_el - min_el
		var elev_t = clamp((gun_el - min_el) / (span if span > 1e-6 else 1.0), 0.0, 1.0)
		var marker_y = tape_bottom - elev_t * (tape_bottom - tape_top)

		var tri_pts = PackedVector2Array([
			Vector2(tape_x - 10.0, marker_y),
			Vector2(tape_x + 4.0, marker_y - 6.0),
			Vector2(tape_x + 4.0, marker_y + 6.0)
		])
		draw_colored_polygon(tri_pts, Color.YELLOW)
		draw_string(ThemeDB.fallback_font, Vector2(tape_x + 8.0, marker_y + 4.0), "%.1f°" % gun_el, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color.YELLOW)

		var ammo_name = _game.get_hud_data().get("ammo_name", "")
		var sight_text = "GUNNER SIGHT  |  %s" % ammo_name
		draw_string(ThemeDB.fallback_font, Vector2(cx - 80.0, cy + 140.0), sight_text, HORIZONTAL_ALIGNMENT_CENTER, -1, 16, reticle_color)

	else:
		# --- Chase Camera Reticle ---
		var cam = get_viewport().get_camera_3d()
		if not cam:
			return

		# The circle is the mouse's requested aim point; the lines show the physical gun.
		draw_arc(Vector2(cx, cy), 18.0, 0, TAU, 24, Color.WHITE, 1.5)

		var boresight_world = _game.get_boresight_target_point()
		var aim_color = Color.WHITE if _game.has_line_of_sight() else Color(1.0, 0.25, 0.25, 1.0)

		if not cam.is_position_behind(boresight_world):
			var screen_pos = cam.unproject_position(boresight_world)
			if screen_pos.x >= 0 and screen_pos.x <= w and screen_pos.y >= 0 and screen_pos.y <= h:
				var length = 10.0
				draw_line(Vector2(screen_pos.x - length, screen_pos.y), Vector2(screen_pos.x - 3.0, screen_pos.y), aim_color, 1.5)
				draw_line(Vector2(screen_pos.x + 3.0, screen_pos.y), Vector2(screen_pos.x + length, screen_pos.y), aim_color, 1.5)
				draw_line(Vector2(screen_pos.x, screen_pos.y - length), Vector2(screen_pos.x, screen_pos.y - 3.0), aim_color, 1.5)
				draw_line(Vector2(screen_pos.x, screen_pos.y + 3.0), Vector2(screen_pos.x, screen_pos.y + length), aim_color, 1.5)
				return

		# Gun is off-screen / behind camera: show direction indicator
		var cam_fwd = -cam.global_transform.basis.z
		var to_aim = (boresight_world - cam.global_position).normalized()
		var cam_bearing = rad_to_deg(atan2(cam_fwd.z, cam_fwd.x))
		var aim_bearing = rad_to_deg(atan2(to_aim.z, to_aim.x))
		var rel = fposmod(aim_bearing - cam_bearing + 180.0, 360.0) - 180.0

		var label = "GUN >>" if rel > 0.0 else "<< GUN"
		var label_x = (w - 120.0) if rel > 0.0 else 24.0
		draw_string(ThemeDB.fallback_font, Vector2(label_x, cy), label, HORIZONTAL_ALIGNMENT_LEFT, -1, 20, aim_color)
