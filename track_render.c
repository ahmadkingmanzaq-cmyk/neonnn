/* track.c - parsing, spline sampling, projection, racing line, checkpoints, lap progress.
 * No rendering or PSP headers here, so it can be unit-tested on a PC. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "track.h"
#include "track_default.inc"

#define MAX_PIECES 64
typedef struct { int curve; float len, angle, radius; int tunnel; float jump_h, hill_h; } Piece;

/* ------------------------------------------------------------------ parsing */
static int tokenize(char *line, char *tok[], int max) {
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t' || *p == '\r') { *p = 0; p++; }
        if (!*p || *p == '#') break;
        tok[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r') p++;
    }
    return n;
}

static bool parse_text(const char *text, Track *t, Piece *pc, int *np, float *spacing, int *ncp) {
    *np = 0;
    const char *p = text;
    while (*p) {
        char line[160];
        int n = 0;
        while (*p && *p != '\n' && n < (int)sizeof(line) - 1) line[n++] = *p++;
        line[n] = 0;
        if (*p == '\n') p++;

        char *tok[10];
        int nt = tokenize(line, tok, 10);
        if (nt == 0) continue;

        if (!strcmp(tok[0], "name") && nt > 1) {
            t->name[0] = 0;
            for (int i = 1; i < nt; i++) {
                if (strlen(t->name) + strlen(tok[i]) + 2 < sizeof(t->name)) { strcat(t->name, tok[i]); if (i + 1 < nt) strcat(t->name, " "); }
            }
        } else if (!strcmp(tok[0], "halfwidth") && nt > 1) { t->half_w = (float)atof(tok[1]);
        } else if (!strcmp(tok[0], "spacing") && nt > 1)   { *spacing = (float)atof(tok[1]);
        } else if (!strcmp(tok[0], "checkpoints") && nt > 1) { *ncp = atoi(tok[1]);
        } else if (!strcmp(tok[0], "straight") && nt > 1 && *np < MAX_PIECES) {
            Piece *q = &pc[(*np)++];
            memset(q, 0, sizeof(*q));
            q->len = (float)atof(tok[1]);
            for (int i = 2; i < nt; i++) {
                if (!strcmp(tok[i], "tunnel")) q->tunnel = 1;
                else if (!strcmp(tok[i], "jump") && i + 1 < nt) q->jump_h = (float)atof(tok[++i]);
                else if (!strcmp(tok[i], "hill") && i + 1 < nt) q->hill_h = (float)atof(tok[++i]);
            }
        } else if (!strcmp(tok[0], "curve") && nt > 2 && *np < MAX_PIECES) {
            Piece *q = &pc[(*np)++];
            memset(q, 0, sizeof(*q));
            q->curve = 1;
            q->angle = (float)atof(tok[1]) * PI_F / 180.0f;
            q->radius = (float)atof(tok[2]);
            q->len = fabsf(q->angle) * q->radius;
            for (int i = 3; i < nt; i++) if (!strcmp(tok[i], "tunnel")) q->tunnel = 1;
        }
    }
    return *np >= 4;
}

static float smoothstepf(float u) { u = clampf(u, 0, 1); return u * u * (3.0f - 2.0f * u); }

/* ------------------------------------------------------------------ build */
static bool build_samples(Track *t, const Piece *pc, int np, float spacing, int ncp) {
    float total = 0, total_turn = 0;
    float start[MAX_PIECES];
    for (int k = 0; k < np; k++) { start[k] = total; total += pc[k].len; total_turn += pc[k].curve ? pc[k].angle : 0; }
    if (total < 200.0f) return false;

    int N = (int)floorf(total / spacing / TRACK_SPP + 0.5f) * TRACK_SPP;
    if (N < TRACK_SPP * 8) return false;
    t->n = N; t->length = total; t->ds = total / N;
    t->s = (TrackSample *)calloc(N, sizeof(TrackSample));

    /* integrate the centreline in small sub-steps; spread any yaw error over the lap */
    float target_turn = 2.0f * PI_F * roundf(total_turn / (2.0f * PI_F));
    float corr = (target_turn - total_turn) / total;
    const int SUB = 12;
    float h = t->ds / SUB, x = 0, z = 0, yaw = 0;
    int k = 0;
    for (int i = 0; i < N; i++) {
        t->s[i].p = v3(x, 0, z);
        for (int j = 0; j < SUB; j++) {
            float s = (i * SUB + j + 0.5f) * h;
            while (k < np - 1 && s >= start[k] + pc[k].len) k++;
            float rate = (pc[k].curve ? pc[k].angle / pc[k].len : 0.0f) + corr;
            float ym = yaw + rate * h * 0.5f;
            x += sinf(ym) * h;  z += -cosf(ym) * h;
            yaw += rate * h;
        }
    }
    /* close the loop: remove the residual with a linear shear along the lap */
    for (int i = 0; i < N; i++) { t->s[i].p.x -= x * i / N; t->s[i].p.z -= z * i / N; }

    /* tangents / headings / curvature from the final points */
    for (int i = 0; i < N; i++) {
        const Vec3 *a = &t->s[(i + N - 1) % N].p, *b = &t->s[(i + 1) % N].p;
        float tx = b->x - a->x, tz = b->z - a->z, l = sqrtf(tx * tx + tz * tz);
        t->s[i].tx = tx / l; t->s[i].tz = tz / l;
        t->s[i].rx = -t->s[i].tz; t->s[i].rz = t->s[i].tx;
        t->s[i].yaw = atan2f(t->s[i].tx, -t->s[i].tz);
    }
    for (int i = 0; i < N; i++)
        t->s[i].curv = wrap_pi(t->s[(i + 1) % N].yaw - t->s[(i + N - 1) % N].yaw) / (2.0f * t->ds);

    /* elevation and flags from piece modifiers */
    for (int i = 0; i < N; i++) {
        float s = i * t->ds, y = 0;
        for (int q = 0; q < np; q++) {
            float u = s - start[q];
            if (u < 0 || u >= pc[q].len) continue;
            if (pc[q].tunnel) t->s[i].flags |= TF_TUNNEL;
            if (pc[q].hill_h > 0) {
                float u0 = 0.1f * pc[q].len, u1 = 0.9f * pc[q].len;
                if (u >= u0 && u <= u1) y += pc[q].hill_h * 0.5f * (1.0f - cosf(2.0f * PI_F * (u - u0) / (u1 - u0)));
            }
            if (pc[q].jump_h > 0) {
                float j0 = 0.55f * pc[q].len;
                if (u >= j0 && u < j0 + 30.0f) y += pc[q].jump_h * (u - j0) / 30.0f;                       /* ramp */
                else if (u >= j0 + 30.0f && u < j0 + 46.0f)
                    y += pc[q].jump_h * (1.0f - smoothstepf((u - j0 - 30.0f) / 16.0f));                  /* drop-off */
            }
        }
        t->s[i].p.y = y;
    }
    for (int i = 0; i < N; i++)
        t->s[i].slope = (t->s[(i + 1) % N].p.y - t->s[(i + N - 1) % N].p.y) / (2.0f * t->ds);

    /* racing line: lean toward the inside of the upcoming bend, speed from curvature,
     * then a backward pass so the line brakes in time for slow corners */
    float *off = (float *)calloc(N, sizeof(float)), *tmp = (float *)calloc(N, sizeof(float));
    for (int i = 0; i < N; i++) {
        float sum = 0;
        for (int q = 0; q < 10; q++) sum += t->s[(i + q) % N].curv;
        off[i] = clampf(sum / 10.0f * 500.0f, -5.5f, 5.5f);
    }
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < N; i++) tmp[i] = (off[(i + N - 1) % N] + off[i] + off[(i + 1) % N]) / 3.0f;
        memcpy(off, tmp, N * sizeof(float));
    }
    for (int i = 0; i < N; i++) {
        float kk = fabsf(t->s[i].curv) * 0.8f;
        float v = kk > 1e-4f ? sqrtf(30.0f / kk) : 1000.0f;       /* ~30 m/s^2 of cornering grip */
        t->s[i].rl_speed = fminf(v, 85.0f);
        t->s[i].rl_off = off[i];
    }
    for (int j = 2 * N - 1; j >= 0; j--) {                          /* braking pass, runs twice around */
        int i = j % N, nx = (i + 1) % N;
        float vmax = sqrtf(t->s[nx].rl_speed * t->s[nx].rl_speed + 2.0f * 35.0f * t->ds);
        if (t->s[i].rl_speed > vmax) t->s[i].rl_speed = vmax;
    }
    free(off); free(tmp);

    /* checkpoints, evenly spaced; 0 is the start / finish line at sample 0 */
    if (ncp < 2) ncp = 2;
    if (ncp > MAX_CHECKPOINTS) ncp = MAX_CHECKPOINTS;
    t->ncp = ncp;
    for (int c = 0; c < ncp; c++) t->cp_sample[c] = (int)((long)c * N / ncp);
    return true;
}

bool track_load(Track *t, const char *base_dir, const char *rel_path) {
    memset(t, 0, sizeof(*t));
    t->half_w = 8.0f;
    strcpy(t->name, "TRACK");
    char *buf = NULL;
    char path[256];
    snprintf(path, sizeof(path), "%s%s", base_dir ? base_dir : "", rel_path);
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 65536) {
            buf = (char *)malloc(sz + 1);
            if (buf && fread(buf, 1, sz, f) == (size_t)sz) buf[sz] = 0; else { free(buf); buf = NULL; }
        }
        fclose(f);
    }
    Piece pc[MAX_PIECES];
    int np = 0, ncp = 8;
    float spacing = 4.0f;
    bool ok = buf && parse_text(buf, t, pc, &np, &spacing, &ncp) && build_samples(t, pc, np, spacing, ncp);
    free(buf);
    if (!ok) {                                   /* data file missing or broken: use the built-in copy */
        free(t->s); memset(t, 0, sizeof(*t));
        t->half_w = 8.0f; strcpy(t->name, "TRACK");
        np = 0; ncp = 8; spacing = 4.0f;
        ok = parse_text(DEFAULT_TRK, t, pc, &np, &spacing, &ncp) && build_samples(t, pc, np, spacing, ncp);
    }
    return ok;
}

void track_free(Track *t) {
    if (t->chunks) {
        for (int i = 0; i < t->nchunks; i++) {
            mesh_free(t->chunks[i].solid); mesh_free(t->chunks[i].glow); mesh_free(t->chunks[i].rl);
        }
        free(t->chunks);
    }
    mesh_free(t->gate_cp); mesh_free(t->gate_cp_glow); mesh_free(t->gate_start); mesh_free(t->gate_start_glow);
    free(t->s);
    memset(t, 0, sizeof(*t));
}

/* ------------------------------------------------------------------ queries */
static void fill(const Track *t, int i, float u, TrackPoint *o) {
    const TrackSample *a = &t->s[i], *b = &t->s[(i + 1) % t->n];
    o->seg = i; o->t = u; o->s = (i + u) * t->ds;
    o->pos = v3(lerpf(a->p.x, b->p.x, u), lerpf(a->p.y, b->p.y, u), lerpf(a->p.z, b->p.z, u));
    float tx = lerpf(a->tx, b->tx, u), tz = lerpf(a->tz, b->tz, u), l = sqrtf(tx * tx + tz * tz);
    if (l < 1e-6f) { tx = a->tx; tz = a->tz; l = 1.0f; }
    o->tx = tx / l; o->tz = tz / l; o->rx = -o->tz; o->rz = o->tx;
    o->yaw = atan2f(o->tx, -o->tz);
    o->slope = lerpf(a->slope, b->slope, u);
    o->curv = lerpf(a->curv, b->curv, u);
    o->flags = a->flags;
}

float track_wrap_s(const Track *t, float s) {
    s = fmodf(s, t->length);
    return s < 0 ? s + t->length : s;
}

float track_delta_s(const Track *t, float from, float to) {
    float d = fmodf(to - from, t->length);
    if (d > t->length * 0.5f) d -= t->length;
    else if (d < -t->length * 0.5f) d += t->length;
    return d;
}

void track_at(const Track *t, float s, float d, TrackPoint *o) {
    s = track_wrap_s(t, s);
    float f = s / t->ds;
    int i = (int)f;
    if (i >= t->n) i = t->n - 1;
    fill(t, i, f - i, o);
    o->s = s; o->d = d;
    o->pos.x += o->rx * d;
    o->pos.z += o->rz * d;
}

static float seg_dist2(const Track *t, int i, float x, float z, float *u) {
    const Vec3 *a = &t->s[i].p, *b = &t->s[(i + 1) % t->n].p;
    float abx = b->x - a->x, abz = b->z - a->z, apx = x - a->x, apz = z - a->z;
    float len2 = abx * abx + abz * abz;
    float uu = len2 > 1e-6f ? clampf((apx * abx + apz * abz) / len2, 0.0f, 1.0f) : 0.0f;
    float dx = apx - abx * uu, dz = apz - abz * uu;
    *u = uu;
    return dx * dx + dz * dz;
}

void track_project(const Track *t, float x, float z, int hint, TrackPoint *o) {
    int N = t->n, bi = 0;
    float best = 1e30f, bu = 0, u;
    if (hint >= 0) {                           /* local search around the last known segment */
        for (int k = -4; k <= 10; k++) {
            int i = (((hint + k) % N) + N) % N;
            float d2 = seg_dist2(t, i, x, z, &u);
            if (d2 < best) { best = d2; bi = i; bu = u; }
        }
    }
    if (hint < 0 || best > 400.0f) {           /* lost (teleport / far off the road): full search */
        best = 1e30f;
        for (int i = 0; i < N; i++) {
            float d2 = seg_dist2(t, i, x, z, &u);
            if (d2 < best) { best = d2; bi = i; bu = u; }
        }
    }
    fill(t, bi, bu, o);
    o->d = (x - o->pos.x) * o->rx + (z - o->pos.z) * o->rz;
}

float track_max_curv(const Track *t, float s, float len) {
    int i0 = (int)(track_wrap_s(t, s) / t->ds), cnt = (int)(len / t->ds) + 1;
    float m = 0;
    for (int k = 0; k < cnt; k++) { float c = fabsf(t->s[(i0 + k) % t->n].curv); if (c > m) m = c; }
    return m;
}

/* ------------------------------------------------------------------ lap progress */
void progress_reset(Progress *p, const Track *t, float s) {
    (void)t;
    memset(p, 0, sizeof(*p));
    p->next_cp = 1;
    p->prev_s = s;
}

bool progress_update(Progress *p, const Track *t, float s, float dt) {
    p->lap_time += dt;
    int cpi = p->next_cp % t->ncp;
    float cs = t->cp_sample[cpi] * t->ds;
    float before = track_delta_s(t, cs, p->prev_s), now = track_delta_s(t, cs, s);
    bool lap = false;
    if (before < 0 && now >= 0 && now < 40.0f) {          /* crossed gate `cpi` going forward */
        if (cpi == 0) {                                    /* start/finish, all gates collected */
            p->laps_done++;
            p->last_lap = p->lap_time;
            if (p->best_lap <= 0 || p->last_lap < p->best_lap) p->best_lap = p->last_lap;
            p->lap_time = 0; p->next_cp = 1; lap = true;
        } else {
            p->next_cp++;
        }
    }
    p->prev_s = s;
    return lap;
}
