#ifndef TERRAIN_H
#define TERRAIN_H

#include "vecmath.h"

double terrain_height_at(Vec2 pos);
double terrain_surface_slope_at(Vec2 pos);
double terrain_grip_at(Vec2 pos);
int terrain_line_blocked(Vec3 from, Vec3 to);

#endif /* TERRAIN_H */