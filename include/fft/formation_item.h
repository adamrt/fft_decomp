#ifndef FORMATION_ITEM_H
#define FORMATION_ITEM_H

/* The Formation screen's item menu in WORLD: equipping, best fit and removing items, the item action and sort menus,
 * the category tabs and icon strips, and the item list. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/screen.h"
#include "psx/types.h"

/* item menu */
/* Command texts of the Formation Item menu, chosen by the
 * selected unit's formation_unit_lock_e. */
enum { FORMATION_ITEM_MENU_TEXT_UNLOCKED = 0xd801, FORMATION_ITEM_MENU_TEXT_LOCKED = 0xd802 };

/* Menu-subflow "running" flags. The readers sign-extend (lb), so these are
 * signed; the initializer only stores 0/1/2 and cannot distinguish. */
extern s8 g_formation_item_menu_initialized;
extern s8 g_formation_item_best_remove_initialized;
extern s8 g_formation_item_submenu_initialized;
extern s8 g_formation_item_action_menu_initialized;
extern s8 g_formation_item_sort_order_initialized;
extern s8 g_formation_item_equipping_units_initialized;

void formation_item_menu(void);
s32 formation_item_get_hp_bonus(s32 unused_row);
s32 formation_item_get_mp_bonus(s32 unused_row);

/* equipping */
extern s8 g_formation_item_equip_menu_initialized;

s32 formation_item_can_equip_in_slot(s16 unit_index, s16 slot, s16 item_id);
s32 formation_item_equip(s16 unit, s16 slot, s16 item_id);
menu_equipment_slot_e formation_item_select_slot(s16 unit_index, s32 item_id);
void formation_item_build_best_fit(s16 unit_index, s16* equipment);

/* item actions */
s8 formation_item_action_menu(void);
s8 formation_item_sort_menu(void);
s32 formation_item_show_equipping_units(void);

/* categories */
extern s8 g_formation_item_category_menu_initialized;
extern u8 g_formation_item_category_input_lock;

/* item list */
extern u8 g_formation_item_list_window_script[194];

s32 formation_item_build_list(s16 unit_id, s16 sort_mode, s8 category, s16* entries, u8 mode);

#endif
