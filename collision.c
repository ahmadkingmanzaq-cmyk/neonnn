/* camera.h - chase / hood camera with damping, speed FOV, boost pull-back, shake. */
#ifndef CAMERA_H
#define CAMERA_H
#include <stdbool.h>
#include "mathutil.h"

typedef enum { CAM_CHASE = 0, CAM_HOOD = 1 } CamMode;

typedef struct {
    Vec3  eye, target;
    float yaw;          /* damped heading -> gives the lag when turning */
    float fov;          /* degrees, vertical */
    float dist;         /* chase distance */
    float lb;           /* 0..1 look-back blend */
    float shake_t;
    float impact;       /* 0..1 collision shake, decays quickly */
    CamMode mode;
} Camera;

void camera_init(Camera *c, float car_yaw);
/* speed_frac: 0..1 of top speed. boost: 0..1 (smoothed by caller). */
void camera_update(Camera *c, float dt, Vec3 car_pos, float car_yaw,
                   float speed_frac, float boost, bool look_back);

/* One-shot shake from a crash; amount 0..1. */
void camera_kick(Camera *c, float amount);

#endif
