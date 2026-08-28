#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Collects the text ids of the location's selectable choices into out.
 *
 * Location 0x14 returns -1, and locations whose script variable 0x267 + id is
 * set have no choices; otherwise the up-to-four entries of
 * g_wldcore_script_state.choice_vars are filtered by script variable
 * 0x292 + their menu byte, and at most five survive. */
s32 wldcore_location_collect_choice_text_ids(s32 location_id, s32* out) {
    s32 available;
    /* Pin: unpinned, output, the choice pointer and available rotate $s3-$s5. */
    register s32* output __asm__("$19");
    s32 i;
    s32 written;
    s32 entry;

    output = out;
    if (location_id == 0x14) {
        return -1;
    }
    if (world_script_get_variable(location_id + 0x267) != 0) {
        return 0;
    }
    if (wldcore_script_process_conditional_set(g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_CHOICE)
        == 0) {
        return 0;
    }
    available = 0;
    for (i = 0; i < 4; i++) {
        if (g_wldcore_script_state.choice_vars[i] == 0) {
            break;
        }
        available++;
    }
    if (available == 1) {
        entry = g_wldcore_script_state.choice_vars[0];
        *output = entry + 0xa7ff;
        return 1;
    }
    written = 0;
    for (i = 0; i < available; i++) {
        entry = g_wldcore_script_state.choice_vars[i];
        if (world_script_get_variable(g_wldcore_choice_hidden_variable_offsets[entry] + 0x292) == 0) {
            *output = entry + 0xa7ff;
            written++;
            output++;
            if (written == 5) {
                break;
            }
        }
    }
    return written;
}
