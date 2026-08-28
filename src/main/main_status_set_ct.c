#include "fft/main_unit.h"
#include "psx/types.h"

s32 main_status_set_ct(battle_stats_t* unit, s32 status_id, s32 removing) {
    battle_stats_t* stats = unit;
    s32 ct_index;
    s32 stored_index;

    if (status_id == BATTLE_STATUS_ID_DEAD) {
        if ((stats->team_flags & BATTLE_TEAM_FLAG_IMMORTAL)
            || (stats->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION))) {
            stats->death_counter = 0xff;
        } else {
            stats->death_counter = 3;
        }
    }

    ct_index = BATTLE_STATUS_CT_INDEX(status_id);
    if ((u32)ct_index >= BATTLE_TIMED_STATUS_COUNT) {
        return 0;
    }
    stored_index = ct_index;
    if (removing) {
        stats->status_ct[stored_index] = 0;
        return 0;
    }
    if ((stored_index == BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE))
        && (stats->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)] != 0)) {
        return -1;
    }
    stats->status_ct[stored_index] = g_main_status_effect_data[status_id].ct;
    return 0;
}
