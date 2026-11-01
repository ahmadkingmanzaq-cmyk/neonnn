#include <string.h>
#include "camera.h"

#define CHASE_HEIGHT 2.5f

void camera_init(Camera *c, float car_yaw) {
    memset(c, 0, sizeof(*c));
    c->yaw  = car_yaw;
    c->fov  = 62.0f;
    c->dist = 6.5f;
    c->mode = CAM_CHASE;
    c->eye    = v3(0, 3, 8);
    c->target = v3(0, 1, 0);
}

void camera_update(Camera *c, float dt, Vec3 p, float car_yaw,
                   float sf, float boost, bool look_back)
{
    /* Heading lags behind the car: the car visibly swings across the screen
     * in a turn and the camera catches up (rate = how tight the leash is). */
    c->yaw = damp_angle(c->yaw, car_yaw, 3.5f, dt);
    c->lb  = damp(c->lb, look_back ? 1.0f : 0.0f, 12.0f, dt);

    /* FOV widens with speed and boost; camera pulls back on boost. */
    c->fov  = damp(c->fov,  62.0f + 22.0f * sf + 10.0f * boost, 3.0f, dt);
    c->dist = damp(c->dist, 6.5f + 1.5f * sf + 2.2f * boost,   2.5f, dt);

    Vec3 cf = yaw_forward(car_yaw);                 /* car forward */
    Vec3 rf = yaw_forward(car_yaw + PI_F);          /* car rear    */
    Vec3 eye, tgt;

    if (c->mode == CAM_HOOD) {
        Vec3 d = (c->lb > 0.5f) ? rf : cf;
        eye = v3(p.x + cf.x * 0.3f, p.y + 1.0f, p.z + cf.z * 0.3f);
        tgt = v3(eye.x + d.x * 20.0f, eye.y + 0.2f, eye.z + d.z * 20.0f);
    } else {
        Vec3 lag = yaw_forward(c->yaw);
        Vec3 e0 = v3(p.x - lag.x * c->dist, p.y + CHASE_HEIGHT, p.z - lag.z * c->dist);
        Vec3 t0 = v3(p.x + cf.x * 6.0f,     p.y + 1.0f,         p.z + cf.z * 6.0f);
        /* look-back: sit on the car and stare out the rear */
        Vec3 e1 = v3(p.x, p.y + 1.3f, p.z);
        Vec3 t1 = v3(e1.x + rf.x * 20.0f, e1.y, e1.z + rf.z * 20.0f);
        eye = v3(lerpf(e0.x, e1.x, c->lb), lerpf(e0.y, e1.y, c->lb), lerpf(e0.z, e1.z, c->lb));
        tgt = v3(lerpf(t0.x, t1.x, c->lb), lerpf(t0.y, t1.y, c->lb), lerpf(t0.z, t1.z, c->lb));
    }

    /* Subtle high-speed shake (kicks in above ~60% speed, stronger on boost). */
    c->shake_t += dt;
    float amp = clampf((sf - 0.6f) / 0.4f, 0.0f, 1.0f) * 0.05f + boost * 0.04f + c->impact * 0.35f;
    c->impact = damp(c->impact, 0.0f, 5.0f, dt);
    float sx = (sinf(c->shake_t * 47.0f) + sinf(c->shake_t * 71.0f)) * 0.5f * amp;
    float sy = (sinf(c->shake_t * 53.0f) + sinf(c->shake_t * 89.0f)) * 0.5f * amp;
    eye.x += cosf(car_yaw) * sx;
    eye.z += sinf(car_yaw) * sx;
    eye.y += sy;

    c->eye = eye;
    c->target = tgt;
}

void camera_kick(Camera *c, float amount) { c->impact = clampf(c->impact + amount, 0.0f, 1.0f); }
