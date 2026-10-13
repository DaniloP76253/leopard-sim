#include "render_tank.h"
#include "math_ext.h"
#include "terrain.h"
#include <math.h>

static const float ROAD_WHEEL_X[5] = { -1.95f, -0.98f, 0.00f, 0.98f, 1.95f };
static const float ROAD_WHEEL_RADIUS = 0.42f;
static const float ROAD_WHEEL_BASE_Y = -0.62f;
static const float RETURN_ROLLER_X[2] = { -0.92f, 0.92f };
static const float RETURN_ROLLER_RADIUS = 0.24f;
static const float RETURN_ROLLER_BASE_Y = 0.34f;
static const float IDLER_X = -2.78f;
static const float SPROCKET_X = 2.78f;
static const float IDLER_RADIUS = 0.66f;
static const float SPROCKET_RADIUS = 0.78f;
static const float TRACK_HALF_GAUGE = 1.95f;
static const int TRACK_LINK_COUNT = 56;
static const Vector3 TRACK_LINK_SCALE = { 0.10f, 0.16f, 0.33f };

float terrain_visual_height_at(Vec2 pos) {
    double logical = terrain_height_at(pos);
    double normalized = clampd((logical + 5.0) / 12.0, 0.0, 1.0);
    return (float)(normalized * 20.0);
}

Vector3 tank_render_pos(const Tank *t, float extra_height) {
    float ground = terrain_visual_height_at(t->position_m);
    return (Vector3){ (float)t->position_m.x, ground + extra_height, (float)t->position_m.y };
}

Vector3 tank_combat_pos(const Tank *t, float extra_height) {
    float ground = (float)terrain_height_at(t->position_m);
    return (Vector3){ (float)t->position_m.x, ground + extra_height, (float)t->position_m.y };
}

Vector3 tank_boresight_dir(const Tank *t) {
    float az = (float)deg2rad(tank_absolute_turret_azimuth_deg(t));
    float el = (float)deg2rad(t->gun_elevation_deg);
    return (Vector3){ cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az) };
}

void update_chassis_pose(Tank *t) {
    double heading = deg2rad(t->heading_deg);
    Vec2 forward = vec2_make(cos(heading), sin(heading));
    Vec2 right = vec2_make(-sin(heading), cos(heading));

    double center = terrain_height_at(t->position_m);
    double front = terrain_height_at(vec2_add(t->position_m, vec2_scale(forward, 3.0)));
    double back = terrain_height_at(vec2_add(t->position_m, vec2_scale(forward, -3.0)));
    double left = terrain_height_at(vec2_add(t->position_m, vec2_scale(right, -2.2)));
    double right_h = terrain_height_at(vec2_add(t->position_m, vec2_scale(right, 2.2)));

    double target_pitch = rad2deg(atan2(front - back, 6.0));
    double target_roll = rad2deg(atan2(right_h - left, 4.4));

    t->hull_pitch_deg = lerpf((float)t->hull_pitch_deg, (float)target_pitch, 0.12f);
    t->hull_roll_deg  = lerpf((float)t->hull_roll_deg,  (float)target_roll,  0.12f);
    t->hull_bounce_m  = lerpf((float)t->hull_bounce_m,  (float)((center - floor(center)) * 0.15), 0.08f);

    Vector3 hull_pos = tank_render_pos(t, PLAYER_HEIGHT + (float)t->hull_bounce_m);
    for (int side = 0; side < 2; side++) {
        float side_sign = (side == 0) ? -1.0f : 1.0f;
        Vec2 side_offset = vec2_scale(right, 1.9 * side_sign);
        for (int i = 0; i < 5; i++) {
            Vec2 wheel_pos = vec2_add(t->position_m, vec2_add(vec2_scale(forward, ROAD_WHEEL_X[i]), side_offset));
            float ground = terrain_visual_height_at(wheel_pos);
            float neutral_center_y = hull_pos.y + ROAD_WHEEL_BASE_Y;
            float desired_center_y = ground + ROAD_WHEEL_RADIUS;
            double compression = desired_center_y - neutral_center_y;
            if (compression < 0.0) compression = 0.0;
            if (compression > 0.55) compression = 0.55;
            t->road_wheel_compression_m[side][i] = compression;
        }
    }
}

static void hull_basis_vectors(const Tank *t, Vector3 *out_forward, Vector3 *out_right) {
    float h = (float)deg2rad(t->heading_deg);
    *out_forward = (Vector3){ cosf(h), 0.0f, sinf(h) };
    *out_right   = (Vector3){ -sinf(h), 0.0f, cosf(h) };
}

static Vector3 tank_local_to_world(Vector3 origin, Vector3 forward_dir, Vector3 right_dir, Vector3 local) {
    return (Vector3){
        origin.x + forward_dir.x * local.x + right_dir.x * local.z,
        origin.y + local.y,
        origin.z + forward_dir.z * local.x + right_dir.z * local.z
    };
}

static Vector3 tank_local_dir_to_world(Vector3 forward_dir, Vector3 right_dir, Vector3 local_dir) {
    return (Vector3){
        forward_dir.x * local_dir.x + right_dir.x * local_dir.z,
        local_dir.y,
        forward_dir.z * local_dir.x + right_dir.z * local_dir.z
    };
}

static float track_profile_from_points(float x, const float *points_x, const float *points_y, int count, float sag) {
    if (count <= 0) return 0.0f;
    if (x <= points_x[0]) return points_y[0];
    if (x >= points_x[count - 1]) return points_y[count - 1];
    for (int i = 0; i < count - 1; i++) {
        float x0 = points_x[i];
        float x1 = points_x[i + 1];
        if (x >= x0 && x <= x1) {
            float t = (x1 - x0) > 1e-6f ? (x - x0) / (x1 - x0) : 0.0f;
            float y = lerpf(points_y[i], points_y[i + 1], t);
            y -= sag * 4.0f * t * (1.0f - t);
            return y;
        }
    }
    return points_y[count - 1];
}

static void draw_wheel_disc(Vector3 center, Vector3 axle_dir, float radius, float width, int sides, Color tint) {
    Vector3 half = vec3_scale(axle_dir, width * 0.5f);
    DrawCylinderEx(vec3_add(center, (Vector3){ -half.x, -half.y, -half.z }),
                   vec3_add(center, half),
                   radius, radius, sides, tint);
}

static void draw_track_loop_side(Model track_model, Vector3 origin,
                                 Vector3 forward_dir, Vector3 right_dir, float side_sign,
                                 float phase_m, const double wheel_compression[5], Color tint) {
    const float local_z = TRACK_HALF_GAUGE * side_sign;
    const float rear_x = IDLER_X;
    const float front_x = SPROCKET_X;
    const float rear_top_y = 0.38f;
    const float front_top_y = 0.40f;
    const float rear_bottom_y = -1.04f;
    const float front_bottom_y = -1.02f;
    const float arc_len_rear = PI * IDLER_RADIUS;
    const float arc_len_front = PI * SPROCKET_RADIUS;
    const float run_len_top = fabsf(front_x - rear_x);
    const float run_len_bottom = fabsf(front_x - rear_x);
    const float total_len = arc_len_rear + run_len_top + arc_len_front + run_len_bottom;
    const float link_step = total_len / (float)TRACK_LINK_COUNT;

    float top_support_x[4] = { rear_x, RETURN_ROLLER_X[0], RETURN_ROLLER_X[1], front_x };
    float top_support_y[4] = {
        rear_top_y,
        RETURN_ROLLER_BASE_Y + 0.12f + 0.08f * (float)wheel_compression[1],
        RETURN_ROLLER_BASE_Y + 0.12f + 0.08f * (float)wheel_compression[3],
        front_top_y
    };
    float bottom_support_x[5] = { front_x, ROAD_WHEEL_X[4], ROAD_WHEEL_X[2], ROAD_WHEEL_X[0], rear_x };
    float bottom_support_y[5] = {
        front_bottom_y,
        ROAD_WHEEL_BASE_Y + (float)wheel_compression[4] + 0.48f,
        ROAD_WHEEL_BASE_Y + (float)wheel_compression[2] + 0.44f,
        ROAD_WHEEL_BASE_Y + (float)wheel_compression[0] + 0.48f,
        rear_bottom_y
    };

    for (int i = 0; i < TRACK_LINK_COUNT; i++) {
        float s = wrap_positive(phase_m + (i + 0.5f) * link_step, total_len);
        Vector3 local_pos = { 0.0f, 0.0f, local_z };
        Vector3 local_tangent = { 1.0f, 0.0f, 0.0f };

        if (s < arc_len_rear) {
            float t = s / arc_len_rear;
            float angle = deg2rad(270.0f - 180.0f * t);
            float rear_center_y = rear_bottom_y + IDLER_RADIUS;
            local_pos.x = rear_x + IDLER_RADIUS * cosf(angle);
            local_pos.y = rear_center_y + IDLER_RADIUS * sinf(angle);
            local_tangent = (Vector3){ sinf(angle), -cosf(angle), 0.0f };
        } else if (s < arc_len_rear + run_len_top) {
            float t = (s - arc_len_rear) / run_len_top;
            float x = lerpf(rear_x, front_x, t);
            float sag = 0.16f + 0.08f * (float)(wheel_compression[1] + wheel_compression[2] + wheel_compression[3]) / 3.0f;
            local_pos.x = x;
            local_pos.y = track_profile_from_points(x, top_support_x, top_support_y, 4, sag);
            local_tangent = (Vector3){ 1.0f, (top_support_y[3] - top_support_y[0]) * 0.012f, 0.0f };
        } else if (s < arc_len_rear + run_len_top + arc_len_front) {
            float t = (s - arc_len_rear - run_len_top) / arc_len_front;
            float angle = deg2rad(90.0f - 180.0f * t);
            float front_center_y = front_bottom_y + SPROCKET_RADIUS;
            local_pos.x = front_x + SPROCKET_RADIUS * cosf(angle);
            local_pos.y = front_center_y + SPROCKET_RADIUS * sinf(angle);
            local_tangent = (Vector3){ sinf(angle), -cosf(angle), 0.0f };
        } else {
            float t = (s - arc_len_rear - run_len_top - arc_len_front) / run_len_bottom;
            float x = lerpf(front_x, rear_x, t);
            float sag = 0.12f + 0.08f * (float)wheel_compression[2];
            local_pos.x = x;
            local_pos.y = track_profile_from_points(x, bottom_support_x, bottom_support_y, 5, sag);
            local_tangent = (Vector3){ -1.0f, (bottom_support_y[4] - bottom_support_y[0]) * 0.012f, 0.0f };
        }

        Vector3 world_pos = tank_local_to_world(origin, forward_dir, right_dir, local_pos);
        Vector3 world_tangent = tank_local_dir_to_world(forward_dir, right_dir, local_tangent);
        Vector3 axis; float angle_deg;
        axis_angle_from_x_to(world_tangent, &axis, &angle_deg);
        DrawModelEx(track_model, world_pos, axis, angle_deg, TRACK_LINK_SCALE, tint);
    }
}

void draw_tank_3d(const Tank *t, Model hull_model, Model turret_model,
                  Model barrel_model, Model muzzle_brake_model,
                  Model mantlet_model, Model track_model) {
    Vector3 pos = tank_render_pos(t, PLAYER_HEIGHT + (float)t->hull_bounce_m);
    Vector3 barrel_dir = tank_boresight_dir(t);
    Vector3 up_axis = { 0.0f, 1.0f, 0.0f };
    Vector3 unit_scale = { 1.0f, 1.0f, 1.0f };

    float hull_yaw = -(float)t->heading_deg;
    Vector3 hull_visual_pos = { pos.x, pos.y - 0.2f, pos.z }; // Ajuste de altura del hull
    DrawModelEx(hull_model, hull_visual_pos, up_axis, hull_yaw, unit_scale, WHITE);

    Vector3 forward_dir, right_dir;
    hull_basis_vectors(t, &forward_dir, &right_dir);
    Color track_tint = (Color){ 40, 40, 42, 255 };
    draw_track_loop_side(track_model, pos, forward_dir, right_dir, -1.0f, (float)t->left_track_phase_m, t->road_wheel_compression_m[0], track_tint);
    draw_track_loop_side(track_model, pos, forward_dir, right_dir,  1.0f, (float)t->right_track_phase_m, t->road_wheel_compression_m[1], track_tint);

    const int wheel_count = 5;
    Color wheel_color = (Color){ 26, 26, 28, 255 };
    for (int i = 0; i < wheel_count; i++) {
        Vector3 wheel_center_l = vec3_add(pos, vec3_add(vec3_scale(forward_dir, ROAD_WHEEL_X[i]), vec3_scale(right_dir, -1.9f)));
        Vector3 wheel_center_r = vec3_add(pos, vec3_add(vec3_scale(forward_dir, ROAD_WHEEL_X[i]), vec3_scale(right_dir, 1.9f)));
        wheel_center_l.y += ROAD_WHEEL_BASE_Y + (float)t->road_wheel_compression_m[0][i];
        wheel_center_r.y += ROAD_WHEEL_BASE_Y + (float)t->road_wheel_compression_m[1][i];
        draw_wheel_disc(wheel_center_l, right_dir, ROAD_WHEEL_RADIUS, 0.18f, 16, wheel_color);
        draw_wheel_disc(wheel_center_r, right_dir, ROAD_WHEEL_RADIUS, 0.18f, 16, wheel_color);
    }

    Vector3 roller_l = vec3_add(pos, vec3_add(vec3_scale(forward_dir, RETURN_ROLLER_X[0]), vec3_scale(right_dir, -TRACK_HALF_GAUGE)));
    Vector3 roller_r = vec3_add(pos, vec3_add(vec3_scale(forward_dir, RETURN_ROLLER_X[1]), vec3_scale(right_dir, TRACK_HALF_GAUGE)));
    roller_l.y += RETURN_ROLLER_BASE_Y;
    roller_r.y += RETURN_ROLLER_BASE_Y;

    Vector3 sprocket_l = vec3_add(pos, vec3_add(vec3_scale(forward_dir, SPROCKET_X), vec3_scale(right_dir, -TRACK_HALF_GAUGE)));
    Vector3 sprocket_r = vec3_add(pos, vec3_add(vec3_scale(forward_dir, SPROCKET_X), vec3_scale(right_dir, TRACK_HALF_GAUGE)));
    sprocket_l.y += -0.24f;
    sprocket_r.y += -0.24f;

    Vector3 idler_l = vec3_add(pos, vec3_add(vec3_scale(forward_dir, IDLER_X), vec3_scale(right_dir, -TRACK_HALF_GAUGE)));
    Vector3 idler_r = vec3_add(pos, vec3_add(vec3_scale(forward_dir, IDLER_X), vec3_scale(right_dir, TRACK_HALF_GAUGE)));
    idler_l.y += -0.38f;
    idler_r.y += -0.38f;

    draw_wheel_disc(roller_l, right_dir, RETURN_ROLLER_RADIUS, 0.12f, 12, (Color){ 48, 48, 50, 255 });
    draw_wheel_disc(roller_r, right_dir, RETURN_ROLLER_RADIUS, 0.12f, 12, (Color){ 48, 48, 50, 255 });
    draw_wheel_disc(sprocket_l, right_dir, SPROCKET_RADIUS, 0.18f, 16, (Color){ 58, 58, 62, 255 });
    draw_wheel_disc(sprocket_r, right_dir, SPROCKET_RADIUS, 0.18f, 16, (Color){ 58, 58, 62, 255 });
    draw_wheel_disc(idler_l, right_dir, IDLER_RADIUS, 0.18f, 16, (Color){ 54, 54, 58, 255 });
    draw_wheel_disc(idler_r, right_dir, IDLER_RADIUS, 0.18f, 16, (Color){ 54, 54, 58, 255 });

    Vector3 turret_pos = { pos.x, (pos.y - 0.2f) + 1.0f, pos.z };
    float turret_yaw = -(float)tank_absolute_turret_azimuth_deg(t);
    DrawModelEx(turret_model, turret_pos, up_axis, turret_yaw, unit_scale, WHITE);

    Vector3 mantlet_pos = vec3_add(turret_pos, vec3_scale(barrel_dir, 1.0f));
    DrawModelEx(mantlet_model, mantlet_pos, up_axis, turret_yaw, unit_scale, (Color){ 100, 102, 98, 255 });

    Vector3 barrel_axis; float barrel_angle_deg;
    axis_angle_from_y_to(barrel_dir, &barrel_axis, &barrel_angle_deg);
    Color metal_tint = (Color){ 130, 132, 128, 255 };
    DrawModelEx(barrel_model, turret_pos, barrel_axis, barrel_angle_deg, unit_scale, metal_tint);

    Vector3 muzzle_pos = vec3_add(turret_pos, vec3_scale(barrel_dir, 3.0f));
    DrawModelEx(muzzle_brake_model, muzzle_pos, barrel_axis, barrel_angle_deg, unit_scale, (Color){ 90, 92, 88, 255 });
}