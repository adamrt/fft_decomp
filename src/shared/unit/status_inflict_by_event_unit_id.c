#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Event-script status change: inflicts (which != 0) or removes one status on the
 * unit an event unit id resolves to.
 *
 * The unit's pending infliction and removal bytes are cleared, the one status bit
 * is set and status_resolve_unit_changes runs on the main stack with
 * removal_only. Nothing happens when the id resolves to no unit on the field. */
void status_inflict_by_event_unit_id(s32 event_unit_id, s32 status, s32 which, s32 removal_only) {
    s32 idx;
    unit_stats_t* unit;
    s32 i;
    s32 byte;
    s32 bit;

    idx = battle_unit_get_battle_id_by_event_unit_id(event_unit_id);
    if (idx == 0x7D0) {
        return;
    }
    unit = battle_unit_get_stats_from_battle_id(idx);
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        unit->action.status_changes.infliction[i] = 0;
        unit->action.status_changes.removal[i] = 0;
    }
    /* Action infliction/removal bytes are LSB-first; stored status sets use
     * STATUS_BYTE_MASK's MSB-first representation. */
    byte = status / 8;
    bit = 1 << (status - byte * 8);
    if (which != 0) {
        unit->action.status_changes.infliction[byte] = bit;
    } else {
        unit->action.status_changes.removal[byte] = bit;
    }
    g_thread_call_target = (void (*)(void))status_resolve_unit_changes;
    thread_call_on_main_stack(idx, removal_only);
}
