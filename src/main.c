/* =====================================================================
 * LEOPARD 2A7 -- DRIVE & GUNNERY SIMULATOR
 *
 * A text/console tank sim with War-Thunder-flavored mechanics:
 *   - differential-steering hull physics (throttle/steer -> speed/heading,
 *     with real drag from turning -- crank the stick at speed and you'll
 *     visibly bleed speed off, same as an actual tracked vehicle)
 *   - independent turret traverse + gun elevation with slew rate limits
 *   - real gravity+drag ballistic flight for each round (not hitscan)
 *   - 3D armor obliquity (hull/turret slope combined with your firing
 *     angle) compared against a range-dependent penetration table
 *   - simple gun-handling dispersion (worse while moving / slewing)
 *   - a smart-ish enemy gunner (leads your velocity, solves elevation
 *     from the actual muzzle velocity, retreats when hurt, circles at
 *     range instead of parking nose-on) that shoots back once you turn
 *     its AI on
 *   - a wave/kill loop: destroy the target and a fresh one rolls in a
 *     little tougher, plus a supply depot to resupply ammo and repair
 *
 * All numeric stats (armor mm, penetration mm, speeds, reload time...)
 * are clearly-labeled EDITABLE PLACEHOLDERS -- see ammo.c and tank.c.
 * They are NOT verified exact War Thunder datamine values, and they are
 * NOT real classified vehicle specifications. Swap in whatever numbers
 * you like; the engine below only cares about the *shape* of the data.
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "vecmath.h"
#include "ammo.h"
#include "armor.h"
#include "tank.h"
#include "projectile.h"
#include "combat.h"
#include "ai.h"
#include "match.h"

#define TICK_DT           0.5   /* seconds of sim time per player command */
#define HIT_RADIUS_M      2.0   /* horizontal tolerance = approx half-width of hull */
#define WAVE_RESPAWN_DELAY_S   4.0
#define WAVE_ORIGIN_X          0.0
#define WAVE_ORIGIN_Y          0.0
#define WAVE_MIN_RANGE_M     500.0
#define WAVE_MAX_RANGE_M     900.0
#define AI_PREFERRED_RANGE_M 350.0
#define SUPPLY_DEPOT_X         0.0
#define SUPPLY_DEPOT_Y       220.0
#define SUPPLY_RADIUS_M       20.0
#define SUPPLY_HEAL_PER_S      8.0
#define MUD_CENTER_X   300.0
#define MUD_CENTER_Y  -150.0
#define MUD_RADIUS_M   120.0

static double terrain_grip_at(Vec2 pos)
{
    Vec2 mud_center = vec2_make(MUD_CENTER_X, MUD_CENTER_Y);
    if (vec2_dist(pos, mud_center) < MUD_RADIUS_M) return 0.55; /* soft mud patch */
    return 1.0; /* firm ground / road */
}

static void print_help(void)
{
    printf(
        "\n================= CONTROLS =================\n"
        " Driving (each advances the sim by %.1fs):\n"
        "   w [n]   throttle forward   s [n]  throttle reverse\n"
        "   a [n]   steer left         d [n]  steer right\n"
        "   b [n]   brake               n [n]  do nothing / wait\n"
        "   drive <throttle -1..1> <steer -1..1>   (custom, 1 tick)\n"
        " Turret / gun:\n"
        "   j [n]   turret left        l [n]  turret right\n"
        "   i [n]   elevate up         k [n]  elevate down\n"
        "   aim <turret -1..1> <elev -1..1>        (custom, 1 tick)\n"
        " Weapons:\n"
        "   1 / 2 / 3   select ammo (APFSDS / HEAT / HE)\n"
        "   f           FIRE current ammo at the target\n"
        " Info:\n"
        "   p       status      m       ASCII map\n"
        " Scenario / debug:\n"
        "   t pos <x> <y> <heading_deg>   move the target\n"
        "   t turret <deg>                 set target turret angle\n"
        "   t ai on|off                    toggle enemy AI (drives, leads\n"
        "                                  your velocity, and shoots back)\n"
        "   reset                           restart the scenario\n"
        " help / h / ?     show this help\n"
        " quit / exit / q  end simulation\n"
        "==============================================\n"
        " Heading convention: 0deg = East (+X), 90deg = North (+Y), CCW+.\n"
        " [n] repeats a driving/turret command for n ticks (default 1).\n"
        " Turning bleeds real speed now -- crank the wheel at speed and\n"
        " you'll feel it, same as a real tracked vehicle.\n"
        " Drive over (%.0f,%.0f), radius %.0fm, to resupply ammo and repair.\n"
        " Destroy the target to clear a wave -- the next one is a little\n"
        " tougher and rolls in a few seconds later. 't ai off' freezes the\n"
        " target in place for penetration testing, same as before.\n"
        "==============================================\n\n",
        TICK_DT, SUPPLY_DEPOT_X, SUPPLY_DEPOT_Y, SUPPLY_RADIUS_M);
}

static void print_status(double sim_time_s, const Tank *p, const Tank *t, int target_ai, const MatchState *match)
{
    printf("\n--- STATUS  (t = %.1fs) ---\n", sim_time_s);
    printf("%-8s pos(%.0f,%.0f) hdg %.0f  spd %.1f km/h  turret %.0f (abs %.0f)  gun %.1f\n",
           p->name, p->position_m.x, p->position_m.y, p->heading_deg,
           tank_speed_kmh(p), p->turret_azimuth_deg,
           tank_absolute_turret_azimuth_deg(p), p->gun_elevation_deg);
    printf("         ammo: %s x%d/%d  %s x%d/%d  %s x%d/%d   selected: %s   reload: %s\n",
           ammo_get(AMMO_APFSDS)->short_name, p->ammo_count[AMMO_APFSDS], p->ammo_max[AMMO_APFSDS],
           ammo_get(AMMO_HEAT)->short_name,   p->ammo_count[AMMO_HEAT], p->ammo_max[AMMO_HEAT],
           ammo_get(AMMO_HE)->short_name,     p->ammo_count[AMMO_HE], p->ammo_max[AMMO_HE],
           ammo_get(p->current_ammo)->short_name,
           p->reload_timer_s > 0.0 ? "reloading" : "READY");
    if (p->reload_timer_s > 0.0) printf("         reload in %.1fs\n", p->reload_timer_s);
    printf("         HP: %.0f/100  %s\n", p->hp, p->alive ? "" : "-- DESTROYED");

    double dx = t->position_m.x - p->position_m.x;
    double dy = t->position_m.y - p->position_m.y;
    double range = sqrt(dx * dx + dy * dy);
    double bearing = norm_deg(rad2deg(atan2(dy, dx)));

    printf("%-8s pos(%.0f,%.0f) hdg %.0f  turret(abs) %.0f  range %.0fm  bearing %.0fdeg  AI:%s\n",
           t->name, t->position_m.x, t->position_m.y, t->heading_deg,
           tank_absolute_turret_azimuth_deg(t), range, bearing,
           target_ai ? "on" : "off");
    printf("         HP: %.0f/100  %s\n", t->hp, t->alive ? "" : "-- DESTROYED");

    if (!t->alive && match->respawn_timer_s > 0.0) {
        printf("Wave %d cleared (kills: %d) -- next wave in %.1fs\n\n", match->wave, match->kills, match->respawn_timer_s);
    } else {
        printf("Wave %d in progress -- kills so far: %d\n\n", match->wave, match->kills);
    }
}

static void print_map(const Tank *p, const Tank *t)
{
    const int half = 10;              /* grid radius in cells */
    const double cell_m = 40.0;       /* meters per cell      */
    const int size = half * 2 + 1;

    printf("\n--- MAP (each cell = %.0fm, P=you, T=target, +X=east/right, +Y=north/up) ---\n", cell_m);
    for (int row = 0; row < size; row++) {
        double world_y = (half - row) * cell_m; /* top row = +Y (north) */
        for (int col = 0; col < size; col++) {
            double world_x = (col - half) * cell_m;
            char c = '.';
            if (col == half && row == half) c = 'P';

            double dx = t->position_m.x - p->position_m.x;
            double dy = t->position_m.y - p->position_m.y;
            int tcol = half + (int)floor(dx / cell_m + 0.5);
            int trow = half - (int)floor(dy / cell_m + 0.5);
            if (col == tcol && row == trow && !(col == half && row == half)) c = t->alive ? 'T' : 'x';

            (void)world_x; (void)world_y;
            putchar(c);
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
}

/* Everything that happens to the *target* and the *player's supply
 * status* every tick, regardless of which player command triggered the
 * tick. The player's own movement/turret/reload are applied by the
 * caller first (they differ per command), then this runs once. */
static void advance_tick(Tank *player, Tank *target, AiState *target_ai_state, int target_ai,
                         MatchState *match, const SupplyDepot *depot, double *sim_time_s, double dt)
{
    if (target->alive) {
        if (target_ai) {
            AiCommand cmd = ai_compute_command(target_ai_state, target, player, dt);
            tank_update_movement(target, cmd.throttle, cmd.steer, terrain_grip_at(target->position_m), dt);
            tank_update_turret(target, cmd.turret_input, cmd.elevation_input, dt);
            tank_tick_reload(target, dt);
            if (cmd.wants_to_fire && tank_can_fire(target) && target->reload_timer_s <= 0.0 &&
                target->ammo_count[target->current_ammo] > 0) {
                combat_fire_shot(target, player, NULL);
            }
        } else {
            tank_tick_reload(target, dt);
        }
    } else {
        match->respawn_timer_s -= dt;
        if (match->respawn_timer_s <= 0.0) {
            match->wave++;
            match_spawn_wave(match, target, "Target", vec2_make(WAVE_ORIGIN_X, WAVE_ORIGIN_Y),
                             WAVE_MIN_RANGE_M, WAVE_MAX_RANGE_M);
            ai_state_init(target_ai_state, AI_PREFERRED_RANGE_M);
            printf("\n=== Wave %d inbound: %s reporting for duty. ===\n", match->wave, target->name);
        }
    }

    if (match_try_resupply(player, depot, dt)) {
        printf(">>> %s fully resupplied and repaired at the depot.\n", player->name);
    }

    *sim_time_s += dt;
}

int main(void)
{
    srand((unsigned)time(NULL));
    ammo_init_table();

    Tank player, target;
    double sim_time_s;
    int target_ai = 0;
    MatchState match;
    AiState target_ai_state;
    SupplyDepot depot = { { SUPPLY_DEPOT_X, SUPPLY_DEPOT_Y }, SUPPLY_RADIUS_M, SUPPLY_HEAL_PER_S };

    tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
    tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);
    match_state_init(&match);
    ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
    sim_time_s = 0.0;

    printf("=====================================================\n");
    printf(" LEOPARD 2A7 -- DRIVE & GUNNERY SIMULATOR\n");
    printf(" (War-Thunder-flavored mechanics, plain C, text console)\n");
    printf("=====================================================\n");
    print_help();
    print_status(sim_time_s, &player, &target, target_ai, &match);

    char line[256];
    int running = 1;

    while (running) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        char *tok[6] = { 0 };
        int ntok = 0;
        char *p = strtok(line, " \t\r\n");
        while (p && ntok < 6) { tok[ntok++] = p; p = strtok(NULL, " \t\r\n"); }

        if (ntok == 0) {
            /* empty line: just let one tick pass */
            player.moved_this_tick = 0;
            player.turret_moved_this_tick = 0;
            tank_update_movement(&player, 0.0, 0.0, terrain_grip_at(player.position_m), TICK_DT);
            tank_update_turret(&player, 0.0, 0.0, TICK_DT);
            tank_tick_reload(&player, TICK_DT);
            advance_tick(&player, &target, &target_ai_state, target_ai, &match, &depot, &sim_time_s, TICK_DT);
            continue;
        }

        const char *cmd = tok[0];

        if (!strcmp(cmd, "help") || !strcmp(cmd, "h") || !strcmp(cmd, "?")) {
            print_help();

        } else if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit") || !strcmp(cmd, "q")) {
            running = 0;

        } else if (!strcmp(cmd, "p")) {
            print_status(sim_time_s, &player, &target, target_ai, &match);

        } else if (!strcmp(cmd, "m")) {
            print_map(&player, &target);

        } else if (!strcmp(cmd, "reset")) {
            tank_init_leopard2a7(&player, "Player", vec2_make(0.0, 0.0), 0.0);
            tank_init_generic_mbt(&target, "Target", vec2_make(700.0, 150.0), 200.0);
            match_state_init(&match);
            ai_state_init(&target_ai_state, AI_PREFERRED_RANGE_M);
            sim_time_s = 0.0;
            target_ai = 0;
            printf("Scenario reset.\n");

        } else if (!strcmp(cmd, "1")) {
            player.current_ammo = AMMO_APFSDS;
            printf("Selected %s (%d left).\n", ammo_get(AMMO_APFSDS)->name, player.ammo_count[AMMO_APFSDS]);
        } else if (!strcmp(cmd, "2")) {
            player.current_ammo = AMMO_HEAT;
            printf("Selected %s (%d left).\n", ammo_get(AMMO_HEAT)->name, player.ammo_count[AMMO_HEAT]);
        } else if (!strcmp(cmd, "3")) {
            player.current_ammo = AMMO_HE;
            printf("Selected %s (%d left).\n", ammo_get(AMMO_HE)->name, player.ammo_count[AMMO_HE]);

        } else if (!strcmp(cmd, "f")) {
            int was_alive = target.alive;
            combat_fire_shot(&player, &target, NULL);
            if (was_alive && !target.alive) {
                match.kills++;
                match.respawn_timer_s = WAVE_RESPAWN_DELAY_S;
                printf(">>> Kill #%d confirmed. Wave %d clear -- reinforcements incoming.\n", match.kills, match.wave);
            }
            player.moved_this_tick = 0;
            player.turret_moved_this_tick = 0;
            tank_update_movement(&player, 0.0, 0.0, terrain_grip_at(player.position_m), TICK_DT);
            tank_update_turret(&player, 0.0, 0.0, TICK_DT);
            tank_tick_reload(&player, TICK_DT);
            advance_tick(&player, &target, &target_ai_state, target_ai, &match, &depot, &sim_time_s, TICK_DT);

        } else if (!strcmp(cmd, "w") || !strcmp(cmd, "s") || !strcmp(cmd, "a") ||
                   !strcmp(cmd, "d") || !strcmp(cmd, "b") || !strcmp(cmd, "n")) {
            int n = (ntok > 1) ? atoi(tok[1]) : 1;
            if (n < 1) n = 1;
            if (n > 200) n = 200;
            for (int i = 0; i < n; i++) {
                double throttle = 0.0, steer = 0.0;
                if (!strcmp(cmd, "w")) throttle = 1.0;
                else if (!strcmp(cmd, "s")) throttle = -1.0;
                else if (!strcmp(cmd, "a")) steer = -1.0;
                else if (!strcmp(cmd, "d")) steer = 1.0;
                else if (!strcmp(cmd, "b")) {
                    if (player.speed_ms > 0.05) throttle = -1.0;
                    else if (player.speed_ms < -0.05) throttle = 1.0;
                }
                tank_update_movement(&player, throttle, steer, terrain_grip_at(player.position_m), TICK_DT);
                tank_update_turret(&player, 0.0, 0.0, TICK_DT);
                tank_tick_reload(&player, TICK_DT);
                advance_tick(&player, &target, &target_ai_state, target_ai, &match, &depot, &sim_time_s, TICK_DT);
            }

        } else if (!strcmp(cmd, "j") || !strcmp(cmd, "l") || !strcmp(cmd, "i") || !strcmp(cmd, "k")) {
            int n = (ntok > 1) ? atoi(tok[1]) : 1;
            if (n < 1) n = 1;
            if (n > 200) n = 200;
            for (int i = 0; i < n; i++) {
                double turret_in = 0.0, elev_in = 0.0;
                if (!strcmp(cmd, "j")) turret_in = -1.0;
                else if (!strcmp(cmd, "l")) turret_in = 1.0;
                else if (!strcmp(cmd, "i")) elev_in = 1.0;
                else if (!strcmp(cmd, "k")) elev_in = -1.0;
                player.moved_this_tick = 0;
                tank_update_movement(&player, 0.0, 0.0, terrain_grip_at(player.position_m), TICK_DT);
                tank_update_turret(&player, turret_in, elev_in, TICK_DT);
                tank_tick_reload(&player, TICK_DT);
                advance_tick(&player, &target, &target_ai_state, target_ai, &match, &depot, &sim_time_s, TICK_DT);
            }

        } else if (!strcmp(cmd, "drive") && ntok >= 3) {
            double throttle = clampd(atof(tok[1]), -1.0, 1.0);
            double steer = clampd(atof(tok[2]), -1.0, 1.0);
            tank_update_movement(&player, throttle, steer, terrain_grip_at(player.position_m), TICK_DT);
            tank_update_turret(&player, 0.0, 0.0, TICK_DT);
            tank_tick_reload(&player, TICK_DT);
            advance_tick(&player, &target, &target_ai_state, target_ai, &match, &depot, &sim_time_s, TICK_DT);

        } else if (!strcmp(cmd, "aim") && ntok >= 3) {
            double turret_in = clampd(atof(tok[1]), -1.0, 1.0);
            double elev_in = clampd(atof(tok[2]), -1.0, 1.0);
            tank_update_movement(&player, 0.0, 0.0, terrain_grip_at(player.position_m), TICK_DT);
            tank_update_turret(&player, turret_in, elev_in, TICK_DT);
            tank_tick_reload(&player, TICK_DT);
            advance_tick(&player, &target, &target_ai_state, target_ai, &match, &depot, &sim_time_s, TICK_DT);

        } else if (!strcmp(cmd, "t") && ntok >= 2) {
            if (!strcmp(tok[1], "pos") && ntok >= 5) {
                target.position_m = vec2_make(atof(tok[2]), atof(tok[3]));
                target.heading_deg = norm_deg(atof(tok[4]));
                printf("Target moved to (%.0f,%.0f) heading %.0f.\n",
                       target.position_m.x, target.position_m.y, target.heading_deg);
            } else if (!strcmp(tok[1], "turret") && ntok >= 3) {
                target.turret_azimuth_deg = norm_deg(atof(tok[2]));
                printf("Target turret set to %.0f (relative).\n", target.turret_azimuth_deg);
            } else if (!strcmp(tok[1], "ai") && ntok >= 3) {
                target_ai = !strcmp(tok[2], "on");
                printf("Target AI (drives + returns fire): %s\n", target_ai ? "ON" : "OFF");
            } else {
                printf("Usage: t pos <x> <y> <heading> | t turret <deg> | t ai on|off\n");
            }

        } else {
            printf("Unknown command '%s'. Type 'help' for the command list.\n", cmd);
        }
    }

    printf("Simulation ended. Final status:\n");
    print_status(sim_time_s, &player, &target, target_ai, &match);
    return 0;
}
