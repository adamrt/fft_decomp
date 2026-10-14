#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/* Resolves an event unit id to a battle unit slot through
 * script_resolve_runtime_id. Returns 0x7d0 when the id resolves to no unit with
 * battle stats. WORLD's copy still calls BATTLE's runtime-id lookup, whose
 * address is the tail of a WLDCORE routine there (see QUIRKS.md). */
s32 unit_get_battle_id_by_event_unit_id(s32 event_unit_id) {
    s32 runtime_id = script_resolve_runtime_id(event_unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        s32 idx = battle_unit_get_battle_id_by_runtime_id(runtime_id);
        if (idx != -1) {
            return idx;
        }
    }
    return 0x7D0;
}
