#ifndef WORLD_ITEM_H
#define WORLD_ITEM_H

/* WORLD's item menus: the formation item menu, the category tabs and icon strips, the item list, and item counts and
 * prices. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/menu_status.h"
#include "fft/world_menu.h"
#include "psx/types.h"

/* item menu */
/* Menu-subflow "running" flags. The readers sign-extend (lb), so these are
 * signed; the initializer only stores 0/1/2 and cannot distinguish. */
extern s8 g_formation_item_menu_initialized;
extern s8 g_formation_item_best_remove_initialized;
extern s8 g_formation_item_submenu_initialized;
extern s8 g_formation_item_action_menu_initialized;
extern s8 g_formation_item_sort_order_initialized;
extern s8 g_formation_item_equipping_units_initialized;
extern s8 g_formation_item_menu_mode; /* menu_item_list_state_e */
extern menu_instruction_t g_formation_item_menu_script;
extern menu_entry_t g_formation_item_icon_menu;
extern u8 g_formation_item_menu_unit_index;
extern u16 g_formation_item_saved_selected_unit_index;
extern u8 g_formation_item_menu_saved_browse_enabled;
extern u8 g_formation_item_submenu_exit_pending;
extern screen_number_range_t g_formation_item_action_quantity_range;
extern s16 g_formation_item_action_option_states[3];
extern menu_entry_t g_formation_item_quantity_change_menu;
extern menu_entry_t g_formation_item_unequip_all_menu;
extern menu_instruction_t g_formation_item_hp_mp_bonus_window_script;
s8 formation_item_action_menu(void);
s8 formation_item_sort_menu(void);
menu_equipment_slot_e formation_item_select_slot(s16 unit_index, s32 item_id);
s32 formation_item_show_equipping_units(void);

/* categories */
extern s8 g_formation_item_category_menu_initialized;
/* The two item-category icon strips; the active one is kept in g_formation_item_category_icon_strip. */
extern menu_entry_t g_formation_item_category_icon_strip_a;
extern menu_entry_t g_formation_item_category_icon_strip_b;
extern menu_entry_t* g_formation_item_category_icon_strip;
extern s16 g_formation_item_category_strip_text_ids[]; /* option text ids of the active strip, -1 terminated */
extern menu_instruction_t g_formation_item_category_tabs_script;      /* menu display stream (narrow variant) */
extern menu_instruction_t g_formation_item_category_tabs_wide_script; /* menu display stream (wide variant) */
extern oriented_quad_t g_formation_item_category_tab_sprites[];       /* 20-byte quads, one per row */
extern u8 g_formation_item_category_tab_x[];                          /* per-row base x */
extern oriented_quad_t g_formation_item_category_tab_cursor_sprite;
extern u8 g_formation_item_category_input_lock;
extern s8 g_formation_item_category_previewed_cursor;
s32 equipment_run_item_list(void);

/* item list */
extern menu_instruction_t g_formation_item_list_window_script;
/* The formation item menu's copy of the EQUIP screen's item-list state. */
extern s8 g_formation_item_list_open;
extern s8 g_formation_item_list_refresh;
extern s8 g_formation_item_preview_active;
extern s16 g_formation_item_previewed_cursor;
extern s8 g_formation_item_slot;
s32 formation_item_build_list(s16 unit_id, s16 sort_mode, s8 category, s16* entries, u8 mode);
void item_sort_id_list(s32 mode, s16* list);

/* counts and prices */
/* The half price is clamped to at least one; stock to a byte. */
s32 shop_get_item_price(s16 item_id);
s32 shop_get_item_half_price(s16 item_id);
void shop_cash_out_excess_items(void);

#endif
