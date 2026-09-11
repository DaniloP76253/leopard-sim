#ifndef VECMATH_H
#define VECMATH_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Shared physics constant -- used by projectile.c (flight integration) and
 * ai.c (ballistic elevation solution). Defined once here so the two can
 * never quietly drift apart. */
#define GRAVITY_MSS 9.81

/* ---- 2D vector (horizontal plane, meters) ---- */
typedef struct {
    double x;
    double y;
} Vec2;

static inline Vec2 vec2_make(double x, double y) { Vec2 v = { x, y }; return v; }
static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return vec2_make(a.x + b.x, a.y + b.y); }
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return vec2_make(a.x - b.x, a.y - b.y); }
static inline Vec2 vec2_scale(Vec2 a, double s) { return vec2_make(a.x * s, a.y * s); }
static inline double vec2_length(Vec2 a) { return sqrt(a.x * a.x + a.y * a.y); }
static inline double vec2_dist(Vec2 a, Vec2 b) { return vec2_length(vec2_sub(a, b)); }

/* ---- 3D vector (x,y horizontal, z = height in meters) ---- */
typedef struct {
    double x, y, z;
} Vec3;

static inline Vec3 vec3_make(double x, double y, double z) { Vec3 v = { x, y, z }; return v; }
static inline double vec3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline double vec3_length(Vec3 a) { return sqrt(vec3_dot(a, a)); }
static inline Vec3 vec3_normalize(Vec3 a) {
    double len = vec3_length(a);
    if (len < 1e-9) return vec3_make(0, 0, 0);
    return vec3_make(a.x / len, a.y / len, a.z / len);
}

/* ---- angle helpers ---- */
static inline double deg2rad(double d) { return d * M_PI / 180.0; }
static inline double rad2deg(double r) { return r * 180.0 / M_PI; }

/* wrap an angle into (-180, 180] */
static inline double norm_deg(double d) {
    while (d > 180.0) d -= 360.0;
    while (d <= -180.0) d += 360.0;
    return d;
}

static inline double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

#endif /* VECMATH_H */
