#include "math_ext.h"
#include "tank.h" // Para norm_deg
#include <math.h>

float lerpf(float from, float to, float t) {
    return from + (to - from) * t;
}

float lerp_angle_deg(float from_deg, float to_deg, float t) {
    float delta = (float)norm_deg((double)to_deg - (double)from_deg);
    return (float)norm_deg((double)from_deg + delta * t);
}

Vector3 vec3_add(Vector3 a, Vector3 b) {
    return (Vector3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 vec3_scale(Vector3 v, float scale) {
    return (Vector3){ v.x * scale, v.y * scale, v.z * scale };
}

Vector3 vec3_lerp(Vector3 from, Vector3 to, float t) {
    return (Vector3){ lerpf(from.x, to.x, t), lerpf(from.y, to.y, t), lerpf(from.z, to.z, t) };
}

void axis_angle_from_y_to(Vector3 dir, Vector3 *out_axis, float *out_angle_deg) {
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-6f) { *out_axis = (Vector3){ 1.0f, 0.0f, 0.0f }; *out_angle_deg = 0.0f; return; }
    Vector3 d = { dir.x / len, dir.y / len, dir.z / len };

    Vector3 axis = { d.z, 0.0f, -d.x };
    float axis_len = sqrtf(axis.x * axis.x + axis.z * axis.z);
    if (axis_len < 1e-5f) {
        *out_axis = (Vector3){ 1.0f, 0.0f, 0.0f };
        *out_angle_deg = (d.y > 0.0f) ? 0.0f : 180.0f;
        return;
    }
    *out_axis = (Vector3){ axis.x / axis_len, 0.0f, axis.z / axis_len };
    float dotv = d.y;
    if (dotv > 1.0f) dotv = 1.0f;
    if (dotv < -1.0f) dotv = -1.0f;
    *out_angle_deg = (float)rad2deg((double)acosf(dotv));
}

void axis_angle_from_x_to(Vector3 dir, Vector3 *out_axis, float *out_angle_deg) {
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-6f) { *out_axis = (Vector3){ 0.0f, 1.0f, 0.0f }; *out_angle_deg = 0.0f; return; }
    Vector3 d = { dir.x / len, dir.y / len, dir.z / len };

    Vector3 axis = { 0.0f, -d.z, d.y };
    float axis_len = sqrtf(axis.y * axis.y + axis.z * axis.z);
    if (axis_len < 1e-5f) {
        *out_axis = (Vector3){ 0.0f, 1.0f, 0.0f };
        *out_angle_deg = (d.x > 0.0f) ? 0.0f : 180.0f;
        return;
    }
    *out_axis = (Vector3){ 0.0f, axis.y / axis_len, axis.z / axis_len };
    float dotv = d.x;
    if (dotv > 1.0f) dotv = 1.0f;
    if (dotv < -1.0f) dotv = -1.0f;
    *out_angle_deg = (float)rad2deg((double)acosf(dotv));
}

float wrap_positive(float value, float period) {
    float wrapped = fmodf(value, period);
    if (wrapped < 0.0f) wrapped += period;
    return wrapped;
}