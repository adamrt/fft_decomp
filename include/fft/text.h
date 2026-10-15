#ifndef TEXT_H
#define TEXT_H

/* Text rendering shared by BATTLE and WORLD: glyph images, message threads
 * and the message strings they print. */

#include "fft/gfx.h"
#include "psx/types.h"

/* glyph positions and bounds */
/* Pen of the BUNIT, CARD, EQUIP and JOBSTTS *_text_render_glyph_to_4bpp_image: x in pixels (two per byte),
 * y in rows, and the image's row stride in 16-bit values. */
typedef struct battle_text_image_position {
    s16 x;          /* 0x00 */
    s16 y;          /* 0x02 */
    s16 row_stride; /* 0x04 */
} battle_text_image_position_t;
typedef char battle_text_image_position_size_must_be_6[(sizeof(battle_text_image_position_t) == 6) ? 1 : -1];

/* Origin and row geometry used by the EVENT menu text image renderers. */
typedef struct battle_menu_text_image_bounds {
    u16 x;
    u16 y;
    u16 row_stride;
    s16 line_height;
} battle_menu_text_image_bounds_t;

/* messages */
extern char g_text_wait_for_allocation_message[];

/* per-module bodies: each module links its own copy under the shared name. */
void text_draw_large_number_glyphs_variant(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos);
void text_init_section_pointers_variant(void);
void text_run_dialog_thread_variant(void);
void text_run_spell_quote_thread_variant(void);
s32 menu_text_render_glyph_to_4bpp_image_variant(
    s32 glyph_id, u8* image, const battle_text_image_position_t* position, s32 style);

/* rendering */
void menu_text_render_encoded_ids_to_image(u8* image, battle_menu_text_image_bounds_t* rect, s32 unused, s32 max_chars,
    u8* font, s16* ids, s32 count, s16 glyph, s32 unused_flags);
void text_concatenate_entries(const u8* text_table, u8* out, s16* list, s32 separate);
void text_render_id_rows_to_vram(u8* text_table, s16* list, RECT* rect, s32 style);
extern u8* g_text_saved_pointers[32];
void text_set_blit_source_rect(s16 source_x, s16 source_y, s16 width, s16 height);
extern glyph_blit_t g_text_glyph_source;

/* rendering */
extern glyph_blit_t g_text_glyph_dest;

#endif
