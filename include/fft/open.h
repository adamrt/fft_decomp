#ifndef OPEN_H
#define OPEN_H

/* OPEN/OPEN.BIN: the opening and title program: main loop, controller stack and the title screens. */

#include "fft/menu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

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

extern u32 g_open_system_runtime_flags;
extern s32 g_open_system_result;
extern s32 g_open_system_vsync_mode;
extern u8 g_open_work_buffer_0[];
extern u8 g_open_work_buffer_1[];

s32 open_system_run_main_loop(s32 skip_movie);
void open_system_init_title_sequence(s32 skip_movie);
void open_system_run_ending_cutscene(void);

/* input */
extern s32 g_open_input_directional_repeat_initial_delay;
extern s32 g_open_input_directional_repeat_period;
extern s32 g_open_input_up_repeat_counter;
extern s32 g_open_input_down_repeat_counter;
extern s32 g_open_input_left_repeat_counter;
extern s32 g_open_input_right_repeat_counter;
/* Volatile: open_menu_handle_sound_type_input reloads it (lui/lw) for every
 * test; readers that test it twice from one load take a local copy. */
extern volatile u32 g_open_input_new_button_presses;

void open_input_init_directional_state(void);
u32 open_input_update_repeat(u32 buttons);

/* files */
extern void* g_open_file_destination;
extern s32 g_open_file_current_openbk_image_id;
extern const s32 g_open_file_openbk_start_sector_offsets[];
extern const s32 g_open_file_openbk_end_sector_offsets[];

void open_file_request_openbk_image(s32 image_id, void* destination);
void open_file_load_main_menu_graphics(void);
void open_file_load_end_movie_graphics(void);
s32 open_file_get_cd_sync_state_delta(void);

/* memory card */
s32 open_card_check_slots(void);

/* sound */
/* Fades the CD audio in at the given mix level (suzuki_sound_put_type), or out when it is 0. */
void open_sound_set_cd_mix_and_volume(s32 cd_mix_level, s32 duration);

/* controllers */
/* Rows of g_open_controller_handlers: the value a push stores in
 * g_open_controller_handler_indices. */
typedef enum open_controller_handler {
    OPEN_CONTROLLER_FFTST_MOVIE = 0,           /* open_movie_update_fftst */
    OPEN_CONTROLLER_FFTOP_MOVIE = 1,           /* open_movie_update_fftop */
    OPEN_CONTROLLER_TITLE_MENU = 2,            /* open_title_update_menu */
    OPEN_CONTROLLER_SOUND_TYPE_MENU = 3,       /* open_menu_handle_sound_type_input */
    OPEN_CONTROLLER_NEW_GAME_START = 4,        /* open_title_step_new_game_start */
    OPEN_CONTROLLER_BIRTHDAY_DATE = 5,         /* open_birthday_handle_menu_input */
    OPEN_CONTROLLER_BIRTHDAY_CONFIRMATION = 6, /* open_birthday_handle_confirmation_input */
    OPEN_CONTROLLER_MUSIC_TEST = 7,            /* open_menu_update_music_test */
    OPEN_CONTROLLER_TEXT_MESSAGE = 8,          /* open_controller_wait_text_message */
    OPEN_CONTROLLER_FFTEND_MOVIE = 9,          /* open_movie_update_fftend */
    OPEN_CONTROLLER_SCRIPT = 10,               /* open_script_update_controller */
    OPEN_CONTROLLER_TITLE_DEMO_MOVIE = 11,     /* open_movie_update_title_demo */
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
    OPEN_TITLE_EXIT_CONFIRM = 2, /* circle or start: the new-game prologue */
} open_title_exit_state_id_e;

/* Movie controller state (handlers 0 and 11): the stream the FFTST and title demo movies play. */
typedef struct open_movie_stream_controller_state {
    s32 start_sector;
    s32 stream_length;
} open_movie_stream_controller_state_t;

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

/* Sound-type menu controller state (handler 3). */
enum { OPEN_SOUND_MENU_ENTRY_COUNT = 3 };

typedef struct open_sound_menu_state {
    /* 0x00 */ s32 entry_render_indices[OPEN_SOUND_MENU_ENTRY_COUNT]; /* Mono, Stereo and Wide labels */
    u8 _unused_0c[16];
    /* 0x1c */ s32 sound_mode_selection; /* Signed so decrement can wrap below Mono. */
    /* 0x20 */ s32 step;
} open_sound_menu_state_t;
typedef char open_sound_menu_state_size_must_be_36[sizeof(open_sound_menu_state_t) == 36 ? 1 : -1];

/* New-game start controller state (handler 4). */
typedef struct open_title_new_game_state {
    /* 0x00 */ u8 _unused_00[0x10];
    /* 0x10 */ s32 step;
    /* 0x14 */ u8 _unused_14[0x0c];
    /* 0x20 */ s32 name_text_id;
} open_title_new_game_state_t;

/* Birthday date-selection controller state (handler 5), shared with its redraw routine. */
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

/* Final birthday-confirmation controller state (handler 6). */
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
    s32 use_world_text;
    s32 saved_blocked_buttons; /* g_menu_blocked_buttons when the message started */
} open_controller_thread_completion_t;

/* Title exit controller state (handler 12). */
typedef struct open_title_exit_state {
    s32 state; /* open_title_exit_state_id_e */
} open_title_exit_state_t;

/* One 100-byte state record per controller, laid out by its handler. The current controller c runs
 * g_open_controller_handlers[g_open_controller_handler_indices[c - 1]] on
 * g_open_controller_records[c]. A push made while the current index is c
 * fills g_open_controller_records_next[c] (which is records[c + 1]) and
 * stores the new handler id at g_open_controller_handler_indices[c]. */
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
/* The handlers, in open_controller_handler_e order. */
void open_movie_update_fftst(open_controller_record_t* record);
void open_movie_update_fftop(open_controller_record_t* unused_record);
void open_title_update_menu(open_controller_record_t* record);
void open_menu_handle_sound_type_input(open_controller_record_t* record);
void open_title_step_new_game_start(open_controller_record_t* record);
void open_birthday_handle_menu_input(open_controller_record_t* record);
void open_birthday_handle_confirmation_input(open_controller_record_t* record);
void open_menu_update_music_test(open_controller_record_t* record);
void open_controller_wait_text_message(open_controller_record_t* record);
void open_movie_update_fftend(open_controller_record_t* unused_record);
void open_script_update_controller(open_controller_record_t* unused_record);
void open_movie_update_title_demo(open_controller_record_t* unused_record);
void open_title_update_card_warning(open_controller_record_t* record);

/* title */
extern s32 g_open_title_demo_movie_index;

void open_title_push_menu_controller(s32 argument);
void open_title_select_new_game(void);
void open_prologue_start(void);
void open_title_init_new_game_party(s32 party_mode, s32 world_load_mode);

/* birthday */
extern u8 g_open_birthday_title_image[];
extern u8 g_open_birthday_window_image[];

void open_birthday_convert_date_to_zodiac(s32* month, s32* day);

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

extern menu_entry_t g_open_music_test_list_params;
extern menu_text_layout_t g_open_music_test_list_state;
extern s16 g_open_music_test_list_entries[96];
extern s16 g_open_music_test_list_entry_flags[96];
extern s16 g_open_music_test_selected_option;
extern u8 g_open_music_test_music_ids[];               /* music test entry -> scenario music id */
extern suzuki_music_t* g_open_music_test_sound_handle; /* handle of the playing music test track */
/* Entry of g_open_gfx_otags the WORLD menu frame draws into while
 * OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME is set. */
extern s32 g_open_menu_world_frame_otag_index;

void open_menu_start_music_test_controller(void);

#endif
