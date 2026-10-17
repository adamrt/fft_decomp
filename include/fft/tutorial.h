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
/* The tutorial command stream. Commands below 0x10 are one-byte shifts; the rest are the id (1 byte) followed by
 * their parameters: 0x10 message id (2) and y (1), 0x11 message id (2), 0x13 x, y, size, width mod and height mod
 * (1 each), 0x15 time (1). */
enum {
    TUTORIAL_CMD_PRESS_BUTTON_LIMIT = 0x10,
    TUTORIAL_CMD_DISPLAY_MESSAGE = 0x10,
    TUTORIAL_CMD_CHANGE_DIALOG = 0x11,
    TUTORIAL_CMD_WAIT_FOR_MESSAGE = 0x12,
    TUTORIAL_CMD_HIGHLIGHT_RING = 0x13,
    TUTORIAL_CMD_FADE_OUT_HIGHLIGHT = 0x14,
    TUTORIAL_CMD_WAIT_TIME = 0x15,
    TUTORIAL_CMD_WAIT_FOR_BUTTON = 0x16,
    TUTORIAL_CMD_END = 0x17,
};

/* Serialized tutorial commands; opcode selects the operand record. */
typedef struct tutorial_message_instruction {
    u8 opcode;
    u8 text_id_le[2];
    u8 y_offset;
} tutorial_message_instruction_t;
typedef char tutorial_message_instruction_size_must_be_4[sizeof(tutorial_message_instruction_t) == 4 ? 1 : -1];

typedef struct tutorial_dialog_instruction {
    u8 opcode;
    u8 text_id_le[2];
} tutorial_dialog_instruction_t;
typedef char tutorial_dialog_instruction_size_must_be_3[sizeof(tutorial_dialog_instruction_t) == 3 ? 1 : -1];

typedef struct tutorial_circle_instruction {
    u8 opcode;
    u8 x;
    u8 y;
    u8 radius;
    u8 scale_x;
    u8 scale_y;
} tutorial_circle_instruction_t;
typedef char tutorial_circle_instruction_size_must_be_6[sizeof(tutorial_circle_instruction_t) == 6 ? 1 : -1];

typedef struct tutorial_wait_instruction {
    u8 opcode;
    u8 frames;
} tutorial_wait_instruction_t;
typedef char tutorial_wait_instruction_size_must_be_2[sizeof(tutorial_wait_instruction_t) == 2 ? 1 : -1];

/* The current game is serialized here while a tutorial runs; its message
 * text and command stream follow the saved bytes at +0x1e00/+0x1e04. */
typedef struct tutorial_state {
    save_image_t saved_game;
    u8* messages;
    u8* script;
} tutorial_state_t;
typedef char tutorial_state_size_must_be_7688[sizeof(tutorial_state_t) == 7688 ? 1 : -1];

extern tutorial_state_t g_tutorial_state;
extern u8 g_tutorial_id;
extern u16 g_tutorial_pressed_button;
extern s16 g_tutorial_wait_time_remaining;
extern s16 g_tutorial_highlight_brightness;
extern s16 g_tutorial_buttons;
extern u16 g_tutorial_latched_button;
extern u8 g_tutorial_activation_timer;

void tutorial_start(s32 index);
void tutorial_load_files(s32 index, tutorial_state_t* record);
s32 tutorial_step(void);
s32 tutorial_get_buttons(void);

/* tutorial events */
s32 tutorial_is_current_event(void);
void tutorial_pulse_wait_value(s32 value);

#endif
