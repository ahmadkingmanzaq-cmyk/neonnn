#include <malloc.h>
#include <string.h>
#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include "mesh.h"

int g_tri_count = 0;

Mesh *mesh_create(int max_verts, int max_indices) {
    Mesh *m = (Mesh *)calloc(1, sizeof(Mesh));
    m->v   = (Vertex *)memalign(16, max_verts * sizeof(Vertex));
    m->idx = (uint16_t *)memalign(16, max_indices * sizeof(uint16_t));
    m->max_v = max_verts;
    m->max_i = max_indices;
    return m;
}

void mesh_free(Mesh *m) {
    if (!m) return;
    free(m->v); free(m->idx); free(m);
}

void mesh_finalize(Mesh *m) {
    /* The GE reads straight from RAM, bypassing the CPU data cache. */
    sceKernelDcacheWritebackRange(m->v,   m->nv * sizeof(Vertex));
    sceKernelDcacheWritebackRange(m->idx, m->ni * sizeof(uint16_t));
}

void mesh_draw(const Mesh *m) {
    if (m->ni == 0) return;
    sceGumDrawArray(GU_TRIANGLES,
                    GU_INDEX_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    m->ni, m->idx, m->v);
    g_tri_count += m->ni / 3;
}

uint32_t color_scale(uint32_t c, float k) {
    int r = (int)((c & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)(((c >> 16) & 255) * k);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (c & 0xFF000000u) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

static int add_vert(Mesh *m, Vec3 p, uint32_t c) {
    if (m->nv >= m->max_v) return -1;
    Vertex *v = &m->v[m->nv];
    v->color = c; v->x = p.x; v->y = p.y; v->z = p.z;
    return m->nv++;
}

static void add_tri(Mesh *m, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || m->ni + 3 > m->max_i) return;
    m->idx[m->ni++] = (uint16_t)a;
    m->idx[m->ni++] = (uint16_t)b;
    m->idx[m->ni++] = (uint16_t)c;
}

void mesh_quad(Mesh *m, const Vec3 p[4], const uint32_t c[4]) {
    int i0 = add_vert(m, p[0], c[0]), i1 = add_vert(m, p[1], c[1]);
    int i2 = add_vert(m, p[2], c[2]), i3 = add_vert(m, p[3], c[3]);
    add_tri(m, i0, i1, i2);
    add_tri(m, i0, i2, i3);
}

static void flat_quad(Mesh *m, Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t col) {
    Vec3 p[4] = { a, b, c, d };
    uint32_t cc[4] = { col, col, col, col };
    mesh_quad(m, p, cc);
}

void mesh_box(Mesh *m, Vec3 c, Vec3 h, uint32_t col) {
    float x0 = c.x - h.x, x1 = c.x + h.x;
    float y0 = c.y - h.y, y1 = c.y + h.y;
    float z0 = c.z - h.z, z1 = c.z + h.z;
    /* top */
    flat_quad(m, v3(x0,y1,z0), v3(x1,y1,z0), v3(x1,y1,z1), v3(x0,y1,z1), color_scale(col, 1.00f));
    /* front / back (-Z / +Z) */
    flat_quad(m, v3(x0,y0,z0), v3(x1,y0,z0), v3(x1,y1,z0), v3(x0,y1,z0), color_scale(col, 0.75f));
    flat_quad(m, v3(x0,y0,z1), v3(x1,y0,z1), v3(x1,y1,z1), v3(x0,y1,z1), color_scale(col, 0.75f));
    /* left / right (-X / +X) */
    flat_quad(m, v3(x0,y0,z0), v3(x0,y0,z1), v3(x0,y1,z1), v3(x0,y1,z0), color_scale(col, 0.60f));
    flat_quad(m, v3(x1,y0,z0), v3(x1,y0,z1), v3(x1,y1,z1), v3(x1,y1,z0), color_scale(col, 0.60f));
    /* no bottom face: never visible, saves triangles */
}

void mesh_glow_fan(Mesh *m, Vec3 c, Vec3 u, Vec3 v, uint32_t rgb, uint8_t alpha) {
    uint32_t cen = (rgb & 0x00FFFFFFu) | ((uint32_t)alpha << 24);
    uint32_t edge = (rgb & 0x00FFFFFFu);             /* alpha 0 */
    int ic = add_vert(m, c, cen);
    int a = add_vert(m, v3(c.x-u.x-v.x, c.y-u.y-v.y, c.z-u.z-v.z), edge);
    int b = add_vert(m, v3(c.x+u.x-v.x, c.y+u.y-v.y, c.z+u.z-v.z), edge);
    int d = add_vert(m, v3(c.x+u.x+v.x, c.y+u.y+v.y, c.z+u.z+v.z), edge);
    int e = add_vert(m, v3(c.x-u.x+v.x, c.y-u.y+v.y, c.z-u.z+v.z), edge);
    add_tri(m, ic, a, b); add_tri(m, ic, b, d);
    add_tri(m, ic, d, e); add_tri(m, ic, e, a);
}

void model_load_ypr(Vec3 p, float yaw, float pitch) {
    /* columns = local axes in world space: +X -> right, +Y -> up, +Z -> rear */
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    Vec3 right = v3(cy, 0, sy);
    Vec3 rear  = v3(-sy * cp, -sp, cy * cp);
    Vec3 up    = v3(rear.y * right.z - rear.z * right.y,      /* up = rear x right */
                    rear.z * right.x - rear.x * right.z,
                    rear.x * right.y - rear.y * right.x);
    ScePspFMatrix4 m;
    m.x.x = right.x; m.x.y = right.y; m.x.z = right.z; m.x.w = 0;
    m.y.x = up.x;    m.y.y = up.y;    m.y.z = up.z;    m.y.w = 0;
    m.z.x = rear.x;  m.z.y = rear.y;  m.z.z = rear.z;  m.z.w = 0;
    m.w.x = p.x;     m.w.y = p.y;     m.w.z = p.z;     m.w.w = 1;
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadMatrix(&m);
}

void model_load_yaw(Vec3 p, float yaw) { model_load_ypr(p, yaw, 0.0f); }

/* ---- road ribbon helpers ---- */
static Vec3 xpt(const Cross *a, float x, float y) {
    return v3(a->c.x + a->r.x * x, a->c.y + y, a->c.z + a->r.z * x);
}

void mesh_flat(Mesh *m, const Cross *a, const Cross *b, float x0, float x1, float yoff, uint32_t c0, uint32_t c1) {
    Vec3 p[4] = { xpt(a, x0, yoff), xpt(a, x1, yoff), xpt(b, x1, yoff), xpt(b, x0, yoff) };
    uint32_t c[4] = { c0, c1, c1, c0 };
    mesh_quad(m, p, c);
}

static void wall_ex(Mesh *m, const Cross *a, const Cross *b, float x,
                    float y0a, float y1a, float y0b, float y1b, uint32_t col) {
    Vec3 p[4] = { xpt(a, x, y0a), xpt(a, x, y1a), xpt(b, x, y1b), xpt(b, x, y0b) };
    uint32_t c[4] = { col, col, col, col };
    mesh_quad(m, p, c);
}

void mesh_wall(Mesh *m, const Cross *a, const Cross *b, float x, float y0, float y1, uint32_t col) {
    wall_ex(m, a, b, x, y0, y1, y0, y1, col);
}

void mesh_prism(Mesh *m, const Cross *a, const Cross *b, float xc, float hw, float y0, float y1,
                uint32_t col, bool to_ground) {
    mesh_flat(m, a, b, xc - hw, xc + hw, y1, col, col);
    uint32_t sc = color_scale(col, 0.65f);
    float y0a = to_ground ? -a->c.y : y0, y0b = to_ground ? -b->c.y : y0;
    wall_ex(m, a, b, xc - hw, y0a, y1, y0b, y1, sc);
    wall_ex(m, a, b, xc + hw, y0a, y1, y0b, y1, sc);
}

void mesh_obox(Mesh *m, Vec3 c, Vec3 r, Vec3 f, Vec3 h, uint32_t col) {
#define OC(sx, sy, sz) v3(c.x + r.x * h.x * (sx) + f.x * h.z * (sz), c.y + h.y * (sy), \
                          c.z + r.z * h.x * (sx) + f.z * h.z * (sz))
    flat_quad(m, OC(-1,1,-1), OC(1,1,-1), OC(1,1,1), OC(-1,1,1), color_scale(col, 1.00f));
    flat_quad(m, OC(-1,-1,-1), OC(1,-1,-1), OC(1,1,-1), OC(-1,1,-1), color_scale(col, 0.75f));
    flat_quad(m, OC(-1,-1,1), OC(1,-1,1), OC(1,1,1), OC(-1,1,1), color_scale(col, 0.75f));
    flat_quad(m, OC(-1,-1,-1), OC(-1,-1,1), OC(-1,1,1), OC(-1,1,-1), color_scale(col, 0.60f));
    flat_quad(m, OC(1,-1,-1), OC(1,-1,1), OC(1,1,1), OC(1,1,-1), color_scale(col, 0.60f));
#undef OC
}

Mesh *mesh_trim(Mesh *m) {
    Mesh *t = mesh_create(m->nv > 0 ? m->nv : 1, m->ni > 0 ? m->ni : 1);
    memcpy(t->v, m->v, m->nv * sizeof(Vertex));
    memcpy(t->idx, m->idx, m->ni * sizeof(uint16_t));
    t->nv = m->nv; t->ni = m->ni;
    mesh_free(m);
    mesh_finalize(t);
    return t;
}
