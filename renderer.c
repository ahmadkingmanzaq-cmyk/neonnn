/* mathutil.h - tiny math helpers (header-only where possible). */
#ifndef MATHUTIL_H
#define MATHUTIL_H
#include <math.h>
#include <stdint.h>

#define PI_F 3.14159265f

typedef struct { float x, y, z; } Vec3;

static inline Vec3 v3(float x, float y, float z) { Vec3 v = { x, y, z }; return v; }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float wrap_pi(float a) {
    while (a >  PI_F) a -= 2.0f * PI_F;
    while (a < -PI_F) a += 2.0f * PI_F;
    return a;
}
/* Frame-rate independent exponential smoothing toward a target. */
static inline float damp(float cur, float tgt, float rate, float dt) {
    return cur + (tgt - cur) * (1.0f - expf(-rate * dt));
}
static inline float damp_angle(float cur, float tgt, float rate, float dt) {
    return cur + wrap_pi(tgt - cur) * (1.0f - expf(-rate * dt));
}
/* forward vector for a yaw (see config.h conventions) */
static inline Vec3 yaw_forward(float yaw) { return v3(sinf(yaw), 0.0f, -cosf(yaw)); }

uint32_t hash32(uint32_t x);   /* cheap integer hash for per-segment variation */

#endif
