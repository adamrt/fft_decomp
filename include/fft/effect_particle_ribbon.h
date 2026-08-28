#ifndef FFT_EFFECT_PARTICLE_RIBBON_H
#define FFT_EFFECT_PARTICLE_RIBBON_H

/* Shared records of the homing particle-ribbon renderer that 16 EFFECT
 * overlays compile from one source (for example E000 at 0x801c2500). It comes
 * in two sizes: the short variant keeps 8 ribbons with 9-point trails (E000,
 * E242, E317, E336, E355, E371, E402, E409) and the long variant 4 ribbons
 * with 17-point trails (E070, E403, E412, E461, E462, E463, E465, E466). */

#include "fft/effect.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

enum {
    EFFECT_PARTICLE_RIBBON_SHORT_SLOTS = 8,
    EFFECT_PARTICLE_RIBBON_SHORT_POINTS = 9,
    EFFECT_PARTICLE_RIBBON_LONG_SLOTS = 4,
    EFFECT_PARTICLE_RIBBON_LONG_POINTS = 17,
};

/* One trail point, in world units. */
typedef struct effect_particle_ribbon_point {
    s16 x;
    s16 y;
    s16 z;
} effect_particle_ribbon_point_t;

typedef char assert_effect_particle_ribbon_point_size[sizeof(effect_particle_ribbon_point_t) == 6 ? 1 : -1];

/* One short ribbon: a particle and the double-buffered quad strip drawn along
 * its recent path, one quad per pair of adjacent trail points. */
typedef struct effect_particle_ribbon_short {
    SVECTOR target;                                                            /* 0x000: homing destination */
    VECTOR velocity;                                                           /* 0x008: 20.12 fixed */
    VECTOR position;                                                           /* 0x018: 20.12 fixed */
    POLY_GT4 quads[2][EFFECT_PARTICLE_RIBBON_SHORT_POINTS - 1];                /* 0x028 */
    effect_particle_ribbon_point_t trail[EFFECT_PARTICLE_RIBBON_SHORT_POINTS]; /* 0x368: ring buffer indexed by age */
    s16 age;                                                                   /* 0x39e */
    s16 lifetime;                                                              /* 0x3a0 */
    s16 active;                                                                /* 0x3a2 */
    u8 _unknown_3a4[0x02];
    s16 frame;       /* 0x3a6: texture animation frame */
    s16 frame_timer; /* 0x3a8 */
    u8 _unknown_3aa[0x02];
} effect_particle_ribbon_short_t;

typedef char assert_effect_particle_ribbon_short_size[sizeof(effect_particle_ribbon_short_t) == 0x3ac ? 1 : -1];

/* One long ribbon: the short layout with 16 quads per buffer and a 17-point
 * trail. */
typedef struct effect_particle_ribbon_long {
    SVECTOR target;                                                           /* 0x000: homing destination */
    VECTOR velocity;                                                          /* 0x008: 20.12 fixed */
    VECTOR position;                                                          /* 0x018: 20.12 fixed */
    POLY_GT4 quads[2][EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1];                /* 0x028 */
    effect_particle_ribbon_point_t trail[EFFECT_PARTICLE_RIBBON_LONG_POINTS]; /* 0x6a8: ring buffer indexed by age */
    s16 age;                                                                  /* 0x70e */
    s16 lifetime;                                                             /* 0x710 */
    s16 active;                                                               /* 0x712 */
    u8 _unknown_714[0x02];
    s16 frame;       /* 0x716: texture animation frame */
    s16 frame_timer; /* 0x718 */
    u8 _unknown_71a[0x02];
} effect_particle_ribbon_long_t;

typedef char assert_effect_particle_ribbon_long_size[sizeof(effect_particle_ribbon_long_t) == 0x71c ? 1 : -1];

/* Work block allocated by the short variant. */
typedef struct effect_particle_ribbon_short_work {
    s32 buffer; /* 0x0000 */
    s16 count;  /* 0x0004: active ribbons */
    u8 _unknown_0006[0x02];
    effect_particle_ribbon_short_t ribbons[EFFECT_PARTICLE_RIBBON_SHORT_SLOTS]; /* 0x0008 */
    MATRIX matrix;                                                              /* 0x1d68 */
    s32 destroy_delay;                                                          /* 0x1d88 */
} effect_particle_ribbon_short_work_t;

typedef char
    assert_effect_particle_ribbon_short_work_size[sizeof(effect_particle_ribbon_short_work_t) == 0x1d8c ? 1 : -1];

/* Work block allocated by the long variant. */
typedef struct effect_particle_ribbon_long_work {
    s32 buffer; /* 0x0000 */
    s16 count;  /* 0x0004: active ribbons */
    u8 _unknown_0006[0x02];
    effect_particle_ribbon_long_t ribbons[EFFECT_PARTICLE_RIBBON_LONG_SLOTS]; /* 0x0008 */
    MATRIX matrix;                                                            /* 0x1c78 */
    s32 destroy_delay;                                                        /* 0x1c98 */
} effect_particle_ribbon_long_work_t;

typedef char
    assert_effect_particle_ribbon_long_work_size[sizeof(effect_particle_ribbon_long_work_t) == 0x1c9c ? 1 : -1];

/* This renderer's view of one effect_record_t. It reads the target index at
 * 0x26 as a halfword, where effect_record_t declares a byte. */
typedef struct effect_particle_ribbon_record_view {
    u8 _unknown_00[0x02];
    s16 record_index; /* 0x02 */
    u8 _unknown_04[0x1e];
    u8 phase[4];      /* 0x22 */
    s16 target_index; /* 0x26: read as a halfword */
    u8 _unknown_28[0xbc];
    void* work_slots[5]; /* 0xe4 */
} effect_particle_ribbon_record_view_t;

typedef char
    assert_effect_particle_ribbon_record_view_size[sizeof(effect_particle_ribbon_record_view_t) == 0xf8 ? 1 : -1];

/* This renderer's view of one effect_geometry_entry_t. */
typedef struct effect_particle_ribbon_geometry_view {
    u8 _unknown_00[0x02];
    u16 placement_flags; /* 0x02 */
    u8 _unknown_04[0x02];
    u16 flags;                              /* 0x06 */
    u32 curve_indices[2];                   /* 0x08 */
    u32 colour_curves;                      /* 0x10 */
    effect_emitter_geometry_view_t emitter; /* 0x14: read by the battle_effect_interpolate_emitter_* helpers */
    u16 texture_page_flags;                 /* 0x4c */
    s16 brightness_row;                     /* 0x4e */
    u8 _unknown_50[0x04];
    s16 ot_depth; /* 0x54 */
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
    s16 target_spread_start[6]; /* 0x7c: x/y/z at even indices */
    s16 target_spread_end[6];   /* 0x88 */
    s16 lifetime_start[2];      /* 0x94 */
    s16 lifetime_end[2];        /* 0x98 */
    s16 target_offset_start[3]; /* 0x9c */
    s16 target_offset_end[3];   /* 0xa2 */
    s16 texture_width;          /* 0xa8 */
    s16 texture_height;         /* 0xaa */
    u8 _unknown_ac[0x04];
    s16 particle_count_start; /* 0xb0 */
    s16 particle_count_end;   /* 0xb2 */
    s16 spawn_interval_start; /* 0xb4 */
    s16 spawn_interval_end;   /* 0xb6 */
    u8 _unknown_b8[0x08];
    u8 child_emitter_on_death; /* 0xc0 */
    u8 child_emitter_mid_life; /* 0xc1 */
    u8 _unknown_c2[0x02];
} effect_particle_ribbon_geometry_view_t;

typedef char
    assert_effect_particle_ribbon_geometry_view_size[sizeof(effect_particle_ribbon_geometry_view_t) == 0xc4 ? 1 : -1];

extern u32 g_effect_e070_particle_ribbon_long_brightness_table[][EFFECT_PARTICLE_RIBBON_LONG_POINTS];
extern u32 g_effect_e403_particle_ribbon_long_brightness_table[][EFFECT_PARTICLE_RIBBON_LONG_POINTS];
extern u32 g_effect_e461_particle_ribbon_long_brightness_table[][EFFECT_PARTICLE_RIBBON_LONG_POINTS];
extern u32 g_effect_particle_ribbon_long_brightness_table[][EFFECT_PARTICLE_RIBBON_LONG_POINTS];
extern u32 g_effect_e000_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];
extern u32 g_effect_e242_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];
extern u32 g_effect_e317_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];
extern u32 g_effect_e371_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];
extern u32 g_effect_e409_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];
extern u32 g_effect_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];

#endif
