#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ammo.h"
#include "combat.h"
#include "tank.h"
#include "terrain.h"

#define SIM_DT_MAX 0.05
#define PLAYER_HEIGHT 1.45f
#define CAMERA_BASE_DISTANCE 18.0f
#define CAMERA_MIN_DISTANCE 8.0f
#define CAMERA_MAX_DISTANCE 32.0f
#define CAMERA_BASE_PITCH -18.0f

typedef struct {
    Model terrain_model;
    Model bunker_model;
    Model rock_model;
    Texture2D terrain_texture;
    Texture2D rock_texture;
} SceneAssets;

static float lerpf(float from, float to, float t)
{
    return from + (to - from) * t;
}

static Vector3 vec3_add(Vector3 a, Vector3 b)
{
    return (Vector3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static Vector3 vec3_scale(Vector3 v, float scale)
{
    return (Vector3){ v.x * scale, v.y * scale, v.z * scale };
}

static Vector3 vec3_lerp(Vector3 from, Vector3 to, float t)
{
    return (Vector3){
        lerpf(from.x, to.x, t),
        lerpf(from.y, to.y, t),
        lerpf(from.z, to.z, t)
    };
}

static Vector3 tank_world_pos(const Tank *t, float extra_height)
{
    float ground = (float)terrain_height_at(t->position_m);
    return (Vector3){ (float)t->position_m.x, ground + extra_height, (float)t->position_m.y };
}

static void print_status(const Tank *p, const Tank *t, int target_ai, double sim_time_s)
{
    printf("\n--- STATUS (t = %.1fs) ---\n", sim_time_s);
    printf("%-8s pos(%.0f,%.0f) hdg %.0f spd %.1f km/h turret %.0f abs %.0f gun %.1f\n",
           p->name, p->position_m.x, p->position_m.y, p->heading_deg,
           tank_speed_kmh(p), p->turret_azimuth_deg,
           tank_absolute_turret_azimuth_deg(p), p->gun_elevation_deg);
    printf("         ammo: %s x%d  %s x%d  %s x%d   selected: %s   reload: %s\n",
           ammo_get(AMMO_APFSDS)->short_name, p->ammo_count[AMMO_APFSDS],
           ammo_get(AMMO_HEAT)->short_name,   p->ammo_count[AMMO_HEAT],
           ammo_get(AMMO_HE)->short_name,     p->ammo_count[AMMO_HE],
           ammo_get(p->current_ammo)->short_name,
           p->reload_timer_s > 0.0 ? "reloading" : "READY");
    printf("         HP: %.0f/100  %s\n", p->hp, p->alive ? "" : "-- DESTROYED");
    printf("         modules: engine %.0f tracks %.0f turret %.0f gun %.0f optics %.0f\n",
           p->module_engine_hp, p->module_tracks_hp, p->module_turret_hp,
           p->module_gun_hp, p->module_optics_hp);

    double dx = t->position_m.x - p->position_m.x;
    double dy = t->position_m.y - p->position_m.y;
    double range = sqrt(dx * dx + dy * dy);
    double bearing = norm_deg(rad2deg(atan2(dy, dx)));

    printf("%-8s pos(%.0f,%.0f) hdg %.0f turret(abs) %.0f range %.0fm bearing %.0fdeg AI:%s\n",
           t->name, t->position_m.x, t->position_m.y, t->heading_deg,
           tank_absolute_turret_azimuth_deg(t), range, bearing,
           target_ai ? "on" : "off");
    printf("         HP: %.0f/100  %s\n", t->hp, t->alive ? "" : "-- DESTROYED");
    printf("         modules: engine %.0f tracks %.0f turret %.0f gun %.0f optics %.0f\n\n",
           t->module_engine_hp, t->module_tracks_hp, t->module_turret_hp,
           t->module_gun_hp, t->module_optics_hp);
}

static void update_chassis_pose(Tank *t)
{
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
}

static void draw_tank_3d(const Tank *t, Color body, Color turret, Color barrel)
{
    Vector3 pos = tank_world_pos(t, PLAYER_HEIGHT + (float)t->hull_bounce_m);
    Vector3 body_size = { 6.0f, 1.4f, 3.4f };
    Vector3 turret_size = { 2.4f, 0.9f, 2.0f };
    float az = (float)deg2rad(tank_absolute_turret_azimuth_deg(t));
    float el = (float)deg2rad(t->gun_elevation_deg);
    Vector3 barrel_dir = { cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az) };

    DrawCubeV(pos, body_size, body);
    DrawCubeWiresV(pos, body_size, BLACK);

    Vector3 turret_pos = { pos.x, pos.y + 1.0f, pos.z };
    DrawCubeV(turret_pos, turret_size, turret);
    DrawCubeWiresV(turret_pos, turret_size, BLACK);

    Vector3 muzzle = vec3_add(turret_pos, vec3_scale(barrel_dir, 3.0f));
    DrawLine3D(turret_pos, muzzle, barrel);
}

static void update_camera(Camera3D *camera, const Tank *player, float *orbit_yaw_deg,
                         float *orbit_pitch_deg, float *orbit_distance)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        *orbit_yaw_deg += delta.x * 0.18f;
        *orbit_pitch_deg -= delta.y * 0.12f;
    } else {
        float desired_yaw = (float)norm_deg(player->heading_deg + 180.0);
        *orbit_yaw_deg = lerpf(*orbit_yaw_deg, desired_yaw, 0.06f);
        *orbit_pitch_deg = lerpf(*orbit_pitch_deg, CAMERA_BASE_PITCH, 0.04f);
    }

    *orbit_pitch_deg = clampd(*orbit_pitch_deg, -62.0f, -8.0f);
    *orbit_distance -= GetMouseWheelMove() * 1.2f;
    *orbit_distance = clampd(*orbit_distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);

    Vector3 player_pos = tank_world_pos(player, PLAYER_HEIGHT);
    float yaw_rad = (float)deg2rad(*orbit_yaw_deg);
    float pitch_rad = (float)deg2rad(*orbit_pitch_deg);

    Vector3 forward = {
        cosf(pitch_rad) * cosf(yaw_rad),
        sinf(pitch_rad),
        cosf(pitch_rad) * sinf(yaw_rad)
    };

    Vector3 desired_pos = vec3_add(player_pos, vec3_scale(forward, -*orbit_distance));
    desired_pos.y += 3.0f;

    camera->position = vec3_lerp(camera->position, desired_pos, 0.14f);
    camera->target = vec3_lerp(camera->target, vec3_add(player_pos, (Vector3){ 0.0f, 1.1f, 0.0f }), 0.20f);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
}

static void update_sight(Vector2 *sight_offset)
{
    Vector2 delta = GetMouseDelta();
    if (fabsf(delta.x) > 0.01f || fabsf(delta.y) > 0.01f) {
        sight_offset->x += delta.x * 5.0f;
        sight_offset->y += delta.y * 5.0f;
    } else {
        sight_offset->x = lerpf(sight_offset->x, 0.0f, 0.015f);
        sight_offset->y = lerpf(sight_offset->y, 0.0f, 0.015f);
    }

    float max_x = GetScreenWidth() * 0.33f;
    float max_y = GetScreenHeight() * 0.27f;
    sight_offset->x = clampd(sight_offset->x, -max_x, max_x);
    sight_offset->y = clampd(sight_offset->y, -max_y, max_y);
}

static Vector2 sight_screen_point(Vector2 sight_offset)
{
    return (Vector2){ GetScreenWidth() * 0.5f + sight_offset.x, GetScreenHeight() * 0.5f + sight_offset.y };
}

static Vector3 aim_point_from_screen(const Camera3D *camera, Vector2 screen_point)
{
    Ray ray = GetScreenToWorldRay(screen_point, *camera);
    Vector3 aim_point = vec3_add(ray.position, vec3_scale(ray.direction, 300.0f));
    if (fabsf(ray.direction.y) > 1e-5f) {
        float t = -ray.position.y / ray.direction.y;
        if (t > 0.0f) {
            aim_point = vec3_add(ray.position, vec3_scale(ray.direction, t));
        }
    }
    return aim_point;
}

static void apply_screen_turret_aim(const Tank *player, const Camera3D *camera,
                                    Vector2 screen_point, double dt,
                                    double *turret_input, double *elev_input,
                                    Vector3 *aim_point_out)
{
    Vector3 aim_point = aim_point_from_screen(camera, screen_point);
    if (aim_point_out) *aim_point_out = aim_point;

    Vec2 aim_flat = vec2_make(aim_point.x - player->position_m.x, aim_point.z - player->position_m.y);
    double desired_azimuth = norm_deg(rad2deg(atan2(aim_flat.y, aim_flat.x)) - player->heading_deg);
    double azimuth_error = norm_deg(desired_azimuth - player->turret_azimuth_deg);
    *turret_input += clampd(azimuth_error / (player->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    double dx = aim_point.x - player->position_m.x;
    double dz = aim_point.z - player->position_m.y;
    double flat_dist = sqrt(dx * dx + dz * dz);
    double desired_elev = rad2deg(atan2(aim_point.y - 2.2, flat_dist > 1e-6 ? flat_dist : 1e-6));
    double elev_error = desired_elev - player->gun_elevation_deg;
    *elev_input += clampd(elev_error / (player->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);
}

static void apply_world_turret_aim(const Tank *player, Vector3 aim_point, double dt,
                                   double *turret_input, double *elev_input)
{
    Vec2 aim_flat = vec2_make(aim_point.x - player->position_m.x, aim_point.z - player->position_m.y);
    double desired_azimuth = norm_deg(rad2deg(atan2(aim_flat.y, aim_flat.x)) - player->heading_deg);
    double azimuth_error = norm_deg(desired_azimuth - player->turret_azimuth_deg);
    *turret_input = clampd(azimuth_error / (player->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    double dx = aim_point.x - player->position_m.x;
    double dz = aim_point.z - player->position_m.y;
    double flat_dist = sqrt(dx * dx + dz * dz);
    double desired_elev = rad2deg(atan2(aim_point.y - 2.2, flat_dist > 1e-6 ? flat_dist : 1e-6));
    double elev_error = desired_elev - player->gun_elevation_deg;
    *elev_input = clampd(elev_error / (player->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);
}

static void draw_crosshair(Vector2 screen_point, Color color)
{
    int cx = (int)screen_point.x;
    int cy = (int)screen_point.y;
    int len = 10;
    DrawLine(cx - len, cy, cx - 3, cy, color);
    DrawLine(cx + 3, cy, cx + len, cy, color);
    DrawLine(cx, cy - len, cx, cy - 3, color);
    DrawLine(cx, cy + 3, cx, cy + len, color);
    DrawCircleLines(cx, cy, 18.0f, Fade(color, 0.4f));
}

static Image build_terrain_image(void)
{
    const int width = 256;
    const int height = 256;
    Image image = GenImageColor(width, height, BLACK);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double world_x = ((double)x / (double)(width - 1) - 0.5) * 2400.0;
            double world_y = ((double)y / (double)(height - 1) - 0.5) * 2400.0;
            double h = terrain_height_at(vec2_make(world_x, world_y));
            double normalized = clampd((h + 5.0) / 12.0, 0.0, 1.0);
            int value = (int)(normalized * 255.0);
            ImageDrawPixel(&image, x, y, (Color){ (unsigned char)value, (unsigned char)value, (unsigned char)value, 255 });
        }
    }

    return image;
}

static void init_scene_assets(SceneAssets *assets)
{
    memset(assets, 0, sizeof(*assets));

    Image terrain_image = build_terrain_image();
    Mesh terrain_mesh = GenMeshHeightmap(terrain_image, (Vector3){ 2400.0f, 20.0f, 2400.0f });
    assets->terrain_model = LoadModelFromMesh(terrain_mesh);
    assets->terrain_texture = LoadTextureFromImage(GenImageChecked(512, 512, 16, 16,
                                                                     (Color){ 74, 88, 68, 255 },
                                                                     (Color){ 52, 64, 48, 255 }));
    SetMaterialTexture(&assets->terrain_model.materials[0], MATERIAL_MAP_ALBEDO, assets->terrain_texture);

    Mesh bunker_mesh = GenMeshCube(10.0f, 4.0f, 10.0f);
    assets->bunker_model = LoadModelFromMesh(bunker_mesh);

    Mesh rock_mesh = GenMeshSphere(4.5f, 16, 16);
    assets->rock_model = LoadModelFromMesh(rock_mesh);
    assets->rock_texture = LoadTextureFromImage(GenImageChecked(128, 128, 8, 8,
                                                                (Color){ 120, 120, 110, 255 },
                                                                (Color){ 86, 80, 78, 255 }));
    SetMaterialTexture(&assets->rock_model.materials[0], MATERIAL_MAP_ALBEDO, assets->rock_texture);

    UnloadImage(terrain_image);
}

static void unload_scene_assets(SceneAssets *assets)
{
    UnloadTexture(assets->terrain_texture);
    UnloadTexture(assets->rock_texture);
    UnloadModel(assets->terrain_model);
    UnloadModel(assets->bunker_model);
    UnloadModel(assets->rock_model);
}

static int turret_has_line_of_sight(const Tank *shooter, const Tank *target)
{
    Vector3 shooter_gun = vec3_add(tank_world_pos(shooter, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.4f, 0.0f });
    Vector3 target_center = vec3_add(tank_world_pos(target, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.5f, 0.0f });
    return !terrain_line_blocked((Vec3){ shooter_gun.x, shooter_gun.y, shooter_gun.z },
                                 (Vec3){ target_center.x, target_center.y, target_center.z });
}

static void update_ai(double *target_throttle, double *target_steer)
{
    if ((rand() % 100) < 15) {
        *target_throttle = clampd(((double)rand() / RAND_MAX) * 1.4 - 0.4, -1.0, 1.0);
        *target_steer = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
    }
}

int main(void)
{
    srand((unsigned)time(NULL));
    ammo_init_table();

    const char *display = getenv("DISPLAY");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if ((display == NULL || display[0] == '\0') &&
        (wayland_display == NULL || wayland_display[0] == '\0')) {
        fprintf(stderr, "Raylib 3D needs a graphical desktop session (Wayland or X11). Use ./leopard_sim for the console version if you are in a headless shell.\n");
        return 1;
    }

    Tank player, target;
    tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
    tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);

    SceneAssets assets;
    InitWindow(1280, 720, "Leopard 2A7 - 3D Frontend");
    SetTargetFPS(60);
    DisableCursor();
    init_scene_assets(&assets);

    int target_ai = 1;
    double target_throttle = 0.4;
    double target_steer = 0.0;
    double sim_time_s = 0.0;

    Camera3D camera = { 0 };
    camera.position = (Vector3){ -14.0f, 8.0f, -14.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float orbit_yaw_deg = 225.0f;
    float orbit_pitch_deg = CAMERA_BASE_PITCH;
    float orbit_distance = CAMERA_BASE_DISTANCE;
    Vector2 sight_offset = { 0.0f, 0.0f };

    while (!WindowShouldClose()) {
        double dt = GetFrameTime();
        if (dt > SIM_DT_MAX) dt = SIM_DT_MAX;

        update_sight(&sight_offset);

        if (IsKeyPressed(KEY_R)) {
            tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
            tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);
            target_ai = 1;
            target_throttle = 0.4;
            target_steer = 0.0;
            sim_time_s = 0.0;
            sight_offset = (Vector2){ 0.0f, 0.0f };
        }
        if (IsKeyPressed(KEY_P)) print_status(&player, &target, target_ai, sim_time_s);
        if (IsKeyPressed(KEY_ONE)) player.current_ammo = AMMO_APFSDS;
        if (IsKeyPressed(KEY_TWO)) player.current_ammo = AMMO_HEAT;
        if (IsKeyPressed(KEY_THREE)) player.current_ammo = AMMO_HE;

        double throttle = 0.0;
        if (IsKeyDown(KEY_W)) throttle += 1.0;
        if (IsKeyDown(KEY_S)) throttle -= 1.0;

        double steer = 0.0;
        if (IsKeyDown(KEY_D)) steer += 1.0;
        if (IsKeyDown(KEY_A)) steer -= 1.0;

        double player_turret = 0.0;
        double player_elev = 0.0;
        if (IsKeyDown(KEY_L)) player_turret += 1.0;
        if (IsKeyDown(KEY_J)) player_turret -= 1.0;
        if (IsKeyDown(KEY_I)) player_elev += 1.0;
        if (IsKeyDown(KEY_K)) player_elev -= 1.0;

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            update_camera(&camera, &player, &orbit_yaw_deg, &orbit_pitch_deg, &orbit_distance);
        } else {
            float desired_yaw = (float)norm_deg(player.heading_deg + 180.0);
            orbit_yaw_deg = lerpf(orbit_yaw_deg, desired_yaw, 0.08f);
            orbit_pitch_deg = lerpf(orbit_pitch_deg, CAMERA_BASE_PITCH, 0.05f);
            update_camera(&camera, &player, &orbit_yaw_deg, &orbit_pitch_deg, &orbit_distance);
        }

        Vector2 sight_point = sight_screen_point(sight_offset);
        Vector3 aim_point = { 0.0f, 0.0f, 0.0f };
        apply_screen_turret_aim(&player, &camera, sight_point, dt, &player_turret, &player_elev, &aim_point);

        tank_update_movement(&player, throttle, steer, terrain_grip_at(player.position_m), dt);
        tank_update_turret(&player, player_turret, player_elev, dt);
        tank_tick_reload(&player, dt);
        update_chassis_pose(&player);

        if (target_ai && target.alive) {
            update_ai(&target_throttle, &target_steer);
            tank_update_movement(&target, target_throttle, target_steer, terrain_grip_at(target.position_m), dt);
            update_chassis_pose(&target);

            Vector3 player_center = vec3_add(tank_world_pos(&player, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.5f, 0.0f });
            double ai_turret = 0.0;
            double ai_elev = 0.0;
            apply_world_turret_aim(&target, player_center, dt, &ai_turret, &ai_elev);
            tank_update_turret(&target, ai_turret, ai_elev, dt);
            tank_tick_reload(&target, dt);

            if (tank_can_fire(&target) && target.reload_timer_s <= 0.0 && turret_has_line_of_sight(&target, &player)) {
                combat_fire_shot(&target, &player);
            }
        }

        if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE)) && tank_can_fire(&player)) {
            combat_fire_shot(&player, &target);
        }

        BeginDrawing();
        ClearBackground((Color){ 12, 16, 20, 255 });

        BeginMode3D(camera);
        DrawModel(assets.terrain_model, (Vector3){ -1200.0f, 0.0f, -1200.0f }, 1.0f, WHITE);

        DrawModelEx(assets.bunker_model, (Vector3){ 520.0f, (float)terrain_height_at(vec2_make(520.0, -120.0)) + 2.0f, -120.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, 5.0f, (Vector3){ 1.0f, 1.0f, 1.0f }, (Color){ 92, 96, 86, 255 });
        DrawModelEx(assets.bunker_model, (Vector3){ 360.0f, (float)terrain_height_at(vec2_make(360.0, 180.0)) + 2.0f, 180.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, -12.0f, (Vector3){ 0.8f, 0.8f, 0.8f }, (Color){ 85, 80, 74, 255 });
        DrawModelEx(assets.rock_model, (Vector3){ 260.0f, (float)terrain_height_at(vec2_make(260.0, -40.0)) + 2.0f, -40.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, 0.0f, (Vector3){ 1.0f, 1.0f, 1.0f }, (Color){ 116, 112, 104, 255 });

        DrawSphere(aim_point, 0.5f, GOLD);
        DrawSphereWires(aim_point, 0.5f, 8, 8, Fade(WHITE, 0.5f));
        draw_tank_3d(&player, (Color){ 78, 96, 63, 255 }, (Color){ 96, 116, 76, 255 }, DARKGREEN);
        draw_tank_3d(&target, (Color){ 120, 74, 64, 255 }, (Color){ 150, 92, 78, 255 }, MAROON);
        EndMode3D();

        Color crosshair_color = turret_has_line_of_sight(&player, &target) ? RAYWHITE : RED;
        draw_crosshair(sight_point, crosshair_color);

        DrawRectangle(16, 16, 580, 196, Fade(BLACK, 0.65f));
        DrawText("Leopard 2A7 - 3D Tank Frontend", 28, 28, 24, RAYWHITE);
        DrawText("Mouse moves the gunner sight   RMB orbit camera   Wheel zoom   Left click/Space fire", 28, 60, 18, RAYWHITE);
        DrawText("W/S drive   A/D steer   J/L turret nudge   I/K elevation nudge   P status   R reset", 28, 84, 18, RAYWHITE);
        DrawText(TextFormat("Range %.0fm  LOS:%s  Sight offset: %.0f, %.0f",
                            sqrt((aim_point.x - player.position_m.x) * (aim_point.x - player.position_m.x) +
                                 (aim_point.z - player.position_m.y) * (aim_point.z - player.position_m.y)),
                            turret_has_line_of_sight(&player, &target) ? "clear" : "blocked",
                            sight_offset.x, sight_offset.y), 28, 108, 18, SKYBLUE);
        DrawText(TextFormat("Player: ammo %s | reload %.1fs | engine %.0f | tracks %.0f | gun %.0f",
                            ammo_get(player.current_ammo)->short_name, player.reload_timer_s,
                            player.module_engine_hp, player.module_tracks_hp, player.module_gun_hp), 28, 132, 18, YELLOW);
        DrawText(TextFormat("Target: ammo %s | reload %.1fs | engine %.0f | tracks %.0f | gun %.0f",
                            ammo_get(target.current_ammo)->short_name, target.reload_timer_s,
                            target.module_engine_hp, target.module_tracks_hp, target.module_gun_hp), 28, 156, 18, ORANGE);
        DrawText(TextFormat("Terrain: height %.1f m | grip %.2f",
                            terrain_height_at(player.position_m), terrain_grip_at(player.position_m)), 28, 180, 18, LIGHTGRAY);

        if (!player.alive || !target.alive) {
            DrawRectangle(650, 16, 340, 60, Fade(BLACK, 0.55f));
            DrawText(player.alive ? "Target destroyed" : "Player destroyed", 664, 34, 28, ORANGE);
        }

        EndDrawing();
        sim_time_s += dt;
    }

    unload_scene_assets(&assets);
    CloseWindow();
    return 0;
}#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ammo.h"
#include "combat.h"
#include "tank.h"
#include "terrain.h"

#define SIM_DT_MAX 0.05
#define PLAYER_HEIGHT 1.45f
#define CAMERA_BASE_DISTANCE 18.0f
#define CAMERA_MIN_DISTANCE 8.0f
#define CAMERA_MAX_DISTANCE 32.0f
#define CAMERA_BASE_PITCH -18.0f

typedef struct {
    Model terrain_model;
    Model bunker_model;
    Model rock_model;
    Texture2D terrain_texture;
    Texture2D rock_texture;
} SceneAssets;

static float lerpf(float from, float to, float t)
{
    return from + (to - from) * t;
}

static Vector3 vec3_add(Vector3 a, Vector3 b)
{
    return (Vector3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static Vector3 vec3_scale(Vector3 v, float scale)
{
    return (Vector3){ v.x * scale, v.y * scale, v.z * scale };
}

static Vector3 vec3_lerp(Vector3 from, Vector3 to, float t)
{
    return (Vector3){
        lerpf(from.x, to.x, t),
        lerpf(from.y, to.y, t),
        lerpf(from.z, to.z, t)
    };
}

static Vector3 tank_world_pos(const Tank *t, float extra_height)
{
    float ground = (float)terrain_height_at(t->position_m);
    return (Vector3){ (float)t->position_m.x, ground + extra_height, (float)t->position_m.y };
}

static void print_status(const Tank *p, const Tank *t, int target_ai, double sim_time_s)
{
    printf("\n--- STATUS (t = %.1fs) ---\n", sim_time_s);
    printf("%-8s pos(%.0f,%.0f) hdg %.0f spd %.1f km/h turret %.0f abs %.0f gun %.1f\n",
           p->name, p->position_m.x, p->position_m.y, p->heading_deg,
           tank_speed_kmh(p), p->turret_azimuth_deg,
           tank_absolute_turret_azimuth_deg(p), p->gun_elevation_deg);
    printf("         ammo: %s x%d  %s x%d  %s x%d   selected: %s   reload: %s\n",
           ammo_get(AMMO_APFSDS)->short_name, p->ammo_count[AMMO_APFSDS],
           ammo_get(AMMO_HEAT)->short_name,   p->ammo_count[AMMO_HEAT],
           ammo_get(AMMO_HE)->short_name,     p->ammo_count[AMMO_HE],
           ammo_get(p->current_ammo)->short_name,
           p->reload_timer_s > 0.0 ? "reloading" : "READY");
    printf("         HP: %.0f/100  %s\n", p->hp, p->alive ? "" : "-- DESTROYED");
    printf("         modules: engine %.0f tracks %.0f turret %.0f gun %.0f optics %.0f\n",
           p->module_engine_hp, p->module_tracks_hp, p->module_turret_hp,
           p->module_gun_hp, p->module_optics_hp);

    double dx = t->position_m.x - p->position_m.x;
    double dy = t->position_m.y - p->position_m.y;
    double range = sqrt(dx * dx + dy * dy);
    double bearing = norm_deg(rad2deg(atan2(dy, dx)));

    printf("%-8s pos(%.0f,%.0f) hdg %.0f turret(abs) %.0f range %.0fm bearing %.0fdeg AI:%s\n",
           t->name, t->position_m.x, t->position_m.y, t->heading_deg,
           tank_absolute_turret_azimuth_deg(t), range, bearing,
           target_ai ? "on" : "off");
    printf("         HP: %.0f/100  %s\n", t->hp, t->alive ? "" : "-- DESTROYED");
    printf("         modules: engine %.0f tracks %.0f turret %.0f gun %.0f optics %.0f\n\n",
           t->module_engine_hp, t->module_tracks_hp, t->module_turret_hp,
           t->module_gun_hp, t->module_optics_hp);
}

static void update_chassis_pose(Tank *t)
{
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
}

static void draw_tank_3d(const Tank *t, Color body, Color turret, Color barrel)
{
    Vector3 pos = tank_world_pos(t, PLAYER_HEIGHT + (float)t->hull_bounce_m);
    Vector3 body_size = { 6.0f, 1.4f, 3.4f };
    Vector3 turret_size = { 2.4f, 0.9f, 2.0f };
    float az = (float)deg2rad(tank_absolute_turret_azimuth_deg(t));
    float el = (float)deg2rad(t->gun_elevation_deg);
    Vector3 barrel_dir = { cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az) };

    DrawCubeV(pos, body_size, body);
    DrawCubeWiresV(pos, body_size, BLACK);

    Vector3 turret_pos = { pos.x, pos.y + 1.0f, pos.z };
    DrawCubeV(turret_pos, turret_size, turret);
    DrawCubeWiresV(turret_pos, turret_size, BLACK);

    Vector3 muzzle = vec3_add(turret_pos, vec3_scale(barrel_dir, 3.0f));
    DrawLine3D(turret_pos, muzzle, barrel);
}

static void update_camera(Camera3D *camera, const Tank *player, float *orbit_yaw_deg,
                         float *orbit_pitch_deg, float *orbit_distance)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        *orbit_yaw_deg += delta.x * 0.18f;
        *orbit_pitch_deg -= delta.y * 0.12f;
    } else {
        float desired_yaw = (float)norm_deg(player->heading_deg + 180.0);
        *orbit_yaw_deg = lerpf(*orbit_yaw_deg, desired_yaw, 0.06f);
        *orbit_pitch_deg = lerpf(*orbit_pitch_deg, CAMERA_BASE_PITCH, 0.04f);
    }

    *orbit_pitch_deg = clampd(*orbit_pitch_deg, -62.0f, -8.0f);
    *orbit_distance -= GetMouseWheelMove() * 1.2f;
    *orbit_distance = clampd(*orbit_distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);

    Vector3 player_pos = tank_world_pos(player, PLAYER_HEIGHT);
    float yaw_rad = (float)deg2rad(*orbit_yaw_deg);
    float pitch_rad = (float)deg2rad(*orbit_pitch_deg);

    Vector3 forward = {
        cosf(pitch_rad) * cosf(yaw_rad),
        sinf(pitch_rad),
        cosf(pitch_rad) * sinf(yaw_rad)
    };

    Vector3 desired_pos = vec3_add(player_pos, vec3_scale(forward, -*orbit_distance));
    desired_pos.y += 3.0f;

    camera->position = vec3_lerp(camera->position, desired_pos, 0.14f);
    camera->target = vec3_lerp(camera->target, vec3_add(player_pos, (Vector3){ 0.0f, 1.1f, 0.0f }), 0.20f);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
}

static void update_sight(Vector2 *sight_offset)
{
    Vector2 delta = GetMouseDelta();
    if (fabsf(delta.x) > 0.01f || fabsf(delta.y) > 0.01f) {
        sight_offset->x += delta.x * 5.0f;
        sight_offset->y += delta.y * 5.0f;
    } else {
        sight_offset->x = lerpf(sight_offset->x, 0.0f, 0.015f);
        sight_offset->y = lerpf(sight_offset->y, 0.0f, 0.015f);
    }

    float max_x = GetScreenWidth() * 0.33f;
    float max_y = GetScreenHeight() * 0.27f;
    sight_offset->x = clampd(sight_offset->x, -max_x, max_x);
    sight_offset->y = clampd(sight_offset->y, -max_y, max_y);
}

static Vector2 sight_screen_point(Vector2 sight_offset)
{
    return (Vector2){ GetScreenWidth() * 0.5f + sight_offset.x, GetScreenHeight() * 0.5f + sight_offset.y };
}

static Vector3 aim_point_from_screen(const Camera3D *camera, Vector2 screen_point)
{
    Ray ray = GetScreenToWorldRay(screen_point, *camera);
    Vector3 aim_point = vec3_add(ray.position, vec3_scale(ray.direction, 300.0f));
    if (fabsf(ray.direction.y) > 1e-5f) {
        float t = -ray.position.y / ray.direction.y;
        if (t > 0.0f) {
            aim_point = vec3_add(ray.position, vec3_scale(ray.direction, t));
        }
    }
    return aim_point;
}

static void apply_screen_turret_aim(const Tank *player, const Camera3D *camera,
                                    Vector2 screen_point, double dt,
                                    double *turret_input, double *elev_input,
                                    Vector3 *aim_point_out)
{
    Vector3 aim_point = aim_point_from_screen(camera, screen_point);
    if (aim_point_out) *aim_point_out = aim_point;

    Vec2 aim_flat = vec2_make(aim_point.x - player->position_m.x, aim_point.z - player->position_m.y);
    double desired_azimuth = norm_deg(rad2deg(atan2(aim_flat.y, aim_flat.x)) - player->heading_deg);
    double azimuth_error = norm_deg(desired_azimuth - player->turret_azimuth_deg);
    *turret_input += clampd(azimuth_error / (player->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    double dx = aim_point.x - player->position_m.x;
    double dz = aim_point.z - player->position_m.y;
    double flat_dist = sqrt(dx * dx + dz * dz);
    double desired_elev = rad2deg(atan2(aim_point.y - 2.2, flat_dist > 1e-6 ? flat_dist : 1e-6));
    double elev_error = desired_elev - player->gun_elevation_deg;
    *elev_input += clampd(elev_error / (player->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);
}

static void apply_world_turret_aim(const Tank *player, Vector3 aim_point, double dt,
                                   double *turret_input, double *elev_input)
{
    Vec2 aim_flat = vec2_make(aim_point.x - player->position_m.x, aim_point.z - player->position_m.y);
    double desired_azimuth = norm_deg(rad2deg(atan2(aim_flat.y, aim_flat.x)) - player->heading_deg);
    double azimuth_error = norm_deg(desired_azimuth - player->turret_azimuth_deg);
    *turret_input = clampd(azimuth_error / (player->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    double dx = aim_point.x - player->position_m.x;
    double dz = aim_point.z - player->position_m.y;
    double flat_dist = sqrt(dx * dx + dz * dz);
    double desired_elev = rad2deg(atan2(aim_point.y - 2.2, flat_dist > 1e-6 ? flat_dist : 1e-6));
    double elev_error = desired_elev - player->gun_elevation_deg;
    *elev_input = clampd(elev_error / (player->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);
}

static void draw_crosshair(Vector2 screen_point, Color color)
{
    int cx = (int)screen_point.x;
    int cy = (int)screen_point.y;
    int len = 10;
    DrawLine(cx - len, cy, cx - 3, cy, color);
    DrawLine(cx + 3, cy, cx + len, cy, color);
    DrawLine(cx, cy - len, cx, cy - 3, color);
    DrawLine(cx, cy + 3, cx, cy + len, color);
    DrawCircleLines(cx, cy, 18.0f, Fade(color, 0.4f));
}

static Image build_terrain_image(void)
{
    const int width = 256;
    const int height = 256;
    Image image = GenImageColor(width, height, BLACK);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double world_x = ((double)x / (double)(width - 1) - 0.5) * 2400.0;
            double world_y = ((double)y / (double)(height - 1) - 0.5) * 2400.0;
            double h = terrain_height_at(vec2_make(world_x, world_y));
            double normalized = clampd((h + 5.0) / 12.0, 0.0, 1.0);
            int value = (int)(normalized * 255.0);
            ImageDrawPixel(&image, x, y, (Color){ (unsigned char)value, (unsigned char)value, (unsigned char)value, 255 });
        }
    }

    return image;
}

static void init_scene_assets(SceneAssets *assets)
{
    memset(assets, 0, sizeof(*assets));

    Image terrain_image = build_terrain_image();
    Mesh terrain_mesh = GenMeshHeightmap(terrain_image, (Vector3){ 2400.0f, 20.0f, 2400.0f });
    assets->terrain_model = LoadModelFromMesh(terrain_mesh);
    assets->terrain_texture = LoadTextureFromImage(GenImageChecked(512, 512, 16, 16,
                                                                     (Color){ 74, 88, 68, 255 },
                                                                     (Color){ 52, 64, 48, 255 }));
    SetMaterialTexture(&assets->terrain_model.materials[0], MATERIAL_MAP_ALBEDO, assets->terrain_texture);

    Mesh bunker_mesh = GenMeshCube(10.0f, 4.0f, 10.0f);
    assets->bunker_model = LoadModelFromMesh(bunker_mesh);

    Mesh rock_mesh = GenMeshSphere(4.5f, 16, 16);
    assets->rock_model = LoadModelFromMesh(rock_mesh);
    assets->rock_texture = LoadTextureFromImage(GenImageChecked(128, 128, 8, 8,
                                                                (Color){ 120, 120, 110, 255 },
                                                                (Color){ 86, 80, 78, 255 }));
    SetMaterialTexture(&assets->rock_model.materials[0], MATERIAL_MAP_ALBEDO, assets->rock_texture);

    UnloadImage(terrain_image);
}

static void unload_scene_assets(SceneAssets *assets)
{
    UnloadTexture(assets->terrain_texture);
    UnloadTexture(assets->rock_texture);
    UnloadModel(assets->terrain_model);
    UnloadModel(assets->bunker_model);
    UnloadModel(assets->rock_model);
}

static int turret_has_line_of_sight(const Tank *shooter, const Tank *target)
{
    Vector3 shooter_gun = vec3_add(tank_world_pos(shooter, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.4f, 0.0f });
    Vector3 target_center = vec3_add(tank_world_pos(target, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.5f, 0.0f });
    return !terrain_line_blocked(shooter_gun, target_center);
}

int main(void)
{
    srand((unsigned)time(NULL));
    ammo_init_table();

    const char *display = getenv("DISPLAY");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if ((display == NULL || display[0] == '\0') &&
        (wayland_display == NULL || wayland_display[0] == '\0')) {
        fprintf(stderr, "Raylib 3D needs a graphical desktop session (Wayland or X11). Use ./leopard_sim for the console version if you are in a headless shell.\n");
        return 1;
    }

    Tank player, target;
    tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
    tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);

    SceneAssets assets;
    InitWindow(1280, 720, "Leopard 2A7 - 3D Frontend");
    SetTargetFPS(60);
    DisableCursor();
    init_scene_assets(&assets);

    int target_ai = 1;
    double target_throttle = 0.4;
    double target_steer = 0.0;
    double sim_time_s = 0.0;

    Camera3D camera = { 0 };
    camera.position = (Vector3){ -14.0f, 8.0f, -14.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float orbit_yaw_deg = 225.0f;
    float orbit_pitch_deg = CAMERA_BASE_PITCH;
    float orbit_distance = CAMERA_BASE_DISTANCE;
    Vector2 sight_offset = { 0.0f, 0.0f };

    while (!WindowShouldClose()) {
        double dt = GetFrameTime();
        if (dt > SIM_DT_MAX) dt = SIM_DT_MAX;

        update_sight(&sight_offset);

        if (IsKeyPressed(KEY_R)) {
            tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
            tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);
            target_ai = 1;
            target_throttle = 0.4;
            target_steer = 0.0;
            sim_time_s = 0.0;
            sight_offset = (Vector2){ 0.0f, 0.0f };
        }
        if (IsKeyPressed(KEY_P)) print_status(&player, &target, target_ai, sim_time_s);
        if (IsKeyPressed(KEY_ONE)) player.current_ammo = AMMO_APFSDS;
        if (IsKeyPressed(KEY_TWO)) player.current_ammo = AMMO_HEAT;
        if (IsKeyPressed(KEY_THREE)) player.current_ammo = AMMO_HE;

        double throttle = 0.0;
        if (IsKeyDown(KEY_W)) throttle += 1.0;
        if (IsKeyDown(KEY_S)) throttle -= 1.0;

        double steer = 0.0;
        if (IsKeyDown(KEY_D)) steer += 1.0;
        if (IsKeyDown(KEY_A)) steer -= 1.0;

        double player_turret = 0.0;
        double player_elev = 0.0;
        if (IsKeyDown(KEY_L)) player_turret += 1.0;
        if (IsKeyDown(KEY_J)) player_turret -= 1.0;
        if (IsKeyDown(KEY_I)) player_elev += 1.0;
        if (IsKeyDown(KEY_K)) player_elev -= 1.0;

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            update_camera(&camera, &player, &orbit_yaw_deg, &orbit_pitch_deg, &orbit_distance);
        } else {
            float desired_yaw = (float)norm_deg(player.heading_deg + 180.0);
            orbit_yaw_deg = lerpf(orbit_yaw_deg, desired_yaw, 0.08f);
            orbit_pitch_deg = lerpf(orbit_pitch_deg, CAMERA_BASE_PITCH, 0.05f);
            update_camera(&camera, &player, &orbit_yaw_deg, &orbit_pitch_deg, &orbit_distance);
        }

        Vector2 sight_point = sight_screen_point(sight_offset);
        Vector3 aim_point = { 0.0f, 0.0f, 0.0f };
        apply_screen_turret_aim(&player, &camera, sight_point, dt, &player_turret, &player_elev, &aim_point);

        tank_update_movement(&player, throttle, steer, terrain_grip_at(player.position_m), dt);
        tank_update_turret(&player, player_turret, player_elev, dt);
        tank_tick_reload(&player, dt);
        update_chassis_pose(&player);

        if (target_ai && target.alive) {
            update_ai(&target_throttle, &target_steer);
            tank_update_movement(&target, target_throttle, target_steer, terrain_grip_at(target.position_m), dt);
            update_chassis_pose(&target);

            Vector3 player_center = vec3_add(tank_world_pos(&player, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.5f, 0.0f });
            double ai_turret = 0.0;
            double ai_elev = 0.0;
            apply_world_turret_aim(&target, player_center, dt, &ai_turret, &ai_elev);
            tank_update_turret(&target, ai_turret, ai_elev, dt);
            tank_tick_reload(&target, dt);

            if (tank_can_fire(&target) && target.reload_timer_s <= 0.0 && turret_has_line_of_sight(&target, &player)) {
                combat_fire_shot(&target, &player);
            }
        }

        if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE)) && tank_can_fire(&player)) {
            combat_fire_shot(&player, &target);
        }

        BeginDrawing();
        ClearBackground((Color){ 12, 16, 20, 255 });

        BeginMode3D(camera);
        DrawModel(assets.terrain_model, (Vector3){ -1200.0f, 0.0f, -1200.0f }, 1.0f, WHITE);

        DrawModelEx(assets.bunker_model, (Vector3){ 520.0f, (float)terrain_height_at(vec2_make(520.0, -120.0)) + 2.0f, -120.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, 5.0f, (Vector3){ 1.0f, 1.0f, 1.0f }, (Color){ 92, 96, 86, 255 });
        DrawModelEx(assets.bunker_model, (Vector3){ 360.0f, (float)terrain_height_at(vec2_make(360.0, 180.0)) + 2.0f, 180.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, -12.0f, (Vector3){ 0.8f, 0.8f, 0.8f }, (Color){ 85, 80, 74, 255 });
        DrawModelEx(assets.rock_model, (Vector3){ 260.0f, (float)terrain_height_at(vec2_make(260.0, -40.0)) + 2.0f, -40.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, 0.0f, (Vector3){ 1.0f, 1.0f, 1.0f }, (Color){ 116, 112, 104, 255 });

        DrawSphere(aim_point, 0.5f, GOLD);
        DrawSphereWires(aim_point, 0.5f, 8, 8, Fade(WHITE, 0.5f));
        draw_tank_3d(&player, (Color){ 78, 96, 63, 255 }, (Color){ 96, 116, 76, 255 }, DARKGREEN);
        draw_tank_3d(&target, (Color){ 120, 74, 64, 255 }, (Color){ 150, 92, 78, 255 }, MAROON);
        EndMode3D();

        Color crosshair_color = turret_has_line_of_sight(&player, &target) ? RAYWHITE : RED;
        draw_crosshair(sight_point, crosshair_color);

        DrawRectangle(16, 16, 580, 196, Fade(BLACK, 0.65f));
        DrawText("Leopard 2A7 - 3D Tank Frontend", 28, 28, 24, RAYWHITE);
        DrawText("Mouse moves the gunner sight   RMB orbit camera   Wheel zoom   Left click/Space fire", 28, 60, 18, RAYWHITE);
        DrawText("W/S drive   A/D steer   J/L turret nudge   I/K elevation nudge   P status   R reset", 28, 84, 18, RAYWHITE);
        DrawText(TextFormat("Range %.0fm  LOS:%s  Sight offset: %.0f, %.0f",
                            Vec2Length(vec2_make(aim_point.x - player.position_m.x, aim_point.z - player.position_m.y)),
                            turret_has_line_of_sight(&player, &target) ? "clear" : "blocked",
                            sight_offset.x, sight_offset.y), 28, 108, 18, SKYBLUE);
        DrawText(TextFormat("Player: ammo %s | reload %.1fs | engine %.0f | tracks %.0f | gun %.0f",
                            ammo_get(player.current_ammo)->short_name, player.reload_timer_s,
                            player.module_engine_hp, player.module_tracks_hp, player.module_gun_hp), 28, 132, 18, YELLOW);
        DrawText(TextFormat("Target: ammo %s | reload %.1fs | engine %.0f | tracks %.0f | gun %.0f",
                            ammo_get(target.current_ammo)->short_name, target.reload_timer_s,
                            target.module_engine_hp, target.module_tracks_hp, target.module_gun_hp), 28, 156, 18, ORANGE);
        DrawText(TextFormat("Terrain: height %.1f m | grip %.2f",
                            terrain_height_at(player.position_m), terrain_grip_at(player.position_m)), 28, 180, 18, LIGHTGRAY);

        if (!player.alive || !target.alive) {
            DrawRectangle(650, 16, 340, 60, Fade(BLACK, 0.55f));
            DrawText(player.alive ? "Target destroyed" : "Player destroyed", 664, 34, 28, ORANGE);
        }

        EndDrawing();
        sim_time_s += dt;
    }

    unload_scene_assets(&assets);
    CloseWindow();
    return 0;
}#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ammo.h"
#include "combat.h"
#include "tank.h"

#define SIM_DT_MAX 0.05
#define PLAYER_HEIGHT 1.4f

#define CAMERA_BASE_DISTANCE 18.0f
#define CAMERA_MIN_DISTANCE   8.0f
#define CAMERA_MAX_DISTANCE   32.0f
#define CAMERA_BASE_PITCH    -18.0f

static float lerpf(float from, float to, float t)
{
    return from + (to - from) * t;
}

static Vector3 vec3_lerp(Vector3 from, Vector3 to, float t)
{
    return (Vector3){
        lerpf(from.x, to.x, t),
        lerpf(from.y, to.y, t),
        lerpf(from.z, to.z, t)
    };
}

static double terrain_grip_at(Vec2 pos)
{
    Vec2 mud_center = vec2_make(300.0, -150.0);
    if (vec2_dist(pos, mud_center) < 120.0) return 0.55;
    return 1.0;
}

static void print_status(const Tank *p, const Tank *t, int target_ai, double sim_time_s)
{
    printf("\n--- STATUS (t = %.1fs) ---\n", sim_time_s);
    printf("%-8s pos(%.0f,%.0f) hdg %.0f spd %.1f km/h turret %.0f abs %.0f gun %.1f\n",
           p->name, p->position_m.x, p->position_m.y, p->heading_deg,
           tank_speed_kmh(p), p->turret_azimuth_deg,
           tank_absolute_turret_azimuth_deg(p), p->gun_elevation_deg);
    printf("         ammo: %s x%d  %s x%d  %s x%d   selected: %s   reload: %s\n",
           ammo_get(AMMO_APFSDS)->short_name, p->ammo_count[AMMO_APFSDS],
           ammo_get(AMMO_HEAT)->short_name,   p->ammo_count[AMMO_HEAT],
           ammo_get(AMMO_HE)->short_name,     p->ammo_count[AMMO_HE],
           ammo_get(p->current_ammo)->short_name,
           p->reload_timer_s > 0.0 ? "reloading" : "READY");
    printf("         HP: %.0f/100  %s\n", p->hp, p->alive ? "" : "-- DESTROYED");

    double dx = t->position_m.x - p->position_m.x;
    double dy = t->position_m.y - p->position_m.y;
    double range = sqrt(dx * dx + dy * dy);
    double bearing = norm_deg(rad2deg(atan2(dy, dx)));

    printf("%-8s pos(%.0f,%.0f) hdg %.0f turret(abs) %.0f range %.0fm bearing %.0fdeg AI:%s\n",
           t->name, t->position_m.x, t->position_m.y, t->heading_deg,
           tank_absolute_turret_azimuth_deg(t), range, bearing,
           target_ai ? "on" : "off");
    printf("         HP: %.0f/100  %s\n\n", t->hp, t->alive ? "" : "-- DESTROYED");
}

static Vector3 tank_world_pos(const Tank *t, float height)
{
    return (Vector3){ (float)t->position_m.x, height * 0.5f, (float)t->position_m.y };
}

static Vector3 vec3_add(Vector3 a, Vector3 b)
{
    return (Vector3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static Vector3 vec3_scale(Vector3 v, float scale)
{
    return (Vector3){ v.x * scale, v.y * scale, v.z * scale };
}

static void draw_tank_3d(const Tank *t, Color body, Color turret, Color barrel)
{
    Vector3 pos = tank_world_pos(t, PLAYER_HEIGHT);
    Vector3 body_size = { 6.0f, 1.4f, 3.4f };
    Vector3 turret_size = { 2.4f, 0.9f, 2.0f };
    float az = (float)deg2rad(tank_absolute_turret_azimuth_deg(t));
    float el = (float)deg2rad(t->gun_elevation_deg);
    Vector3 barrel_dir = { cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az) };

    DrawCubeV(pos, body_size, body);
    DrawCubeWiresV(pos, body_size, BLACK);

    Vector3 turret_pos = { pos.x, pos.y + 1.0f, pos.z };
    DrawCubeV(turret_pos, turret_size, turret);
    DrawCubeWiresV(turret_pos, turret_size, BLACK);

    Vector3 muzzle = vec3_add(turret_pos, vec3_scale(barrel_dir, 3.0f));
    DrawLine3D(turret_pos, muzzle, barrel);
}

static void update_ai(double *target_throttle, double *target_steer)
{
    if ((rand() % 100) < 15) {
        *target_throttle = clampd(((double)rand() / RAND_MAX) * 1.4 - 0.4, -1.0, 1.0);
        *target_steer = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
    }
}

static void update_camera(Camera3D *camera, const Tank *player, float *orbit_yaw_deg,
                         float *orbit_pitch_deg, float *orbit_distance)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        *orbit_yaw_deg += delta.x * 0.18f;
        *orbit_pitch_deg -= delta.y * 0.12f;
    } else {
        float desired_yaw = (float)norm_deg(player->heading_deg + 180.0);
        *orbit_yaw_deg = lerpf(*orbit_yaw_deg, desired_yaw, 0.06f);
        *orbit_pitch_deg = lerpf(*orbit_pitch_deg, CAMERA_BASE_PITCH, 0.04f);
    }

    *orbit_pitch_deg = clampd(*orbit_pitch_deg, -62.0f, -8.0f);
    *orbit_distance -= GetMouseWheelMove() * 1.2f;
    *orbit_distance = clampd(*orbit_distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);

    Vector3 player_pos = tank_world_pos(player, PLAYER_HEIGHT);
    float yaw_rad = (float)deg2rad(*orbit_yaw_deg);
    float pitch_rad = (float)deg2rad(*orbit_pitch_deg);

    Vector3 forward = {
        cosf(pitch_rad) * cosf(yaw_rad),
        sinf(pitch_rad),
        cosf(pitch_rad) * sinf(yaw_rad)
    };

    Vector3 desired_pos = vec3_add(player_pos, vec3_scale(forward, -*orbit_distance));
    desired_pos.y += 3.0f;

    camera->position = vec3_lerp(camera->position, desired_pos, 0.14f);
    camera->target = vec3_lerp(camera->target, vec3_add(player_pos, (Vector3){ 0.0f, 1.1f, 0.0f }), 0.20f);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
}

static void draw_crosshair(void)
{
    Vector2 mouse = GetMousePosition();
    int cx = (int)mouse.x;
    int cy = (int)mouse.y;
    int len = 10;
    DrawLine(cx - len, cy, cx - 3, cy, RAYWHITE);
    DrawLine(cx + 3, cy, cx + len, cy, RAYWHITE);
    DrawLine(cx, cy - len, cx, cy - 3, RAYWHITE);
    DrawLine(cx, cy + 3, cx, cy + len, RAYWHITE);
    DrawCircleLines(cx, cy, 18.0f, Fade(RAYWHITE, 0.4f));
}

static Vector3 aim_point_from_mouse(const Camera3D *camera)
{
    Vector2 mouse = GetMousePosition();
    Ray ray = GetScreenToWorldRay(mouse, *camera);

    if (fabsf(ray.direction.y) > 1e-5f) {
        float t = -ray.position.y / ray.direction.y;
        if (t > 0.0f) {
            return vec3_add(ray.position, vec3_scale(ray.direction, t));
        }
    }

    return vec3_add(ray.position, vec3_scale(ray.direction, 300.0f));
}

static void apply_mouse_turret_aim(const Tank *player, const Camera3D *camera,
                                   double dt, double *turret_input, double *elev_input,
                                   Vector3 *aim_point_out)
{
    Vector3 aim_point = aim_point_from_mouse(camera);
    if (aim_point_out) *aim_point_out = aim_point;

    Vec2 aim_flat = vec2_make(aim_point.x - player->position_m.x, aim_point.z - player->position_m.y);
    double desired_azimuth = norm_deg(rad2deg(atan2(aim_flat.y, aim_flat.x)) - player->heading_deg);
    double azimuth_error = norm_deg(desired_azimuth - player->turret_azimuth_deg);
    *turret_input += clampd(azimuth_error / (player->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    double dx = aim_point.x - player->position_m.x;
    double dz = aim_point.z - player->position_m.y;
    double flat_dist = sqrt(dx * dx + dz * dz);
    double desired_elev = rad2deg(atan2(aim_point.y - 2.2, flat_dist > 1e-6 ? flat_dist : 1e-6));
    double elev_error = desired_elev - player->gun_elevation_deg;
    *elev_input += clampd(elev_error / (player->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);
}

int main(void)
{
    srand((unsigned)time(NULL));
    ammo_init_table();

    const char *display = getenv("DISPLAY");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if ((display == NULL || display[0] == '\0') &&
        (wayland_display == NULL || wayland_display[0] == '\0')) {
        fprintf(stderr, "Raylib 3D needs a graphical desktop session (Wayland or X11). Use ./leopard_sim for the console version if you are in a headless shell.\n");
        return 1;
    }

    Tank player, target;
    tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
    tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);

    int target_ai = 1;
    double target_throttle = 0.4;
    double target_steer = 0.0;
    double sim_time_s = 0.0;

    InitWindow(1280, 720, "Leopard 2A7 - 3D Frontend");
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ -14.0f, 8.0f, -14.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float orbit_yaw_deg = 225.0f;
    float orbit_pitch_deg = CAMERA_BASE_PITCH;
    float orbit_distance = CAMERA_BASE_DISTANCE;

    while (!WindowShouldClose()) {
        double dt = GetFrameTime();
        if (dt > SIM_DT_MAX) dt = SIM_DT_MAX;

        double throttle = 0.0;
        if (IsKeyDown(KEY_W)) throttle += 1.0;
        if (IsKeyDown(KEY_S)) throttle -= 1.0;

        double steer = 0.0;
        if (IsKeyDown(KEY_D)) steer += 1.0;
        if (IsKeyDown(KEY_A)) steer -= 1.0;

        double turret = 0.0;
        if (IsKeyDown(KEY_L)) turret += 1.0;
        if (IsKeyDown(KEY_J)) turret -= 1.0;

        double elev = 0.0;
        if (IsKeyDown(KEY_I)) elev += 1.0;
        if (IsKeyDown(KEY_K)) elev -= 1.0;

        if (IsKeyPressed(KEY_ONE)) player.current_ammo = AMMO_APFSDS;
        if (IsKeyPressed(KEY_TWO)) player.current_ammo = AMMO_HEAT;
        if (IsKeyPressed(KEY_THREE)) player.current_ammo = AMMO_HE;
        if (IsKeyPressed(KEY_P)) print_status(&player, &target, target_ai, sim_time_s);
        if (IsKeyPressed(KEY_R)) {
            tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
            tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);
            target_ai = 1;
            target_throttle = 0.4;
            target_steer = 0.0;
            sim_time_s = 0.0;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE)) {
            combat_fire_shot(&player, &target);
        }

        tank_update_movement(&player, throttle, steer, terrain_grip_at(player.position_m), dt);
        tank_update_turret(&player, turret, elev, dt);
        tank_tick_reload(&player, dt);

        if (target_ai && target.alive) {
            update_ai(&target_throttle, &target_steer);
            tank_update_movement(&target, target_throttle, target_steer, terrain_grip_at(target.position_m), dt);
        }

        update_camera(&camera, &player, &orbit_yaw_deg, &orbit_pitch_deg, &orbit_distance);

        Vector3 aim_point = { 0.0f, 0.0f, 0.0f };
        apply_mouse_turret_aim(&player, &camera, dt, &turret, &elev, &aim_point);

        BeginDrawing();
        ClearBackground((Color){ 14, 18, 22, 255 });

        DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                               (Color){ 18, 28, 32, 255 },
                               (Color){ 8, 10, 12, 255 });

        BeginMode3D(camera);
        DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 2400.0f, 2400.0f }, (Color){ 38, 52, 41, 255 });
        DrawGrid(40, 10.0f);
        DrawCylinder((Vector3){ 520.0f, 60.0f, -120.0f }, 8.0f, 8.0f, 120.0f, 16, (Color){ 72, 80, 72, 255 });
        DrawCylinder((Vector3){ 360.0f, 50.0f, 180.0f }, 10.0f, 10.0f, 90.0f, 12, (Color){ 64, 72, 64, 255 });
        DrawSphere(aim_point, 0.5f, GOLD);
        DrawSphereWires(aim_point, 0.5f, 8, 8, Fade(WHITE, 0.5f));
        draw_tank_3d(&player, (Color){ 78, 96, 63, 255 }, (Color){ 96, 116, 76, 255 }, DARKGREEN);
        draw_tank_3d(&target, (Color){ 120, 74, 64, 255 }, (Color){ 150, 92, 78, 255 }, MAROON);
        EndMode3D();

        draw_crosshair();

        DrawRectangle(16, 16, 500, 168, Fade(BLACK, 0.62f));
        DrawText("Leopard 2A7 - 3D Tank Frontend", 28, 28, 24, RAYWHITE);
        DrawText("W/S drive   A/D steer   J/L turret   I/K elevate", 28, 60, 18, RAYWHITE);
        DrawText("Mouse aims turret   RMB drag: orbit camera   Wheel: zoom   Left click/Space: fire", 28, 84, 18, RAYWHITE);
        DrawText("1/2/3 ammo   P status   R reset", 28, 108, 18, RAYWHITE);
        DrawText(TextFormat("Speed: %.1f km/h   Turret: %.0f deg   Gun: %.1f deg", tank_speed_kmh(&player), tank_absolute_turret_azimuth_deg(&player), player.gun_elevation_deg), 28, 132, 18, SKYBLUE);
        DrawText(TextFormat("Ammo: %s  Reload: %.1fs", ammo_get(player.current_ammo)->short_name, player.reload_timer_s), 28, 156, 18, YELLOW);

        if (!player.alive || !target.alive) {
            DrawRectangle(420, 16, 360, 60, Fade(BLACK, 0.55f));
            DrawText(player.alive ? "Target destroyed" : "Player destroyed", 434, 34, 28, ORANGE);
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            DrawText("CAMERA ORBIT", GetScreenWidth() - 160, 18, 20, LIGHTGRAY);
        }

        EndDrawing();
        sim_time_s += dt;
    }

    CloseWindow();
    return 0;
}