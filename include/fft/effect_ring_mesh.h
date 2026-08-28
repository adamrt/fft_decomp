#ifndef FFT_EFFECT_RING_MESH_H
#define FFT_EFFECT_RING_MESH_H

/* Shared records of the 5,584-byte ring mesh renderer that 32 EFFECT overlays
 * compile from one source (for example E450 at 0x801c2d08). */

#include "fft/effect_summon_mesh.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/types.h"

enum {
    RING_MESH_SEGMENTS = 32,  /* quads around one ring */
    RING_MESH_ARC_POINTS = 9, /* lattice points of one 90-degree arc */
};

/* This renderer's view of one effect_geometry_entry_t. It shares the layout of
 * effect_summon_mesh_geometry_view_t and adds the per-row radius terms at
 * 0x6c/0x78 and 0x84/0x90. */
typedef struct effect_ring_mesh_geometry_view {
    u8 _unknown_00[0x02];
    u16 placement_flags; /* 0x02: bits 9-11 select how the origin is placed */
    u8 _unknown_04[0x02];
    u16 flags;              /* 0x06: bit 6 = per-channel colour curves */
    u32 scroll_curves;      /* 0x08: nibbles 0 origin, 2 texture, 7 spin speed */
    u32 wave_curves;        /* 0x0c: nibbles 0 growth, 1 growth step, 4 radius */
    u32 colour_curves;      /* 0x10: nibbles 0-2 = r/g/b */
    s16 origin_start[3];    /* 0x14 */
    s16 origin_end[3];      /* 0x1a */
    s16 u_span;             /* 0x20 */
    s16 v_span;             /* 0x22 */
    u16 texture_page_flags; /* 0x24: bits 0-1 blend rate, bit 2 clears ABE */
    u8 _unknown_26[0x06];
    s16 u_start;        /* 0x2c */
    s16 v_start;        /* 0x2e */
    u16 v_scroll_speed; /* 0x30 */
    s16 u_end;          /* 0x32 */
    s16 v_end;          /* 0x34 */
    u8 _unknown_36[0x06];
    s16 brightness_row; /* 0x3c */
    u8 _unknown_3e[0x1e];
    s16 spin_speed_start; /* 0x5c */
    u8 _unknown_5e[0x02];
    s16 spin_speed_end; /* 0x60 */
    u8 _unknown_62[0x02];
    s16 growth_start; /* 0x64 */
    u8 _unknown_66[0x02];
    s16 height_start; /* 0x68 */
    u8 _unknown_6a[0x02];
    s16 row_radius_start; /* 0x6c */
    u8 _unknown_6e[0x02];
    s16 growth_end; /* 0x70 */
    u8 _unknown_72[0x02];
    s16 height_end; /* 0x74 */
    u8 _unknown_76[0x02];
    s16 row_radius_end; /* 0x78 */
    u8 _unknown_7a[0x02];
    s16 growth_step_start; /* 0x7c */
    u8 _unknown_7e[0x02];
    s16 height_step_start; /* 0x80 */
    u8 _unknown_82[0x02];
    s16 row_radius_step_start; /* 0x84 */
    u8 _unknown_86[0x02];
    s16 growth_step_end; /* 0x88 */
    u8 _unknown_8a[0x02];
    s16 height_step_end; /* 0x8c */
    u8 _unknown_8e[0x02];
    s16 row_radius_step_end; /* 0x90 */
    u8 _unknown_92[0x16];
    s16 radius_start; /* 0xa8 */
    u8 _unknown_aa[0x02];
    s16 radius_end; /* 0xac */
    u8 _unknown_ae[0x16];
} effect_ring_mesh_geometry_view_t;

typedef char assert_effect_ring_mesh_geometry_view_size[sizeof(effect_ring_mesh_geometry_view_t) == 0xc4 ? 1 : -1];

/* Work block: two POLY_GT4 buffers of two rings of 32 quads. */
typedef struct effect_ring_mesh_work {
    s32 buffer;                                /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][RING_MESH_SEGMENTS * 2]; /* 0x0004: ring * 32 + segment */
    s16 spin;                                  /* 0x1a04: ring rotation, wrapped to 0xfff */
    s16 v_scroll;                              /* 0x1a06: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;                         /* 0x1a08: 24.8 fixed, added to the radius */
    s32 radius_speed;                          /* 0x1a0c */
    u8 _unknown_1a10[0x08];
    s32 destroy_delay; /* 0x1a18: the destroy phase waits one call before freeing */
} effect_ring_mesh_work_t;

typedef char assert_effect_ring_mesh_work_size[sizeof(effect_ring_mesh_work_t) == 0x1a1c ? 1 : -1];

/* Scratchpad (0x1f800000) layout while the ring is rebuilt. points[row][0..8]
 * sweep one quadrant in 0x80 steps; the rotating add helpers reuse them for
 * the other three quadrants. */
typedef struct effect_ring_mesh_scratch {
    VECTOR points[3][RING_MESH_ARC_POINTS]; /* 0x000 */
    VECTOR origin;                          /* 0x1b0 */
    SVECTOR vertex;                         /* 0x1c0 */
    VECTOR screen[4];                       /* 0x1c8: projected quad corners; RotTrans writes its flag to pad */
    s16 depth[3][RING_MESH_SEGMENTS + 1];   /* 0x208 */
} effect_ring_mesh_scratch_t;

/* Work block of the single-ring variant (4,948-byte renderer in E411, E464
 * and E483): two POLY_GT4 buffers of one ring of 32 quads. */
typedef struct effect_single_ring_mesh_work {
    s32 buffer;                            /* 0x000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][RING_MESH_SEGMENTS]; /* 0x004 */
    s16 spin;                              /* 0xd04: ring rotation, wrapped to 0xfff */
    s16 v_scroll;                          /* 0xd06: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;                     /* 0xd08: 24.8 fixed, added to the radius */
    s32 radius_speed;                      /* 0xd0c */
    u8 _unknown_d10[0x08];
    s32 destroy_delay; /* 0xd18: the destroy phase waits one call before freeing */
} effect_single_ring_mesh_work_t;

typedef char assert_effect_single_ring_mesh_work_size[sizeof(effect_single_ring_mesh_work_t) == 0xd1c ? 1 : -1];

/* Scratchpad (0x1f800000) layout of the single-ring variant: two lattice
 * rows of one quadrant. */
typedef struct effect_single_ring_mesh_scratch {
    VECTOR points[2][RING_MESH_ARC_POINTS]; /* 0x000 */
} effect_single_ring_mesh_scratch_t;

extern void battle_effect_add_vectors_to_svector(VECTOR* left, VECTOR* right, SVECTOR* result);
extern void battle_effect_add_vectors_to_svector_rot_y_270(VECTOR* left, VECTOR* right, SVECTOR* result);
extern void battle_effect_add_vectors_to_svector_rot_y_180(VECTOR* left, VECTOR* right, SVECTOR* result);
extern void battle_effect_add_vectors_to_svector_rot_y_90(VECTOR* left, VECTOR* right, SVECTOR* result);

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-2) the column. */
extern u32 g_effect_e015_ring_mesh_brightness_table[][3];
extern u32 g_effect_e080_ring_mesh_brightness_table[][3];
extern u32 g_effect_e134_ring_mesh_brightness_table[][3];
extern u32 g_effect_e235_ring_mesh_brightness_table[][3];
extern u32 g_effect_e256_ring_mesh_brightness_table[][3];
extern u32 g_effect_e371_ring_mesh_brightness_table[][3];
extern u32 g_effect_e373_ring_mesh_brightness_table[][3];
extern u32 g_effect_e383_ring_mesh_brightness_table[][3];
extern u32 g_effect_e450_ring_mesh_brightness_table[][3];
extern u32 g_effect_e451_ring_mesh_brightness_table[][3];
extern u32 g_effect_e452_ring_mesh_brightness_table[][3];
extern u32 g_effect_e461_ring_mesh_brightness_table[][3];
extern u32 g_effect_e474_ring_mesh_brightness_table[][3];
extern u32 g_effect_e480_ring_mesh_brightness_table[][3];
extern u32 g_effect_ring_mesh_brightness_table[][3];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_e004_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e015_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e065_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e067_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e073_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e074_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e077_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e134_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e242_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e256_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e259_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_e383_ring_stack_mesh_brightness_table[][9];
extern u32 g_effect_ring_stack_mesh_brightness_table[][9];

/* Overlay-local vertex brightness rows (12-bit fixed). */
extern u32 g_effect_e464_single_ring_mesh_brightness_table[][2];
extern u32 g_effect_single_ring_mesh_brightness_table[][2];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_e317_tube_mesh_brightness_table[][9];
extern u32 g_effect_e383_tube_mesh_brightness_table[][9];
extern u32 g_effect_tube_mesh_brightness_table[][9];

/* Overlay-local ring scale per lattice row pair (12-bit fixed): row 4 - i and
 * row 4 + i share g_effect_tube_mesh_ring_scales[i]. */
extern s32 g_effect_e317_tube_mesh_ring_scales[5];
extern s32 g_effect_e383_tube_mesh_ring_scales[5];
extern s32 g_effect_tube_mesh_ring_scales[5];

#endif
