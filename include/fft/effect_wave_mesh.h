#ifndef FFT_EFFECT_WAVE_MESH_H
#define FFT_EFFECT_WAVE_MESH_H

/* Shared records of the 2,924-byte 8x8 wave-distorted quad mesh renderer that
 * eight EFFECT overlays compile from one source (for example E033 at
 * 0x801c3c7c). */

#include "psx/gpu.h"
#include "psx/types.h"

/* One lattice point of the scratchpad wave grid. */
typedef struct effect_wave_mesh_point {
    s16 x;
    s16 y;
} effect_wave_mesh_point_t;

/* Work block of the 8x8 wave mesh, allocated in phase 1. The four wave phases
 * are 4096-per-turn angles; the geometry entry's spawn spread (x, y) and
 * wave_phase_speed curves advance them once per update. */
typedef struct effect_wave_mesh_work {
    s32 buffer;            /* 0x0000: quads[buffer] is drawn this frame */
    POLY_GT4 quads[2][64]; /* 0x0004: row-major 8x8 grid, double buffered */
    s32 wave_y_phase;      /* 0x1a04: sine phase of the vertical wave at point (0, 0) */
    u8 _unknown_1a08[0x0c];
    s32 wave_y_amplitude_phase; /* 0x1a14: cosine phase scaling row 0's vertical amplitude */
    u8 _unknown_1a18[0x0c];
    s32 wave_x_phase; /* 0x1a24: sine phase of the horizontal wave at point (0, 0) */
    u8 _unknown_1a28[0x0c];
    s32 wave_x_amplitude_phase; /* 0x1a34: cosine phase scaling column 0's horizontal amplitude */
    u8 _unknown_1a38[0x0c];
    s32 scroll_u; /* 0x1a44 */
    s32 scroll_v; /* 0x1a48 */
    u8 _unknown_1a4c[4];
    u16 texture_u;       /* 0x1a50 */
    u16 texture_v;       /* 0x1a52 */
    s32 frame;           /* 0x1a54 */
    s32 destroy_latched; /* 0x1a58 */
} effect_wave_mesh_work_t;

typedef char assert_effect_wave_mesh_work_size[sizeof(effect_wave_mesh_work_t) == 0x1a5c ? 1 : -1];

/* Scratchpad (0x1f800000) layout while the mesh is rebuilt. */
typedef struct effect_wave_mesh_scratch {
    effect_wave_mesh_point_t points[9][9]; /* 0x000: wave-displaced lattice */
} effect_wave_mesh_scratch_t;

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the quad row (0-8) the column. */
extern u32 g_effect_e035_wave_mesh_brightness_table[][9];
extern u32 g_effect_e073_wave_mesh_brightness_table[][9];
extern u32 g_effect_e080_wave_mesh_brightness_table[][9];
extern u32 g_effect_e230_wave_mesh_brightness_table[][9];
extern u32 g_effect_e453_wave_mesh_brightness_table[][9];
extern u32 g_effect_wave_mesh_brightness_table[][9];

#endif
