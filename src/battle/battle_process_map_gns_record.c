#include "fft/battle.h"
#include "fft/script_variables.h"

s32 battle_process_map_gns_record(s32 phase, gns_file_record_t* record) {
    char unused_zeroes[8] = { 0 };
    s32 unused_sizes[4] = { 0x1000, 0x1000, 0x1000, 0 };
    s32 requested_layout;
    s32 requested_layout_and_weather;
    gns_file_record_t* selected;

    if (phase == 2) {
        /* Everything below works on this copy; the parameter itself dies
         * here, which keeps it out of a callee-saved register. */
        selected = record;
        if (selected->resource_type == GNS_RESOURCE_SET_INDOOR) {
            battle_map_dispatch_gns_resource(GNS_RESOURCE_SET_INDOOR, (u8*)selected);
        } else if (selected->resource_type == GNS_RESOURCE_SET_OUTDOOR) {
            battle_map_dispatch_gns_resource(GNS_RESOURCE_SET_OUTDOOR, (u8*)selected);
        } else if (selected->resource_type == GNS_RESOURCE_CLEAR_SNOW) {
            battle_map_dispatch_gns_resource(GNS_RESOURCE_CLEAR_SNOW, (u8*)selected);
        } else if (selected->resource_type == GNS_RESOURCE_SET_SNOW) {
            battle_map_dispatch_gns_resource(GNS_RESOURCE_SET_SNOW, (u8*)selected);
        } else if (selected->resource_type == GNS_RESOURCE_RESERVED_8A) {
            battle_map_dispatch_gns_resource(GNS_RESOURCE_RESERVED_8A, (u8*)selected);
        } else if (selected->resource_type == GNS_RESOURCE_SET_WEATHER_MODIFIER) {
            battle_map_dispatch_gns_resource(GNS_RESOURCE_SET_WEATHER_MODIFIER, (u8*)selected);
        } else {
            /*
             * Requested map state, packed like gns_file_record_t.map_state: layout in
             * bits 0-11, weather in 12-14, time of day in 15.  The target reads
             * the record's variable id and map state as single bytes (lbu), not
             * as the halfwords the struct declares, so those two stay byte reads.
             */
            requested_layout = battle_script_get_variable(((u8*)selected)[0]) & 0xfff;
            requested_layout_and_weather
                = requested_layout | ((battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12);
            if ((battle_map_is_gns_record_matching_state(selected->map_state_comparison, ((u8*)selected)[2],
                     requested_layout_and_weather
                         | ((battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15))
                    << 16)
                != 0) {
                battle_map_dispatch_gns_resource(selected->resource_type, (u8*)selected);
            }
        }
        /* Byte read of start_sector's low byte, as the target does. */
        g_battle_map_gns_record_cursor += ((u8*)selected)[8];
    }
    return 0;
}
