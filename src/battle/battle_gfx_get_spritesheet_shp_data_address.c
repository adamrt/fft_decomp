#include "fft/battle_gfx.h"

battle_gfx_unit_shp_frame_tables_t* battle_gfx_get_spritesheet_shp_data_address(u32 index) {
    s32 value;

    value = g_battle_gfx_spritesheet_data[index & 0xffff].shp_id;
    if (value < 5) {
        return &g_battle_gfx_spritesheet_record_data[value];
    }
    return &g_battle_gfx_spritesheet_fallback;
}
