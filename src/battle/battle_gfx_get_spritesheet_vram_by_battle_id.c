#include "fft/battle.h"

s32 battle_gfx_get_spritesheet_vram_by_battle_id(u32 battle_id) {
    battle_unit_misc_data_t* unit = g_battle_unit_last_misc_data;

    while (unit != 0) {
        battle_stats_t* battle_data = unit->battle_data;

        if (battle_data != 0 && battle_data->misc_unit_id == battle_id) {
            return unit->spritesheet_vram_slot;
        }
        unit = unit->previous;
    }
    return -1;
}
