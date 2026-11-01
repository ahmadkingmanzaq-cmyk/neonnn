#include "collision.h"
#include "config.h"

/* local axes of a box in the XZ plane: [0] = right, [1] = forward */
static void axes(float yaw, float a[2][2]) {
    a[0][0] = cosf(yaw); a[0][1] = sinf(yaw);
    a[1][0] = sinf(yaw); a[1][1] = -cosf(yaw);
}

bool box_vs_box(const Box2 *a, const Box2 *b, Contact *out) {
    float A[2][2], B[2][2];
    axes(a->yaw, A); axes(b->yaw, B);
    float dx = b->x - a->x, dz = b->z - a->z;
    float best = 1e9f, bnx = 0, bnz = 0;

    for (int i = 0; i < 4; i++) {                    /* separating axis test */
        float ux = (i < 2) ? A[i][0] : B[i - 2][0];
        float uz = (i < 2) ? A[i][1] : B[i - 2][1];
        float ra = a->hw * fabsf(ux * A[0][0] + uz * A[0][1]) + a->hl * fabsf(ux * A[1][0] + uz * A[1][1]);
        float rb = b->hw * fabsf(ux * B[0][0] + uz * B[0][1]) + b->hl * fabsf(ux * B[1][0] + uz * B[1][1]);
        float dist = dx * ux + dz * uz;
        float ov = ra + rb - fabsf(dist);
        if (ov <= 0.0f) return false;                /* found a gap: no collision */
        if (ov < best) { best = ov; float s = dist < 0 ? -1.0f : 1.0f; bnx = ux * s; bnz = uz * s; }
    }
    out->nx = bnx; out->nz = bnz; out->depth = best;
    return true;
}

bool box_vs_track_walls(const Track *t, const Box2 *a, int hint, Contact *out) {
    float A[2][2];
    axes(a->yaw, A);
    const float wall = t->half_w + 0.4f;                    /* inner face of the barrier */
    float deep = 0.0f, nx = 0, nz = 0;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            float px = a->x + A[0][0] * sx * a->hw + A[1][0] * sz * a->hl;
            float pz = a->z + A[0][1] * sx * a->hw + A[1][1] * sz * a->hl;
            TrackPoint tp;
            track_project(t, px, pz, hint, &tp);
            float over = fabsf(tp.d) - wall;
            if (over > deep) { deep = over; float sg = tp.d > 0 ? -1.0f : 1.0f; nx = tp.rx * sg; nz = tp.rz * sg; }
        }
    if (deep <= 0.0f) return false;
    out->nx = nx; out->nz = nz; out->depth = deep;
    return true;
}

float collide_car_wall(Car *c, const Track *t, float dt) {
    Box2 b = { c->pos.x, c->pos.z, c->yaw, c->hw, c->hl };
    Contact k;
    if (!box_vs_track_walls(t, &b, c->seg, &k)) return 0.0f;

    c->pos.x += k.nx * k.depth;                       /* push out of the wall */
    c->pos.z += k.nz * k.depth;

    float vn = c->vx * k.nx + c->vz * k.nz;           /* < 0: moving into the wall */
    float impact = 0.0f;
    if (vn < 0.0f) {                                  /* bounce with a little restitution */
        impact = -vn;
        c->vx -= 1.15f * vn * k.nx;
        c->vz -= 1.15f * vn * k.nz;
    }
    float drag = expf(-1.2f * dt);                    /* scraping costs speed */
    c->vx *= drag; c->vz *= drag;

    /* the wall nudges the nose parallel to it: rotate toward the wall's tangent */
    Vec3 f = yaw_forward(c->yaw);
    float tx = -k.nz, tz = k.nx;                      /* wall direction (perp. to the inward normal) */
    float cross = f.x * tz - f.z * tx;                /* >0: tangent is clockwise (more yaw) from heading */
    float dot = f.x * tx + f.z * tz;
    c->yaw += (dot >= 0 ? cross : -cross) * 6.0f * dt;

    c->hit_timer = 0.0f;
    return impact;
}

float collide_car_traffic(Car *c, TrafficCar *t, float dt) {
    (void)dt;
    if (!t->active) return 0.0f;
    if (fabsf(t->pos.z - c->pos.z) > 8.0f || fabsf(t->pos.x - c->pos.x) > 5.0f) return 0.0f;  /* broadphase */

    Box2 a = { c->pos.x, c->pos.z, c->yaw, c->hw, c->hl };
    Box2 b = { t->pos.x, t->pos.z, t->yaw, t->hw, t->hl };
    Contact k;
    if (!box_vs_box(&a, &b, &k)) return 0.0f;

    float ma = c->mass, mb = t->mass, tot = ma + mb;
    c->pos.x -= k.nx * k.depth * (mb / tot);          /* separate, heavier car moves less */
    c->pos.z -= k.nz * k.depth * (mb / tot);
    t->pos.x += k.nx * k.depth * (ma / tot);
    t->pos.z += k.nz * k.depth * (ma / tot);

    float vrel = (t->vx - c->vx) * k.nx + (t->vz - c->vz) * k.nz;   /* < 0: closing */
    float impact = 0.0f;
    if (vrel < 0.0f) {
        float j = -1.3f * vrel / (1.0f / ma + 1.0f / mb);             /* restitution 0.3 */
        c->vx -= (j / ma) * k.nx;  c->vz -= (j / ma) * k.nz;
        t->vx += (j / mb) * k.nx;  t->vz += (j / mb) * k.nz;
        impact = -vrel;

        /* off-centre hits spin the traffic car: torque = r x J, r ~ half the centre offset */
        float rx = (c->pos.x - t->pos.x) * 0.5f, rz = (c->pos.z - t->pos.z) * 0.5f;
        float torque = rx * (j * k.nz) - rz * (j * k.nx);
        t->spin = clampf(t->spin + torque * 0.35f / mb, -2.5f, 2.5f);
    }
    c->hit_timer = 0.0f;
    return impact;
}

bool nearmiss_check(const Car *c, TrafficCar *t) {
    if (!t->active) return false;
    Vec3 f = yaw_forward(c->yaw);
    float rx = cosf(c->yaw), rz = sinf(c->yaw);
    float dx = t->pos.x - c->pos.x, dz = t->pos.z - c->pos.z;
    float ahead = dx * f.x + dz * f.z, lat = dx * rx + dz * rz;
    if (dx * dx + dz * dz > 900.0f) t->nm_done = false;       /* re-arm once we're well clear */
    if (t->nm_done || fabsf(ahead) > 3.0f || c->hit_timer < 1.0f) return false;
    if (c->speed - t->spd < 10.0f) return false;              /* must be passing at speed */
    float gap = fabsf(lat) - (c->hw + t->hw);
    if (gap > 0.15f && gap < 1.8f) { t->nm_done = true; return true; }
    return false;
}
