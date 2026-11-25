/* The shared menus' default sort keys and empty item-page orders, the BIOS memory-card events that
 * OPEN, WORLD and CARD poll (MAIN keeps the handles), and the resident loaders for the title sequence
 * and ending in OPEN.BIN. */

#include "fft/front/ending.h"
#include "fft/front/title.h"
#include "fft/game/item.h"
#include "fft/menu/menu_sort.h"
#include "fft/party/formation.h"
#include "fft/party/item_list.h"
#include "fft/party/unit_list.h"
#include "fft/sys/file.h"
#include "fft/sys/memcard.h"
#include "fft/sys/memory.h"
#include "fft/sys/overlay.h"
#include "psx/kernel.h"
#include "psx/libapi.h"
#include "psx/libc.h"

/* menu sort defaults */

/* Reset the Unit List, Formation and five item sort orders to their default -1-terminated
 * sequences, empty the weapon, helmet, armor, accessory and item page orders, and zero the
 * poached item counts. */
void menu_init_sort_tables(void) {
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

/* memory card events */

extern u8 g_memcard_bios_events_opened;

/* Opens and enables the eight memory-card BIOS events (SwCARD then HwCARD,
 * each with IOE, ERROR, TIMOUT and NEW) once. */
void memcard_init_events(void) {
    s32 i;

    if (g_memcard_bios_events_opened != 0) {
        return;
    }
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

/* title and ending */

/* Load OPEN.BIN into the low overlay area and run its main loop; nonzero skip_movie skips the opening movie.
 * Returns OPEN.BIN's result, 0 meaning go straight into BATTLE.BIN. */
title_result_e overlay_run_title(s32 skip_movie) {
    file_load_into_buffer_checked(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    return open_title_entrypoint(skip_movie);
}

/* Load OPEN.BIN into the low overlay area and play its ending cutscene. */
void overlay_run_ending(void) {
    file_load_into_buffer_checked(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    open_ending_entrypoint();
}
