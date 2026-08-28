#ifndef FFT_EFFECT_POLAR_MESH_H
#define FFT_EFFECT_POLAR_MESH_H

/* Shared records of the 6,012-byte polar quad mesh renderer that 23 EFFECT
 * overlays compile from one source (for example E461 at 0x801c44a0). */

#include "fft/effect.h"
#include "psx/types.h"

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

#endif
