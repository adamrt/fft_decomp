#ifndef BAR_H
#define BAR_H

/* The Bar in a town: its menu rows and level handlers, and the rumor list's detail level. */

#include "fft/world/menu_stack.h"
#include "psx/types.h"

/* bar menu */
/* Row kinds of the bar's menu (bar_build_rows); the row text is TEXT_ID_WORLD_MENU_BASE + kind. */
typedef enum bar_row {
    BAR_ROW_RUMORS = 0,
    BAR_ROW_AVAILABLE_PROPOSITIONS = 1,
    BAR_ROW_RETURNED = 2,
    BAR_ROW_ACTIVE_PROPOSITIONS = 3,
    BAR_ROW_LEAVE = 4,
    BAR_ROW_COUNT = 5
} bar_row_e;

/* The bar owns a cursor, side label and row image. At +0x0c it keeps a
 * welcome-message request, unlike the image index in Brave Story's menu. */
typedef struct bar_menu_level {
    s32 cursor_window;   /* 0x00 */
    s32 label_window;    /* 0x04 */
    s32 rows_image;      /* 0x08 */
    s32 restore_welcome; /* 0x0c */
    u8 _unused_10[8];
    s32 mode;           /* 0x18; menu_stack_window_mode_e, BROWSE or LEAVE_MESSAGE */
    s32 selected_entry; /* 0x1c */
    u8 _unused_20[0x10];
    s32 entry_count;            /* 0x30 */
    s32 entries[BAR_ROW_COUNT]; /* 0x34; bar_row_e */
} bar_menu_level_t;
typedef char bar_menu_level_size_must_be_72[sizeof(bar_menu_level_t) == 72 ? 1 : -1];

void bar_build_rows(bar_menu_level_t* level);
void bar_open_menu(void);
void bar_resume_menu(menu_stack_record_t* level);
void bar_update_menu(menu_stack_record_t* level);

/* Suspend handler of the bar menu level: dim its windows (palettes 10, 6, 2) and show the pressed cursor. */
void bar_suspend_menu(menu_stack_record_t* level);

/* rumors */
/* The RUMOR_DETAIL level pushed by open_rumor_detail. Its close handler
 * rumor_update_detail reads the saved parent window and render indices. */
typedef struct rumor_detail_level {
    s32 rumor_index;
    s32 _unused_04;
    s32 first_window;
    s32 second_window;
    s32 render_index;
} rumor_detail_level_t;

/* Withhold every button before a list level opens a message over itself. */
void rumor_suspend_list(menu_stack_record_t* unused_level);
void rumor_update_list_input(menu_stack_record_t* level);
void rumor_resume_list(menu_stack_record_t* level);
void rumor_update_detail(menu_stack_record_t* level);

#endif
