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

/* at list */
extern menu_list_page_t g_option_at_list_pages[2];
extern battle_at_entry_t g_option_menu_at_list_descriptors[BATTLE_AT_LIST_CAPACITY];
/* OPTION addresses the at-list record's context and menu type as separate
 * globals (0x30 and 0x3a into g_menu_at_list_state). */
extern s16 g_option_menu_at_list_flags[];
extern s16 g_option_menu_at_list_indices[];
extern s16 g_option_menu_at_list_primary_values[];
extern s16 g_option_menu_at_list_secondary_values[];

/* dead unit */
extern menu_entry_t g_option_dead_unit_menu[DEAD_UNIT_MENU_ENTRY_COUNT];
extern menu_text_layout_t g_option_dead_unit_menu_entries;
extern s16 g_option_dead_unit_menu_flags[];
extern s16 g_option_dead_unit_primary_result;
extern s16 g_option_dead_unit_secondary_result;

/* menus */

/* graphics */

/* text */

void text_set_blit_destination(s16 destination_x, s16 destination_y);

/* input */
extern u32* g_option_dead_unit_controller_input;

/* threads */

/* unit panels */

#endif
