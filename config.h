/* car.h - player car: arcade grip/drift model, drift-charge boost, nitro, slipstream.
 *
 * Rules (see docs in README):
 *  DRIFT   hold drift (Circle) above 40% top speed with steering -> grip drops to the
 *          car's drift_grip, steering authority +25%, direction is locked to the side you
 *          entered. Counter-steer to widen/tighten. Braking or a wall hit cancels it.
 *  CHARGE  while the slip angle is above ~12 deg the charge fills (faster at speed and
 *          at bigger angles). Tiers at 25 / 55 / 90.
 *  RELEASE letting go of drift fires a burst: tier 1/2/3 = 0.8 / 1.4 / 2.0 s,
 *          +20% top speed and double acceleration.
 *  NITRO   0..100 meter. Fills from drifting, near misses (+8) and slipstream.
 *          Cross spends it (+35% top speed, +80% accel, empty in ~3 s).
 *  CAP     all speed bonuses stack but total top-speed multiplier is capped at 1.5.
 */
#ifndef CAR_H
#define CAR_H
#include <stdbool.h>
#include "mathutil.h"
#include "input.h"

#define NUM_CAR_TYPES 2

typedef struct {
    const char *name;
    float top_speed;       /* m/s */
    float accel;           /* exponential approach rate toward top speed (1/s) */
    float grip;            /* lateral damping while driving normally (1/s) */
    float drift_grip;      /* lateral damping while drifting: lower = slidier */
    float steer;           /* max yaw rate (rad/s) at low speed */
    float mass;
} CarStats;

extern const CarStats CAR_STATS[NUM_CAR_TYPES];

typedef struct {
    int   type;                      /* index into CAR_STATS */
    Vec3  pos;
    float yaw;
    float vx, vz;                    /* world velocity (m/s) - collisions edit this directly */
    float speed;                     /* signed forward speed, derived each step */
    float grip;                      /* current effective grip (blends between modes) */
    float hw, hl, mass;              /* collision half width / half length / mass */
    float hit_timer;                 /* seconds since last contact */

    /* drift / boost state */
    bool  drifting;
    float drift_dir;                 /* +1 right, -1 left */
    float slip;                      /* |slip angle| in radians */
    float charge;                    /* 0..100 */
    float boost_timer;               /* remaining drift-burst time */
    float nitro;                     /* 0..100 */
    bool  nitro_on;
    float draft, draft_target;       /* slipstream 0..1; draft_target set by the sim (0/1) */

    /* track state (filled by the sim from track_project) */
    int   seg;                       /* nearest track sample, -1 = unknown (forces a full search) */
    float s, d;                      /* arclength along the track, lateral offset (+ right) */
    /* vertical state: jumps */
    float vy, pitch;
    bool  airborne;
} Car;

void  car_reset(Car *c, int type);
void  car_place(Car *c, Vec3 pos, float yaw);   /* put the car somewhere, at rest */
/* Follow the road height; goes airborne when the road drops away faster than gravity.
 * Returns the landing speed (m/s, downward) on the tick the car touches down, else 0. */
float car_ground_update(Car *c, float ground_y, float dt);
void  car_step(Car *c, const Input *in, float dt);
void  car_on_hit(Car *c, float impact);        /* wall/traffic hit: cancels drift + charge */
void  car_add_nitro(Car *c, float amount);
int   car_tier(const Car *c);                  /* 0..3 */
float car_top_speed(const Car *c);
float car_boost_visual(const Car *c);          /* 0..1 for camera FOV / FX */

#endif
