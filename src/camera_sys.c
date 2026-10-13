#include "camera_sys.h"
#include "math_ext.h"
#include "render_tank.h" // Para PLAYER_HEIGHT y tank_render_pos

void update_gunner_control_from_mouse(GunnerControl *ctrl, const Tank *player) {
    Vector2 delta = GetMouseDelta();
    ctrl->desired_turret_azimuth_deg = norm_deg(ctrl->desired_turret_azimuth_deg + delta.x * TURRET_MOUSE_SENS_DEG_PER_PX);
    ctrl->desired_gun_elevation_deg = clampd(ctrl->desired_gun_elevation_deg - delta.y * ELEVATION_MOUSE_SENS_DEG_PER_PX,
                                             player->gun_elevation_min_deg, player->gun_elevation_max_deg);
}

void turret_input_toward(const Tank *player, double desired_az_rel_deg, double desired_el_deg,
                         double dt, double *turret_input, double *elev_input) {
    double az_error = norm_deg(desired_az_rel_deg - player->turret_azimuth_deg);
    *turret_input = clampd(az_error / (player->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    double el_error = desired_el_deg - player->gun_elevation_deg;
    *elev_input = clampd(el_error / (player->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);
}

void update_chase_camera(Camera3D *camera, const Tank *player, const GunnerControl *ctrl, float *orbit_yaw_deg,
                         float *orbit_pitch_deg, float *orbit_distance, float kick) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        *orbit_yaw_deg = (float)norm_deg((double)*orbit_yaw_deg + delta.x * 0.18f);
        *orbit_pitch_deg -= delta.y * 0.12f;
    } else {
        float desired_yaw = (float)norm_deg(player->heading_deg + ctrl->desired_turret_azimuth_deg);
        *orbit_yaw_deg = lerp_angle_deg(*orbit_yaw_deg, desired_yaw, 0.40f);
        *orbit_pitch_deg = lerpf(*orbit_pitch_deg, CAMERA_BASE_PITCH, 0.04f);
    }

    *orbit_pitch_deg = clampd(*orbit_pitch_deg, -62.0f, -8.0f);
    *orbit_distance -= GetMouseWheelMove() * 1.2f;
    *orbit_distance = clampd(*orbit_distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);

    Vector3 player_pos = tank_render_pos(player, PLAYER_HEIGHT);
    float yaw_rad = (float)deg2rad(*orbit_yaw_deg);
    float pitch_rad = (float)deg2rad(*orbit_pitch_deg);

    Vector3 forward = { cosf(pitch_rad) * cosf(yaw_rad), sinf(pitch_rad), cosf(pitch_rad) * sinf(yaw_rad) };

    Vector3 desired_pos = vec3_add(player_pos, vec3_scale(forward, -*orbit_distance));
    desired_pos.y += 3.0f;

    float ground_at_cam = terrain_visual_height_at(vec2_make(desired_pos.x, desired_pos.z));
    if (desired_pos.y < ground_at_cam + 1.2f) desired_pos.y = ground_at_cam + 1.2f;

    camera->position = vec3_lerp(camera->position, desired_pos, 0.14f);
    camera->target = vec3_lerp(camera->target, vec3_add(player_pos, (Vector3){ 0.0f, 1.1f, 0.0f }), 0.20f);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera->fovy = lerpf(camera->fovy, CAMERA_CHASE_FOVY, 0.15f) + kick * 1.5f;
}

void update_gunner_camera(Camera3D *camera, const Tank *player, float kick) {
    Vector3 turret_pos = tank_render_pos(player, PLAYER_HEIGHT + 1.0f + (float)player->hull_bounce_m);
    Vector3 aim_dir = tank_boresight_dir(player);
    Vector3 eye = vec3_add(turret_pos, vec3_scale(aim_dir, 3.5f));

    camera->position = vec3_lerp(camera->position, eye, 0.35f);
    camera->target = vec3_add(camera->position, aim_dir);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera->fovy = lerpf(camera->fovy, GUNNER_SIGHT_FOVY, 0.15f) + kick * 3.0f;
}