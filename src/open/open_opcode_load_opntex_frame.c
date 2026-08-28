#include "fft/open.h"

/* Each OPNTEX frame occupies 23 * 0x800 bytes of the loaded data. */
#define OPEN_OPNTEX_FRAME_SECTORS 23

void open_opcode_load_opntex_frame(const s16* command) {
    s32 offset = command[1] * OPEN_OPNTEX_FRAME_SECTORS * 0x800;
    u8* source = g_open_gfx_opntex_data;

    open_gfx_load_opntex_into_frame_buffer(source + offset);
    g_open_script_state.opntex.fade_intensity = 0;
    g_open_script_state.dispatch.byte_offset += 4;
}
