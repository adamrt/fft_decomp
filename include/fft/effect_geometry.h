#ifndef EFFECT_GEOMETRY_H
#define EFFECT_GEOMETRY_H

/* Emitter records of the effect particle-system section and each native renderer's view of them. */

#include "fft/gfx.h"
#include "psx/types.h"

/* placement */
/* Emitter spawn origin: bits 9-11 of an emitter's 16-bit placement flags (byte 0x03
 * mask 0x0e). 0xa00 is the map center:
 * every handler adds half the map geometry width and depth there. */
typedef enum effect_emitter_origin {
    EFFECT_EMITTER_ORIGIN_WORLD = 0x000,
    EFFECT_EMITTER_ORIGIN_CURSOR_TILE = 0x200, /* center of g_effect_context.target_tile */
    EFFECT_EMITTER_ORIGIN_CASTER = 0x400,
    EFFECT_EMITTER_ORIGIN_TARGET = 0x600,
    EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE = 0x800, /* position of the spawning work record */
    EFFECT_EMITTER_ORIGIN_MAP_CENTER = 0xa00,
    EFFECT_EMITTER_ORIGIN_TRACKED_ENTITY = 0xc00 /* target plus its unit coordinates */
} effect_emitter_origin_e;

enum { EFFECT_EMITTER_ORIGIN_MASK = 0xe00 };

/* Point particles home toward: bits 5-7 of the 16-bit placement flags (byte 0x02 mask
 * 0xe0). 0x00 and 0x20 both leave the
 * interpolated offset unchanged; 0x40 is the map center. */
typedef enum effect_emitter_homing {
    EFFECT_EMITTER_HOMING_OFFSET = 0x00,
    EFFECT_EMITTER_HOMING_OFFSET_ALT = 0x20,
    EFFECT_EMITTER_HOMING_MAP_CENTER = 0x40,
    EFFECT_EMITTER_HOMING_CASTER = 0x60,
    EFFECT_EMITTER_HOMING_TARGET = 0x80,
    EFFECT_EMITTER_HOMING_CURSOR_TILE = 0xa0
} effect_emitter_homing_e;

enum { EFFECT_EMITTER_HOMING_MASK = 0xe0 };

/* The 16-bit flags at 0x02 of an emitter record (effect_geometry_entry_t.placement_flags), copied to each
 * particle's kind_flags. Bits 0x0e0 are effect_emitter_homing_e and bits 0xe00
 * effect_emitter_origin_e. The work kind says what a particle's data is: a sprite node placed one
 * of the four EFFECT_LIST_NODE_* ways, or a heap block. Retail emitters use SPRITE_WORLD and
 * SPRITE_WORLD_UNSCALED only. */
enum {
    EFFECT_EMITTER_ORIENT_TO_VELOCITY = 0x0002, /* turn the sprite along its heading on screen */
    EFFECT_EMITTER_BOX_SPREAD = 0x0100,         /* spawn inside the spread box; clear, on the spread ellipsoid */
    EFFECT_WORK_KIND_MASK = 0xf000
};

/* The work-record payload selected by the high nibble of placement_flags. */
typedef enum effect_work_kind {
    EFFECT_WORK_KIND_SPRITE_WORLD = 0x0000,
    EFFECT_WORK_KIND_SPRITE_SCREEN_OFFSET = 0x1000,
    EFFECT_WORK_KIND_SPRITE_WORLD_UNSCALED = 0x2000,
    EFFECT_WORK_KIND_SPRITE_SCREEN = 0x3000,
    EFFECT_WORK_KIND_HEAP_BLOCK = 0x4000
} effect_work_kind_e;

/* motion flags */
/* Initial particle velocity mode: the velocity_inward bit (byte 0x06 mask
 * 0x10) and align_to_unit_facing bit (byte 0x07 mask 0x04) of the emitter's
 * 16-bit child/velocity flags. ANGLE_DIRECTED scatters around the base angles, INWARD aims at
 * the spawn origin, CASTER_FACING rotates those angles by the caster's facing, and
 * TOWARD_TARGET (effect_slot_spawn_motion) rotates it along the
 * caster-to-target direction. */
typedef enum effect_emitter_velocity {
    EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED = 0x000,
    EFFECT_EMITTER_VELOCITY_INWARD = 0x010,
    EFFECT_EMITTER_VELOCITY_TOWARD_TARGET = 0x400,
    EFFECT_EMITTER_VELOCITY_CASTER_FACING = 0x410
} effect_emitter_velocity_e;

enum { EFFECT_EMITTER_VELOCITY_MASK = 0x410 };

/* The 16-bit flags at 0x06 of an emitter record (effect_geometry_entry_t.flags), copied to each particle's flags
 * (effect_work_record_t.flags). Bits 0x410 are effect_emitter_velocity_e. A child mode of 1 or 2
 * enables the child emitter; the engine does not tell the two apart. */
enum {
    EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK = 0x0003, /* spawn child_emitter_on_death when the particle dies */
    EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_1 = 0x0001,
    EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_2 = 0x0002,
    EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK = 0x000c, /* spawn child_emitter_mid_life every frame it lives */
    EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_1 = 0x0004,
    EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_2 = 0x0008,
    EFFECT_EMITTER_FLAG_COLOR_CURVES = 0x0040, /* tint by the three color curves; clear, a neutral 128 */
    /* A particle that comes within this distance of its homing target on all three axes ends its
     * life: the field shifted right by SHIFT is 16, 32 or 48 world units, 0 for never. */
    EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_MASK = 0x0300,
    EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_SHIFT = 4
};

/* curve bindings */
/* One-based bindings for the emitter's initial motion. */
typedef struct effect_emitter_spawn_curve_bindings {
    u32 position : 4;
    u32 spread : 4;
    u32 velocity_angle : 4;
    u32 velocity_spread : 4;
    u32 inertia : 4;
    u32 _unknown_02 : 4;
    u32 weight : 4;
    u32 radial_speed : 4;
} effect_emitter_spawn_curve_bindings_t;
typedef char
    effect_emitter_spawn_curve_bindings_size_must_be_4[sizeof(effect_emitter_spawn_curve_bindings_t) == 4 ? 1 : -1];

/* The same fields as bit positions. The native renderers copy each binding
 * word into a signed local and extract a field as (word >> SHIFT) & 0xf;
 * unsigned copies change the shifts. */
enum {
    EFFECT_SPAWN_CURVE_SPAWN_POSITION_SHIFT = 0,
    EFFECT_SPAWN_CURVE_SPAWN_SPREAD_SHIFT = 4,
    EFFECT_SPAWN_CURVE_VELOCITY_BASE_ANGLE_SHIFT = 8,
    EFFECT_SPAWN_CURVE_VELOCITY_DIRECTION_SPREAD_SHIFT = 12,
    EFFECT_SPAWN_CURVE_RADIAL_SPEED_SHIFT = 28
};

/* Bit positions in the emitter's packed 32-bit motion-curve binding word.
 * Zero disables a curve; other values select its row starting at one. */
enum {
    EFFECT_EMITTER_CURVE_ACCELERATION_SHIFT = 0,
    EFFECT_EMITTER_CURVE_ACCELERATION_STEP_SHIFT = 4,
    EFFECT_EMITTER_CURVE_LIFETIME_SHIFT = 8,
    EFFECT_EMITTER_CURVE_TARGET_OFFSET_SHIFT = 12,
    EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT = 20,
    EFFECT_EMITTER_CURVE_SPAWN_INTERVAL_SHIFT = 24,
    EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT = 28,
    EFFECT_EMITTER_CURVE_HOMING_MOTION_SHIFT = 30
};

/* Nibble positions of the zero-based red, green and blue curve indices in a
 * record's color_curves word. The renderers copy the word into a signed local
 * and shift and mask each nibble. A bit-field record would extract with
 * different shifts, which the original instructions rule out. */
enum { EFFECT_COLOR_CURVE_RED_SHIFT = 0, EFFECT_COLOR_CURVE_GREEN_SHIFT = 4, EFFECT_COLOR_CURVE_BLUE_SHIFT = 8 };

/* texture pages */
/* Blend mode and primitive transparency control in a geometry record. */
typedef struct effect_texture_page_config {
    u16 blend_mode : 2;
    u16 clear_abe : 1;
    u16 _unused_00 : 13;
} effect_texture_page_config_t;
typedef char effect_texture_page_config_size_must_be_2[sizeof(effect_texture_page_config_t) == 2 ? 1 : -1];

/* The texture_page_flags bits of the effect geometry records. */
enum {
    EFFECT_TEXTURE_PAGE_ABR_MASK = 0x03, /* the semi-transparency rate, shifted into bits 5-6 of the tpage */
    EFFECT_TEXTURE_PAGE_FLAG_CLEAR_ABE = 0x04
};

/* particle emitter */
/* The spawn block at 0x14-0x4b of an emitter record, as the effect_interpolate_emitter_* helpers read it; the native
 * renderers that spawn like the particle emitter embed it. The spread is the ellipsoid or box half-extent around the
 * spawn position and the velocity spread randomizes the base angles. */
typedef struct effect_emitter_geometry_view {
    vec3s16_t spawn_position_start;      /* 0x00 */
    vec3s16_t spawn_position_end;        /* 0x06 */
    vec3s16_t spawn_spread_start;        /* 0x0c */
    vec3s16_t spawn_spread_end;          /* 0x12 */
    vec3s16_t velocity_base_angle_start; /* 0x18 */
    vec3s16_t velocity_base_angle_end;   /* 0x1e */
    vec3s16_t velocity_spread_start;     /* 0x24 */
    vec3s16_t velocity_spread_end;       /* 0x2a */
    ranges16_t inertia_start;            /* 0x30 */
    ranges16_t inertia_end;              /* 0x34 */
} effect_emitter_geometry_view_t;
typedef char effect_emitter_geometry_view_size_must_be_56[sizeof(effect_emitter_geometry_view_t) == 56 ? 1 : -1];

/* Minimum and maximum components of a particle-motion vector. */
typedef struct effect_xyz_min_max {
    ranges16_t x;
    ranges16_t y;
    ranges16_t z;
} effect_xyz_min_max_t;
typedef char effect_xyz_min_max_size_must_be_12[sizeof(effect_xyz_min_max_t) == 12 ? 1 : -1];

/* One 196-byte record in the effect file's particle-system section.
 * Native EFFECT renderers reuse its slots for their own parameter groups.
 * One-based motion bindings select interpolation between each start/end pair;
 * minimum/maximum pairs bound the random values chosen for each particle. */
typedef struct effect_geometry_entry {
    u8 _unused_00;
    u8 animation_index;  /* 0x01: the sprite sequence its particles play */
    u16 placement_flags; /* 0x02: EFFECT_EMITTER_* placement bits and EFFECT_WORK_KIND_*; the particle's kind_flags */
    u8 animation_frame_group; /* 0x04: frame group of that sequence's frames */
    u8 _padding_05;           /* aligns flags */
    u16 flags;                /* 0x06: EFFECT_EMITTER_FLAG_* and effect_emitter_velocity_e; the particle's flags */
    effect_emitter_spawn_curve_bindings_t spawn_bindings; /* 0x08 */
    u32 motion_curve_selectors; /* 0x0c: acceleration, acceleration step, lifetime, target offset, count and interval;
                                 * bits 28-29 bind homing strength and bits 30-31 bind its curve */
    u32 color_curves;           /* 0x10: zero-based RGB curve indices in the low three nibbles */
    effect_emitter_geometry_view_t spawn;         /* 0x14: initial position, spread, angles and inertia */
    s16 _unknown_4c[2];                           /* 0x4c: lerped to _unknown_50 and discarded */
    s16 _unknown_50[2];                           /* 0x50 */
    ranges16_t weight_start;                      /* 0x54: scales gravity */
    ranges16_t weight_end;                        /* 0x58 */
    ranges16_t radial_speed_start;                /* 0x5c */
    ranges16_t radial_speed_end;                  /* 0x60 */
    effect_xyz_min_max_t acceleration_start;      /* 0x64 */
    effect_xyz_min_max_t acceleration_end;        /* 0x70 */
    effect_xyz_min_max_t acceleration_step_start; /* 0x7c */
    effect_xyz_min_max_t acceleration_step_end;   /* 0x88 */
    ranges16_t lifetime_start;                    /* 0x94: in frames */
    ranges16_t lifetime_end;                      /* 0x98 */
    vec3s16_t target_offset_start; /* 0x9c: homing target, an offset from what effect_emitter_homing_e names */
    vec3s16_t target_offset_end;   /* 0xa2 */
    u8 _unused_a8[0x08];
    s16 particle_count_start;         /* 0xb0: particles per spawn */
    s16 particle_count_end;           /* 0xb2 */
    s16 spawn_interval_start;         /* 0xb4: frames between spawns */
    s16 spawn_interval_end;           /* 0xb6 */
    ranges16_t homing_strength_start; /* 0xb8 */
    ranges16_t homing_strength_end;   /* 0xbc */
    u8 child_emitter_on_death;        /* 0xc0: see EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK */
    u8 child_emitter_mid_life;        /* 0xc1: see EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK */
    u8 _padding_c2[0x02];             /* tail padding to 4-byte alignment */
} effect_geometry_entry_t;
typedef char effect_geometry_entry_size_must_be_196[sizeof(effect_geometry_entry_t) == 196 ? 1 : -1];

/* textured quad grids and wave mesh */
/* One-based curve bindings for texture scroll and wave phase rates. */
typedef struct effect_textured_quad_scroll_curves {
    u32 _unused_00 : 4;
    u32 wave_y_phase_speed : 4;
    u32 scroll_angle : 4;
    u32 wave_x_phase_speed : 4;
    u32 _unused_02 : 12;
    u32 scroll_speed : 4;
} effect_textured_quad_scroll_curves_t;
typedef char
    effect_textured_quad_scroll_curves_size_must_be_4[sizeof(effect_textured_quad_scroll_curves_t) == 4 ? 1 : -1];

/* One curve controls the shape parameters of both waves. */
typedef struct effect_wave_shape_curves {
    u32 shape : 4;
    u32 _unused_00 : 28;
} effect_wave_shape_curves_t;
typedef char effect_wave_shape_curves_size_must_be_4[sizeof(effect_wave_shape_curves_t) == 4 ? 1 : -1];

/* One wave of the wave mesh: the view holds a start and an end set for each axis, between which
 * the wave curves interpolate. */
typedef struct effect_wave_parameters {
    s16 period;
    s16 amplitude_period;
    s16 amplitude;
    u8 _unused_06[2];
    s16 phase_step;
    u8 _unused_0a[2];
} effect_wave_parameters_t;
typedef char effect_wave_parameters_size_must_be_12[sizeof(effect_wave_parameters_t) == 12 ? 1 : -1];

/* The scrolling textured quad grids' (2x2, 4x4 and 8x8) and wave mesh's
 * view of one emitter record. Both use the scroll angle and speed bindings.
 * The wave mesh also reads vertical and horizontal phase-speed bindings and
 * one shape binding for both waves. Its vertical phase-speed pair occupies
 * the emitter spawn-spread block at 0x20 and is read by that interpolation helper. */
typedef struct effect_textured_quad_geometry_view {
    u8 _unused_00[0x06];
    u16 flags;                                          /* 0x06: EFFECT_EMITTER_FLAG_COLOR_CURVES */
    effect_textured_quad_scroll_curves_t scroll_curves; /* 0x08 */
    effect_wave_shape_curves_t wave_curves;             /* 0x0c */
    u32 color_curves;                                   /* 0x10: nibbles 0-2 = r/g/b */
    u16 texture_u;                                      /* 0x14 */
    u16 texture_v;                                      /* 0x16 */
    u8 _unused_18[0x16];
    s16 scroll_angle_start; /* 0x2e */
    u8 _unused_30[0x04];
    s16 scroll_angle_end; /* 0x34 */
    u8 _unused_36[0x02];
    s16 wave_phase_speed_start[2]; /* 0x38 */
    u8 _unused_3c[0x02];
    s16 wave_phase_speed_end[2]; /* 0x3e */
    u8 _unused_42[0x0a];
    effect_texture_page_config_t texture_page_config; /* 0x4c: EFFECT_TEXTURE_PAGE_* */
    s16 brightness_row;                               /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54 */
    u8 _unused_56[0x06];
    s16 scroll_speed_start; /* 0x5c */
    u8 _unused_5e[0x02];
    s16 scroll_speed_end; /* 0x60 */
    u8 _unused_62[0x02];
    effect_wave_parameters_t wave_y_start; /* 0x64 */
    effect_wave_parameters_t wave_y_end;   /* 0x70 */
    effect_wave_parameters_t wave_x_start; /* 0x7c */
    effect_wave_parameters_t wave_x_end;   /* 0x88 */
    u8 _unused_94[0x14];
    s16 texture_width;  /* 0xa8 */
    s16 texture_height; /* 0xaa */
    u8 _unused_ac[0x18];
} effect_textured_quad_geometry_view_t;
typedef char
    assert_effect_textured_quad_geometry_view_size[sizeof(effect_textured_quad_geometry_view_t) == 196 ? 1 : -1];

/* particle ribbon */
/* The ribbon binds target spread at bit 4 of motion_curve_selectors, where the
 * emitter binds acceleration step; its other curves keep the emitter's places. */
enum { EFFECT_PARTICLE_RIBBON_CURVE_TARGET_SPREAD_SHIFT = 4 };

/* The particle ribbon renderer's view of one emitter record. */
typedef struct effect_particle_ribbon_geometry_view {
    u8 _unused_00[0x02];
    u16 placement_flags; /* 0x02 */
    u8 _unused_04[0x02];
    u16 flags;                                        /* 0x06 */
    u32 spawn_curve_bindings;                         /* 0x08 */
    u32 target_curve_bindings;                        /* 0x0c */
    u32 color_curves;                                 /* 0x10 */
    effect_emitter_geometry_view_t emitter;           /* 0x14: read by effect_interpolate_emitter_* */
    effect_texture_page_config_t texture_page_config; /* 0x4c */
    s16 brightness_row;                               /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54 */
    u8 _unused_56[0x06];
    ranges16_t radial_speed_start; /* 0x5c */
    ranges16_t radial_speed_end;   /* 0x60 */
    s16 texture_u;                 /* 0x64 */
    u8 _unused_66[0x02];
    s16 texture_v; /* 0x68 */
    u8 _unused_6a[0x02];
    s16 frame_count; /* 0x6c */
    s16 frame_delay; /* 0x6e */
    u8 _unused_70[0x0c];
    s16 target_spread_start_x; /* 0x7c */
    u8 _unused_7e[2];
    s16 target_spread_start_y; /* 0x80 */
    u8 _unused_82[2];
    s16 target_spread_start_z; /* 0x84 */
    u8 _unused_86[2];
    s16 target_spread_end_x; /* 0x88 */
    u8 _unused_8a[2];
    s16 target_spread_end_y; /* 0x8c */
    u8 _unused_8e[2];
    s16 target_spread_end_z; /* 0x90 */
    u8 _unused_92[2];
    ranges16_t lifetime_start;     /* 0x94 */
    ranges16_t lifetime_end;       /* 0x98 */
    vec3s16_t target_offset_start; /* 0x9c */
    vec3s16_t target_offset_end;   /* 0xa2 */
    s16 texture_width;             /* 0xa8 */
    s16 texture_height;            /* 0xaa */
    u8 _unused_ac[0x04];
    s16 particle_count_start; /* 0xb0 */
    s16 particle_count_end;   /* 0xb2 */
    s16 spawn_interval_start; /* 0xb4 */
    s16 spawn_interval_end;   /* 0xb6 */
    u8 _unused_b8[0x08];
    u8 child_emitter_on_death; /* 0xc0 */
    u8 child_emitter_mid_life; /* 0xc1 */
    u8 _padding_c2[0x02];      /* tail padding to 4-byte alignment */
} effect_particle_ribbon_geometry_view_t;
typedef char
    assert_effect_particle_ribbon_geometry_view_size[sizeof(effect_particle_ribbon_geometry_view_t) == 196 ? 1 : -1];

/* polar mesh */
/* Polar motion bindings use one-based curve selectors; zero keeps the starting value. */
typedef struct effect_polar_curve_bindings {
    u16 center : 4;
    u16 radius : 4;
    u16 velocity : 4;
    u16 acceleration : 4;
    u16 angle : 4;
    u16 _unused_02 : 8;
    u16 radius_step : 4;
    u16 radius_step_delta : 4;
    u16 scroll : 4;
    u16 _unused_04 : 4;
    u16 angle_step : 4;
    u8 _unused_06;
    u8 angle_step_delta : 4;
    u8 _unused_07 : 4;
} effect_polar_curve_bindings_t;
typedef char effect_polar_curve_bindings_size_must_be_8[sizeof(effect_polar_curve_bindings_t) == 8 ? 1 : -1];

/* The polar renderer gives the particle emitter's parameter slots its own meanings. */
typedef struct effect_polar_mesh_geometry_view {
    u8 _unused_00[0x06];
    u16 flags;                                    /* 0x06: bit 6 = per-channel color curves */
    effect_polar_curve_bindings_t curve_bindings; /* 0x08 */
    u32 color_curves;                             /* 0x10: nibbles 0-2 = r/g/b */
    vec3s16_t center_start; /* 0x14: spawn position, read by effect_interpolate_emitter_spawn_position */
    vec3s16_t center_end;   /* 0x1a */
    s16 radius_start;       /* 0x20: spawn spread x */
    u8 _unused_22[0x04];
    s16 radius_end; /* 0x26 */
    u8 _unused_28[0x04];
    s16 velocity_angle_start; /* 0x2c: velocity base angle x */
    s16 velocity_speed_start; /* 0x2e: velocity base angle y */
    u8 _unused_30[0x02];
    s16 velocity_angle_end; /* 0x32 */
    s16 velocity_speed_end; /* 0x34 */
    u8 _unused_36[0x02];
    s16 acceleration_angle_start; /* 0x38: velocity spread x */
    s16 acceleration_speed_start; /* 0x3a: velocity spread y */
    u8 _unused_3c[0x02];
    s16 acceleration_angle_end; /* 0x3e */
    s16 acceleration_speed_end; /* 0x40 */
    u8 _unused_42[0x02];
    s16 angle_start; /* 0x44: minimum inertia */
    u8 _unused_46[0x02];
    s16 angle_end; /* 0x48 */
    u8 _unused_4a[0x02];
    /* Bit fields change the 16-bit reload and register allocation. */
    u16 texture_page_flags; /* 0x4c: EFFECT_TEXTURE_PAGE_* */
    s16 brightness_row;     /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54 */
    u8 _unused_56[0x06];
    s16 radius_step_start; /* 0x5c: minimum radial speed */
    u8 _unused_5e[0x02];
    s16 radius_step_end; /* 0x60 */
    u8 _unused_62[0x02];
    s16 radius_step_delta_start; /* 0x64: minimum x acceleration */
    u8 _unused_66[0x0a];
    s16 radius_step_delta_end; /* 0x70 */
    u8 _unused_72[0x0a];
    s16 scroll_u_speed_start; /* 0x7c: minimum x acceleration step */
    u8 _unused_7e[0x02];
    s16 scroll_v_speed_start; /* 0x80: minimum y acceleration step */
    u8 _unused_82[0x06];
    s16 scroll_u_speed_end; /* 0x88 */
    u8 _unused_8a[0x02];
    s16 scroll_v_speed_end; /* 0x8c */
    u8 _unused_8e[0x0e];
    s16 angle_step_start; /* 0x9c: target offset x */
    u8 _unused_9e[0x04];
    s16 angle_step_end; /* 0xa2 */
    u8 _unused_a4[0x04];
    s16 texture_width;  /* 0xa8 */
    s16 texture_height; /* 0xaa */
    u8 _unused_ac[0x08];
    s16 angle_step_delta_start; /* 0xb4: spawn interval */
    s16 angle_step_delta_end;   /* 0xb6 */
    u16 texture_u;              /* 0xb8: minimum start homing strength */
    u16 texture_v;              /* 0xba: maximum start homing strength */
    u8 _unused_bc[0x08];
} effect_polar_mesh_geometry_view_t;
typedef char
    effect_polar_mesh_geometry_view_size_must_be_196[sizeof(effect_polar_mesh_geometry_view_t) == 196 ? 1 : -1];

/* ring, tube and dome meshes */
/* Radial motion bindings use one-based curve selectors; zero keeps the starting value. */
typedef struct effect_radial_motion_curve_bindings {
    u32 origin : 4;
    u32 _unused_00 : 4;
    u32 texture : 4;
    u32 _unused_01 : 16;
    u32 spin : 4;
} effect_radial_motion_curve_bindings_t;
typedef char
    effect_radial_motion_curve_bindings_size_must_be_4[sizeof(effect_radial_motion_curve_bindings_t) == 4 ? 1 : -1];

typedef struct effect_radial_shape_curve_bindings {
    u16 growth : 4;
    u16 growth_step : 4;
    u16 _unused_01 : 8;
    u16 radius : 4;
    u16 _unused_02 : 12;
} effect_radial_shape_curve_bindings_t;
typedef char
    effect_radial_shape_curve_bindings_size_must_be_4[sizeof(effect_radial_shape_curve_bindings_t) == 4 ? 1 : -1];

/* Geometry parameters shared by ring, tube and summon-dome renderers.
 * Dome renderers leave the row-radius terms unread. */
typedef struct effect_radial_mesh_geometry {
    u8 _unused_00[0x02];
    u16 placement_flags; /* 0x02: bits 9-11 select how the origin is placed */
    u8 _unused_04[0x02];
    u16 flags;                                           /* 0x06: bit 6 = per-channel color curves */
    effect_radial_motion_curve_bindings_t motion_curves; /* 0x08 */
    effect_radial_shape_curve_bindings_t shape_curves;   /* 0x0c */
    u32 color_curves;                                    /* 0x10: nibbles 0-2 = r/g/b */
    vec3s16_t origin_start;                              /* 0x14 */
    vec3s16_t origin_end;                                /* 0x1a */
    s16 u_span;                                          /* 0x20 */
    s16 v_span;                                          /* 0x22 */
    effect_texture_page_config_t texture_page_config;    /* 0x24: bits 0-1 blend rate, bit 2 clears ABE */
    u8 _unused_26[0x06];
    s16 u_start;        /* 0x2c */
    s16 v_start;        /* 0x2e */
    u16 v_scroll_speed; /* 0x30 */
    s16 u_end;          /* 0x32 */
    s16 v_end;          /* 0x34 */
    u8 _unused_36[0x06];
    s16 brightness_row; /* 0x3c */
    u8 _unused_3e[0x1e];
    s16 spin_speed_start; /* 0x5c */
    u8 _unused_5e[0x02];
    s16 spin_speed_end; /* 0x60 */
    u8 _unused_62[0x02];
    s16 growth_start; /* 0x64 */
    u8 _unused_66[0x02];
    s16 height_start; /* 0x68 */
    u8 _unused_6a[0x02];
    s16 row_radius_start; /* 0x6c */
    u8 _unused_6e[0x02];
    s16 growth_end; /* 0x70 */
    u8 _unused_72[0x02];
    s16 height_end; /* 0x74 */
    u8 _unused_76[0x02];
    s16 row_radius_end; /* 0x78 */
    u8 _unused_7a[0x02];
    s16 growth_step_start; /* 0x7c */
    u8 _unused_7e[0x02];
    s16 height_step_start; /* 0x80 */
    u8 _unused_82[0x02];
    s16 row_radius_step_start; /* 0x84 */
    u8 _unused_86[0x02];
    s16 growth_step_end; /* 0x88 */
    u8 _unused_8a[0x02];
    s16 height_step_end; /* 0x8c */
    u8 _unused_8e[0x02];
    s16 row_radius_step_end; /* 0x90 */
    u8 _unused_92[0x16];
    s16 radius_start; /* 0xa8 */
    u8 _unused_aa[0x02];
    s16 radius_end; /* 0xac */
    u8 _unused_ae[0x16];
} effect_radial_mesh_geometry_t;
typedef char effect_radial_mesh_geometry_size_must_be_196[sizeof(effect_radial_mesh_geometry_t) == 196 ? 1 : -1];

/* tentacle mesh */
/* The tentacle reads bits 4 to 15 of motion_curve_selectors as its tip's
 * curves; particle count and spawn interval keep the emitter's places. */
enum {
    EFFECT_TENTACLE_CURVE_TIP_SPREAD_SHIFT = 4,
    EFFECT_TENTACLE_CURVE_TRAVEL_DURATION_SHIFT = 8,
    EFFECT_TENTACLE_CURVE_TIP_OFFSET_SHIFT = 12
};

/* Segment renderers reuse the particle emitter's spawn parameters;
 * tip spread, travel duration and tip offset describe the moving endpoint. */
typedef struct effect_tentacle_mesh_geometry_view {
    u8 _unused_00[0x02];
    u16 placement_flags; /* 0x02: bits 9-11 place the spawn origin, bits 5-7 the tip */
    u8 _unused_04[0x02];
    u16 flags;                                        /* 0x06: bits 0-1 and 2-3 enable death and mid-life children */
    u32 spawn_curve_bindings;                         /* 0x08 */
    u32 tip_curve_bindings;                           /* 0x0c */
    u32 color_curves;                                 /* 0x10 */
    effect_emitter_geometry_view_t emitter;           /* 0x14: read by effect_interpolate_emitter_* */
    effect_texture_page_config_t texture_page_config; /* 0x4c */
    s16 brightness_row;                               /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54: added to each quad's depth */
    u8 _unused_56[0x06];
    ranges16_t radial_speed_start; /* 0x5c */
    ranges16_t radial_speed_end;   /* 0x60 */
    s16 texture_u;                 /* 0x64 */
    u8 _unused_66[0x02];
    s16 texture_v; /* 0x68 */
    u8 _unused_6a[0x02];
    s16 frame_count; /* 0x6c */
    s16 frame_delay; /* 0x6e */
    u8 _unused_70[0x0c];
    s16 tip_spread_start_x; /* 0x7c */
    u8 _unused_7e[0x02];
    s16 tip_spread_start_y; /* 0x80 */
    u8 _unused_82[0x02];
    s16 tip_spread_start_z; /* 0x84 */
    u8 _unused_86[0x02];
    s16 tip_spread_end_x; /* 0x88 */
    u8 _unused_8a[0x02];
    s16 tip_spread_end_y; /* 0x8c */
    u8 _unused_8e[0x02];
    s16 tip_spread_end_z; /* 0x90 */
    u8 _unused_92[0x02];
    ranges16_t travel_duration_start; /* 0x94: frames to reach the tip, excluding tail shrink */
    ranges16_t travel_duration_end;   /* 0x98 */
    vec3s16_t target_offset_start;    /* 0x9c: the tip, added to the anchor chosen by placement_flags bits 5-7 */
    vec3s16_t target_offset_end;      /* 0xa2 */
    s16 texture_width;                /* 0xa8 */
    s16 texture_height;               /* 0xaa */
    u8 _unused_ac[0x04];
    s16 particle_count_start; /* 0xb0: segments spawned per interval */
    s16 particle_count_end;   /* 0xb2 */
    s16 spawn_interval_start; /* 0xb4 */
    s16 spawn_interval_end;   /* 0xb6 */
    u8 _unused_b8[0x08];
    u8 child_emitter_on_death; /* 0xc0 */
    u8 child_emitter_mid_life; /* 0xc1: spawned at the trailing point each frame */
    u8 _padding_c2[0x02];      /* tail padding to 4-byte alignment */
} effect_tentacle_mesh_geometry_view_t;
typedef char
    assert_effect_tentacle_mesh_geometry_view_size[sizeof(effect_tentacle_mesh_geometry_view_t) == 196 ? 1 : -1];

/* map vertex particles */
/* The E454 and E464 particle handlers use the renderer-specific meanings of
 * the emitter record's 0x4c and 0xa8 fields. */
typedef struct effect_particle_vertex_emitter_view {
    u8 _unused_00[0x4c];
    s16 vertex_group; /* 0x4c: selects a vertex in the effect work buffer */
    u8 _unused_4e[0x5a];
    s16 start_frame; /* 0xa8 */
    s16 end_frame;   /* 0xaa */
    u8 _unused_ac[0x18];
} effect_particle_vertex_emitter_view_t;
typedef char
    assert_effect_particle_vertex_emitter_view_size[sizeof(effect_particle_vertex_emitter_view_t) == 196 ? 1 : -1];

/* particle-system section */
/* One 0xc4-byte emitter record, read through the view of the renderer that owns it. The
 * interpolation helpers read the particle-emitter slots these layouts share; renderer-specific
 * fields keep their own names. */
typedef union effect_geometry_record {
    effect_geometry_entry_t emitter;
    effect_textured_quad_geometry_view_t textured_quad;
    effect_particle_ribbon_geometry_view_t particle_ribbon;
    effect_polar_mesh_geometry_view_t polar_mesh;
    effect_radial_mesh_geometry_t radial_mesh;
    effect_tentacle_mesh_geometry_view_t tentacle_mesh;
    effect_particle_vertex_emitter_view_t particle_vertex_emitter;
} effect_geometry_record_t;
typedef char effect_geometry_record_size_must_be_196[sizeof(effect_geometry_record_t) == 196 ? 1 : -1];

/* The particle-system section (effect_file_header_t.particle_system). */
typedef struct effect_geometry_table {
    u16 _unused_00;        /* 2, or 1 in seven shipped files */
    u16 entry_count;       /* 0x02 */
    vec3s32_t gravity;     /* 0x04: copied to g_effect_gravity by effect script opcode 0x27 */
    s32 inertia_threshold; /* 0x10: passed to effect_set_inertia_threshold by opcode 0x27 */
    /* One 0xc4-byte record per emitter or mesh. The slots keep their curve
     * bindings, but each native EFFECT renderer gives them its own meanings,
     * so a record is read through the member of the renderer that owns it. */
    effect_geometry_record_t entries[0];
} effect_geometry_table_t;
typedef char effect_geometry_table_size_must_be_20[sizeof(effect_geometry_table_t) == 20 ? 1 : -1];

extern const effect_geometry_table_t* g_effect_geometry_table;

#endif
