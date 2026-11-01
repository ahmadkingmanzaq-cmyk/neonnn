/* track_render.c - builds world-space geometry per chunk and draws the streaming window.
 *
 * Per chunk: road surface, lane markings, neon edge strips, barriers (with support walls
 * where the deck is elevated), street lamps, tunnel shell, plus a glow mesh (additive) and a
 * racing-line debug ribbon. Buildings are shared meshes placed per chunk at draw time.
 */
#include <stdlib.h>
#include <pspgu.h>
#include <pspgum.h>
#include "track.h"
#include "config.h"
#include "mesh.h"
#include "renderer.h"

#define NUM_BLD 8
static Mesh *s_bld[NUM_BLD];
static const float BLD_H[NUM_BLD] = { 45, 70, 110, 60, 90, 130, 50, 80 };
static const float BLD_W[NUM_BLD] = { 14, 18,  22, 16, 20,  14, 22, 18 };

static Cross cross_at(const Track *t, int i) {
    const TrackSample *s = &t->s[((i % t->n) + t->n) % t->n];
    Cross c;
    c.c = s->p; c.r = v3(s->rx, 0, s->rz); c.t = v3(s->tx, 0, s->tz);
    return c;
}

/* ---- shared buildings (built facing +X; the right-hand side is drawn mirrored) ---- */
static void build_buildings(void) {
    const uint32_t neon[3] = { COL_CYAN, COL_MAGENTA, COL_YELLOW };
    const float d = SEG_LEN * 0.9f;
    for (int i = 0; i < NUM_BLD; i++) {
        float w = BLD_W[i], h = BLD_H[i];
        Mesh *m = mesh_create(160, 320);
        mesh_box(m, v3(0, h / 2, 0), v3(w / 2, h / 2, d / 2), RGB(14, 14, 34));
        int bands = (int)(h / 10.0f);
        for (int b = 0; b < bands; b++) {
            float y0 = 6.0f + b * 10.0f;
            uint32_t col = neon[(i + (b % 3 == 2 ? 1 : 0)) % 3];
            Vec3 p[4] = { v3(w/2+0.04f, y0, -d/2+1), v3(w/2+0.04f, y0, d/2-1),
                          v3(w/2+0.04f, y0+0.5f, d/2-1), v3(w/2+0.04f, y0+0.5f, -d/2+1) };
            uint32_t c[4] = { col, col, col, col };
            mesh_quad(m, p, c);
        }
        uint32_t sc = neon[(i + 1) % 3];
        Vec3 sp[4] = { v3(w/2+0.06f, 3, d/2-2.6f), v3(w/2+0.06f, 3, d/2-1.6f),
                       v3(w/2+0.06f, h-3, d/2-1.6f), v3(w/2+0.06f, h-3, d/2-2.6f) };
        uint32_t scc[4] = { sc, sc, sc, sc };
        mesh_quad(m, sp, scc);
        mesh_finalize(m);
        s_bld[i] = m;
    }
}

/* ---- checkpoint / start gates (local space, forward = -Z) ---- */
static void build_gate(Track *t, uint32_t bar, uint32_t post, Mesh **solid, Mesh **glow) {
    float W = t->half_w + 1.2f;
    Mesh *m = mesh_create(128, 256), *g = mesh_create(32, 64);
    mesh_box(m, v3(-W, 4.0f, 0), v3(0.25f, 4.0f, 0.25f), post);
    mesh_box(m, v3( W, 4.0f, 0), v3(0.25f, 4.0f, 0.25f), post);
    mesh_box(m, v3(0, 7.9f, 0),  v3(W + 0.25f, 0.30f, 0.30f), bar);
    mesh_finalize(m);
    mesh_glow_fan(g, v3(0, 7.9f, 0), v3(W, 0, 0), v3(0, 2.2f, 0), bar, 150);
    mesh_finalize(g);
    *solid = m; *glow = g;
}

/* ---- one chunk ---- */
static void build_chunk(Track *t, int ci) {
    const float W = t->half_w;
    const uint32_t EDGE = RGB(10, 10, 28), CENTRE = RGB(26, 28, 52), BARRIER = RGB(22, 22, 48);
    Mesh *m = mesh_create(1600, 2800), *g = mesh_create(600, 1000), *rl = mesh_create(100, 200);
    Chunk *ch = &t->chunks[ci];
    int mid_i = ci * TRACK_SPP + TRACK_SPP / 2;
    const TrackSample *ms = &t->s[mid_i % t->n];
    ch->mid = ms->p; ch->mid_yaw = ms->yaw; ch->mid_curv = ms->curv;
    ch->flags = ms->flags;

    for (int k = 0; k < TRACK_SPP; k++) {
        int i = ci * TRACK_SPP + k;
        Cross a = cross_at(t, i), b = cross_at(t, i + 1);
        const TrackSample *sa = &t->s[i % t->n];
        bool tunnel = (sa->flags & TF_TUNNEL) != 0;
        bool raised = (a.c.y > 0.3f || b.c.y > 0.3f);

        /* road surface: lighter wet-looking centre */
        mesh_flat(m, &a, &b, -W, 0, 0, EDGE, CENTRE);
        mesh_flat(m, &a, &b, 0,  W, 0, CENTRE, EDGE);
        /* dashes every other sample pair: centre (yellow) + lane lines (pale blue) */
        if ((k & 1) == 0) {
            mesh_flat(m, &a, &b, -0.12f, 0.12f, 0.03f, RGB(210, 185, 60), RGB(210, 185, 60));
            mesh_flat(m, &a, &b, -W/2 - 0.1f, -W/2 + 0.1f, 0.03f, RGB(120, 140, 210), RGB(120, 140, 210));
            mesh_flat(m, &a, &b,  W/2 - 0.1f,  W/2 + 0.1f, 0.03f, RGB(120, 140, 210), RGB(120, 140, 210));
        }
        /* neon edge strips and barriers */
        mesh_prism(m, &a, &b, -W, 0.18f, 0, 0.2f, COL_CYAN, false);
        mesh_prism(m, &a, &b,  W, 0.18f, 0, 0.2f, COL_MAGENTA, false);
        mesh_prism(m, &a, &b, -(W + 0.9f), 0.5f, 0, 1.0f, BARRIER, raised);
        mesh_prism(m, &a, &b,  (W + 0.9f), 0.5f, 0, 1.0f, BARRIER, raised);
        /* edge glow bleeding onto the road */
        mesh_flat(g, &a, &b, -W, -W + 4, 0.25f, RGBA(0,229,255,110), RGBA(0,229,255,0));
        mesh_flat(g, &a, &b,  W - 4, W, 0.25f, RGBA(255,0,200,0), RGBA(255,0,200,110));

        if (tunnel) {
            const uint32_t WALL = RGB(18, 18, 40), ROOF = RGB(10, 10, 26);
            mesh_wall(m, &a, &b, -(W + 0.4f), 0, 9.0f, WALL);
            mesh_wall(m, &a, &b,  (W + 0.4f), 0, 9.0f, WALL);
            mesh_flat(m, &a, &b, -(W + 0.4f), W + 0.4f, 9.0f, ROOF, ROOF);
            mesh_flat(m, &a, &b, -4.15f, -3.85f, 8.95f, COL_CYAN, COL_CYAN);       /* ceiling light strips */
            mesh_flat(m, &a, &b,  3.85f,  4.15f, 8.95f, COL_MAGENTA, COL_MAGENTA);
            mesh_wall(m, &a, &b, -(W + 0.38f), 1.0f, 1.25f, COL_CYAN);              /* wall accent lines */
            mesh_wall(m, &a, &b,  (W + 0.38f), 1.0f, 1.25f, COL_MAGENTA);
            mesh_flat(g, &a, &b, -5.5f, -4.0f, 8.85f, RGBA(0,229,255,0),   RGBA(0,229,255,110));
            mesh_flat(g, &a, &b, -4.0f, -2.5f, 8.85f, RGBA(0,229,255,110), RGBA(0,229,255,0));
            mesh_flat(g, &a, &b,  2.5f,  4.0f, 8.85f, RGBA(255,0,200,0),   RGBA(255,0,200,110));
            mesh_flat(g, &a, &b,  4.0f,  5.5f, 8.85f, RGBA(255,0,200,110), RGBA(255,0,200,0));
            mesh_flat(g, &a, &b, -4.5f, -3.5f, 0.28f, RGBA(0,229,255,0), RGBA(0,229,255,45));  /* floor spill */
            mesh_flat(g, &a, &b,  3.5f,  4.5f, 0.28f, RGBA(255,0,200,45), RGBA(255,0,200,0));
        }

        /* street lamps once per chunk, open road only */
        if (k == TRACK_SPP / 2 && !tunnel) {
            for (int sd = 0; sd < 2; sd++) {
                float sx = sd ? 1.0f : -1.0f;
                uint32_t neon = sd ? COL_MAGENTA : COL_CYAN;
                float px = sx * (W + 1.6f);
                Vec3 pole = v3(a.c.x + a.r.x * px, a.c.y + 3.5f, a.c.z + a.r.z * px);
                mesh_box(m, pole, v3(0.10f, 3.5f, 0.10f), RGB(30, 30, 60));
                Vec3 head = v3(a.c.x + a.r.x * (px - sx * 1.2f), a.c.y + 7.1f, a.c.z + a.r.z * (px - sx * 1.2f));
                mesh_obox(m, head, a.r, a.t, v3(1.2f, 0.08f, 0.2f), neon);
                mesh_glow_fan(g, head, v3(0, 2.4f, 0), v3(a.r.x * 2.4f, 0, a.r.z * 2.4f), neon, 150);
                Vec3 pool = v3(a.c.x + a.r.x * sx * (W - 2.5f), a.c.y + 0.30f, a.c.z + a.r.z * sx * (W - 2.5f));
                mesh_glow_fan(g, pool, v3(a.r.x * 4.5f, 0, a.r.z * 4.5f), v3(a.t.x * 9.0f, 0, a.t.z * 9.0f), neon, 90);
            }
        }

        /* start / finish checker on the first chunk */
        if (ci == 0 && k < 2) {
            for (int cell = 0; cell < 8; cell++) {
                float x0 = -W + cell * (2 * W / 8), x1 = x0 + 2 * W / 8;
                uint32_t col = ((cell + k) & 1) ? RGB(235, 235, 245) : RGB(15, 15, 25);
                mesh_flat(m, &a, &b, x0, x1, 0.05f, col, col);
            }
        }

        /* racing-line debug ribbon: green = fast, red = slow (drawn only with the overlay on) */
        {
            const TrackSample *sb = &t->s[(i + 1) % t->n];
            Cross ra = a, rb = b;
            ra.c.x += a.r.x * sa->rl_off; ra.c.z += a.r.z * sa->rl_off;
            rb.c.x += b.r.x * sb->rl_off; rb.c.z += b.r.z * sb->rl_off;
            float f = clampf(sa->rl_speed / 85.0f, 0.0f, 1.0f);
            uint32_t col = RGB((int)(255 * (1 - f)), (int)(255 * f), 60);
            mesh_flat(rl, &ra, &rb, -0.3f, 0.3f, 0.45f, col, col);
        }
    }
    ch->solid = mesh_trim(m);
    ch->glow  = mesh_trim(g);
    ch->rl    = mesh_trim(rl);
    ch->bl = (int)(hash32((uint32_t)(ci * 2 + 1)) % NUM_BLD);
    ch->br = (int)(hash32((uint32_t)(ci * 2 + 2)) % NUM_BLD);
}

void track_build_meshes(Track *t) {
    build_buildings();
    build_gate(t, COL_CYAN,   RGB(25, 25, 50), &t->gate_cp,    &t->gate_cp_glow);
    build_gate(t, COL_YELLOW, RGB(40, 40, 60), &t->gate_start, &t->gate_start_glow);
    t->nchunks = t->n / TRACK_SPP;
    t->chunks = (Chunk *)calloc(t->nchunks, sizeof(Chunk));
    for (int i = 0; i < t->nchunks; i++) build_chunk(t, i);
}

/* ---- drawing: only chunks inside the window around the player ---- */
static void window(const Track *t, int centre, int dir, int *first, int *count) {
    int behind = SEGS_BEHIND, ahead = SEGS_AHEAD;
    if (dir < 0) { behind = SEGS_AHEAD; ahead = SEGS_BEHIND; }
    *first = centre - behind;
    *count = behind + ahead + 1;
    if (*count > t->nchunks) *count = t->nchunks;
}

static int wrapc(const Track *t, int i) { return ((i % t->nchunks) + t->nchunks) % t->nchunks; }

static void draw_gate(const Track *t, int j, bool glow) {
    const TrackSample *s = &t->s[t->cp_sample[j]];
    model_load_ypr(s->p, s->yaw, atanf(s->slope));
    mesh_draw(glow ? (j == 0 ? t->gate_start_glow : t->gate_cp_glow) : (j == 0 ? t->gate_start : t->gate_cp));
}

void track_draw_solid(const Track *t, int centre, int dir, bool show_rl) {
    int first, count;
    window(t, centre, dir, &first, &count);
    for (int n = 0; n < count; n++) {
        int ci = wrapc(t, first + n);
        const Chunk *c = &t->chunks[ci];
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        mesh_draw(c->solid);
        if (show_rl) mesh_draw(c->rl);

        if (!(c->flags & TF_TUNNEL)) {
            /* skip the inside-of-the-bend building where the footprint would overlap its neighbours */
            bool skip_l = c->mid_curv < -0.008f, skip_r = c->mid_curv > 0.008f;
            if (!skip_l) {
                float xl = t->half_w + 3.0f + BLD_W[c->bl] * 0.5f;
                model_load_yaw(v3(c->mid.x, 0, c->mid.z), c->mid_yaw);
                ScePspFVector3 tr = { -xl, 0, 0 };
                sceGumTranslate(&tr);
                mesh_draw(s_bld[c->bl]);
            }
            if (!skip_r) {
                float xr = t->half_w + 3.0f + BLD_W[c->br] * 0.5f;
                model_load_yaw(v3(c->mid.x, 0, c->mid.z), c->mid_yaw);
                ScePspFVector3 tr = { xr, 0, 0 }, mir = { -1.0f, 1.0f, 1.0f };
                sceGumTranslate(&tr);
                sceGumScale(&mir);                        /* mirror so the lit side faces the road */
                mesh_draw(s_bld[c->br]);
            }
        }
        for (int j = 0; j < t->ncp; j++)
            if (t->cp_sample[j] / TRACK_SPP == ci) draw_gate(t, j, false);
    }
}

void track_draw_glow(const Track *t, int centre, int dir) {
    int first, count;
    window(t, centre, dir, &first, &count);
    for (int n = 0; n < count; n++) {
        int ci = wrapc(t, first + n);
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        mesh_draw(t->chunks[ci].glow);
        for (int j = 0; j < t->ncp; j++)
            if (t->cp_sample[j] / TRACK_SPP == ci) draw_gate(t, j, true);
    }
}
