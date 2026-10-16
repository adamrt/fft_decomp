#ifndef BAR_H
#define BAR_H

/* The Bar in a town: its menu, entering and leaving. */

#include "fft/menu_stack.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The RUMOR_DETAIL level pushed by rumor_push_detail_level. Its close handler
 * rumor_wait_detail_then_pop reads the saved parent window and render indices. */
typedef struct rumor_detail_level {
    s32 rumor_index;
    s32 _unused_04;
    s32 first_window;
    s32 second_window;
    s32 render_index;
} rumor_detail_level_t;

/* Withhold every button (g_menu_blocked_buttons = -1) before a list level opens a message over itself. The list
 * steps hand each its level, which it does not read: the parameter is only what the target passes in $a0. */
void rumor_block_list_buttons(menu_stack_panel_level_t* level);

/* Row kinds of the bar's menu (bar_build_rows); the row text is TEXT_ID_WORLD_MENU_BASE + kind. */
typedef enum bar_row {
    BAR_ROW_RUMORS = 0,
    BAR_ROW_AVAILABLE_PROPOSITIONS = 1,
    BAR_ROW_RETURNED = 2,
    BAR_ROW_ACTIVE_PROPOSITIONS = 3,
    BAR_ROW_LEAVE = 4,
} bar_row_e;

extern s32 g_bar_saved_cursor;
extern u8 g_bar_row_help_text_ids[];
extern s32* g_rumor_location_masks;
void bar_build_rows(menu_stack_window_level_t* level);
void bar_resume_level(menu_stack_record_t* level);

/* Publish handler of the bar menu level: dim its windows (palettes 10, 6, 2) and show the pressed cursor. */
void bar_suspend_level(menu_stack_window_level_t* level);

#endif
