#ifndef OPEN_H
#define OPEN_H

/* OPEN/OPEN.BIN: opening and title. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* card */
s32 open_card_check_slots(void);

/* file */
extern s32 g_open_file_current_openbk_image_id;
extern void* g_open_file_destination;
extern const s32 g_open_file_openbk_end_sector_offsets[];
extern const s32 g_open_file_openbk_start_sector_offsets[];
void open_file_build_openbk_header(s32 image_id, void* destination);
void open_file_load_end_movie_graphics(void);
void open_file_load_main_menu_graphics(void);

s32 open_file_get_cd_sync_state_delta(void);

/* input */
extern s32 g_open_input_left_repeat_counter;
extern s32 g_open_input_right_repeat_counter;
extern s32 g_open_input_directional_repeat_initial_delay;
extern s32 g_open_input_directional_repeat_period;
extern s32 g_open_input_down_repeat_counter;

/* Volatile: open_menu_handle_sound_type_input reloads it (lui/lw) for every
 * test; readers that test it twice from one load take a local copy. */
extern volatile u32 g_open_input_new_button_presses;
extern s32 g_open_input_up_repeat_counter;
u32 open_input_update_repeat(u32 buttons);
void open_input_init_directional_state(void);

/* movie */
/* g_open_movie_stream_status, set as each frame header arrives. */
typedef enum open_movie_stream_status {
    OPEN_MOVIE_STREAM_PLAYING = 0,
    OPEN_MOVIE_STREAM_STOP_AND_PAUSE_AUDIO = 1, /* reached g_open_movie_end_frame */
    OPEN_MOVIE_STREAM_STOP = 2,                 /* reached g_open_movie_video_end_frame; the CD audio plays on */
} open_movie_stream_status_e;

/* Shared VLC/MDEC state used by OPEN.BIN's movie-stream pipeline. */
typedef struct open_movie_mdec_stream_state {
    u32* vlc_buffers[2];     /* 0x00; decoded run-level data in 32-bit entries */
    s32 vlc_buffer_index;    /* 0x08 */
    u32* output_buffers[2];  /* 0x0c; decoded image data in 32-bit entries */
    s32 output_buffer_index; /* 0x14 */
    RECT frame_rects[2];     /* 0x18 */
    s32 frame_rect_index;    /* 0x28 */
    RECT output_rect;        /* 0x2c */
    s32 output_ready;        /* 0x34 */
} open_movie_mdec_stream_state_t;
typedef char open_movie_mdec_stream_state_size_must_be_56[(sizeof(open_movie_mdec_stream_state_t) == 56) ? 1 : -1];

extern s32 g_open_movie_aligned_height;
extern u32 g_open_movie_cd_stream_ring_buffer[];
extern u32 g_open_movie_current_frame;
extern u32 g_open_movie_end_frame;
extern s32 g_open_movie_height;
extern s32 g_open_movie_video_end_frame; /* -1: none */
extern u32 g_open_movie_mdec_output_buffer_0[];
extern u32 g_open_movie_mdec_output_buffer_1[];
extern open_movie_mdec_stream_state_t g_open_movie_mdec_stream_state;
extern open_movie_stream_status_e g_open_movie_stream_status;
extern s32 g_open_movie_width;

/* movie playback */
void open_movie_init_and_play_end(void);
void open_system_init_title_sequence(s32 skip_movie);

void open_movie_pause_cd_audio(void);
void open_movie_play_end(void);
void open_movie_start_fftst_or_title_menu(s32 use_alternate);
void open_movie_start_stream(s32 sector, s32 end_frame, s32 video_end_frame, s32 cd_mix_level);
void open_movie_stop_stream(s32 pause_cd_audio);
void open_movie_stream_fftst_and_push_controller(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 video_end_frame);

void open_movie_stream_title_demo_and_push_controller(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level);

/* render record */
/* world_animated_render_record_t.palette of the selected menu entry. */
enum { OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE = 9 };

enum { OPEN_GFX_RENDER_RECORD_CAPACITY = 16 };

/* Render records. window_dispatch_record_list_variant dispatches on flags
 * (world_render_record_flag_e): ANIMATED is a 36-byte record, IMAGE a
 * 56-byte record, and a record with BLANK or HIDDEN set is skipped.
 * Each pool holds OPEN_GFX_RENDER_RECORD_CAPACITY entries. */
/* The render queue holds either record format; its leading 32-bit flags value selects
 * the concrete layout. */

/* CLUT position and source rectangle at +0x24 of a 56-byte record.
 * open_gfx_init_window_frame_record takes its address as one pointer. */
typedef struct open_render_record_56_tail {
    /* 0x00 */ point32_t clut; /* open_gfx_draw_window_record passes it to GetClut */
    /* 0x08 */ RECT quad;      /* sprite u, v, w, h: texels within the texture page */
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
typedef char open_render_record_56_size_must_be_56[(sizeof(open_render_record_56_t) == 56) ? 1 : -1];

/* x/y view of the 56-byte render records, based at records_56[0].x; the
 * retail code addresses that pair through its own base register, loaded
 * before the index is scaled, which &records_56[i].x does not reproduce. */
typedef struct open_render_record_56_position {
    /* 0x00 */ s32 x; /* == g_open_gfx_render_records_56[i].x */
    /* 0x04 */ s32 y;
    /* 0x08 */ u8 _unused_08[0x38 - 8];
} open_render_record_56_position_t;

/* controllers */
/* Rows of g_open_controller_handlers: the value a push stores in
 * g_open_controller_handler_indices. */
typedef enum open_controller_handler {
    OPEN_CONTROLLER_FFTST_MOVIE = 0,           /* open_movie_finish_stream_controller_if_ready_or_skipped */
    OPEN_CONTROLLER_FFTOP_MOVIE = 1,           /* open_movie_finish_fftop_controller_if_ready_or_skipped */
    OPEN_CONTROLLER_TITLE_MENU = 2,            /* open_title_update_menu */
    OPEN_CONTROLLER_SOUND_TYPE_MENU = 3,       /* open_menu_handle_sound_type_input */
    OPEN_CONTROLLER_NEW_GAME_START = 4,        /* open_title_step_new_game_start */
    OPEN_CONTROLLER_BIRTHDAY_DATE = 5,         /* open_birthday_handle_menu_input */
    OPEN_CONTROLLER_BIRTHDAY_CONFIRMATION = 6, /* open_birthday_handle_confirmation_input */
    OPEN_CONTROLLER_MUSIC_TEST = 7,            /* open_menu_update_music_test */
    OPEN_CONTROLLER_TEXT_MESSAGE = 8,          /* open_controller_wait_text_message */
    OPEN_CONTROLLER_FFTEND_MOVIE = 9,          /* open_movie_finish_fftend_controller_if_ready_or_skipped */
    OPEN_CONTROLLER_SCRIPT = 10,               /* open_script_update_controller */
    OPEN_CONTROLLER_TITLE_DEMO_MOVIE = 11,     /* open_movie_start_fftst_or_skip_controller */
    OPEN_CONTROLLER_TITLE_EXIT = 12,           /* open_title_update_card_warning */
} open_controller_handler_e;

/* open_title_controller_t.cursor: the four title menu entries. */
typedef enum open_title_menu_entry {
    OPEN_TITLE_MENU_NEW_GAME = 0,
    OPEN_TITLE_MENU_CONTINUE = 1,
    OPEN_TITLE_MENU_TUTORIAL = 2,
    OPEN_TITLE_MENU_SOUND = 3,
} open_title_menu_entry_e;

/* open_title_controller_t.state. */
typedef enum open_title_state {
    OPEN_TITLE_STATE_MENU = 0,
    OPEN_TITLE_STATE_SOUND_MENU = 2, /* pushes the sound type menu on the next update */
    OPEN_TITLE_STATE_DEMO_MOVIE = 3, /* idle timeout: starts the next demo movie once the screen has faded */
} open_title_state_e;

/* 32-bit state of the title-exit controller (open_title_update_card_warning). */
typedef enum open_title_exit_state_id {
    OPEN_TITLE_EXIT_INPUT = 0,
    OPEN_TITLE_EXIT_CANCEL = 1,  /* cross: back to the title menu */
    OPEN_TITLE_EXIT_CONFIRM = 2, /* circle or start: the new-game transition */
} open_title_exit_state_id_e;

/* Title-menu controller state (handler 2), pushed into the next controller
 * record by open_title_push_menu_controller and run by open_title_update_menu. */
typedef struct open_title_controller {
    /* 0x00 */ s32 records_36[5]; /* logo, then the four menu items */
    /* 0x14 */ s32 idle_timer;
    /* 0x18 */ s32 cd_end_position;
    /* 0x1c */ s32 cursor; /* open_title_menu_entry_e */
    /* 0x20 */ s32 state;  /* open_title_state_e */
    /* 0x24 */ s32 cursor_record_36;
    /* 0x28 */ s32 _unused_28[2];
    /* 0x30 */ s32 exit_timer;
    /* 0x34 */ s32 exiting;
    /* 0x38 */ u8 _unused_38[0x64 - 0x38];
} open_title_controller_t;

/* Birthday date-selection controller state shared with its redraw routine. */
typedef struct open_birthday_date_state {
    s32 cursor_record_36; /* 0x00: cursor moved between month and day */
    s32 label_record;
    s32 title_record_56;  /* 0x08: birthday title image (0xb800); palette 2 on
                             confirmation, 0 on restore */
    s32 window_record_56; /* 0x0c: flags bit 8 blocks input */
    s32 _unused_10[4];
    s32 month;           /* 0x20: 1..12 */
    s32 day;             /* 0x24 */
    s32 selecting_month; /* 0x28: 0 edits the day, 1 edits the month */
} open_birthday_date_state_t;

/* Final birthday-confirmation controller state. */
typedef struct open_birthday_confirmation_state {
    s32 cursor_record_36; /* 0x00 */
    s32 label_record;     /* 0x04 */
    s32 menu_record_56;   /* 0x08 */
    s32 selected_option;  /* 0x0C: 0 confirms, 1 returns to date entry */
    s32 _unused_10[4];
    s32 month; /* 0x20 */
    s32 day;   /* 0x24 */
    u8 _unused_28[0x64 - 0x28];
} open_birthday_confirmation_state_t;
typedef char
    open_birthday_confirmation_state_size_must_be_100[(sizeof(open_birthday_confirmation_state_t) == 100) ? 1 : -1];

/* Sound-type menu controller state (handler 3). */
enum { OPEN_SOUND_MENU_ENTRY_COUNT = 3 };

typedef struct open_sound_menu_state {
    /* 0x00 */ s32 entry_render_indices[OPEN_SOUND_MENU_ENTRY_COUNT]; /* Mono, Stereo and Wide labels */
    u8 _unused_0c[16];
    /* 0x1c */ s32 sound_mode_selection; /* Signed so decrement can wrap below Mono. */
    /* 0x20 */ s32 step;
} open_sound_menu_state_t;
typedef char open_sound_menu_state_size_must_be_36[sizeof(open_sound_menu_state_t) == 36 ? 1 : -1];

/* Movie controller state (handlers 0 and 11): the stream the FFTST and title demo movies play. */
typedef struct open_movie_stream_controller_state {
    s32 start_sector;
    s32 stream_length;
} open_movie_stream_controller_state_t;

/* New-game start controller state (handler 4). */
typedef struct open_title_new_game_state {
    /* 0x00 */ u8 _unused_00[0x10];
    /* 0x10 */ s32 step;
    /* 0x14 */ u8 _unused_14[0x0c];
    /* 0x20 */ s32 name_text_id;
} open_title_new_game_state_t;

/* Music test controller state (handler 7), pushed by open_menu_start_music_test_controller. */
typedef struct open_menu_sound_test_state {
    /* 0x00 */ s32 music_id;     /* scenario music selected for playback */
    /* 0x04 */ s32 delay;        /* frames before loading music_id */
    /* 0x08 */ s32 music_slot;   /* slot from main_sound_open_music_into_free_slot */
    /* 0x0c */ s32 step;         /* open_music_test_step_e */
    /* 0x10 */ s32 blink_frames; /* counts ten frames after a selection, while Up and Down stay blocked */
} open_menu_sound_test_state_t;

/* Text message controller state (handler 8). */
typedef struct open_controller_thread_completion {
    s32 run_followup;
    s32 saved_blocked_buttons; /* g_menu_blocked_buttons when the message started */
} open_controller_thread_completion_t;

/* Title exit controller state (handler 12). */
typedef struct open_title_exit_state {
    s32 state;
} open_title_exit_state_t;

/* One 100-byte state record per controller, laid out by its handler. The current controller c runs
 * g_open_controller_handlers[g_open_controller_handler_indices[c - 1]] on
 * g_open_controller_records[c]. A push made while the current index is c
 * fills g_open_controller_records_next[c] (which is records[c + 1]) and
 * stores the new handler id at handler_indices[c]. */
typedef union open_controller_record {
    open_movie_stream_controller_state_t movie;
    open_title_controller_t title;
    open_sound_menu_state_t sound_menu;
    open_title_new_game_state_t new_game;
    open_birthday_date_state_t birthday_date;
    open_birthday_confirmation_state_t birthday_confirmation;
    open_menu_sound_test_state_t music_test;
    open_controller_thread_completion_t text_message;
    open_title_exit_state_t title_exit;
} open_controller_record_t;
typedef char open_controller_record_size_must_be_100[(sizeof(open_controller_record_t) == 100) ? 1 : -1];

typedef void (*open_controller_handler_t)(open_controller_record_t* record);
extern open_controller_handler_t g_open_controller_handlers[];
extern open_controller_record_t g_open_controller_records[];
extern open_controller_record_t g_open_controller_records_next[];

extern s32 g_open_controller_handler_indices[]; /* open_controller_handler_e */
extern s32 g_open_controller_selected_options[];
extern s32 g_open_current_controller_index;

void open_controller_dispatch_current(void);
void open_movie_finish_stream_controller_if_ready_or_skipped(open_controller_record_t* record);
void open_movie_finish_fftop_controller_if_ready_or_skipped(open_controller_record_t* unused_record);
void open_title_update_menu(open_controller_record_t* record);
void open_menu_handle_sound_type_input(open_controller_record_t* record);
void open_title_step_new_game_start(open_controller_record_t* record);
void open_birthday_handle_menu_input(open_controller_record_t* record);
void open_birthday_handle_confirmation_input(open_controller_record_t* record);
void open_menu_update_music_test(open_controller_record_t* record);
void open_controller_wait_text_message(open_controller_record_t* record);
void open_movie_finish_fftend_controller_if_ready_or_skipped(open_controller_record_t* unused_record);
void open_script_update_controller(open_controller_record_t* unused_record);
void open_movie_start_fftst_or_skip_controller(open_controller_record_t* unused_record);
void open_title_update_card_warning(open_controller_record_t* record);

/* birthday */
extern u8 g_open_birthday_title_image[];
extern u8 g_open_birthday_window_image[];

/* Start date of each ordinary sign as {month, day} byte pairs, Aries first
 * ({3, 21}, {4, 20}, ...). open_birthday_convert_date_to_zodiac indexes
 * it flat ([i * 2], [i * 2 + 1]), and it is not const: that routine reloads a
 * day after storing through its month pointer. */
extern u8 g_open_birthday_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT * 2];
void open_birthday_convert_date_to_zodiac(s32* month, s32* day);

void open_gfx_load_window_record_image(s32 record_index, u32* image);

/* menu */

extern s32 g_open_menu_formation_otag_index;

void open_gfx_init_vram_render_record(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags);

/* music test */
/* Step of the music test controller (open_menu_update_music_test). */
typedef enum open_music_test_step {
    OPEN_MUSIC_TEST_STEP_IDLE = 0,
    OPEN_MUSIC_TEST_STEP_CLOSE = 1,       /* waits for the list thread, then fades the screen */
    OPEN_MUSIC_TEST_STEP_FADE_OUT = 2,    /* waits for the fade, then returns to the title menu */
    OPEN_MUSIC_TEST_STEP_START_TRACK = 3, /* waits for the music to load, then plays it */
    OPEN_MUSIC_TEST_STEP_OPEN = 4,        /* waits for the fade, then starts the list thread */
} open_music_test_step_e;

/* The slot of the music test's list thread. */
enum {
    OPEN_THREAD_SLOT_MUSIC_TEST_LIST = 12,
};

extern s16 g_open_music_test_row_actions[16];
extern menu_entry_t g_open_music_test_list_params;
extern menu_text_layout_t g_open_music_test_list_state;
extern s16 g_open_music_test_list_entry_flags[96];
extern s16 g_open_music_test_list_entries[96];
extern s16 g_open_music_test_selected_option;
extern u8 g_open_music_test_music_ids[];               /* music test entry -> scenario music id */
extern suzuki_music_t* g_open_music_test_sound_handle; /* handle of the playing music test track */

void open_menu_start_music_test_controller(void);

/* Fades the CD audio in at the given mix level (suzuki_sound_put_type), or out when it is 0. */
void open_sound_set_cd_mix_and_volume(s32 cd_mix_level, s32 duration);

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

extern s32 g_open_text_section_offsets[];
void open_text_measure_menu_image(s32 combined_text_index, size32_t* dimensions);

/* script */
typedef enum open_script_dispatch_flag {
    OPEN_SCRIPT_DISPATCH_RUNNING = 0x01,  /* cleared to stop the dispatcher and hide the text records */
    OPEN_SCRIPT_DISPATCH_CONTINUE = 0x02, /* runs another entry this frame */
    OPEN_SCRIPT_DISPATCH_WAITING = 0x04,  /* a wait opcode is in progress */
    OPEN_SCRIPT_DISPATCH_XA_PLAYING = 0x08,
    OPEN_SCRIPT_DISPATCH_XA_STOPPING = 0x10, /* the stop sequence of open_script_update_xa_audio runs */
    OPEN_SCRIPT_DISPATCH_XA_STARTING = 0x20, /* its start sequence runs */
    OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY = 0x40,
    OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY = 0x80,
} open_script_dispatch_flag_e;

/* open_script_record_t.flags. */
typedef enum open_script_record_flag {
    OPEN_SCRIPT_RECORD_ACTIVE = 0x01, /* the record holds a text being shown */
} open_script_record_flag_e;

typedef struct open_script_record {
    u32 flags; /* open_script_record_flag_e */
    s32 glyph_count;
    s32 skew; /* 0x08: italic skew added to the top edge of each glyph quad */
    s32 x;
    s32 y;
    RECT glyphs[42];
} open_script_record_t;
typedef char open_script_record_size_must_be_356[(sizeof(open_script_record_t) == 356) ? 1 : -1];

/* Serialized ending-script opcodes, in the order of g_open_opcode_handlers. */
typedef enum open_script_opcode {
    OPEN_SCRIPT_OPCODE_END = 0x00,
    OPEN_SCRIPT_OPCODE_POSITIONED_TEXT = 0x01,
    OPEN_SCRIPT_OPCODE_SCROLL_SPEED = 0x02,
    OPEN_SCRIPT_OPCODE_REVEAL_SCREEN = 0x03,
    OPEN_SCRIPT_OPCODE_HIDE_SCREEN = 0x04,
    OPEN_SCRIPT_OPCODE_WAIT_FRAMES = 0x05,
    OPEN_SCRIPT_OPCODE_WAIT_SCROLL = 0x06,
    OPEN_SCRIPT_OPCODE_START_XA_AUDIO = 0x07,
    OPEN_SCRIPT_OPCODE_FADE_IN_AUDIO = 0x08,
    OPEN_SCRIPT_OPCODE_FADE_OUT_AUDIO = 0x09,
    OPEN_SCRIPT_OPCODE_STOP_XA_AUDIO = 0x0a,
    OPEN_SCRIPT_OPCODE_LOAD_OPNTEX_FRAME = 0x0b,
    OPEN_SCRIPT_OPCODE_FADE_IN_PICTURE = 0x0c,
    OPEN_SCRIPT_OPCODE_FADE_OUT_PICTURE = 0x0d,
    OPEN_SCRIPT_OPCODE_WAIT_XA_AUDIO_IDLE = 0x0e,
    OPEN_SCRIPT_OPCODE_START_OPNTEX_SEQUENCE = 0x0f,
} open_script_opcode_e;

/* The ending-script dispatcher passes a serialized 16-bit cursor. */
typedef void (*open_script_entry_handler_t)(s16* entry);

/* Byte-addressed ending-script cursor. */
typedef struct open_script_cursor {
    u8* data_base;
    s32 byte_offset;
} open_script_cursor_t;
typedef char open_script_cursor_size_must_be_8[(sizeof(open_script_cursor_t) == 8) ? 1 : -1];

/* OPEN script interpreter state at 0x8008e548. */
typedef struct open_script_dispatch_state {
    /* 0x00 */ u32 flags; /* open_script_dispatch_flag_e */
    /* 0x04 */ s32 tpage;
    /* 0x08 */ s32 clut;
    /* 0x0c */ s32 font_index; /* each font has 0x380 bytes of glyph metrics */
    /* 0x10 */ s32 skew;       /* copied into each text record's skew */
    /* 0x14 */ s32 wide_glyphs;
    /* 0x18 */ s32 timing_step;
    /* 0x1c */ s32 timing_fraction;
    /* 0x20 */ s32 external_counter;
    /* 0x24 */ s32 wait_counter;
    /* 0x28 */ open_script_cursor_t cursor;
} open_script_dispatch_state_t;
typedef char open_script_dispatch_state_size_must_be_48[(sizeof(open_script_dispatch_state_t) == 48) ? 1 : -1];

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
    /* 0x1670 */ s32 _unused_1670;
    /* 0x1674 */ s32 _unknown_1674;     /* cleared by open_script_init_state */
    /* 0x1678 */ s32 control_parameter; /* XA channel of ENDING.XA to play */
    /* 0x167c */ s32 control_state;     /* step of the XA start or stop sequence (open_script_update_xa_audio) */
    /* 0x1680 */ s32 xa_wait_frames;    /* frames since the XA read started */
    /* 0x1684 */ open_opntex_sequence_state_t opntex;
    /* 0x16a4 */ s32 opntex_control_value;
} open_script_state_t;
typedef char open_script_state_size_must_be_5800[(sizeof(open_script_state_t) == 5800) ? 1 : -1];

extern open_script_entry_handler_t g_open_opcode_handlers[16];
extern open_script_state_t g_open_script_state;
extern open_font_metrics_t* g_open_script_font_metrics;

/* Scalar views of 32-bit fields of g_open_script_state. open_opcode_set_timing_step,
 * open_script_update_timing_and_record_values and
 * open_gfx_update_opntex_sequence load and store each of these by absolute
 * address; the member spelling lets cse keep the address in a register. */
extern s32 g_open_script_byte_offset;      /* dispatch.cursor.byte_offset */
extern s32 g_open_script_timing_fraction;  /* dispatch.timing_fraction */
extern s32 g_open_script_external_counter; /* dispatch.external_counter */

extern POLY_FT4 g_open_script_glyph_primitives[2][672];
extern u8* g_open_script_initial_data_base;
void open_opcode_end_script(s16* unused_entry);
void open_opcode_create_positioned_text_record(s16* command);
void open_opcode_set_timing_step(s16* entry);
void open_opcode_start_overlay_fade_out(s16* command);
void open_opcode_start_overlay_fade_in(s16* command);
void open_opcode_wait_counted(s16* entry);
void open_opcode_wait_for_external_counter(s16* entry);
void open_opcode_start_xa_audio(s16* command);
void open_opcode_start_sound_fade_in(s16* command);
void open_opcode_start_sound_fade_out(s16* command);
void open_opcode_stop_xa_audio(s16* unused_entry);
void open_opcode_load_opntex_frame(s16* command);
void open_opcode_start_fade_intensity_increase(s16* command);
void open_opcode_start_fade_intensity_decrease(s16* command);
void open_opcode_wait_for_xa_audio_idle(s16* unused_entry);
void open_opcode_start_opntex_sequence(s16* command);

void open_script_draw_text_records(u32* otag, s32 graphics_buffer_index);

/* title */
extern s32 g_open_title_demo_movie_index;
void open_title_push_menu_controller(s32 argument);
void open_title_init_new_game_party(s32 party_mode, s32 world_load_mode);
void open_title_start_new_game_transition(void);
void open_title_start_new_game_or_clear_file_buffer(void);

/* gfx */
/* New-game prologue entry: a packed line count, texture-page position and
 * image operation, followed by four-byte texture-row/width records. */
typedef struct open_gfx_transition_line {
    u8 texture_row;
    u8 width;
    u8 _unused_02[2];
} open_gfx_transition_line_t;
typedef char open_gfx_transition_line_size_must_be_4[sizeof(open_gfx_transition_line_t) == 4 ? 1 : -1];

typedef struct open_gfx_transition_entry {
    u32 header;
    u8 line_data[0]; /* variable number of encoded open_gfx_transition_line_t records */
} open_gfx_transition_entry_t;
typedef char open_gfx_transition_entry_header_size_must_be_4[sizeof(open_gfx_transition_entry_t) == 4 ? 1 : -1];

/* High nibble of the image command byte in open_gfx_transition_entry_t.header. */
typedef enum open_gfx_transition_image_operation {
    OPEN_GFX_TRANSITION_PREPARE_IMAGE = 0x10,
    OPEN_GFX_TRANSITION_START_CROSSFADE = 0x20,
} open_gfx_transition_image_operation_e;

enum {
    OPEN_GFX_TRANSITION_IMAGE_OPERATION_MASK = 0xf0,
    OPEN_GFX_TRANSITION_IMAGE_INDEX_MASK = 0x0f,
};

/* open_gfx_transition_t.flags. */
typedef enum open_gfx_transition_flag {
    OPEN_GFX_TRANSITION_NEXT_ENTRY = 0x01,  /* starts the next entry of the table, or ends at the target */
    OPEN_GFX_TRANSITION_NEXT_STEP = 0x02,   /* starts the next step of the entry */
    OPEN_GFX_TRANSITION_WAIT_CIRCLE = 0x08, /* the entry is complete: waits for circle */
    OPEN_GFX_TRANSITION_DIM = 0x10,         /* the line strips fade out before the next entry */
    OPEN_GFX_TRANSITION_SKIP = 0x20,        /* start was pressed: fades out and ends */
} open_gfx_transition_flag_e;

/* open_overlay_fade_t.flags. */
typedef enum open_overlay_fade_flag {
    OPEN_OVERLAY_FADE_OUT = 0x01,    /* the overlay clears */
    OPEN_OVERLAY_FADE_IN = 0x02,     /* the overlay darkens the screen */
    OPEN_OVERLAY_FADE_ACTIVE = 0x04, /* the overlay is drawn; cleared when either fade ends */
} open_overlay_fade_flag_e;

/* OPEN image transition controller at 0x800855b4. */
typedef struct open_gfx_transition {
    /* 0x00 */ u16 flags;    /* open_gfx_transition_flag_e */
    /* 0x02 */ s16 ot_layer; /* text and shadow occupy this layer and the next */
    /* 0x04 */ s16 current_index;
    /* 0x06 */ s16 target_index;
    /* 0x08 */ s16 step;
    /* 0x0a */ s16 step_count;
    /* 0x0c */ s16 tpage_x; /* (header & 0xf00) >> 2; GetTPage x */
    /* 0x0e */ s16 tpage_y; /* (header >> 4) & 0x100; GetTPage y */
    /* 0x10 */ s16 line_texture_rows[7];
    /* 0x1e */ s16 line_widths[7];
    /* 0x2c */ s16 timer;
    /* 0x2e */ s16 intensity;
    /* 0x30 */ open_gfx_transition_entry_t** entry_table;
    /* 0x34 */ open_gfx_transition_entry_t* entry;
} open_gfx_transition_t;
typedef char open_gfx_transition_size_must_be_56[(sizeof(open_gfx_transition_t) == 56) ? 1 : -1];

/* One DR_MODE + SPRT pair of the shared 32-entry per-buffer sprite pool. */
typedef struct open_sprite_prim {
    /* 0x00 */ DR_MODE mode;
    /* 0x0c */ SPRT sprt;
} open_sprite_prim_t;
typedef char open_sprite_prim_size_must_be_32[(sizeof(open_sprite_prim_t) == 32) ? 1 : -1];

/* Double-buffered draw/display environments at 0x800851c0, one pair per
 * frame buffer (0x70 bytes each); open_gfx_init_screen_environments fills
 * both and the movie presenter flips between them. */
typedef struct open_screen_environment {
    DRAWENV draw; /* 0x00 */
    DISPENV disp; /* 0x5c */
} open_screen_environment_t;

/* OPEN screen overlay fade at 0x80085cac..0x80085d03: one record, since
 * open_gfx_update_fade_overlay reaches flags..rect and the tiles/modes from a
 * single base. */
typedef struct open_overlay_fade {
    /* 0x00 */ u32 flags; /* open_overlay_fade_flag_e */
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
extern u16* g_open_gfx_image_copy_destination_buffer;
extern u16* g_open_gfx_image_copy_file_pixels;

/* Volatile: open_gfx_step_image_crossfade reloads it every column. */
extern u16* volatile g_open_gfx_image_copy_source_buffer;
extern u8 g_open_gfx_image_copy_progress;
extern u8 g_open_gfx_image_copy_row_flags[];
extern s32 g_open_gfx_next_render_record_36;
extern s32 g_open_gfx_next_render_record_56;
extern u32* g_open_gfx_opntex_data;
extern u32 g_open_gfx_otags[2][16];
extern s32 g_open_gfx_primitive_count;
extern open_render_record_56_position_t g_open_gfx_record_translation[OPEN_GFX_RENDER_RECORD_CAPACITY];
extern s32 g_open_gfx_render_record_pointer_count;
/* Both render pools publish record pointers to this queue. */
extern void* g_open_gfx_render_record_pointers[];
extern world_animated_render_record_t g_open_gfx_render_records_36[OPEN_GFX_RENDER_RECORD_CAPACITY];
extern open_render_record_56_t g_open_gfx_render_records_56[OPEN_GFX_RENDER_RECORD_CAPACITY];
extern open_screen_environment_t g_open_gfx_screen_environments[2];
extern open_overlay_fade_t g_open_gfx_overlay_fade;
extern open_gfx_transition_t g_open_gfx_transition;
extern open_sprite_prim_t g_open_gfx_sprite_primitive_pool[2][32];
extern POLY_FT4 g_open_gfx_credit_band_primitives[2];
extern world_sprite_animation_sequence_t** g_open_gfx_record_36_animations;
extern world_sprite_cel_header_t** g_open_gfx_record_36_sprites;
extern POLY_GT4 g_open_gfx_transition_line_primitives[2][14];
s32 open_gfx_update_transition_sequence(void);
void open_gfx_restore_frame_bin_page(void);
void open_gfx_clear_and_draw_current_frame(void);
void open_gfx_clear_current_otag(void);
void open_gfx_init_screen_environments(s32 clear_first);
void open_gfx_init_transition_request(s32 target, s32 ot_layer, open_gfx_transition_entry_t** entry_table);
void open_gfx_init_window_frame_record(s32 record_index, RECT quad, s32 depth, u16* image);
void open_gfx_load_opntex_into_frame_buffer(u32* tim);
void open_gfx_start_overlay_fade_in(u32 duration);
void open_gfx_start_overlay_fade_out(u32 duration);
void open_gfx_update_fade_overlay(u32* otag);

/* system */
/* g_open_system_runtime_flags. */
typedef enum open_system_runtime_flag {
    OPEN_SYSTEM_RUNTIME_FLAG_RUNNING = 0x00001,          /* the OPEN main loop runs while set */
    OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING = 0x00002,  /* cleared by open_movie_stop_stream */
    OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING = 0x00004, /* open_movie_pause_cd_audio pauses the drive and clears it */
    OPEN_SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN = 0x00008,
    OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED = 0x00010,
    OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED = 0x00040,
    OPEN_SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED = 0x00080,
    OPEN_SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN = 0x00100,
    OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME = 0x00400, /* the WORLD menu frame runs every loop */
    OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN = 0x01000,
    OPEN_SYSTEM_RUNTIME_FLAG_RGB24 = 0x02000,
    OPEN_PARTY_MODE_FLAG_BASE = 0x04000, /* shifted left by the party mode: which new-game party was built */
    OPEN_PARTY_MODE_FLAG_MASK = 0x1c000,
    /* open_movie_present_frame: clears the frame, or copies it from VRAM (512, 256), instead of
     * uploading the file image. */
    OPEN_SYSTEM_RUNTIME_FLAG_CLEAR_FRAME = 0x20000,
    OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM = 0x40000,
    OPEN_SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED = 0x80000,
} open_system_runtime_flag_e;

extern s32 g_open_system_result;
extern u32 g_open_system_runtime_flags;
extern s32 g_open_system_vsync_mode;
extern open_gfx_transition_entry_t** g_open_new_game_transition_entries;
void open_gfx_transition_finished_stub(void);
s32 open_system_run_main_loop(s32 skip_movie);
void open_system_run_ending_cutscene(void);

/* other */
extern u8 g_open_work_buffer_0[];
extern u8 g_open_work_buffer_1[];

#endif
