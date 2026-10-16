#ifndef TITLE_H
#define TITLE_H

/* The title screen and its menu, sound type and card warning. */

#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

union controller_record;

s32 title_run_main_loop(s32 skip_movie);
void title_init_sequence(s32 skip_movie);
void title_load_menu_graphics(void);
s32 title_check_card_slots(void);

typedef enum title_menu_entry {
    TITLE_MENU_NEW_GAME = 0,
    TITLE_MENU_CONTINUE = 1,
    TITLE_MENU_TUTORIAL = 2,
    TITLE_MENU_SOUND = 3,
} title_menu_entry_e;

typedef enum title_state {
    TITLE_STATE_MENU = 0,
    TITLE_STATE_SOUND_MENU = 2, /* pushes the sound type menu on the next update */
    TITLE_STATE_DEMO_MOVIE = 3, /* idle timeout: starts the next demo movie once the screen has faded */
} title_state_e;

/* 32-bit state of the title-exit controller (title_update_card_warning). */
typedef enum title_exit_step {
    TITLE_EXIT_INPUT = 0,
    TITLE_EXIT_CANCEL = 1,  /* cross: back to the title menu */
    TITLE_EXIT_CONFIRM = 2, /* circle or start: the new-game prologue */
} title_exit_step_e;

/* Title-menu controller state (handler 2), pushed into the next controller
 * record by title_push_menu and run by title_update_menu. */
typedef struct title_menu_state {
    /* 0x00 */ s32 records_36[5]; /* logo, then the four menu items */
    /* 0x14 */ s32 idle_timer;
    /* 0x18 */ s32 cd_end_position;
    /* 0x1c */ s32 cursor; /* title_menu_entry_e */
    /* 0x20 */ s32 state;  /* title_state_e */
    /* 0x24 */ s32 cursor_record_36;
    /* 0x28 */ s32 _unused_28[2];
    /* 0x30 */ s32 exit_timer;
    /* 0x34 */ s32 exiting;
    /* 0x38 */ u8 _unused_38[0x64 - 0x38];
} title_menu_state_t;

/* Sound-type menu controller state (handler 3). */
enum { TITLE_SOUND_MENU_ENTRY_COUNT = 3 };

typedef struct title_sound_type_menu {
    /* 0x00 */ s32 entry_render_indices[TITLE_SOUND_MENU_ENTRY_COUNT]; /* Mono, Stereo and Wide labels */
    u8 _unused_0c[16];
    /* 0x1c */ s32 sound_mode_selection; /* Signed so decrement can wrap below Mono. */
    /* 0x20 */ s32 step;
} title_sound_type_menu_t;

typedef char title_sound_type_menu_size_must_be_36[sizeof(title_sound_type_menu_t) == 36 ? 1 : -1];

/* New-game start controller state (handler 4). */
typedef struct new_game_state {
    /* 0x00 */ u8 _unused_00[0x10];
    /* 0x10 */ s32 step;
    /* 0x14 */ u8 _unused_14[0x0c];
    /* 0x20 */ s32 name_text_id;
} new_game_state_t;

/* Birthday date-selection controller state (handler 5), shared with its redraw routine. */
typedef struct birthday_date_state {
    s32 cursor_record_36; /* 0x00: cursor moved between month and day */
    s32 label_record;
    s32 title_record_56;  /* 0x08: birthday title image (0xb800); palette 2 on
                             confirmation, 0 on restore */
    s32 window_record_56; /* 0x0c: flags bit 8 blocks input */
    s32 _unused_10[4];
    s32 month;           /* 0x20: 1..12 */
    s32 day;             /* 0x24 */
    s32 selecting_month; /* 0x28: 0 edits the day, 1 edits the month */
} birthday_date_state_t;

/* Final birthday-confirmation controller state (handler 6). */
typedef struct birthday_confirmation_state {
    s32 cursor_record_36; /* 0x00 */
    s32 label_record;     /* 0x04 */
    s32 menu_record_56;   /* 0x08 */
    s32 selected_option;  /* 0x0C: 0 confirms, 1 returns to date entry */
    s32 _unused_10[4];
    s32 month; /* 0x20 */
    s32 day;   /* 0x24 */
    u8 _unused_28[0x64 - 0x28];
} birthday_confirmation_state_t;

typedef char birthday_confirmation_state_size_must_be_100[(sizeof(birthday_confirmation_state_t) == 100) ? 1 : -1];

/* Music test controller state (handler 7), pushed by music_test_open. */
typedef struct music_test_state {
    /* 0x00 */ s32 music_id;     /* scenario music selected for playback */
    /* 0x04 */ s32 delay;        /* frames before loading music_id */
    /* 0x08 */ s32 music_slot;   /* slot from music_open_into_free_slot */
    /* 0x0c */ s32 step;         /* music_test_step_e */
    /* 0x10 */ s32 blink_frames; /* counts ten frames after a selection, while Up and Down stay blocked */
} music_test_state_t;

/* Title exit controller state (handler 12). */
typedef struct title_exit_state {
    s32 state; /* title_exit_step_e */
} title_exit_state_t;

void title_update_menu(union controller_record* record);
void title_update_sound_type_menu(union controller_record* record);
void new_game_update(union controller_record* record);
void birthday_update_date_menu(union controller_record* record);
void birthday_update_confirmation(union controller_record* record);
void music_test_update(union controller_record* record);
void title_update_card_warning(union controller_record* record);
extern s32 g_title_demo_movie_index;
void title_push_menu(s32 argument);
void title_select_new_game(void);
void new_game_init_party(s32 party_mode, s32 world_load_mode);
extern u8 g_birthday_title_image[];
extern u8 g_birthday_window_image[];

/* Step of the music test controller (music_test_update). */
typedef enum music_test_step {
    MUSIC_TEST_STEP_IDLE = 0,
    MUSIC_TEST_STEP_CLOSE = 1,       /* waits for the list thread, then fades the screen */
    MUSIC_TEST_STEP_FADE_OUT = 2,    /* waits for the fade, then returns to the title menu */
    MUSIC_TEST_STEP_START_TRACK = 3, /* waits for the music to load, then plays it */
    MUSIC_TEST_STEP_OPEN = 4,        /* waits for the fade, then starts the list thread */
} music_test_step_e;

/* The slot of the music test's list thread. */
enum {
    MUSIC_TEST_THREAD_SLOT_LIST = 12,
};

extern menu_entry_t g_music_test_list_params;
extern menu_text_layout_t g_music_test_list_state;
extern s16 g_music_test_list_entries[96];
extern s16 g_music_test_list_entry_flags[96];
extern s16 g_music_test_selected_option;
extern u8 g_music_test_music_ids[];               /* music test entry -> scenario music id */
extern suzuki_music_t* g_music_test_sound_handle; /* handle of the playing music test track */

/* Entry of g_gfx_otags the WORLD menu frame draws into while
 * SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME is set. */
extern s32 g_music_test_world_menu_otag_index;

void music_test_open(void);
enum { TITLE_MENU_ENTRY_HIGHLIGHTED_PALETTE = 9 };
void title_start_logo_movie_or_menu(s32 use_alternate);

#endif
