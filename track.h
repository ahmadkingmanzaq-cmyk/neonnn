/* renderer.c
 *
 * VRAM MAP (2 MB total = 0x200000), 16-bit 5650 buffers, 512-pixel stride:
 *   0x000000  draw buffer    512*272*2 = 0x44000 (272 KB)
 *   0x044000  display buffer 0x44000
 *   0x088000  depth buffer   0x44000 (PSP depth is always 16-bit)
 *   0x0CC000  free -> textures (~1.2 MB)
 * Using 5650 instead of 8888 halves colour-buffer cost; banding is hidden by
 * the dark, fogged palette.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspge.h>
#include "renderer.h"
#include "config.h"
#include "mesh.h"

#define FB_SIZE        (BUF_W * SCREEN_H * 2)
#define VRAM_DRAW      0
#define VRAM_DISP      (FB_SIZE)
#define VRAM_DEPTH     (FB_SIZE * 2)
#define VRAM_TEX_START (FB_SIZE * 3)
#define VRAM_TOTAL     0x200000

/* GU display list: must be 16-byte aligned. 1 MB is plenty for our budget. */
static unsigned int __attribute__((aligned(16))) s_list[262144];
static size_t s_vram_top = VRAM_TEX_START;

int renderer_init(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, s_list);

    sceGuDrawBuffer(GU_PSM_5650, (void *)VRAM_DRAW, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)VRAM_DISP, BUF_W);
    sceGuDepthBuffer((void *)VRAM_DEPTH, BUF_W);

    sceGuOffset(2048 - (SCREEN_W / 2), 2048 - (SCREEN_H / 2));
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);

    /* PSP depth is "reversed": range 65535..0 with GEQUAL test. */
    sceGuDepthRange(65535, 0);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);

    sceGuShadeModel(GU_SMOOTH);
    /* Culling OFF in stage 1: all meshes are procedural with mixed winding.
     * Turn it on later once winding is audited (saves fill-rate). */
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_LIGHTING);

    sceGuFog(FOG_NEAR, FOG_FAR, FOG_COLOR);
    sceGuEnable(GU_FOG);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    return 1;
}

void renderer_begin_frame(const Camera *cam) {
    sceGuStart(GU_DIRECT, s_list);
    g_tri_count = 0;

    sceGuClearColor(FOG_COLOR);
    sceGuClearDepth(0);                               /* reversed depth: 0 = far */
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(cam->fov, (float)SCREEN_W / (float)SCREEN_H, CAM_NEAR, CAM_FAR);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    ScePspFVector3 eye = { cam->eye.x, cam->eye.y, cam->eye.z };
    ScePspFVector3 tgt = { cam->target.x, cam->target.y, cam->target.z };
    ScePspFVector3 up  = { 0.0f, 1.0f, 0.0f };
    sceGumLookAt(&eye, &tgt, &up);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void renderer_end_frame(int vblanks) {
    sceGuFinish();
    sceGuSync(0, 0);
    for (int i = 0; i < vblanks; i++) sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void renderer_glow_begin(void) {
    sceGuDepthMask(GU_TRUE);                          /* TRUE = depth writes OFF */
    sceGuEnable(GU_BLEND);
    /* result = dst + src.rgb * src.alpha  (fixed dest factor = 1.0) */
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
}

void renderer_glow_end(void) {
    sceGuDisable(GU_BLEND);
    sceGuDepthMask(GU_FALSE);
}

size_t renderer_vram_alloc(size_t bytes) {
    bytes = (bytes + 15) & ~(size_t)15;
    if (s_vram_top + bytes > VRAM_TOTAL) return (size_t)-1;
    size_t off = s_vram_top;
    s_vram_top += bytes;
    return off;
}

void *renderer_vram_ptr(size_t offset) {
    return (void *)((unsigned char *)sceGeEdramGetAddr() + offset);
}

size_t renderer_vram_free(void) { return VRAM_TOTAL - s_vram_top; }
