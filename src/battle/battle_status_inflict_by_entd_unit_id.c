#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_status_inflict_by_entd_unit_id(s32 entd_id, s32 status, s32 which, s32 removal_only) {
    s32 idx;
    battle_stats_t* unit;
    s32 i;
    s32 byte;
    s32 bit;

    idx = battle_unit_get_battle_index_by_entd_unit_id(entd_id);
    if (idx == 0x7D0) {
        return;
    }
    unit = battle_unit_get_stats_from_battle_id(idx);
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        unit->action.status_infliction[i] = 0;
        unit->action.status_removal[i] = 0;
    }
    /* Action infliction/removal bytes are LSB-first; stored status sets use
     * BATTLE_STATUS_BYTE_MASK's MSB-first representation. */
    byte = status / 8;
    bit = 1 << (status - byte * 8);
    if (which != 0) {
        unit->action.status_infliction[byte] = bit;
    } else {
        unit->action.status_removal[byte] = bit;
    }
    g_battle_thread_call_target = (void (*)(void))battle_status_resolve_unit_changes;
    battle_thread_call_on_main_stack(idx, removal_only);
}
