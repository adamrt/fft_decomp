#ifndef FORMATION_ITEM_H
#define FORMATION_ITEM_H

/* Declarations. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/screen.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Command texts of the formation's Item and Ability menus, chosen by the
 * selected unit's formation_unit_lock_e. */
enum {
    FORMATION_ITEM_MENU_TEXT_UNLOCKED = 0xd801,
    FORMATION_ITEM_MENU_TEXT_LOCKED = 0xd802,
    FORMATION_ABILITY_MENU_TEXT_UNLOCKED = 0xd80b,
    FORMATION_ABILITY_MENU_TEXT_LOCKED = 0xd80c,
    FORMATION_ABILITY_MENU_TEXT_MIME = 0xd80d,
};

s32 formation_item_can_equip_in_slot(s16 unit_index, s16 slot, s16 item_id);
s32 formation_item_equip(s16 unit, s16 slot, s16 item_id);
extern menu_entry_t g_formation_item_best_fit_confirm_menu;
void formation_item_build_best_fit(s16 unit_index, s16* equipment);
extern s8 g_formation_item_equip_menu_initialized;
extern u16 g_formation_item_equip_item_cursor_anim;
extern point16_t g_formation_item_equip_item_cursor_point;
extern point16_t g_formation_item_remove_all_cursor_point;
extern volatile u16 g_formation_item_remove_all_blink_counter;
extern s8 g_formation_item_remove_unit_index;
extern u16 g_formation_item_remove_cursor_anim;
extern point16_t g_formation_item_remove_cursor_point;
extern s16 g_formation_item_remove_saved_primary_input;
extern s16 g_formation_item_remove_saved_secondary_input;
extern u16 g_formation_item_remove_saved_newly_pressed;
void formation_item_menu(void);
s32 formation_item_get_hp_bonus(s32 unused_row);
s32 formation_item_get_mp_bonus(s32 unused_row);

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
extern menu_instruction_t g_formation_item_list_window_script;

/* The formation item menu's copy of the EQUIP screen's item-list state. */
extern s8 g_formation_item_list_open;

extern s8 g_formation_item_list_refresh;
extern s8 g_formation_item_preview_active;
extern s16 g_formation_item_previewed_cursor;
extern s8 g_formation_item_slot;
s32 formation_item_build_list(s16 unit_id, s16 sort_mode, s8 category, s16* entries, u8 mode);

#endif
