#ifndef MENU_TEXT_H
#define MENU_TEXT_H

/* The menu library's own text routines: entry text bindings and three-column layouts, its pen and visible line range,
 * and its variants of the text cursor, entry lookup, glyph and number rendering and the text threads. */

#include "fft/text/text.h"
#include "psx/types.h"

struct menu_entry;
struct menu_number_entry;

/* text records */
/* Record reached through menu_entry_t::context by menu_text_entry_wait_thread and WORLD's
 * menu_screen_text_entry_thread: they call prepare, then start text_thread_id with speaker_id as its third
 * parameter. menu_text_entry_wait_thread also supplies speaker_id as the fourth parameter. */
typedef struct menu_text_binding {
    void (*prepare)(void); /* 0x00 */
    u16 speaker_id;        /* 0x04: unit whose portrait speaks (roster slot) */
    u16 text_thread_id;    /* 0x06 */
} menu_text_binding_t;
typedef char menu_text_binding_size_must_be_8[sizeof(menu_text_binding_t) == 8 ? 1 : -1];

/* Values of menu_text_layout_t.mode[]: what a column of the layout draws. */
typedef enum menu_text_column_mode {
    MENU_TEXT_COLUMN_TEXT = 0,         /* text ids, drawn by text_render_entry_to_image */
    MENU_TEXT_COLUMN_NUMBER = 1,       /* two-digit numbers, drawn by text_draw_number */
    MENU_TEXT_COLUMN_HIDDEN = 2,       /* the column is skipped */
    MENU_TEXT_COLUMN_NUMBER_GLYPHS = 3 /* two-digit numbers, drawn by text_draw_large_number_glyphs */
} menu_text_column_mode_e;

/* Three-column text layout walked by menu_draw_text_columns and WORLD's menu_screen_draw_text_columns,
 * reached through menu_entry_t::context when the entry draws a list. Column i draws mode[i] at x[i] from
 * text_ids[i] in text_colors[i], both indexed by row plus the row offset. */
typedef struct menu_text_layout {
    s16 row_count;       /* 0x00 */
    s16 hidden_rows;     /* 0x02: rows beyond the visible page (count - visible, 0 when
                            all fit); row_offset is clamped to it */
    s16 x[3];            /* 0x04 */
    s16 mode[3];         /* 0x0a: menu_text_column_mode_e */
    s16* text_ids[3];    /* 0x10 */
    s16* text_colors[3]; /* 0x1c */
} menu_text_layout_t;
typedef char menu_text_layout_size_must_be_40[sizeof(menu_text_layout_t) == 40 ? 1 : -1];

/* pen and line range */
extern glyph_blit_t g_menu_text_pen;

/* Visible line range [first_line, line_limit); BATTLE and WORLD each define these. */
extern s32 g_menu_text_first_line;
extern s32 g_menu_text_line_limit;

/* Added to glyph palette indices while menu text is drawn. */
extern s32 g_menu_text_palette_offset;

/* cursor and entries */
/* The menu renderer's own copy of the same cursor helper. */
const u8* menu_text_advance_cursor_with_backreferences(text_reader_state_t* state, const u8* cursor);
const u8* menu_text_find_entry_by_index(const u8* data, s16 count, s32 unused_2);
void menu_text_set_origin(s32 x, s32 y);

/* rendering */
s32 menu_text_render_glyph_to_4bpp_image(s32 glyph_id, u8* image, const text_image_position_t* position, s32 style);
void menu_text_render_encoded_ids_to_image(u8* image, const text_image_bounds_t* bounds, s32 unused, s32 max_chars,
    const u8* text_table, const s16* text_ids, s32 count, s16 glyph, s32 unused_flags);
void menu_text_render_decimal_entry_list(
    u8* pixels, const struct menu_number_entry* entries, glyph_blit_t* out, s32 count);
void menu_text_render_signed_decimal_entries(
    u8* pixels, struct menu_number_entry* entries, glyph_blit_t* out, s32 count);

/* text threads */
/* Start a dialog only when its scheduler slot is idle. */
void menu_text_thread_start_if_idle(s32 thread_id, s32 dialog_type, s32 text_id, s32 speaker_id);

/* An entry that shows a message through its menu_text_binding_t, then waits for the text thread. */
void menu_text_entry_wait_thread(void);

/* Applies scroll input, rebuilds the entry's text image and uploads the rows that changed. */
u8* menu_text_update_page_scroll(struct menu_entry* menu, s32* first_row, s32* render_pending, s32* unused_cursor);

/* Builds and runs the AT list screen; OPTION and WORLD each build their own body. */
void menu_text_list_thread(void);

#endif
