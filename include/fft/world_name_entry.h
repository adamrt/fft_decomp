#ifndef WORLD_NAME_ENTRY_H
#define WORLD_NAME_ENTRY_H

/* WORLD's name entry keyboard. */

#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Editing keys of world_name_apply_entry_key; any other key is the character
 * written at the cursor. */
typedef enum world_name_entry_key {
    WORLD_NAME_ENTRY_KEY_BACKSPACE = -1,
    WORLD_NAME_ENTRY_KEY_DELETE = -2,
    WORLD_NAME_ENTRY_KEY_INSERT_SPACE = -3,
    WORLD_NAME_ENTRY_KEY_CURSOR_LEFT = -4,
    WORLD_NAME_ENTRY_KEY_CURSOR_RIGHT = -5,
} world_name_entry_key_e;

/* name */
extern u8 g_world_name_entry_character_rows[];
extern menu_instruction_t g_world_name_entry_display_script;
extern menu_instruction_t g_world_name_entry_keyboard_script;
extern s8 g_world_name_entry_started; /* name entry started */
extern u16 g_world_name_keyboard_text_cursor_anim;
extern u16 g_world_name_entry_text_cursor_anim;
extern RECT g_world_name_entry_screen_image_rect;
extern s16 g_world_name_entry_key_codes[];
extern u8 g_world_name_entry_confirm_message_shown;
extern u16 g_world_name_entry_key_cursor_anim;
extern u8 g_world_name_entry_char_to_special_column[];
extern u8 g_world_name_entry_special_to_char_column[];
extern u8 g_world_name_entry_cursor_x_positions[];
extern RECT g_world_name_entry_text_vram_rect;
extern s16 g_world_name_entry_text_row_ids[];
extern menu_entry_t g_world_name_confirm_menu;
extern u8 g_world_name_confirm_initialized;
extern s8 g_world_name_keyboard_last_state; /* last drawn state */
extern s8 g_world_name_entry_last_state;    /* last drawn name-entry state */
extern u8 g_world_name_entry_input_delay;
extern u8 g_world_name_confirm_finished;
extern u8 g_world_name_used_original_name;
extern s16 g_world_name_entry_key_ids[];
extern void (*g_world_name_entry_state_handlers[])(void);
extern u8 g_world_name_entry_state;
extern u8 g_world_name_entry_saved_state;
extern u8 g_world_name_entry_cursor_index;
extern u8 g_world_name_entry_list_cursor;
extern u8 g_world_name_entry_cursor;
extern u8 g_world_name_entry_text[17];
extern const u8* g_world_name_entry_original_name;
extern s16 g_world_name_confirm_result;
void world_name_run_confirmation_menu(void);
u8* world_name_run_entry_screen(const u8* list, u32* image);
u8* world_name_run_entry_keyboard_frame(const u8* list, s32 unused_1);

#endif
