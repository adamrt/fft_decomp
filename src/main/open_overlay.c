/* Item order tables, card event init and the OPEN.BIN main loop and ending calls. */

#include "fft/main.h"
#include "fft/open.h"
#include "psx/types.h"

void main_item_init_order_tables(void) {
    g_main_item_type_order_tables.order_0[0] = 0;
    g_main_item_type_order_tables.order_0[1] = 1;
    g_main_item_type_order_tables.order_0[2] = 2;
    g_main_item_type_order_tables.order_0[3] = 3;
    g_main_item_type_order_tables.order_0[4] = 4;
    g_main_item_type_order_tables.order_0[5] = 5;
    g_main_item_type_order_tables.order_0[6] = 6;
    g_main_item_type_order_tables.order_0[7] = 7;
    g_main_item_type_order_tables.order_0[8] = 8;
    g_main_item_type_order_tables.order_0[9] = 9;
    g_main_item_type_order_tables.order_0[10] = 10;
    g_main_item_type_order_tables.order_0[11] = -1;
    g_main_item_type_order_tables.order_1[0] = 0;
    g_main_item_type_order_tables.order_1[1] = 1;
    g_main_item_type_order_tables.order_1[2] = 2;
    g_main_item_type_order_tables.order_1[3] = 4;
    g_main_item_type_order_tables.order_1[4] = 5;
    g_main_item_type_order_tables.order_1[5] = 7;
    g_main_item_type_order_tables.order_1[6] = 9;
    g_main_item_type_order_tables.order_1[7] = -1;
    g_main_item_type_order_tables.order_2[0] = 0;
    g_main_item_type_order_tables.order_2[1] = 1;
    g_main_item_type_order_tables.order_2[2] = 2;
    g_main_item_type_order_tables.order_2[3] = 3;
    g_main_item_type_order_tables.order_2[4] = 4;
    g_main_item_type_order_tables.order_2[5] = 5;
    g_main_item_type_order_tables.order_2[6] = -1;
    g_main_item_type_order_tables.order_3[0] = 0;
    g_main_item_type_order_tables.order_3[1] = 1;
    g_main_item_type_order_tables.order_3[2] = 2;
    g_main_item_type_order_tables.order_3[3] = 5;
    g_main_item_type_order_tables.order_3[4] = -1;
    g_main_item_type_order_tables.order_4[0] = 0;
    g_main_item_type_order_tables.order_4[1] = 1;
    g_main_item_type_order_tables.order_4[2] = 2;
    g_main_item_type_order_tables.order_4[3] = 5;
    g_main_item_type_order_tables.order_4[4] = -1;
    g_main_item_type_order_tables.order_5[0] = 0;
    g_main_item_type_order_tables.order_5[1] = 1;
    g_main_item_type_order_tables.order_5[2] = 2;
    g_main_item_type_order_tables.order_5[3] = 3;
    g_main_item_type_order_tables.order_5[4] = 4;
    g_main_item_type_order_tables.order_5[5] = 5;
    g_main_item_type_order_tables.order_5[6] = -1;
    g_main_item_type_order_tables.order_6[0] = 0;
    g_main_item_type_order_tables.order_6[1] = 1;
    g_main_item_type_order_tables.order_6[2] = 2;
    g_main_item_type_order_tables.order_6[3] = 5;
    g_main_item_type_order_tables.order_6[4] = -1;
    g_main_weapon_page_order[0] = ITEM_ID_NONE;
    g_main_helmet_page_order[0] = ITEM_ID_NONE;
    g_main_armor_page_order[0] = ITEM_ID_NONE;
    g_main_accessory_page_order[0] = ITEM_ID_NONE;
    g_main_item_page_order[0] = ITEM_ID_NONE;
    memset(g_main_item_poached_quantities, 0, sizeof(g_main_item_poached_quantities));
}

/* Opens and enables the eight memory-card BIOS events (SwCARD then HwCARD,
 * each with IOE, ERROR, TIMOUT and NEW) once. */
void main_card_init_events(void) {
    s32 i;

    if (g_main_card_bios_events_opened != 0) {
        return;
    }
    i = 0;
    EnterCriticalSection();
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE] = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR] = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT] = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD] = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE] = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR] = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT] = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD] = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, 0);
    InitCARD(1);
    StartCARD();
    ChangeClearPAD(0);
    _bu_init();
    _card_auto(0);
    for (i = 0; i < MAIN_CARD_EVENT_COUNT; i++) {
        EnableEvent(g_main_card_bios_events[i]);
    }
    ExitCriticalSection();
    g_main_card_bios_events_opened = 1;
}

int main_overlay_exec_open_bin_main_loop(int mode) {
    main_file_load_checked_to_address(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_main_heap_low_overlay_load_address);
    return open_system_run_main_loop(mode);
}

void main_overlay_exec_open_bin_ending(void) {
    main_file_load_checked_to_address(DISC_SECTOR_OPEN_OPEN_BIN, 0x36800, g_main_heap_low_overlay_load_address);
    open_system_run_ending_cutscene();
}
