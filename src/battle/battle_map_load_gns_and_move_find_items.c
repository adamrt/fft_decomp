#include "fft/battle.h"

enum {
    GNS_INITIAL_LOAD_BYTES = 0x1000,
    GNS_BUFFER_CLEAR_BYTES = 0xbb8,
};

gns_file_record_t* battle_map_load_gns_and_move_find_items(s32 map_id) {
    u32 selected_map;
    gns_file_record_t* gns_records;
    u32 lba;

    selected_map = map_id & 0xffff;
    battle_map_load_move_find_item_data(&g_battle_map_move_find_item_data[selected_map]);

    gns_records = g_battle_map_gns_records;
    bzero(gns_records, GNS_BUFFER_CLEAR_BYTES);

    lba = g_battle_map_gns_lba[selected_map];
    if (lba != 0) {
        if (main_file_call_build_header(lba, GNS_INITIAL_LOAD_BYTES, g_battle_map_data_load_buffer) != 0) {
            return 0;
        }
        return gns_records;
    }
    /* The target falls through with the BIOS bzero result still in v0. */
}
