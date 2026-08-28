#include "fft/battle.h"
#include "fft/event.h"

/* Event instructions 0x6c (enable 0) and 0x6d (enable 1): clear or set
 * palette-update suppression on the event unit, or on every unit its group
 * mode selects.
 *
 * 0x80149698 stores a0 directly as a halfword. The interpreter passes a
 * zero-extended word; signed argument normalization is not part of this ABI. */
void battle_script_set_units_palette_update_suppression(s32 event_unit_id, s32 enable) {
    u16 unit_id;
    s32 mode;
    s32 i;

    unit_id = (u16)event_unit_id;
    if (battle_unit_try_get_misc_data_by_unit_id(&unit_id, &mode) != 0) {
        i = 0;
        do {
            if (battle_script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &mode) != 0) {
                if (enable == 0) {
                    battle_unit_clear_palette_update_suppression(unit_id);
                } else {
                    battle_unit_set_palette_update_suppression(unit_id);
                }
                if (mode == 0) {
                    return;
                }
            }
            i += 1;
        } while (i < EVENT_UNIT_SLOT_COUNT);
    }
}
