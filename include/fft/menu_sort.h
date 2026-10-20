#ifndef MENU_SORT_H
#define MENU_SORT_H

/* Saved sort-key orders for Unit List, Formation and item lists. */

#include "psx/types.h"

/* saved menu sort orders */
/* Independent sort-key lists in their resident storage layout. Unit List and
 * Formation have their own key domains; the five item categories use item_sort_key_e.
 * Each list ends in -1. The item rows save only their live lengths. */
typedef struct menu_sort_orders {
    s8 unit_list[12];
    s8 formation[8];
    s8 weapons_and_shields[8];
    s8 headgear[8];
    s8 body_armor[8];
    s8 accessories[8];
    s8 other_items[8];
} menu_sort_orders_t;

extern menu_sort_orders_t g_menu_sort_orders;

/* Restore default orders, empty item-page lists and clear poached-item counts. */
void menu_sort_tables_init(void);

#endif
