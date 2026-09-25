#ifndef FFT_EVENT_CARD_H
#define FFT_EVENT_CARD_H

/* EVENT/CARD.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

/* bits */
extern u8 g_card_bits_primary_reset;
extern s32 g_card_bits_reader_1_index;
extern s32 g_card_bits_reader_2_index;
extern u8 g_card_bits_reader_2_reset;
extern const u8* g_card_bits_reader_2_stream;
extern s32 g_card_bits_writer_1_index;
extern u8* g_card_bits_primary_cursor;
void card_bits_init_primary_reader(const u8* data);
u32 card_bits_read_primary(s32 count);
u32 card_bits_read_secondary(s32 count);
void card_bits_write_primary(s32 value);
void card_bits_init_secondary_reader(const u8* data);

/* card */
/* BIOS directory entry as returned by firstfile/nextfile and consumed by
 * card_file_enumerate_saves (0x801c2c60) and card_file_count_free_blocks. */
typedef struct card_directory_entry {
    u8 name[20];
    u32 attributes;
    s32 size;
    u8 remainder[12];
} card_directory_entry_t;

extern u8 g_card_io_operation_pending;
extern u8 g_card_io_selected_slot;
extern s8 g_card_io_open_result;
void card_io_consume_all_events(void);
void card_io_consume_bios_events(void);
void card_io_consume_hardware_events(void);
s32 card_io_create_new_with_retries(s32 port, s32 retry_limit);
s32 card_io_format_selected_slot(void);
s32 card_io_get_info_with_retries(s32 port, s32 retry_limit);
u8 card_io_get_selected_slot(void);
s32 card_io_load_with_retries(s32 port, s32 retry_limit);
s32 card_io_open_or_create_selected_card(void);
s32 card_io_poll_bios_event_result(void);
s32 card_io_poll_hardware_event_result(void);
s32 card_io_poll_operation_result(void);
void card_io_set_selected_slot(s32 value);
s32 card_io_wait_bios_event_result(void);
s32 card_io_wait_hardware_event_result(void);
s32 card_io_wait_operation_result(s32 retry_limit);

/* command */
extern world_menu_window_command_t g_card_cmd_window_interior_command;
u8* card_cmd_draw_decimal_number_handler(u8* command);
world_menu_window_command_t* card_cmd_draw_window_frame_handler(world_menu_window_command_t* command);
u8* card_cmd_draw_tiled_rectangle_handler(u8* command);

/* file */
extern const char g_card_file_all_pattern[];
extern s32 g_card_file_open_descriptor;
extern const char g_card_file_slot_0_path[];
extern const char g_card_file_slot_1_path[];
s32 card_file_close_with_retries(s32 descriptor);
s32 card_file_delete_selected_with_retries(const char* filename);
s32 card_file_open_selected_with_retries(const char* filename, s32 mode);
s32 card_file_read_with_retries(s32 descriptor, void* destination, s32 size);
s32 card_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin);
s32 card_file_write_buffer(const char* filename, const u8* source, s32 size, s32 create_file);
s32 card_file_write_with_retries(s32 descriptor, const void* source, s32 size);
s32 card_file_count_free_blocks(const card_directory_entry_t* entries, s32 entry_count);
s32 card_file_enumerate_saves(card_directory_entry_t* entries, s32 all_files);

/* gfx */
/* The primitive-pool prefix of card_graphics_context_t that
 * card_gfx_init_primitive_lists resets: the polygon and tile pointers sit at
 * the same 0x10/0x3c offsets in both views. */
typedef struct card_primitive_lists {
    u8 unknown_00[0x10];
    POLY_FT4* polygons;
    u8 unknown_14[0x28];
    TILE* tiles;
} card_primitive_lists_t;

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
    u8 unknown_04[0x0c];         /* 0x04 */
    POLY_FT4* polygons;          /* 0x10 */
    u8 unknown_14[0x28];         /* 0x14 */
    TILE* tiles;                 /* 0x3c: TILE pool */
    u8 unknown_40[0x20];         /* 0x40 */
    DRAWENV draw_environment;    /* 0x60 */
    DISPENV display_environment; /* 0xbc */
    DR_MODE draw_mode;           /* 0xd0 */
    u8 unknown_dc[0x18];         /* 0xdc */
} card_graphics_context_t;
typedef char card_graphics_context_size_must_be_0xf4[(sizeof(card_graphics_context_t) == 0xf4) ? 1 : -1];

/*
 * The g_card_save_*_descriptor menu-thread descriptors below are 0x3c-byte
 * world_menu_entry_t menu records.
 * card_thread_start_managed and its delayed twin start descriptor->thread_entry
 * (+0x28). The yes/no prompts are a message record followed by a choice record
 * whose text_id is 0xd810; the choice's selected_index (+0x74 of the prompt) is
 * the default answer, and the message window_y (+0x0a) places the prompt.
 */
/* Textured-quad descriptor read by card_gfx_enqueue_oriented_textured_quad
 * (screen rectangle, texture u/v and extent, CLUT, texture page). The save
 * list's slot quads (g_card_save_slot_quads) are sign-tested on y. */
typedef struct card_oriented_quad {
    u16 x;
    s16 y;
    u16 w;
    u16 h;
    u8 u;
    u8 pad_09;
    u8 v;
    u8 pad_0b;
    u8 uw;
    u8 pad_0d;
    u8 vh;
    u16 clut;
    u16 tpage;
} card_oriented_quad_t;
typedef char card_oriented_quad_size_must_be_0x14[(sizeof(card_oriented_quad_t) == 0x14) ? 1 : -1];

extern card_graphics_context_t* g_card_gfx_context;
extern card_graphics_context_t* g_card_gfx_context_base;
extern u8 g_card_gfx_decreasing_fade_active;
extern RECT g_card_gfx_draw_area_template;
extern u16 g_card_gfx_draw_clut;
extern s16 g_card_gfx_draw_semitrans;
extern u16 g_card_gfx_drawenv_y;
extern u8 g_card_gfx_fade_color[3];
extern u8 g_card_gfx_fade_color_b;
extern u8 g_card_gfx_fade_color_g;
extern s16 g_card_gfx_fade_intensity;
extern u8 g_card_gfx_fade_overlay_color[3];
extern RECT g_card_gfx_fade_rect;
extern RECT g_card_gfx_fullscreen_fade_rect;
extern u8 g_card_gfx_increasing_fade_active;
extern POLY_FT4 g_card_gfx_poly_ft4_buffers[];
extern u8 g_card_gfx_scaled_sprite_count;

/* Double-buffered ZODIAC sprite chains (0x4b0 bytes each). */
extern POLY_FT4 g_card_gfx_scaled_sprite_primitive_buffers[2][30];
extern point16_t g_card_gfx_scaled_sprite_scale[];
extern point16_t g_card_gfx_scaled_sprite_texture_page;
extern main_zodiac_sprite_frame_t g_card_gfx_scaled_sprite_uv_rects[];
extern u16 g_card_gfx_texture_page;
extern u16 g_card_gfx_tile_count;

/* The four tiled-rect scalars below viewed as one RECT; the window-frame
 * parser's store-back only matches as RECT field stores. */
extern RECT g_card_gfx_tiled_rect;
extern u16 g_card_gfx_otag_length;
extern u16 g_card_gfx_poly_ft4_capacity;
extern u16 g_card_gfx_poly_ft4_count;
extern u32* g_card_gfx_render_otag;
extern u16 g_card_gfx_tile_capacity;
void card_gfx_build_scaled_sprite_primitive_chain(battle_menu_status_panel_scaled_sprite_t* sprite, POLY_FT4* poly);

void card_gfx_enqueue_oriented_textured_quad(
    const card_oriented_quad_t* descriptor, const u8* color, s32 orientation, s32 semitrans, s32 otag_index);

u8* card_gfx_get_draw_color(void);
s32 card_gfx_get_fade_state(void);
void card_gfx_set_draw_mode_and_link_ot(s32 dfe, s32 dtd, s32 tpage, RECT* texture_window, s32 otag_index);
void card_gfx_set_otag_index(s32 value);

void card_gfx_set_scaled_poly_ft4_geometry_and_uv(POLY_FT4* poly, const point16_t* texture, const RECT* position,
    const main_zodiac_sprite_frame_t* uv_rect, const point16_t* scale, const s16* offset);

void card_gfx_set_render_otag(u32* otag);
void card_gfx_start_decreasing_fade(void);
void card_gfx_start_increasing_fade(void);
void card_gfx_store_image_and_wait(RECT* rect, u32* data);
void card_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag);
s32 card_gfx_update_decreasing_fade(void);
s32 card_gfx_update_increasing_fade(void);

void card_gfx_enqueue_textured_quad(
    const RECT* rect, s32 u, s32 v, const u8* color, s32 semitrans, u16 texture_page, u16 clut, s32 otag_index);

void card_gfx_enqueue_translucent_tile(const RECT* rect, const u8* color, s32 semitrans, s32 otag_index);

void card_gfx_init_contexts(card_graphics_context_t* contexts, u32* otags, s32 unused_2, s32 unused_3, s32 unused_4,
    POLY_FT4* polygons, s32 unused_6, s32 unused_7, s32 unused_8, s32 unused_9, s32 unused_10, s32 unused_11,
    s32 unused_12, s32 unused_13, s32 unused_14, s32 unused_15, TILE* tiles);

void card_gfx_init_primitive_lists(card_primitive_lists_t* lists);
void card_gfx_load_image_and_wait(RECT* rect, u32* data);

/* input */
extern void* g_card_input_controller;
extern u32 g_card_input_current;
extern u32 g_card_input_event;
extern u32 g_card_input_initial_repeat_counter;
extern u16 g_card_input_latched_button;
extern u16 g_card_input_newly_pressed;
extern u16 g_card_input_previous;
extern u32 g_card_input_repeat_counter;
extern volatile u32 g_card_input_repeat_counters[16];
extern u32 g_card_input_secondary_repeat;
extern u32 g_card_input_secondary_repeat_counter;
extern u32 g_card_previous_input;
extern u8 g_card_input_activation_timer;

/* g_card_input_display_state.scale_x/.scale_y as their own symbols: the initial stores use absolute addresses, and the
 * member spelling lets GCC derive the later `state` pointer from the scale_x address instead. */
extern s16 g_card_input_display_scale_x;
extern u16 g_card_input_display_scale_y;
extern u32 g_card_input_primary_repeat;
void card_input_clear_state(void);
void card_input_update_controller(void);
void card_input_update_suppressed_during_fade(void);
void card_input_update_event_state(void* state, u32 input, s32 count_frame);

/* menu */
/* Window rectangle source record; only the RECT at +6 is known. */
typedef struct card_window_rect_source {
    u8 unk_0[6];
    RECT rect;
} card_window_rect_source_t;

extern const u8 g_card_menu_cursor_forward_offset_keyframes[];
extern u16 g_card_menu_cursor_mode0_background_clut;
extern u16 g_card_menu_cursor_mode0_foreground_clut;
extern u16 g_card_menu_cursor_mode1_background_clut;
extern u16 g_card_menu_cursor_mode1_foreground_clut;
extern const u8 g_card_menu_cursor_reverse_offset_keyframes[];
extern u16 g_card_menu_cursor_texture_page;
extern s8 g_card_menu_event_speed;
extern s16 g_card_menu_selection_index;
extern card_window_rect_source_t* g_card_menu_window_rect_source;
extern s32 g_card_menu_script_callback;
extern s16 g_card_menu_window_right_x;
s32 card_menu_get_event_speed(void);
void card_menu_update_and_draw_animated_cursor(const u16* position, u16* animation_state_arg, s32 mode);

/* save */
/* CARD save-menu dispatch at 0x801bf3b8.
 * Only states whose roles are established by the matching handlers are named. */
typedef enum card_save_menu_state {
    CARD_SAVE_MENU_INITIAL_PROMPT = 0,
    CARD_SAVE_MENU_SELECT_CARD = 1,
    CARD_SAVE_MENU_SCAN_RESULT = 2,
    CARD_SAVE_MENU_SCANNING = 3,
    CARD_SAVE_MENU_SELECT_SAVE = 4,
    CARD_SAVE_MENU_WRITING = 6
} card_save_menu_state_e;

/* The 0x1e00-byte serialized save is parity-covered in 0x80-byte blocks;
 * each block contributes 0x400 one-bit reads to the primary parity stream. */
enum {
    CARD_SAVE_PARITY_BLOCK_COUNT = 0x3c,
    CARD_SAVE_PARITY_BITS_PER_BLOCK = 0x400,
    CARD_SAVE_PARITY_BLOCK_BYTES = 0x80,
};

/*
 * The 0x1e00-byte memory-card save image, filled from the SCUS globals by
 * card_save_build_buffer (0x801c01ac) and parity-covered per 128-byte block.
 * Shares its layout with WORLD's world_card_save_buffer_t.
 */
typedef struct card_save_buffer {
    u8 unknown_000[0x100]; /* 0x000 */
    u8 slot;               /* 0x100 */
    u8 name[0x10];         /* 0x101 */
    u8 name_terminator;    /* 0x111 */
    u8 job_id;             /* 0x112 */
    u8 level;              /* 0x113 */
    u8 month;              /* 0x114 */
    u8 day;                /* 0x115 */
    u8 location;           /* 0x116 */
    u8 format_version;     /* 0x117 */
    u8 parity_bits[8];     /* 0x118 */
    s32 elapsed_seconds;   /* 0x120 */
    u8 treasure_acquisition_date_bits[0x35];
    u8 land_discovery_date_bits[0x12];
    u8 proposition_last_attempt_date_bits[0x6c];
    u8 proposition_states[0x60];
    u8 reserved_237[1]; /* 0x237; serialized and parity-covered */
    u8 saved_records[5][0x38];
    u32 saved_data_bits[40];
    u32 secondary_saved_data_bits[2];
    u8 brave_story_character_ages[0x40];
    u8 proposition_count; /* 0x438 */
    u8 active_propositions[8][9];
    u8 reserved_481[3];                        /* 0x481; serialized and parity-covered */
    u8 party_records[20][0xe0];                /* 0x484 */
    u8 item_quantities[ITEM_ID_COUNT];         /* 0x1604 */
    u8 poached_item_quantities[ITEM_ID_COUNT]; /* 0x1704 */
    u8 item_location_flags[0x80];
    s32 script_variables[0x100];
    u8 game_options[4]; /* 0x1c84 */
    u8 field_1c88;      /* 0x1c88 */
    s8 item_type_order_0[0xc];
    u8 reserved_1c95[1]; /* 0x1c95; serialized and parity-covered */
    s8 item_type_order_1[8];
    u8 reserved_1c9e[1]; /* 0x1c9e; serialized and parity-covered */
    s8 item_type_order_2[7];
    s8 item_type_order_3[5];
    s8 item_type_order_4[5];
    s8 item_type_order_5[7];
    s8 item_type_order_6[5];
    u8 weapon_page_order[0x8a];    /* 0x1cbc */
    u8 helmet_page_order[0x1d];    /* 0x1d46 */
    u8 armor_page_order[0x25];     /* 0x1d63 */
    u8 accessory_page_order[0x21]; /* 0x1d88 */
    u8 item_page_order[0x15];      /* 0x1da9 */
    u8 unknown_1dbe[0x42];         /* 0x1dbe */
} card_save_buffer_t;
typedef char card_save_buffer_size_must_be_0x1e00[(sizeof(card_save_buffer_t) == 0x1e00) ? 1 : -1];

/* One save slot's rendered description string, 0x50 bytes per slot. */
typedef struct card_save_slot_text {
    u8 data[0x50];
} card_save_slot_text_t;

/* 0x801ca968, running to g_card_io_open_result: 0x1e80 bytes, the 0x1e00 save image
 * (card_save_buffer_t) plus a 0x80 tail that card_save_init_menu_state clears
 * with it. */
extern card_save_buffer_t g_card_save_buffer[];
extern card_save_buffer_t* g_card_save_buffer_pointer;
extern world_menu_entry_t g_card_save_card_slot_selection_descriptor[];
extern s8 g_card_save_completed;
extern world_menu_entry_t g_card_save_failure_thread_descriptor[];
extern const char* g_card_save_file_names[];
extern const char g_card_save_file_pattern[];
extern card_save_icon_t g_card_save_icon_records[15];
extern world_menu_entry_t g_card_save_in_progress_thread_descriptor[];
extern world_menu_entry_t g_card_save_initial_prompt_descriptor[];
extern u8 g_card_save_initial_prompt_thread_state;
extern u16 g_card_save_menu_palette_image[0x100];
extern u8 g_card_save_menu_state;
extern card_directory_entry_t* g_card_save_menu_work_buffer;
extern world_menu_entry_t g_card_save_no_data_prompt_descriptor[];
extern u16 g_card_save_slot_cursor_animation[];
extern u16 g_card_save_slot_cursor_position[];
extern card_save_slot_text_t g_card_save_slot_descriptions[];
extern s16 g_card_save_slot_index;
extern u8 g_card_save_slot_number_commands[4][12];
extern card_oriented_quad_t g_card_save_slot_quads[];
extern s16 g_card_save_slot_row_y;

/* A pointer cell, not a record: card_save_update_slot_scan_result clears it to
 * 0, tests it, and assigns g_card_save_no_data_prompt_descriptor into it. */
extern world_menu_entry_t* g_card_save_slot_scan_prompt_descriptor;
extern u8 g_card_save_slot_selection_thread_state;
extern const u16 g_card_save_slot_text_row_ids[];
extern u8 g_card_save_title_template[0x16];
extern u8 g_card_save_write_failed;
extern u8 g_card_save_write_phase;
extern u8 g_card_save_list_cursor_index;
extern s16 g_card_save_list_scroll_anim_offset;
extern s16 g_card_save_list_scroll_base_y;
extern s16 g_card_save_list_selected_slot;
extern s16 g_card_save_list_top_row;
extern s16 g_card_save_initial_prompt_timer;
extern world_menu_entry_t g_card_save_checking_message_descriptor[];
extern world_menu_entry_t g_card_save_format_prompt_descriptor[2];
extern world_menu_entry_t g_card_save_overwrite_prompt_descriptor[2];
extern world_menu_entry_t g_card_save_formatting_message_descriptor[];
extern world_menu_entry_t g_card_save_card_error_prompt_descriptor[];
extern s8 g_card_save_slot_scan_active;
extern s8 g_card_save_format_delay_counter;
extern s8 g_card_save_error_prompt_closing;
extern world_menu_window_command_t g_card_save_slot_window_command;
extern card_window_rect_source_t g_card_save_slot_window_rect_source[];
extern u8 g_card_save_list_input_armed;
extern u8 g_card_save_overwrite_prompt_active;
extern s8 g_card_save_format_prompt_active;
extern s8 g_card_save_slot_scan_index;
extern s8 g_card_save_file_count;
extern s8 g_card_save_scan_setup_step;
extern s8 g_card_save_occupied_slot_count;
void card_save_build_buffer(s32 slot);
void card_save_build_slot_description(s32 mode, u8* destination);
void card_save_init_menu_graphics(void);
void card_save_init_menu_state(void);
s32 card_save_init_occupied_slot_selection(void);
void card_save_render_slot_text(s32 slot);
void card_save_run_menu_graphic_thread(void);
void card_save_update_and_draw_slot_list(u16 input);
void card_save_update_card_slot_selection(void);
void card_save_update_initial_prompt(void);
void card_save_update_slot_playtime(s32 slot);
void card_save_update_slot_scan(void);
void card_save_update_slot_scan_result(void);
void card_save_update_slot_scroll(s32 selection);
void card_save_update_slot_write(s32 slot);
void card_build_save_file_header(s32 slot, s32 level, card_save_buffer_t* buffer);
void card_save_run_menu(s32 unused);

/* text */
/* Pen position and row stride card_text_render_glyph_to_4bpp_image draws
 * one glyph at. */
typedef struct card_text_image_position {
    u16 x;
    s16 y;
    s16 row_stride;
} card_text_image_position_t;

extern u8 g_card_text_color[3];
extern const u8 g_card_text_command_help[];
extern u8 g_card_text_data[];
extern const char g_card_text_decimal_format[];
extern u16 g_card_text_digit_clut;
extern u16 g_card_text_digit_texture_page;
extern const u8* g_card_text_glyph_bitmap_data;
extern const u8 g_card_text_glyph_widths[];
extern const u8 g_card_text_job_name[];
extern s8 g_card_text_layout_mode;
extern const u8 g_card_text_location_name[];
extern u16 g_card_text_metric_0;
extern u16 g_card_text_metric_1;
extern u16 g_card_text_metric_2;
extern u16 g_card_text_metric_3;
extern u16 g_card_text_metric_4;
extern u16 g_card_text_metric_5;
extern s16 g_card_text_otag_index;
extern u16 g_card_text_clut_2_mode0;
extern u16 g_card_text_clut_2_mode1;
extern u16 g_card_text_clut_1_mode1;
extern u16 g_card_text_clut_1_mode0;
extern u16 g_card_text_clut_3_mode0;
extern u16 g_card_text_clut_3_mode1;
extern u16 g_card_text_clut_0_mode1;
extern u16 g_card_text_clut_0_mode0;

void card_text_render_encoded_ids_to_image(u8* image, const battle_menu_text_image_bounds_t* bounds, s32 glyph_spacing,
    s32 line_width, const void* glyph_data, const u16* text_ids, s32 max_glyphs, s32 terminator, s32 unused_style);

s32 card_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const card_text_image_position_t* position, s32 style);

void card_text_render_id_rows_to_vram(void* glyph_data, const u16* text_ids, const RECT* destination, s32 flags);
void card_text_set_palette_and_metrics(s32 mode);
const u8* card_text_skip_encoded_segments(const u8* data, s16 count);

/* thread */
extern s32 g_card_thread_active_key;
extern s32 g_card_thread_state;
extern u8 g_card_thread_status_snapshot[16];
void card_thread_request_stop(s32 id);
void card_thread_stop_and_clear_state(s32 id);
void card_thread_wait_and_clear_state(s32 thread_id);
s32 card_thread_start_managed(s32 thread_id, world_menu_entry_t* descriptor);
void card_thread_start_managed_with_delay(s32 thread_id, world_menu_entry_t* descriptor);

/* other */
extern u8 g_card_sound_queued_effect_id;
extern s8 g_card_free_block_count;

/* unnamed */
extern u8 D_801ca964;

#endif
