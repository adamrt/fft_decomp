#ifndef EVENT_CARD_H
#define EVENT_CARD_H

/* EVENT/CARD.OUT: the BATTLE overlay of the memory-card save screen. */

#include "fft/gfx.h"
#include "fft/main_card.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "psx/kernel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* save image */
/* The 0x1e00-byte serialized save is parity-covered in 0x80-byte blocks;
 * each block contributes 0x400 one-bit reads to the primary parity stream. */
enum {
    SAVE_PARITY_BLOCK_COUNT = 60,
    SAVE_PARITY_BITS_PER_BLOCK = 1024,
    SAVE_PARITY_BLOCK_BYTES = 128,
};

/* 0x1e80 bytes running to g_card_io_open_result: the 0x1e00-byte save image
 * plus a 0x80-byte tail that save_init_menu_state clears with it. */
extern save_image_t g_save_buffer[];
extern save_image_t* g_save_buffer_pointer;
extern save_icon_t g_save_icon_records[15];
extern u8 g_save_title_template[0x16];
extern const char* g_save_file_names[];

/* save menu */
/* g_save_menu_state, the state save_run_menu dispatches on. Only
 * the states whose roles the matching handlers establish are named. */
typedef enum save_menu_state {
    SAVE_MENU_INITIAL_PROMPT = 0,
    SAVE_MENU_SELECT_CARD = 1,
    SAVE_MENU_SCAN_RESULT = 2,
    SAVE_MENU_SCANNING = 3,
    SAVE_MENU_SELECT_SAVE = 4,
    SAVE_MENU_WRITING = 6,
    SAVE_MENU_LEAVE_ALT = 9, /* never stored; handled like LEAVE */
    SAVE_MENU_LEAVE = 10,    /* the player declined or is done: fade the music and the screen out */
    SAVE_MENU_CLOSING = 0xff /* fading out; no state handler runs */
} save_menu_state_e;

extern u8 g_save_menu_state; /* save_menu_state_e */
extern DIRENTRY* g_save_menu_work_buffer;
extern clut_t g_save_menu_clut;
extern s32 g_save_menu_script_callback;
extern s16 g_save_menu_selection_index;
extern s8 g_save_completed;

void save_run_menu(s32 unused);
void save_backdrop_thread(void);

/* prompts */
/*
 * The g_card_save_*_descriptor menu-thread descriptors are 0x3c-byte
 * menu_entry_t menu records. menu_start_managed_thread and its delayed twin
 * start descriptor->thread_entry (+0x28). The yes/no prompts are a message
 * record followed by a choice record whose text_id is 0xd810; the choice's
 * selected_index (+0x74 of the prompt) is the default answer, and the message
 * window.y (+0x0a) places the prompt.
 */
extern menu_entry_t g_save_initial_prompt_descriptor[];
extern menu_entry_t g_save_card_slot_selection_descriptor[];
extern menu_entry_t g_save_checking_message_descriptor[];
extern menu_entry_t g_save_card_error_prompt_descriptor[];
extern menu_entry_t g_save_no_data_prompt_descriptor[];
extern menu_entry_t g_save_format_prompt_descriptor[2];
extern menu_entry_t g_save_formatting_message_descriptor[];
extern menu_entry_t g_save_overwrite_prompt_descriptor[2];
extern menu_entry_t g_save_in_progress_thread_descriptor[];
extern menu_entry_t g_save_failure_thread_descriptor[];
/* A pointer cell, not a record: save_update_scan_result clears it to
 * 0, tests it, and assigns g_save_no_data_prompt_descriptor into it. */
extern menu_entry_t* g_save_slot_scan_prompt_descriptor;
extern s16 g_save_initial_prompt_timer;
extern u8 g_save_initial_prompt_thread_state;
extern u8 g_save_slot_selection_thread_state;
extern s8 g_save_error_prompt_closing;
extern s8 g_save_format_prompt_active;
extern s8 g_save_format_delay_counter;
extern u8 g_save_overwrite_prompt_active;

/* card scan and write */
extern s8 g_save_open_result;
extern s8 g_save_free_block_count;
extern s8 g_save_file_count;
extern s8 g_save_occupied_slot_count;
extern s8 g_save_scan_setup_step;
extern s8 g_save_slot_scan_active;
extern s8 g_save_slot_scan_index;
extern u8 g_save_write_phase;
extern u8 g_save_write_failed;
/* Cleared with the write-failed flag when a save starts; nothing reads it. */
extern u8 g_save_unread_flag; /* cleared when a write starts; never read */

/* slot list */
/* Ten-byte number command in a 12-byte save-menu slot. */
typedef struct save_slot_number_command {
    menu_draw_number_command_t number;
    u8 _unused_0a[2];
} save_slot_number_command_t;
typedef char card_save_slot_number_command_size_must_be_12[sizeof(save_slot_number_command_t) == 12 ? 1 : -1];

extern s16 g_save_slot_index;
extern s16 g_save_list_selected_slot;
extern s16 g_save_list_top_row;
extern u8 g_save_list_cursor_index;
extern s16 g_save_list_scroll_anim_offset;
extern s16 g_save_list_scroll_base_y;
extern s16 g_save_slot_row_y;
extern u16 g_save_slot_cursor_animation;
extern point16_t g_save_slot_cursor_position;
/* Two-line encoded description text, 80 bytes per memory-card slot. */
extern u8 g_save_slot_descriptions[CARD_SAVE_SLOT_COUNT][0x50];
extern s16 g_save_slot_text_row_ids[];
extern save_slot_number_command_t g_save_slot_number_commands[4];
extern oriented_quad_t g_save_slot_quads[];
extern menu_window_command_t g_save_slot_window_command;
extern menu_window_rect_source_t g_save_slot_window_rect_source[];

/* graphics */
/*
 * CARD.OUT's double-buffered graphics context. g_save_gfx_context_base points
 * at the two-element array and g_save_gfx_context at the active buffer.
 *
 * The 0xf4 stride is established twice by the matching code:
 * save_gfx_init_contexts steps its per-buffer offset by 0xf4, and
 * save_gfx_submit_frame toggles the active pointer by the
 * same 0xf4. The field offsets come from the accesses in those routines and
 * in the enqueue helpers: the ordering table at 0x00, the POLY_FT4 pool at
 * 0x10, the tile pool at 0x3c, and the Psy-Q environments at 0x60/0xbc/0xd0
 * (DRAWENV 92 + DISPENV 20 + DR_MODE 12 end exactly at 0xdc).
 */
typedef struct save_gfx_context {
    u32* otag;                   /* 0x00 */
    u8 _unused_04[0x0c];         /* 0x04 */
    POLY_FT4* polygons;          /* 0x10 */
    u8 _unused_14[0x28];         /* 0x14 */
    TILE* tiles;                 /* 0x3c: TILE pool */
    u8 _unused_40[0x20];         /* 0x40 */
    DRAWENV draw_environment;    /* 0x60 */
    DISPENV display_environment; /* 0xbc */
    DR_MODE draw_mode;           /* 0xd0 */
    u8 _unused_dc[0x18];         /* 0xdc */
} save_gfx_context_t;
typedef char card_graphics_context_size_must_be_244[(sizeof(save_gfx_context_t) == 244) ? 1 : -1];

extern save_gfx_context_t* g_save_gfx_context_base;
extern save_gfx_context_t* g_save_gfx_context;
extern u16 g_save_gfx_drawenv_y;
extern u16 g_save_gfx_texture_page;
extern u16 g_save_gfx_draw_clut;
extern rgb24_t g_save_gfx_fade_overlay_color;
extern RECT g_save_gfx_fullscreen_fade_rect;

void save_gfx_init_contexts(save_gfx_context_t* contexts, u32* otags, s32 unused_2, s32 unused_3, s32 unused_4,
    POLY_FT4* polygons, s32 unused_6, s32 unused_7, s32 unused_8, s32 unused_9, s32 unused_10, s32 unused_11,
    s32 unused_12, s32 unused_13, s32 unused_14, s32 unused_15, TILE* tiles, s32 unused_17, s32 unused_18,
    s32 unused_19, s32 unused_20, s32 unused_21, s32 unused_22, s32 unused_23, s32 unused_24);
void save_gfx_set_render_otag(u32* otag);
void save_gfx_submit_frame(s32 first_otag, s32 draw_otag);

/* input */
extern zodiac_draw_context_t g_save_input_display_state;
extern u16 g_save_input_latched_button;
extern u16 g_save_input_previous;
extern u8 g_save_list_input_armed;

void save_update_input(void);

/* menu commands */
menu_draw_number_command_t* menu_draw_decimal_number(menu_draw_number_command_t* command);
menu_window_command_t* menu_draw_window_frame(menu_window_command_t* command);

/* text */
extern const char g_save_text_decimal_format[];
extern const u8 g_save_text_command_help[];
extern const u8 g_save_text_job_name[];
extern const u8 g_save_text_location_name[];

/* sound */
extern u8 g_save_sound_queued_effect_id; /* sound_sfx_e */

#endif
