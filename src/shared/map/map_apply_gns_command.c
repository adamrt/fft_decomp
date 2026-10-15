#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"

/* Apply one command record that follows the resource rows of a map's GNS file and
 * step g_map_gns_record_cursor past it.
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
s32 map_apply_gns_command(s32 phase, map_resource_t* record) {
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
            map_dispatch_gns_resource(GNS_COMMAND_SET_INDOOR, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_OUTDOOR) {
            map_dispatch_gns_resource(GNS_COMMAND_SET_OUTDOOR, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_CLEAR_SNOW) {
            map_dispatch_gns_resource(GNS_COMMAND_CLEAR_SNOW, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_SNOW) {
            map_dispatch_gns_resource(GNS_COMMAND_SET_SNOW, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_UNHANDLED_8A) {
            map_dispatch_gns_resource(GNS_COMMAND_UNHANDLED_8A, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_WEATHER_MODIFIER) {
            map_dispatch_gns_resource(GNS_COMMAND_SET_WEATHER_MODIFIER, selected);
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
            if (map_is_gns_record_matching_state(selected->command.map_state_comparison,
                    *(u8*)&selected->command.map_state,
                    requested_layout_and_weather | ((script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15))) {
                map_dispatch_gns_resource(selected->command.resource_type, selected);
            }
        }
        g_map_gns_record_cursor += selected->command.record_byte_length;
    }
    return 0;
}
