/* Unit and item sort orders, card event init and the OPEN.BIN main loop and ending calls. */

#include "fft/ending.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/item.h"
#include "fft/memcard.h"
#include "fft/memory.h"
#include "fft/overlay.h"
#include "fft/title.h"
#include "fft/unit_list.h"
#include "psx/kernel.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/types.h"

extern u8 g_memcard_bios_events_opened;

/* Reset the Unit List, Formation and five item sort orders to their default -1-terminated
 * sequences, empty the weapon, helmet, armor, accessory and item page orders, and zero the
 * poached item counts. */
void item_init_order_tables(void) {
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

/* Opens and enables the eight memory-card BIOS events (SwCARD then HwCARD,
 * each with IOE, ERROR, TIMOUT and NEW) once. */
void memcard_init_events(void) {
    s32 i;

    if (g_memcard_bios_events_opened != 0) {
        return;
    }
    i = 0;
    EnterCriticalSection();
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_DONE] = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_ERROR] = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_TIMEOUT] = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_NEW_CARD] = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_DONE] = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_ERROR] = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_TIMEOUT] = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_NEW_CARD] = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, 0);
    InitCARD(1);
    StartCARD();
    ChangeClearPAD(0);
    _bu_init();
    _card_auto(0);
    for (i = 0; i < MEMCARD_EVENT_COUNT; i++) {
        EnableEvent(g_memcard_bios_events[i]);
    }
    ExitCriticalSection();
    g_memcard_bios_events_opened = 1;
}

/* Load OPEN.BIN into the low overlay area and run its main loop; mode 1 skips the opening movie.
 * Returns OPEN.BIN's result, 0 meaning go straight into BATTLE.BIN. */
int overlay_exec_open_title(int mode) {
    file_load_checked_to_address(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    return open_title_entrypoint(mode);
}

/* Load OPEN.BIN into the low overlay area and play its ending cutscene. */
void overlay_exec_open_ending(void) {
    file_load_checked_to_address(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    open_ending_entrypoint();
}
