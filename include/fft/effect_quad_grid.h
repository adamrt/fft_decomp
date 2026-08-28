#ifndef FFT_EFFECT_QUAD_GRID_H
#define FFT_EFFECT_QUAD_GRID_H

/* Work blocks of the scrolling textured quad grid renderer that EFFECT
 * overlays compile from one source in 2x2, 4x4 and 8x8 builds (for example
 * effect_e047_update_textured_quad_grid_4x4 in E047). Each block is allocated in
 * phase 1 and freed in phase 3. The 2x2 and 4x4 builds leave a 0x48-byte gap
 * after the quads that the 8x8 build does not have. */

#include "psx/gpu.h"
#include "psx/types.h"

typedef struct effect_textured_quad_grid_2x2_work {
    s32 buffer;           /* 0x000: quads[buffer] is drawn this frame */
    POLY_GT4 quads[2][4]; /* 0x004: row-major 2x2 grid, double buffered */
    u8 _unknown_1a4[0x48];
    s32 scroll_u; /* 0x1ec: 20.12 texture scroll, wrapped to texture_width / 2 */
    s32 scroll_v; /* 0x1f0: 20.12 texture scroll, wrapped to texture_height / 2 */
    u8 _unknown_1f4[4];
    u16 texture_u;       /* 0x1f8: from the geometry entry; read back as u8 */
    u16 texture_v;       /* 0x1fa */
    s32 frame;           /* 0x1fc: column of the palette-curve rows */
    s32 destroy_latched; /* 0x200: phase 3 frees the block on its second call */
} effect_textured_quad_grid_2x2_work_t;

typedef char
    assert_effect_textured_quad_grid_2x2_work_size[sizeof(effect_textured_quad_grid_2x2_work_t) == 0x204 ? 1 : -1];

typedef struct effect_textured_quad_grid_4x4_work {
    s32 buffer;            /* 0x000: quads[buffer] is drawn this frame */
    POLY_GT4 quads[2][16]; /* 0x004: row-major 4x4 grid, double buffered */
    u8 _unknown_684[0x48];
    s32 scroll_u; /* 0x6cc: 20.12 texture scroll, wrapped to texture_width / 2 */
    s32 scroll_v; /* 0x6d0: 20.12 texture scroll, wrapped to texture_height / 2 */
    u8 _unknown_6d4[4];
    u16 texture_u;       /* 0x6d8: from the geometry entry; read back as u8 */
    u16 texture_v;       /* 0x6da */
    s32 frame;           /* 0x6dc: column of the palette-curve rows */
    s32 destroy_latched; /* 0x6e0: phase 3 frees the block on its second call */
} effect_textured_quad_grid_4x4_work_t;

typedef char
    assert_effect_textured_quad_grid_4x4_work_size[sizeof(effect_textured_quad_grid_4x4_work_t) == 0x6e4 ? 1 : -1];

typedef struct effect_textured_quad_grid_8x8_work {
    s32 buffer;            /* 0x000: quads[buffer] is drawn this frame */
    POLY_GT4 quads[2][64]; /* 0x004: row-major 8x8 grid, double buffered */
    s32 scroll_u;          /* 0x1a04: 20.12 texture scroll, wrapped to texture_width / 2 */
    s32 scroll_v;          /* 0x1a08: 20.12 texture scroll, wrapped to texture_height / 2 */
    u8 _unknown_1a0c[4];
    u16 texture_u;       /* 0x1a10: from the geometry entry; read back as u8 */
    u16 texture_v;       /* 0x1a12 */
    s32 frame;           /* 0x1a14: column of the palette-curve rows */
    s32 destroy_latched; /* 0x1a18: phase 3 frees the block on its second call */
} effect_textured_quad_grid_8x8_work_t;

typedef char
    assert_effect_textured_quad_grid_8x8_work_size[sizeof(effect_textured_quad_grid_8x8_work_t) == 0x1a1c ? 1 : -1];

/* Overlay-local vertex brightness rows (12-bit fixed) for the 2x2 grid:
 * brightness_row selects the row, the column is the quad row. */
extern u32 g_effect_electricity_grid_brightness_table[][3];

/* Overlay-local vertex brightness rows (12-bit fixed) for the 8x8 grid:
 * brightness_row selects the row, the column is the quad row. */
extern u32 g_effect_textured_grid_brightness_table[][9];

#endif
