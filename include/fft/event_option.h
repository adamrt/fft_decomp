#ifndef EVENT_OPTION_H
#define EVENT_OPTION_H

/* EVENT/OPTION.OUT: menu screen overlay that runs with BATTLE. */

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

/* screen */
extern s32 g_option_menu_state;

void option_entrypoint(s32 menu_type);

/* game options */
extern s32 g_option_choice_counts[];
extern s32 g_option_choice_window_widths[];
extern s32 g_option_current_values[GAME_OPTION_UNCHANGED + 1];
extern menu_icon_record_t g_option_picker_icon_records[2];
extern s32 g_option_reference_values[];
extern s32 g_option_value_text_id_bases[];

void option_menu_run_game_options(void);
void option_menu_run_choice(void);

/* at list */
extern menu_list_page_t g_option_at_list_pages[2];
extern battle_at_entry_t g_option_menu_at_list_descriptors[BATTLE_AT_LIST_CAPACITY];
extern menu_text_layout_t g_option_menu_at_list_entry_table[1];
/* OPTION addresses the at-list record's context and menu type as separate
 * globals (0x30 and 0x3a into g_menu_at_list_state). */
extern menu_text_layout_t* g_option_menu_at_list_entry_table_pointer;
extern s16 g_option_menu_at_list_menu_type;
extern s16 g_option_menu_at_list_flags[];
extern s16 g_option_menu_at_list_indices[];
extern s16 g_option_menu_at_list_primary_values[];
extern s16 g_option_menu_at_list_secondary_values[];

void option_menu_run_at_list(void);
void option_menu_start_at_list_thread(void);

/* dead unit */
extern menu_entry_t g_option_dead_unit_menu[DEAD_UNIT_MENU_ENTRY_COUNT];
extern menu_text_layout_t g_option_dead_unit_menu_entries;
extern s16 g_option_dead_unit_menu_flags[];
extern s16 g_option_dead_unit_primary_result;
extern s16 g_option_dead_unit_secondary_result;

/* menus */
extern menu_text_layout_t g_option_menu_entry_table[1];
extern options_menu_page_t g_option_menu_pages[2];
extern RECT g_option_menu_title_image_rect;
extern RECT g_option_menu_value_image_rect;

/* graphics */

/* text */

void text_set_blit_destination(s16 destination_x, s16 destination_y);

/* input */
extern u32* g_option_dead_unit_controller_input;
extern menu_input_timing_profile_t g_option_input_repeat_timings[];

/* threads */
extern menu_entry_t g_option_menu_icon_thread_param;

/* unit panels */
extern RECT g_option_menu_entry_panel_rect;

#endif
