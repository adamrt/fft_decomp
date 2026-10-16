#ifndef WORLD_CARD_H
#define WORLD_CARD_H

/* WORLD's memory card screen: the card and mode selection, its messages and prompts, the save slot list, and the save
 * images it reads and writes. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_card.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/kernel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* memory card screen */
/* Step of the WORLD memory card screen (save_run_screen). */
typedef enum save_step {
    SAVE_STEP_SELECT_CARD = 0, /* memory card slot 1 or 2 */
    SAVE_STEP_SELECT_MODE = 1, /* save or load */
    SAVE_STEP_SCANNING = 2,
    SAVE_STEP_SELECT_SAVE = 3, /* the save slot list takes input */
    SAVE_STEP_LOADING = 4,
    SAVE_STEP_SAVING = 5,
    SAVE_STEP_LEAVE = 9,   /* starts the fade out */
    SAVE_STEP_LEFT = 0xff, /* no step runs while the screen fades */
} save_step_e;

extern u8 g_save_menu_step;           /* save_step_e */
extern s16 g_save_menu_request_state; /* world_map_request_state_e */
extern u8 g_save_menu_result;
extern u8 g_save_mode;
extern s16 g_save_mode_select_enabled;
extern s16 g_save_active_slot;
extern RECT g_save_screen_background_tile;
extern menu_entry_t g_save_slot_select_menu[2];
extern u8 g_save_slot_select_running;
extern s16 g_save_slot_select_timer;
extern u8 g_save_slot_cursor_row;
extern menu_entry_t g_save_mode_select_menu;
extern u8 g_save_mode_select_running;
extern s8 g_save_slot_scan_active;
extern s8 g_save_slot_scan_index;
extern s8 g_save_scan_prompt_active;
extern s8 g_save_scan_start_frame;
extern s8 g_save_probe_result;
extern struct DIRENTRY* g_save_directory_buffer;
extern s8 g_save_free_block_count;
extern s8 g_save_file_count;
extern s8 g_save_selectable_slot_count;
void save_init_screen(void);
s32 save_run_screen(s32 mode_select_enabled);
void save_close_stub(void);

/* messages and prompts */
/* Message entries selected by save_mode_select_step. */
extern menu_entry_t g_save_unavailable_message;
extern menu_entry_t g_save_no_data_message;
/* A pointer cell, not a record: cleared to 0, tested, and assigned
 * g_save_unavailable_message / g_save_no_data_message by save_mode_select_step. */
extern menu_entry_t* g_save_pending_message;
extern menu_entry_t g_save_saving_message;
extern menu_entry_t g_save_loading_message;
extern menu_entry_t g_save_checking_message;
extern menu_entry_t g_save_formatting_message;
extern menu_entry_t g_save_load_error_message;
extern menu_entry_t g_save_write_error_message;
extern menu_entry_t g_save_access_error_message;
extern s8 g_save_error_message_closing;
/* Two-entry confirmation chains; the choice is entry 1 selected_index. */
extern menu_entry_t g_save_format_prompt[2];
extern menu_entry_t g_save_overwrite_confirm_prompt[2];
extern menu_entry_t g_save_load_confirm_prompt[2];
extern menu_entry_t* g_save_confirm_prompt;
extern u8 g_save_overwrite_prompt_running;
extern u8 g_save_load_prompt_running;
extern s8 g_save_format_delay_counter;
extern u8 g_save_load_step_state;
extern u8 g_save_write_step_state;
extern u8 g_save_load_failed;
extern u8 g_save_write_failed;
/* Cleared with the load-failed flag when a card load starts; nothing reads it. */
extern u8 g_save_load_unread_flag;  /* cleared with the card load and save failure flags; never read */
extern u8 g_save_write_unread_flag; /* as g_save_load_unread_flag */

/* save slot list */
extern const char* g_save_file_names[15];
/* Encoded captions addressed directly by save_build_slot_description,
 * immediately after the 15 save-file pointers at g_save_file_names. */
extern u8 g_save_slot_text_table[];
extern save_icon_t g_save_icon_records[15];
extern u8 g_save_slot_descriptions[][0x8C];
extern s16 g_save_slot_description_row_ids[];
extern menu_window_command_t g_save_slot_window_command;
extern menu_window_rect_source_t g_save_slot_window;
extern menu_draw_number_command_t g_save_slot_number_command;
extern menu_draw_number_command_t g_save_playtime_hours_command;
extern menu_draw_number_command_t g_save_playtime_minutes_command;
extern menu_draw_number_command_t g_save_playtime_seconds_command;
extern oriented_quad_t g_save_slot_quad_0;
extern oriented_quad_t g_save_slot_quad_1;
extern oriented_quad_t g_save_slot_quad_2;
extern oriented_quad_t g_save_slot_quad_3;
extern oriented_quad_t g_save_slot_quad_4;
extern point16_t g_save_slot_cursor_point;
extern u16 g_save_slot_cursor_anim;
extern u8 g_save_slot_list_input_ready;
extern s16 g_save_list_scroll_y;
extern s16 g_save_list_scroll_velocity;
extern s16 g_save_list_first_visible_row;

/* save images */
extern save_image_t* g_save_work_buffer;
s32 save_read_file_chunked(const char* filename, u8* destination, u32 size);
void save_build_image(s32 slot);
s32 save_restore_image(s32 partial_load);

#endif
