#ifndef TEXT_H
#define TEXT_H

/* Text shared by BATTLE, WORLD and the menu overlays: the dialog and spell quote threads, decimal glyphs, and the
 * menu library's glyph images and text rows. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* dialog */
/* Task words of a dialog thread, which the DisplayMessage instruction fills
 * before the thread runs; its other starters set only the y offset. */
enum {
    DIALOG_THREAD_WORD_X_OFFSET = 2,
    DIALOG_THREAD_WORD_Y_OFFSET = 3,
    DIALOG_THREAD_WORD_ARROW_OFFSET = 4,
    DIALOG_THREAD_WORD_WINDOW_OPTIONS = 5, /* low nibble picks the opening tween; a high nibble mirrors the tail */
    DIALOG_THREAD_WORD_TEXT_WIDTH = 6,     /* nonzero overrides the measured width */
};

extern s32 g_text_current_line;
extern u16 g_text_speaker_swap_unit_a;
extern u16 g_text_speaker_swap_unit_b;
extern char g_text_wait_for_allocation_message[];

void text_init_section_pointers(void);
/* BATTLE and WORLD each have their own body. */
void text_run_dialog_thread(void);
void text_run_spell_quote_thread(void);

/* decimal glyphs */
/* BATTLE and WORLD each have their own body. */
void text_draw_large_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos);

/* glyph images */
/* Pen of menu_text_render_glyph_to_4bpp_image: x in pixels (two per byte), y in rows, and the image's row stride in
 * 16-bit values. */
typedef struct battle_text_image_position {
    s16 x;          /* 0x00 */
    s16 y;          /* 0x02 */
    s16 row_stride; /* 0x04 */
} battle_text_image_position_t;
typedef char battle_text_image_position_size_must_be_6[(sizeof(battle_text_image_position_t) == 6) ? 1 : -1];

/* Origin and row geometry of the image menu_text_render_encoded_ids_to_image draws into. */
typedef struct battle_menu_text_image_bounds {
    u16 x;
    u16 y;
    u16 row_stride;
    s16 line_height;
} battle_menu_text_image_bounds_t;

/* The source and destination rectangles of the next glyph blit. */
extern glyph_blit_t g_text_glyph_source;
extern glyph_blit_t g_text_glyph_dest;

void text_set_blit_source_rect(s16 source_x, s16 source_y, s16 width, s16 height);
s32 menu_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const battle_text_image_position_t* position, s32 style);
void menu_text_render_encoded_ids_to_image(u8* image, battle_menu_text_image_bounds_t* rect, s32 unused, s32 max_chars,
    u8* font, s16* ids, s32 count, s16 glyph, s32 unused_flags);
void text_render_id_rows_to_vram(u8* text_table, s16* list, RECT* rect, s32 style);
void text_concatenate_entries(const u8* text_table, u8* out, s16* list, s32 separate);

/* menu text state */
/* The text file an overlay loaded. */
extern u8 g_text_data[];
extern u8* g_text_saved_pointers[32];
extern s32 g_text_selection_id;
extern u8 g_text_thread_running_state;

#endif
