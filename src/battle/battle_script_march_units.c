#include "fft/battle.h"
#include "psx/types.h"

/*
 * Event instruction: put the addressed unit(s) into their status idle
 * animation and wait.  Parameters: 0x00 unit id (halfword), 0x02 wait length
 * in frames.  A unit id above 0xff selects a group, so the misc-state loop
 * runs over all 21 unit indices.
 */
s32 battle_script_march_units(u8* parameters) {
    u16 misc_id;
    s32 state;
    s32 unit_index;

    misc_id = battle_script_load_halfword(parameters);
    if (battle_unit_try_get_misc_data_by_unit_id(&misc_id, &state) == 0) {
        return;
    }
    unit_index = 0;
    do {
        if (battle_script_filter_unit_id_by_mode(&misc_id, (u16*)&unit_index, &state) != 0) {
            battle_unit_set_status_animation_by_misc_id(misc_id);
            battle_thread_wait_frames(parameters[2]);
            if (state == 0) {
                return;
            }
        }
        unit_index = unit_index + 1;
    } while (unit_index < EVENT_UNIT_SLOT_COUNT);
}
