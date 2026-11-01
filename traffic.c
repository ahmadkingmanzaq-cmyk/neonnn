/* track.h - spline-sampled closed tracks defined by piece lists in data files.
 *
 * A track is a closed loop of N samples spaced ~ds metres apart (centreline position,
 * heading, curvature, slope, racing-line offset and target speed). Pieces in the .trk
 * file (straight / curve, with tunnel / jump / hill modifiers) are integrated into that
 * loop at load time. Rendering geometry is built per CHUNK of TRACK_SPP samples, and
 * only chunks near the player are drawn (segment culling).
 *
 * Lateral convention: d > 0 is to the RIGHT of the direction of travel.
 */
#ifndef TRACK_H
#define TRACK_H
#include <stdbool.h>
#include "mathutil.h"
#include "mesh.h"

#define TF_TUNNEL       1
#define TRACK_SPP       6        /* samples per render chunk (~24 m) */
#define MAX_CHECKPOINTS 16

typedef struct {
    Vec3  p;                     /* centreline position (y = road height) */
    float tx, tz, rx, rz;        /* unit tangent (forward) and right vector in XZ */
    float yaw, curv, slope;      /* curv > 0 = turning right (1/m); slope = dy/ds */
    float rl_off, rl_speed;      /* racing line: lateral offset (m, + right) and target speed (m/s) */
    unsigned char flags;
} TrackSample;

typedef struct {                 /* result of projecting / sampling a point on the track */
    float s, d;                  /* arclength along the track, signed lateral offset */
    int   seg;  float t;         /* sample index and fraction toward the next */
    Vec3  pos;                   /* centreline point */
    float tx, tz, rx, rz, yaw, slope, curv;
    unsigned char flags;
} TrackPoint;

typedef struct {
    Mesh *solid, *glow, *rl;     /* world-space meshes for this chunk */
    unsigned char flags;
    float mid_curv, mid_yaw;
    Vec3  mid;
    int   bl, br;                /* building variants (left / right) */
} Chunk;

typedef struct {
    char  name[40];
    float half_w, ds, length;
    int   n;  TrackSample *s;
    int   nchunks;  Chunk *chunks;
    int   ncp;  int cp_sample[MAX_CHECKPOINTS];     /* cp 0 = start / finish line */
    Mesh *gate_cp, *gate_cp_glow, *gate_start, *gate_start_glow;
} Track;

typedef struct {                 /* lap / checkpoint progress for one car */
    int   laps_done, next_cp;
    float prev_s, lap_time, last_lap, best_lap;
} Progress;

/* ---- data (track.c) ---- */
/* Loads base_dir + rel_path; falls back to the embedded copy if the file is missing. */
bool  track_load(Track *t, const char *base_dir, const char *rel_path);
void  track_free(Track *t);
void  track_project(const Track *t, float x, float z, int hint, TrackPoint *out);  /* hint<0: full search */
void  track_at(const Track *t, float s, float d, TrackPoint *out);                /* out->pos includes d */
float track_wrap_s(const Track *t, float s);
float track_delta_s(const Track *t, float from, float to);       /* signed shortest, (-L/2, L/2] */
float track_max_curv(const Track *t, float s, float len);        /* max |curvature| over [s, s+len] */

void  progress_reset(Progress *p, const Track *t, float s);
bool  progress_update(Progress *p, const Track *t, float s, float dt);   /* true on lap completed */

/* ---- rendering (track_render.c) ---- */
void  track_build_meshes(Track *t);
/* centre_chunk: chunk the camera/player is in. dir: +1 looking forward along the track, -1 backward. */
void  track_draw_solid(const Track *t, int centre_chunk, int dir, bool show_racing_line);
void  track_draw_glow(const Track *t, int centre_chunk, int dir);

#endif
