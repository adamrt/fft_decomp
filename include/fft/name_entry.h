#ifndef NAME_ENTRY_H
#define NAME_ENTRY_H

/* The name entry screen (new units, renames): its keyboard, cursors and the confirmation menu. */

#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* screen */
typedef enum name_entry_state {
    NAME_ENTRY_STATE_EDITING = 0,
    NAME_ENTRY_STATE_CONFIRMATION = 1,
    NAME_ENTRY_STATE_FINISHED = 0xff
} name_entry_state_e;

extern void (*g_name_entry_state_handlers[])(void);
extern u8 g_name_entry_state;       /* name_entry_state_e */
extern u8 g_name_entry_saved_state; /* name_entry_state_e */
extern s8 g_name_entry_started;
extern s8 g_name_entry_last_state;          /* last drawn name-entry state */
extern s8 g_name_entry_keyboard_last_state; /* last drawn state */
extern u8 g_name_entry_input_delay;
extern const u8* g_name_entry_original_name;
extern u8 g_name_entry_used_original_name;
extern RECT g_name_entry_screen_image_rect;
extern RECT g_name_entry_text_vram_rect;

u8* name_entry_run_screen(const u8* list, u32* image);
u8* name_entry_keyboard_frame(const u8* list, s32 unused_1);

/* keyboard */
/* Editing keys of name_entry_apply_key; any other key is the character
 * written at the cursor. */
typedef enum name_entry_key {
    NAME_ENTRY_KEY_BACKSPACE = -1,
    NAME_ENTRY_KEY_DELETE = -2,
    NAME_ENTRY_KEY_INSERT_SPACE = -3,
    NAME_ENTRY_KEY_CURSOR_LEFT = -4,
    NAME_ENTRY_KEY_CURSOR_RIGHT = -5
} name_entry_key_e;

extern s16 g_name_entry_text_row_ids[];
extern u8 g_name_entry_character_rows[];
extern s16 g_name_entry_key_codes[];
extern s16 g_name_entry_key_ids[];
extern u8 g_name_entry_char_to_special_column[];
extern u8 g_name_entry_special_to_char_column[];
extern u8 g_name_entry_cursor_x_positions[];
extern u8 g_name_entry_cursor;
extern u8 g_name_entry_cursor_index;
extern u8 g_name_entry_list_cursor;
extern u16 g_name_entry_key_cursor_anim;
extern u16 g_name_entry_text_cursor_anim;
extern u16 g_name_entry_keyboard_text_cursor_anim;

/* confirmation */
extern menu_entry_t g_name_entry_confirm_menu;
extern u8 g_name_entry_confirm_initialized;
extern u8 g_name_entry_confirm_finished;
extern s16 g_name_entry_confirm_result;
extern u8 g_name_entry_confirm_message_shown;

void name_entry_confirm_menu(void);

#endif
