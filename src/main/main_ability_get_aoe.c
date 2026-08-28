#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

u32 main_ability_get_aoe(u32 ability_id) {
    u32 result;

    ability_id &= 0xffff;
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        result = (&g_ability_aoe_data)[ability_id * 14];
    } else {
        result = 0;
    }
    return result;
}
