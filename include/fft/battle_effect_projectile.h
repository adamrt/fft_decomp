#ifndef BATTLE_EFFECT_PROJECTILE_H
#define BATTLE_EFFECT_PROJECTILE_H

/* BATTLE effect projectiles: arc and straight trajectories, obstacle tests and the projectile models. */

#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* trajectories */
/* Arc endpoint handed to the per-arc obstacle test: the unit's raised tile
 * position followed by its id and height allowance. */
typedef struct projectile_arc_endpoint {
    SVECTOR position;
    s32 unit_id;
    s32 height;
} projectile_arc_endpoint_t;

/* Offset to the target plus its fixed-point horizontal length. */
typedef struct projectile_arc_delta {
    s32 x;
    s32 y;
    s32 z;
    s32 distance;
} projectile_arc_delta_t;

/* Query read by projectile_arc_calculate_trajectory: the offset to the
 * target and both endpoints. */
typedef struct projectile_arc_query {
    projectile_arc_delta_t delta;
    projectile_arc_endpoint_t source;
    projectile_arc_endpoint_t target;
} projectile_arc_query_t;

/* Unit list built in the scratchpad at 0x1f800000 by
 * projectile_list_obstacle_units and read by the
 * obstacle tests. */
typedef struct effect_obstacle_unit_list {
    s32 count;                           /* 0x00 */
    u8 battle_ids[UNIT_SLOT_COUNT];      /* 0x04 */
    u8 _padding_19;                      /* 0x19; aligns the position vectors */
    SVECTOR positions[UNIT_SLOT_COUNT];  /* 0x1a; tile center and height */
    u8 _padding_c2[2];                   /* 0xc2; aligns the sprite heights */
    s32 sprite_heights[UNIT_SLOT_COUNT]; /* 0xc4 */
} projectile_obstacle_unit_list_t;
typedef char assert_effect_obstacle_unit_list_size[sizeof(projectile_obstacle_unit_list_t) == 280 ? 1 : -1];

extern s32 g_projectile_arc_height;
extern s16 g_projectile_arctan_angle_mod;
extern s16 g_effect_arrow_arc_caster_relative_yaw;
extern s32 g_effect_arrow_arc_distance;
extern s32 g_effect_arrow_arc_distance_step;
extern s32 g_effect_arrow_arc_height;
extern s16 g_effect_arrow_arc_launch_angle;
extern s32 g_effect_arrow_arc_length;
extern u16 g_effect_arrow_arc_yaw;
extern VECTOR g_effect_projectile_delta;
extern s32 g_effect_projectile_length;
extern s32 g_effect_projectile_progress;
extern s32 g_effect_projectile_speed;
extern s16 g_effect_projectile_spin_passes;
extern s16 g_effect_projectile_step_count;
extern SVECTOR g_projectile_destination;
extern s32 g_projectile_hit_unit_id;
extern SVECTOR g_projectile_origin;
extern SVECTOR g_projectile_position; /* final projectile position */
extern s32 g_projectile_source_id;
extern VECTOR g_projectile_step; /* last step's movement */
extern s32 g_projectile_step_count;
extern battle_map_tile_coordinates_t g_projectile_tile;
extern s32 g_projectile_tile_flags;

s32 projectile_arc_check_between_units(u8 source_id, u8 target_id);
s32 projectile_direct_check_between_units(u8 source_id, u8 target_id);
s32 projectile_direct_check_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id);
s32 projectile_get_bow_shot_angle(void);
s32 projectile_arc_set_and_validate(s32 source_id, SVECTOR* tile_position, s32 target_id);
void projectile_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);

/* models */
/* The projectile models of g_effect_model_files. */
typedef enum effect_model {
    EFFECT_MODEL_ARROW = 0,
    EFFECT_MODEL_STONE = 1,
    EFFECT_MODEL_REFLECT = 2,
    EFFECT_MODEL_SHURIKEN = 3, /* drawn for the thrown items of ITEM_TYPE_SHURIKEN */
} effect_model_e;

/* Opcodes (top header byte) of the effect model commands that effect_model_decode_next_command
 * decodes: each draws the primitive it is named for. Nothing in the code tells the two GT3 opcodes
 * apart except that effect_model_draw_projectile draws only the second. */
enum {
    EFFECT_MODEL_COMMAND_G3 = 0x31,
    EFFECT_MODEL_COMMAND_GT3_35 = 0x35,
    EFFECT_MODEL_COMMAND_GT3_37 = 0x37,
    EFFECT_MODEL_COMMAND_G4 = 0x39,
};

/* Shared output of the model-command decoder and input of the projectile renderer. */
typedef struct effect_decoded_model_command {
    struct {
        u8 _unused_00;
        u8 word_count;
        u8 _unused_02;
        u8 opcode;
    } header;          /* 0x00 */
    CVECTOR colors[4]; /* 0x04 */
    struct {
        u16 tpage;
        u16 clut;
        uv8_t uv[3];
    } texture; /* 0x14 */
    u8 _unused_1e[0x4a];
    /* The decoder copies two packed pairs; the renderer reads four indices. */
    union {
        u32 pairs[2];
        u16 indices[4];
    } vertices; /* 0x68 */
} effect_decoded_model_command_t;
typedef char effect_decoded_model_command_size_must_be_112[sizeof(effect_decoded_model_command_t) == 112 ? 1 : -1];

/* 0xc88-byte arrow work buffer of effect_secondary_arrow_arc_handler and
 * effect_secondary_linear_projectile_handler: two sets of four G3 and thirteen G4
 * polygons. */
typedef struct effect_arrow_buffer {
    POLY_G3 g3[2][4];  /* 0x000 */
    POLY_G4 g4[2][13]; /* 0x0e0 */
    u8 _unused_488[0x800];
} effect_arrow_buffer_t;
typedef char effect_arrow_buffer_size_must_be_3208[(sizeof(effect_arrow_buffer_t) == 3208) ? 1 : -1];

/* Double-buffered polygons initialized by the secondary handler and drawn by
 * effect_model_draw_projectile. */
typedef struct effect_stone_buffer {
    POLY_G3 g3[2][12];
} effect_stone_buffer_t;

typedef struct effect_shuriken_buffer {
    POLY_G3 g3[2][16];
} effect_shuriken_buffer_t;

typedef struct effect_reflect_buffer {
    u16 clut[2][6];
    POLY_GT3 prims[2][24];
} effect_reflect_buffer_t;

extern s32 g_effect_model_command_word_offset;
extern psyq_tmd_packet_t* g_effect_model_commands;
extern psyq_tmd_header_t* g_effect_model_files[];
extern SVECTOR* g_effect_model_normals;
extern psyq_tmd_object_t* g_effect_model_object;
extern SVECTOR* g_effect_model_vertices;
extern MATRIX g_effect_projectile_matrix;

void effect_model_decode_next_command(effect_decoded_model_command_t* out);
s32 effect_model_init_sections(psyq_tmd_header_t* resource, s32 unused_1);

#endif
