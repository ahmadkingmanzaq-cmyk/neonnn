#include <stdio.h>
#include <string.h>
#include <pspgu.h>
#include "debug.h"
#include "config.h"
#include "mathutil.h"

typedef struct { uint32_t color; float x, y, z; } V2;

/* 3x5 digit font, one byte per row, low 3 bits used */
static const unsigned char FONT[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
};

static void rect(float x0, float y0, float x1, float y1, uint32_t col) {
    V2 *v = (V2 *)sceGuGetMemory(2 * sizeof(V2));   /* lives in the display list */
    v[0].color = col; v[0].x = x0; v[0].y = y0; v[0].z = 0;
    v[1].color = col; v[1].x = x1; v[1].y = y1; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

static void number(int n, float x, float y, uint32_t col) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n < 0 ? 0 : n);
    const float S = 3.0f;
    for (const char *p = buf; *p; p++) {
        int d = *p - '0';
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 3; c++)
                if (FONT[d][r] & (4 >> c))
                    rect(x + c * S, y + r * S, x + (c + 1) * S, y + (r + 1) * S, col);
        x += 4 * S;
    }
}

void debug_draw(int fps, int speed_kmh, int tris, int nearmisses, int car_no, int lap, int lap_seconds) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_FOG);

    rect(6, 6, 76, 134, RGBA(0, 0, 0, 255));
    uint32_t g = RGB(80, 255, 80), c = COL_CYAN, y = COL_YELLOW, m = COL_MAGENTA, w = RGB(255, 255, 255);
    rect(10, 10, 14, 14, g); number(fps,        20, 8,  g);
    rect(10, 28, 14, 32, c); number(speed_kmh,  20, 26, c);
    rect(10, 46, 14, 50, y); number(tris,       20, 44, y);
    rect(10, 64, 14, 68, m); number(nearmisses, 20, 62, m);
    rect(10, 82, 14, 86, w); number(car_no,     20, 80, w);
    uint32_t o = RGB(255, 150, 40), l = RGB(120, 200, 120);
    rect(10, 100, 14, 104, o); number(lap,         20, 98,  o);
    rect(10, 118, 14, 122, l); number(lap_seconds, 20, 116, l);

    sceGuEnable(GU_FOG);
    sceGuEnable(GU_DEPTH_TEST);
}

void debug_bars(float nitro01, int nitro_on, float charge01, int tier, float draft01) {
    nitro01  = clampf(nitro01, 0, 1);
    charge01 = clampf(charge01, 0, 1);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_FOG);

    rect(8, 246, 136, 258, RGBA(0, 0, 0, 255));
    rect(10, 248, 10 + 124 * nitro01, 256, nitro_on ? COL_YELLOW : COL_CYAN);

    uint32_t cc = tier >= 3 ? COL_YELLOW : tier == 2 ? COL_MAGENTA : tier == 1 ? COL_CYAN : RGB(90, 90, 110);
    rect(8, 230, 136, 242, RGBA(0, 0, 0, 255));
    rect(10, 232, 10 + 124 * charge01, 240, cc);
    const float ticks[3] = { 0.25f, 0.55f, 0.90f };
    for (int i = 0; i < 3; i++) rect(10 + 124 * ticks[i] - 1, 230, 10 + 124 * ticks[i] + 1, 242, RGB(255, 255, 255));

    if (draft01 > 0.05f) rect(142, 248, 142 + 12 * draft01, 256, RGB(255, 255, 255));

    sceGuEnable(GU_FOG);
    sceGuEnable(GU_DEPTH_TEST);
}
