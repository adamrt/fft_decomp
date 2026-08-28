#ifndef FFT_MAIN_ZODIAC_H
#define FFT_MAIN_ZODIAC_H

#include "psx/gpu.h"

/* ZODIAC.BIN sprite-table records shared by the builder and polygon helper. */
typedef struct main_zodiac_texture_position {
    s16 x;
    u16 y; /* Bits 8..11 select the texture-page row. */
} main_zodiac_texture_position_t;

typedef struct main_zodiac_screen_position {
    u16 x;
    u16 y;
} main_zodiac_screen_position_t;

typedef struct main_zodiac_sprite_frame {
    u8 u;
    u8 unknown_01;
    u8 v;
    u8 unknown_03;
    s16 width;
    s16 height;
    s16 offset_x;
    s16 offset_y;
} main_zodiac_sprite_frame_t;

typedef struct main_zodiac_scale {
    s16 x;
    s16 y;
} main_zodiac_scale_t;

typedef struct main_zodiac_draw_offset {
    u8 unknown_00[8];
    u16 x;
    u16 y;
} main_zodiac_draw_offset_t;

/* Draw state handed to SCUS Build ZODIAC.BIN (provisional layout). */
typedef struct zodiac_draw_context {
    u32* ot;       /* 0x00: ordering table entry the primitives link into */
    u8 brightness; /* 0x04: r0/g0/b0 of every sprite */
    u8 unknown_05[3];
    s32 link_primitives; /* 0x08: nonzero: link each sprite into the OT */
    s16 scale_x;         /* 0x0c: 12.12 */
    s16 scale_y;         /* 0x0e: 12.12 */
    u8 unknown_10[8];
    main_zodiac_draw_offset_t offset; /* 0x18 */
} zodiac_draw_context_t;

void main_gfx_build_zodiac_bin(zodiac_draw_context_t* context, POLY_FT4* poly);
void main_gfx_call_build_zodiac_bin(zodiac_draw_context_t* context, POLY_FT4* poly);

void main_gfx_build_zodiac_sprite_polygon(POLY_FT4* poly, main_zodiac_texture_position_t* texture,
    main_zodiac_screen_position_t* base, main_zodiac_sprite_frame_t* frame, main_zodiac_scale_t* scale,
    main_zodiac_draw_offset_t* offset);

extern POLY_FT4 g_main_zodiac_primitives[2][30];

#endif
