#include "fft/main.h"

/* Add, remove, or replace a status-set flag and refresh the unit's current status. */
void main_status_change_unit(unit_stats_t* unit, s32 status_byte_index, u8 status_flag, s32 change_type) {
    u8 updated;

    /* An enum selector removes the native signed range test. */
    switch (change_type) {
    case MAIN_STATUS_ADD:
        updated = unit->inflicted_status[status_byte_index] | status_flag;
        break;
    case MAIN_STATUS_REMOVE:
        updated = unit->inflicted_status[status_byte_index] & ~status_flag;
        break;
    case MAIN_STATUS_REPLACE:
        updated = status_flag;
        break;
    }

    unit->inflicted_status[status_byte_index] = updated;
    unit->status_sets.current[status_byte_index] = updated | unit->status_sets.innate[status_byte_index];
}
