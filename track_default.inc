/* renderer.h - libgu setup, VRAM layout, frame begin/end, glow pass. */
#ifndef RENDERER_H
#define RENDERER_H
#include <stddef.h>
#include "camera.h"

int    renderer_init(void);
void   renderer_begin_frame(const Camera *cam);   /* clear + projection + view */
void   renderer_end_frame(int vblanks);           /* finish, vsync, swap */

/* Additive "fake bloom" pass: no depth writes, src-alpha additive blend. */
void   renderer_glow_begin(void);
void   renderer_glow_end(void);

/* Simple VRAM bump allocator for textures (font, glow sprites later).
 * Returns an offset from VRAM base, or (size_t)-1 if out of space. */
size_t renderer_vram_alloc(size_t bytes);
void  *renderer_vram_ptr(size_t offset);
size_t renderer_vram_free(void);

#endif
