#ifndef EFFECT_PROJECTILE_H
#define EFFECT_PROJECTILE_H

/* Projectile model decoding and shared flight transform. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Projectile rotation shared by the flight handlers and emitter transform. */
extern MATRIX g_effect_projectile_matrix;

/* models */
/* The projectile models of g_effect_model_files. */
typedef enum effect_model {
    EFFECT_MODEL_ARROW = 0,
    EFFECT_MODEL_STONE = 1,
    EFFECT_MODEL_REFLECT = 2,
    EFFECT_MODEL_SHURIKEN = 3 /* drawn for the thrown items of ITEM_TYPE_SHURIKEN */
} effect_model_e;

/* Top-byte TMD commands decoded for the effect models. Bit 1 selects semitransparency;
 * the shipped Reflect model uses the semitransparent GT3 form. */
enum {
    EFFECT_MODEL_COMMAND_G3 = 0x31,
    EFFECT_MODEL_COMMAND_GT3_OPAQUE = 0x35,
    EFFECT_MODEL_COMMAND_GT3_SEMITRANSPARENT = 0x37,
    EFFECT_MODEL_COMMAND_G4 = 0x39
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

s32 effect_model_init_sections(psyq_tmd_header_t* resource, s32 unused_1);
void effect_model_decode_next_command(effect_decoded_model_command_t* out);

#endif
