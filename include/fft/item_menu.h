#ifndef ITEM_MENU_H
#define ITEM_MENU_H

/* Item menus: category lists, sort choices, stat previews, icons and rearranging. */

#include "psx/types.h"

struct menu_entry;
struct menu_icon_source;
struct unit_panel_record;
struct unit_panel_summary;

/* item entry encoding */
/* formation_unit_t.equipment 16-bit values carry the item id in the low ten bits and flags above (bit 14 is tested
 * by fitting_purchase_commit and the job wheel); the WORLD item tables are indexed by the masked id
 * (shop_item_price_get, item_type_get). */
enum {
    ITEM_ENTRY_ID_MASK = 0x3ff,
    ITEM_ENTRY_UNEQUIPPABLE = 0x4000, /* item list entries: listed, but the unit cannot equip the item */
    ITEM_LIST_END = 0xff,             /* the item id that ends an item list */
    SHOP_FITTING_ITEM = 0x8000,       /* equipment 16-bit values: an unpaid shop item tried on in the fitting room */
    /* The equipment values of a WORLD stat detail: the top two bits pick the name's text palette, and
     * HIGHLIGHT, the same bit as SHOP_FITTING_ITEM, draws it in the highlight palette. */
    FORMATION_PALETTE_MASK = 0xc000,
    FORMATION_HIGHLIGHT = 0x8000
};

/* item lists */
/* Broad item categories shared by the WORLD and EQUIP menu classifiers.
 * Throwable weapons and consumables both use OTHER because neither maps to a
 * normal equipment slot category. */
typedef enum item_menu_category {
    ITEM_MENU_CATEGORY_WEAPON = 0,
    ITEM_MENU_CATEGORY_SHIELD = 1,
    ITEM_MENU_CATEGORY_HEADGEAR = 2,
    ITEM_MENU_CATEGORY_BODY_ARMOR = 3,
    ITEM_MENU_CATEGORY_ACCESSORY = 4,
    ITEM_MENU_CATEGORY_OTHER = 5
} item_menu_category_e;

/* The category argument of formation_item_list_build,
 * shop_stock_list_build and equipment_build_item_list. 0 to 4
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
    ITEM_LIST_FILTER_OTHER = 7
} item_list_filter_e;

/* The item menus' tabs (g_menu_selected_slot there, g_shop_item_category): tabs 0 to 3 (hand, head,
 * body, accessory) list filter tab + 1, and this last tab lists ITEM_LIST_FILTER_OTHER. */
enum { ITEM_MENU_TAB_OTHER = 4 };

/* The last argument of the same functions: the equip check against the unit. */
typedef enum item_list_mode {
    ITEM_LIST_MODE_ALL = 0,                    /* no check */
    ITEM_LIST_MODE_MARK_UNEQUIPPABLE = 1,      /* 0x4000 on entries the unit cannot equip; taken as
                                                  EQUIPPABLE unless Show Unequippable Items is on */
    ITEM_LIST_MODE_EQUIPPABLE = 2,             /* entries the unit cannot equip are left out */
    ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY = 3 /* formation_item_list_build only: as
                                                   EQUIPPABLE, counting no equipped copies */
} item_list_mode_e;

/* State of the item list screen (g_formation_item_menu_mode in the formation
 * screen, g_equipment_substate in EQUIP). Each state's handler returns the
 * next state. */
typedef enum menu_item_list_state {
    MENU_ITEM_LIST_STATE_CLOSE = -1,
    MENU_ITEM_LIST_STATE_BROWSE = 0,
    MENU_ITEM_LIST_STATE_REARRANGE = 1,
    MENU_ITEM_LIST_STATE_ACTION = 2, /* action strip for the selected item */
    MENU_ITEM_LIST_STATE_SORT_ORDER = 5,
    MENU_ITEM_LIST_STATE_EQUIPPED_BY = 6 /* WORLD only: the units equipping the selected item */
} menu_item_list_state_e;

extern s16 g_item_list_entries[];
extern u8 g_item_weapon_page_order[0x8a];
extern u8 g_item_helmet_page_order[0x1d];
extern u8 g_item_armor_page_order[0x25];
extern u8 g_item_accessory_page_order[0x21];
extern u8 g_item_page_order[0x15];

/* The equipment slot or list category the item screens show (menu_equipment_slot_e). */
extern u8 g_menu_selected_slot;

/* Throwable weapons and consumables share the fallback category. */
item_menu_category_e item_menu_category_get(s16 item_id);

/* Reconciles a category list with what the party owns: newly owned items go first. */
void item_rebuild_category_list(s32 category);
s32 item_sort_list_by_category_order(s32 category_index, s16* list);

/* Packs a -1 terminated s16 item list into the category's byte list. */
void item_store_category_list(s32 category_index, const s16* source);

/* sort orders */
/* A key in the list that item_id_list_sort (EQUIP and WORLD each build their own body) sorts by
 * (g_item_type_order_lists); -1 ends the list. TYPE sorts ascending, the others descending. */
typedef enum item_sort_key {
    ITEM_SORT_KEY_TYPE = 0,
    ITEM_SORT_KEY_PRICE = 1,
    ITEM_SORT_KEY_RANKING = 2, /* item_ranking_value_get */
    ITEM_SORT_KEY_PHYSICAL_EVADE = 3,
    ITEM_SORT_KEY_MAGIC_EVADE = 4,
    ITEM_SORT_KEY_HELD_COUNT = 5 /* inventory plus equipped copies */
} item_sort_key_e;

/* Indices of g_item_type_order_lists, distinct from the keys within each list.
 * WORLD alone has the fixed stock and best-fit orders; slot 0 is its Formation
 * order and is null in EQUIP. */
typedef enum item_sort_order {
    ITEM_SORT_ORDER_WEAPONS_AND_SHIELDS = 1,
    ITEM_SORT_ORDER_HEADGEAR = 2,
    ITEM_SORT_ORDER_BODY_ARMOR = 3,
    ITEM_SORT_ORDER_ACCESSORIES = 4,
    ITEM_SORT_ORDER_OTHER_ITEMS = 5,
    ITEM_SORT_ORDER_STOCK = 6,   /* type, price, ranking */
    ITEM_SORT_ORDER_BEST_FIT = 7 /* ranking, type, price */
} item_sort_order_e;

/* Packed sort-key orders ending in -1. WORLD slot 0 points to Formation's
 * formation order (formation_sort_key_e); EQUIP slot 0 is null. Slots 1..5
 * in both programs point to the five item-category orders (item_sort_key_e).
 * WORLD adds the fixed stock and best-fit orders at slots 6 and 7. The directory
 * is fixed; item_store_type_order edits the lists it selects. */
extern s8* const g_item_type_order_lists[];

s32 item_ranking_value_get(s32 item_id);
void item_id_list_sort(s32 mode, s16* list);

/* Store and load a category's sort key order as -1 terminated halfwords. */
void item_store_type_order(s32 list_index, const s16* source);
void item_load_type_order(s32 list_index, s16* destination);

/* stat previews */
extern struct unit_panel_record g_item_preview_stat_detail;

/* EQUIP and WORLD each build their own body. */
void item_stat_preview_build(
    s16 raw_item, struct unit_panel_summary* summary, struct unit_panel_record* detail, s32 alternate);

/* icons and the item panel */
/* Item and item-type icons as menu_icon_source_t, for the icon row callbacks; item_type_icon_build gives the
 * texture cell of an item type's 12x12 icon. */
void item_type_icon_build(s32 item_type, struct menu_icon_source* output);
void item_icon_source_build(struct menu_icon_source* output, u32 item_id);
struct menu_icon_source* item_build_type_icon_source(s32 index);

/* A weapon's hand marks on the item panel: two-handed, or the Two Hands and Two Swords marks it allows. */
void item_draw_weapon_hand_icons(s16 weapon_id);

/* The item detail panel's status column. */
void item_draw_status_list(s16 item_id);

/* item action menu */
extern struct menu_entry g_item_action_menu;

/* rearranging */
/* The item screens' rearrange mode: pick an item up and move it within its category list. */
extern s8 g_item_rearrange_initialized;

s32 item_rearrange_update(void);

#endif
