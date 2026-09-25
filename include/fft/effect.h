#ifndef FFT_EFFECT_H
#define FFT_EFFECT_H

/* EFFECT/E*.BIN: ability effect overlays that run with BATTLE. */

#include "fft/battle.h"

struct effect_timeline_header;

/* helpers */
/* Reads curve `nibble` (0 = none, otherwise palette row + 1) at `frame`. The
 * separate decrement keeps the target's compare against -1. */
#define READ_CURVE(result, index, nibble, frame)                                                                       \
    {                                                                                                                  \
        index = (nibble);                                                                                              \
        index--;                                                                                                       \
        if (index != -1) {                                                                                             \
            result = g_effect_palette_table[index].factor[frame];                                                      \
        } else {                                                                                                       \
            result = 0;                                                                                                \
        }                                                                                                              \
    }

/* Scales a scratch record's colour by a 4.12 brightness and packs it with its
 * primitive code into one RGB-code word. */
#define PACK_RGB(scratch, brightness)                                                                                  \
    ((((brightness) * (scratch)->colour[0]) >> 12) | (scratch)->code                                                   \
        | ((((brightness) * (scratch)->colour[1]) >> 12) << 8)                                                         \
        | ((((brightness) * (scratch)->colour[2]) >> 12) << 16))

/* The RGB-code word of vertex `n` of a primitive. */
#define POLY_RGB(poly, n) (*(u32*)&(poly)->r##n)

/* A random value in [-|x|, |x|). */
#define RAND_SPREAD(x) ((x) == -(x) ? (x) : (-(x) < (x) ? rand() % ((x) * 2) - (x) : rand() % (-(x) - (x)) + (x)))

/* A random value between `a` and `b`, in either order. */
#define RAND_BETWEEN(a, b) ((a) == (b) ? (a) : ((b) < (a) ? (b) + rand() % ((a) - (b)) : (a) + rand() % ((b) - (a))))

/* particle ribbon */
/* Shared records of the homing particle-ribbon renderer that 16 EFFECT
 * overlays compile from one source (for example E000 at 0x801c2500). It comes
 * in two sizes: the short variant keeps 8 ribbons with 9-point trails (E000,
 * E242, E317, E336, E355, E371, E402, E409) and the long variant 4 ribbons
 * with 17-point trails (E070, E403, E412, E461, E462, E463, E465, E466). */
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

/* polar mesh */
/* Shared records of the 6,012-byte polar quad mesh renderer that 23 EFFECT
 * overlays compile from one source (for example E461 at 0x801c44a0). */
/* One vertex of the scratch polar grid, relative to the mesh centre. */
typedef struct effect_polar_mesh_point {
    s16 x;
    s16 y;
} effect_polar_mesh_point_t;

/* Scratchpad (0x1f800000) layout used while one polar mesh is rebuilt.
 * points[ring][0..3] sweep one quadrant in 0x100 steps; points[ring][4] is
 * points[ring][0] rotated by 0x400, closing the quadrant. The renderer never
 * touches the unknown ranges. */
typedef struct effect_polar_mesh_scratch {
    s32 colour[3]; /* 0x00: r/g/b curve factors, 0x80 when colour curves are off */
    u8 _unknown_0c[0x04];
    u32 rgb;  /* 0x10: packed colour word of the current ring */
    u32 code; /* 0x14: primitive code byte of the first quad, kept in bits 24-31 */
    u8 u0;    /* 0x18 */
    u8 u1;    /* 0x19 */
    u8 v0;    /* 0x1a */
    u8 v1;    /* 0x1b */
    u8 _unknown_1c[0x20];
    s32 translucent; /* 0x3c */
    u16 tpage;       /* 0x40 */
    u8 _unknown_42[0x02];
    s32 half_width;     /* 0x44 */
    s32 half_height;    /* 0x48 */
    s32 brightness_row; /* 0x4c */
    u8 _unknown_50[0x0c];
    s32 velocity_angle;     /* 0x5c */
    s32 acceleration_angle; /* 0x60 */
    s32 center[3];          /* 0x64: interpolated emitter spawn position; z is written but unused */
    u8 _unknown_70[0x34];
    s32 velocity[2]; /* 0xa4: centre drift per ring, 20.12 fixed */
    u8 _unknown_ac[0x08];
    s32 acceleration[2]; /* 0xb4 */
    u8 _unknown_bc[0x1c];
    s32 radius;                             /* 0xd8: 20.12 fixed */
    s32 radius_step;                        /* 0xdc */
    s32 radius_step_delta;                  /* 0xe0 */
    s32 angle;                              /* 0xe4 */
    s32 angle_step;                         /* 0xe8 */
    s32 angle_step_delta;                   /* 0xec */
    s32 ot_depth;                           /* 0xf0: clamped to 4..0x17f */
    effect_polar_mesh_point_t points[6][5]; /* 0xf4 */
} effect_polar_mesh_scratch_t;

/* Scratchpad (0x1f800000) layout of the unit-anchored variant (the 6,492-byte
 * renderer in E467, E468, E469, E471 and E472). Its fields shift from
 * effect_polar_mesh_scratch_t, so it is a separate layout, not a view. */
typedef struct effect_anchored_polar_mesh_scratch {
    s32 colour[3]; /* 0x00: r/g/b curve factors, 0x80 when colour curves are off */
    u8 r;          /* 0x0c: colour bytes of the current ring */
    u8 _unknown_0d;
    u8 g; /* 0x0e */
    u8 _unknown_0f;
    u8 b; /* 0x10 */
    u8 _unknown_11;
    u8 u0; /* 0x12 */
    u8 u1; /* 0x13 */
    u8 v0; /* 0x14 */
    u8 v1; /* 0x15 */
    u8 _unknown_16[0x22];
    s32 translucent; /* 0x38 */
    u16 tpage;       /* 0x3c */
    u8 _unknown_3e[0x02];
    s32 half_width;     /* 0x40 */
    s32 half_height;    /* 0x44 */
    s32 brightness_row; /* 0x48 */
    u8 _unknown_4c[0x0c];
    s32 velocity_angle;     /* 0x58 */
    s32 acceleration_angle; /* 0x5c */
    s32 center[3];          /* 0x60: interpolated emitter spawn position */
    u8 _unknown_6c[0x34];
    s32 velocity[2]; /* 0xa0 */
    u8 _unknown_a8[0x08];
    s32 acceleration[2]; /* 0xb0 */
    u8 _unknown_b8[0x18];
    s32 target_index; /* 0xd0 */
    u8 _unknown_d4[0x14];
    s32 radius;                             /* 0xe8 */
    s32 radius_step;                        /* 0xec */
    s32 radius_step_delta;                  /* 0xf0 */
    s32 angle;                              /* 0xf4 */
    s32 angle_step;                         /* 0xf8 */
    s32 angle_step_delta;                   /* 0xfc */
    s32 ot_depth;                           /* 0x100 */
    SVECTOR position;                       /* 0x104 */
    s16* unit_coords;                       /* 0x10c */
    s32 screen[3];                          /* 0x110 */
    long flag;                              /* 0x11c */
    effect_polar_mesh_point_t points[6][5]; /* 0x120 */
} effect_anchored_polar_mesh_scratch_t;

/* Work block: two POLY_GT4 buffers of four quadrants of 5x4 quads. */
typedef struct effect_polar_mesh_work {
    s32 buffer;            /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][80]; /* 0x0004: quadrant * 20 + ring * 4 + segment */
    u8 _unknown_2084[0x60];
    s32 scroll_u; /* 0x20e4: 20.12 fixed, wrapped to half_width */
    s32 scroll_v; /* 0x20e8: 20.12 fixed, wrapped to half_height */
    u8 _unknown_20ec[0x04];
    s16 base_u;        /* 0x20f0 */
    s16 base_v;        /* 0x20f2 */
    s32 frame;         /* 0x20f4: curve column, advanced once per update */
    s32 destroy_delay; /* 0x20f8: the destroy phase waits one call before freeing */
} effect_polar_mesh_work_t;
typedef char assert_effect_polar_mesh_work_size[sizeof(effect_polar_mesh_work_t) == 0x20fc ? 1 : -1];

/* This renderer's view of one effect_geometry_entry_t. It keeps the particle
 * emitter's slot-to-curve bindings but gives the slots its own meanings; each
 * field notes the emitter slot it occupies, and offsets it never reads stay
 * unknown. Curve nibbles: curve_indices[0] 0 centre, 1 radius, 2 velocity,
 * 3 acceleration, 4 angle, 7 radius step; curve_indices[1] 0 radius step
 * delta, 1 scroll, 3 angle step, 6 angle step delta. */
typedef struct effect_polar_mesh_geometry_view {
    u8 _unknown_00[0x06];
    u16 flags;            /* 0x06: bit 6 = per-channel colour curves */
    u32 curve_indices[2]; /* 0x08 */
    u32 colour_curves;    /* 0x10: nibbles 0-2 = r/g/b */
    s16 center_start[3];  /* 0x14: spawn position, read by battle_effect_interpolate_emitter_spawn_position */
    s16 center_end[3];    /* 0x1a */
    s16 radius_start;     /* 0x20: spawn spread x */
    u8 _unknown_22[0x04];
    s16 radius_end; /* 0x26 */
    u8 _unknown_28[0x04];
    s16 velocity_angle_start; /* 0x2c: velocity base angle x */
    s16 velocity_speed_start; /* 0x2e: velocity base angle y */
    u8 _unknown_30[0x02];
    s16 velocity_angle_end; /* 0x32 */
    s16 velocity_speed_end; /* 0x34 */
    u8 _unknown_36[0x02];
    s16 acceleration_angle_start; /* 0x38: velocity spread x */
    s16 acceleration_speed_start; /* 0x3a: velocity spread y */
    u8 _unknown_3c[0x02];
    s16 acceleration_angle_end; /* 0x3e */
    s16 acceleration_speed_end; /* 0x40 */
    u8 _unknown_42[0x02];
    s16 angle_start; /* 0x44: minimum inertia */
    u8 _unknown_46[0x02];
    s16 angle_end; /* 0x48 */
    u8 _unknown_4a[0x02];
    u16 texture_page_flags; /* 0x4c: bits 0-1 tpage x, bit 2 clears ABE */
    s16 brightness_row;     /* 0x4e */
    u8 _unknown_50[0x04];
    s16 ot_depth; /* 0x54 */
    u8 _unknown_56[0x06];
    s16 radius_step_start; /* 0x5c: minimum radial speed */
    u8 _unknown_5e[0x02];
    s16 radius_step_end; /* 0x60 */
    u8 _unknown_62[0x02];
    s16 radius_step_delta_start; /* 0x64: minimum x acceleration */
    u8 _unknown_66[0x0a];
    s16 radius_step_delta_end; /* 0x70 */
    u8 _unknown_72[0x0a];
    s16 scroll_u_speed_start; /* 0x7c: minimum x drag */
    u8 _unknown_7e[0x02];
    s16 scroll_v_speed_start; /* 0x80: minimum y drag */
    u8 _unknown_82[0x06];
    s16 scroll_u_speed_end; /* 0x88 */
    u8 _unknown_8a[0x02];
    s16 scroll_v_speed_end; /* 0x8c */
    u8 _unknown_8e[0x0e];
    s16 angle_step_start; /* 0x9c: target offset x */
    u8 _unknown_9e[0x04];
    s16 angle_step_end; /* 0xa2 */
    u8 _unknown_a4[0x04];
    s16 texture_width;  /* 0xa8 */
    s16 texture_height; /* 0xaa */
    u8 _unknown_ac[0x08];
    s16 angle_step_delta_start; /* 0xb4: spawn interval */
    s16 angle_step_delta_end;   /* 0xb6 */
    u16 texture_u;              /* 0xb8: minimum start homing strength */
    u16 texture_v;              /* 0xba: maximum start homing strength */
    u8 _unknown_bc[0x08];
} effect_polar_mesh_geometry_view_t;
typedef char assert_effect_polar_mesh_geometry_view_size[sizeof(effect_polar_mesh_geometry_view_t) == 0xc4 ? 1 : -1];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the ring (0-5) the column. */
extern u32 g_effect_anchored_polar_mesh_brightness_table[][6];
extern u32 g_effect_e467_anchored_polar_mesh_brightness_table[][6];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the ring (0-5) the column. */
extern u32 g_effect_e004_polar_mesh_brightness_table[][6];
extern u32 g_effect_e033_polar_mesh_brightness_table[][6];
extern u32 g_effect_e065_polar_mesh_brightness_table[][6];
extern u32 g_effect_e070_polar_mesh_brightness_table[][6];
extern u32 g_effect_e077_polar_mesh_brightness_table[][6];
extern u32 g_effect_e241_polar_mesh_brightness_table[][6];
extern u32 g_effect_e242_polar_mesh_brightness_table[][6];
extern u32 g_effect_e245_polar_mesh_brightness_table[][6];
extern u32 g_effect_e384_polar_mesh_brightness_table[][6];
extern u32 g_effect_e409_polar_mesh_brightness_table[][6];
extern u32 g_effect_e453_polar_mesh_brightness_table[][6];
extern u32 g_effect_e454_polar_mesh_brightness_table[][6];
extern u32 g_effect_e461_polar_mesh_brightness_table[][6];
extern u32 g_effect_polar_mesh_brightness_table[][6];

/* quad grid */
/* Work blocks of the scrolling textured quad grid renderer that EFFECT
 * overlays compile from one source in 2x2, 4x4 and 8x8 builds (for example
 * effect_e047_update_textured_quad_grid_4x4 in E047). Each block is allocated in
 * phase 1 and freed in phase 3. The 2x2 and 4x4 builds leave a 0x48-byte gap
 * after the quads that the 8x8 build does not have. */
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

/* ring mesh */
/* Shared records of the 5,584-byte ring mesh renderer that 32 EFFECT overlays
 * compile from one source (for example E450 at 0x801c2d08). */
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

/* summon mesh */
/* Shared records of the 4,744-byte summon dome mesh renderer that 12 EFFECT
 * overlays compile from one source (for example E474 at 0x801c42c4). */
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

/* system */
/* Shared layout of the EFFECT/E###.BIN overlay state handlers.
 *
 * Every native overlay routine is a state handler
 *     handler(s16 record_index, s32 byte_offset, s32 entry_index, s32 frame)
 * over one 0xf8-byte record of g_effect_state_records. The state block
 * addressed by byte_offset carries the phase byte (1 init, 2 update, 3 destroy);
 * the record's work-slot table at +0xe4 is indexed by the same byte_offset.
 * Field names are provisional: only offsets proven by matched code are named. */
typedef struct effect_state {
    u8 _unknown_00[0x22];
    u8 phase; /* 0x22: 1 init, 2 update, 3 destroy, 0 idle */
} effect_state_t;

extern u8 g_effect_e454_particle_work_offset_scale_16;
extern u8 g_effect_e454_particle_work_offset_scale_64;
extern u8 g_effect_e464_particle_work_offset_scale_16;
extern u8 g_effect_e464_particle_work_offset_scale_64;

/* tentacle mesh */
/* Shared records of two emitter-driven EFFECT renderers compiled from one
 * source family: the 8,668-byte tentacle mesh (E376, E381, E382, E384, E401
 * and E453) that draws up to eight textured strips, and the 5,624-byte particle
 * trail (E077, E161, E451 and E485) that tracks up to four point trails. Both
 * read their effect_geometry_entry_t through
 * effect_tentacle_mesh_geometry_view_t. */
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

/* wave mesh */
/* Shared records of the 2,924-byte 8x8 wave-distorted quad mesh renderer that
 * eight EFFECT overlays compile from one source (for example E033 at
 * 0x801c3c7c). */
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

/* other */
/* The E454 and E464 particle handlers use the renderer-specific meanings of
 * the geometry entry's 0x4c and 0xa8 fields. */
typedef struct effect_particle_vertex_emitter_view {
    u8 _unknown_00[0x4c];
    s16 vertex_group; /* 0x4c: selects a vertex in the effect work buffer */
    u8 _unknown_4e[0x5a];
    s16 start_frame; /* 0xa8 */
    s16 end_frame;   /* 0xaa */
    u8 _unknown_ac[0x18];
} effect_particle_vertex_emitter_view_t;
typedef char
    assert_effect_particle_vertex_emitter_view_size[sizeof(effect_particle_vertex_emitter_view_t) == 0xc4 ? 1 : -1];

/* Low halfwords of a VECTOR's components, as effect_add_scratchpad_xyz_components
 * reads them (lhu at +0, +4 and +8). */
typedef struct effect_wide_vector {
    u16 x;
    u16 pad_x;
    u16 y;
    u16 pad_y;
    u16 z;
} effect_wide_vector_t;

/* Overlay-local vertex brightness rows (12-bit fixed) for the 4x4 grid-mesh
 * family: brightness_row selects the row, the column is the quad row. */
extern u32 g_effect_gfx_brightness_table[][5];
extern struct effect_timeline_header* g_effect_timeline_channel_base;
void effect_add_scratchpad_xyz_components(s32 group, s32 point, const VECTOR* offset, SVECTOR* output);

#endif
