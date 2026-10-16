/* GNS command application, GNS and move-find item loading, and file load starts. */

#include "fft/battle_map.h"
#include "fft/main_file.h"
#include "fft/script.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Apply one command record that follows the resource rows of a map's GNS file and
 * step g_battle_map_gns_record_cursor past it.
 *
 * The indoor, outdoor, snow, 0x8a and weather-modifier commands always run. Any
 * other command runs only when the requested map state compares with the record's
 * map state as its comparison asks. The requested state is the low 12 bits of the
 * script variable the record names (the layout) packed with the script's weather
 * and time of day. Only the low byte of the record's variable id and of its map
 * state is read.
 *
 * Rows are applied in file order, so the last row of a kind wins; type 0x87 has
 * no dispatcher case.
 *
 * Only phase 2 does anything, and it is the only phase the map loader passes.
 * Always returns 0. */
s32 battle_map_apply_gns_command(s32 phase, map_resource_t* record) {
    char unused_zeroes[8] = { 0 };
    s32 unused_sizes[4] = { 0x1000, 0x1000, 0x1000, 0 };
    s32 requested_layout;
    s32 requested_layout_and_weather;
    map_resource_t* selected;

    if (phase == 2) {
        /* Everything below works on this copy; the parameter itself dies
         * here, which keeps it out of a callee-saved register. */
        selected = record;
        if (selected->command.resource_type == GNS_COMMAND_SET_INDOOR) {
            battle_map_dispatch_gns_resource(GNS_COMMAND_SET_INDOOR, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_OUTDOOR) {
            battle_map_dispatch_gns_resource(GNS_COMMAND_SET_OUTDOOR, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_CLEAR_SNOW) {
            battle_map_dispatch_gns_resource(GNS_COMMAND_CLEAR_SNOW, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_SNOW) {
            battle_map_dispatch_gns_resource(GNS_COMMAND_SET_SNOW, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_UNHANDLED_8A) {
            battle_map_dispatch_gns_resource(GNS_COMMAND_UNHANDLED_8A, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_WEATHER_MODIFIER) {
            battle_map_dispatch_gns_resource(GNS_COMMAND_SET_WEATHER_MODIFIER, selected);
        } else {
            /*
             * Requested map state, packed like the file row's map_state: layout in
             * bits 0-11, weather in 12-14, time of day in 15.  The target reads
             * the record's variable id and map state as single bytes (lbu), not
             * as the 16-bit values the struct declares, so those two stay byte reads.
             */
            requested_layout = script_get_variable(*(u8*)&selected->command.script_variable_id) & 0xfff;
            requested_layout_and_weather
                = requested_layout | ((script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12);
            if (battle_map_is_gns_record_matching_state(selected->command.map_state_comparison,
                    *(u8*)&selected->command.map_state,
                    requested_layout_and_weather | ((script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15))) {
                battle_map_dispatch_gns_resource(selected->command.resource_type, selected);
            }
        }
        g_battle_map_gns_record_cursor += selected->command.record_byte_length;
    }
    return 0;
}

enum {
    GNS_INITIAL_LOAD_BYTES = 0x1000,
    GNS_BUFFER_CLEAR_BYTES = 0xbb8,
};

/* Make a map current for loading: copy its move-find item record, clear the
 * 150-row GNS table and start reading the first 0x1000 bytes of its GNS file into
 * the load buffer.
 *
 * Returns the GNS table when the read started and 0 when it could not start. A
 * map without a GNS sector returns whatever bzero left in $v0. */
gns_record_t* battle_map_load_gns_and_move_find_items(s32 map_id) {
    u32 selected_map;
    gns_record_t* gns_records;
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

/* Start reading one map file into `destination`. Returns `destination` when
 * the read was started, 0 when the row names no file or the start failed. */
void* battle_map_start_file_load(
    const char* unused_directory_path, const gns_load_info_t* load_info, void* destination, s32 unused_resource_type) {
    void* result;

    if (load_info->fields.file_present_marker == 0) {
        result = 0;
    } else {
        /* These 32-bit values are aligned within the file row; retail loads both with lw. */
        if (main_file_call_build_header(
                *(const s32*)load_info->fields.disc_lba_le, *(const s32*)load_info->fields.byte_length_le, destination)
            == 0) {
            return destination;
        }
        result = 0;
    }
    return result;
}
