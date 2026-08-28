#include "fft/battle.h"
#include "psx/types.h"

enum {
    MAP_EFFECT_DATA_END = 0x87,
    MAP_EFFECT_DATA_BEGIN = 0x88,
};

s32 battle_map_configure_effect_data_state(s32 command) {
    s32 result;

    switch (command) {
    case MAP_EFFECT_DATA_END:
        result = 4;
        g_battle_map_untextured_otz_mask = 0;
        g_battle_map_untextured_fixed_ot_index = result;
        break;
    case MAP_EFFECT_DATA_BEGIN:
        result = -1;
        g_battle_map_untextured_otz_mask = result;
        g_battle_map_untextured_fixed_ot_index = 0;
        break;
    default:
        result = -1;
        break;
    }
    return result;
}
