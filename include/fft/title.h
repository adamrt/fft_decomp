#ifndef TITLE_H
#define TITLE_H

/* OPEN's title screen: its main loop, the title menu and the controllers it pushes (sound type, card warning, new game,
 * birthday, music test). */

#include "fft/menu.h"
#include "psx/types.h"

struct suzuki_music;
union controller_record;

/* title screen */
/* Run OPEN's title/movie loop and return the next game route; nonzero skips the opening movie. */
s32 open_title_entrypoint(s32 skip_movie);
void title_init_sequence(s32 skip_movie);
void title_start_logo_movie_or_menu(s32 use_alternate);
void title_load_menu_graphics(void);
s32 title_check_card_slots(void);

/* title menu */
/* title_menu_state_t.cursor: the four title menu entries. */
typedef enum title_menu_entry {
    TITLE_MENU_NEW_GAME = 0,
    TITLE_MENU_CONTINUE = 1,
    TITLE_MENU_TUTORIAL = 2,
    TITLE_MENU_SOUND = 3
} title_menu_entry_e;

/* display_object_animated_t.palette of the selected menu entry. */
enum { TITLE_MENU_ENTRY_HIGHLIGHTED_PALETTE = 9 };

/* title_menu_state_t.state. */
typedef enum title_state {
    TITLE_STATE_MENU = 0,
    TITLE_STATE_SOUND_MENU = 2, /* pushes the sound type menu on the next update */
    TITLE_STATE_DEMO_MOVIE = 3  /* idle timeout: starts the next demo movie once the screen has faded */
} title_state_e;

/* Title-menu controller state (handler 2), pushed into the next controller
 * record by title_push_menu and run by title_update_menu. */
typedef struct title_menu_state {
    /* 0x00 */ s32 records_36[5]; /* logo, then the four menu items */
    /* 0x14 */ s32 idle_timer;
    /* 0x18 */ s32 cd_end_position;
    /* 0x1c */ s32 cursor; /* title_menu_entry_e */
    /* 0x20 */ s32 state;  /* title_state_e */
    /* 0x24 */ s32 cursor_record;
    /* 0x28 */ s32 _unused_28[2];
    /* 0x30 */ s32 exit_timer;
    /* 0x34 */ s32 exiting;
    /* 0x38 */ u8 _unused_38[0x64 - 0x38];
} title_menu_state_t;

enum { TITLE_SOUND_MENU_ENTRY_COUNT = 3 };

/* Sound-type menu controller state (handler 3). */
typedef struct title_sound_type_menu {
    /* 0x00 */ s32 entry_render_indices[TITLE_SOUND_MENU_ENTRY_COUNT]; /* Mono, Stereo and Wide labels */
    u8 _unused_0c[16];
    /* 0x1c */ s32 sound_mode_selection; /* Signed so decrement can wrap below Mono. */
    /* 0x20 */ s32 step;
} title_sound_type_menu_t;
typedef char title_sound_type_menu_size_must_be_36[sizeof(title_sound_type_menu_t) == 36 ? 1 : -1];

/* 32-bit state of the no-memory-card warning controller (title_update_card_warning). */
typedef enum title_card_warning_step {
    TITLE_CARD_WARNING_INPUT = 0,
    TITLE_CARD_WARNING_CANCEL = 1, /* cross: back to the title menu */
    TITLE_CARD_WARNING_CONFIRM = 2 /* circle or start: the new-game prologue */
} title_card_warning_step_e;

/* No-memory-card warning controller state (handler 12). */
typedef struct title_card_warning_state {
    s32 state; /* title_card_warning_step_e */
} title_card_warning_state_t;

extern s32 g_title_demo_movie_index;

void title_push_menu(s32 argument);
void title_update_menu(union controller_record* record);
void title_update_sound_type_menu(union controller_record* record);
void title_select_new_game(void);
void title_update_card_warning(union controller_record* record);

/* new game */
/* New-game start controller state (handler 4). */
typedef struct new_game_state {
    /* 0x00 */ u8 _unused_00[0x10];
    /* 0x10 */ s32 step;
    /* 0x14 */ u8 _unused_14[0x0c];
    /* 0x20 */ s32 name_text_id;
} new_game_state_t;

void new_game_update(union controller_record* record);
void new_game_init_party(s32 party_mode, s32 world_load_mode);

/* birthday */
/* Birthday date-selection controller state (handler 5), shared with its redraw routine. */
typedef struct birthday_date_state {
    s32 cursor_record;         /* 0x00: cursor moved between month and day */
    s32 retained_title_record; /* 0x04: title-menu actor index; the date constructor does not set it */
    s32 title_image;           /* 0x08: birthday title image (0xb800); palette 2 on
                                      confirmation, 0 on restore */
    s32 window_image;          /* 0x0c: flags bit 8 blocks input */
    s32 _unused_10[4];
    s32 month;           /* 0x20: 1..12 */
    s32 day;             /* 0x24 */
    s32 selecting_month; /* 0x28: 0 edits the day, 1 edits the month */
} birthday_date_state_t;

/* Final birthday-confirmation controller state (handler 6). */
typedef struct birthday_confirmation_state {
    s32 cursor_record;   /* 0x00 */
    s32 label_record;    /* 0x04 */
    s32 menu_image;      /* 0x08 */
    s32 selected_option; /* 0x0C: 0 confirms, 1 returns to date entry */
    s32 _unused_10[4];
    s32 month; /* 0x20 */
    s32 day;   /* 0x24 */
    u8 _unused_28[0x64 - 0x28];
} birthday_confirmation_state_t;
typedef char birthday_confirmation_state_size_must_be_100[(sizeof(birthday_confirmation_state_t) == 100) ? 1 : -1];

void birthday_update_date_menu(union controller_record* record);
void birthday_update_confirmation(union controller_record* record);

/* music test */
/* Step of the music test controller (music_test_update). */
typedef enum music_test_step {
    MUSIC_TEST_STEP_IDLE = 0,
    MUSIC_TEST_STEP_CLOSE = 1,       /* waits for the list thread, then fades the screen */
    MUSIC_TEST_STEP_FADE_OUT = 2,    /* waits for the fade, then returns to the title menu */
    MUSIC_TEST_STEP_START_TRACK = 3, /* waits for the music to load, then plays it */
    MUSIC_TEST_STEP_OPEN = 4         /* waits for the fade, then starts the list thread */
} music_test_step_e;

/* Music test controller state (handler 7), pushed by music_test_open. */
typedef struct music_test_state {
    /* 0x00 */ s32 music_id;     /* scenario music selected for playback */
    /* 0x04 */ s32 delay;        /* frames before loading music_id */
    /* 0x08 */ s32 music_slot;   /* slot from music_load_into_free_slot */
    /* 0x0c */ s32 step;         /* music_test_step_e */
    /* 0x10 */ s32 blink_frames; /* counts ten frames after a selection, while Up and Down stay blocked */
} music_test_state_t;

/* The slot of the music test's list thread. */
enum { MUSIC_TEST_THREAD_SLOT_LIST = 12 };

/* Entry of g_gfx_otags the WORLD menu frame draws into while
 * SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME is set. */
extern s32 g_music_test_world_menu_otag_index;

void music_test_open(void);
void music_test_update(union controller_record* record);

#endif
