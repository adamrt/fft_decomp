#ifndef OPTION_H
#define OPTION_H

/* Option: the game options screen and its values. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/state.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern menu_input_timing_profile_t g_option_input_repeat_timings[];

/* The argument of option_entrypoint and option_entrypoint: which of
 * the OPTION overlay's screens to run. */
typedef enum menu_option_type {
    MENU_OPTION_TYPE_GAME_OPTIONS = 0,
    MENU_OPTION_TYPE_AT_LIST = 1,
    MENU_OPTION_TYPE_CRYSTAL_PICKUP = 2, /* crystal and treasure panel */
} option_screen_e;

/* One of the two 0x190-byte packet pages of the options screen (g_option_pages), alternated by frame.
 * Provisional. */
typedef struct options_menu_page {
    SPRT title;                         /* 0x000: window title image */
    SPRT value;                         /* 0x014: selected value image */
    scaled_draw_area_pair_t title_area; /* 0x028 */
    scaled_draw_area_pair_t value_area; /* 0x050 */
    DR_MODE text_mode;                  /* 0x078 */
    DR_MODE window_mode;                /* 0x084 */
    DR_MODE icon_mode;                  /* 0x090 */
    menu_icon_record_t icons;           /* 0x09c */
    SPRT frame;                         /* 0x118: cursor frame */
    SPRT arrows[2];                     /* 0x12c: scroll arrows */
    SPRT arrow_marks[2];                /* 0x154 */
    SPRT thumb;                         /* 0x17c: scroll-bar thumb */
} option_page_t;

typedef char option_page_size_must_be_400[(sizeof(option_page_t) == 400) ? 1 : -1];
extern option_page_t g_option_pages[2];
extern menu_text_layout_t g_option_entry_table[1];
extern menu_entry_t g_option_icon_thread_param;
extern menu_icon_record_t g_option_picker_icon_records[2];

/* The values being edited, one per game_option_index_e. */
extern s32 g_option_values[GAME_OPTION_UNCHANGED + 1];

extern s32 g_option_reference_values[];
extern s32 g_option_choice_counts[];
extern s32 g_option_choice_widths[];
extern s32 g_option_value_text_id_bases[];
extern RECT g_option_title_image_rect;
extern RECT g_option_entry_panel_rect;
extern RECT g_option_value_image_rect;

/* The value picker thread of one game option. */
void option_choice_thread(void);

void option_entrypoint(s32 menu_type);
void option_game_options_thread(void);
void option_open(void);
void option_entrypoint(s32 mode);
void option_game_options_thread(void);
void option_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
extern const RECT g_option_draw_area_template;

#endif
