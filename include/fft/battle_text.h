#ifndef BATTLE_TEXT_H
#define BATTLE_TEXT_H

/* Battle and event-overlay text buffers, glyph rendering and messages. */

#include "fft/menu.h"
#include "fft/text.h"
#include "fft/unit.h"

/* glyph rendering */

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
