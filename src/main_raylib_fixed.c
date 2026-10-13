// src/main_raylib_fixed.c
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "tank.h"
#include "match.h"
#include "ai.h"
#include "combat.h"
#include "ammo.h"
#include "terrain.h"

#include "math_ext.h"
#include "assets.h"
#include "vfx.h"
#include "hud.h"
#include "camera_sys.h"
#include "render_tank.h"

// Definición de constantes
#define SIM_DT_MAX 0.05
#define AI_PREFERRED_RANGE_M 350.0
#define WAVE_ORIGIN_X          0.0
#define WAVE_ORIGIN_Y          0.0
#define WAVE_MIN_RANGE_M     500.0
#define WAVE_MAX_RANGE_M     900.0
#define WAVE_RESPAWN_DELAY_S   4.0
#define SUPPLY_DEPOT_X         0.0
#define SUPPLY_DEPOT_Y       220.0
#define SUPPLY_RADIUS_M       20.0
#define SUPPLY_HEAL_PER_S      8.0

// Función para determinar si el cañón del tanque tiene visión clara de un objetivo
static int turret_has_line_of_sight(const Tank *shooter, const Tank *target) {
    Vector3 shooter_gun = vec3_add(tank_combat_pos(shooter, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.4f, 0.0f });
    Vector3 target_center = vec3_add(tank_combat_pos(target, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.5f, 0.0f });
    return !terrain_line_blocked((Vec3){ shooter_gun.x, shooter_gun.y, shooter_gun.z },
                                 (Vec3){ target_center.x, target_center.y, target_center.z });
}

int main(void) {
    srand((unsigned)time(NULL));
    ammo_init_table();

    // Verificación de la sesión gráfica
    const char *display = getenv("DISPLAY");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if ((display == NULL || display[0] == '\0') &&
        (wayland_display == NULL || wayland_display[0] == '\0')) {
        fprintf(stderr, "Raylib 3D necesita una sesión gráfica (Wayland o X11).\n");
        return 1;
    }

    // Inicialización de tanques y escena
    Tank player, target;
    tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
    tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);

    SceneAssets assets;
    InitWindow(1280, 720, "Leopard 2A7 - 3D Tank Frontend");
    SetTargetFPS(60);
    DisableCursor();
    init_scene_assets(&assets);

    // Inicialización de estados y variables
    const int target_ai = 1;
    double sim_time_s = 0.0;
    int gunner_sight_active = 0;
    float camera_kick = 0.0f;

    GunnerControl ctrl = { 0.0, 0.0 };
    AiState target_ai_state;
    ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
    MatchState wave_state;
    match_state_init(&wave_state);
    SupplyDepot depot = { { SUPPLY_DEPOT_X, SUPPLY_DEPOT_Y }, SUPPLY_RADIUS_M, SUPPLY_HEAL_PER_S };
    ImpactEffectList effects = {0};

    Camera3D camera = { 0 };
    camera.position = (Vector3){ -14.0f, 8.0f, -14.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = CAMERA_CHASE_FOVY;
    camera.projection = CAMERA_PERSPECTIVE;

    float orbit_yaw_deg = 225.0f;
    float orbit_pitch_deg = CAMERA_BASE_PITCH;
    float orbit_distance = CAMERA_BASE_DISTANCE;

    // Bucle principal del juego
    while (!WindowShouldClose()) {
        double dt = GetFrameTime();
        if (dt > SIM_DT_MAX) dt = SIM_DT_MAX;

        // Manejo de eventos de teclado
        if (IsKeyPressed(KEY_R)) {
            tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
            tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);
            match_state_init(&wave_state);
            ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
            effects.count = 0;
            sim_time_s = 0.0;
            ctrl.desired_turret_azimuth_deg = 0.0;
            ctrl.desired_gun_elevation_deg = 0.0;
            camera_kick = 0.0f;
        }
        if (IsKeyPressed(KEY_P)) print_status(&player, &target, target_ai, sim_time_s, &wave_state);
        if (IsKeyPressed(KEY_V)) gunner_sight_active = !gunner_sight_active;
        if (IsKeyPressed(KEY_ONE)) player.current_ammo = AMMO_APFSDS;
        if (IsKeyPressed(KEY_TWO)) player.current_ammo = AMMO_HEAT;
        if (IsKeyPressed(KEY_THREE)) player.current_ammo = AMMO_HE;

        // Actualización de controles del jugador
        double throttle = 0.0, steer = 0.0;
        if (IsKeyDown(KEY_W)) throttle += 1.0;
        if (IsKeyDown(KEY_S)) throttle -= 1.0;
        if (IsKeyDown(KEY_D)) steer += 1.0;
        if (IsKeyDown(KEY_A)) steer -= 1.0;

        if (IsKeyDown(KEY_L)) ctrl.desired_turret_azimuth_deg = norm_deg(ctrl.desired_turret_azimuth_deg + player.turret_traverse_dps * dt);
        if (IsKeyDown(KEY_J)) ctrl.desired_turret_azimuth_deg = norm_deg(ctrl.desired_turret_azimuth_deg - player.turret_traverse_dps * dt);
        if (IsKeyDown(KEY_I)) ctrl.desired_gun_elevation_deg = clampd(ctrl.desired_gun_elevation_deg + player.gun_elevation_dps * dt, player.gun_elevation_min_deg, player.gun_elevation_max_deg);
        if (IsKeyDown(KEY_K)) ctrl.desired_gun_elevation_deg = clampd(ctrl.desired_gun_elevation_deg - player.gun_elevation_dps * dt, player.gun_elevation_min_deg, player.gun_elevation_max_deg);
        update_gunner_control_from_mouse(&ctrl, &player);

        double player_turret = 0.0, player_elev = 0.0;
        turret_input_toward(&player, ctrl.desired_turret_azimuth_deg, ctrl.desired_gun_elevation_deg, dt, &player_turret, &player_elev);

        // Actualización de movimiento y estado del tanque del jugador
        tank_update_movement(&player, throttle, steer, terrain_grip_at(player.position_m), dt);
        tank_update_turret(&player, player_turret, player_elev, dt);
        tank_tick_reload(&player, dt);
        update_chassis_pose(&player);

        // Actualización del tanque objetivo
        if (target.alive) {
            AiCommand cmd = ai_compute_command(&target_ai_state, &target, &player, dt);
            tank_update_movement(&target, cmd.throttle, cmd.steer, terrain_grip_at(target.position_m), dt);
            update_chassis_pose(&target);
            tank_update_turret(&target, cmd.turret_input, cmd.elevation_input, dt);
            tank_tick_reload(&target, dt);

            if (cmd.wants_to_fire && tank_can_fire(&target) && target.reload_timer_s <= 0.0 &&
                target.ammo_count[target.current_ammo] > 0 && turret_has_line_of_sight(&target, &player)) {
                CombatShotResult result;
                combat_fire_shot(&target, &player, &result);
                spawn_impact_effect(&effects, result, GOLD);
            }
        } else {
            wave_state.respawn_timer_s -= dt;
            if (wave_state.respawn_timer_s <= 0.0) {
                wave_state.wave++;
                match_spawn_wave(&wave_state, &target, "Target", vec2_make(WAVE_ORIGIN_X, WAVE_ORIGIN_Y), WAVE_MIN_RANGE_M, WAVE_MAX_RANGE_M);
                ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
            }
        }

        // Manejo de disparo
        if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE)) && tank_can_fire(&player)) {
            CombatShotResult result;
            combat_fire_shot(&player, &target, &result);
            spawn_impact_effect(&effects, result, RED);
            camera_kick = 1.0f;
            if (!target.alive) {
                wave_state.kills++;
                wave_state.respawn_timer_s = WAVE_RESPAWN_DELAY_S;
            }
        }
        camera_kick = fmaxf(camera_kick - (float)dt * 4.0f, 0.0f);

        // Manejo de recarga y reparación en el depósito
        if (match_try_resupply(&player, &depot, dt)) {
            printf(">>> %s fully resupplied and repaired at the depot.\n", player.name);
        }
        update_impact_effects(&effects, (float)dt);

        // Actualización de la cámara
        if (gunner_sight_active) {
            update_gunner_camera(&camera, &player, camera_kick);
        } else {
            update_chase_camera(&camera, &player, &ctrl, &orbit_yaw_deg, &orbit_pitch_deg, &orbit_distance, camera_kick);
        }

        // Renderizado de los tanques y efectos
        Vector3 turret_pos_now = tank_render_pos(&player, PLAYER_HEIGHT + 1.0f + (float)player.hull_bounce_m);
        Vector3 boresight_point = vec3_add(turret_pos_now, vec3_scale(tank_boresight_dir(&player), 260.0f));

        BeginDrawing();
        ClearBackground((Color){ 12, 16, 20, 255 });

        BeginMode3D(camera);
        DrawModel(assets.terrain_model, (Vector3){ -1200.0f, 0.0f, -1200.0f }, 1.0f, WHITE);
        DrawModelEx(assets.bunker_model, (Vector3){ 520.0f, terrain_visual_height_at(vec2_make(520.0, -120.0)) + 2.0f, -1200.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, 5.0f, (Vector3){ 1.0f, 1.0f, 1.0f }, WHITE);
        DrawModelEx(assets.bunker_model, (Vector3){ 360.0f, terrain_visual_height_at(vec2_make(360.0, 180.0)) + 2.0f, 180.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, -12.0f, (Vector3){ 0.8f, 0.8f, 0.8f }, WHITE);
        DrawModelEx(assets.rock_model, (Vector3){ 260.0f, terrain_visual_height_at(vec2_make(260.0, -40.0)) + 2.0f, -40.0f },
                    (Vector3){ 0.0f, 1.0f, 0.0f }, 0.0f, (Vector3){ 1.0f, 1.0f, 1.0f }, WHITE);

        Vector3 depot_pos = { (float)SUPPLY_DEPOT_X, terrain_visual_height_at(vec2_make(SUPPLY_DEPOT_X, SUPPLY_DEPOT_Y)) + 0.3f, (float)SUPPLY_DEPOT_Y };
        DrawCylinder(depot_pos, 4.0f, 4.0f, 0.4f, 16, (Color){ 60, 120, 200, 255 });
        DrawCylinderWires(depot_pos, 4.0f, 4.0f, 0.4f, 16, RAYWHITE);

        if (!gunner_sight_active) {
            DrawSphere(boresight_point, 0.5f, GOLD);
            DrawSphereWires(boresight_point, 0.5f, 8, 8, Fade(WHITE, 0.5f));
        }
        draw_tank_3d(&player, assets.player_hull_model, assets.player_turret_model,
                    assets.barrel_model, assets.muzzle_brake_model, assets.mantlet_model, assets.track_model);
        if (target.alive) {
            draw_tank_3d(&target, assets.enemy_hull_model, assets.enemy_turret_model,
                        assets.barrel_model, assets.muzzle_brake_model, assets.mantlet_model, assets.track_model);
        }
        draw_impact_effects(&effects);
        EndMode3D();

        // Renderizado de la interfaz de usuario
        Color aim_color = turret_has_line_of_sight(&player, &target) ? RAYWHITE : RED;
        if (gunner_sight_active) {
            draw_gunner_sight_overlay(&player);
        } else {
            draw_chase_reticle(&camera, boresight_point, aim_color);
        }

        double range_to_target = vec2_dist(player.position_m, target.position_m);
        double dist_to_depot = vec2_dist(player.position_m, depot.position);

        DrawRectangle(16, 16, 600, 244, Fade(BLACK, 0.65f));
        DrawText("Leopard 2A7 - 3D Tank Frontend", 28, 28, 24, RAYWHITE);
        DrawText("Mouse aims the gun (works all the way round) - V: toggle gunner sight", 28, 60, 18, RAYWHITE);
        DrawText("W/S drive  A/D steer  J/L/I/K trim turret+elev  RMB: free-look  Wheel: zoom", 28, 84, 18, RAYWHITE);
        DrawText("Left click / Space: fire   1/2/3 ammo   P status   R reset", 28, 108, 18, RAYWHITE);
        DrawText(TextFormat("Speed %.1f km/h   Range %.0fm  LOS:%s  Turret %.0f  Gun %.1f",
                            tank_speed_kmh(&player), range_to_target,
                            turret_has_line_of_sight(&player, &target) ? "clear" : "blocked",
                            tank_absolute_turret_azimuth_deg(&player), player.gun_elevation_deg), 28, 132, 18, SKYBLUE);
        DrawText(TextFormat("Player: ammo %s | reload %.1fs | engine %.0f | tracks %.0f | gun %.0f",
                            ammo_get(player.current_ammo)->short_name, player.reload_timer_s,
                            player.module_engine_hp, player.module_tracks_hp, player.module_gun_hp), 28, 156, 18, YELLOW);
        if (target.alive) {
            DrawText(TextFormat("Target: ammo %s | reload %.1fs | engine %.0f | tracks %.0f | gun %.0f",
                                ammo_get(target.current_ammo)->short_name, target.reload_timer_s,
                                target.module_engine_hp, target.module_tracks_hp, target.module_gun_hp), 28, 180, 18, ORANGE);
        } else {
            DrawText(TextFormat("Target down -- next wave in %.1fs", wave_state.respawn_timer_s > 0.0 ? wave_state.respawn_timer_s : 0.0),
                     28, 180, 18, ORANGE);
        }
        DrawText(TextFormat("Wave %d  |  Kills %d", wave_state.wave, wave_state.kills), 28, 204, 18, RAYWHITE);
        if (dist_to_depot <= depot.radius_m) {
            DrawText("RESUPPLYING", 220, 204, 18, SKYBLUE);
        }
        DrawText(TextFormat("Supply depot at (%.0f,%.0f), radius %.0fm", SUPPLY_DEPOT_X, SUPPLY_DEPOT_Y, SUPPLY_RADIUS_M), 28, 228, 16, GRAY);

        if (!player.alive) {
            DrawRectangle(650, 16, 340, 60, Fade(BLACK, 0.55f));
            DrawText("Player destroyed", 664, 34, 28, ORANGE);
        }

        EndDrawing();
        sim_time_s += dt;
    }

    // Liberación de recursos y cierre de la ventana
    unload_scene_assets(&assets);
    CloseWindow();
    return 0;
}