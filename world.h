#include <string.h>
#include "traffic.h"
#include "config.h"
#include "mesh.h"

#define NUM_VARIANTS 6
typedef struct { int kind; uint32_t body, trim; float hw, hl, mass; Mesh *solid, *glow; } Variant;
/* kind: 0 sedan, 1 hatch, 2 van, 3 truck. All original generic shapes. */
static Variant V[NUM_VARIANTS] = {
    { 0, RGB( 30, 60,140), COL_CYAN,    1.00f, 2.10f, 1.0f, 0, 0 },
    { 0, RGB( 20,110, 90), COL_MAGENTA, 1.00f, 2.10f, 1.0f, 0, 0 },
    { 1, RGB(150, 90, 20), COL_YELLOW,  1.00f, 1.90f, 0.9f, 0, 0 },
    { 2, RGB( 70, 70, 90), COL_MAGENTA, 1.10f, 2.40f, 1.5f, 0, 0 },
    { 3, RGB( 90, 20, 30), COL_CYAN,    1.25f, 4.20f, 3.5f, 0, 0 },
    { 0, RGB(120,120,140), COL_YELLOW,  1.00f, 2.10f, 1.0f, 0, 0 },
};
static bool s_built = false;

static float lane_x(int lane) { return -6.0f + 4.0f * (float)lane; }

/* ---- tiny xorshift RNG ---- */
static uint32_t rnd(Traffic *t) { t->rng ^= t->rng << 13; t->rng ^= t->rng >> 17; t->rng ^= t->rng << 5; return t->rng; }
static float rndf(Traffic *t, float lo, float hi) { return lo + (hi - lo) * ((rnd(t) & 0xFFFF) / 65535.0f); }

/* ---- meshes ---- */
static void wheel(Mesh *m, float x, float z) { mesh_box(m, v3(x, 0.34f, z), v3(0.18f, 0.34f, 0.40f), RGB(18, 18, 22)); }

static void build_variant(Variant *v) {
    Mesh *m = v->solid = mesh_create(640, 1280);
    Mesh *g = v->glow  = mesh_create(96, 192);
    const uint32_t glass = RGB(10, 24, 48), tail = RGB(255, 20, 60);
    float hw = v->hw, hl = v->hl, rear = 0;

    if (v->kind == 0 || v->kind == 1) {                       /* sedan / hatch */
        float bl = (v->kind == 0) ? 2.0f : 1.8f;
        for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) wheel(m, sx * (hw - 0.05f), sz * (bl - 0.8f));
        mesh_box(m, v3(0, 0.55f, 0),    v3(hw - 0.1f, 0.25f, bl), v->body);
        mesh_box(m, v3(0, 0.98f, 0.25f), v3(0.70f, 0.22f, v->kind == 0 ? 1.0f : 1.1f), glass);
        mesh_box(m, v3(0, 1.22f, 0.25f), v3(0.40f, 0.02f, 0.60f), v->trim);               /* neon roof bar */
        mesh_box(m, v3(0, 0.70f, bl + 0.02f), v3(0.80f, 0.06f, 0.02f), tail);
        rear = bl;
        mesh_glow_fan(g, v3(0, 0.70f, rear + 0.15f), v3(1.5f, 0, 0), v3(0, 0.5f, 0), tail, 170);
    } else if (v->kind == 2) {                                 /* van */
        for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) wheel(m, sx * (hw - 0.05f), sz * 1.5f);
        mesh_box(m, v3(0, 0.90f, 0), v3(hw - 0.05f, 0.55f, hl - 0.1f), v->body);
        mesh_box(m, v3(0, 0.95f, 0), v3(hw - 0.04f, 0.08f, hl - 0.09f), v->trim);          /* side stripe */
        rear = hl - 0.1f;
        for (int sx = -1; sx <= 1; sx += 2) {
            mesh_box(m, v3(sx * 0.7f, 1.0f, rear + 0.02f), v3(0.15f, 0.15f, 0.02f), tail);
            mesh_glow_fan(g, v3(sx * 0.7f, 1.0f, rear + 0.15f), v3(0.9f, 0, 0), v3(0, 0.6f, 0), tail, 170);
        }
    } else {                                                   /* truck: cab + container */
        const float zs[3] = { -3.0f, 0.8f, 3.0f };
        for (int i = 0; i < 3; i++) for (int sx = -1; sx <= 1; sx += 2) wheel(m, sx * (hw - 0.05f), zs[i]);
        mesh_box(m, v3(0, 1.10f, -(hl - 1.0f)), v3(hw - 0.05f, 0.90f, 1.0f), v->body);   /* cab */
        mesh_box(m, v3(0, 1.65f, 1.0f), v3(hw, 1.25f, 3.0f), RGB(40, 45, 70));            /* container */
        mesh_box(m, v3(0, 1.00f, 1.0f), v3(hw + 0.01f, 0.10f, 3.01f), v->trim);           /* stripe */
        rear = hl - 0.2f;
        for (int sx = -1; sx <= 1; sx += 2) {
            mesh_box(m, v3(sx * 0.8f, 0.8f, rear + 0.02f), v3(0.2f, 0.1f, 0.02f), tail);
            mesh_glow_fan(g, v3(sx * 0.8f, 0.8f, rear + 0.15f), v3(1.0f, 0, 0), v3(0, 0.6f, 0), tail, 170);
        }
    }
    /* neon underglow in the car's trim colour */
    mesh_glow_fan(g, v3(0, 0.08f, 0), v3(hw + 1.2f, 0, 0), v3(0, 0, hl + 1.0f), v->trim, 80);
    mesh_finalize(m);
    mesh_finalize(g);
}

/* ---- spawning (cars live on the track: arclength s + lane offset) ---- */
static bool lane_clear(const Traffic *t, const Track *tr, int lane, float s, float clear, const TrafficCar *self) {
    for (int i = 0; i < MAX_TRAFFIC; i++) {
        const TrafficCar *o = &t->cars[i];
        if (!o->active || o == self) continue;
        if (fabsf(o->d - lane_x(lane)) < 2.5f && fabsf(track_delta_s(tr, s, o->s)) < clear) return false;
    }
    return true;
}

static void spawn(Traffic *t, const Track *tr, TrafficCar *c, float s) {
    c->active = false;
    for (int tries = 0; tries < 8; tries++) {
        int lane = (int)(rnd(t) % NUM_LANES);
        if (!lane_clear(t, tr, lane, s, 35.0f, c)) continue;
        int type = (int)(rnd(t) % NUM_VARIANTS);
        memset(c, 0, sizeof(*c));
        c->active = true;
        c->type = type; c->lane = lane;
        c->hw = V[type].hw; c->hl = V[type].hl; c->mass = V[type].mass;
        c->target_speed = (V[type].kind == 3) ? rndf(t, 18.0f, 26.0f) : rndf(t, 22.0f, 42.0f);
        TrackPoint tp;
        track_at(tr, s, lane_x(lane), &tp);
        c->pos = tp.pos; c->yaw = tp.yaw; c->seg = tp.seg; c->s = tp.s; c->d = lane_x(lane);
        c->vx = tp.tx * c->target_speed; c->vz = tp.tz * c->target_speed;
        c->spd = c->target_speed;
        c->lc_timer = rndf(t, 3.0f, 10.0f);
        return;
    }
}

void traffic_init(Traffic *t, const Car *player, const Track *tr) {
    if (!s_built) { for (int i = 0; i < NUM_VARIANTS; i++) build_variant(&V[i]); s_built = true; }
    memset(t, 0, sizeof(*t));
    t->rng = 0x1234567u;
    for (int i = 0; i < MAX_TRAFFIC; i++)
        spawn(t, tr, &t->cars[i], player->s + 50.0f + i * 14.0f + rndf(t, 0, 8.0f));
}

/* ---- behaviour ---- */
void traffic_update(Traffic *t, float dt, const Car *pl, const Track *tr) {
    for (int i = 0; i < MAX_TRAFFIC; i++) {
        TrafficCar *c = &t->cars[i];
        if (!c->active) { spawn(t, tr, c, pl->s + rndf(t, 130.0f, 210.0f)); continue; }

        TrackPoint tp;
        track_project(tr, c->pos.x, c->pos.z, c->seg, &tp);
        c->seg = tp.seg; c->s = tp.s; c->d = tp.d;
        c->spd = c->vx * tp.tx + c->vz * tp.tz;

        /* recycle cars that fell far behind or got far ahead of the player */
        float rel = track_delta_s(tr, pl->s, c->s);                 /* + = ahead of the player */
        if (rel < -70.0f || rel > 240.0f) { spawn(t, tr, c, pl->s + rndf(t, 130.0f, 210.0f)); continue; }

        /* follow whatever is ahead in our lane (other traffic or the player) */
        float cap = c->target_speed;
        for (int j = 0; j < MAX_TRAFFIC; j++) {
            const TrafficCar *o = &t->cars[j];
            if (j == i || !o->active) continue;
            float dd = track_delta_s(tr, c->s, o->s);
            if (dd > 0 && fabsf(o->d - c->d) < 2.6f) {
                float gap = dd - (c->hl + o->hl);
                if (gap < 30.0f) { float lim = gap < 8.0f ? o->spd * 0.8f : o->spd; if (lim < cap) cap = lim; }
            }
        }
        {
            float dd = track_delta_s(tr, c->s, pl->s);
            if (dd > 0 && fabsf(pl->d - c->d) < 2.6f) {
                float gap = dd - (c->hl + pl->hl);
                if (gap < 30.0f) { float os = fmaxf(pl->speed, 0.0f); float lim = gap < 8.0f ? os * 0.8f : os; if (lim < cap) cap = lim; }
            }
        }
        /* slow down for bends so the lane-keeping can hold the line */
        float kmax = track_max_curv(tr, c->s, 40.0f);
        cap = fminf(cap, sqrtf(12.0f / (kmax + 1e-3f)));

        /* occasional lane change when the neighbouring lane is clear */
        c->lc_timer -= dt;
        if (c->lc_timer <= 0.0f) {
            c->lc_timer = rndf(t, 4.0f, 10.0f);
            int nl = c->lane + ((rnd(t) & 1) ? 1 : -1);
            if (nl >= 0 && nl < NUM_LANES && lane_clear(t, tr, nl, c->s, 24.0f, c)) {
                float dp = track_delta_s(tr, c->s, pl->s);
                bool player_near = fabsf(dp) < 20.0f && fabsf(pl->d - lane_x(nl)) < 3.0f;
                if (!player_near) c->lane = nl;
            }
        }

        /* steer toward a point ahead in our lane; speed toward the cap (shoves bleed out here) */
        TrackPoint tg;
        track_at(tr, c->s + 8.0f + c->spd * 0.3f, lane_x(c->lane), &tg);
        float dx = tg.pos.x - c->pos.x, dz = tg.pos.z - c->pos.z;
        float len = sqrtf(dx * dx + dz * dz) + 1e-3f;
        float dvx = dx / len * cap - c->vx, dvz = dz / len * cap - c->vz;
        float mag = sqrtf(dvx * dvx + dvz * dvz), maxdv = 12.0f * dt;
        if (mag > maxdv) { dvx *= maxdv / mag; dvz *= maxdv / mag; }
        c->vx += dvx; c->vz += dvz;

        c->pos.x += c->vx * dt;
        c->pos.z += c->vz * dt;
        c->pos.y = tp.pos.y;
        c->pitch = atanf(tp.slope);

        /* stay inside the barriers */
        float lim = tr->half_w + 0.4f - c->hw - 0.1f;
        if (fabsf(tp.d) > lim) {
            float push = (fabsf(tp.d) - lim) * (tp.d > 0 ? -1.0f : 1.0f);
            c->pos.x += tp.rx * push; c->pos.z += tp.rz * push;
            float vout = (c->vx * tp.rx + c->vz * tp.rz) * (tp.d > 0 ? 1.0f : -1.0f);
            if (vout > 0) { float sgn = tp.d > 0 ? 1.0f : -1.0f; c->vx -= tp.rx * vout * sgn; c->vz -= tp.rz * vout * sgn; }
        }

        /* heading follows velocity; crash spin decays back to straight */
        float base = (c->vx * c->vx + c->vz * c->vz > 4.0f) ? atan2f(c->vx, -c->vz) : tp.yaw;
        c->yaw_off += c->spin * dt;
        c->spin *= expf(-2.5f * dt);
        c->yaw_off -= c->yaw_off * (1.0f - expf(-1.5f * dt));
        c->yaw_off = clampf(c->yaw_off, -1.4f, 1.4f);
        c->yaw = base + c->yaw_off;
    }
}

float traffic_draft_factor(const Traffic *t, const Car *c) {
    Vec3 f = yaw_forward(c->yaw);
    float rx = cosf(c->yaw), rz = sinf(c->yaw), best = 0.0f;
    for (int i = 0; i < MAX_TRAFFIC; i++) {
        const TrafficCar *o = &t->cars[i];
        if (!o->active) continue;
        float dx = o->pos.x - c->pos.x, dz = o->pos.z - c->pos.z;
        float ahead = dx * f.x + dz * f.z - o->hl - c->hl;       /* bumper-to-bumper gap */
        float lat = dx * rx + dz * rz;
        if (ahead > 0.5f && ahead < 24.0f && fabsf(lat) < 2.2f) {
            float k = (1.0f - ahead / 24.0f) * (1.0f - fabsf(lat) / 2.2f);
            if (k > best) best = k;
        }
    }
    return best;
}

/* ---- drawing (called from inside testworld's solid / glow passes) ---- */
void traffic_draw_solid(const Traffic *t, Vec3 pp) {
    for (int i = 0; i < MAX_TRAFFIC; i++) {
        const TrafficCar *c = &t->cars[i];
        if (!c->active || fabsf(c->pos.z - pp.z) > FOG_FAR + 20.0f) continue;
        model_load_ypr(c->pos, c->yaw, c->pitch);
        mesh_draw(V[c->type].solid);
    }
}

void traffic_draw_glow(const Traffic *t, Vec3 pp) {
    for (int i = 0; i < MAX_TRAFFIC; i++) {
        const TrafficCar *c = &t->cars[i];
        if (!c->active || fabsf(c->pos.z - pp.z) > FOG_FAR + 20.0f) continue;
        model_load_ypr(c->pos, c->yaw, c->pitch);
        mesh_draw(V[c->type].glow);
    }
}
