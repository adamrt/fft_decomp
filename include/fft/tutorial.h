#ifndef TUTORIAL_H
#define TUTORIAL_H

/* The Tutorial: the world map's Tutorial menu and lists (WLDCORE), WORLD's tutorial player that drives the screens with
 * scripted button presses, and the event library's tutorial checks. */

#include "fft/menu_selection.h"
#include "fft/menu_stack.h"
#include "fft/save.h"
#include "psx/types.h"

/* tutorial menu */
/* Row kinds of the tutorial category list (tutorial_push_menu_level). */
typedef enum tutorial_menu_row {
    TUTORIAL_MENU_ROW_FORMATION = 0,
    TUTORIAL_MENU_ROW_BATTLE = 1,
    TUTORIAL_MENU_ROW_TOPICS = 2,
    TUTORIAL_MENU_ROW_LEAVE = 3,
    TUTORIAL_MENU_ROW_COUNT = 4
} tutorial_menu_row_e;

enum { TUTORIAL_FORMATION_COUNT = 7, TUTORIAL_BATTLE_COUNT = 8 };

/* The Tutorial category menu: two animated windows and an image containing
 * its four rows. */
typedef struct tutorial_menu_level {
    s32 cursor_window;
    s32 label_window; /* 0x04 */
    s32 rows_image;   /* 0x08 */
    s32 _unknown_0c;  /* set to 24 by the push; no reader found */
    u8 _unused_10[8];
    s32 mode; /* 0x18; menu_stack_window_mode_e */
    s32 selected_entry;
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[TUTORIAL_MENU_ROW_COUNT];
} tutorial_menu_level_t;
typedef char tutorial_menu_level_size_must_be_68[sizeof(tutorial_menu_level_t) == 68 ? 1 : -1];

void tutorial_open_topic_list(void);
void tutorial_handle_topic_list_input(menu_stack_record_t* level);
void tutorial_restore_topic_list_buttons(menu_stack_record_t* level);
void tutorial_wait_topic_then_pop(menu_stack_record_t* level);
void tutorial_block_topic_list_buttons(menu_stack_record_t* unused_level);
void tutorial_block_formation_list_buttons(menu_stack_record_t* unused_level);
void tutorial_handle_formation_list_input(menu_stack_record_t* level);
void tutorial_restore_formation_list_buttons(menu_stack_record_t* level);
void tutorial_block_battle_list_buttons(menu_stack_record_t* level);
void tutorial_handle_battle_list_input(menu_stack_record_t* level);
void tutorial_restore_battle_list_buttons(menu_stack_record_t* level);
void tutorial_resume_menu_level(menu_stack_record_t* level);
void tutorial_handle_menu_input(menu_stack_record_t* level);

/* Suspend handler of the tutorial categories level; a second copy of the bar menu body. */
void tutorial_suspend_menu_level(menu_stack_record_t* level);

/* tutorial player */
/* The current game is serialized here while a tutorial runs; its message
 * text and command stream follow the saved bytes at +0x1e00/+0x1e04. */
typedef struct tutorial_state {
    save_image_t saved_game;
    u8* messages;
    u8* script;
} tutorial_state_t;
typedef char tutorial_state_size_must_be_7688[sizeof(tutorial_state_t) == 7688 ? 1 : -1];

extern u8 g_tutorial_id;
extern u16 g_tutorial_pressed_button;

void tutorial_start(s32 index);
void tutorial_load_files(s32 index, tutorial_state_t* record);
s32 tutorial_step(void);
s32 tutorial_get_buttons(void);

/* tutorial events */
s32 tutorial_is_current_event(void);
void tutorial_pulse_wait_value(s32 value);

#endif
