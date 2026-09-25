#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_march_units(const u8* parameters) {
    u16 unit_id;
    s32 misc_index;
    s32 unit_index;

    unit_id = world_script_load_halfword(parameters);
    if (world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        unit_index = 0;
        do {
            if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &misc_index) != 0) {
                battle_unit_set_status_animation_by_misc_id(unit_id);
                world_thread_wait_frames(parameters[2]);
                if (misc_index == 0) {
                    break;
                }
            }
            unit_index++;
        } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
    }
}
