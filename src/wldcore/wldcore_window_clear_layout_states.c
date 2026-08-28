#include "fft/wldcore.h"

/* Clears the window/render states for one world-map layout: the palette word
 * of four 36-byte window records and one 52-byte render record, each selected by an index word of the caller's
 * descriptor.
 *
 * The target keeps 0x800bb504 in $a1 for the three record-relative stores
 * instead of folding the field offset into the ASPSX $at form; the tied empty
 * asm reproduces that. The remaining stores address their fields by symbol.
 * Sibling of the publisher at 0x800863a0, which writes the same fields with
 * non-zero values. */
void wldcore_window_clear_layout_states(s32* param) {
    wldcore_window_record_t* records;
    s32 window_0;
    s32 window_1;
    s32 window_2;
    s32 window_3;
    s32 slot;

    records = g_wldcore_window_records;
    __asm__("" : "=r"(records) : "0"(records));
    window_0 = param[0];
    window_1 = param[1];
    window_2 = param[2];
    window_3 = param[3];

    g_wldcore_window_render_records[param[4]].palette = 0;
    records[window_3].palette = 0;
    records[window_2].palette = 0;
    records[window_1].palette = 0;
    g_wldcore_window_records[window_0].palette = 0;
    g_wldcore_window_records[param[0]].sequence = 2;
    slot = param[0];
    records[slot].anim_counter = 0;
    g_wldcore_window_records[slot].frame_index = 0;
}
