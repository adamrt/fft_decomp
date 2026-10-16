#ifndef WORLD_ITEM_H
#define WORLD_ITEM_H

/* WORLD's item lists, counts and prices. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/world.h"
#include "psx/types.h"

/* item */
extern oriented_quad_t g_world_item_category_tab_cursor_sprite;
/* Menu-subflow "running" flags. The readers sign-extend (lb), so these are
 * signed; the initializer only stores 0/1/2 and cannot distinguish. */
extern s8 g_world_item_menu_initialized;
extern s8 g_world_item_best_remove_initialized;
extern s8 g_world_item_submenu_initialized;
extern s8 g_world_item_category_menu_initialized;
extern s8 g_world_item_action_menu_initialized;
extern s8 g_world_item_sort_order_initialized;
extern s8 g_world_item_equipping_units_initialized;
extern menu_entry_t g_world_item_icon_menu;
extern u8 g_world_item_menu_unit_index;
extern u8 g_world_item_submenu_exit_pending;
extern world_menu_number_range_t g_world_item_action_quantity_range;
extern s16 g_world_item_action_option_states[3];
extern menu_entry_t g_world_item_quantity_change_menu;
extern menu_entry_t g_world_item_unequip_all_menu;
extern menu_instruction_t g_world_item_list_window_script;
extern menu_instruction_t g_world_item_hp_mp_bonus_window_script;
extern s8 g_world_item_menu_mode; /* menu_item_list_state_e */
/* The two item-category icon strips; the active one is kept in g_world_item_category_icon_strip. */
extern menu_entry_t g_world_item_category_icon_strip_a;
extern menu_entry_t g_world_item_category_icon_strip_b;
extern menu_instruction_t g_world_item_category_tabs_script;      /* menu display stream (narrow variant) */
extern menu_instruction_t g_world_item_category_tabs_wide_script; /* menu display stream (wide variant) */
extern oriented_quad_t g_world_item_category_tab_sprites[];       /* 20-byte quads, one per row */
extern u8 g_world_item_category_tab_x[];                          /* per-row base x */
extern u8 g_world_item_category_input_lock;
extern u8 g_world_item_menu_saved_browse_enabled;
extern s8 g_world_item_category_previewed_cursor;
extern s16 g_world_item_category_strip_text_ids[]; /* option text ids of the active strip, -1 terminated */
extern menu_entry_t* g_world_item_category_icon_strip;
extern u16 g_world_item_saved_selected_unit_index;
s32 world_item_build_category_list(s16 unit_id, s16 sort_mode, s8 category, s16* entries, u8 mode);
void world_item_cash_out_excess_inventory(void);
/* WORLD menu helpers. Half-price is clamped to one; stock to a byte. */
s32 world_item_get_half_price(s16 item_id);
s32 world_item_get_price(s16 item_id);
s8 world_item_run_action_menu(void);
s8 world_item_run_sort_order_menu(void);
menu_equipment_slot_e world_item_select_equipment_slot(s16 unit_index, s32 item_id);
s32 world_item_show_units_equipping_selected(void);

#endif
