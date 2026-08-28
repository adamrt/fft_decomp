#ifndef FFT_EFFECT_SUMMON_MESH_H
#define FFT_EFFECT_SUMMON_MESH_H

/* Shared records of the 4,744-byte summon dome mesh renderer that 12 EFFECT
 * overlays compile from one source (for example E474 at 0x801c42c4). */

#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "fft/map.h"
#include "psx/gte.h"
#include "psx/types.h"

/* One scratchpad lattice point; the 9x17 lattice starts at 0x1f800000 and is
 * read back by effect_add_scratchpad_xyz_components. */
typedef struct effect_summon_mesh_point {
    s16 x;
    s16 y;
    s16 z;
} effect_summon_mesh_point_t;

typedef char assert_effect_summon_mesh_point_size[sizeof(effect_summon_mesh_point_t) == 6 ? 1 : -1];

/* Work block: two POLY_GT4 buffers of 8 rows x 16 columns. */
typedef struct effect_summon_mesh_work {
    s32 buffer;             /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][128]; /* 0x0004: row * 16 + column */
    s16 spin;               /* 0x3404: lattice rotation, wrapped to 0xfff */
    u16 v_scroll;           /* 0x3406: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;      /* 0x3408: 24.8 fixed, added to the radius */
    s32 radius_speed;       /* 0x340c */
    u8 _unknown_3410[0x08];
    s32 destroy_delay; /* 0x3418: the destroy phase waits one call before freeing */
} effect_summon_mesh_work_t;

typedef char assert_effect_summon_mesh_work_size[sizeof(effect_summon_mesh_work_t) == 0x341c ? 1 : -1];

typedef struct effect_summon_mesh_curve_halves {
    u16 low;  /* 0x00 */
    u16 high; /* 0x02 */
} effect_summon_mesh_curve_halves_t;

/* The target reads the radius curve nibble with its own halfword load. */
typedef union effect_summon_mesh_curve_word {
    u32 word;
    effect_summon_mesh_curve_halves_t half;
} effect_summon_mesh_curve_word_t;

/* This renderer's view of one effect_geometry_entry_t. The shared entry type
 * names several of these offsets for other effect kinds; the curve nibbles
 * select rows of g_effect_palette_table. */
typedef struct effect_summon_mesh_geometry_view {
    u8 _unknown_00[0x02];
    u16 placement_flags; /* 0x02: bits 9-11 select how the origin is placed */
    u8 _unknown_04[0x02];
    u16 flags;                                   /* 0x06: bit 6 = per-channel colour curves */
    u32 scroll_curves;                           /* 0x08: nibbles 0 origin, 2 texture, 7 spin speed */
    effect_summon_mesh_curve_word_t wave_curves; /* 0x0c: nibbles 0 growth, 1 growth step, 4 radius */
    u32 colour_curves;                           /* 0x10: nibbles 0-2 = r/g/b */
    s16 origin_start[3];                         /* 0x14 */
    s16 origin_end[3];                           /* 0x1a */
    s16 u_span;                                  /* 0x20 */
    s16 v_span;                                  /* 0x22 */
    u16 texture_page_flags;                      /* 0x24: bits 0-1 blend rate, bit 2 clears ABE */
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
    u8 _unknown_6a[0x06];
    s16 growth_end; /* 0x70 */
    u8 _unknown_72[0x02];
    s16 height_end; /* 0x74 */
    u8 _unknown_76[0x06];
    s16 growth_step_start; /* 0x7c */
    u8 _unknown_7e[0x02];
    s16 height_step_start; /* 0x80 */
    u8 _unknown_82[0x06];
    s16 growth_step_end; /* 0x88 */
    u8 _unknown_8a[0x02];
    s16 height_step_end; /* 0x8c */
    u8 _unknown_8e[0x1a];
    s16 radius_start; /* 0xa8 */
    u8 _unknown_aa[0x02];
    s16 radius_end; /* 0xac */
    u8 _unknown_ae[0x16];
} effect_summon_mesh_geometry_view_t;

typedef char assert_effect_summon_mesh_geometry_view_size[sizeof(effect_summon_mesh_geometry_view_t) == 0xc4 ? 1 : -1];

/* Leading flags word of the effect section at *g_battle_effect_flags_section. */
typedef struct effect_summon_mesh_effect_header {
    u32 flags; /* 0x00: bit 3 is passed as the add_depth argument */
} effect_summon_mesh_effect_header_t;

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_e071_summon_mesh_brightness_table[][9];
extern u32 g_effect_e074_summon_mesh_brightness_table[][9];
extern u32 g_effect_e259_summon_mesh_brightness_table[][9];
extern u32 g_effect_e338_summon_mesh_brightness_table[][9];
extern u32 g_effect_e373_summon_mesh_brightness_table[][9];
extern u32 g_effect_e452_summon_mesh_brightness_table[][9];
extern u32 g_effect_e474_summon_mesh_brightness_table[][9];
extern u32 g_effect_summon_mesh_brightness_table[][9];

#endif
