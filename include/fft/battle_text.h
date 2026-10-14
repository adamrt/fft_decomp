#ifndef BATTLE_TEXT_H
#define BATTLE_TEXT_H

/* Battle and event-overlay text buffers, glyph rendering and messages. */

#include "fft/menu.h"
#include "fft/unit.h"

/* glyph rendering */
/* Origin and row geometry used by the EVENT menu text image renderers. */
typedef struct battle_menu_text_image_bounds {
    u16 x;
    u16 y;
    u16 row_stride;
    s16 line_height;
} battle_menu_text_image_bounds_t;

/* Pen of the BUNIT, CARD, EQUIP and JOBSTTS *_text_render_glyph_to_4bpp_image: x in pixels (two per byte),
 * y in rows, and the image's row stride in 16-bit values. */
typedef struct battle_text_image_position {
    s16 x;          /* 0x00 */
    s16 y;          /* 0x02 */
    s16 row_stride; /* 0x04 */
} battle_text_image_position_t;
typedef char battle_text_image_position_size_must_be_6[(sizeof(battle_text_image_position_t) == 6) ? 1 : -1];

void menu_text_render_encoded_ids_to_image(u8* image, battle_menu_text_image_bounds_t* rect, s32 unused, s32 max_chars,
    u8* font, s16* ids, s32 count, s16 glyph, s32 unused_flags);
s32 menu_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const battle_text_image_position_t* position, s32 style);
void menu_text_render_id_rows_to_vram(u8* text_table, s16* list, RECT* rect, s32 style);

s32 battle_text_read_packed_nibble(const u8* data, s32 index, s32 row, s32 stride);
void battle_write_packed_nibble(u8* data, s32 index, s32 row, s32 stride, s32 value);

/* numbers */

/* text sections */
/* Text variables $27 and $28 (rows of glyph pixels drawn, 0x10 by default),
 * bound separately: battle_menu_display_text also takes the table base, and
 * element spellings let GCC address them from it. */

/* MAIN selects the BATTLE address of text_find_entry through this existing alias. */
u8* battle_text_find_entry(u32 text_id);

/* messages */
extern s32 g_battle_text_awaiting_input;
extern s32 g_battle_text_current_line;
extern u16 g_battle_text_speaker_swap_unit_a;
extern u16 g_battle_text_speaker_swap_unit_b;

void battle_text_run_dialog_thread(void);

/* typewriter entrypoint */

/* spell quotes */

/* diagnostics */
extern const char g_battle_text_heap_dp_format[];     /* "DP %x\n" */
extern const char g_battle_text_heap_remain_format[]; /* "HEAP REMAIN %x MAX %x\n" */
extern const char g_battle_text_heap_task_format[];   /* "TASK %d %x\n" */
extern const char g_battle_text_min_heap_format[];    /* "MIN HEAP %x " */
extern char g_battle_text_wait_for_allocation_message[];

#endif
