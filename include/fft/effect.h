#ifndef EFFECT_H
#define EFFECT_H

/* Native ability-effect handlers of the effect overlays (EFFECT/E###.BIN) and their work records. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Effect script opcode 06 puts a handler in one of the callback slots, and a
 * particle keyframe whose 16-bit action value names the slot binds it to the record.
 * While a keyframe that names the slot runs, the particle channel calls
 *     handler(record_index, callback_slot, emitter_index, frame)
 * once a frame in place of the particle emitter: emitter_index is the
 * keyframe's emitter record, which the handler reads through its own view,
 * and frame counts from the start of the keyframe. The slot's phase
 * (effect_record_t.callback_phase, effect_callback_phase_e) starts at INIT, where the
 * handler allocates its work block into work_slots; it then stays at UPDATE
 * until the channel sets DESTROY two frames before the keyframe ends. A handler reads its emitter
 * record through its own view of battle_effect_geometry.h. */

/* map control */
s32 effect_map_destruction_update(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_freeze(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_set_3d_objects_state_1(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_set_3d_objects_state_2(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_set_timed_transform_duration(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_split_mesh_and_set_3d_objects_state_3(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_unfreeze(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
/* Native callback at 0x801c44a0 in E000 (catalog ids 0, 24, 73, 92, 121); does nothing and leaves the result unset. */
s32 effect_noop(s32 unused_record_index, s32 unused_callback_slot, s32 unused_emitter_index, s32 unused_frame);

/* texture pages */
/* Fixed texture-page bits shared by the native effect renderers. */
enum {
    EFFECT_RENDER_TEXTURE_PAGE_BASE = 0x86,
    EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT = 5,
};

/* particle ribbon */
/* The homing ribbon renderer has short 9-point trails and long 17-point trails. */
enum {
    EFFECT_PARTICLE_RIBBON_SHORT_SLOTS = 8,
    EFFECT_PARTICLE_RIBBON_SHORT_POINTS = 9,
    EFFECT_PARTICLE_RIBBON_LONG_SLOTS = 4,
    EFFECT_PARTICLE_RIBBON_LONG_POINTS = 17,
};
/* One short ribbon: a particle and the double-buffered quad strip drawn along
 * its recent path, one quad per pair of adjacent trail points. */
typedef struct effect_particle_ribbon_short {
    SVECTOR target;                                             /* 0x000: homing destination */
    VECTOR velocity;                                            /* 0x008: 20.12 fixed */
    VECTOR position;                                            /* 0x018: 20.12 fixed */
    POLY_GT4 quads[2][EFFECT_PARTICLE_RIBBON_SHORT_POINTS - 1]; /* 0x028 */
    vec3s16_t trail[EFFECT_PARTICLE_RIBBON_SHORT_POINTS];       /* 0x368: ring buffer indexed by age */
    s16 age;                                                    /* 0x39e */
    s16 lifetime;                                               /* 0x3a0 */
    s16 active;                                                 /* 0x3a2 */
    u8 _unused_3a4[0x02];
    s16 frame;             /* 0x3a6: texture animation frame */
    s16 frame_timer;       /* 0x3a8 */
    u8 _padding_3aa[0x02]; /* tail padding to 4-byte alignment */
} effect_particle_ribbon_short_t;
typedef char assert_effect_particle_ribbon_short_size[sizeof(effect_particle_ribbon_short_t) == 940 ? 1 : -1];

/* One long ribbon: the short layout with 16 quads per buffer and a 17-point
 * trail. */
typedef struct effect_particle_ribbon_long {
    SVECTOR target;                                            /* 0x000: homing destination */
    VECTOR velocity;                                           /* 0x008: 20.12 fixed */
    VECTOR position;                                           /* 0x018: 20.12 fixed */
    POLY_GT4 quads[2][EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1]; /* 0x028 */
    vec3s16_t trail[EFFECT_PARTICLE_RIBBON_LONG_POINTS];       /* 0x6a8: ring buffer indexed by age */
    s16 age;                                                   /* 0x70e */
    s16 lifetime;                                              /* 0x710 */
    s16 active;                                                /* 0x712 */
    u8 _unused_714[0x02];
    s16 frame;             /* 0x716: texture animation frame */
    s16 frame_timer;       /* 0x718 */
    u8 _padding_71a[0x02]; /* tail padding to 4-byte alignment */
} effect_particle_ribbon_long_t;
typedef char assert_effect_particle_ribbon_long_size[sizeof(effect_particle_ribbon_long_t) == 1820 ? 1 : -1];

/* Work block allocated by the short variant. */
typedef struct effect_particle_ribbon_short_work {
    s32 buffer;                                                                 /* 0x0000 */
    s16 count;                                                                  /* 0x0004: active ribbons */
    u8 _padding_0006[0x02];                                                     /* aligns ribbons */
    effect_particle_ribbon_short_t ribbons[EFFECT_PARTICLE_RIBBON_SHORT_SLOTS]; /* 0x0008 */
    MATRIX matrix;                                                              /* 0x1d68 */
    s32 destroy_delay;                                                          /* 0x1d88 */
} effect_particle_ribbon_short_work_t;
typedef char
    assert_effect_particle_ribbon_short_work_size[sizeof(effect_particle_ribbon_short_work_t) == 7564 ? 1 : -1];

/* Work block allocated by the long variant. */
typedef struct effect_particle_ribbon_long_work {
    s32 buffer;                                                               /* 0x0000 */
    s16 count;                                                                /* 0x0004: active ribbons */
    u8 _padding_0006[0x02];                                                   /* aligns ribbons */
    effect_particle_ribbon_long_t ribbons[EFFECT_PARTICLE_RIBBON_LONG_SLOTS]; /* 0x0008 */
    MATRIX matrix;                                                            /* 0x1c78 */
    s32 destroy_delay;                                                        /* 0x1c98 */
} effect_particle_ribbon_long_work_t;
typedef char assert_effect_particle_ribbon_long_work_size[sizeof(effect_particle_ribbon_long_work_t) == 7324 ? 1 : -1];

extern u32 g_effect_particle_ribbon_long_brightness_table[][EFFECT_PARTICLE_RIBBON_LONG_POINTS];
extern u32 g_effect_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];

s32 effect_particle_ribbon_long_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_particle_ribbon_short_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* polar mesh */
enum {
    POLAR_MESH_QUADRANT_COUNT = 4,
    POLAR_MESH_RING_COUNT = 5,
    POLAR_MESH_SEGMENTS_PER_QUADRANT = 4,
    POLAR_MESH_QUADS_PER_QUADRANT = POLAR_MESH_RING_COUNT * POLAR_MESH_SEGMENTS_PER_QUADRANT,
    POLAR_MESH_QUAD_COUNT = POLAR_MESH_QUADRANT_COUNT * POLAR_MESH_QUADS_PER_QUADRANT,
    POLAR_MESH_OUTER_RING_OFFSET = (POLAR_MESH_RING_COUNT - 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT,
    POLAR_MESH_ANGLE_STEP = ONE / (POLAR_MESH_QUADRANT_COUNT * POLAR_MESH_SEGMENTS_PER_QUADRANT),
};
/* Scratchpad (0x1f800000) layout used while one polar mesh is rebuilt.
 * Each row sweeps one quadrant in four steps; its last point closes the
 * quadrant by rotating the first point a quarter turn. The renderer never
 * touches the unknown ranges. */
typedef struct effect_polar_mesh_scratch {
    s32 red_factor;   /* 0x00: color curve factors, 128 when curves are off */
    s32 green_factor; /* 0x04 */
    s32 blue_factor;  /* 0x08 */
    u8 _unused_00c[0x04];
    u32 rgb;  /* 0x10: packed 32-bit color value of the current ring */
    u32 code; /* 0x14: primitive code byte of the first quad, kept in bits 24-31 */
    u8 u0;    /* 0x18 */
    u8 u1;    /* 0x19 */
    u8 v0;    /* 0x1a */
    u8 v1;    /* 0x1b */
    u8 _unused_01c[0x20];
    s32 translucent;       /* 0x3c */
    u16 tpage;             /* 0x40 */
    u8 _padding_042[0x02]; /* aligns half_width */
    s32 half_width;        /* 0x44 */
    s32 half_height;       /* 0x48 */
    s32 brightness_row;    /* 0x4c */
    u8 _unused_050[0x0c];
    s32 velocity_angle;     /* 0x5c */
    s32 acceleration_angle; /* 0x60 */
    VECTOR center;          /* 0x64: interpolated emitter spawn position; z is written but unused */
    u8 _unused_074[0x30];
    point32_t velocity; /* 0xa4: center drift per ring, 20.12 fixed */
    u8 _unused_0ac[0x08];
    point32_t acceleration; /* 0xb4 */
    u8 _unused_0bc[0x1c];
    s32 radius;                                                                        /* 0xd8: 20.12 fixed */
    s32 radius_step;                                                                   /* 0xdc */
    s32 radius_step_delta;                                                             /* 0xe0 */
    s32 angle;                                                                         /* 0xe4 */
    s32 angle_step;                                                                    /* 0xe8 */
    s32 angle_step_delta;                                                              /* 0xec */
    s32 ot_depth;                                                                      /* 0xf0: clamped to 4..0x17f */
    point16_t points[POLAR_MESH_RING_COUNT + 1][POLAR_MESH_SEGMENTS_PER_QUADRANT + 1]; /* 0xf4 */
} effect_polar_mesh_scratch_t;

/* Scratchpad (0x1f800000) layout of the Zodiac Stone glow variant. Its fields shift from
 * effect_polar_mesh_scratch_t, so it is a separate layout, not a view. */
typedef struct effect_anchored_polar_mesh_scratch {
    s32 red_factor;   /* 0x00: color curve factors, 128 when curves are off */
    s32 green_factor; /* 0x04 */
    s32 blue_factor;  /* 0x08 */
    u8 r;             /* 0x0c: color bytes of the current ring */
    u8 _unused_00d;
    u8 g; /* 0x0e */
    u8 _unused_00f;
    u8 b; /* 0x10 */
    u8 _unused_011;
    u8 u0; /* 0x12 */
    u8 u1; /* 0x13 */
    u8 v0; /* 0x14 */
    u8 v1; /* 0x15 */
    u8 _unused_016[0x22];
    s32 translucent;       /* 0x38 */
    u16 tpage;             /* 0x3c */
    u8 _padding_03e[0x02]; /* aligns half_width */
    s32 half_width;        /* 0x40 */
    s32 half_height;       /* 0x44 */
    s32 brightness_row;    /* 0x48 */
    u8 _unused_04c[0x0c];
    s32 velocity_angle;     /* 0x58 */
    s32 acceleration_angle; /* 0x5c */
    VECTOR center;          /* 0x60: interpolated emitter spawn position */
    u8 _unused_070[0x30];
    point32_t velocity; /* 0xa0 */
    u8 _unused_0a8[0x08];
    point32_t acceleration; /* 0xb0 */
    u8 _unused_0b8[0x18];
    s32 target_index; /* 0xd0 */
    u8 _unused_0d4[0x14];
    s32 radius;            /* 0xe8 */
    s32 radius_step;       /* 0xec */
    s32 radius_step_delta; /* 0xf0 */
    s32 angle;             /* 0xf4 */
    s32 angle_step;        /* 0xf8 */
    s32 angle_step_delta;  /* 0xfc */
    s32 ot_depth;          /* 0x100 */
    SVECTOR position;      /* 0x104 */
    SVECTOR* unit_coords;  /* 0x10c */
    VECTOR screen;         /* 0x110; RotTrans stores flags in the unused fourth 32-bit value */
    point16_t points[POLAR_MESH_RING_COUNT + 1][POLAR_MESH_SEGMENTS_PER_QUADRANT + 1]; /* 0x120 */
} effect_anchored_polar_mesh_scratch_t;

/* Work block: two POLY_GT4 buffers of four quadrants of 5x4 quads. */
typedef struct effect_polar_mesh_work {
    s32 buffer;                               /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][POLAR_MESH_QUAD_COUNT]; /* 0x0004: quadrant * 20 + ring * 4 + segment */
    u8 _unused_2084[0x60];
    s32 scroll_u; /* 0x20e4: 20.12 fixed, wrapped to half_width */
    s32 scroll_v; /* 0x20e8: 20.12 fixed, wrapped to half_height */
    u8 _unused_20ec[0x04];
    s16 base_u;        /* 0x20f0 */
    s16 base_v;        /* 0x20f2 */
    s32 frame;         /* 0x20f4: curve column, advanced once per update */
    s32 destroy_delay; /* 0x20f8: the destroy phase waits one call before freeing */
} effect_polar_mesh_work_t;
typedef char assert_effect_polar_mesh_work_size[sizeof(effect_polar_mesh_work_t) == 8444 ? 1 : -1];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the ring (0-5) the column. */
extern u32 g_effect_anchored_polar_mesh_brightness_table[][6];
extern u32 g_effect_polar_mesh_brightness_table[][6];

s32 effect_anchored_polar_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_polar_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* quad grid */
/* E230 links a second, 4x4 quad grid after its 8x8 one: vertex brightness rows (12-bit fixed) of that copy,
 * indexed by brightness setting and quad row edge. The shared renderer declares its own table, whose width
 * follows the build's grid size. */
extern u32 g_effect_textured_quad_grid_4x4_brightness_table[][5];

s32 effect_textured_quad_grid_2x2_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_textured_quad_grid_4x4_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_textured_quad_grid_8x8_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* ring mesh */
enum {
    RING_MESH_SEGMENTS = 32, /* quads around one ring */
    RING_MESH_QUADRANT_COUNT = 4,
    RING_MESH_SEGMENTS_PER_QUADRANT = RING_MESH_SEGMENTS / RING_MESH_QUADRANT_COUNT,
    RING_MESH_ARC_POINTS = RING_MESH_SEGMENTS_PER_QUADRANT + 1,
};
/* Work block: two POLY_GT4 buffers of two rings of 32 quads. */
typedef struct effect_ring_mesh_work {
    s32 buffer;                                /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][RING_MESH_SEGMENTS * 2]; /* 0x0004: ring * 32 + segment */
    s16 spin;                                  /* 0x1a04: ring rotation, modulo one turn */
    s16 v_scroll;                              /* 0x1a06: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;                         /* 0x1a08: 24.8 fixed, added to the radius */
    s32 radius_speed;                          /* 0x1a0c */
    u8 _unused_1a10[0x08];
    s32 destroy_delay; /* 0x1a18: the destroy phase waits one call before freeing */
} effect_ring_mesh_work_t;
typedef char assert_effect_ring_mesh_work_size[sizeof(effect_ring_mesh_work_t) == 6684 ? 1 : -1];

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

/* Work block of the single-ring variant: two POLY_GT4 buffers of 32 quads. */
typedef struct effect_single_ring_mesh_work {
    s32 buffer;                            /* 0x000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][RING_MESH_SEGMENTS]; /* 0x004 */
    s16 spin;                              /* 0xd04: ring rotation, modulo one turn */
    s16 v_scroll;                          /* 0xd06: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;                     /* 0xd08: 24.8 fixed, added to the radius */
    s32 radius_speed;                      /* 0xd0c */
    u8 _unused_d10[0x08];
    s32 destroy_delay; /* 0xd18: the destroy phase waits one call before freeing */
} effect_single_ring_mesh_work_t;
typedef char assert_effect_single_ring_mesh_work_size[sizeof(effect_single_ring_mesh_work_t) == 3356 ? 1 : -1];

/* Scratchpad (0x1f800000) layout of the single-ring variant: two lattice
 * rows of one quadrant. */
typedef struct effect_single_ring_mesh_scratch {
    VECTOR points[2][RING_MESH_ARC_POINTS]; /* 0x000 */
} effect_single_ring_mesh_scratch_t;

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-2) the column. */
extern u32 g_effect_ring_mesh_brightness_table[][3];
/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_ring_stack_mesh_brightness_table[][9];
/* Overlay-local vertex brightness rows (12-bit fixed). */
extern u32 g_effect_single_ring_mesh_brightness_table[][2];
/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_tube_mesh_brightness_table[][9];
/* Overlay-local ring scale per lattice row pair (12-bit fixed): row 4 - i and
 * row 4 + i share g_effect_tube_mesh_ring_scales[i]. */
extern s32 g_effect_tube_mesh_ring_scales[5];

s32 effect_ring_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_ring_stack_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_single_ring_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_tube_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* dome mesh */
/* Lattice of the spinning dome renderer: eight rows of sixteen segments. */
enum {
    DOME_MESH_ROWS = 8,
    DOME_MESH_SEGMENTS = 16,
    DOME_MESH_LATTICE_COMPONENTS = 3,
    DOME_MESH_QUAD_COUNT = DOME_MESH_ROWS * DOME_MESH_SEGMENTS,
    DOME_MESH_LATTICE_POINT_BYTES = sizeof(vec3s16_t),
    DOME_MESH_LATTICE_ROW_BYTES = (DOME_MESH_SEGMENTS + 1) * DOME_MESH_LATTICE_POINT_BYTES,
};
/* Work block: two POLY_GT4 buffers of 8 rows x 16 columns. */
typedef struct effect_dome_mesh_work {
    s32 buffer;                              /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][DOME_MESH_QUAD_COUNT]; /* 0x0004: row * 16 + column */
    s16 spin;                                /* 0x3404: lattice rotation, modulo one turn */
    s16 v_scroll;                            /* 0x3406: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;                       /* 0x3408: 24.8 fixed, added to the radius */
    s32 radius_speed;                        /* 0x340c */
    u8 _unused_3410[0x08];
    s32 destroy_delay; /* 0x3418: the destroy phase waits one call before freeing */
} effect_dome_mesh_work_t;
typedef char assert_effect_dome_mesh_work_size[sizeof(effect_dome_mesh_work_t) == 13340 ? 1 : -1];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_dome_mesh_brightness_table[][9];

s32 effect_dome_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* map vertex particles */
typedef struct effect_map_vertex_location {
    u8 vertex_index;    /* one of the mesh part's four cached vertices */
    u8 mesh_part_index; /* zero-based among map mesh parts 1-8 */
} effect_map_vertex_location_t;

/* Eight animated map mesh parts each provide four cached vertices. */
enum {
    EFFECT_PARTICLE_VERTEX_EMITTER_COUNT = 16,
    EFFECT_MAP_VERTEX_PART_COUNT = 8,
    EFFECT_MAP_VERTEX_COUNT_PER_PART = 4,
    EFFECT_MAP_VERTEX_WORK_BYTES = EFFECT_MAP_VERTEX_PART_COUNT * EFFECT_MAP_VERTEX_COUNT_PER_PART * sizeof(VECTOR),
};

extern effect_map_vertex_location_t g_effect_altima_transformation_vertex_locations[3];
extern effect_map_vertex_location_t g_effect_banish_vertex_locations[12];

s32 effect_altima_transformation_map_vertex_particles_update(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 frame);
s32 effect_banish_map_vertex_particles_update(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 frame);

/* tentacle mesh */
/* The tentacle renderer draws textured strips; the related particle-trail
 * renderer tracks points without drawing the strips. Both use the same emitter view. */

typedef struct effect_tentacle_mesh_segment {
    SVECTOR tip;          /* 0x000 */
    VECTOR velocity;      /* 0x008: 20.12 fixed */
    VECTOR position;      /* 0x018: 20.12 fixed */
    POLY_GT4 quads[4][8]; /* 0x028: sets buffer and buffer + 2 */
    vec3s16_t points[9];  /* 0x6a8 */
    s16 head;             /* 0x6de */
    s16 length;           /* 0x6e0 */
    s16 active;           /* 0x6e2 */
    u8 _unused_6e4[0x02];
    s16 frame;             /* 0x6e6: texture animation frame */
    s16 frame_timer;       /* 0x6e8 */
    u8 _padding_6ea[0x02]; /* tail padding to 4-byte alignment */
} effect_tentacle_mesh_segment_t;
typedef char assert_effect_tentacle_mesh_segment_size[sizeof(effect_tentacle_mesh_segment_t) == 1772 ? 1 : -1];

typedef struct effect_tentacle_mesh_work {
    s32 buffer;                                 /* 0x0000 */
    s16 count;                                  /* 0x0004 */
    u8 _padding_0006[0x02];                     /* aligns segments */
    effect_tentacle_mesh_segment_t segments[8]; /* 0x0008 */
    MATRIX matrix;                              /* 0x3768 */
    s32 destroy_delay;                          /* 0x3788 */
} effect_tentacle_mesh_work_t;
typedef char assert_effect_tentacle_mesh_work_size[sizeof(effect_tentacle_mesh_work_t) == 14220 ? 1 : -1];

/* A 17-point particle trail; spawning seeds only points 0-8 (the target's loop bound is 9). */
typedef struct effect_trail_segment {
    SVECTOR tip;          /* 0x00 */
    VECTOR velocity;      /* 0x08: 20.12 fixed */
    VECTOR position;      /* 0x18: 20.12 fixed */
    vec3s16_t points[17]; /* 0x28 */
    s16 head;             /* 0x8e */
    s16 length;           /* 0x90 */
    s16 active;           /* 0x92 */
    u8 _unused_94[0x04];
} effect_trail_segment_t;
typedef char assert_effect_trail_segment_size[sizeof(effect_trail_segment_t) == 152 ? 1 : -1];

typedef struct effect_trail_work {
    s32 buffer;                         /* 0x000 */
    s16 count;                          /* 0x004 */
    u8 _padding_006[0x02];              /* aligns segments */
    effect_trail_segment_t segments[4]; /* 0x008 */
    MATRIX matrix;                      /* 0x268 */
    s32 destroy_delay;                  /* 0x288 */
} effect_trail_work_t;
typedef char assert_effect_trail_work_size[sizeof(effect_trail_work_t) == 652 ? 1 : -1];

extern u32 g_effect_tentacle_mesh_brightness_table[][9];

s32 effect_particle_trails_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_tentacle_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* wave mesh */
/* Work block of the 8x8 wave mesh, allocated in phase 1. The four wave phases
 * are 4096-per-turn angles; the emitter record's spawn spread (x, y) and
 * wave_phase_speed curves advance them once per update. */
typedef struct effect_wave_mesh_work {
    s32 buffer;            /* 0x0000: quads[buffer] is drawn this frame */
    POLY_GT4 quads[2][64]; /* 0x0004: row-major 8x8 grid, double buffered */
    s32 wave_y_phase;      /* 0x1a04: sine phase of the vertical wave at point (0, 0) */
    u8 _unused_1a08[0x0c];
    s32 wave_y_amplitude_phase; /* 0x1a14: cosine phase scaling row 0's vertical amplitude */
    u8 _unused_1a18[0x0c];
    s32 wave_x_phase; /* 0x1a24: sine phase of the horizontal wave at point (0, 0) */
    u8 _unused_1a28[0x0c];
    s32 wave_x_amplitude_phase; /* 0x1a34: cosine phase scaling column 0's horizontal amplitude */
    u8 _unused_1a38[0x0c];
    s32 scroll_u; /* 0x1a44 */
    s32 scroll_v; /* 0x1a48 */
    u8 _unused_1a4c[4];
    u16 texture_u;       /* 0x1a50 */
    u16 texture_v;       /* 0x1a52 */
    s32 frame;           /* 0x1a54 */
    s32 destroy_latched; /* 0x1a58 */
} effect_wave_mesh_work_t;
typedef char assert_effect_wave_mesh_work_size[sizeof(effect_wave_mesh_work_t) == 6748 ? 1 : -1];

/* Scratchpad (0x1f800000) layout while the mesh is rebuilt. */
typedef struct effect_wave_mesh_scratch {
    point16_t points[9][9]; /* 0x000: wave-displaced lattice */
} effect_wave_mesh_scratch_t;

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the quad row (0-8) the column. */
extern u32 g_effect_wave_mesh_brightness_table[][9];

s32 effect_wave_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

#endif
