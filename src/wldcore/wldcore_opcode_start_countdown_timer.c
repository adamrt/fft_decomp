#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_start_countdown_timer(void) {
    u32 instruction;

    instruction = g_wldcore_opcode_state.instruction.word;
    g_wldcore_sound_novel_countdown_elapsed = 0;
    g_wldcore_sound_novel_countdown_frames = ((instruction << 8) >> 16) * 60;
    g_wldcore_sound_novel_countdown_section = instruction >> 24;
    g_wldcore_state_flags |= 4;
}
