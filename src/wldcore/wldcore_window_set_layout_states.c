#include "fft/wldcore.h"
#include "psx/types.h"

/* Publish the window/render states for one world-map layout: four 36-byte
 * window records and one 52-byte render record, each selected by an index
 * word of the caller's descriptor.
 *
 * The target keeps 0x800bb504 in its own register for two of the state
 * stores and reaches the other two through the symbol, so those sites stay
 * plain subscripts. */
void wldcore_window_set_layout_states(s32* param) {
    /* Pin: unpinned, the separate base and window_1 exchange $a3 and $a2. */
    register wldcore_window_record_t* records __asm__("$7");
    s32 window_3;
    s32 window_1;
    s32 window_2;

    g_wldcore_window_records[param[0]].palette = 10;
    window_3 = param[3];
    window_1 = param[1];
    window_2 = param[2];
    records = g_wldcore_window_records;
    /* Hides the base from cse; the window_3 input keeps the base setup early. */
    __asm__("" : "=r"(records) : "0"(records), "r"(window_3));
    records[window_3].palette = 6;
    records[window_2].palette = 6;
    g_wldcore_window_records[window_1].palette = 6;
    g_wldcore_window_render_records[param[4]].palette = 2;
}
