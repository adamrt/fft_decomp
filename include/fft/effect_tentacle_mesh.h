#ifndef FFT_EFFECT_TENTACLE_MESH_H
#define FFT_EFFECT_TENTACLE_MESH_H

/* Shared records of two emitter-driven EFFECT renderers compiled from one
 * source family: the 8,668-byte tentacle mesh (E376, E381, E382, E384, E401
 * and E453) that draws up to eight textured strips, and the 5,624-byte particle
 * trail (E077, E161, E451 and E485) that tracks up to four point trails. Both
 * read their effect_geometry_entry_t through
 * effect_tentacle_mesh_geometry_view_t. */

#include "fft/effect.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* One point of a segment's ring of trailing positions. */
typedef struct effect_tentacle_mesh_point {
    s16 x;
    s16 y;
    s16 z;
} effect_tentacle_mesh_point_t;

typedef char assert_effect_tentacle_mesh_point_size[sizeof(effect_tentacle_mesh_point_t) == 6 ? 1 : -1];

typedef struct effect_tentacle_mesh_segment {
    SVECTOR tip;                            /* 0x000 */
    VECTOR velocity;                        /* 0x008: 20.12 fixed */
    VECTOR position;                        /* 0x018: 20.12 fixed */
    POLY_GT4 quads[4][8];                   /* 0x028: banks buffer and buffer + 2 */
    effect_tentacle_mesh_point_t points[9]; /* 0x6a8 */
    s16 head;                               /* 0x6de */
    s16 length;                             /* 0x6e0 */
    s16 active;                             /* 0x6e2 */
    u8 _unknown_6e4[0x02];
    s16 frame;       /* 0x6e6: texture animation frame */
    s16 frame_timer; /* 0x6e8 */
    u8 _unknown_6ea[0x02];
} effect_tentacle_mesh_segment_t;

typedef char assert_effect_tentacle_mesh_segment_size[sizeof(effect_tentacle_mesh_segment_t) == 0x6ec ? 1 : -1];

typedef struct effect_tentacle_mesh_work {
    s32 buffer; /* 0x0000 */
    s16 count;  /* 0x0004 */
    u8 _unknown_0006[0x02];
    effect_tentacle_mesh_segment_t segments[8]; /* 0x0008 */
    MATRIX matrix;                              /* 0x3768 */
    s32 destroy_delay;                          /* 0x3788 */
} effect_tentacle_mesh_work_t;

typedef char assert_effect_tentacle_mesh_work_size[sizeof(effect_tentacle_mesh_work_t) == 0x378c ? 1 : -1];

/* A 17-point particle trail; spawning seeds only points 0-8 (the target's loop bound is 9). */
typedef struct effect_trail_segment {
    SVECTOR tip;                             /* 0x00 */
    VECTOR velocity;                         /* 0x08: 20.12 fixed */
    VECTOR position;                         /* 0x18: 20.12 fixed */
    effect_tentacle_mesh_point_t points[17]; /* 0x28 */
    s16 head;                                /* 0x8e */
    s16 length;                              /* 0x90 */
    s16 active;                              /* 0x92 */
    u8 _unknown_94[0x04];
} effect_trail_segment_t;

typedef char assert_effect_trail_segment_size[sizeof(effect_trail_segment_t) == 0x98 ? 1 : -1];

typedef struct effect_trail_work {
    s32 buffer; /* 0x000 */
    s16 count;  /* 0x004 */
    u8 _unknown_006[0x02];
    effect_trail_segment_t segments[4]; /* 0x008 */
    MATRIX matrix;                      /* 0x268 */
    s32 destroy_delay;                  /* 0x288 */
} effect_trail_work_t;

typedef char assert_effect_trail_work_size[sizeof(effect_trail_work_t) == 0x28c ? 1 : -1];

/* These renderers' view of one effect_geometry_entry_t. They spawn segments as
 * the BATTLE particle emitter spawns particles, so the fields they share with
 * it keep its names and curve nibbles: curve_indices[0] nibbles 0 spawn
 * position, 1 spawn spread, 2 velocity base angle, 3 velocity spread and
 * 7 radial speed; curve_indices[1] nibbles 1 tip spread, 2 lifetime, 3 target
 * offset, 5 particle count and 6 spawn interval. The remaining named fields
 * are renderer-specific. */
typedef struct effect_tentacle_mesh_geometry_view {
    u8 _unknown_00[0x02];
    u16 placement_flags; /* 0x02: bits 9-11 place the spawn origin, bits 5-7 the tip */
    u8 _unknown_04[0x02];
    u16 flags;                              /* 0x06: bits 0-1 and 2-3 enable death and mid-life children */
    u32 curve_indices[2];                   /* 0x08 */
    u32 colour_curves;                      /* 0x10 */
    effect_emitter_geometry_view_t emitter; /* 0x14: read by the battle_effect_interpolate_emitter_* helpers */
    u16 texture_page_flags;                 /* 0x4c */
    s16 brightness_row;                     /* 0x4e */
    u8 _unknown_50[0x04];
    s16 ot_depth; /* 0x54: added to each quad's depth */
    u8 _unknown_56[0x06];
    s16 radial_speed_min_start; /* 0x5c */
    s16 radial_speed_max_start; /* 0x5e */
    s16 radial_speed_min_end;   /* 0x60 */
    s16 radial_speed_max_end;   /* 0x62 */
    s16 texture_u;              /* 0x64 */
    u8 _unknown_66[0x02];
    s16 texture_v; /* 0x68 */
    u8 _unknown_6a[0x02];
    s16 frame_count; /* 0x6c */
    s16 frame_delay; /* 0x6e */
    u8 _unknown_70[0x0c];
    s16 tip_spread_start_x; /* 0x7c */
    u8 _unknown_7e[0x02];
    s16 tip_spread_start_y; /* 0x80 */
    u8 _unknown_82[0x02];
    s16 tip_spread_start_z; /* 0x84 */
    u8 _unknown_86[0x02];
    s16 tip_spread_end_x; /* 0x88 */
    u8 _unknown_8a[0x02];
    s16 tip_spread_end_y; /* 0x8c */
    u8 _unknown_8e[0x02];
    s16 tip_spread_end_z; /* 0x90 */
    u8 _unknown_92[0x02];
    s16 lifetime_start[2];      /* 0x94: min, max; the segment's length */
    s16 lifetime_end[2];        /* 0x98 */
    s16 target_offset_start[3]; /* 0x9c: the tip, added to the anchor chosen by placement_flags bits 5-7 */
    s16 target_offset_end[3];   /* 0xa2 */
    s16 texture_width;          /* 0xa8 */
    s16 texture_height;         /* 0xaa */
    u8 _unknown_ac[0x04];
    s16 particle_count_start; /* 0xb0: segments spawned per interval */
    s16 particle_count_end;   /* 0xb2 */
    s16 spawn_interval_start; /* 0xb4 */
    s16 spawn_interval_end;   /* 0xb6 */
    u8 _unknown_b8[0x08];
    u8 child_emitter_on_death; /* 0xc0 */
    u8 child_emitter_mid_life; /* 0xc1: spawned at the trailing point each frame */
    u8 _unknown_c2[0x02];
} effect_tentacle_mesh_geometry_view_t;

typedef char
    assert_effect_tentacle_mesh_geometry_view_size[sizeof(effect_tentacle_mesh_geometry_view_t) == 0xc4 ? 1 : -1];

extern u32 g_effect_e384_tentacle_mesh_brightness_table[][9];
extern u32 g_effect_e453_tentacle_mesh_brightness_table[][9];
extern u32 g_effect_tentacle_mesh_brightness_table[][9];

#endif
