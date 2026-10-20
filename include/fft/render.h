#ifndef RENDER_H
#define RENDER_H

/* BATTLE's render buffers. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* Primitive pool sizes of a render buffer. The map mesh's position records (battle_map_mesh.h)
 * parallel the four map polygon pools; the weather sprites are the flat-textured triangles. */
enum {
    RENDER_TEXTURED_TRIANGLE_COUNT = 360,
    RENDER_TEXTURED_QUAD_COUNT = 710,
    RENDER_UNTEXTURED_TRIANGLE_COUNT = 64,
    RENDER_UNTEXTURED_QUAD_COUNT = 256,
    RENDER_WEATHER_SPRITE_COUNT = 64
};

/* One of the two 60968-byte battle render buffers. The primitive pools are
 * pre-initialized once; gfx_battle_otag_swap reads the ordering table
 * pointer at +0xe5b4. */
typedef struct render_buffer {
    POLY_GT3 gt3[RENDER_TEXTURED_TRIANGLE_COUNT]; /* 0x00000 */
    POLY_GT4 gt4[RENDER_TEXTURED_QUAD_COUNT];     /* 0x03840 */
    POLY_F3 f3[RENDER_UNTEXTURED_TRIANGLE_COUNT]; /* 0x0c878 */
    POLY_F4 f4[RENDER_UNTEXTURED_QUAD_COUNT];     /* 0x0cd78 */
    POLY_G4 overlay;                              /* 0x0e578 */
    DR_MODE overlay_modes[2];                     /* 0x0e59c */
    u32* otag;                                    /* 0x0e5b4 */
    u8 _unused_e5b8[0x70];                        /* 0x0e5b8 */
    POLY_FT3 ft3[RENDER_WEATHER_SPRITE_COUNT];    /* 0x0e628 */
} render_buffer_t;
typedef char render_buffer_size_must_be_60968[(sizeof(render_buffer_t) == 60968) ? 1 : -1];

extern render_buffer_t g_render_buffers[2];

/* The buffer being built this frame; gfx_battle_otag_swap alternates
 * it between the two g_render_buffers entries. */
extern render_buffer_t* g_render_current_buffer;

void render_buffers_init(void);

#endif
