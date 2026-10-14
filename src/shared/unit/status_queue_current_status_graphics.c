#include "fft/battle_status.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Queue the renderer status flags for each of a unit's current statuses on
 * the main stack; the Draw event instruction runs this only for the Zalera
 * event. */
void status_queue_current_status_graphics(s32 battle_id, s32 enabled) {
    unit_stats_t* stats;
    s32 i;
    s32 mask;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    g_thread_call_target = (void (*)(void))status_queue_runtime_graphics_flag_change;
    for (i = 0; i < STATUS_COUNT; i++) {
        mask = 0x80 >> (i % 8);
        if ((stats->status_sets.current[i / 8] & mask) != 0) {
            thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), enabled, battle_id);
        }
    }
}
