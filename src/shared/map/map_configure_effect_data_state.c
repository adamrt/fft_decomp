#include "fft/battle_map.h"
#include "psx/types.h"

/* Switch the ordering-table placement of untextured map polygons around an
 * ability effect: BEGIN places them by depth (OT z mask -1, fixed index 0), END
 * returns them to fixed slot 4 (mask 0), the values a map load sets. Returns 4
 * for END, and -1 for BEGIN or any other command. */
s32 map_configure_effect_data_state(map_effect_data_command_e command) {
    s32 result;

    switch (command) {
    case MAP_EFFECT_DATA_END:
        result = 4;
        g_map_untextured_otz_mask = 0;
        g_map_untextured_fixed_ot_index = result;
        break;
    case MAP_EFFECT_DATA_BEGIN:
        result = -1;
        g_map_untextured_otz_mask = result;
        g_map_untextured_fixed_ot_index = 0;
        break;
    default:
        result = -1;
        break;
    }
    return result;
}
