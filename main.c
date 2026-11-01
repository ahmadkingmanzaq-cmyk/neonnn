/* debug.h - minimal numeric overlay (Select toggles). Drawn with GU sprites so it
 * works with the 5650 framebuffer. A full bitmap-font HUD arrives in stage 5.
 * Legend: GREEN = FPS, CYAN = km/h, YELLOW = triangles, MAGENTA = near misses,
 * WHITE = car number, ORANGE = lap, GREEN-dim = current lap time (s). */
#ifndef DEBUG_H
#define DEBUG_H
void debug_draw(int fps, int speed_kmh, int tris, int nearmisses, int car_no, int lap, int lap_seconds);

/* Gameplay bars (always on until the stage-5 HUD): nitro (cyan, yellow while firing),
 * drift charge with tier ticks at 25/55/90 (grey -> cyan -> magenta -> yellow),
 * and a white slipstream blip. */
void debug_bars(float nitro01, int nitro_on, float charge01, int tier, float draft01);

#endif
