#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ai.h"
#include "ammo.h"
#include "combat.h"
#include "match.h"
#include "tank.h"
#include "terrain.h"

#define SIM_DT_MAX 0.05
#define PLAYER_HEIGHT 1.45f

#define CAMERA_BASE_DISTANCE 18.0f
#define CAMERA_MIN_DISTANCE   8.0f
#define CAMERA_MAX_DISTANCE  32.0f
#define CAMERA_BASE_PITCH   -18.0f
#define CAMERA_CHASE_FOVY    55.0f
#define GUNNER_SIGHT_FOVY    20.0f  /* narrow FOV = "magnified sight optics" */

/* how many degrees of desired aim one pixel of mouse motion buys you --
 * this drives the turret directly (not a raycast into the world), so it
 * works identically in chase view and gunner-sight view, and the turret
 * can track all the way round to the rear instead of being limited to
 * wherever the chase camera happens to be pointed. */
#define TURRET_MOUSE_SENS_DEG_PER_PX     0.12
#define ELEVATION_MOUSE_SENS_DEG_PER_PX  0.07

/* match/AI tuning -- kept in sync with main.c's values by convention,
 * not by sharing a header, since the two frontends' main()s differ too
 * much to be worth factoring further right now */
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

#define MAX_IMPACT_EFFECTS 8

typedef struct {
    Model terrain_model;
    Model bunker_model;
    Model rock_model;
    Model player_hull_model;
    Model player_turret_model;
    Model enemy_hull_model;
    Model enemy_turret_model;
    Model barrel_model;
    Model muzzle_brake_model;
    Model mantlet_model;
    Model track_model;
    Texture2D terrain_texture;
    Texture2D rock_texture;
    Texture2D bunker_texture;
    Texture2D player_camo_texture;
    Texture2D enemy_camo_texture;
    Texture2D metal_texture;
} SceneAssets;

/* Where the player currently *wants* the gun pointed. Both mouse motion
 * and the J/L/I/K trim keys nudge this same state; tank_update_turret()
 * is then just servoed toward it every tick. Azimuth is hull-relative
 * (matches Tank::turret_azimuth_deg) so it wraps cleanly through the
 * full 360 degrees. */
typedef struct {
    double desired_turret_azimuth_deg;
    double desired_gun_elevation_deg;
} GunnerControl;

typedef struct {
    Vector3 position;
    float life_s;
    float max_life_s;
    Color color;
} ImpactEffect;

typedef struct {
    ImpactEffect items[MAX_IMPACT_EFFECTS];
    int count;
} ImpactEffectList;

static float lerpf(float from, float to, float t)
{
    return from + (to - from) * t;
}

/* Plain lerpf() on raw degrees takes the *long* way round whenever the
 * two angles straddle the +-180 wrap boundary (e.g. 179 -> -179, which
 * is a 2-degree correction, not a 358-degree one), and orbit_yaw_deg is
 * unbounded (keeps accumulating during RMB free-look) so it can drift
 * far outside +-180 entirely. Both produce a camera that visibly spins
 * or lurches instead of smoothly settling -- this interpolates along
 * the shortest arc instead. */
static float lerp_angle_deg(float from_deg, float to_deg, float t)
{
    float delta = (float)norm_deg((double)to_deg - (double)from_deg);
    return (float)norm_deg((double)from_deg + delta * t);
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

static Color color_lerp(Color a, Color b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    Color c;
    c.r = (unsigned char)(a.r + (b.r - a.r) * t);
    c.g = (unsigned char)(a.g + (b.g - a.g) * t);
    c.b = (unsigned char)(a.b + (b.b - a.b) * t);
    c.a = 255;
    return c;
}

static float terrain_visual_height_at(Vec2 pos)
{
    double logical = terrain_height_at(pos);
    double normalized = clampd((logical + 5.0) / 12.0, 0.0, 1.0);
    return (float)(normalized * 20.0);
}

static Vector3 tank_render_pos(const Tank *t, float extra_height)
{
    float ground = terrain_visual_height_at(t->position_m);
    return (Vector3){ (float)t->position_m.x, ground + extra_height, (float)t->position_m.y };
}

static Vector3 tank_combat_pos(const Tank *t, float extra_height)
{
    float ground = (float)terrain_height_at(t->position_m);
    return (Vector3){ (float)t->position_m.x, ground + extra_height, (float)t->position_m.y };
}

static Vector3 tank_boresight_dir(const Tank *t)
{
    float az = (float)deg2rad(tank_absolute_turret_azimuth_deg(t));
    float el = (float)deg2rad(t->gun_elevation_deg);
    return (Vector3){ cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az) };
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

static void print_status(const Tank *p, const Tank *t, int target_ai, double sim_time_s, const MatchState *match)
{
    printf("\n--- STATUS (t = %.1fs) ---\n", sim_time_s);
    printf("%-8s pos(%.0f,%.0f) hdg %.0f spd %.1f km/h turret %.0f abs %.0f gun %.1f\n",
           p->name, p->position_m.x, p->position_m.y, p->heading_deg,
           tank_speed_kmh(p), p->turret_azimuth_deg,
           tank_absolute_turret_azimuth_deg(p), p->gun_elevation_deg);
    printf("         ammo: %s x%d/%d  %s x%d/%d  %s x%d/%d   selected: %s   reload: %s\n",
           ammo_get(AMMO_APFSDS)->short_name, p->ammo_count[AMMO_APFSDS], p->ammo_max[AMMO_APFSDS],
           ammo_get(AMMO_HEAT)->short_name,   p->ammo_count[AMMO_HEAT], p->ammo_max[AMMO_HEAT],
           ammo_get(AMMO_HE)->short_name,     p->ammo_count[AMMO_HE], p->ammo_max[AMMO_HE],
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
    printf("         modules: engine %.0f tracks %.0f turret %.0f gun %.0f optics %.0f\n",
           t->module_engine_hp, t->module_tracks_hp, t->module_turret_hp,
           t->module_gun_hp, t->module_optics_hp);
    printf("         Wave %d | kills %d\n\n", match->wave, match->kills);
}

/* Computes an (axis, angle) pair that rotates a mesh built with its
 * length along +Y (like GenMeshCylinder's default orientation) so that
 * axis instead points along `dir` (need not be normalized). Standard
 * axis = cross((0,1,0), dir), angle = acos(dot((0,1,0), dir)) -- this
 * was verified independently against the Rodrigues rotation formula
 * (not just eyeballed) before going anywhere near this file: rotating
 * (0,1,0) by the returned (axis, angle) reproduces `dir` to within
 * floating-point error for straight-ahead, behind, 45-degree azimuths,
 * both gun elevation limits, and the hull-right wheel direction. */
static void axis_angle_from_y_to(Vector3 dir, Vector3 *out_axis, float *out_angle_deg)
{
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-6f) { *out_axis = (Vector3){ 1.0f, 0.0f, 0.0f }; *out_angle_deg = 0.0f; return; }
    Vector3 d = { dir.x / len, dir.y / len, dir.z / len };

    Vector3 axis = { d.z, 0.0f, -d.x }; /* cross((0,1,0), d) */
    float axis_len = sqrtf(axis.x * axis.x + axis.z * axis.z);
    if (axis_len < 1e-5f) {
        /* dir is (anti)parallel to +Y -- doesn't happen in practice given
         * this game's elevation limits, but handled cleanly regardless */
        *out_axis = (Vector3){ 1.0f, 0.0f, 0.0f };
        *out_angle_deg = (d.y > 0.0f) ? 0.0f : 180.0f;
        return;
    }
    *out_axis = (Vector3){ axis.x / axis_len, 0.0f, axis.z / axis_len };
    float dotv = d.y; /* dot((0,1,0), d) */
    if (dotv > 1.0f) dotv = 1.0f;
    if (dotv < -1.0f) dotv = -1.0f;
    *out_angle_deg = (float)rad2deg((double)acosf(dotv));
}

static void hull_basis_vectors(const Tank *t, Vector3 *out_forward, Vector3 *out_right)
{
    float h = (float)deg2rad(t->heading_deg);
    *out_forward = (Vector3){ cosf(h), 0.0f, sinf(h) };
    *out_right   = (Vector3){ -sinf(h), 0.0f, cosf(h) };
}

/* Real hull/turret rotation, texture and all -- DrawCubeV (the original
 * approach) can't rotate at all, so the hull used to stay axis-aligned
 * regardless of heading and only the gun-barrel line actually swung to
 * face the way you were aiming. Deriving raylib's rotate-about-Y matrix
 * against this project's heading convention (0=+X, CCW+, world Z = sim
 * Y) gives rotationAngle = -heading_deg; if a build ever looks like it's
 * facing backwards, flip that sign first. */
static void draw_tank_3d(const Tank *t, Model hull_model, Model turret_model,
                         Model barrel_model, Model muzzle_brake_model,
                         Model mantlet_model, Model track_model)
{
    Vector3 pos = tank_render_pos(t, PLAYER_HEIGHT + (float)t->hull_bounce_m);
    Vector3 barrel_dir = tank_boresight_dir(t);
    Vector3 up_axis = { 0.0f, 1.0f, 0.0f };
    Vector3 unit_scale = { 1.0f, 1.0f, 1.0f };

    float hull_yaw = -(float)t->heading_deg;
    DrawModelEx(hull_model, pos, up_axis, hull_yaw, unit_scale, WHITE);

    /* tracks flank the hull and share its yaw exactly -- they run
     * parallel to the hull, so no extra rotation math is needed beyond
     * what the hull itself already uses */
    Vector3 forward_dir, right_dir;
    hull_basis_vectors(t, &forward_dir, &right_dir);
    Vector3 track_offset = vec3_scale(right_dir, 1.9f);
    Vector3 track_l = { pos.x - track_offset.x, pos.y - 0.6f, pos.z - track_offset.z };
    Vector3 track_r = { pos.x + track_offset.x, pos.y - 0.6f, pos.z + track_offset.z };
    Color track_tint = (Color){ 40, 40, 42, 255 };
    DrawModelEx(track_model, track_l, up_axis, hull_yaw, unit_scale, track_tint);
    DrawModelEx(track_model, track_r, up_axis, hull_yaw, unit_scale, track_tint);

    /* road wheels: plain spheres, deliberately -- a sphere looks the
     * same from every angle, so these need zero orientation math and
     * carry zero risk of the "which way did that rotate" bugs the
     * barrel/tracks needed care for */
    const int wheel_count = 5;
    const float wheel_spacing = 1.05f;
    const float wheel_radius = 0.42f;
    Color wheel_color = (Color){ 26, 26, 28, 255 };
    for (int i = 0; i < wheel_count; i++) {
        float along = (i - (wheel_count - 1) * 0.5f) * wheel_spacing;
        Vector3 along_offset = vec3_scale(forward_dir, along);
        DrawSphere((Vector3){ track_l.x + along_offset.x, track_l.y, track_l.z + along_offset.z }, wheel_radius, wheel_color);
        DrawSphere((Vector3){ track_r.x + along_offset.x, track_r.y, track_r.z + along_offset.z }, wheel_radius, wheel_color);
    }

    Vector3 turret_pos = { pos.x, pos.y + 1.0f, pos.z };
    float turret_yaw = -(float)tank_absolute_turret_azimuth_deg(t);
    DrawModelEx(turret_model, turret_pos, up_axis, turret_yaw, unit_scale, WHITE);

    Vector3 mantlet_pos = vec3_add(turret_pos, vec3_scale(barrel_dir, 1.0f));
    DrawModelEx(mantlet_model, mantlet_pos, up_axis, turret_yaw, unit_scale, (Color){ 100, 102, 98, 255 });

    /* the barrel is a real cylinder now, oriented along the actual 3D
     * aim direction (azimuth + elevation) via axis_angle_from_y_to();
     * GenMeshCylinder is assumed base-anchored (extends from local
     * y=0 to y=height), so `turret_pos` is where the barrel *starts*.
     * If a real build ever shows the barrel centered on the turret
     * instead of projecting out from it, that assumption is the first
     * thing to check. */
    Vector3 barrel_axis; float barrel_angle_deg;
    axis_angle_from_y_to(barrel_dir, &barrel_axis, &barrel_angle_deg);
    Color metal_tint = (Color){ 130, 132, 128, 255 };
    DrawModelEx(barrel_model, turret_pos, barrel_axis, barrel_angle_deg, unit_scale, metal_tint);

    Vector3 muzzle_pos = vec3_add(turret_pos, vec3_scale(barrel_dir, 3.0f));
    DrawModelEx(muzzle_brake_model, muzzle_pos, barrel_axis, barrel_angle_deg, unit_scale, (Color){ 90, 92, 88, 255 });
}

/* ---------------------------------------------------------------------
 * Aiming: mouse + keyboard both feed one "desired aim" state, which is
 * then servoed toward -- the AI uses the exact same servo math in ai.c.
 * ------------------------------------------------------------------- */

static void update_gunner_control_from_mouse(GunnerControl *ctrl, const Tank *player)
{
    Vector2 delta = GetMouseDelta();
    ctrl->desired_turret_azimuth_deg = norm_deg(ctrl->desired_turret_azimuth_deg + delta.x * TURRET_MOUSE_SENS_DEG_PER_PX);
    ctrl->desired_gun_elevation_deg = clampd(ctrl->desired_gun_elevation_deg - delta.y * ELEVATION_MOUSE_SENS_DEG_PER_PX,
                                             player->gun_elevation_min_deg, player->gun_elevation_max_deg);
}

static void turret_input_toward(const Tank *player, double desired_az_rel_deg, double desired_el_deg,
                                double dt, double *turret_input, double *elev_input)
{
    double az_error = norm_deg(desired_az_rel_deg - player->turret_azimuth_deg);
    *turret_input = clampd(az_error / (player->turret_traverse_dps * dt + 1e-6), -1.0, 1.0);

    double el_error = desired_el_deg - player->gun_elevation_deg;
    *elev_input = clampd(el_error / (player->gun_elevation_dps * dt + 1e-6), -1.0, 1.0);
}

/* ---------------------------------------------------------------------
 * Cameras: a third-person chase/orbit view for driving and situational
 * awareness, and a real gunner-sight view (locked to the barrel, narrow
 * FOV) for precision shooting. Both share the same aiming state above.
 * ------------------------------------------------------------------- */

static void update_chase_camera(Camera3D *camera, const Tank *player, float *orbit_yaw_deg,
                                float *orbit_pitch_deg, float *orbit_distance, float kick)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        *orbit_yaw_deg = (float)norm_deg((double)*orbit_yaw_deg + delta.x * 0.18f);
        *orbit_pitch_deg -= delta.y * 0.12f;
    } else {
        float desired_yaw = (float)norm_deg(player->heading_deg + 180.0);
        *orbit_yaw_deg = lerp_angle_deg(*orbit_yaw_deg, desired_yaw, 0.06f);
        *orbit_pitch_deg = lerpf(*orbit_pitch_deg, CAMERA_BASE_PITCH, 0.04f);
    }

    *orbit_pitch_deg = clampd(*orbit_pitch_deg, -62.0f, -8.0f);
    *orbit_distance -= GetMouseWheelMove() * 1.2f;
    *orbit_distance = clampd(*orbit_distance, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);

    Vector3 player_pos = tank_render_pos(player, PLAYER_HEIGHT);
    float yaw_rad = (float)deg2rad(*orbit_yaw_deg);
    float pitch_rad = (float)deg2rad(*orbit_pitch_deg);

    Vector3 forward = {
        cosf(pitch_rad) * cosf(yaw_rad),
        sinf(pitch_rad),
        cosf(pitch_rad) * sinf(yaw_rad)
    };

    Vector3 desired_pos = vec3_add(player_pos, vec3_scale(forward, -*orbit_distance));
    desired_pos.y += 3.0f;

    /* keep the camera from sinking into a hillside behind the tank */
    float ground_at_cam = terrain_visual_height_at(vec2_make(desired_pos.x, desired_pos.z));
    if (desired_pos.y < ground_at_cam + 1.2f) desired_pos.y = ground_at_cam + 1.2f;

    camera->position = vec3_lerp(camera->position, desired_pos, 0.14f);
    camera->target = vec3_lerp(camera->target, vec3_add(player_pos, (Vector3){ 0.0f, 1.1f, 0.0f }), 0.20f);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera->fovy = lerpf(camera->fovy, CAMERA_CHASE_FOVY, 0.15f) + kick * 1.5f;
}

static void update_gunner_camera(Camera3D *camera, const Tank *player, float kick)
{
    Vector3 turret_pos = tank_render_pos(player, PLAYER_HEIGHT + 1.0f + (float)player->hull_bounce_m);
    Vector3 aim_dir = tank_boresight_dir(player);
    Vector3 eye = vec3_add(turret_pos, vec3_scale(aim_dir, 0.9f));

    camera->position = vec3_lerp(camera->position, eye, 0.35f);
    camera->target = vec3_add(camera->position, aim_dir);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera->fovy = lerpf(camera->fovy, GUNNER_SIGHT_FOVY, 0.15f) + kick * 3.0f;
}

/* ---------------------------------------------------------------------
 * Reticles
 * ------------------------------------------------------------------- */

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

static void draw_chase_reticle(const Camera3D *camera, Vector3 aim_target, Color color)
{
    Vector3 to_target = {
        aim_target.x - camera->position.x,
        aim_target.y - camera->position.y,
        aim_target.z - camera->position.z
    };
    Vector3 fwd = {
        camera->target.x - camera->position.x,
        camera->target.y - camera->position.y,
        camera->target.z - camera->position.z
    };
    float dot = to_target.x * fwd.x + to_target.y * fwd.y + to_target.z * fwd.z;

    int w = GetScreenWidth();
    int h = GetScreenHeight();

    if (dot > 0.0f) {
        Vector2 screen = GetWorldToScreen(aim_target, *camera);
        if (screen.x >= 0.0f && screen.x <= (float)w && screen.y >= 0.0f && screen.y <= (float)h) {
            draw_crosshair(screen, color);
            return;
        }
    }

    double cam_bearing = rad2deg(atan2(fwd.z, fwd.x));
    double aim_bearing = rad2deg(atan2(to_target.z, to_target.x));
    double rel = norm_deg(aim_bearing - cam_bearing);

    const char *label = (rel > 0.0) ? "GUN >>" : "<< GUN";
    int text_w = MeasureText(label, 20);
    int y = h / 2;
    int x = (rel > 0.0) ? (w - text_w - 24) : 24;
    DrawText(label, x, y, 20, color);
}

static void draw_gunner_sight_overlay(const Tank *player)
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int cx = w / 2;
    int cy = h / 2;
    Color reticle_color = (Color){ 60, 255, 120, 220 };

    DrawLine(cx - 140, cy, cx - 20, cy, reticle_color);
    DrawLine(cx + 20, cy, cx + 140, cy, reticle_color);
    DrawLine(cx, cy - 140, cx, cy - 20, reticle_color);
    DrawLine(cx, cy + 20, cx, cy + 100, reticle_color);
    DrawCircleLines(cx, cy, 3.0f, reticle_color);

    for (int i = -6; i <= 6; i++) {
        if (i == 0) continue;
        int x = cx + i * 22;
        int tick_h = (i % 3 == 0) ? 10 : 5;
        DrawLine(x, cy - tick_h, x, cy + tick_h, Fade(reticle_color, 0.7f));
    }

    int tape_x = w - 60;
    int tape_top = cy - 160;
    int tape_bottom = cy + 160;
    DrawLine(tape_x, tape_top, tape_x, tape_bottom, Fade(RAYWHITE, 0.5f));
    double elev_span = player->gun_elevation_max_deg - player->gun_elevation_min_deg;
    double elev_t = (elev_span > 1e-6) ? (player->gun_elevation_deg - player->gun_elevation_min_deg) / elev_span : 0.5;
    elev_t = clampd(elev_t, 0.0, 1.0);
    int marker_y = tape_bottom - (int)(elev_t * (double)(tape_bottom - tape_top));
    DrawTriangle((Vector2){ (float)(tape_x - 10), (float)marker_y },
                (Vector2){ (float)(tape_x + 4), (float)(marker_y - 6) },
                (Vector2){ (float)(tape_x + 4), (float)(marker_y + 6) }, YELLOW);
    DrawText(TextFormat("%.1f", player->gun_elevation_deg), tape_x + 8, marker_y - 8, 16, YELLOW);

    DrawText(TextFormat("GUNNER SIGHT  |  %s", ammo_get(player->current_ammo)->short_name),
             cx - 110, cy + 150, 20, reticle_color);
}

/* ---------------------------------------------------------------------
 * Impact effects -- driven by CombatShotResult so the visual reflects
 * what actually happened (and where) instead of guessing from stdout.
 * ------------------------------------------------------------------- */

static void spawn_impact_effect(ImpactEffectList *list, CombatShotResult result, Color color)
{
    if (result.outcome == SHOT_OUTCOME_INVALID) return;
    int idx = list->count % MAX_IMPACT_EFFECTS;
    ImpactEffect *e = &list->items[idx];
    e->position = (Vector3){ (float)result.impact_point.x, (float)result.impact_point.z, (float)result.impact_point.y };
    e->max_life_s = 0.45f;
    e->life_s = e->max_life_s;
    e->color = color;
    list->count++;
}

static void update_impact_effects(ImpactEffectList *list, float dt)
{
    for (int i = 0; i < MAX_IMPACT_EFFECTS; i++) {
        if (list->items[i].life_s > 0.0f) list->items[i].life_s -= dt;
    }
}

static void draw_impact_effects(const ImpactEffectList *list)
{
    for (int i = 0; i < MAX_IMPACT_EFFECTS; i++) {
        const ImpactEffect *e = &list->items[i];
        if (e->life_s <= 0.0f) continue;
        float t = e->life_s / e->max_life_s;
        float radius = 1.6f * (1.0f - t) + 0.3f;
        DrawSphere(e->position, radius, Fade(e->color, t));
    }
}

/* ---------------------------------------------------------------------
 * Procedural textures -- no network/asset pipeline here, so everything
 * is generated in code: mottled ground, speckled rock, a blocky bunker
 * pattern, and a blotch camo for each side (different palettes so
 * player/enemy read apart at a glance even before you check the HUD).
 * ------------------------------------------------------------------- */

static Image build_terrain_heightmap_image(void)
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

static Image build_ground_texture_image(void)
{
    const int size = 512;
    Image noise = GenImagePerlinNoise(size, size, 0, 0, 4.0f);
    Image img = GenImageColor(size, size, (Color){ 60, 74, 48, 255 });

    Color grass_a = (Color){ 58, 78, 46, 255 };
    Color grass_b = (Color){ 78, 92, 58, 255 };
    Color dirt    = (Color){ 96, 78, 54, 255 };

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            Color n = GetImageColor(noise, x, y);
            float t = n.r / 255.0f;
            Color base = color_lerp(grass_a, grass_b, t);
            if (t > 0.72f) base = color_lerp(base, dirt, (t - 0.72f) / 0.28f);
            ImageDrawPixel(&img, x, y, base);
        }
    }

    for (int i = 0; i < (size * size) / 40; i++) {
        int x = rand() % size, y = rand() % size;
        ImageDrawPixel(&img, x, y, (Color){ 70, 66, 60, 255 });
    }

    UnloadImage(noise);
    return img;
}

static Image build_rock_texture_image(void)
{
    const int size = 128;
    Image img = GenImageColor(size, size, (Color){ 110, 108, 100, 255 });
    for (int i = 0; i < 90; i++) {
        int cx = rand() % size, cy = rand() % size;
        int r = 4 + rand() % 14;
        int shade = rand() % 40;
        Color c = { (unsigned char)(90 + shade), (unsigned char)(88 + shade), (unsigned char)(82 + shade), 255 };
        ImageDrawCircle(&img, cx, cy, r, c);
    }
    return img;
}

static Image build_bunker_texture_image(void)
{
    const int size = 256;
    Image img = GenImageColor(size, size, (Color){ 96, 92, 82, 255 });
    for (int y = 0; y < size; y += 32) {
        int offset = ((y / 32) % 2) * 32;
        for (int x = -32; x < size; x += 64) {
            Color shade = (((x + offset) / 64 + y / 32) % 2 == 0) ? (Color){ 88, 84, 74, 255 } : (Color){ 104, 100, 90, 255 };
            ImageDrawRectangle(&img, x + offset, y, 58, 28, shade);
        }
    }
    return img;
}

static Image build_camo_texture_image(Color base, Color blotch_a, Color blotch_b)
{
    const int size = 256;
    Image img = GenImageColor(size, size, base);

    for (int i = 0; i < 55; i++) {
        int cx = rand() % size, cy = rand() % size;
        int r = 10 + rand() % 26;
        Color c = (rand() % 2) ? blotch_a : blotch_b;
        ImageDrawCircle(&img, cx, cy, r, c);
        /* a smaller overlapping circle breaks up the perfect-circle
         * silhouette so blotches read as organic instead of polka-dots */
        int r2 = r / 2 + rand() % 8;
        int ox = cx + (rand() % (r + 1)) - r / 2;
        int oy = cy + (rand() % (r + 1)) - r / 2;
        ImageDrawCircle(&img, ox, oy, r2, c);
    }

    /* faint vertical wear/rain streaks */
    for (int i = 0; i < 10; i++) {
        int x = rand() % size;
        int y0 = rand() % (size / 2);
        int len = 40 + rand() % 90;
        Color streak = color_lerp(base, BLACK, 0.18f);
        ImageDrawRectangle(&img, x, y0, 1 + rand() % 2, len, streak);
    }

    for (int i = 0; i < (size * size) / 30; i++) {
        int x = rand() % size, y = rand() % size;
        Color c = GetImageColor(img, x, y);
        int r = (int)c.r - 10; if (r < 0) r = 0;
        int g = (int)c.g - 10; if (g < 0) g = 0;
        int b = (int)c.b - 10; if (b < 0) b = 0;
        ImageDrawPixel(&img, x, y, (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    }

    return img;
}

static Image build_metal_texture_image(void)
{
    const int size = 128;
    Image img = GenImageColor(size, size, (Color){ 112, 114, 110, 255 });

    for (int i = 0; i < (size * size) / 8; i++) {
        int x = rand() % size, y = rand() % size;
        int shade = (rand() % 30) - 15;
        Color c = GetImageColor(img, x, y);
        int r = (int)c.r + shade; if (r < 0) r = 0; if (r > 255) r = 255;
        int g = (int)c.g + shade; if (g < 0) g = 0; if (g > 255) g = 255;
        int b = (int)c.b + shade; if (b < 0) b = 0; if (b > 255) b = 255;
        ImageDrawPixel(&img, x, y, (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    }

    /* a few horizontal machining/wear bands */
    for (int i = 0; i < 6; i++) {
        int y = rand() % size;
        ImageDrawRectangle(&img, 0, y, size, 1 + rand() % 2, (Color){ 80, 82, 78, 255 });
    }

    return img;
}

static void init_scene_assets(SceneAssets *assets)
{
    memset(assets, 0, sizeof(*assets));

    Image terrain_heightmap = build_terrain_heightmap_image();
    Mesh terrain_mesh = GenMeshHeightmap(terrain_heightmap, (Vector3){ 2400.0f, 20.0f, 2400.0f });
    assets->terrain_model = LoadModelFromMesh(terrain_mesh);
    Image ground_image = build_ground_texture_image();
    assets->terrain_texture = LoadTextureFromImage(ground_image);
    SetMaterialTexture(&assets->terrain_model.materials[0], MATERIAL_MAP_ALBEDO, assets->terrain_texture);
    UnloadImage(ground_image);
    UnloadImage(terrain_heightmap);

    Mesh bunker_mesh = GenMeshCube(10.0f, 4.0f, 10.0f);
    assets->bunker_model = LoadModelFromMesh(bunker_mesh);
    Image bunker_image = build_bunker_texture_image();
    assets->bunker_texture = LoadTextureFromImage(bunker_image);
    SetMaterialTexture(&assets->bunker_model.materials[0], MATERIAL_MAP_ALBEDO, assets->bunker_texture);
    UnloadImage(bunker_image);

    Mesh rock_mesh = GenMeshSphere(4.5f, 16, 16);
    assets->rock_model = LoadModelFromMesh(rock_mesh);
    Image rock_image = build_rock_texture_image();
    assets->rock_texture = LoadTextureFromImage(rock_image);
    SetMaterialTexture(&assets->rock_model.materials[0], MATERIAL_MAP_ALBEDO, assets->rock_texture);
    UnloadImage(rock_image);

    assets->player_hull_model = LoadModel("hull.obj");
    assets->player_turret_model = LoadModel("turret.obj");
    Image player_camo = build_camo_texture_image((Color){ 74, 86, 58, 255 }, (Color){ 54, 64, 42, 255 }, (Color){ 96, 100, 74, 255 });
    assets->player_camo_texture = LoadTextureFromImage(player_camo);
    SetMaterialTexture(&assets->player_hull_model.materials[0], MATERIAL_MAP_ALBEDO, assets->player_camo_texture);
    SetMaterialTexture(&assets->player_turret_model.materials[0], MATERIAL_MAP_ALBEDO, assets->player_camo_texture);
    UnloadImage(player_camo);

    assets->enemy_hull_model = LoadModel("hull.obj");
    assets->enemy_turret_model = LoadModel("turret.obj");
    Image enemy_camo = build_camo_texture_image((Color){ 150, 124, 88, 255 }, (Color){ 120, 96, 66, 255 }, (Color){ 168, 140, 104, 255 });
    assets->enemy_camo_texture = LoadTextureFromImage(enemy_camo);
    SetMaterialTexture(&assets->enemy_hull_model.materials[0], MATERIAL_MAP_ALBEDO, assets->enemy_camo_texture);
    SetMaterialTexture(&assets->enemy_turret_model.materials[0], MATERIAL_MAP_ALBEDO, assets->enemy_camo_texture);
    UnloadImage(enemy_camo);

    /* barrel/mantlet/tracks are shared between both sides -- bare metal
     * and dark rubber/steel don't really vary by camo scheme */
    Image metal_image = build_metal_texture_image();
    assets->metal_texture = LoadTextureFromImage(metal_image);
    UnloadImage(metal_image);

    assets->barrel_model = LoadModelFromMesh(GenMeshCylinder(0.09f, 3.3f, 10));
    SetMaterialTexture(&assets->barrel_model.materials[0], MATERIAL_MAP_ALBEDO, assets->metal_texture);

    assets->muzzle_brake_model = LoadModelFromMesh(GenMeshCylinder(0.14f, 0.35f, 10));
    SetMaterialTexture(&assets->muzzle_brake_model.materials[0], MATERIAL_MAP_ALBEDO, assets->metal_texture);

    assets->mantlet_model = LoadModelFromMesh(GenMeshCube(0.7f, 0.6f, 0.5f));
    SetMaterialTexture(&assets->mantlet_model.materials[0], MATERIAL_MAP_ALBEDO, assets->metal_texture);

    assets->track_model = LoadModelFromMesh(GenMeshCube(5.6f, 0.6f, 0.7f));
    SetMaterialTexture(&assets->track_model.materials[0], MATERIAL_MAP_ALBEDO, assets->metal_texture);
}

static void unload_scene_assets(SceneAssets *assets)
{
    UnloadTexture(assets->terrain_texture);
    UnloadTexture(assets->rock_texture);
    UnloadTexture(assets->bunker_texture);
    UnloadTexture(assets->player_camo_texture);
    UnloadTexture(assets->enemy_camo_texture);
    UnloadTexture(assets->metal_texture);
    UnloadModel(assets->terrain_model);
    UnloadModel(assets->bunker_model);
    UnloadModel(assets->rock_model);
    UnloadModel(assets->player_hull_model);
    UnloadModel(assets->player_turret_model);
    UnloadModel(assets->enemy_hull_model);
    UnloadModel(assets->enemy_turret_model);
    UnloadModel(assets->barrel_model);
    UnloadModel(assets->muzzle_brake_model);
    UnloadModel(assets->mantlet_model);
    UnloadModel(assets->track_model);
}

static int turret_has_line_of_sight(const Tank *shooter, const Tank *target)
{
    Vector3 shooter_gun = vec3_add(tank_combat_pos(shooter, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.4f, 0.0f });
    Vector3 target_center = vec3_add(tank_combat_pos(target, PLAYER_HEIGHT), (Vector3){ 0.0f, 0.5f, 0.0f });
    return !terrain_line_blocked((Vec3){ shooter_gun.x, shooter_gun.y, shooter_gun.z },
                                 (Vec3){ target_center.x, target_center.y, target_center.z });
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
    InitWindow(1280, 720, "Leopard 2A7 - 3D Tank Frontend");
    SetTargetFPS(60);
    DisableCursor();
    init_scene_assets(&assets);

    const int target_ai = 1; /* always on in the 3D build; console has the toggle */
    double sim_time_s = 0.0;
    int gunner_sight_active = 0;
    float camera_kick = 0.0f;

    GunnerControl ctrl = { 0.0, 0.0 };
    AiState target_ai_state;
    ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
    MatchState wave_state;
    match_state_init(&wave_state);
    SupplyDepot depot = { { SUPPLY_DEPOT_X, SUPPLY_DEPOT_Y }, SUPPLY_RADIUS_M, SUPPLY_HEAL_PER_S };
    ImpactEffectList effects;
    memset(&effects, 0, sizeof(effects));

    Camera3D camera = { 0 };
    camera.position = (Vector3){ -14.0f, 8.0f, -14.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = CAMERA_CHASE_FOVY;
    camera.projection = CAMERA_PERSPECTIVE;

    float orbit_yaw_deg = 225.0f;
    float orbit_pitch_deg = CAMERA_BASE_PITCH;
    float orbit_distance = CAMERA_BASE_DISTANCE;

    while (!WindowShouldClose()) {
        double dt = GetFrameTime();
        if (dt > SIM_DT_MAX) dt = SIM_DT_MAX;

        if (IsKeyPressed(KEY_R)) {
            tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
            tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);
            match_state_init(&wave_state);
            ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
            memset(&effects, 0, sizeof(effects));
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

        double throttle = 0.0;
        if (IsKeyDown(KEY_W)) throttle += 1.0;
        if (IsKeyDown(KEY_S)) throttle -= 1.0;

        double steer = 0.0;
        if (IsKeyDown(KEY_D)) steer += 1.0;
        if (IsKeyDown(KEY_A)) steer -= 1.0;

        /* keyboard trim and mouse both nudge the same desired-aim state */
        if (IsKeyDown(KEY_L)) ctrl.desired_turret_azimuth_deg = norm_deg(ctrl.desired_turret_azimuth_deg + player.turret_traverse_dps * dt);
        if (IsKeyDown(KEY_J)) ctrl.desired_turret_azimuth_deg = norm_deg(ctrl.desired_turret_azimuth_deg - player.turret_traverse_dps * dt);
        if (IsKeyDown(KEY_I)) ctrl.desired_gun_elevation_deg = clampd(ctrl.desired_gun_elevation_deg + player.gun_elevation_dps * dt, player.gun_elevation_min_deg, player.gun_elevation_max_deg);
        if (IsKeyDown(KEY_K)) ctrl.desired_gun_elevation_deg = clampd(ctrl.desired_gun_elevation_deg - player.gun_elevation_dps * dt, player.gun_elevation_min_deg, player.gun_elevation_max_deg);
        update_gunner_control_from_mouse(&ctrl, &player);

        double player_turret = 0.0, player_elev = 0.0;
        turret_input_toward(&player, ctrl.desired_turret_azimuth_deg, ctrl.desired_gun_elevation_deg, dt,
                            &player_turret, &player_elev);

        tank_update_movement(&player, throttle, steer, terrain_grip_at(player.position_m), dt);
        tank_update_turret(&player, player_turret, player_elev, dt);
        tank_tick_reload(&player, dt);
        update_chassis_pose(&player);

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
                match_spawn_wave(&wave_state, &target, "Target", vec2_make(WAVE_ORIGIN_X, WAVE_ORIGIN_Y),
                                 WAVE_MIN_RANGE_M, WAVE_MAX_RANGE_M);
                ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
            }
        }

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

        if (match_try_resupply(&player, &depot, dt)) {
            printf(">>> %s fully resupplied and repaired at the depot.\n", player.name);
        }
        update_impact_effects(&effects, (float)dt);

        if (gunner_sight_active) {
            update_gunner_camera(&camera, &player, camera_kick);
        } else {
            update_chase_camera(&camera, &player, &orbit_yaw_deg, &orbit_pitch_deg, &orbit_distance, camera_kick);
        }

        Vector3 turret_pos_now = tank_render_pos(&player, PLAYER_HEIGHT + 1.0f + (float)player.hull_bounce_m);
        Vector3 boresight_point = vec3_add(turret_pos_now, vec3_scale(tank_boresight_dir(&player), 260.0f));

        BeginDrawing();
        ClearBackground((Color){ 12, 16, 20, 255 });

        BeginMode3D(camera);
        DrawModel(assets.terrain_model, (Vector3){ -1200.0f, 0.0f, -1200.0f }, 1.0f, WHITE);
        DrawModelEx(assets.bunker_model, (Vector3){ 520.0f, terrain_visual_height_at(vec2_make(520.0, -120.0)) + 2.0f, -120.0f },
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

    unload_scene_assets(&assets);
    CloseWindow();
    return 0;
}
