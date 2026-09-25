#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Set or clear palette-update suppression on every misc record resolved from
 * the given unit id. */
void world_script_set_units_palette_update_suppression(u16 id, s32 enable) {
    u16 unit_id;
    s32 misc_index;
    s32 unit_index;

    unit_id = id;
    if (world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        unit_index = 0;
        do {
            if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &misc_index) != 0) {
                if (enable == 0) {
                    battle_unit_clear_palette_update_suppression(unit_id);
                } else {
                    battle_unit_set_palette_update_suppression(unit_id);
                }
                if (misc_index == 0) {
                    break;
                }
            }
            unit_index++;
        } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
    }
}
