#ifndef EVENT_OPTION_H
#define EVENT_OPTION_H

/* EVENT/OPTION.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/job.h"
#include "fft/menu.h"
#include "psx/types.h"

/* screen */
extern s32 g_option_menu_state;

void option_entrypoint(s32 menu_type);

/* game options */

/* at list */
extern battle_at_entry_t g_option_menu_at_list_descriptors[BATTLE_AT_LIST_CAPACITY];

/* dead unit */
extern s16 g_option_dead_unit_primary_result;
extern s16 g_option_dead_unit_secondary_result;

/* menus */

/* graphics */

/* text */

void text_set_blit_destination(s16 destination_x, s16 destination_y);

/* input */

/* threads */

/* unit panels */

#endif
