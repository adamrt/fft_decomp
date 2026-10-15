#include "fft/battle_status.h"
#include "fft/main.h"
#include "psx/types.h"

/* Apply Float and reconcile the unit's status flags and status CT. */
void main_status_update_unit_flags_and_ct(unit_stats_t* unit) {
    s32 status;
    u8 immunity;

    if (unit->ability_flags.movement[2] & BATTLE_MOVEMENT_SET_3_FLOAT) {
        unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] |= STATUS_BYTE_MASK(STATUS_ID_FLOAT);
    }

    for (status = 0; status < STATUS_BYTE_COUNT; status++) {
        immunity = unit->status_sets.immunity[status];
        immunity &= ~unit->status_sets.innate[status];
        unit->status_sets.immunity[status] = immunity;
        unit->status_sets.current[status] = unit->status_sets.innate[status] | unit->inflicted_status[status];
    }

    for (status = STATUS_ID_POISON; status < STATUS_COUNT; status++) {
        s32 status_byte = status / 8;
        s32 status_bit = status & 7;

        if ((unit->inflicted_status[status_byte] & (0x80 >> status_bit))
            && unit->status_ct[STATUS_CT_INDEX(status)] == 0) {
            main_status_set_ct(unit, status, 0);
        }
    }
}
