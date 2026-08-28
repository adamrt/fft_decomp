#include "fft/data.h"
#include "fft/main_runtime.h"

u32 main_ability_get_range(u32 ability_id) {
    u32 result;

    ability_id &= 0xffff;
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        result = g_main_ability_range_data[ability_id].range;
    } else {
        result = 0;
    }
    return result;
}
