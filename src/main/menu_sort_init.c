/* Default sort keys and empty item-page orders used by the shared menus. */

#include "fft/formation.h"
#include "fft/item.h"
#include "fft/unit_list.h"
#include "psx/libc.h"

/* Reset the Unit List, Formation and five item sort orders to their default -1-terminated
 * sequences, empty the weapon, helmet, armor, accessory and item page orders, and zero the
 * poached item counts. */
void item_order_tables_init(void) {
    g_menu_sort_orders.unit_list[0] = UNIT_LIST_SORT_KEY_NUMBER;
    g_menu_sort_orders.unit_list[1] = UNIT_LIST_SORT_KEY_LEVEL;
    g_menu_sort_orders.unit_list[2] = UNIT_LIST_SORT_KEY_JOB;
    g_menu_sort_orders.unit_list[3] = UNIT_LIST_SORT_KEY_AT_LIST;
    g_menu_sort_orders.unit_list[4] = UNIT_LIST_SORT_KEY_BRAVE;
    g_menu_sort_orders.unit_list[5] = UNIT_LIST_SORT_KEY_FAITH;
    g_menu_sort_orders.unit_list[6] = UNIT_LIST_SORT_KEY_HP;
    g_menu_sort_orders.unit_list[7] = UNIT_LIST_SORT_KEY_MAX_HP;
    g_menu_sort_orders.unit_list[8] = UNIT_LIST_SORT_KEY_MP;
    g_menu_sort_orders.unit_list[9] = UNIT_LIST_SORT_KEY_MAX_MP;
    g_menu_sort_orders.unit_list[10] = UNIT_LIST_SORT_KEY_CT;
    g_menu_sort_orders.unit_list[UNIT_LIST_SORT_KEY_COUNT] = -1;
    g_menu_sort_orders.formation[0] = FORMATION_SORT_KEY_NUMBER;
    g_menu_sort_orders.formation[1] = FORMATION_SORT_KEY_LEVEL;
    g_menu_sort_orders.formation[2] = FORMATION_SORT_KEY_JOB;
    g_menu_sort_orders.formation[3] = FORMATION_SORT_KEY_BRAVE;
    g_menu_sort_orders.formation[4] = FORMATION_SORT_KEY_FAITH;
    g_menu_sort_orders.formation[5] = FORMATION_SORT_KEY_MAX_HP;
    g_menu_sort_orders.formation[6] = FORMATION_SORT_KEY_MAX_MP;
    g_menu_sort_orders.formation[7] = -1;
    g_menu_sort_orders.weapons_and_shields[0] = ITEM_SORT_KEY_TYPE;
    g_menu_sort_orders.weapons_and_shields[1] = ITEM_SORT_KEY_PRICE;
    g_menu_sort_orders.weapons_and_shields[2] = ITEM_SORT_KEY_RANKING;
    g_menu_sort_orders.weapons_and_shields[3] = ITEM_SORT_KEY_PHYSICAL_EVADE;
    g_menu_sort_orders.weapons_and_shields[4] = ITEM_SORT_KEY_MAGIC_EVADE;
    g_menu_sort_orders.weapons_and_shields[5] = ITEM_SORT_KEY_HELD_COUNT;
    g_menu_sort_orders.weapons_and_shields[6] = -1;
    g_menu_sort_orders.headgear[0] = ITEM_SORT_KEY_TYPE;
    g_menu_sort_orders.headgear[1] = ITEM_SORT_KEY_PRICE;
    g_menu_sort_orders.headgear[2] = ITEM_SORT_KEY_RANKING;
    g_menu_sort_orders.headgear[3] = ITEM_SORT_KEY_HELD_COUNT;
    g_menu_sort_orders.headgear[4] = -1;
    g_menu_sort_orders.body_armor[0] = ITEM_SORT_KEY_TYPE;
    g_menu_sort_orders.body_armor[1] = ITEM_SORT_KEY_PRICE;
    g_menu_sort_orders.body_armor[2] = ITEM_SORT_KEY_RANKING;
    g_menu_sort_orders.body_armor[3] = ITEM_SORT_KEY_HELD_COUNT;
    g_menu_sort_orders.body_armor[4] = -1;
    g_menu_sort_orders.accessories[0] = ITEM_SORT_KEY_TYPE;
    g_menu_sort_orders.accessories[1] = ITEM_SORT_KEY_PRICE;
    g_menu_sort_orders.accessories[2] = ITEM_SORT_KEY_RANKING;
    g_menu_sort_orders.accessories[3] = ITEM_SORT_KEY_PHYSICAL_EVADE;
    g_menu_sort_orders.accessories[4] = ITEM_SORT_KEY_MAGIC_EVADE;
    g_menu_sort_orders.accessories[5] = ITEM_SORT_KEY_HELD_COUNT;
    g_menu_sort_orders.accessories[6] = -1;
    g_menu_sort_orders.other_items[0] = ITEM_SORT_KEY_TYPE;
    g_menu_sort_orders.other_items[1] = ITEM_SORT_KEY_PRICE;
    g_menu_sort_orders.other_items[2] = ITEM_SORT_KEY_RANKING;
    g_menu_sort_orders.other_items[3] = ITEM_SORT_KEY_HELD_COUNT;
    g_menu_sort_orders.other_items[4] = -1;
    g_item_weapon_page_order[0] = ITEM_ID_NONE;
    g_item_helmet_page_order[0] = ITEM_ID_NONE;
    g_item_armor_page_order[0] = ITEM_ID_NONE;
    g_item_accessory_page_order[0] = ITEM_ID_NONE;
    g_item_page_order[0] = ITEM_ID_NONE;
    memset(g_item_poached_quantities, 0, sizeof(g_item_poached_quantities));
}
