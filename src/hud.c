#include "hud.h"
#include "ammo.h"
#include "math_ext.h"
#include <stdio.h>
#include <math.h>

static void draw_crosshair(Vector2 screen_point, Color color) {
    int cx = (int)screen_point.x;
    int cy = (int)screen_point.y;
    int len = 10;
    DrawLine(cx - len, cy, cx - 3, cy, color);
    DrawLine(cx + 3, cy, cx + len, cy, color);
    DrawLine(cx, cy - len, cx, cy - 3, color);
    DrawLine(cx, cy + 3, cx, cy + len, color);
    DrawCircleLines(cx, cy, 18.0f, Fade(color, 0.4f));
}

void draw_chase_reticle(const Camera3D *camera, Vector3 aim_target, Color color) {
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

void draw_gunner_sight_overlay(const Tank *player) {
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

void print_status(const Tank *p, const Tank *t, int target_ai, double sim_time_s, const MatchState *match) {
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