/* config.h - global constants and tuning for NEON RUSH.
 *
 * WORLD CONVENTIONS (used everywhere):
 *   right-handed, +Y up, +X right, yaw 0 faces -Z.
 *   forward(yaw) = ( sin(yaw), 0, -cos(yaw) ), yaw increases to the right.
 *   Stage-1 road runs along -Z; segment i starts at z = -i * SEG_LEN.
 */
#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>

#define SCREEN_W   480
#define SCREEN_H   272
#define BUF_W      512            /* framebuffer stride (must be power of 2 >= 480) */

#define SIM_DT     (1.0f / 60.0f) /* fixed physics/gameplay timestep */
#define TARGET_VBLANKS 1          /* 1 = 60 FPS cap, 2 = locked 30 FPS */

/* colour helpers: PSP vertex colours are ABGR 8888 */
#define RGBA(r,g,b,a) ((((uint32_t)(a))<<24)|(((uint32_t)(b))<<16)|(((uint32_t)(g))<<8)|((uint32_t)(r)))
#define RGB(r,g,b)    RGBA(r,g,b,255)

#define COL_CYAN     RGB(0,229,255)
#define COL_MAGENTA  RGB(255,0,200)
#define COL_YELLOW   RGB(255,225,40)
#define FOG_COLOR    RGB(8,6,24)

/* fog hides the end of the streaming window: FOG_FAR < (SEGS_AHEAD * SEG_LEN) */
#define FOG_NEAR     50.0f
#define FOG_FAR      170.0f
#define CAM_NEAR     1.5f         /* 16-bit depth buffer: keep near plane generous */
#define CAM_FAR      260.0f

/* road / streaming (test road) */
#define SEG_LEN      24.0f
#define ROAD_HALF_W  8.0f
#define SEGS_AHEAD   8
#define SEGS_BEHIND  2

/* collision: inner face of the barrier (barrier centre W+0.9, half-width 0.5) */
#define WALL_X       (ROAD_HALF_W + 0.4f)

#endif
