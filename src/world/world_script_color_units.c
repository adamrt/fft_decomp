#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_color_units(const u8* parameters) {
    u16 unit_id;
    s32 misc_index;
    s32 unit_index;

    unit_id = world_script_load_halfword(parameters);
    if (world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        parameters += 2;
        unit_index = 0;
        do {
            if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &misc_index) != 0) {
                battle_gfx_start_misc_unit_palette_modulation(
                    parameters[0], parameters[4], unit_id, (s8)parameters[1], (s8)parameters[2], (s8)parameters[3]);
                if (misc_index == 0) {
                    break;
                }
            }
            unit_index++;
        } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
    }
}
