#ifndef FFT_OPEN_H
#define FFT_OPEN_H

/* OPEN/OPEN.BIN: opening and title. */

#include "fft/world.h"

/* card */
extern u8 g_open_card_info_pending;
extern u8 g_open_card_slot;
s32 open_card_check_slots(void);
void open_card_consume_all_events(void);
void open_card_consume_bios_events(void);
void open_card_consume_hardware_events(void);
s32 open_card_get_info_with_retries(s32 port, s32 retry_limit);
s32 open_card_poll_bios_event(void);
s32 open_card_poll_hardware_event(void);
s32 open_card_poll_info_result(void);
s32 open_card_poll_info_until_stable(s32 retry_limit);
void open_card_set_slot(u8 slot);
s32 open_card_wait_for_bios_event(void);
s32 open_card_wait_for_hardware_event(void);

/* file */
extern s32 g_open_file_current_openbk_image_id;
extern void* g_open_file_destination;
extern const s32 g_open_file_openbk_end_sector_offsets[];
extern const s32 g_open_file_openbk_start_sector_offsets[];
extern main_file_load_descriptor_t g_open_file_header;
extern u8 g_open_file_location[];
void open_file_build_openbk_header(s32 image_id, void* destination);
void open_file_load_end_movie_graphics(void);
void open_file_load_main_menu_graphics(void);
void open_file_load_sound_menu_textures(void);
void open_file_wait_for_pending(void);

void open_file_wait_then_build_header(
    main_file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination);

s32 open_file_get_cd_sync_state_delta(void);

/* input */
/* One 100-byte state record per controller. The current controller c runs
 * g_open_controller_handlers[g_open_controller_handler_indices[c - 1]] on
 * g_open_controller_records[c]. A push made while the current index is c
 * fills g_open_controller_stream_start[c] (which is records[c + 1]) and
 * stores the new handler id at handler_indices[c]. Only the stream pair is
 * common; the remaining words are handler-specific (the thread-2 controller
 * keeps run_followup / result in the first two words). */
typedef struct open_controller_record {
    /* 0x00 */ s32 stream_start;
    /* 0x04 */ s32 stream_length;
    /* 0x08 */ s32 field_08;
    /* 0x0c */ s32 field_0c;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
    /* 0x18 */ s32 field_18;
    /* 0x1c */ s32 field_1c;
    /* 0x20 */ s32 field_20; /* birthday date menu: month */
    /* 0x24 */ s32 field_24; /* birthday date menu: day */
    /* 0x28 */ s32 field_28; /* birthday date menu: selecting_month */
    /* 0x2c */ u8 padding_2c[0x64 - 0x2c];
} open_controller_record_t;
typedef char open_controller_record_size_must_be_0x64[(sizeof(open_controller_record_t) == 0x64) ? 1 : -1];

/* Field-base view used to read birthday values from 100-byte controller records. */
typedef struct open_controller_birthday {
    s32 month;
    s32 day;
    u8 padding_08[0x64 - 8];
} open_controller_birthday_t;

typedef void (*open_controller_handler_t)(void* record);

extern open_controller_birthday_t g_open_controller_birthdays[];
extern s32 g_open_controller_handler_indices[];
extern open_controller_handler_t g_open_controller_handlers[];
extern open_controller_record_t g_open_controller_records[];
extern open_controller_record_t g_open_controller_stream_start[];
extern u32 g_open_input_current_buttons;
extern s32 g_open_input_direction_counter_2;
extern s32 g_open_input_direction_counter_3;
extern s32 g_open_input_directional_repeat_initial_delay;
extern s32 g_open_input_directional_repeat_period;
extern s32 g_open_input_down_repeat_counter;

/* Volatile: open_menu_handle_sound_type_input reloads it (lui/lw) for every
 * test; readers that test it twice from one load take a local copy. */
extern volatile u32 g_open_input_new_button_presses;
extern u32 g_open_input_polled_buttons;
extern u32 g_open_input_previous_buttons;
extern s32 g_open_input_up_repeat_counter;
extern s32 g_open_controller_flags[];
void open_controller_dispatch_current(void);
void open_controller_start_text_message(s32 parameter, s32 run_followup);
u32 open_input_check_repeating_directional(u32 buttons);
void open_input_init_directional_state(void);
void open_input_update_buttons_and_check_game_reset(void);

/* movie */
/* Shared VLC/MDEC state used by OPEN.BIN's movie-stream pipeline. */
typedef struct open_movie_mdec_stream_state {
    void* vlc_buffers[2];    /* 0x00 */
    s32 vlc_buffer_index;    /* 0x08 */
    void* output_buffers[2]; /* 0x0c */
    s32 output_buffer_index; /* 0x14 */
    RECT frame_rects[2];     /* 0x18 */
    s32 frame_rect_index;    /* 0x28 */
    RECT output_rect;        /* 0x2c */
    s32 output_ready;        /* 0x34 */
} open_movie_mdec_stream_state_t;
typedef char open_mdec_stream_state_size_must_be_0x38[(sizeof(open_movie_mdec_stream_state_t) == 0x38) ? 1 : -1];

extern s32 g_open_movie_aligned_height;
extern u8 g_open_movie_cd_stream_ring_buffer[];
extern u32 g_open_movie_current_frame;
extern u32 g_open_movie_first_frame;
extern s32 g_open_movie_height;
extern s32 g_open_movie_last_frame;
extern u8 g_open_movie_mdec_output_buffer_0[];
extern u8 g_open_movie_mdec_output_buffer_1[];
extern open_movie_mdec_stream_state_t g_open_movie_mdec_stream_state;
extern s32 g_open_movie_stream_status;
extern u32 g_open_movie_width;
extern u32 g_open_mdec_quant_table_command[];
extern u32 g_open_mdec_luma_quant_table[16];
extern u32 g_open_mdec_chroma_quant_table[16];
extern u32 g_open_mdec_scale_table_command[];
extern u32 g_open_mdec_scale_table[32];
extern volatile u32* g_open_mdec_command_reg;
extern volatile u32* g_open_mdec_status_reg;
extern s32 g_st_cd_interrupt_pending;
void open_mdec_reset(s32 mode);

/* OPEN.BIN interfaces shared across its reconstructed translation units. */
void* open_movie_acquire_next_frame(open_movie_mdec_stream_state_t* state);
s32 open_movie_decode_next_frame(open_movie_mdec_stream_state_t* state);
void open_movie_handle_mdec_output_slice(void);
void open_movie_init_and_play_end(void);
void open_movie_init_and_stream_opening(s32 skip_movie);

void open_movie_init_mdec_stream_state(
    volatile open_movie_mdec_stream_state_t* state, s16 x, s16 y, s16 width, s32 height);

void open_movie_init_stream(s32 sector, void* output_callback);
void open_movie_pause_cd_audio(void);
void open_movie_play_end(void);
void open_movie_present_frame(void);
void open_movie_start_cd_stream_read(const void* location);
void open_movie_start_fftst_or_alternate_controller(s32 use_alternate);
void open_movie_start_fftst_or_skip_controller(void);
void open_movie_start_stream(s32 sector, s32 first_frame, s32 last_frame, s32 sound_type);
void open_movie_stop_stream(s32 pause_cd_audio);
void open_movie_stream_and_push_controller_1(s32 start_sector, s32 unused, s32 end_frame);
void open_movie_stream_fftend(s32 start_sector, s32 unused, s32 end_frame);
void open_movie_stream_fftst_and_push_controller(s32 start_sector, s32 stream_length, s32 end_frame, s32 frame_count);

void open_movie_stream_other_str_and_push_controller_11(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 frame_rate);

void open_movie_update_stream_frame(void);
void open_movie_wait_for_mdec_output(open_movie_mdec_stream_state_t* state, s32 unused);

/* render record */
#define OPEN_RENDER_RECORD_VISIBLE 0x10

/* Two-word point: music-test window extent/origin, sprite CLUT position and
 * the birthday menu's text origin. The birthday menu passes it by value; the
 * target homes a2/a3 into the incoming argument slots, which GCC 2.6.3 only
 * does for an aggregate register argument. The WLDCORE twin uses
 * wldcore_point32_t. */
typedef struct open_point32 {
    s32 x;
    s32 y;
} open_point32_t;

/* Render records. open_gfx_draw_render_record_pointer_list dispatches on flags bit 0
 * (36-byte record) or bit 1 (56-byte record); bit 4 marks a visible record.
 * Both pools hold 16 entries, allocated by the open_append_render_record_*
 * counters. */
typedef struct open_render_record_36 {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 anim_id;
    /* 0x08 */ s32 ot_layer;
    /* 0x0c */ s32 frame_index;
    /* 0x10 */ s32 frame_timer;
    /* 0x14 */ s32 palette; /* birthday menu: 9 = highlighted entry */
    /* 0x18 */ s32 x;
    /* 0x1c */ s32 y;
    /* 0x20 */ u8 r;
    /* 0x21 */ u8 g;
    /* 0x22 */ u8 b;
    /* 0x23 */ u8 padding_23;
} open_render_record_36_t;
typedef char open_render_record_36_size_must_be_0x24[(sizeof(open_render_record_36_t) == 0x24) ? 1 : -1];

/* x/y view of a 36-byte render record, based at its x field. */
typedef struct open_render_record_36_position {
    /* 0x00 */ s32 x; /* == g_open_gfx_render_records_36[i].x */
    /* 0x04 */ s32 y;
    /* 0x08 */ u8 padding_08[0x24 - 8];
} open_render_record_36_position_t;

/* Common prefix used to dispatch either render-record layout. */
typedef struct open_render_record {
    u32 flags;
} open_render_record_t;

/* CLUT position and source rectangle at +0x24 of a 56-byte record.
 * open_gfx_init_window_frame_record takes its address as one pointer. */
typedef struct open_render_record_56_tail {
    /* 0x00 */ open_point32_t clut; /* open_gfx_draw_render_record_56 passes it to GetClut */
    /* 0x08 */ RECT quad;           /* quarter-pixel source rectangle */
} open_render_record_56_tail_t;

typedef struct open_render_record_56 {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 vram_x; /* quad.x / 4 + 64 * depth + 0x180 */
    /* 0x08 */ s32 vram_y;
    /* 0x0c */ s32 ot_layer;
    /* 0x10 */ s32 depth;
    /* 0x14 */ s32 pop_in_step;
    /* 0x18 */ s32 palette;
    /* 0x1c */ s32 x; /* quad.x - 0x80 */
    /* 0x20 */ s32 y; /* quad.y - 0x78 */
    /* 0x24 */ open_render_record_56_tail_t tail;
    /* 0x34: a sibling of `tail`, not part of it. open_gfx_init_window_frame_record
     * computes its address as records + index * 0x38 + 0x34, which a nested
     * `&rec[i].tail.r` cannot give (CSE rewrites that as tail + 0x10). */
    CVECTOR color;
} open_render_record_56_t;
typedef char open_render_record_56_size_must_be_0x38[(sizeof(open_render_record_56_t) == 0x38) ? 1 : -1];

/* x/y view of the 56-byte render records, based at records_56[0].x; the
 * retail code addresses that pair through its own base register, loaded
 * before the index is scaled, which &records_56[i].x does not reproduce. */
typedef struct open_render_record_56_position {
    /* 0x00 */ s32 x; /* == g_open_gfx_render_records_56[i].x */
    /* 0x04 */ s32 y;
    /* 0x08 */ u8 padding_08[0x38 - 8];
} open_render_record_56_position_t;

/* birthday */
/* Birthday-menu controller record containing four render-record indices. */
typedef struct open_birthday_render_record_indices {
    s32 unused;
    s32 indices[4];
} open_birthday_render_record_indices_t;

/* Birthday date-selection controller state shared with its redraw routine. */
typedef struct open_birthday_date_state {
    s32 cursor_record_36; /* 0x00: cursor moved between month and day */
    s32 unknown_04;
    s32 title_record_56;  /* 0x08: birthday title image (0xb800); palette 2 on
                             confirmation, 0 on restore */
    s32 window_record_56; /* 0x0c: flags bit 8 blocks input */
    s32 unknown_10[4];
    s32 month;           /* 0x20: 1..12 */
    s32 day;             /* 0x24 */
    s32 selecting_month; /* 0x28: 0 edits the day, 1 edits the month */
} open_birthday_date_state_t;

/* Final birthday-confirmation controller state. */
typedef struct open_birthday_confirmation_state {
    s32 cursor_record_36; /* 0x00 */
    s32 unknown_04;
    s32 menu_record_56;  /* 0x08 */
    s32 selected_option; /* 0x0C: 0 confirms, 1 returns to date entry */
    s32 unknown_10[4];
    s32 month; /* 0x20 */
    s32 day;   /* 0x24 */
    u8 padding_28[0x64 - 0x28];
} open_birthday_confirmation_state_t;
typedef char
    open_birthday_confirmation_state_size_must_be_0x64[(sizeof(open_birthday_confirmation_state_t) == 0x64) ? 1 : -1];

extern const u8 g_open_birthday_month_lengths[12];
extern u8 g_open_birthday_title_image[];
extern u8 g_open_birthday_window_image[];

/* Start date of each ordinary sign as {month, day} byte pairs, Aries first
 * ({3, 21}, {4, 20}, ...). open_birthday_convert_to_zodiac_position indexes
 * it flat ([i * 2], [i * 2 + 1]), and it is not const: that routine reloads a
 * day after storing through its month pointer. */
extern u8 g_open_birthday_zodiac_months[ZODIAC_SIGN_ORDINARY_COUNT * 2];
void open_birthday_push_date_controller(void);
void open_birthday_build_confirmation_menu(const open_birthday_date_state_t* menu);
void open_birthday_build_menu_text(open_birthday_date_state_t* menu);
s32 open_birthday_calculate_day_index(s32 month, s32 day);
void open_birthday_convert_to_zodiac_position(s32* month, s32* day);

void open_birthday_draw_menu_text_entry(
    s32 record_index, s32 combined_text_index, open_point32_t position, void* image);

void open_birthday_handle_confirmation_input(open_birthday_confirmation_state_t* menu);
void open_birthday_load_window_image(s32 record_index, u32* image);
void open_birthday_push_confirmation_controller(void);

/* menu */
/* Text origin record passed to world_menu_display_text_entry; the same
 * layout as the origin_x..stride tail of world_menu_text_state_t, which most
 * WORLD callers pass directly. */
typedef struct menu_text_origin {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s32 unknown_04;
    /* 0x08 */ s32 stride; /* OPEN/WLDCORE pass their window record's width */
} menu_text_origin_t;

extern s32 g_open_menu_formation_entry_mask;
extern s32 g_open_menu_formation_otag_index;

void open_menu_start_music_test_list_thread(
    s32 thread_id, s32 limit, const open_point32_t* extent, const open_point32_t* origin);

void open_menu_start_music_test_controller(void);
void open_menu_update_world_formation(void);
void open_menu_push_sound_type_controller(void);

/* sound */
/* Sound-type menu controller state (handler 3). */
typedef struct open_sound_menu_state {
    /* 0x00 */ s32 render_records[7];
    /* 0x1c */ s32 sound_mode_selection; /* Signed so decrement can wrap below Mono. */
    /* 0x20 */ s32 step;
} open_sound_menu_state_t;

extern s16 g_open_music_test_row_actions[16];
extern world_menu_entry_t g_open_music_test_list_params;
extern world_menu_text_layout_t g_open_music_test_list_state;
extern s16 g_open_music_test_list_entry_flags[96];
extern s16 g_open_music_test_list_entries[96];
extern s16 g_open_music_test_selected_option;
extern u8 g_open_music_test_music_ids[];   /* music test entry -> scenario music id */
extern s32 g_open_music_test_sound_handle; /* handle of the playing music test track */
void open_sound_set_type_and_volume(s32 sound_type, s32 duration);

/* text */
/* Eight-byte glyph metrics record addressed by OPEN script text. */
typedef struct open_font_metrics {
    u8 u;
    u8 v;
    u8 flags;
    u8 bearing;
    u8 height;
    u8 width;
    u8 descender;
    u8 advance;
} open_font_metrics_t;

extern const char g_open_text_mdec_reset_bad_option_format[];
extern const char g_open_text_mdec_in_sync_tag[];
extern const char g_open_text_mdec_out_sync_tag[];
extern const char g_open_text_mdec_timeout_dma_format[];
extern const char g_open_text_mdec_timeout_fifo_format[];
extern const char g_open_text_mdec_timeout_format[];
extern s32 g_open_text_section_offsets[];
s32 open_text_decode_hex_digit(s32 character);
void open_text_init_menu_render_record(s32 record_index, RECT quad, s32 depth, s32 combined_text_index, void* image);

/* script */
typedef enum open_script_dispatch_flag {
    OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY = 0x40,
    OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY = 0x80,
} open_script_dispatch_flag_e;

typedef struct open_script_glyph {
    s16 x;
    s16 y;
    s16 width;
    s16 height;
} open_script_glyph_t;

typedef struct open_script_record {
    u32 flags;
    s32 glyph_count;
    s32 skew; /* 0x08: italic skew added to the top edge of each glyph quad */
    s32 x;
    s32 y;
    open_script_glyph_t glyphs[42];
} open_script_record_t;
typedef char open_script_record_size_must_be_0x164[(sizeof(open_script_record_t) == 0x164) ? 1 : -1];

/* OPEN script interpreter state at 0x8008e548. flags bit 0 keeps the
 * dispatcher running, bit 1 requests another entry this frame and bit 2 marks
 * a wait opcode in progress. */
typedef struct open_script_dispatch_state {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 tpage;
    /* 0x08 */ s32 clut;
    /* 0x0c */ s32 font_bank;
    /* 0x10 */ s32 skew; /* copied into each text record's skew */
    /* 0x14 */ s32 wide_glyphs;
    /* 0x18 */ s32 timing_step;
    /* 0x1c */ s32 timing_fraction;
    /* 0x20 */ s32 external_counter;
    /* 0x24 */ s32 wait_counter;
    /* 0x28 */ s32 data_base;
    /* 0x2c */ s32 byte_offset;
} open_script_dispatch_state_t;
typedef char open_script_dispatch_state_size_must_be_0x30[(sizeof(open_script_dispatch_state_t) == 0x30) ? 1 : -1];

/* OPNTEX frame sequence and the script screen fade it starts. */
typedef struct open_opntex_sequence_state {
    /* 0x00 */ s32 fade_elapsed_frames;
    /* 0x04 */ s32 fade_duration_frames;
    /* 0x08 */ s32 fade_intensity;
    /* 0x0c */ s32 first_frame;
    /* 0x10 */ s32 last_frame;
    /* 0x14 */ s32 frame_duration;
    /* 0x18 */ s32 frame_countdown;
    /* 0x1c */ s32 delay;
} open_opntex_sequence_state_t;

/* The dispatcher state, the 16 text records and the control/OPNTEX tail
 * (0x8008e548..0x8008fbef) are one object: open_gfx_update_opntex_sequence
 * reaches dispatch.flags at -0x1698 from its &opntex.frame_duration base. */
typedef struct open_script_state {
    /* 0x0000 */ open_script_dispatch_state_t dispatch;
    /* 0x0030 */ open_script_record_t records[16];
    /* 0x1670 */ s32 unknown_1670;
    /* 0x1674 */ s32 unknown_1674; /* cleared by open_script_init_state */
    /* 0x1678 */ s32 control_parameter;
    /* 0x167c */ s32 control_state;
    /* 0x1680 */ s32 xa_wait_frames;
    /* 0x1684 */ open_opntex_sequence_state_t opntex;
    /* 0x16a4 */ s32 opntex_control_value;
} open_script_state_t;
typedef char open_script_state_size_must_be_0x16a8[(sizeof(open_script_state_t) == 0x16a8) ? 1 : -1];

extern open_script_state_t g_open_script_state;
extern open_font_metrics_t* g_open_script_font_metrics;

/* Scalar views of g_open_script_state words. open_opcode_store_s16_and_advance,
 * open_script_update_timing_and_record_values and
 * open_gfx_update_opntex_sequence load and store each of these by absolute
 * address; the member spelling lets cse keep the address in a register. */
extern s32 g_open_script_byte_offset;      /* dispatch.byte_offset */
extern s32 g_open_script_timing_fraction;  /* dispatch.timing_fraction */
extern s32 g_open_script_external_counter; /* dispatch.external_counter */

/* Byte bases of records[0] fields for open_opcode_create_positioned_text_record. */
extern u8 g_open_script_glyph_height_field_base[];
extern u8 g_open_script_glyph_width_field_base[];
extern u8 g_open_script_glyph_x_field_base[];
extern u8 g_open_script_glyph_y_field_base[];
extern u8 g_open_script_record_glyph_count_field_base[];
extern POLY_FT4 g_open_script_glyph_primitives[2][672];
extern s32 g_open_script_initial_data_base;
void open_script_init_state(void);
void open_script_draw_text_records(u32* otag, s32 graphics_buffer_index);
void open_script_update_xa_audio(void);
s32 open_script_add_pointer_offset(s32 left, s32 right);
s32 open_script_build_text_record(u8* text);
s32 open_script_init_and_dispatch_entries(void);
void open_script_push_controller(void);
void open_script_update_controller(void);
void open_script_update_screen_fade(void);
void open_script_update_timing_and_record_values(void);

/* title */
extern s32 g_open_title_demo_movie_index;
void open_title_push_menu_controller(s32 argument);
void open_title_init_new_game_party(s32 party_mode, s32 world_load_mode);
void open_title_start_new_game_transition(void);
void open_title_start_new_game_or_clear_file_buffer(void);

/* gfx */
/* Image transition script entry: a header word (bits 0-7 step count, bits
 * 8-12 the texture-page position, bits 16-23 the image operation) followed by
 * four-byte steps holding a value byte and a duration byte. */
typedef union open_gfx_transition_entry {
    u32 header;
    u8 bytes[32];
} open_gfx_transition_entry_t;

/* OPEN image transition controller at 0x800855b4. */
typedef struct open_gfx_transition {
    /* 0x00 */ u16 flags;
    /* 0x02 */ s16 mode;
    /* 0x04 */ s16 current_index;
    /* 0x06 */ s16 target_index;
    /* 0x08 */ s16 step;
    /* 0x0a */ s16 step_count;
    /* 0x0c */ s16 tpage_x; /* (header & 0xf00) >> 2; GetTPage x */
    /* 0x0e */ s16 tpage_y; /* (header >> 4) & 0x100; GetTPage y */
    /* 0x10 */ s16 step_values[7];
    /* 0x1e */ s16 step_durations[7];
    /* 0x2c */ s16 timer;
    /* 0x2e */ s16 intensity;
    /* 0x30 */ open_gfx_transition_entry_t** entry_table;
    /* 0x34 */ open_gfx_transition_entry_t* entry;
} open_gfx_transition_t;
typedef char open_gfx_transition_size_must_be_0x38[(sizeof(open_gfx_transition_t) == 0x38) ? 1 : -1];

/* One DR_MODE + SPRT pair of the shared 32-entry per-buffer sprite pool. */
typedef struct open_sprite_prim {
    /* 0x00 */ DR_MODE mode;
    /* 0x0c */ SPRT sprt;
} open_sprite_prim_t;
typedef char open_sprite_prim_size_must_be_0x20[(sizeof(open_sprite_prim_t) == 0x20) ? 1 : -1];

/* open_gfx_draw_render_record_36's view of a 36-byte render record:
 * the same layout as open_render_record_36_t, with the fields it animates
 * named. */
typedef struct open_sprite_actor {
    /* 0x00 */ s32 field_00;
    /* 0x04 */ s32 anim_id;
    /* 0x08 */ s32 ot_layer;
    /* 0x0c */ s32 frame_index;
    /* 0x10 */ s32 frame_timer;
    /* 0x14 */ s32 palette;
    /* 0x18 */ s32 x;
    /* 0x1c */ s32 y;
    /* 0x20 */ u8 r;
    /* 0x21 */ u8 g;
    /* 0x22 */ u8 b;
    /* 0x23 */ u8 padding_23;
} open_sprite_actor_t;
typedef char open_sprite_actor_size_must_be_0x24[(sizeof(open_sprite_actor_t) == 0x24) ? 1 : -1];

/* Double-buffered draw/display environments at 0x800851c0, one pair per
 * frame buffer (0x70 bytes each); open_initialize_screen_environments fills
 * both and the movie presenter flips between them. */
typedef struct open_screen_environment {
    DRAWENV draw; /* 0x00 */
    DISPENV disp; /* 0x5c */
} open_screen_environment_t;

/* OPEN screen overlay fade at 0x80085cac..0x80085d03: one record, since
 * open_gfx_update_fade_overlay reaches flags..rect and the tiles/modes from a
 * single base. */
typedef struct open_overlay_fade {
    /* 0x00 */ u32 flags; /* bit 0 fade out, bit 1 fade in, bit 2 draw */
    /* 0x04 */ s32 frame;
    /* 0x08 */ s32 duration;
    /* 0x0c */ s32 level;
    /* 0x10 */ s32 abr;
    /* 0x14 */ s32 ot_index;
    /* 0x18 */ RECT rect;
    /* 0x20 */ TILE tiles[2];
    /* 0x40 */ DR_MODE modes[2];
} open_overlay_fade_t;

extern POLY_FT4 g_open_gfx_fade_primitives[2];
extern u16* volatile g_open_gfx_image_copy_destination_buffer;
extern u16* volatile g_open_gfx_image_copy_file_pixels;

/* Volatile: open_gfx_step_image_crossfade reloads it every column. */
extern u16* volatile g_open_gfx_image_copy_source_buffer;
extern u8 g_open_gfx_image_copy_progress;
extern u8 g_open_gfx_image_copy_row_flags[];
extern s32 g_open_gfx_next_render_record_36;
extern s32 g_open_gfx_next_render_record_56;
extern u8* g_open_gfx_opntex_data;
extern u32 g_open_gfx_otags[2][16];
extern s32 g_open_gfx_primitive_count;
extern s32 g_open_gfx_render_record_56_count;
extern open_render_record_56_position_t g_open_gfx_record_translation[16];
extern s32 g_open_gfx_render_record_pointer_count;
extern open_render_record_t* g_open_gfx_render_record_pointers[16];
extern open_render_record_36_t g_open_gfx_render_records_36[16];
extern s32 g_open_gfx_render_records_36_y_field_base[];
extern open_render_record_56_t g_open_gfx_render_records_56[16];
extern open_screen_environment_t g_open_gfx_screen_environments[2];
extern open_overlay_fade_t g_open_gfx_overlay_fade;
extern s32 g_open_gfx_opntex_delay; /* opntex.delay */
extern open_gfx_transition_t g_open_gfx_transition;
extern open_sprite_prim_t g_open_gfx_sprite_primitive_pool[2][32];
extern POLY_FT4 g_open_gfx_credit_band_primitives[2];
extern u32** g_open_gfx_record_36_sprites;
extern POLY_GT4 g_open_gfx_transition_line_primitives[2][14];
void open_gfx_draw_render_record_36(open_sprite_actor_t* actor, u32* ot);
void open_gfx_draw_render_record_56(open_render_record_56_t* record, u32* ot);
s32 open_gfx_update_transition_sequence(void);
void open_gfx_draw_transition_line_strips(s32 intensity);
s32 open_gfx_append_render_record_36(open_render_record_t** list, s32* count);
s32 open_gfx_append_render_record_56(open_render_record_t** list, s32* count);
void open_gfx_backup_frame_vram_page(void);
void open_gfx_calculate_scaled_rectangle(s32 scale_mode, const RECT* quad, RECT* rectangle);
void open_gfx_clear_and_draw_current_frame(void);
void open_gfx_clear_current_otag(void);
void open_gfx_copy_file_image_to_210x180_buffer(s32 file_index);
void open_gfx_draw_render_record_pointer_list(u32* otag, open_render_record_t** records, s32 count);
void open_gfx_hide_four_render_records(const open_birthday_render_record_indices_t* group);
void open_gfx_init_render_buffers(void);
void open_gfx_init_screen_environments(s32 clear_first);
void open_gfx_init_transition_request(s32 target, s32 mode, open_gfx_transition_entry_t** entry_table);
void open_gfx_init_window_frame_record(s32 record_index, RECT quad, s32 depth, u16* image);
void open_gfx_load_opntex_into_frame_buffer(const void* source_data);
void open_gfx_show_four_render_records(const open_birthday_render_record_indices_t* group);
void open_gfx_start_overlay_fade_in(u32 duration);
void open_gfx_start_overlay_fade_out(u32 duration);
void open_gfx_step_image_crossfade(void);
void open_gfx_update_fade_overlay(u32* otag);
void open_gfx_update_opntex_sequence(void);

/* system */
enum {
    OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN = 0x1000,
    OPEN_SYSTEM_RUNTIME_FLAG_RGB24 = 0x2000,
};

enum {
    OPEN_RUNTIME_INPUT_DISABLED = 0x10,
    OPEN_OVERLAY_FADE_ACTIVE = 0x04,
};

typedef enum game_sound_mode {
    GAME_SOUND_MODE_MONO = 0,
    GAME_SOUND_MODE_STEREO = 1,
    GAME_SOUND_MODE_WIDE = 2,
} game_sound_mode_e;

extern volatile u32* g_open_dma_dpcr;
extern volatile u32* g_open_dma_mdec_out_chcr;
extern volatile u32* g_open_dma_mdec_out_madr;
extern u32* g_open_dma_mdec_out_bcr;
extern s32 g_open_system_result;
extern u32 g_open_system_runtime_flags;
extern s32 g_open_system_vsync_mode;
extern open_gfx_transition_entry_t** g_open_new_game_transition_entries;
extern volatile u32* g_open_dma_mdec_in_madr;
extern volatile u32* g_open_dma_mdec_in_bcr;
extern volatile u32* g_open_dma_mdec_in_chcr;
s32 open_bin_decdctbufsize(const u16* bs);
void open_bin_decdctin(s32* packet, s32 mode);
void open_bin_decdctout(u32 command, u32 words);
void open_bin_decdctoutcallback(void* callback);
void open_bin_decdctreset(s32 mode);
s32 open_bin_libpress_timeout(const char* operation);
void open_bin_mdec_in(u32* packet, u32 g_main_save_word_flags);
s32 open_bin_mdec_in_sync(void);
void open_bin_mdec_out(u32 command, u32 g_main_save_word_flags);
s32 open_bin_mdec_out_sync(void);
void open_restore_birthday_date_menu(open_birthday_date_state_t* menu);
void open_noop_8006d7ec(void);
void open_system_init_runtime_state(void);

/* other */
extern s32 g_open_current_controller_index;
extern u8 g_open_work_buffer_0[];
extern u8 g_open_work_buffer_1[];

#endif
