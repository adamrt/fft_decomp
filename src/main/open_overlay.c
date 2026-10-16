/* Item order tables, card event init and the OPEN.BIN main loop and ending calls. */

#include "fft/data_item.h"
#include "fft/main_card.h"
#include "fft/main_file.h"
#include "fft/main_item.h"
#include "fft/open.h"
#include "psx/kernel.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Reset the seven item-type order lists to their default sequences (each ended by -1), empty the
 * weapon, helmet, armor, accessory and item page orders and zero the poached item counts. */
void item_init_order_tables(void) {
    g_item_type_order_tables.order_0[0] = 0;
    g_item_type_order_tables.order_0[1] = 1;
    g_item_type_order_tables.order_0[2] = 2;
    g_item_type_order_tables.order_0[3] = 3;
    g_item_type_order_tables.order_0[4] = 4;
    g_item_type_order_tables.order_0[5] = 5;
    g_item_type_order_tables.order_0[6] = 6;
    g_item_type_order_tables.order_0[7] = 7;
    g_item_type_order_tables.order_0[8] = 8;
    g_item_type_order_tables.order_0[9] = 9;
    g_item_type_order_tables.order_0[10] = 10;
    g_item_type_order_tables.order_0[11] = -1;
    g_item_type_order_tables.order_1[0] = 0;
    g_item_type_order_tables.order_1[1] = 1;
    g_item_type_order_tables.order_1[2] = 2;
    g_item_type_order_tables.order_1[3] = 4;
    g_item_type_order_tables.order_1[4] = 5;
    g_item_type_order_tables.order_1[5] = 7;
    g_item_type_order_tables.order_1[6] = 9;
    g_item_type_order_tables.order_1[7] = -1;
    g_item_type_order_tables.order_2[0] = 0;
    g_item_type_order_tables.order_2[1] = 1;
    g_item_type_order_tables.order_2[2] = 2;
    g_item_type_order_tables.order_2[3] = 3;
    g_item_type_order_tables.order_2[4] = 4;
    g_item_type_order_tables.order_2[5] = 5;
    g_item_type_order_tables.order_2[6] = -1;
    g_item_type_order_tables.order_3[0] = 0;
    g_item_type_order_tables.order_3[1] = 1;
    g_item_type_order_tables.order_3[2] = 2;
    g_item_type_order_tables.order_3[3] = 5;
    g_item_type_order_tables.order_3[4] = -1;
    g_item_type_order_tables.order_4[0] = 0;
    g_item_type_order_tables.order_4[1] = 1;
    g_item_type_order_tables.order_4[2] = 2;
    g_item_type_order_tables.order_4[3] = 5;
    g_item_type_order_tables.order_4[4] = -1;
    g_item_type_order_tables.order_5[0] = 0;
    g_item_type_order_tables.order_5[1] = 1;
    g_item_type_order_tables.order_5[2] = 2;
    g_item_type_order_tables.order_5[3] = 3;
    g_item_type_order_tables.order_5[4] = 4;
    g_item_type_order_tables.order_5[5] = 5;
    g_item_type_order_tables.order_5[6] = -1;
    g_item_type_order_tables.order_6[0] = 0;
    g_item_type_order_tables.order_6[1] = 1;
    g_item_type_order_tables.order_6[2] = 2;
    g_item_type_order_tables.order_6[3] = 5;
    g_item_type_order_tables.order_6[4] = -1;
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
    g_memcard_bios_events[MAIN_CARD_EVENT_BIOS_DONE] = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_memcard_bios_events[MAIN_CARD_EVENT_BIOS_ERROR] = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_memcard_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT] = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_memcard_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD] = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, 0);
    g_memcard_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE] = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_memcard_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR] = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_memcard_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT] = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_memcard_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD] = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, 0);
    InitCARD(1);
    StartCARD();
    ChangeClearPAD(0);
    _bu_init();
    _card_auto(0);
    for (i = 0; i < MAIN_CARD_EVENT_COUNT; i++) {
        EnableEvent(g_memcard_bios_events[i]);
    }
    ExitCriticalSection();
    g_memcard_bios_events_opened = 1;
}

/* Load OPEN.BIN into the low overlay area and run its main loop; mode 1 skips the opening movie.
 * Returns OPEN.BIN's result, 0 meaning go straight into BATTLE.BIN. */
int overlay_exec_open_title(int mode) {
    file_load_checked_to_address(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    return title_run_main_loop(mode);
}

/* Load OPEN.BIN into the low overlay area and play its ending cutscene. */
void overlay_exec_open_ending(void) {
    file_load_checked_to_address(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_heap_low_overlay_load_address);
    ending_run();
}
