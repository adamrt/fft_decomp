#ifndef EVENT_CARD_H
#define EVENT_CARD_H

/* EVENT/CARD.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"
#include "fft/bits.h"
#include "fft/card.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"

/* save menus */
/*
 * The g_card_save_*_descriptor menu-thread descriptors below are 0x3c-byte
 * menu_entry_t menu records.
 * card_thread_start_managed and its delayed twin start descriptor->thread_entry
 * (+0x28). The yes/no prompts are a message record followed by a choice record
 * whose text_id is 0xd810; the choice's selected_index (+0x74 of the prompt) is
 * the default answer, and the message window.y (+0x0a) places the prompt.
 */
/* The 0x1e00-byte serialized save is parity-covered in 0x80-byte blocks;
 * each block contributes 0x400 one-bit reads to the primary parity stream. */
enum {
    CARD_SAVE_PARITY_BLOCK_COUNT = 60,
    CARD_SAVE_PARITY_BITS_PER_BLOCK = 1024,
    CARD_SAVE_PARITY_BLOCK_BYTES = 128,
};
/* Ten-byte number command in a 12-byte save-menu slot. */
typedef struct card_save_slot_number_command {
    menu_draw_number_command_t number;
    u8 _unused_0a[2];
} card_save_slot_number_command_t;
typedef char card_save_slot_number_command_size_must_be_12[sizeof(card_save_slot_number_command_t) == 12 ? 1 : -1];

/* 0x801ca968, running to g_card_io_open_result: 0x1e80 bytes, the 0x1e00 save image
 * (card_save_buffer_t) plus a 0x80 tail that card_save_init_menu_state clears
 * with it. */
extern card_save_buffer_t g_card_save_buffer[];
extern card_save_buffer_t* g_card_save_buffer_pointer;
extern menu_entry_t g_card_save_card_error_prompt_descriptor[];
extern menu_entry_t g_card_save_card_slot_selection_descriptor[];
extern menu_entry_t g_card_save_checking_message_descriptor[];
extern s8 g_card_save_completed;
extern s8 g_card_save_error_prompt_closing;
extern s8 g_card_save_file_count;
extern const char* g_card_save_file_names[];
extern const char g_card_save_file_pattern[];
extern s8 g_card_save_format_delay_counter;
extern s8 g_card_save_format_prompt_active;
extern menu_entry_t g_card_save_format_prompt_descriptor[2];
extern menu_entry_t g_card_save_formatting_message_descriptor[];
extern card_save_icon_t g_card_save_icon_records[15];
extern menu_entry_t g_card_save_initial_prompt_descriptor[];
extern s16 g_card_save_initial_prompt_timer;
extern u8 g_card_save_list_cursor_index;
extern s16 g_card_save_list_scroll_anim_offset;
extern s16 g_card_save_list_scroll_base_y;
extern s16 g_card_save_list_selected_slot;
extern s16 g_card_save_list_top_row;
extern menu_entry_t g_card_save_no_data_prompt_descriptor[];
extern s8 g_card_save_occupied_slot_count;
extern u8 g_card_save_overwrite_prompt_active;
extern menu_entry_t g_card_save_overwrite_prompt_descriptor[2];
extern s8 g_card_save_scan_setup_step;
extern u16 g_card_save_slot_cursor_animation;
extern point16_t g_card_save_slot_cursor_position;
/* Two-line encoded description text, 80 bytes per memory-card slot. */
extern u8 g_card_save_slot_descriptions[CARD_SAVE_SLOT_COUNT][0x50];
extern s16 g_card_save_slot_index;
extern card_save_slot_number_command_t g_card_save_slot_number_commands[4];
extern oriented_quad_t g_card_save_slot_quads[];
extern s16 g_card_save_slot_row_y;
extern s8 g_card_save_slot_scan_active;
extern s8 g_card_save_slot_scan_index;
/* A pointer cell, not a record: card_save_update_slot_scan_result clears it to
 * 0, tests it, and assigns g_card_save_no_data_prompt_descriptor into it. */
extern menu_entry_t* g_card_save_slot_scan_prompt_descriptor;
extern menu_window_command_t g_card_save_slot_window_command;
extern menu_window_rect_source_t g_card_save_slot_window_rect_source[];
extern u8 g_card_save_title_template[0x16];
extern u8 g_card_save_write_failed;
extern u8 g_card_save_write_phase;

void card_save_build_file_header(s32 slot, s32 level, card_save_header_t* header);
void card_save_build_buffer(s32 slot);
void card_save_build_slot_description(s32 mode, u8* destination);
s32 card_save_init_occupied_slot_selection(void);
void card_save_render_slot_text(s32 slot);
void card_save_run_menu(s32 unused);
void card_save_update_and_draw_slot_list(u16 input);
void card_save_update_card_slot_selection(void);
void card_save_update_initial_prompt(void);
void card_save_update_slot_playtime(s32 slot);
void card_save_update_slot_scan(void);
void card_save_update_slot_scan_result(void);
void card_save_update_slot_scroll(s32 selection);
void card_save_update_slot_write(s32 slot);

/* card io */
extern s8 g_card_io_open_result;
extern u8 g_card_io_operation_pending;
extern u8 g_card_io_selected_slot;

s32 card_io_create_new_with_retries(s32 port, s32 retry_limit);
s32 card_io_format_selected_slot(void);
s32 card_io_get_info_with_retries(s32 port, s32 retry_limit);
s32 card_io_get_selected_slot(void);
s32 card_io_load_with_retries(s32 port, s32 retry_limit);
s32 card_io_open_or_create_selected_card(void);
s32 card_io_poll_operation_result(void);
void card_io_set_selected_slot(s32 value);
s32 card_io_wait_bios_event_result(void);
s32 card_io_wait_hardware_event_result(void);
s32 card_io_wait_operation_result(s32 retry_limit);

/* files */
extern const char g_card_file_all_pattern[];
extern s32 g_card_file_open_descriptor;
extern const char g_card_file_slot_0_path[];
extern const char g_card_file_slot_1_path[];

extern s8 g_card_free_block_count;

s32 card_file_close_with_retries(s32 descriptor);
s32 card_file_count_free_blocks(const DIRENTRY* entries, s32 entry_count);
s32 card_file_delete_selected_with_retries(const char* filename);
s32 card_file_enumerate_saves(DIRENTRY* entries, s32 all_files);
s32 card_file_open_selected_with_retries(const char* filename, s32 mode);
s32 card_file_read_with_retries(s32 descriptor, void* destination, s32 size);
s32 card_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin);
s32 card_file_write_buffer(const char* filename, const card_save_buffer_t* source, s32 size, s32 create_file);
s32 card_file_write_with_retries(s32 descriptor, const void* source, s32 size);

/* graphics */
/*
 * CARD.OUT's double-buffered graphics context. g_card_gfx_context_base
 * (0x801d92a8) points at the two-element array and g_card_gfx_context
 * (0x801d9294) at the active buffer.
 *
 * The 0xf4 stride is established twice by the matching code:
 * card_gfx_init_contexts steps its per-buffer offset by 0xf4, and
 * card_gfx_submit_frame_and_swap_buffers toggles the active pointer by the
 * same 0xf4. The field offsets come from the accesses in those routines and
 * in the enqueue helpers: the ordering table at 0x00, the POLY_FT4 pool at
 * 0x10, the tile pool at 0x3c, and the Psy-Q environments at 0x60/0xbc/0xd0
 * (DRAWENV 92 + DISPENV 20 + DR_MODE 12 end exactly at 0xdc).
 */
typedef struct card_graphics_context {
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
} card_graphics_context_t;
typedef char card_graphics_context_size_must_be_244[(sizeof(card_graphics_context_t) == 244) ? 1 : -1];

extern card_graphics_context_t* g_card_gfx_context_base;
extern card_graphics_context_t* g_card_gfx_context;
extern u16 g_card_gfx_draw_clut;
extern u16 g_card_gfx_drawenv_y;
extern rgb24_t g_card_gfx_fade_overlay_color;
extern RECT g_card_gfx_fullscreen_fade_rect;
extern POLY_FT4 g_card_gfx_poly_ft4_buffers[];
extern u16 g_card_gfx_poly_ft4_capacity;
extern u16 g_card_gfx_poly_ft4_count;
extern u32* g_card_gfx_render_otag;
/* Double-buffered ZODIAC sprite chains (0x4b0 bytes each). */
extern POLY_FT4 g_card_gfx_scaled_sprite_primitive_buffers[2][30];
extern u16 g_card_gfx_texture_page;
extern u16 g_card_gfx_tile_capacity;

void card_gfx_enqueue_oriented_textured_quad(
    const oriented_quad_t* descriptor, const rgb24_t* color, s32 orientation, s32 semitrans, s32 otag_index);
void card_gfx_enqueue_textured_quad(
    const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans, u16 texture_page, u16 clut, s32 otag_index);
void card_gfx_enqueue_textured_quad_list(const menu_sprite_cell_t* quads, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 otag_index, s32 count);
rgb24_t* card_gfx_get_draw_color(void);
s32 card_gfx_get_fade_state(void);
void card_gfx_init_contexts(card_graphics_context_t* contexts, u32* otags, s32 unused_2, s32 unused_3, s32 unused_4,
    POLY_FT4* polygons, s32 unused_6, s32 unused_7, s32 unused_8, s32 unused_9, s32 unused_10, s32 unused_11,
    s32 unused_12, s32 unused_13, s32 unused_14, s32 unused_15, TILE* tiles, s32 unused_17, s32 unused_18,
    s32 unused_19, s32 unused_20, s32 unused_21, s32 unused_22, s32 unused_23, s32 unused_24);
void card_gfx_init_primitive_lists(card_graphics_context_t* lists);
void card_gfx_set_otag_index(s32 value);
void card_gfx_set_render_otag(u32* otag);
void card_gfx_start_increasing_fade(void);
void card_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag);

/* input */
extern u8 g_card_input_activation_timer;
extern u32* g_card_input_controller;
extern u32 g_card_input_current;
extern zodiac_draw_context_t g_card_input_display_state;
extern u32 g_card_input_event;
extern u32 g_card_input_initial_repeat_counter;
extern u16 g_card_input_latched_button;
extern u16 g_card_input_previous;
extern u32 g_card_input_repeat_counter;
extern s32 g_card_input_repeat_counters[16];
extern u32 g_card_input_secondary_repeat_counter;
extern u32 g_card_previous_input;
extern u8 g_card_save_list_input_armed;

void card_input_update_controller(void);
void card_input_update_suppressed_during_fade(void);

/* menus */
/* CARD save-menu dispatch at 0x801bf3b8.
 * Only states whose roles are established by the matching handlers are named. */
typedef enum card_save_menu_state {
    CARD_SAVE_MENU_INITIAL_PROMPT = 0,
    CARD_SAVE_MENU_SELECT_CARD = 1,
    CARD_SAVE_MENU_SCAN_RESULT = 2,
    CARD_SAVE_MENU_SCANNING = 3,
    CARD_SAVE_MENU_SELECT_SAVE = 4,
    CARD_SAVE_MENU_WRITING = 6,
    CARD_SAVE_MENU_LEAVE_ALT = 9, /* never stored; handled like LEAVE */
    CARD_SAVE_MENU_LEAVE = 10,    /* the player declined or is done: fade the music and the screen out */
    CARD_SAVE_MENU_CLOSING = 0xff /* fading out; no state handler runs */
} card_save_menu_state_e;

extern s32 g_card_menu_script_callback;
extern s16 g_card_menu_selection_index;
extern menu_window_rect_source_t* g_card_menu_window_rect_source;
extern s16 g_card_menu_window_right_x;
extern clut_t g_card_save_menu_clut;
extern u8 g_card_save_menu_state; /* card_save_menu_state_e */

extern DIRENTRY* g_card_save_menu_work_buffer;

void card_menu_update_and_draw_animated_cursor(const point16_t* position, u16* animation_state, s32 mode);
void card_save_init_menu_graphics(void);
void card_save_init_menu_state(void);
void card_save_run_menu_graphic_thread(void);

/* menu commands */
extern menu_window_command_t g_card_cmd_window_interior_command;

menu_draw_number_command_t* card_cmd_draw_decimal_number_handler(menu_draw_number_command_t* command);
menu_window_command_t* card_cmd_draw_window_frame_handler(menu_window_command_t* command);

/* text */
extern s16 g_card_save_slot_text_row_ids[];
extern const u8 g_card_text_command_help[];
extern u8 g_card_text_data[];
extern const char g_card_text_decimal_format[];
extern const u8 g_card_text_job_name[];
extern const u8 g_card_text_location_name[];

/* bit streams */

void card_bits_init_primary_reader(const u8* data);

/* threads */
extern menu_entry_t g_card_save_failure_thread_descriptor[];
extern menu_entry_t g_card_save_in_progress_thread_descriptor[];
extern u8 g_card_save_initial_prompt_thread_state;
extern u8 g_card_save_slot_selection_thread_state;
extern u8 g_card_thread_status_snapshot[16];

/* sound */
extern u8 g_card_sound_queued_effect_id; /* main_sfx_e */

/* unknown */
extern u8 D_801ca964;

#endif
