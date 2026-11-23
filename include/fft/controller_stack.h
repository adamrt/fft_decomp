#ifndef CONTROLLER_STACK_H
#define CONTROLLER_STACK_H

/* OPEN's stack of per-frame controllers: the handler table, the 100-byte state record each handler lays out, and the
 * dispatcher. */

#include "fft/movie.h"
#include "fft/title.h"
#include "psx/types.h"

/* Rows of g_controller_handlers: the value a push stores in
 * g_controller_handler_indices. */
typedef enum controller_handler {
    CONTROLLER_FFTST_MOVIE = 0,           /* update_logo_movie */
    CONTROLLER_FFTOP_MOVIE = 1,           /* update_opening_movie */
    CONTROLLER_TITLE_MENU = 2,            /* update_main_menu */
    CONTROLLER_SOUND_TYPE_MENU = 3,       /* update_sound_menu */
    CONTROLLER_NEW_GAME_START = 4,        /* update_new_game */
    CONTROLLER_BIRTHDAY_DATE = 5,         /* update_birthday_menu */
    CONTROLLER_BIRTHDAY_CONFIRMATION = 6, /* update_birthday_confirmation */
    CONTROLLER_MUSIC_TEST = 7,            /* music_test_update_menu */
    CONTROLLER_TEXT_MESSAGE = 8,          /* controller_update_text_message */
    CONTROLLER_FFTEND_MOVIE = 9,          /* movie_update_fftend */
    CONTROLLER_SCRIPT = 10,               /* ending_update_script */
    CONTROLLER_TITLE_DEMO_MOVIE = 11,     /* movie_update_title_demo */
    CONTROLLER_TITLE_CARD_WARNING = 12    /* title_update_card_warning */
} controller_handler_e;

/* Text message controller state (handler 8). */
typedef struct controller_text_message_state {
    s32 use_world_text;
    s32 saved_blocked_buttons; /* g_menu_blocked_buttons when the message started */
} controller_text_message_state_t;

/* One state record per controller. At depth c, dispatch uses slot c - 1.
 * A push fills slot c and its handler index before incrementing the depth. */
typedef union controller_record {
    movie_stream_state_t movie;
    title_menu_state_t title;
    title_sound_type_menu_t sound_menu;
    new_game_state_t new_game;
    birthday_date_state_t birthday_date;
    birthday_confirmation_state_t birthday_confirmation;
    music_test_state_t music_test;
    controller_text_message_state_t text_message;
    title_card_warning_state_t card_warning;
} controller_record_t;
typedef char controller_record_size_must_be_100[(sizeof(controller_record_t) == 100) ? 1 : -1];

enum { CONTROLLER_STACK_CAPACITY = 4 };

extern s32 g_controller_handler_indices[CONTROLLER_STACK_CAPACITY]; /* controller_handler_e */
extern controller_record_t g_controller_records[CONTROLLER_STACK_CAPACITY];
extern s32 g_controller_depth;

void controller_dispatch_current(void);
void controller_update_text_message(controller_record_t* record);

#endif
