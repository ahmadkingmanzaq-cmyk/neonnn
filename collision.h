#include <string.h>
#include "car.h"

const CarStats CAR_STATS[NUM_CAR_TYPES] = {
    /* name         top    accel  grip  drift  steer  mass */
    { "AURORA GT",  80.0f, 0.70f, 7.0f, 2.6f,  1.50f, 1.0f },   /* faster, wider drifts   */
    { "KESTREL R",  73.0f, 0.78f, 8.5f, 3.2f,  1.75f, 0.9f },   /* sharper, quicker off the line */
};

#define NITRO_MAX    100.0f
#define NITRO_DRAIN  33.3f          /* per second: full tank lasts ~3 s */
#define TIER1 25.0f
#define TIER2 55.0f
#define TIER3 90.0f
static const float BURST_DUR[4] = { 0.0f, 0.8f, 1.4f, 2.0f };

void car_reset(Car *c, int type) {
    memset(c, 0, sizeof(*c));
    c->type = type;
    c->grip = CAR_STATS[type].grip;
    c->hw = 1.0f;  c->hl = 2.1f;
    c->mass = CAR_STATS[type].mass;
    c->hit_timer = 10.0f;
    c->seg = -1;
}

void car_place(Car *c, Vec3 pos, float yaw) {
    c->pos = pos; c->yaw = yaw;
    c->vx = c->vz = c->speed = c->vy = c->pitch = 0.0f;
    c->airborne = false;
    c->drifting = false; c->charge = 0.0f; c->boost_timer = 0.0f;
    c->seg = -1;
}

#define GRAVITY 24.0f      /* arcade-heavy so jumps feel weighty rather than floaty */

float car_ground_update(Car *c, float ground_y, float dt) {
    float landed = 0.0f;
    if (!c->airborne) {
        float want_vy = (ground_y - c->pos.y) / dt;                 /* vertical speed needed to hug the road */
        if (want_vy < c->vy - GRAVITY * dt * 1.2f && fabsf(c->speed) > 5.0f) {
            c->airborne = true;                                    /* road fell away: take off with current vy */
        } else {
            c->pos.y = ground_y;
            c->vy = want_vy;
        }
    }
    if (c->airborne) {
        c->vy -= GRAVITY * dt;
        c->pos.y += c->vy * dt;
        if (c->pos.y <= ground_y) {
            landed = -c->vy;
            c->pos.y = ground_y;
            c->vy = 0.0f;
            c->airborne = false;
        }
    }
    c->pitch = damp(c->pitch, atan2f(c->vy, fmaxf(fabsf(c->speed), 5.0f)), 10.0f, dt);
    return landed > 0 ? landed : 0.0f;
}

int   car_tier(const Car *c)      { return c->charge >= TIER3 ? 3 : (c->charge >= TIER2 ? 2 : (c->charge >= TIER1 ? 1 : 0)); }
float car_top_speed(const Car *c) { return CAR_STATS[c->type].top_speed; }

float car_boost_visual(const Car *c) {
    if (c->nitro_on) return 1.0f;
    if (c->boost_timer > 0.0f) return 0.7f * clampf(c->boost_timer / 0.4f, 0.0f, 1.0f);
    return 0.0f;
}

void car_add_nitro(Car *c, float amount) { c->nitro = clampf(c->nitro + amount, 0.0f, NITRO_MAX); }

static void end_drift(Car *c, bool release) {
    int tier = car_tier(c);
    if (release && tier > 0) c->boost_timer = BURST_DUR[tier];   /* fire the burst */
    c->charge = 0.0f;
    c->drifting = false;
}

void car_on_hit(Car *c, float impact) {
    if (impact > 3.0f) { c->charge = 0.0f; c->drifting = false; }  /* wall / crash wipes charge */
}

void car_step(Car *c, const Input *in, float dt) {
    const CarStats *s = &CAR_STATS[c->type];

    if (c->airborne) {                           /* no steering, grip or throttle in the air */
        Vec3 fa = yaw_forward(c->yaw);
        c->pos.x += c->vx * dt;
        c->pos.z += c->vz * dt;
        c->speed = c->vx * fa.x + c->vz * fa.z;
        c->hit_timer += dt;
        c->draft = clampf(c->draft - 2.0f * dt, 0.0f, 1.0f);
        c->nitro_on = false;
        return;
    }

    Vec3 f = yaw_forward(c->yaw);
    float fs = c->vx * f.x + c->vz * f.z;
    float sf = clampf(fs / s->top_speed, 0.0f, 1.2f);

    /* ---- drift state machine ---- */
    if (!c->drifting) {
        if (in->drift && fs > 0.40f * s->top_speed && fabsf(in->steer) > 0.30f) {
            c->drifting  = true;
            c->drift_dir = in->steer > 0 ? 1.0f : -1.0f;
            c->charge    = 0.0f;
        }
    } else {
        if (in->brake > 0)                                        end_drift(c, false);  /* cancel */
        else if (!in->drift || fs < 0.25f * s->top_speed)         end_drift(c, true);   /* release */
    }

    /* ---- steering ---- */
    float grip_target = c->drifting ? s->drift_grip : s->grip;
    c->grip = damp(c->grip, grip_target, 10.0f, dt);               /* smooth grip transition */

    float steer = in->steer;
    if (c->drifting) {
        /* direction locked to entry side; stick only tightens/widens the arc */
        steer = c->drift_dir * (0.55f + 0.45f * (in->steer * c->drift_dir));
    }
    float dir  = fs >= 0 ? 1.0f : -1.0f;
    float turn = steer * s->steer * (c->drifting ? 1.25f : 1.0f)
               * clampf(fabsf(fs) / 15.0f, 0, 1) / (1.0f + fabsf(fs) / 60.0f);
    c->yaw += turn * dir * dt;

    /* ---- split velocity into forward / lateral in the NEW heading basis ---- */
    f = yaw_forward(c->yaw);
    Vec3 r = v3(cosf(c->yaw), 0, sinf(c->yaw));
    fs = c->vx * f.x + c->vz * f.z;
    float lat = c->vx * r.x + c->vz * r.z;
    c->slip = fabsf(atan2f(lat, fmaxf(fs, 1.0f)));

    /* ---- slipstream ---- */
    c->draft = clampf(c->draft + (c->draft_target > 0.5f ? 1.2f : -2.0f) * dt, 0.0f, 1.0f);

    /* ---- boost sources and stacking ---- */
    c->nitro_on = in->boost && c->nitro > 0.0f;
    if (c->nitro_on) c->nitro = fmaxf(c->nitro - NITRO_DRAIN * dt, 0.0f);
    float burst = 0.0f;
    if (c->boost_timer > 0.0f) { burst = 1.0f; c->boost_timer -= dt; }
    float nitro_f = c->nitro_on ? 1.0f : 0.0f;

    float top_mult = fminf(1.0f + 0.35f * nitro_f + 0.20f * burst + 0.08f * c->draft, 1.5f);
    float acc_mult = fminf(1.0f + 0.80f * nitro_f + 1.00f * burst, 2.2f);
    float top = s->top_speed * top_mult;

    /* ---- longitudinal ---- */
    if (in->throttle > 0 || nitro_f > 0 || burst > 0) fs += (top - fs) * (1.0f - expf(-s->accel * acc_mult * dt));
    else                                              fs -= fs * 0.15f * dt;   /* coast drag */
    if (in->brake > 0) {
        if (fs > 0.5f) fs -= 70.0f * dt;
        else           fs = fmaxf(fs - 25.0f * dt, -12.0f);                    /* reverse */
    }
    if (c->drifting) fs -= fs * 0.06f * dt;                                    /* sliding scrubs speed */

    /* ---- drift charge + nitro income ---- */
    if (c->drifting && c->slip > 0.21f) {                                      /* ~12 degrees */
        c->charge = fminf(c->charge + 35.0f * clampf(sf / 0.6f, 0.5f, 1.4f)
                                           * clampf(c->slip / 0.35f, 0.6f, 1.5f) * dt, 100.0f);
        car_add_nitro(c, 6.0f * dt);
    }
    if (c->draft > 0.0f) car_add_nitro(c, 5.0f * c->draft * dt);

    /* ---- lateral grip, recombine, integrate ---- */
    lat *= expf(-c->grip * dt);
    c->vx = f.x * fs + r.x * lat;
    c->vz = f.z * fs + r.z * lat;
    c->pos.x += c->vx * dt;
    c->pos.z += c->vz * dt;
    c->speed = fs;
    c->hit_timer += dt;
}
