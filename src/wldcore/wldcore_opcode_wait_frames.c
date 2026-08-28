#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_wait_frames(void) {
    u16* state_flags;
    u16 flags;
    u16 updated;
    s32 counter;
    s32 operand;

    state_flags = &g_wldcore_active_saved_record.state_flags;
    flags = *state_flags;
    updated = flags | 8;
    *state_flags = updated;
    if ((updated & 0x10) != 0) {
        g_wldcore_sound_novel_wait_counter = 0;
        g_wldcore_sound_novel_wait_frames = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    }
    counter = g_wldcore_sound_novel_wait_counter;
    operand = g_wldcore_sound_novel_wait_frames;
    if (operand < counter) {
        *state_flags = flags | 0xc;
    } else {
        g_wldcore_sound_novel_wait_counter = counter + 1;
    }
}
