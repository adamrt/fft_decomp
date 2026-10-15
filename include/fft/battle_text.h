#ifndef BATTLE_TEXT_H
#define BATTLE_TEXT_H

/* Battle and event-overlay text buffers, glyph rendering and messages. */

#include "fft/menu.h"
#include "fft/text.h"
#include "fft/unit.h"

/* glyph rendering */

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

/* typewriter entrypoint */

/* spell quotes */

/* diagnostics */
extern const char g_battle_text_heap_dp_format[];     /* "DP %x\n" */
extern const char g_battle_text_heap_remain_format[]; /* "HEAP REMAIN %x MAX %x\n" */
extern const char g_battle_text_heap_task_format[];   /* "TASK %d %x\n" */
extern const char g_battle_text_min_heap_format[];    /* "MIN HEAP %x " */

#endif
