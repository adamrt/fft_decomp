#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/world.h"
#include "psx/types.h"

/* Set or clear palette-update suppression on every unit record resolved from
 * the given unit id. */
void world_script_set_units_palette_update_suppression(u16 id, s32 enable) {
    u16 unit_id;
    s32 filter_mode;
    s32 unit_index;

    unit_id = id;
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                if (enable == 0) {
                    battle_unit_clear_palette_update_suppression(unit_id);
                } else {
                    battle_unit_set_palette_update_suppression(unit_id);
                }
                if (filter_mode == 0) {
                    break;
                }
            }
        }
    }
}
