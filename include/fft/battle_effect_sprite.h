#ifndef FFT_BATTLE_EFFECT_SPRITE_H
#define FFT_BATTLE_EFFECT_SPRITE_H

#include "psx/gpu.h"
#include "psx/gte.h"

/* The four GTE input corners at 0x801bbf40; only the leading x/y word of each
 * is written here, straight from the sprite frame record. */
typedef union battle_effect_corner {
    SVECTOR vector;
    s32 packed;
} battle_effect_corner_t;

/* Variable-offset sprite table consumed by 0x801a5798. */
typedef struct battle_effect_sprite_table_header {
    s16 offsets_offset;
} battle_effect_sprite_table_header_t;

typedef struct battle_effect_sprite_frame {
    u16 flags; /* 0x00; 0x200 selects the semi-transparent code, 0xe0 the texture-page mode bits */
    u8 unknown_02[6];
    u8 u;           /* 0x08 */
    u8 v;           /* 0x09 */
    u8 width;       /* 0x0a */
    u8 height;      /* 0x0b */
    s32 corners[4]; /* 0x0c; packed x/y pairs for the four quad corners */
} battle_effect_sprite_frame_t;

/* The POLY_FT4 the slot double-buffers. include/psx/gpu.h spells the tag as one
 * word and the colour as four bytes; this routine writes the tag's length byte
 * and copies the colour as a word, so both are named here. */
typedef struct battle_effect_quad_prim {
    u8 unknown_00[3]; /* 0x00; tag address */
    u8 length;        /* 0x03; tag word count */
    union {
        u32 packed;
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 code;
        } field;
    } color; /* 0x04 */
    s32 xy0; /* 0x08 */
    u8 u0;
    u8 v0;
    u16 clut; /* 0x0e */
    s32 xy1;  /* 0x10 */
    u8 u1;
    u8 v1;
    u16 tpage; /* 0x16 */
    s32 xy2;   /* 0x18 */
    u8 u2;
    u8 v2;
    u16 pad2;
    s32 xy3; /* 0x20 */
    u8 u3;
    u8 v3;
    u16 pad3;
} battle_effect_quad_prim_t;

/* The 0xcc effect slot of include/fft/effect.h, with the fields this sprite
 * path uses named. The colour word's top byte is the sprite frame index: the
 * quad's code byte overwrites it immediately after the word copy. */
typedef struct battle_effect_sprite_slot {
    u8 unknown_00[4];
    DR_MODE modes[2][2];                /* 0x04; two draw modes per buffer */
    battle_effect_quad_prim_t quads[2]; /* 0x34 */
    union {
        u32 packed;
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 frame;
        } field;
    } color;  /* 0x84 */
    u16 clut; /* 0x88 */
    u8 unknown_8a[4];
    s16 angle; /* 0x8e */
    u8 unknown_90[0xc];
    s32 x; /* 0x9c */
    s32 y; /* 0xa0 */
    s32 z; /* 0xa4 */
    u8 unknown_a8[0x24];
} battle_effect_sprite_slot_t;

typedef char battle_effect_sprite_slot_size_must_be_0xcc[sizeof(battle_effect_sprite_slot_t) == 0xcc ? 1 : -1];
typedef char
    battle_effect_quad_prim_size_must_match_ft4[sizeof(battle_effect_quad_prim_t) == sizeof(POLY_FT4) ? 1 : -1];

#endif
