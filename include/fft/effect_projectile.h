#ifndef EFFECT_PROJECTILE_H
#define EFFECT_PROJECTILE_H

/* BATTLE's projectile secondary effects: the flight state of arcing arrows and straight shots, and the projectile
 * models with their polygon buffers. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* arrow arc */
/* Flight state of effect_secondary_arrow_arc_handler, set in its first phase from the traced trajectory
 * (g_projectile_*): heading, launch pitch, distance along the arc, steps left and the yaw rotation matrix. */
extern s32 g_effect_arrow_arc_length;
extern u16 g_effect_arrow_arc_yaw;
extern s16 g_effect_arrow_arc_caster_relative_yaw;
extern s32 g_effect_arrow_arc_height;
extern s16 g_effect_arrow_arc_launch_angle;
extern s32 g_effect_arrow_arc_distance;
extern s32 g_effect_arrow_arc_distance_step;
extern s16 g_effect_projectile_step_count;
extern MATRIX g_effect_projectile_matrix;

/* Provisional: 0x80000000 when an arrow arc ends without a hit; nothing reads it. */
extern s32 g_effect_arrow_miss_marker;

/* straight flight */
/* Flight state of effect_secondary_linear_projectile_handler: the origin-to-destination offset and its length, the
 * progress and speed along it, and the frames left (spin passes). */
extern s32 g_effect_projectile_length;
extern VECTOR g_effect_projectile_delta;
extern s32 g_effect_projectile_progress;
extern s32 g_effect_projectile_speed;
extern s16 g_effect_projectile_spin_passes;

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

extern psyq_tmd_header_t* g_effect_model_files[];

s32 effect_model_init_sections(psyq_tmd_header_t* resource, s32 unused_1);
void effect_model_decode_next_command(effect_decoded_model_command_t* out);

#endif
