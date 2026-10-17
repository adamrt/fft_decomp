#ifndef BAR_H
#define BAR_H

/* The Bar in a town: its menu rows and level handlers, and the rumor list's detail level. */

#include "fft/menu_stack.h"
#include "psx/types.h"

/* bar menu */
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

void bar_build_rows(menu_stack_window_level_t* level);
void bar_resume_level(menu_stack_record_t* level);
void bar_handle_input(menu_stack_record_t* level);

/* Suspend handler of the bar menu level: dim its windows (palettes 10, 6, 2) and show the pressed cursor. */
void bar_suspend_level(menu_stack_record_t* level);

/* rumors */
/* The RUMOR_DETAIL level pushed by rumor_push_detail_level. Its close handler
 * rumor_wait_detail_then_pop reads the saved parent window and render indices. */
typedef struct rumor_detail_level {
    s32 rumor_index;
    s32 _unused_04;
    s32 first_window;
    s32 second_window;
    s32 render_index;
} rumor_detail_level_t;

extern s32* g_rumor_location_masks;

/* Withhold every button before a list level opens a message over itself. */
void rumor_block_list_buttons(menu_stack_record_t* unused_level);
void rumor_handle_list_input(menu_stack_record_t* level);
void rumor_restore_list_prompt(menu_stack_record_t* level);
void rumor_wait_detail_then_pop(menu_stack_record_t* level);

#endif
