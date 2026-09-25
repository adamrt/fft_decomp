#include "fft/main.h"
#include "psx/types.h"

/* The load-game overlay restores only 12/8/7/5/5/7/5 bytes of the seven order
 * lists respectively. The weapon item-ID order starts at 0x80057b5c, outside
 * this 0x3c-byte aggregate. */

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
    memset(table, 0, 0x100);
}
