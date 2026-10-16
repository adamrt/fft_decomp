#ifndef ITEM_H
#define ITEM_H

/* Items and equipment shared by EQUIP and WORLD: item lists and their sort orders, inventory counts, stat
 * previews, item icons and the item list screens. */

#include "fft/data_item.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_status.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* item lists */
/* The category argument of formation_item_build_list,
 * shop_build_stock_list and equip_item_build_filtered_list. 0 to 4
 * select what fits that equipment slot. The item menus pass their tab + 1 for
 * the first four tabs and OTHER for the fifth; the best-fit builders pass
 * WEAPON or SHIELD for the left hand. */
typedef enum item_list_filter {
    ITEM_LIST_FILTER_HAND = 0, /* weapons and shields; 1, the left hand, selects the same */
    ITEM_LIST_FILTER_HEADGEAR = 2,
    ITEM_LIST_FILTER_BODY_ARMOR = 3,
    ITEM_LIST_FILTER_ACCESSORY = 4,
    ITEM_LIST_FILTER_WEAPON = 5,
    ITEM_LIST_FILTER_SHIELD = 6,
    ITEM_LIST_FILTER_OTHER = 7,
} item_list_filter_e;

/* The item menus' tabs (g_menu_selected_slot there, g_shop_item_category): tabs 0 to 3 (hand, head,
 * body, accessory) list filter tab + 1, and this last tab lists ITEM_LIST_FILTER_OTHER. */
enum { ITEM_MENU_TAB_OTHER = 4 };

/* The last argument of the same functions: the equip check against the unit. */
typedef enum item_list_mode {
    ITEM_LIST_MODE_ALL = 0,                     /* no check */
    ITEM_LIST_MODE_MARK_UNEQUIPPABLE = 1,       /* 0x4000 on entries the unit cannot equip; taken as
                                                   EQUIPPABLE unless Show Unequippable Items is on */
    ITEM_LIST_MODE_EQUIPPABLE = 2,              /* entries the unit cannot equip are left out */
    ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY = 3, /* formation_item_build_list only: as
                                                   EQUIPPABLE, counting no equipped copies */
} item_list_mode_e;

/* A key in the list equip_item_sort_id_list and item_sort_id_list
 * sort by (g_item_type_order_lists); -1 ends
 * the list. TYPE sorts ascending, the others descending. */
typedef enum item_sort_key {
    ITEM_SORT_KEY_TYPE = 0,
    ITEM_SORT_KEY_PRICE = 1,
    ITEM_SORT_KEY_RANKING = 2, /* item_get_ranking_value */
    ITEM_SORT_KEY_PHYSICAL_EVADE = 3,
    ITEM_SORT_KEY_MAGIC_EVADE = 4,
    ITEM_SORT_KEY_HELD_COUNT = 5, /* inventory plus equipped copies */
} item_sort_key_e;

/* State of the item list screen (g_formation_item_menu_mode in the formation
 * screen, g_equip_menu_substate in EQUIP). Each state's handler returns the
 * next state. */
typedef enum menu_item_list_state {
    MENU_ITEM_LIST_STATE_CLOSE = -1,
    MENU_ITEM_LIST_STATE_BROWSE = 0,
    MENU_ITEM_LIST_STATE_REARRANGE = 1,
    MENU_ITEM_LIST_STATE_ACTION = 2, /* action strip for the selected item */
    MENU_ITEM_LIST_STATE_SORT_ORDER = 5,
    MENU_ITEM_LIST_STATE_EQUIPPED_BY = 6, /* WORLD only: the units equipping the selected item */
} menu_item_list_state_e;

extern s16 g_item_list_entries[];
/* The owned items of each list category (0 weapons and shields, then headgear, body armor, accessories and
 * other: the menu category minus one), byte lists ending in a terminator. */
extern u8* g_item_category_lists[];
/* Each category's sort key order (item_sort_key_e), packed bytes ending in -1. */
extern s8* g_item_type_order_lists[];
/* The equipment slot or list category the item screens show (menu_equipment_slot_e). */
extern u8 g_menu_selected_slot;

/* Throwable weapons and consumables share the fallback category. */
item_menu_category_e item_get_menu_category(s16 item_id);
s32 item_get_type(s32 item_id);
s32 item_get_ranking_value(s32 item_id);
/* Reconciles a category list with what the party owns: newly owned items go first. */
void item_rebuild_category_list(s32 category);
s32 item_is_in_category_list(s32 item_id, s32 category_index);
void item_prepend_to_category_list(s32 item_id, s32 category_index);
void item_remove_from_category_list(s32 item_id, s32 category_index);
s32 item_sort_list_by_category_order(s32 category_index, s16* list);
/* Packs a -1 terminated s16 item list into the category's byte list. */
void item_store_category_list(s32 category_index, const s16* source);
/* Store and load a category's sort key order as -1 terminated halfwords. */
void menu_store_sort_key_list(s32 list_index, const s16* source);
void menu_load_sort_key_list(s32 list_index, s16* destination);

/* inventory counts */
/* EQUIP and WORLD each build their own bodies of the first two. */
s32 item_get_total_count(s16 item_id);
s32 item_get_equipped_count(s32 item_entry);
s32 item_adjust_inventory_count(s16 item_entry, s32 delta);

/* stat previews */
extern struct menu_status_panel_slot_storage g_item_preview_stat_detail;

/* Sums the stat bonuses of the five equipped items. */
void item_sum_equipment_stat_details(menu_status_panel_slot_storage_t* total, const u16* equipment);
/* EQUIP and WORLD each build their own body. */
void item_collect_stat_deltas(
    s16 raw_item, menu_unit_status_record_t* summary, menu_status_panel_slot_storage_t* detail, s32 alternate);
void item_calculate_equipment_swap_stat_delta(menu_status_panel_slot_storage_t* detail_total,
    menu_unit_status_record_t* summary_total, const u16* from_items, const u16* to_items);

/* item icons */
/* Item and item-type icons as menu_icon_source_t, for the icon row callbacks. */
extern uv8_t g_item_type_icon_origins[];

/* The texture cell of an item type's 12x12 icon. */
void item_get_type_icon_rect(s32 item_type, menu_icon_source_t* output);
void menu_build_item_icon_source(menu_icon_source_t* output, u32 item_id);
menu_icon_source_t* menu_build_item_type_icon_source(s32 index);
/* A weapon's hand marks on the item panel: two-handed, or the Two Hands and Two Swords marks it allows. */
void menu_draw_weapon_hand_icons(s16 weapon_id);

/* equipment rows */
/* Row callbacks of the item lists, and the item detail panel's status column. */
extern RECT g_menu_status_display_image_rects[];

menu_icon_source_t* menu_get_entry_item_icon_source(s32 row);
s32 menu_get_entry_item_count(s32 row);
s32 menu_get_entry_item_equipped_count(s32 row);
void menu_draw_item_status_list(s16 item_id);

/* rearranging */
/* The item screens' rearrange mode: pick an item up and move it within its category list. */
extern menu_entry_t g_item_action_menu;
extern s8 g_item_rearrange_initialized;
extern s16 g_item_rearrange_picked_cursor;
extern s16 g_item_rearrange_preview_cursor;
extern u16 g_item_rearrange_picked_scroll;
extern u8 g_item_rearrange_anim_frame;

s32 item_run_rearrange_mode(void);

#endif
