#ifndef CONTROLLER_H
#define CONTROLLER_H

/* OPEN's stack of per-frame controllers: the handler table, the 100-byte state record each handler lays out, and the
 * dispatcher. */

#include "fft/movie.h"
#include "fft/title.h"
#include "psx/types.h"

/* Rows of g_controller_handlers: the value a push stores in
 * g_controller_handler_indices. */
typedef enum controller_handler {
    CONTROLLER_FFTST_MOVIE = 0,           /* movie_update_fftst */
    CONTROLLER_FFTOP_MOVIE = 1,           /* movie_update_fftop */
    CONTROLLER_TITLE_MENU = 2,            /* title_update_menu */
    CONTROLLER_SOUND_TYPE_MENU = 3,       /* title_update_sound_type_menu */
    CONTROLLER_NEW_GAME_START = 4,        /* new_game_update */
    CONTROLLER_BIRTHDAY_DATE = 5,         /* birthday_update_date_menu */
    CONTROLLER_BIRTHDAY_CONFIRMATION = 6, /* birthday_update_confirmation */
    CONTROLLER_MUSIC_TEST = 7,            /* music_test_update */
    CONTROLLER_TEXT_MESSAGE = 8,          /* controller_wait_text_message */
    CONTROLLER_FFTEND_MOVIE = 9,          /* movie_update_fftend */
    CONTROLLER_SCRIPT = 10,               /* ending_update_script */
    CONTROLLER_TITLE_DEMO_MOVIE = 11,     /* movie_update_title_demo */
    CONTROLLER_TITLE_EXIT = 12,           /* title_update_card_warning */
} controller_handler_e;

/* Text message controller state (handler 8). */
typedef struct controller_text_message_state {
    s32 use_world_text;
    s32 saved_blocked_buttons; /* g_menu_blocked_buttons when the message started */
} controller_text_message_state_t;

/* One 100-byte state record per controller, laid out by its handler. The current controller c runs
 * g_controller_handlers[g_controller_handler_indices[c - 1]] on
 * g_controller_records[c]. A push made while the current index is c
 * fills g_controller_records_next[c] (which is records[c + 1]) and
 * stores the new handler id at g_controller_handler_indices[c]. */
typedef union controller_record {
    movie_stream_state_t movie;
    title_menu_state_t title;
    title_sound_type_menu_t sound_menu;
    new_game_state_t new_game;
    birthday_date_state_t birthday_date;
    birthday_confirmation_state_t birthday_confirmation;
    music_test_state_t music_test;
    controller_text_message_state_t text_message;
    title_exit_state_t title_exit;
} controller_record_t;
typedef char controller_record_size_must_be_100[(sizeof(controller_record_t) == 100) ? 1 : -1];

typedef void (*controller_handler_t)(controller_record_t* record);

extern controller_handler_t g_controller_handlers[];
extern s32 g_controller_handler_indices[]; /* controller_handler_e */
extern controller_record_t g_controller_records[];
extern controller_record_t g_controller_records_next[];
extern s32 g_controller_current_index;
extern s32 g_controller_selected_options[];

void controller_dispatch_current(void);
void controller_wait_text_message(controller_record_t* record);

#endif
