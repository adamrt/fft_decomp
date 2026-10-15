#include "fft/battle_status.h"
#include "fft/main.h"
#include "psx/types.h"

/* Start a timed status's CT from g_main_status_effect_data, or clear it when
 * removing. For Dead, in either case, the death counter is set to 3, or to
 * 0xff for an immortal unit and for one with a save- or load-formation flag.
 * Returns -1 when Death Sentence is already counting, so a second one does not
 * restart it; otherwise 0. */
s32 main_status_set_ct(unit_stats_t* unit, s32 status_id, s32 removing) {
    u32 ct_index;
    s32 stored_index;

    if (status_id == STATUS_ID_DEAD) {
        if ((unit->team_flags & BATTLE_TEAM_FLAG_IMMORTAL)
            || (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION))) {
            unit->death_counter = 0xff;
        } else {
            unit->death_counter = 3;
        }
    }

    ct_index = STATUS_CT_INDEX(status_id);
    if (ct_index >= BATTLE_TIMED_STATUS_COUNT) {
        return 0;
    }
    stored_index = ct_index;
    if (removing) {
        unit->status_ct[stored_index] = 0;
        return 0;
    }
    if ((stored_index == STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE))
        && (unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] != 0)) {
        return -1;
    }
    unit->status_ct[stored_index] = g_main_status_effect_data[status_id].ct;
    return 0;
}
