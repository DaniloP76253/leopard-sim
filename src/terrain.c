#include "terrain.h"

#include <math.h>

static double clamp_unit(double value)
{
    return clampd(value, 0.0, 1.0);
}

double terrain_height_at(Vec2 pos)
{
    double ridge = 1.6 * sin(pos.x * 0.0024) + 1.2 * cos(pos.y * 0.0018);
    double rolling = 1.0 * sin((pos.x + pos.y) * 0.0012) + 0.7 * cos((pos.x - pos.y) * 0.0011);
    double hill_a = 0.0;
    double hill_b = 0.0;
    double hill_c = 0.0;

    Vec2 a = vec2_make(240.0, 120.0);
    Vec2 b = vec2_make(520.0, -260.0);
    Vec2 c = vec2_make(860.0, 260.0);

    double da = vec2_dist(pos, a);
    double db = vec2_dist(pos, b);
    double dc = vec2_dist(pos, c);
    if (da < 180.0) hill_a = (1.0 - da / 180.0) * 3.5;
    if (db < 220.0) hill_b = (1.0 - db / 220.0) * 2.8;
    if (dc < 260.0) hill_c = (1.0 - dc / 260.0) * 2.1;

    double berm = 0.0;
    if (pos.x > 420.0 && pos.x < 780.0 && pos.y > 40.0 && pos.y < 140.0) {
        berm = 1.4;
    }

    return ridge + rolling + hill_a + hill_b + hill_c + berm;
}

double terrain_surface_slope_at(Vec2 pos)
{
    const double step = 1.5;
    double h_l = terrain_height_at(vec2_make(pos.x - step, pos.y));
    double h_r = terrain_height_at(vec2_make(pos.x + step, pos.y));
    double h_d = terrain_height_at(vec2_make(pos.x, pos.y - step));
    double h_u = terrain_height_at(vec2_make(pos.x, pos.y + step));

    double dx = (h_r - h_l) / (2.0 * step);
    double dy = (h_u - h_d) / (2.0 * step);
    return sqrt(dx * dx + dy * dy);
}

double terrain_grip_at(Vec2 pos)
{
    double slope = terrain_surface_slope_at(pos);
    double grip = 1.0 - slope * 0.9;
    double dist_to_mud = vec2_dist(pos, vec2_make(300.0, -150.0));
    if (dist_to_mud < 120.0) grip *= 0.55;
    return clamp_unit(grip);
}

int terrain_line_blocked(Vec3 from, Vec3 to)
{
    const int steps = 64;
    for (int i = 1; i < steps; i++) {
        double t = (double)i / (double)steps;
        Vec2 xy = vec2_make(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t);
        double z = from.z + (to.z - from.z) * t;
        if (terrain_height_at(xy) + 0.05 > z) {
            return 1;
        }
    }
    return 0;
}
