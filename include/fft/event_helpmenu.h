#ifndef EVENT_HELPMENU_H
#define EVENT_HELPMENU_H

/* EVENT/HELPMENU.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"
#include "fft/bits.h"
#include "fft/card.h"
#include "fft/file.h"
#include "fft/item.h"

/* help menus */
extern s16 g_helpmenu_selected_unit_panel_mode;

void helpmenu_menu_run_battle_help(void);
void helpmenu_menu_run_require_help(void);

/* saved panels */
/* Saved copies of four BATTLE records (menu_copy_billboard_data_out);
 * a second, unused slot follows the banner and the unit record. */
extern menu_unit_status_record_t g_helpmenu_active_banner;
extern menu_active_unit_t g_helpmenu_active_unit_data;
/* 0x40-byte billboard filled by menu_copy_billboard_data_out; +0x0e is the monster-skillset flag. */
extern menu_status_panel_slot_storage_t g_helpmenu_billboard;
extern map_selected_tile_t g_helpmenu_menu_cursor_tile;

/* navigation */
extern help_navigation_record_t g_helpmenu_require_navigation[];
extern help_navigation_record_t g_helpmenu_view_mode_2_navigation[];
extern help_navigation_record_t g_helpmenu_view_mode_3_navigation[];
extern help_navigation_record_t g_helpmenu_view_mode_4_navigation[];

/* text */
extern u16* g_helpmenu_current_text_ids;
extern u16* const g_helpmenu_menu_text_id_tables[23];
extern u8 g_helpmenu_text_data[];
/* Section offsets of the help text file, relative to g_helpmenu_text_data (0x80 bytes before it). */
extern s32 g_helpmenu_text_section_offsets[32];

/* input */
extern u32* g_helpmenu_controller_state_ptr;

/* threads */

#endif
