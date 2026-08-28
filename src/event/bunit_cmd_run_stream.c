#include "fft/bunit.h"
#include "psx/types.h"

/* Twin of jobstts_cmd_run_stream; BUNIT's stream terminator is 0x1C. */
void bunit_cmd_run_stream(u8* data, s32 flags) {
    g_bunit_gfx_otag_index = 0;
    g_bunit_gfx_semitrans_enabled = 0;
    g_bunit_cmd_stream_input = flags;
    g_bunit_gfx_otag_index_locked = 0;
    if (data[0] != 0x1C) {
        do {
            data = ((u8 * (*)(void)) g_bunit_cmd_handlers[data[0]])();
        } while (data[0] != 0x1C);
    }
}
