#include "fft/wldcore.h"

/* Clears the window/render record states for one world-map layout and
 * queues message 0xB806 on thread 14.
 *
 * Each store addresses its record field by symbol, the ASPSX $at indexed
 * form the target uses. `second` is read before the
 * stores because the target schedules that load into the first store's
 * address computation; `slot` is reloaded per store because the stores
 * invalidate the cached parameter loads. `pad` covers the eight frame
 * bytes the target reserves below the saved return address.
 * The sibling at 0x800719bc publishes the same fields without the queue. */
void wldcore_window_clear_layout_states_and_queue_msg_b806(s32* param) {
    s32 index;
    s32 second;
    s32 slot;
    s32 pad[2];

    index = param[0];
    second = param[1];
    g_wldcore_window_render_records[param[2]].palette = 0;
    g_wldcore_window_records[second].palette = 0;
    g_wldcore_window_records[index].palette = 0;
    slot = param[0];
    g_wldcore_window_records[slot].sequence = 2;
    slot = param[0];
    g_wldcore_window_records[slot].anim_counter = 0;
    g_wldcore_window_records[slot].frame_index = 0;
    world_thread_set_parameters(14, 0x19, 0xB806, 0);
}
