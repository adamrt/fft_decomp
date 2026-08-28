#include "fft/battle.h"
#include "fft/status.h"
#include "psx/types.h"

/* Disable the renderer flags of every current status, then clear the unit's
 * innate, immune, current, status CT and inflicted status bytes (Morbol
 * transformation preparation). */
void battle_status_reapply_active_flags(battle_stats_t* unit) {
    s32 i;
    s32 misc_unit_id = unit->misc_unit_id;

    i = 0;
    do {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            battle_status_queue_misc_graphics_flag_change(BATTLE_STATUS_HANDLER_INDEX(i), 0, misc_unit_id);
        }
        i += 1;
    } while (i < BATTLE_STATUS_COUNT);
    main_util_clear_byte_data(unit->status_sets.innate, 0x1F);
    main_util_clear_byte_data(unit->inflicted_status, BATTLE_STATUS_BYTE_COUNT);
}
