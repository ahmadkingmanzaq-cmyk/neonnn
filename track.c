/* mesh.h - vertex-coloured indexed meshes built in code (no asset files).
 * Vertex layout matches GU_COLOR_8888 | GU_VERTEX_32BITF: colour FIRST, then xyz. */
#ifndef MESH_H
#define MESH_H
#include <stdint.h>
#include <stdbool.h>
#include "mathutil.h"

typedef struct { uint32_t color; float x, y, z; } Vertex;

typedef struct {
    Vertex   *v;
    uint16_t *idx;
    int nv, ni, max_v, max_i;
} Mesh;

/* A road cross-section: centre point (incl. height), unit right vector and unit forward
 * vector (both horizontal). Ribbon helpers extrude geometry between two of these. */
typedef struct { Vec3 c, r, t; } Cross;

extern int g_tri_count;                 /* triangles submitted this frame (debug) */

Mesh    *mesh_create(int max_verts, int max_indices);
void     mesh_free(Mesh *m);
void     mesh_finalize(Mesh *m);        /* flush CPU cache so the GE sees the data */
void     mesh_draw(const Mesh *m);      /* uses the current GU_MODEL matrix */

/* Load GU_MODEL = world matrix for an object at p with heading yaw (forward = -Z local). */
void     model_load_yaw(Vec3 p, float yaw);
/* Same with pitch (radians, nose up = positive) for slopes and jumps. */
void     model_load_ypr(Vec3 p, float yaw, float pitch);

uint32_t color_scale(uint32_t c, float k);   /* scale RGB, keep alpha */

/* Box without a bottom face. Faces get baked fake lighting (top brightest). */
void mesh_box(Mesh *m, Vec3 center, Vec3 half, uint32_t color);
/* Horizontal strip between lateral offsets x0..x1 at height yoff (colour c0 at x0, c1 at x1). */
void mesh_flat(Mesh *m, const Cross *a, const Cross *b, float x0, float x1, float yoff, uint32_t c0, uint32_t c1);
/* Vertical wall at lateral offset x spanning heights y0..y1 (relative to the road). */
void mesh_wall(Mesh *m, const Cross *a, const Cross *b, float x, float y0, float y1, uint32_t col);
/* Raised strip (kerb/barrier): top + both sides. to_ground extends the sides down to y=0. */
void mesh_prism(Mesh *m, const Cross *a, const Cross *b, float xc, float hw, float y0, float y1,
                uint32_t col, bool to_ground);
/* Box oriented in the XZ plane (r = right, f = forward, unit vectors). */
void mesh_obox(Mesh *m, Vec3 c, Vec3 r, Vec3 f, Vec3 half, uint32_t col);
/* Copy to an exactly-sized mesh and free the oversized builder mesh. */
Mesh *mesh_trim(Mesh *m);

/* Free quad with per-corner colours (p[0..3] in order around the quad). */
void mesh_quad(Mesh *m, const Vec3 p[4], const uint32_t c[4]);
/* Additive glow: centre vertex has `alpha`, the 4 corners fade to 0.
 * u and v are half-extent vectors spanning the glow plane. */
void mesh_glow_fan(Mesh *m, Vec3 center, Vec3 u, Vec3 v, uint32_t rgb, uint8_t alpha);

#endif
