#ifndef PORTRAIT_H
#define PORTRAIT_H

/* Unit portraits: the EVTFACE.BIN and WLDFACE.BIN layouts, the party portraits ATTACK and REQUIRE upload, and the
 * portrait quads and their cross-fades. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* portrait files */
/* EVENT/WLDFACE.BIN is four of these sheets: forty 32x48 portraits at 4 bits
 * per pixel, eight across and five down, then sixteen rows of four palettes.
 * Palette n is portrait n's; rows 10-15 hold the other palettes of the monster
 * portraits, one row per monster with the palette number as the column. */
typedef struct portrait_sheet {
    u8 pixels[240][128];       /* 0x0000; 4bpp, 256 pixels per row */
    palette_t palettes[16][4]; /* 0x7800; four 16-color palettes per row */
} portrait_sheet_t;
typedef char portrait_sheet_size_must_be_32768[(sizeof(portrait_sheet_t) == 32768) ? 1 : -1];

/* Unreferenced in both programs: uploads one EVTFACE.BIN block and its palettes. */
void portrait_load_image(s32 portrait_id);

/* party portraits */
extern u16 g_portrait_index_by_sprite_set[];
extern RECT g_portrait_sheet_rect_0;
extern RECT g_portrait_sheet_rect_1;
extern RECT g_portrait_sheet_rect_2;
extern RECT g_portrait_sheet_rect_3;
extern u8 g_portrait_cell_pixels[768];
extern RECT g_portrait_cell_rect;
extern RECT g_portrait_palette_rect;
extern RECT g_portrait_special_rect;
extern RECT g_portrait_special_palette_rect;
extern u8 g_portrait_special_unit_id;

/* WLDFACE portrait for portrait slot 20: ATTACK sets it from the scenario
 * record and portrait_prepare_party_textures uploads it before the
 * battle; BATTLE never reads it. */
extern u8 g_portrait_special_wldface_id;

/* WORLD resolves the party record to a WLDFACE portrait code with palette bits. */
s32 portrait_get_party_unit_code(s32 party_index);

void portrait_load_party(void);
void portrait_prepare_party_textures(void);

/* portrait quads */
void portrait_build_transition(const RECT* texture_rect, s32* transition, const s32* first_portrait,
    const s32* second_portrait, palette_t palettes[2], POLY_FT4* poly, s32 direction);

/* ATTACK, REQUIRE, WORLD and the EVENT library each build their own body. */
void portrait_build_poly(s32 portrait_id, POLY_FT4* poly);

/* BATTLE's body takes a battle id and textures the quad from the unit's spritesheet page; WORLD's takes a
 * portrait code into the WLDFACE.BIN sheets. */
void portrait_build_unit_poly(POLY_FT4* poly, s32 id);

#endif
